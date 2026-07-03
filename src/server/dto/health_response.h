#ifndef CPPWIKI_SRC_SERVER_DTO_HEALTH_RESPONSE_H_
#define CPPWIKI_SRC_SERVER_DTO_HEALTH_RESPONSE_H_

#include <string>

namespace cppwiki::server::dto {

struct HealthResult final {
  std::string service;
  std::string status;
};

auto MakeHealthResult() -> HealthResult;

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_HEALTH_RESPONSE_H_
