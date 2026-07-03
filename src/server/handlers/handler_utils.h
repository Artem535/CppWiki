#ifndef CPPWIKI_SRC_SERVER_HANDLERS_HANDLER_UTILS_H_
#define CPPWIKI_SRC_SERVER_HANDLERS_HANDLER_UTILS_H_

#include <string_view>

#include <userver/server/http/http_request.hpp>
#include <userver/server/http/http_response.hpp>

namespace cppwiki::server::handlers {

void PrepareJsonResponse(const userver::server::http::HttpRequest& request,
                         std::string_view allowed_methods);

void PrepareHtmlResponse(const userver::server::http::HttpRequest& request);

void PrepareJsonDocumentResponse(const userver::server::http::HttpRequest& request);

}  // namespace cppwiki::server::handlers

#endif  // CPPWIKI_SRC_SERVER_HANDLERS_HANDLER_UTILS_H_
