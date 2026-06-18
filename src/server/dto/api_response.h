#ifndef CPPWIKI_SRC_SERVER_DTO_API_RESPONSE_H_
#define CPPWIKI_SRC_SERVER_DTO_API_RESPONSE_H_

#include <rfl.hpp>
#include <rfl/json.hpp>

namespace cppwiki::server::dto {

struct ApiError {
  std::string code;
  std::string message;
};

template <typename T>
struct ApiResponse {
  std::string api_version = "v1";
  bool ok = false;
  std::optional<ApiError> error;
  std::optional<T> result;
};

// Returns a JSON string representing a successful response with the given
// result payload. Requires T to be reflect-cpp serializable.
template <typename T>
[[nodiscard]] inline auto SuccessJson(const T& result) -> std::string {
  ApiResponse<T> response;
  response.ok = true;
  response.result = result;
  return rfl::json::write(response);
}

// Returns a JSON string representing an error response.
[[nodiscard]] inline auto ErrorJson(std::string_view code,
                                    std::string_view message) -> std::string {
  ApiResponse<std::monostate> response;
  response.ok = false;
  response.error = ApiError{std::string{code}, std::string{message}};
  return rfl::json::write(response);
}

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_API_RESPONSE_H_
