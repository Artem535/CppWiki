#ifndef CPPWIKI_SRC_ADMIN_ADMIN_TOKEN_STORE_H_
#define CPPWIKI_SRC_ADMIN_ADMIN_TOKEN_STORE_H_

#include <optional>
#include <string>

namespace cppwiki::admin {

// Stores the cppwiki-admin session token.
//
// KNOWN LIMITATION (stage 1 MVP): this stores the token as plaintext in a file
// under the user's config directory with 0600 permissions, rather than
// reusing qt-keychain as originally intended in the Admin_Surface_Backlog
// decision. qtkeychain in this repo is only fetched/built when
// CPPWIKI_BUILD_DESKTOP_APP is ON, and it requires a full Qt6 find_package
// call (Core/DBus/Gui/Widgets/WebEngineWidgets/...) — pulling that into a
// lightweight ftxui CLI target would defeat the point of having a
// Qt-GUI-free admin tool. A follow-up could split qtkeychain's CMake wiring
// so only Qt6::Core/DBus is required, then swap this implementation to use
// QKeychain::ReadPasswordJob/WritePasswordJob like src/auth/AuthTokenStore.
class AdminTokenStore final {
 public:
  explicit AdminTokenStore(std::string token_file_path);

  // Returns the default per-user token file path
  // ($XDG_CONFIG_HOME/cppwiki-admin/token, falling back to
  // $HOME/.config/cppwiki-admin/token).
  [[nodiscard]] static auto DefaultTokenFilePath() -> std::string;

  [[nodiscard]] auto Load() const -> std::optional<std::string>;
  // Writes the token to disk, creating parent directories as needed and
  // restricting file permissions to the owner (chmod 0600).
  [[nodiscard]] auto Save(const std::string& token) const -> bool;
  [[nodiscard]] auto Clear() const -> bool;

 private:
  std::string token_file_path_;
};

}  // namespace cppwiki::admin

#endif  // CPPWIKI_SRC_ADMIN_ADMIN_TOKEN_STORE_H_
