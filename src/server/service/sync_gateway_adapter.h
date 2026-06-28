#ifndef CPPWIKI_SRC_SERVER_SERVICE_SYNC_GATEWAY_ADAPTER_H_
#define CPPWIKI_SRC_SERVER_SERVICE_SYNC_GATEWAY_ADAPTER_H_

#include <map>
#include <string>
#include <vector>

#include "server/components/sync_bootstrap_component.h"
#include "server/dto/sync_config_response.h"
#include "server/middleware/auth_checker_impl.h"

namespace cppwiki::server::service {

class SyncGatewayAdapter final {
 public:
  explicit SyncGatewayAdapter(const components::SyncBootstrapState& state);

  [[nodiscard]] auto BuildBootstrap(const middleware::JwtPrincipal& principal) const
      -> dto::SyncConfigResultDto;

 private:
  [[nodiscard]] auto DeriveChannels(const middleware::JwtPrincipal& principal) const
      -> std::vector<std::string>;

  components::SyncBootstrapState state_;
};

}  // namespace cppwiki::server::service

#endif  // CPPWIKI_SRC_SERVER_SERVICE_SYNC_GATEWAY_ADAPTER_H_
