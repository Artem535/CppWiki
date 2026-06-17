#ifndef CPPWIKI_SRC_APP_APPLICATION_STYLESHEET_H_
#define CPPWIKI_SRC_APP_APPLICATION_STYLESHEET_H_

#include <QString>

namespace cppwiki {

[[nodiscard]] auto ResolveApplicationStylesheetPath() -> QString;
void ApplyApplicationStylesheet();

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_APP_APPLICATION_STYLESHEET_H_
