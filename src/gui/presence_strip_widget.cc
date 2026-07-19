#include "gui/presence_strip_widget.h"

#include <algorithm>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QStyle>
#include <QWidget>

namespace cppwiki::gui {

PresenceStripWidget::PresenceStripWidget(QWidget* parent) : QFrame(parent) {
  setObjectName(QStringLiteral("presenceStripWidget"));
  setFrameShape(QFrame::NoFrame);
  setProperty("collaborationState", QStringLiteral("idle"));

  root_layout_ = new QHBoxLayout(this);
  root_layout_->setContentsMargins(0, 0, 0, 0);
  // Negative spacing produces the ~8px avatar overlap called for by ADR-016's "compact
  // overlapping avatar cluster" (avatar width is 28px, so -8 leaves 20px of visible offset per
  // avatar).
  root_layout_->setSpacing(-8);

  Rebuild();
}

void PresenceStripWidget::SetCollaborationState(const QString& state) {
  setProperty("collaborationState", state);
  style()->unpolish(this);
  style()->polish(this);
  update();
}

void PresenceStripWidget::SetEditor(const QString& name, bool is_self) {
  const auto trimmed = name.trimmed();
  editor_name_ = trimmed;
  editor_is_self_ = is_self;
  editor_active_ = !trimmed.isEmpty();
  Rebuild();
}

void PresenceStripWidget::ClearEditor() {
  editor_name_.clear();
  editor_is_self_ = false;
  editor_active_ = false;
  Rebuild();
}

void PresenceStripWidget::SetViewers(const QStringList& names) {
  viewers_ = names;
  Rebuild();
}

QWidget* PresenceStripWidget::CreateAvatar(const QString& text, const QString& role,
                                           const QString& tooltip) {
  auto* avatar = new QWidget(this);
  avatar->setObjectName(QStringLiteral("presenceAvatar"));
  avatar->setProperty("presenceRole", role);
  avatar->setFixedSize(28, 28);
  avatar->setToolTip(tooltip);

  auto* layout = new QGridLayout(avatar);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto* label = new QLabel(text, avatar);
  label->setObjectName(QStringLiteral("avatarLabel"));
  label->setAlignment(Qt::AlignCenter);
  layout->addWidget(label, 0, 0, Qt::AlignCenter);

  return avatar;
}

void PresenceStripWidget::Rebuild() {
  while (root_layout_->count() > 0) {
    auto* item = root_layout_->takeAt(0);
    if (item == nullptr) {
      continue;
    }
    if (auto* widget = item->widget(); widget != nullptr) {
      widget->deleteLater();
    }
    delete item;
  }

  int rendered_count = 0;

  if (editor_active_) {
    const auto display_name = editor_is_self_ ? QStringLiteral("You") : editor_name_;
    const auto tooltip =
        QStringLiteral("%1 (editing)").arg(editor_is_self_ ? QStringLiteral("You") : editor_name_);
    auto* avatar = CreateAvatar(MakeAvatarText(display_name), QStringLiteral("editing"), tooltip);
    root_layout_->addWidget(avatar, 0, Qt::AlignVCenter);
    ++rendered_count;
  }

  const auto viewer_slots = std::max(0, kMaxVisibleAvatars - rendered_count);
  const auto visible_viewer_count = std::min<qsizetype>(viewers_.size(), viewer_slots);
  for (qsizetype index = 0; index < visible_viewer_count; ++index) {
    const auto& viewer_name = viewers_.at(index);
    auto* avatar = CreateAvatar(MakeAvatarText(viewer_name), QStringLiteral("viewing"),
                                QStringLiteral("%1 (viewing)").arg(viewer_name));
    root_layout_->addWidget(avatar, 0, Qt::AlignVCenter);
    ++rendered_count;
  }

  const auto overflow_count = viewers_.size() - visible_viewer_count;
  if (overflow_count > 0) {
    auto* avatar = CreateAvatar(QStringLiteral("+%1").arg(overflow_count),
                                QStringLiteral("viewing"),
                                QStringLiteral("%1 more viewer(s)").arg(overflow_count));
    root_layout_->addWidget(avatar, 0, Qt::AlignVCenter);
  }

  if (rendered_count == 0 && overflow_count <= 0) {
    auto* avatar =
        CreateAvatar(QStringLiteral("-"), QStringLiteral("idle"), QStringLiteral("No one here"));
    root_layout_->addWidget(avatar, 0, Qt::AlignVCenter);
  }

  root_layout_->addStretch(0);
}

QString PresenceStripWidget::MakeAvatarText(const QString& name) {
  const auto trimmed = name.trimmed();
  if (trimmed.isEmpty()) {
    return QStringLiteral("-");
  }

  const auto parts = trimmed.split(QChar(' '), Qt::SkipEmptyParts);
  if (parts.isEmpty()) {
    return trimmed.left(1).toUpper();
  }

  QString initials;
  for (const auto& part : parts) {
    initials += part.left(1).toUpper();
    if (initials.size() >= 2) {
      break;
    }
  }
  return initials.isEmpty() ? trimmed.left(1).toUpper() : initials;
}

}  // namespace cppwiki::gui
