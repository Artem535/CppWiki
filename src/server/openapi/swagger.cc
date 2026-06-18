#include "server/openapi/swagger.h"

#include <memory>
#include <string>

#include "core/constants.h"
#include "oatpp-swagger/Controller.hpp"
#include "oatpp-swagger/Model.hpp"
#include "oatpp-swagger/Resources.hpp"
#include "oatpp/web/server/HttpRouter.hpp"
#include "oatpp/web/server/api/ApiController.hpp"
#include "server/config/server_config.h"

namespace cppwiki::server {

namespace {

[[nodiscard]] auto MakeSwaggerServerUrl(const ServerConfig& config) -> std::string {
  std::string host = config.BindHost();
  if (host == "0.0.0.0") {
    host = "127.0.0.1";
  }

  return "http://" + host + ":" + std::to_string(config.Port());
}

[[nodiscard]] auto MakeSwaggerDocumentInfo(const ServerConfig& config)
    -> std::shared_ptr<oatpp::swagger::DocumentInfo> {
  oatpp::swagger::DocumentInfo::Builder builder;
  builder.setTitle(constants::kDefaultSwaggerTitle.data())
      .setDescription(constants::kDefaultSwaggerDescription.data())
      .setVersion(constants::kApplicationVersion.data())
      .addServer(MakeSwaggerServerUrl(config).c_str(), "CppWiki local server");
  return builder.build();
}

class SwaggerResourcesComponent final {
 public:
  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::swagger::Resources>, swaggerResources)([] {
    return oatpp::swagger::Resources::loadResources(OATPP_SWAGGER_RES_PATH);
  }());
};

}  // namespace

auto RegisterSwaggerEndpoints(
    const std::shared_ptr<oatpp::web::server::HttpRouter>& router,
    const oatpp::web::server::api::Endpoints& endpoints,
    const ServerConfig& config) -> void {
  if (!config.SwaggerEnabled()) {
    return;
  }

  oatpp::base::Environment::Component<std::shared_ptr<oatpp::swagger::DocumentInfo>>
      swagger_document_info_component(MakeSwaggerDocumentInfo(config));

  static SwaggerResourcesComponent swagger_resources_component;
  (void)swagger_resources_component;
  router->addController(oatpp::swagger::Controller::createShared(endpoints));
}

}  // namespace cppwiki::server
