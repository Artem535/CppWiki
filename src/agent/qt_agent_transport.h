#ifndef CPPWIKI_SRC_AGENT_QT_AGENT_TRANSPORT_H_
#define CPPWIKI_SRC_AGENT_QT_AGENT_TRANSPORT_H_

#include <functional>
#include <optional>

#include <QObject>
#include <QString>

#include "agent/agent_transport.h"
#include "auth/ai_api_key_store.h"

class QNetworkAccessManager;

namespace cppwiki::agent {

// The Agent engine's production AgentTransport: an OpenAI-compatible `/chat/completions` HTTP
// client. Reuses AiApiKeyStore (src/auth/, added in PR #26 for the BlockNote AI MVP) for its
// credential -- the same keychain entry the BlockNote AI local-key fallback already reads via
// Settings > AI -- rather than adding a second key storage mechanism (ADR-015). Not covered by
// the unit tests in tests/agent/ (those exercise the engine against a fake transport, per
// issue #29's "no real network calls in tests" scope); this class is build-verified only.
class QtAgentTransport final : public QObject, public AgentTransport {
 public:
  // `model` defaults to constants::kAiDefaultModel when empty.
  explicit QtAgentTransport(QString model = QString(), QObject* parent = nullptr);

  void SendChatRequest(const AgentChatRequest& request,
                       std::function<void(AgentChatOutcome)> on_done) override;

 private:
  void SendWithApiKey(const AgentChatRequest& request,
                      std::function<void(AgentChatOutcome)> on_done, const QString& api_key);

  QNetworkAccessManager* network_manager_ = nullptr;
  auth::AiApiKeyStore api_key_store_;
  std::optional<QString> cached_api_key_;
  QString model_;
  QString base_url_ = QStringLiteral("https://api.openai.com/v1/chat/completions");
};

}  // namespace cppwiki::agent

#endif  // CPPWIKI_SRC_AGENT_QT_AGENT_TRANSPORT_H_
