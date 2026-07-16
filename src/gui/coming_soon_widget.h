#ifndef CPPWIKI_SRC_GUI_COMING_SOON_WIDGET_H_
#define CPPWIKI_SRC_GUI_COMING_SOON_WIDGET_H_

#include <QString>
#include <QWidget>

namespace cppwiki::gui {

// Placeholder content for a workspace-rail mode whose real content is not
// implemented yet (AI Chat, Code — see ADR-014/ADR-015 and issues #29-#32).
// Exercises the rail's mode-swap mechanism end-to-end without any agent/tool
// logic.
class ComingSoonWidget final : public QWidget {
  Q_OBJECT

 public:
  ComingSoonWidget(const QString& title, const QString& subtitle, QWidget* parent = nullptr);
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_COMING_SOON_WIDGET_H_
