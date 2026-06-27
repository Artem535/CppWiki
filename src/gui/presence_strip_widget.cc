#include "gui/presence_strip_widget.h"

#include <algorithm>

#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QGridLayout>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>

namespace cppwiki::gui {

PresenceStripWidget::PresenceStripWidget(QWidget* parent) : QFrame(parent) {
  setObjectName(QStringLiteral("presenceStripWidget"));
  setFrameShape(QFrame::NoFrame);

  root_layout_ = new QHBoxLayout(this);
  root_layout_->setContentsMargins(14, 8, 14, 8);
  root_layout_->setSpacing(10);
  root_layout_->addStretch(1);

  auto* editor_section = new QWidget(this);
  auto* editor_layout = new QHBoxLayout(editor_section);
  editor_layout->setContentsMargins(0, 0, 0, 0);
  editor_layout->setSpacing(0);

  editor_avatar_ = CreateAvatar(QStringLiteral("presenceEditorAvatar"), QStringLiteral("-"));
  editor_avatar_label_ = editor_avatar_->findChild<QLabel*>(QStringLiteral("avatarLabel"));
  editor_layout->addWidget(editor_avatar_, 0, Qt::AlignVCenter);
  editor_section->setToolTip(QStringLiteral("No active editor"));

  auto* viewers_section = new QWidget(this);
  auto* viewers_layout = new QHBoxLayout(viewers_section);
  viewers_layout->setContentsMargins(0, 0, 0, 0);
  viewers_layout->setSpacing(0);

  auto* viewers_avatars = new QWidget(viewers_section);
  viewer_avatars_layout_ = new QHBoxLayout(viewers_avatars);
  viewer_avatars_layout_->setContentsMargins(0, 0, 0, 0);
  viewer_avatars_layout_->setSpacing(-8);
  viewers_layout->addWidget(viewers_avatars, 0, Qt::AlignVCenter);
  viewers_section->setToolTip(QStringLiteral("No viewers"));

  root_layout_->addWidget(editor_section, 0, Qt::AlignVCenter);
  root_layout_->addWidget(viewers_section, 0, Qt::AlignVCenter);

  ClearEditor();
  SetViewers({});
}

void PresenceStripWidget::SetEditor(const QString& name, bool is_self) {
  const auto trimmed = name.trimmed();
  const auto display_name =
      trimmed.isEmpty() ? QStringLiteral("No active editor")
                        : (is_self ? QStringLiteral("You") : trimmed);
  if (editor_avatar_label_ != nullptr) {
    editor_avatar_label_->setText(MakeAvatarText(display_name));
  }
  editor_avatar_->setToolTip(display_name);
  editor_avatar_->setProperty("presenceActive", !trimmed.isEmpty());
  editor_avatar_->setProperty("presenceSelf", is_self);
  style()->unpolish(editor_avatar_);
  style()->polish(editor_avatar_);
}

void PresenceStripWidget::ClearEditor() {
  SetEditor({}, false);
  editor_avatar_->setProperty("presenceActive", false);
  editor_avatar_->setProperty("presenceSelf", false);
  style()->unpolish(editor_avatar_);
  style()->polish(editor_avatar_);
}

void PresenceStripWidget::SetViewers(const QStringList& names) {
  viewers_ = names;
  UpdateViewerAvatars();
}

QWidget* PresenceStripWidget::CreateAvatar(const QString& object_name, const QString& text) {
  auto* avatar = new QWidget(this);
  avatar->setObjectName(object_name);
  avatar->setProperty("presenceActive", false);
  avatar->setProperty("presenceSelf", false);
  avatar->setFixedSize(28, 28);

  auto* layout = new QGridLayout(avatar);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto* label = new QLabel(text, avatar);
  label->setObjectName(QStringLiteral("avatarLabel"));
  label->setAlignment(Qt::AlignCenter);
  layout->addWidget(label, 0, 0, Qt::AlignCenter);

  if (object_name == QStringLiteral("presenceEditorAvatar")) {
    auto* dot = new QWidget(avatar);
    dot->setObjectName(QStringLiteral("presenceOnlineDot"));
    dot->setFixedSize(8, 8);
    dot->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(dot, 0, 0, Qt::AlignBottom | Qt::AlignRight);
  }

  return avatar;
}

void PresenceStripWidget::UpdateViewerAvatars() {
  while (viewer_avatars_layout_->count() > 0) {
    auto* item = viewer_avatars_layout_->takeAt(0);
    if (item == nullptr) {
      continue;
    }
    if (auto* widget = item->widget(); widget != nullptr) {
      widget->deleteLater();
    }
    delete item;
  }

  if (viewers_.isEmpty()) {
    auto* avatar = CreateAvatar(QStringLiteral("presenceViewerAvatar"), QStringLiteral("-"));
    if (auto* label = avatar->findChild<QLabel*>(QStringLiteral("avatarLabel")); label != nullptr) {
      label->setText(QStringLiteral("-"));
    }
    avatar->setToolTip(QStringLiteral("No viewers"));
    viewer_avatars_layout_->addWidget(avatar, 0, Qt::AlignVCenter);
    return;
  }

  const auto visible_count = std::min<qsizetype>(viewers_.size(), 3);
  for (qsizetype index = visible_count - 1; index >= 0; --index) {
    auto* avatar = CreateAvatar(QStringLiteral("presenceViewerAvatar"),
                                MakeAvatarText(viewers_.at(index)));
    avatar->setProperty("presenceActive", true);
    avatar->setToolTip(viewers_.at(index));
    style()->unpolish(avatar);
    style()->polish(avatar);
    viewer_avatars_layout_->addWidget(avatar, 0, Qt::AlignVCenter);
    if (index == 0) {
      break;
    }
  }
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
