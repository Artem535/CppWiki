#ifndef CPPWIKI_SRC_GUI_PROPERTIES_STRIP_WIDGET_H_
#define CPPWIKI_SRC_GUI_PROPERTIES_STRIP_WIDGET_H_

#include <memory>

#include <QFrame>
#include <QString>

class QHBoxLayout;

namespace cppwiki::storage {
class LocalDocumentRepository;
}  // namespace cppwiki::storage

namespace cppwiki::gui {

// Companion to PresenceStripWidget: renders the currently open wiki page's assigned properties
// (Status/Owner/Type/Tags/...) as the same kind of compact, fixed-height chip that widget uses
// for avatars, in the same collaboration_panel_ chrome (see MainWindow::BuildUi()) -- object
// metadata is chrome, not document content, so it doesn't belong inside the QWebEngineView-hosted
// page. Reads the knowledge repository directly (no bridge/JS round trip); property *edits* still
// happen in the JS-side PropertiesDrawer, reached through the trailing "Properties" button.
class PropertiesStripWidget final : public QFrame {
  Q_OBJECT

 public:
  explicit PropertiesStripWidget(QWidget* parent = nullptr);

  void SetRepository(std::shared_ptr<storage::LocalDocumentRepository> repository);
  // Called from Page::documentPropertiesContextChanged whenever the open document changes.
  void SetDocumentContext(const QString& workspace_id, const QString& page_id, bool has_document);
  // Re-fetches for the context set by the last SetDocumentContext() call -- called from
  // Page::propertiesChanged after an edit lands in the drawer.
  void RefreshCurrent();

 signals:
  void propertiesRequested();

 private:
  // Mirrors PresenceStripWidget::kMaxVisibleAvatars: beyond this many assigned properties, the
  // rest collapse into a trailing "+N" label rather than growing the strip unboundedly.
  static constexpr int kMaxVisibleChips = 4;

  void Rebuild();
  [[nodiscard]] QWidget* CreateChip(const QString& name, const QString& value,
                                    const QString& color_name);

  std::shared_ptr<storage::LocalDocumentRepository> repository_;
  QString workspace_id_;
  QString page_id_;
  bool has_document_ = false;
  QHBoxLayout* root_layout_ = nullptr;
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_PROPERTIES_STRIP_WIDGET_H_
