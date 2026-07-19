#include "gui/workspace_rail_widget.h"

#include <QAction>
#include <QActionGroup>
#include <QIcon>
#include <QSize>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

namespace cppwiki::gui {

namespace {

// A plain, checkable, auto-raised QToolButton (same convention as MainWindow's
// statusLineButton) rather than oclero::qlementine::ActionButton: ActionButton is a filled
// call-to-action button by design (see its use for "Open folder" in SettingsDialog), always
// painted with a solid accent background regardless of checked state — wrong look for a flat
// nav rail. QToolButton delegates entirely to QStyle's normal drawControl()/drawPrimitive(),
// so it picks up checked/hover highlighting from the real QlementineStyle without needing the
// setStyle() pin that hand-painted qlementine widgets require.
auto MakeRailButton(QAction* action, QWidget* parent) -> QToolButton* {
  auto* button = new QToolButton(parent);
  button->setDefaultAction(action);
  button->setCheckable(true);
  button->setAutoRaise(true);
  button->setFixedSize(40, 40);
  button->setIconSize(QSize(20, 20));
  return button;
}

}  // namespace

WorkspaceRailWidget::WorkspaceRailWidget(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("workspaceRailWidget"));
  // Required for a plain QWidget to actually paint its QSS background-color at all — see
  // page_panel_'s and content_widget_'s matching setAttribute() calls in page.cc.
  setAttribute(Qt::WA_StyledBackground, true);
  setFixedWidth(56);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 12, 8, 12);
  layout->setSpacing(8);
  layout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

  action_group_ = new QActionGroup(this);
  action_group_->setExclusive(true);

  // Bundled monochrome icons (not QIcon::fromTheme()) so the rail matches the app's dark
  // theme instead of showing whatever multi-color icon set happens to be installed on the
  // system.
  documents_action_ = new QAction(QIcon(QStringLiteral(":/cppwiki/icons/rail-documents.svg")),
                                  QStringLiteral("Documents"), this);
  documents_action_->setCheckable(true);
  documents_action_->setChecked(true);
  documents_action_->setToolTip(QStringLiteral("Documents"));
  action_group_->addAction(documents_action_);

  ai_chat_action_ = new QAction(QIcon(QStringLiteral(":/cppwiki/icons/rail-ai-chat.svg")),
                                QStringLiteral("AI Chat"), this);
  ai_chat_action_->setCheckable(true);
  ai_chat_action_->setToolTip(QStringLiteral("AI Chat"));
  action_group_->addAction(ai_chat_action_);

  code_action_ = new QAction(QIcon(QStringLiteral(":/cppwiki/icons/rail-code.svg")),
                             QStringLiteral("Code"), this);
  code_action_->setCheckable(true);
  code_action_->setToolTip(QStringLiteral("Code"));
  action_group_->addAction(code_action_);

  documents_button_ = MakeRailButton(documents_action_, this);
  ai_chat_button_ = MakeRailButton(ai_chat_action_, this);
  code_button_ = MakeRailButton(code_action_, this);

  layout->addWidget(documents_button_);
  layout->addWidget(ai_chat_button_);
  layout->addWidget(code_button_);
  layout->addStretch(1);

  connect(documents_action_, &QAction::triggered, this,
          [this]() { SelectMode(Mode::kDocuments); });
  connect(ai_chat_action_, &QAction::triggered, this, [this]() { SelectMode(Mode::kAiChat); });
  connect(code_action_, &QAction::triggered, this, [this]() { SelectMode(Mode::kCode); });
}

void WorkspaceRailWidget::SetCurrentMode(Mode mode) {
  current_mode_ = mode;
  switch (mode) {
    case Mode::kDocuments:
      documents_action_->setChecked(true);
      break;
    case Mode::kAiChat:
      ai_chat_action_->setChecked(true);
      break;
    case Mode::kCode:
      code_action_->setChecked(true);
      break;
  }
}

void WorkspaceRailWidget::SelectMode(Mode mode) {
  if (current_mode_ == mode) {
    return;
  }
  current_mode_ = mode;
  emit modeSelected(mode);
}

}  // namespace cppwiki::gui
