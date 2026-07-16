#include "gui/workspace_rail_widget.h"

#include <QAction>
#include <QActionGroup>
#include <QIcon>
#include <QSize>
#include <QVBoxLayout>
#include <QWidget>
#include <oclero/qlementine/widgets/ActionButton.hpp>

namespace cppwiki::gui {

namespace {

auto MakeRailButton(QAction* action, QWidget* parent) -> oclero::qlementine::ActionButton* {
  auto* button = new oclero::qlementine::ActionButton(parent);
  button->setAction(action);
  button->setFixedSize(40, 40);
  button->setIconSize(QSize(20, 20));
  return button;
}

}  // namespace

WorkspaceRailWidget::WorkspaceRailWidget(QWidget* parent) : QWidget(parent) {
  setObjectName(QStringLiteral("workspaceRailWidget"));
  setFixedWidth(56);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 12, 8, 12);
  layout->setSpacing(8);
  layout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

  action_group_ = new QActionGroup(this);
  action_group_->setExclusive(true);

  documents_action_ =
      new QAction(QIcon::fromTheme(QStringLiteral("folder")), QStringLiteral("Documents"), this);
  documents_action_->setCheckable(true);
  documents_action_->setChecked(true);
  documents_action_->setToolTip(QStringLiteral("Documents"));
  action_group_->addAction(documents_action_);

  ai_chat_action_ = new QAction(QIcon::fromTheme(QStringLiteral("mail-message-new")),
                                QStringLiteral("AI Chat"), this);
  ai_chat_action_->setCheckable(true);
  ai_chat_action_->setToolTip(QStringLiteral("AI Chat"));
  action_group_->addAction(ai_chat_action_);

  code_action_ = new QAction(QIcon::fromTheme(QStringLiteral("utilities-terminal")),
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
