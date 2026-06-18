#include "server/server_application.h"

auto main() -> int {
  cppwiki::server::ServerApplication app(
      cppwiki::server::ServerConfig{});
  return app.Run();
}
