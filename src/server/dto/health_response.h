#ifndef CPPWIKI_SRC_SERVER_DTO_HEALTH_RESPONSE_H_
#define CPPWIKI_SRC_SERVER_DTO_HEALTH_RESPONSE_H_

#include <string>

#include <rfl.hpp>

namespace cppwiki::server::dto {

struct HealthResponse {
  std::string status;
  std::string version;
};

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_HEALTH_RESPONSE_H_
