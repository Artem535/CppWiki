#ifndef CPPWIKI_EDITOR_BRIDGE_H
#define CPPWIKI_EDITOR_BRIDGE_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <memory>
#include <optional>

#include "sync/sync_state_provider.h"

class QNetworkAccessManager;
class QNetworkReply;
class QUrl;
class QByteArray;

namespace cppwiki::storage {
class LocalDocumentRepository;
}

namespace cppwiki::auth {
class AiApiKeyStore;
}

namespace cppwiki::bridge {

class QEditorBridge final : public QObject {
  Q_OBJECT

 public:
  explicit QEditorBridge(QObject* parent = nullptr);

  // Set the document repository for persistence operations.
  void SetRepository(std::shared_ptr<storage::LocalDocumentRepository> repository);
  void SetSyncStateProvider(const sync::SyncStateProvider* provider);
  void SetPendingDocumentAccess(bool editable, QString lock_owner = {},
                                QString access_message = {});
  void SetCurrentDocumentAccess(bool editable, QString lock_owner = {},
                                QString access_message = {});
  // Marks whether the currently open document has an unresolved sync conflict.
  // Independent of the lock-based access gate above (see ADR-013): a document
  // can be simultaneously unlocked and conflicted, and mutations must be
  // rejected for either reason.
  void SetCurrentDocumentConflicted(bool has_conflict);
  void SetCurrentAuthorId(QString author_id);
  void SetCurrentWorkspaceId(QString workspace_id);

  // AI transport wiring (ADR-012 + addendum). Selects the server-mediated
  // backend when `backend_enabled` and `backend_base_url` are set; otherwise
  // startAiRequest() falls back to the local-key path via `key_store`
  // (never both, never a direct-fetch path from JS).
  void SetAiTransportConfig(bool backend_enabled, QString backend_base_url,
                            QString backend_access_token);
  void SetAiApiKeyStore(auth::AiApiKeyStore* key_store);
  // Mirrors ProgramSettings::AiFeaturesEnabled()/AiAutocompleteEnabled()/
  // AiInlineSuggestionsEnabled(); the JS side reads these from getBridgeInfo()
  // to decide whether to render the AI toolbar button / slash-menu items /
  // ghost-text extension at all. `inline_suggestions_enabled` is a separate
  // opt-in (issue #59), independent of `features_enabled`.
  void SetAiFeatureFlags(bool features_enabled, bool autocomplete_enabled,
                        bool inline_suggestions_enabled);
  void RequestOpenDocument(const QString& page_id);
  void ClearCurrentDocumentSelection();
  [[nodiscard]] QVariantMap listDocumentsInWorkspace(const QString& workspace_id);
  // `kind` is the DocumentKind key (see document::ToDocumentKindKey/DocumentKindFromKey),
  // e.g. "wikiPage" (default), "jupyterNotebook", "excalidrawCanvas". Unrecognized/empty
  // values fall back to "wikiPage".
  [[nodiscard]] QVariantMap createDocumentInWorkspace(const QString& workspace_id,
                                                      const QString& kind = QStringLiteral("wikiPage"));
  [[nodiscard]] QVariantMap createChildDocumentInWorkspace(
      const QString& workspace_id, const QString& parent_id,
      const QString& kind = QStringLiteral("wikiPage"));

  Q_INVOKABLE QVariantMap getBridgeInfo();
  Q_INVOKABLE QVariantMap getInitialDocument();
  Q_INVOKABLE QVariantMap listDocuments();
  Q_INVOKABLE QVariantMap createDocument();
  Q_INVOKABLE QVariantMap createChildDocument(const QString& parent_id,
                                              const QString& kind = QStringLiteral("wikiPage"));
  Q_INVOKABLE QVariantMap renameDocument(const QString& page_id, const QString& title);
  QVariantMap updateDocumentPlacement(const QString& page_id, const QString& parent_id,
                                      bool has_parent_id, int sort_order);
  QVariantMap deleteDocument(const QString& page_id);
  Q_INVOKABLE QVariantMap loadDocument(const QString& page_id);
  Q_INVOKABLE QVariantMap openDocument(const QString& page_id);
  Q_INVOKABLE QVariantMap updateSnapshot(const QString& snapshot_json);

