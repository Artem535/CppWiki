#include "admin/admin_token_store.h"

#include <sys/stat.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace cppwiki::admin {

AdminTokenStore::AdminTokenStore(std::string token_file_path)
    : token_file_path_(std::move(token_file_path)) {}

auto AdminTokenStore::DefaultTokenFilePath() -> std::string {
  if (const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME");
      xdg_config_home != nullptr && *xdg_config_home != '\0') {
    return std::filesystem::path(xdg_config_home) / "cppwiki-admin" / "token";
  }

  if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
    return std::filesystem::path(home) / ".config" / "cppwiki-admin" / "token";
  }

  return "cppwiki-admin-token";
}

auto AdminTokenStore::Load() const -> std::optional<std::string> {
  std::ifstream file(token_file_path_, std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    return std::nullopt;
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  auto contents = buffer.str();
  while (!contents.empty() && (contents.back() == '\n' || contents.back() == '\r')) {
    contents.pop_back();
  }
  if (contents.empty()) {
    return std::nullopt;
  }
  return contents;
}

auto AdminTokenStore::Save(const std::string& token) const -> bool {
  std::error_code error_code;
  const std::filesystem::path path(token_file_path_);
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), error_code);
    if (error_code) {
      return false;
    }
  }

  std::ofstream file(token_file_path_, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    return false;
  }
  file << token;
  file.close();
  if (file.fail()) {
    return false;
  }

#ifndef _WIN32
  ::chmod(token_file_path_.c_str(), S_IRUSR | S_IWUSR);
#endif

  return true;
}

auto AdminTokenStore::Clear() const -> bool {
  std::error_code error_code;
  std::filesystem::remove(token_file_path_, error_code);
  return !error_code;
}

}  // namespace cppwiki::admin
