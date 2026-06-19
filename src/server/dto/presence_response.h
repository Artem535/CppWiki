#ifndef CPPWIKI_SRC_SERVER_DTO_PRESENCE_RESPONSE_H_
#define CPPWIKI_SRC_SERVER_DTO_PRESENCE_RESPONSE_H_

#include <string>
#include <vector>

#include <userver/formats/json/value.hpp>

#include "server/service/presence_service.h"

namespace cppwiki::server::dto {

auto MakePresenceResultJson(const std::string& workspace_id,
                            const std::vector<service::PresenceInfo>& entries)
    -> userver::formats::json::Value;

}  // namespace cppwiki::server::dto

#endif  // CPPWIKI_SRC_SERVER_DTO_PRESENCE_RESPONSE_H_
