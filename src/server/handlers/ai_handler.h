#ifndef CPPWIKI_SRC_SERVER_HANDLERS_AI_HANDLER_H_
#define CPPWIKI_SRC_SERVER_HANDLERS_AI_HANDLER_H_

#include <userver/server/handlers/http_handler_json_base.hpp>

#include "server/service/ai_chat_service.h"

namespace cppwiki::server::handlers {

// POST /api/v1/ai/chat — server-mediated AI backend (ADR-012): the desktop
// client's EditorBridge forwards rewrite/autocomplete requests here instead
// of calling the AI provider directly, so the provider API key only ever
// lives on the server (config/server.yaml's `ai:` section).
class AiHandler final : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-ai-chat";

  AiHandler(const userver::components::ComponentConfig& config,
           const userver::components::ComponentContext& context);

  [[nodiscard]] auto HandleRequestJsonThrow(
      const userver::server::http::HttpRequest& request,
      const userver::formats::json::Value& request_body,
      userver::server::request::RequestContext& context) const
      -> userver::formats::json::Value override;

 private:
  service::AiChatService ai_chat_service_;
};

}  // namespace cppwiki::server::handlers

#endif  // CPPWIKI_SRC_SERVER_HANDLERS_AI_HANDLER_H_
