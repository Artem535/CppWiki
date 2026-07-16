#include "admin/admin_http_client.h"

#include <curl/curl.h>

#include <utility>

#include "admin/admin_response_parsing.h"

namespace cppwiki::admin {

namespace {

auto WriteCallback(char* contents, size_t size, size_t nmemb, void* user_data) -> size_t {
  auto* body = static_cast<std::string*>(user_data);
  body->append(contents, size * nmemb);
  return size * nmemb;
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
  Response response;

  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    response.network_error = "Failed to initialize HTTP client (curl_easy_init failed).";
    return response;
  }

  const auto url = BuildApiUrl(base_url_, path);
  std::string response_body;

  curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  if (!access_token_.empty()) {
    const auto auth_header = "Authorization: Bearer " + access_token_;
    headers = curl_slist_append(headers, auth_header.c_str());
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms_);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms_);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

  if (json_body != nullptr) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body->c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json_body->size()));
  }

  const CURLcode result = curl_easy_perform(curl);
  if (result != CURLE_OK) {
    response.network_ok = false;
    response.network_error = curl_easy_strerror(result);
  } else {
    response.network_ok = true;
    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    response.status_code = status_code;
    response.body = std::move(response_body);
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return response;
}

}  // namespace cppwiki::admin
