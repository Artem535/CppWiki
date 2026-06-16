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
  void RequestOpenDocument(const QString& page_id);
  void ClearCurrentDocumentSelection();

  Q_INVOKABLE QVariantMap getBridgeInfo();
  Q_INVOKABLE QVariantMap getInitialDocument();
  Q_INVOKABLE QVariantMap listDocuments();
  Q_INVOKABLE QVariantMap createDocument();
  Q_INVOKABLE QVariantMap createChildDocument(const QString& parent_id);
  QVariantMap updateDocumentPlacement(const QString& page_id, const QString& parent_id,
                                      bool has_parent_id, int sort_order);
  QVariantMap deleteDocument(const QString& page_id);
  Q_INVOKABLE QVariantMap loadDocument(const QString& page_id);
  Q_INVOKABLE QVariantMap openDocument(const QString& page_id);
  Q_INVOKABLE QVariantMap updateSnapshot(const QString& snapshot_json);

signals:
  void documentOpenRequested(const QString& pageId);
  void documentLoaded(const QVariantMap& document);
  void documentLoadFailed(const QString& pageId, const QString& message);
  void documentSelectionCleared();

  // Emitted when document save status changes (for UI feedback).
  void saveStatusChanged(const QString& pageId, bool success, const QString& message);

 private:
  std::shared_ptr<storage::LocalDocumentRepository> repository_;
  QString current_page_id_;
};

}  // namespace cppwiki::bridge

#endif  // CPPWIKI_EDITOR_BRIDGE_H
