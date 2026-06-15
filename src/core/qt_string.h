#ifndef CPPWIKI_SRC_CORE_QT_STRING_H_
#define CPPWIKI_SRC_CORE_QT_STRING_H_

#include <QByteArrayView>
#include <QString>
#include <string_view>

namespace cppwiki {

[[nodiscard]] inline auto ToQString(std::string_view value) -> QString {
  return QString::fromUtf8(QByteArrayView(value));
}

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_CORE_QT_STRING_H_
