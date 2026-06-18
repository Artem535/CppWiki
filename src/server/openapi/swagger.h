#ifndef CPPWIKI_SRC_SERVER_SWAGGER_H_
#define CPPWIKI_SRC_SERVER_SWAGGER_H_

#include <memory>

#include "oatpp/web/server/api/ApiController.hpp"

namespace oatpp::web::server {
class HttpRouter;
}

namespace cppwiki::server {

class ServerConfig;

auto RegisterSwaggerEndpoints(
    const std::shared_ptr<oatpp::web::server::HttpRouter>& router,
    const oatpp::web::server::api::Endpoints& endpoints,
    const ServerConfig& config) -> void;

}  // namespace cppwiki::server

#endif  // CPPWIKI_SRC_SERVER_SWAGGER_H_
