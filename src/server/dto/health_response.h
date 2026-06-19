#ifndef CPPWIKI_SRC_SERVER_DTO_HEALTH_RESPONSE_H_
#define CPPWIKI_SRC_SERVER_DTO_HEALTH_RESPONSE_H_

#include <userver/formats/json/value.hpp>

namespace cppwiki::server::dto {

auto MakeHealthResult() -> userver::formats::json::Value;

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_HEALTH_RESPONSE_H_
