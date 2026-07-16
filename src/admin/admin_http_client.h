#ifndef CPPWIKI_SRC_ADMIN_ADMIN_HTTP_CLIENT_H_
#define CPPWIKI_SRC_ADMIN_ADMIN_HTTP_CLIENT_H_

#include <string>

namespace cppwiki::admin {

// Minimal libcurl-based HTTP client for cppwiki-admin talking to
// cppwiki_server's admin_handler/workspace_handler, following the same
// request shape as src/backend/BackendClient (Bearer auth header, JSON
// body/response), just without any Qt dependency.
class AdminHttpClient final {
 public:
  struct Response final {
    bool network_ok = false;
    long status_code = 0;
    std::string body;
    std::string network_error;
  };

  AdminHttpClient(std::string base_url, std::string access_token, long timeout_ms = 5000);

  void SetAccessToken(std::string access_token);
  void SetBaseUrl(std::string base_url);

  [[nodiscard]] auto Get(const std::string& path) const -> Response;
  [[nodiscard]] auto Post(const std::string& path, const std::string& json_body) const -> Response;

 private:
  [[nodiscard]] auto Perform(const std::string& path, const std::string* json_body) const
      -> Response;

  std::string base_url_;
  std::string access_token_;
  long timeout_ms_;
};

}  // namespace cppwiki::admin

#endif  // CPPWIKI_SRC_ADMIN_ADMIN_HTTP_CLIENT_H_
