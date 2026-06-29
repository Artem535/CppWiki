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
  void SetPendingDocumentAccess(bool editable, QString lock_owner = {},
                                QString access_message = {});
  void SetCurrentDocumentAccess(bool editable, QString lock_owner = {},
                                QString access_message = {});
  void SetCurrentAuthorId(QString author_id);
  void SetCurrentWorkspaceId(QString workspace_id);
  void RequestOpenDocument(const QString& page_id);
  void ClearCurrentDocumentSelection();

  Q_INVOKABLE QVariantMap getBridgeInfo();
  Q_INVOKABLE QVariantMap getInitialDocument();
  Q_INVOKABLE QVariantMap listDocuments();
  Q_INVOKABLE QVariantMap createDocument();
  Q_INVOKABLE QVariantMap createChildDocument(const QString& parent_id);
  Q_INVOKABLE QVariantMap renameDocument(const QString& page_id, const QString& title);
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
  void documentAccessChanged(bool editable, const QString& lock_owner, const QString& access_message);

  // Emitted when document save status changes (for UI feedback).
  void saveStatusChanged(const QString& pageId, bool success, const QString& message);

 private:
  bool pending_document_editable_ = true;
  bool current_document_editable_ = true;
  QString pending_lock_owner_;
  QString current_lock_owner_;
  QString pending_access_message_;
  QString current_access_message_;
  std::shared_ptr<storage::LocalDocumentRepository> repository_;
  QString current_page_id_;
  QString current_author_id_;
  QString current_workspace_id_{QStringLiteral("default")};
};

}  // namespace cppwiki::bridge

#endif  // CPPWIKI_EDITOR_BRIDGE_H
