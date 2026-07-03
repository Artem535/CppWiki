#ifndef CPPWIKI_SRC_GUI_MERGE_CONFLICT_MERGE_DIALOG_H_
#define CPPWIKI_SRC_GUI_MERGE_CONFLICT_MERGE_DIALOG_H_

#include <QDialog>
#include <QPointer>

#include <memory>

#include "storage/local_document_repository.h"

class QLabel;
class QQuickWidget;

namespace cppwiki::gui::merge {
class ConflictMergeModel;
}

namespace cppwiki::gui::merge {

class ConflictMergeDialog final : public QDialog {
  Q_OBJECT
  Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
  Q_PROPERTY(QString conflictTitle READ conflictTitle NOTIFY statusMessageChanged)

 public:
  ConflictMergeDialog(std::shared_ptr<storage::LocalDocumentRepository> repository,
                      QString conflict_id, QString author_id = {},
                      QWidget* parent = nullptr);
  ~ConflictMergeDialog() override;

  [[nodiscard]] auto statusMessage() const -> QString;
  [[nodiscard]] auto busy() const -> bool;
  [[nodiscard]] auto conflictTitle() const -> QString;

  Q_INVOKABLE void setResolution(int row, const QString& resolution);
  Q_INVOKABLE void useAllLocal();
  Q_INVOKABLE void useAllRemote();
  Q_INVOKABLE void saveMerge();
  Q_INVOKABLE void cancelMerge();

 signals:
  void statusMessageChanged();
  void busyChanged();
  void mergeApplied();

 private:
  void SetStatusMessage(QString message);
  void SetBusy(bool busy);
  void Initialize();

  std::shared_ptr<storage::LocalDocumentRepository> repository_;
  QString conflict_id_;
  QString author_id_;
  QString status_message_;
  bool busy_{false};
  QPointer<ConflictMergeModel> model_;
  QPointer<QQuickWidget> view_;
};

}  // namespace cppwiki::gui::merge

#endif  // CPPWIKI_SRC_GUI_MERGE_CONFLICT_MERGE_DIALOG_H_
