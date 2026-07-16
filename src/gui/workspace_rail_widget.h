#ifndef CPPWIKI_SRC_GUI_WORKSPACE_RAIL_WIDGET_H_
#define CPPWIKI_SRC_GUI_WORKSPACE_RAIL_WIDGET_H_

#include <QWidget>

class QAction;
class QActionGroup;

namespace oclero::qlementine {
class ActionButton;
}

namespace cppwiki::gui {

// The narrow left-edge icon rail (an IDE "activity bar" analog) for switching
// between MainWindow's full-view modes. Selecting an entry fully replaces the
// central content area rather than showing modes side by side. See
// ADR-014 ("Workspace rail" in CONTEXT.adoc).
class WorkspaceRailWidget final : public QWidget {
  Q_OBJECT

 public:
  enum class Mode {
    kDocuments,
    kAiChat,
    kCode,
  };

  explicit WorkspaceRailWidget(QWidget* parent = nullptr);

  Mode CurrentMode() const { return current_mode_; }
  void SetCurrentMode(Mode mode);

 signals:
  void modeSelected(cppwiki::gui::WorkspaceRailWidget::Mode mode);

 private:
  void SelectMode(Mode mode);

  QActionGroup* action_group_ = nullptr;
  QAction* documents_action_ = nullptr;
  QAction* ai_chat_action_ = nullptr;
  QAction* code_action_ = nullptr;
  oclero::qlementine::ActionButton* documents_button_ = nullptr;
  oclero::qlementine::ActionButton* ai_chat_button_ = nullptr;
  oclero::qlementine::ActionButton* code_button_ = nullptr;
  Mode current_mode_ = Mode::kDocuments;
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_WORKSPACE_RAIL_WIDGET_H_
