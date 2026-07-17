#ifndef CPPWIKI_SRC_GUI_PRESENCE_STRIP_WIDGET_H_
#define CPPWIKI_SRC_GUI_PRESENCE_STRIP_WIDGET_H_

#include <QFrame>
#include <QStringList>

class QLabel;
class QHBoxLayout;

namespace cppwiki::gui {

// ADR-016 "presence indicator: overlapping avatar cluster". Renders the editor topbar's
// collaborator presence as a compact, overlapping (Figma/Notion-style) avatar cluster instead of
// the previously-shipped "EDITOR"/"VIEWERS" text-labeled dot pairs: whoever currently holds the
// edit lock gets a green ring, viewers get a neutral/grey ring — role is conveyed by ring color,
// not by a text label grouping.
class PresenceStripWidget final : public QFrame {
  Q_OBJECT

 public:
  explicit PresenceStripWidget(QWidget* parent = nullptr);

  void SetCollaborationState(const QString& state);
  void SetEditor(const QString& name, bool is_self);
  void ClearEditor();
  void SetViewers(const QStringList& names);

 private:
  // Maximum number of avatars actually rendered in the cluster (editor + viewers); any
  // additional viewers collapse into a trailing "+N" overflow avatar, keeping the cluster
  // compact regardless of how many collaborators are present.
  static constexpr int kMaxVisibleAvatars = 4;

  QWidget* CreateAvatar(const QString& text, const QString& role, const QString& tooltip);
  void Rebuild();
  [[nodiscard]] static QString MakeAvatarText(const QString& name);

  QHBoxLayout* root_layout_ = nullptr;
  QString editor_name_;
  bool editor_is_self_ = false;
  bool editor_active_ = false;
  QStringList viewers_;
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_PRESENCE_STRIP_WIDGET_H_
