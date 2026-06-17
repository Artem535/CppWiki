#ifndef CPPWIKI_SRC_APP_EDITOR_FALLBACK_H_
#define CPPWIKI_SRC_APP_EDITOR_FALLBACK_H_

#include <QString>

namespace cppwiki {

[[nodiscard]] auto ResolveEditorFallbackHtmlPath() -> QString;
[[nodiscard]] auto LoadEditorFallbackHtml(const QString& expected_path) -> QString;

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_APP_EDITOR_FALLBACK_H_
