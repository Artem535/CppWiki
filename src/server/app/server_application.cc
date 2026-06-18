#include "server/app/server_application.h"

#include <spdlog/spdlog.h>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/components/run.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>

#include "core/logging.h"
#include "server/components/cppwiki_server_component.h"

namespace cppwiki::server {

namespace {

auto SaveStaticConfig(const std::string& yaml) -> std::filesystem::path {
  auto path = std::filesystem::temp_directory_path() / "cppwiki_server_static_config.yaml";
  std::ofstream out(path);
  out << yaml;
  return path;
}

}  // namespace

ServerApplication::ServerApplication(config::RuntimeConfig config) : config_(std::move(config)) {}

auto ServerApplication::Run() const -> int {
  cppwiki::logging::ConfigureBaseLogging();
  cppwiki::logging::ConfigureLogLevel(config_.LogLevel());

  const auto static_config = config_.ToStaticConfigYaml();
  const auto config_path = SaveStaticConfig(static_config);

  spdlog::info("Starting cppwiki-server on http://{}:{}/api/v1/health", config_.Host(),
               config_.Port());

  auto component_list = userver::components::MinimalServerComponentList();
  components::RegisterCppWikiComponents(component_list);

  userver::components::Run(config_path.string(), std::nullopt, std::nullopt, component_list);
  return 0;
}

}  // namespace cppwiki::server
