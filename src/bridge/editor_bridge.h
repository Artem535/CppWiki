#ifndef CPPWIKI_EDITOR_BRIDGE_H
#define CPPWIKI_EDITOR_BRIDGE_H

#include <QObject>
#include <QString>
#include <QVariant>

#include <memory>

namespace cppwiki::storage {
class LocalDocumentRepository;
}

namespace cppwiki::bridge {

class QEditorBridge final : public QObject {
  Q_OBJECT

 public:
  explicit QEditorBridge(QObject* parent = nullptr);

  // Set the document repository for persistence operations.
  void SetRepository(std::shared_ptr<storage::LocalDocumentRepository> repository);

  Q_INVOKABLE QVariantMap getBridgeInfo();
  Q_INVOKABLE QVariantMap getInitialDocument();
  Q_INVOKABLE QVariantMap updateSnapshot(const QString& snapshot_json);

 signals:
  // Emitted when document save status changes (for UI feedback).
  void saveStatusChanged(const QString& pageId, bool success, const QString& message);

 private:
  std::shared_ptr<storage::LocalDocumentRepository> repository_;
  QString current_page_id_;
};

}  // namespace cppwiki::bridge

#endif  // CPPWIKI_EDITOR_BRIDGE_H
