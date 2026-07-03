#include "server/handlers/handler_utils.h"

#include "server/middleware/logging_middleware.h"

namespace cppwiki::server::handlers {

namespace {

auto MutableResponse(const userver::server::http::HttpRequest& request)
    -> userver::server::http::HttpResponse& {
  return const_cast<userver::server::http::HttpRequest&>(request).GetHttpResponse();
}

void ApplyCorsHeaders(userver::server::http::HttpResponse& response,
                      std::string_view allowed_methods) {
  response.SetHeader(std::string_view{"Access-Control-Allow-Origin"}, "*");
  response.SetHeader(std::string_view{"Access-Control-Allow-Methods"}, std::string{allowed_methods});
  response.SetHeader(std::string_view{"Access-Control-Allow-Headers"},
                     "Content-Type, Authorization");
}

void AttachRequestTags(const userver::server::http::HttpRequest& request) {
  middleware::AttachRequestTags(const_cast<userver::server::http::HttpRequest&>(request));
}

}  // namespace

void PrepareJsonResponse(const userver::server::http::HttpRequest& request,
                         std::string_view allowed_methods) {
  ApplyCorsHeaders(MutableResponse(request), allowed_methods);
  AttachRequestTags(request);
}

void PrepareHtmlResponse(const userver::server::http::HttpRequest& request) {
  auto& response = MutableResponse(request);
  response.SetHeader(std::string_view{"Content-Type"}, "text/html; charset=utf-8");
  response.SetHeader(std::string_view{"Cache-Control"}, "no-store");
  AttachRequestTags(request);
}

void PrepareJsonDocumentResponse(const userver::server::http::HttpRequest& request) {
  auto& response = MutableResponse(request);
  response.SetHeader(std::string_view{"Content-Type"}, "application/json; charset=utf-8");
  response.SetHeader(std::string_view{"Access-Control-Allow-Origin"}, "*");
  AttachRequestTags(request);
}

}  // namespace cppwiki::server::handlers
