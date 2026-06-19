#include "server/dto/health_response.h"

#include "core/constants.h"

namespace cppwiki::server::dto {

auto MakeHealthResult() -> HealthResult {
  return HealthResult{
      .service = std::string(cppwiki::constants::kServerServiceName),
      .status = "ok",
  };
}

}  // namespace cppwiki::server::dto
