#include "server/middleware/logging_middleware.h"

#include <spdlog/spdlog.h>

#include <string_view>

#include <userver/server/http/http_request.hpp>
#include <userver/tracing/span.hpp>

namespace cppwiki::server::middleware {

namespace {

constexpr std::string_view kRequestIdHeader = "x-request-id";

}  // namespace

void AttachRequestTags(userver::server::http::HttpRequest& request) {
  const auto request_id = request.GetHeader(kRequestIdHeader);
  if (!request_id.empty()) {
    request.GetHttpResponse().SetHeader(kRequestIdHeader, std::string(request_id));
  }

  auto& span = userver::tracing::Span::CurrentSpan();
  span.AddTag("component", "http_server");
  span.AddTag("operation", request.GetMethodStr() + " " + std::string(request.GetUrl()));
  if (!request_id.empty()) {
    span.AddTag("request_id", request_id);
  }

  const auto document_id = request.GetPathArg("document_id");
  if (!document_id.empty()) {
    span.AddTag("document_id", document_id);
  }

  const auto workspace_id = request.GetPathArg("workspace_id");
  if (!workspace_id.empty()) {
    span.AddTag("workspace_id", workspace_id);
  }

  spdlog::debug("Request started: {} {}", request.GetMethodStr(), request.GetUrl());
}

}  // namespace cppwiki::server::middleware
