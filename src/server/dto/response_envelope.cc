#include "server/dto/response_envelope.h"

namespace cppwiki::server::dto {

auto MakeErrorEnvelope(int api_version, ErrorDto error) -> ErrorEnvelope {
  return ErrorEnvelope{
      .api_version = api_version,
      .ok = false,
      .error = std::move(error),
  };
}

}  // namespace cppwiki::server::dto
