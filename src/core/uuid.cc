#include "core/uuid.h"

#include <QUuid>

namespace cppwiki {

auto GenerateUuidString() -> std::string {
  return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}

}  // namespace cppwiki
