#include "server/handlers/lock_handler.h"

#include <string>

#include <spdlog/spdlog.h>

#include "server/dto/json_adapter.h"
#include "server/dto/lock_response.h"
#include "server/dto/response_envelope.h"
#include "server/handlers/handler_utils.h"

namespace cppwiki::server::handlers {

LockHandler::LockHandler(const userver::components::ComponentConfig& config,
                         const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context) {}

auto LockHandler::HandleRequestJsonThrow(const userver::server::http::HttpRequest& request,
                                         const userver::formats::json::Value& request_body,
                                         userver::server::request::RequestContext&) const
    -> userver::formats::json::Value {
  auto& response = request.GetHttpResponse();
  PrepareJsonResponse(request, "GET, POST, PUT, DELETE, OPTIONS");

  const auto document_id = std::string(request.GetPathArg("document_id"));
  const auto owner = ExtractOwner(request_body, request);
  const auto method = request.GetMethod();

  dto::LockActionResult result;
  result.document_id = document_id;

  switch (method) {
    case userver::server::http::HttpMethod::kPost:
      result.acquired = lock_service_.Acquire(document_id, owner);
      result.owner = result.acquired ? std::optional(owner) : lock_service_.GetOwner(document_id);
      spdlog::info("Lock acquire document_id={} owner={} acquired={}", document_id, owner,
                   result.acquired);
      break;

    case userver::server::http::HttpMethod::kPut:
      result.heartbeat = lock_service_.Heartbeat(document_id, owner);
      result.owner = lock_service_.GetOwner(document_id);
      spdlog::info("Lock heartbeat document_id={} owner={} ok={}", document_id, owner,
                   result.heartbeat);
      break;

    case userver::server::http::HttpMethod::kDelete:
      if (request.GetArg("force") == "true") {
        result.force_released = lock_service_.ForceRelease(document_id);
        spdlog::info("Lock force release document_id={} ok={}", document_id,
                     result.force_released);
      } else {
        result.released = lock_service_.Release(document_id, owner);
        spdlog::info("Lock release document_id={} owner={} ok={}", document_id, owner,
                     result.released);
      }
      break;

    case userver::server::http::HttpMethod::kGet:
      result.owner = lock_service_.GetOwner(document_id);
      spdlog::info("Lock status document_id={} locked={}", document_id, result.owner.has_value());
      break;

    default:
      response.SetStatus(userver::server::http::HttpStatus::kMethodNotAllowed);
      return dto::MakeErrorEnvelopeJson(dto::kApiVersion,
                                        dto::ErrorDto{"method_not_allowed",
                                                      "Method not allowed"});
  }

  return dto::MakeSuccessEnvelopeJson(dto::kApiVersion, dto::MakeLockResult(result));
}

auto LockHandler::ExtractOwner(const userver::formats::json::Value& request_body,
                               const userver::server::http::HttpRequest& request) -> std::string {
  if (const auto parsed = dto::ParseJsonBody<dto::LockRequestDto>(request_body);
      parsed && parsed->owner) {
    return *parsed->owner;
  }
  const auto header = request.GetHeader("x-lock-owner");
  if (!header.empty()) {
    return std::string(header);
  }
  return "anonymous";
}

}  // namespace cppwiki::server::handlers