  // Starts an AI request (rewrite or autocomplete, per ADR-010's MVP scope)
  // and returns a request id immediately; the actual provider call happens
  // asynchronously and its result streams back via the ai* signals below.
  // `mode` is "rewrite" or "autocomplete". This never performs a fetch from
  // JS: the request always originates from this C++ method (ADR-012).
  //
  // `tool_name`/`tool_schema_json` are optional (empty when not used). When
  // both are non-empty, xl-ai wants a structured tool-call response matching
  // the given JSON Schema (see issue #65): the provider is called in
  // tool-calling/JSON mode and the parsed arguments are relayed via
  // aiToolCallReceived() instead of aiChunkReceived()/plain text.
  Q_INVOKABLE QString startAiRequest(const QString& prompt, const QString& context_text,
                                     const QString& mode, const QString& tool_name = {},
                                     const QString& tool_schema_json = {});

 signals:
  // AI streaming relay (ADR-012): chunks of the AI provider's response,
  // relayed one at a time because a bridge signal cannot expose a native
  // ReadableStream to JS.
  void aiChunkReceived(const QString& request_id, const QString& chunk);
  // Emitted instead of aiChunkReceived when startAiRequest() was called with
  // a tool schema and the provider replied with a structured tool call.
  // `arguments_json` is the tool call's arguments, JSON-stringified.
  void aiToolCallReceived(const QString& request_id, const QString& tool_name,
                          const QString& arguments_json);
  void aiRequestCompleted(const QString& request_id);
  void aiRequestFailed(const QString& request_id, const QString& error);
  void documentOpenRequested(const QString& pageId);
  void documentLoaded(const QVariantMap& document);
  void documentLoadFailed(const QString& pageId, const QString& message);
  void documentSelectionCleared();
  void documentAccessChanged(bool editable, const QString& lock_owner,
                             const QString& access_message);

  // Emitted when document save status changes (for UI feedback).
  void saveStatusChanged(const QString& pageId, bool success, const QString& message);

 private:
  // Returns a document_read_only error envelope if `page_id` refers to the currently
  // open document and that document is locked/read-only; otherwise returns std::nullopt.
  // Mirrors the gate applied in updateSnapshot() so rename/move/delete cannot bypass
  // the lock model that mutations through the editor are subject to.
  [[nodiscard]] std::optional<QVariantMap> RejectIfCurrentDocumentLocked(
      const QString& page_id) const;

  // Returns a document_read_only error envelope if `page_id` refers to the currently
  // open document and that document has an unresolved sync conflict; otherwise
  // returns std::nullopt. Independent of RejectIfCurrentDocumentLocked() (see ADR-013).
  [[nodiscard]] std::optional<QVariantMap> RejectIfCurrentDocumentConflicted(
      const QString& page_id) const;

  void StartServerMediatedAiRequest(const QString& request_id, const QString& prompt,
                                    const QString& context_text, const QString& mode,
                                    const QString& tool_name, const QString& tool_schema_json);
  void StartLocalKeyAiRequest(const QString& request_id, const QString& prompt,
                              const QString& context_text, const QString& mode,
                              const QString& tool_name, const QString& tool_schema_json);
  void CallProviderAndRelay(const QString& request_id, const QUrl& url,
                           const QByteArray& body, const QString& auth_header_value,
                           const QString& tool_name);
  void EmitChunkedCompletion(const QString& request_id, const QString& full_text);

  bool pending_document_editable_ = true;
  bool current_document_editable_ = true;
  bool current_document_has_conflict_ = false;
  QString pending_lock_owner_;
  QString current_lock_owner_;
  QString pending_access_message_;
  QString current_access_message_;
  std::shared_ptr<storage::LocalDocumentRepository> repository_;
  const sync::SyncStateProvider* sync_state_provider_ = nullptr;
  QString current_page_id_;
  QString current_author_id_;
  QString current_workspace_id_{QStringLiteral("default")};

  bool ai_backend_enabled_ = false;
  QString ai_backend_base_url_;
  QString ai_backend_access_token_;
  auth::AiApiKeyStore* ai_api_key_store_ = nullptr;
  QNetworkAccessManager* network_manager_ = nullptr;
  bool ai_features_enabled_ = false;
  bool ai_autocomplete_enabled_ = false;
  bool ai_inline_suggestions_enabled_ = false;
};

}  // namespace cppwiki::bridge

#endif  // CPPWIKI_EDITOR_BRIDGE_H
