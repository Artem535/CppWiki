#ifndef CPPWIKI_SRC_GUI_PRESENCE_STRIP_WIDGET_H_
#define CPPWIKI_SRC_GUI_PRESENCE_STRIP_WIDGET_H_

#include <QFrame>
#include <QStringList>

class QLabel;
class QHBoxLayout;

namespace cppwiki::gui {

class PresenceStripWidget final : public QFrame {
  Q_OBJECT

 public:
  explicit PresenceStripWidget(QWidget* parent = nullptr);

  void SetEditor(const QString& name, bool is_self);
  void ClearEditor();
  void SetViewers(const QStringList& names);

 private:
  QWidget* CreateAvatar(const QString& object_name, const QString& text);
  void UpdateViewerAvatars();
  [[nodiscard]] static QString MakeAvatarText(const QString& name);

  QHBoxLayout* root_layout_ = nullptr;
  QHBoxLayout* viewer_avatars_layout_ = nullptr;
  QWidget* editor_avatar_ = nullptr;
  QLabel* editor_avatar_label_ = nullptr;
  QStringList viewers_;
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_PRESENCE_STRIP_WIDGET_H_
