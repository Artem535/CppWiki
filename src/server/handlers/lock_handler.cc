#include "server/handlers/lock_handler.h"

#include <rfl/json/read.hpp>

#include <optional>
#include <string>

#include <spdlog/spdlog.h>
#include <userver/formats/json.hpp>
#include <userver/server/http/http_response.hpp>

#include "server/dto/lock_response.h"
#include "server/dto/response_envelope.h"
#include "server/middleware/logging_middleware.h"

namespace cppwiki::server::handlers {

namespace {

auto ParseLockRequest(const userver::formats::json::Value& request_body) -> std::optional<dto::LockRequestDto> {
  auto parsed = rfl::json::read<dto::LockRequestDto>(userver::formats::json::ToString(request_body));
  if (!parsed) {
    return std::nullopt;
  }
  return parsed.value();
}

}  // namespace

LockHandler::LockHandler(const userver::components::ComponentConfig& config,
                         const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context) {}

auto LockHandler::HandleRequestJsonThrow(const userver::server::http::HttpRequest& request,
                                         const userver::formats::json::Value& request_body,
                                         userver::server::request::RequestContext&) const
    -> userver::formats::json::Value {
  auto& response = request.GetHttpResponse();
  ApplyCorsHeaders(response);
  middleware::AttachRequestTags(const_cast<userver::server::http::HttpRequest&>(request));

  const auto document_id = std::string(request.GetPathArg("document_id"));
  const auto owner = ExtractOwner(request_body, request);
  const auto method = request.GetMethod();

  dto::LockActionResult result;
  result.document_id = document_id;

  if (method == userver::server::http::HttpMethod::kPost) {
    result.acquired = lock_service_.Acquire(document_id, owner);
    result.owner = result.acquired ? std::optional(owner) : lock_service_.GetOwner(document_id);
    spdlog::info("Lock acquire document_id={} owner={} acquired={}", document_id, owner,
                 result.acquired);
  } else if (method == userver::server::http::HttpMethod::kPut) {
    result.heartbeat = lock_service_.Heartbeat(document_id, owner);
    result.owner = lock_service_.GetOwner(document_id);
    spdlog::info("Lock heartbeat document_id={} owner={} ok={}", document_id, owner,
                 result.heartbeat);
  } else if (method == userver::server::http::HttpMethod::kDelete) {
    if (request.GetArg("force") == "true") {
      result.force_released = lock_service_.ForceRelease(document_id);
      spdlog::info("Lock force release document_id={} ok={}", document_id, result.force_released);
    } else {
      result.released = lock_service_.Release(document_id, owner);
      spdlog::info("Lock release document_id={} owner={} ok={}", document_id, owner,
                   result.released);
    }
  } else if (method == userver::server::http::HttpMethod::kGet) {
    result.owner = lock_service_.GetOwner(document_id);
    spdlog::info("Lock status document_id={} locked={}", document_id, result.owner.has_value());
  } else {
    response.SetStatus(userver::server::http::HttpStatus::kMethodNotAllowed);
    return dto::MakeErrorEnvelopeJson(dto::kApiVersion, dto::ErrorDto{"method_not_allowed",
                                                                        "Method not allowed"});
  }

  return dto::MakeSuccessEnvelopeJson(dto::kApiVersion, dto::MakeLockResult(result));
}

void LockHandler::ApplyCorsHeaders(userver::server::http::HttpResponse& response) {
  response.SetHeader(std::string_view{"Access-Control-Allow-Origin"}, "*");
  response.SetHeader(std::string_view{"Access-Control-Allow-Methods"},
                     "GET, POST, PUT, DELETE, OPTIONS");
  response.SetHeader(std::string_view{"Access-Control-Allow-Headers"},
                     "Content-Type, Authorization");
}

auto LockHandler::ExtractOwner(const userver::formats::json::Value& request_body,
                               const userver::server::http::HttpRequest& request) -> std::string {
  if (const auto parsed = ParseLockRequest(request_body); parsed && parsed->owner) {
    return *parsed->owner;
  }
  const auto header = request.GetHeader("x-lock-owner");
  if (!header.empty()) {
    return std::string(header);
  }
  return "anonymous";
}

}  // namespace cppwiki::server::handlers
