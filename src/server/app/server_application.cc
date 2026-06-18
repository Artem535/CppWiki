#include "server/app/server_application.h"

#include <memory>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

#include "oatpp/network/Address.hpp"
#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/web/server/HttpRouter.hpp"
#include "server/http/health_controller.h"
#include "server/openapi/swagger.h"

namespace cppwiki::server {

ServerApplication::ServerApplication(ServerConfig config) : config_(std::move(config)) {}

auto ServerApplication::Run() const -> int {
  auto router = oatpp::web::server::HttpRouter::createShared();
  auto object_mapper = oatpp::parser::json::mapping::ObjectMapper::createShared();

  auto health_controller = std::make_shared<HealthController>(object_mapper);
  oatpp::web::server::api::Endpoints endpoints = health_controller->getEndpoints();
  router->addController(health_controller);
  RegisterSwaggerEndpoints(router, endpoints, config_);

  auto connection_handler =
      oatpp::web::server::HttpConnectionHandler::createShared(router);

  const auto address = oatpp::network::Address(
      config_.BindHost().c_str(), config_.Port(), oatpp::network::Address::IP_4);
  auto connection_provider =
      oatpp::network::tcp::server::ConnectionProvider::createShared(address);

  spdlog::info("Starting cppwiki-server on http://{}:{}/api/v1/health", config_.BindHost(),
               config_.Port());
  if (config_.SwaggerEnabled()) {
    spdlog::info("Swagger UI enabled on http://{}:{}/swagger/ui", config_.BindHost(),
                 config_.Port());
  }

  oatpp::network::Server server(connection_provider, connection_handler);
  server.run();
  return 0;
}

}  // namespace cppwiki::server
