#ifndef CPPWIKI_SRC_GUI_REVISION_HISTORY_DIALOG_H_
#define CPPWIKI_SRC_GUI_REVISION_HISTORY_DIALOG_H_

#include <QDialog>
#include <QPointer>
#include <QString>

class QVBoxLayout;
class QWidget;

namespace cppwiki::bridge {
class QEditorBridge;
}

namespace cppwiki::gui {

// Issue #166: a minimal, native view onto one document's revision history -- lists prior
// snapshots recorded by EditorBridge::updateSnapshot()/restoreDocumentRevision(), each
// restorable. No diff viewing, no naming/tagging (see the issue's explicit out-of-scope list).
// All operations go through EditorBridge, which owns the revision-recording/pruning/restore
// logic; this dialog only renders the result and re-lists after a restore.
class RevisionHistoryDialog final : public QDialog {
  Q_OBJECT

 public:
  // `bridge` must outlive this dialog -- see TrashDialog's identical constraint.
  RevisionHistoryDialog(bridge::QEditorBridge* bridge, QString page_id, QWidget* parent = nullptr);

 private:
  void RefreshList();
  void RestoreRevision(const QString& revision_id);

  QPointer<bridge::QEditorBridge> bridge_;
  QString page_id_;
  QVBoxLayout* rows_layout_ = nullptr;
  QWidget* rows_container_ = nullptr;
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_REVISION_HISTORY_DIALOG_H_
