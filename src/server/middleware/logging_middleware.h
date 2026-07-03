#ifndef CPPWIKI_SRC_SERVER_MIDDLEWARE_LOGGING_MIDDLEWARE_H_
#define CPPWIKI_SRC_SERVER_MIDDLEWARE_LOGGING_MIDDLEWARE_H_

#include <string>

#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/http/http_request.hpp>

namespace cppwiki::server::middleware {

void AttachRequestTags(userver::server::http::HttpRequest& request);

}  // namespace cppwiki::server::middleware

#endif  // CPPWIKI_SRC_SERVER_MIDDLEWARE_LOGGING_MIDDLEWARE_H_
