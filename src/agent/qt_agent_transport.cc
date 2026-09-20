#include "agent/qt_agent_transport.h"

#include <string>
#include <utility>

#include <spdlog/spdlog.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "core/constants.h"
#include "core/qt_string.h"

namespace cppwiki::agent {

namespace {

auto RoleToString(AgentMessageRole role) -> QString {
  switch (role) {
    case AgentMessageRole::kSystem:
      return QStringLiteral("system");
    case AgentMessageRole::kUser:
      return QStringLiteral("user");
    case AgentMessageRole::kAssistant:
      return QStringLiteral("assistant");
    case AgentMessageRole::kTool:
      return QStringLiteral("tool");
  }
  return QStringLiteral("user");
}

auto BuildMessagesArray(const std::vector<AgentMessage>& messages) -> QJsonArray {
  QJsonArray array;
  for (const auto& message : messages) {
    QJsonObject object;
    object.insert(QStringLiteral("role"), RoleToString(message.role));
    object.insert(QStringLiteral("content"), ToQString(message.content));
    if (message.tool_call_id.has_value()) {
      object.insert(QStringLiteral("tool_call_id"), ToQString(*message.tool_call_id));
    }
    if (!message.tool_calls.empty()) {
      QJsonArray tool_calls;
      for (const auto& call : message.tool_calls) {
        QJsonObject function;
        function.insert(QStringLiteral("name"), ToQString(call.tool_name));
        function.insert(QStringLiteral("arguments"), ToQString(call.arguments_json));

        QJsonObject tool_call;
        tool_call.insert(QStringLiteral("id"), ToQString(call.id));
        tool_call.insert(QStringLiteral("type"), QStringLiteral("function"));
        tool_call.insert(QStringLiteral("function"), function);
        tool_calls.append(tool_call);
      }
      object.insert(QStringLiteral("tool_calls"), tool_calls);
    }
    array.append(object);
  }
  return array;
}

auto BuildToolsArray(const std::vector<AgentToolSchema>& tools) -> QJsonArray {
  QJsonArray array;
  for (const auto& tool : tools) {
    QJsonObject function;
    function.insert(QStringLiteral("name"), ToQString(tool.name));
    function.insert(QStringLiteral("description"), ToQString(tool.description));

    QJsonObject parameters;
    if (!tool.parameters_json_schema.empty()) {
      QJsonParseError parse_error{};
      const auto schema_document =
          QJsonDocument::fromJson(ToQString(tool.parameters_json_schema).toUtf8(), &parse_error);
      if (parse_error.error == QJsonParseError::NoError && schema_document.isObject()) {
        parameters = schema_document.object();
      }
    }
    function.insert(QStringLiteral("parameters"), parameters);

    QJsonObject tool_def;
    tool_def.insert(QStringLiteral("type"), QStringLiteral("function"));
    tool_def.insert(QStringLiteral("function"), function);
    array.append(tool_def);
  }
  return array;
}

// Parses an OpenAI-compatible `choices[0].message` object into an assistant AgentMessage. Empty
// or missing `content` (common when the reply is tool-calls-only) becomes an empty string
// rather than being treated as an error.
auto ParseAssistantMessage(const QJsonObject& message_object) -> AgentMessage {
  AgentMessage message;
  message.role = AgentMessageRole::kAssistant;

  const auto content = message_object.value(QStringLiteral("content"));
  if (content.isString()) {
    message.content = content.toString().toStdString();
  }

  const auto tool_calls = message_object.value(QStringLiteral("tool_calls")).toArray();
  for (const auto& raw_call : tool_calls) {
    const auto call_object = raw_call.toObject();
    const auto function_object = call_object.value(QStringLiteral("function")).toObject();

    AgentToolCall call;
    call.id = call_object.value(QStringLiteral("id")).toString().toStdString();
    call.tool_name = function_object.value(QStringLiteral("name")).toString().toStdString();
    call.arguments_json =
        function_object.value(QStringLiteral("arguments")).toString().toStdString();
    message.tool_calls.push_back(std::move(call));
  }

  return message;
}

}  // namespace

QtAgentTransport::QtAgentTransport(QString model, QObject* parent)
    : QObject(parent),
      api_key_store_(ToQString(constants::kApplicationName), this),
      model_(model.isEmpty() ? ToQString(constants::kAiDefaultModel) : std::move(model)) {
  connect(&api_key_store_, &auth::AiApiKeyStore::apiKeyLoaded, this,
          [this](const QString& api_key) { cached_api_key_ = api_key; });
  connect(&api_key_store_, &auth::AiApiKeyStore::apiKeyMissing, this,
          [this]() { cached_api_key_.reset(); });
  connect(&api_key_store_, &auth::AiApiKeyStore::storageError, this,
          [](const QString& operation, const QString& message) {
            spdlog::warn("Agent engine could not {} the AI provider API key: {}",
                        operation.toStdString(), message.toStdString());
          });
  api_key_store_.Load();
}

void QtAgentTransport::SendChatRequest(const AgentChatRequest& request,
                                       std::function<void(AgentChatOutcome)> on_done) {
  if (!cached_api_key_.has_value()) {
    on_done(std::string("No AI provider API key is configured. Configure one in Settings > AI."));
    return;
  }
  SendWithApiKey(request, std::move(on_done), *cached_api_key_);
}

void QtAgentTransport::SendWithApiKey(const AgentChatRequest& request,
                                      std::function<void(AgentChatOutcome)> on_done,
                                      const QString& api_key) {
  if (network_manager_ == nullptr) {
    network_manager_ = new QNetworkAccessManager(this);
  }

  QJsonObject body;
  body.insert(QStringLiteral("model"), model_);
  body.insert(QStringLiteral("messages"), BuildMessagesArray(request.messages));
  if (!request.tools.empty()) {
    body.insert(QStringLiteral("tools"), BuildToolsArray(request.tools));
  }

  QNetworkRequest network_request{QUrl(base_url_)};
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  network_request.setRawHeader("Authorization", ("Bearer " + api_key).toUtf8());

  auto* reply =
      network_manager_->post(network_request, QJsonDocument(body).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [reply, on_done = std::move(on_done)]() mutable {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
      on_done(reply->errorString().toStdString());
      return;
    }

    QJsonParseError parse_error{};
    const auto document = QJsonDocument::fromJson(reply->readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
      on_done(std::string("AI provider returned an invalid response."));
      return;
    }

    const auto choices = document.object().value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
      on_done(std::string("AI provider response did not contain any choices."));
      return;
    }

    const auto message_object =
        choices.first().toObject().value(QStringLiteral("message")).toObject();
    on_done(ParseAssistantMessage(message_object));
  });
}

}  // namespace cppwiki::agent
