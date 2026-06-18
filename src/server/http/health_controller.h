#ifndef CPPWIKI_SRC_SERVER_HEALTH_CONTROLLER_H_
#define CPPWIKI_SRC_SERVER_HEALTH_CONTROLLER_H_

#include <memory>

#include "core/constants.h"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/web/server/api/ApiController.hpp"
#include "server/dto/api_dto.h"

#include OATPP_CODEGEN_BEGIN(ApiController)

namespace cppwiki::server {

class HealthController : public oatpp::web::server::api::ApiController {
 public:
  explicit HealthController(
      const std::shared_ptr<oatpp::parser::json::mapping::ObjectMapper>& object_mapper)
      : oatpp::web::server::api::ApiController(object_mapper) {}

 private:
  auto ApplyCorsHeaders(const std::shared_ptr<OutgoingResponse>& response) const -> void {
    response->putHeader("Access-Control-Allow-Origin", "*");
    response->putHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    response->putHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
  }

 public:

  ENDPOINT_INFO(GetHealth) {
    info->summary = "Health check";
    info->description = "Returns the current liveness status of the CppWiki server.";
    info->addResponse<Object<dto::HealthEnvelopeDto>>(Status::CODE_200, "application/json");
  }

  ENDPOINT("GET", "/api/v1/health", GetHealth) {
    auto result = dto::HealthStatusDto::createShared();
    result->service = oatpp::String(constants::kServerServiceName.data());
    result->status = oatpp::String("ok");

    auto response = dto::HealthEnvelopeDto::createShared();
    response->apiVersion = constants::kServerApiVersion;
    response->ok = true;
    response->result = result;

    auto outgoing_response = createDtoResponse(Status::CODE_200, response);
    ApplyCorsHeaders(outgoing_response);
    return outgoing_response;
  }

  ENDPOINT_INFO(OptionsHealth) {
    info->summary = "Health check preflight";
    info->description = "Returns CORS headers for health check requests.";
    info->addResponse(Status::CODE_204, "text/plain");
  }

  ENDPOINT("OPTIONS", "/api/v1/health", OptionsHealth) {
    auto response = createResponse(Status::CODE_204);
    ApplyCorsHeaders(response);
    return response;
  }
};

}  // namespace cppwiki::server

#include OATPP_CODEGEN_END(ApiController)

#endif  // CPPWIKI_SRC_SERVER_HEALTH_CONTROLLER_H_
