#include "admin/admin_http_client.h"

#include <cstdint>
#include <utility>

#include <cpr/cpr.h>

#include "admin/admin_response_parsing.h"

namespace cppwiki::admin {

namespace {

auto BuildHeaders(const std::string& access_token) -> cpr::Header {
  cpr::Header headers{{"Content-Type", "application/json"}};
  if (!access_token.empty()) {
    headers["Authorization"] = "Bearer " + access_token;
  }
  return headers;
}

auto ToResponse(const cpr::Response& cpr_response) -> AdminHttpClient::Response {
  AdminHttpClient::Response response;

  if (cpr_response.error) {
    response.network_ok = false;
    response.network_error = cpr_response.error.message;
    return response;
  }

  response.network_ok = true;
  response.status_code = static_cast<long>(cpr_response.status_code);
  response.body = cpr_response.text;
  return response;
}

}  // namespace

AdminHttpClient::AdminHttpClient(std::string base_url, std::string access_token, long timeout_ms)
    : base_url_(std::move(base_url)),
      access_token_(std::move(access_token)),
      timeout_ms_(timeout_ms) {}

void AdminHttpClient::SetAccessToken(std::string access_token) {
  access_token_ = std::move(access_token);
}

void AdminHttpClient::SetBaseUrl(std::string base_url) {
  base_url_ = std::move(base_url);
}

auto AdminHttpClient::Get(const std::string& path) const -> Response {
  return Perform(path, nullptr);
}

auto AdminHttpClient::Post(const std::string& path, const std::string& json_body) const
    -> Response {
  return Perform(path, &json_body);
}

auto AdminHttpClient::Perform(const std::string& path, const std::string* json_body) const
    -> Response {
  const auto url = BuildApiUrl(base_url_, path);
  const auto headers = BuildHeaders(access_token_);
  const auto timeout = cpr::Timeout{static_cast<int32_t>(timeout_ms_)};
  const auto connect_timeout = cpr::ConnectTimeout{static_cast<int32_t>(timeout_ms_)};

  cpr::Response cpr_response;
  if (json_body != nullptr) {
    cpr_response = cpr::Post(
        cpr::Url{url}, headers, timeout, connect_timeout, cpr::Body{*json_body});
  } else {
    cpr_response = cpr::Get(cpr::Url{url}, headers, timeout, connect_timeout);
  }

  return ToResponse(cpr_response);
}

}  // namespace cppwiki::admin
