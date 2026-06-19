#ifndef CPPWIKI_SRC_SERVER_COMPONENTS_CPPWIKI_SERVER_COMPONENT_H_
#define CPPWIKI_SRC_SERVER_COMPONENTS_CPPWIKI_SERVER_COMPONENT_H_

#include <string>

#include <userver/components/component_list.hpp>

namespace cppwiki::server::components {

auto GetStaticConfigComponentName() -> const std::string&;
auto RegisterCppWikiComponents(userver::components::ComponentList& component_list)
    -> userver::components::ComponentList&;

}  // namespace cppwiki::server::components

#endif  // CPPWIKI_SRC_SERVER_COMPONENTS_CPPWIKI_SERVER_COMPONENT_H_
