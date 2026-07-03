#ifndef CPPWIKI_SRC_SERVER_DTO_WORKSPACE_RESPONSE_H_
#define CPPWIKI_SRC_SERVER_DTO_WORKSPACE_RESPONSE_H_

#include <rfl/Rename.hpp>

#include <string>
#include <vector>

namespace cppwiki::server::dto {

struct WorkspaceDto final {
  std::string id;
};

struct WorkspaceListResultDto final {
  std::vector<WorkspaceDto> workspaces;
};

struct WorkspaceCreateRequestDto final {
  std::string id;
};

struct WorkspaceCreateResultDto final {
  WorkspaceDto workspace;
  rfl::Rename<"created", bool> created{true};
};

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_WORKSPACE_RESPONSE_H_
