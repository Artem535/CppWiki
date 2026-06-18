#ifndef CPPWIKI_SRC_SERVER_API_DTO_H_
#define CPPWIKI_SRC_SERVER_API_DTO_H_

#include "oatpp/core/Types.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include OATPP_CODEGEN_BEGIN(DTO)

namespace cppwiki::server::dto {

class HealthStatusDto : public oatpp::DTO {
  DTO_INIT(HealthStatusDto, DTO)

  DTO_FIELD(String, service);
  DTO_FIELD(String, status);
};

class HealthEnvelopeDto : public oatpp::DTO {
  DTO_INIT(HealthEnvelopeDto, DTO)

  DTO_FIELD(Int32, apiVersion);
  DTO_FIELD(Boolean, ok);
  DTO_FIELD(Object<HealthStatusDto>, result);
};

}  // namespace cppwiki::server::dto

#include OATPP_CODEGEN_END(DTO)

#endif  // CPPWIKI_SRC_SERVER_API_DTO_H_
