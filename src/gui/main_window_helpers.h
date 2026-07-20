#ifndef CPPWIKI_SRC_GUI_MAIN_WINDOW_HELPERS_H_
#define CPPWIKI_SRC_GUI_MAIN_WINDOW_HELPERS_H_

#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include <QString>
#include <QStringList>
#include <QStringView>

#include "backend/backend_client.h"
#include "document/document.h"
#include "storage/local_document_repository.h"
#include "sync/sync_service.h"

class QWidget;
class QLabel;

namespace cppwiki::auth {
class AuthSessionManager;
}

namespace oclero::qlementine {
class StatusBadgeWidget;
}

namespace cppwiki::gui::main_window_helpers {

auto StateTextColor(bool is_error, bool is_warning, bool is_success) -> QString;

auto BoolLabel(bool value, QStringView true_text = u"Yes", QStringView false_text = u"No")
    -> QString;

auto SyncLifecycleStateLabel(storage::SyncLifecycleState state) -> QString;

auto JoinOrFallback(const QStringList& values,
                    const QString& fallback = QStringLiteral("None")) -> QString;

auto BuildSyncGuidance(const sync::DocumentSyncSnapshot& snapshot) -> QString;

auto HydrationStateLabel(sync::WorkspaceHydrationState state) -> QString;

auto BuildWorkspaceHydrationSummary(const sync::DocumentSyncSnapshot& snapshot) -> QString;

auto PendingConflicts(const std::shared_ptr<storage::LocalDocumentRepository>& repository)
    -> std::vector<storage::DocumentConflictRecord>;

auto FirstPendingConflict(const std::shared_ptr<storage::LocalDocumentRepository>& repository)
    -> std::optional<storage::DocumentConflictRecord>;

void ApplyStatusTooltip(QWidget* widget, QLabel* label,
                        oclero::qlementine::StatusBadgeWidget* badge, const QString& tooltip);

auto CompactBackendStatusText(backend::BackendConnectionState state) -> QString;

auto CompactDocumentStatusText(const QString& message, bool is_error) -> QString;

auto CompactSyncStatusText(const sync::DocumentSyncSnapshot& snapshot) -> QString;

auto IsHighPrioritySaveHint(const QString& text) -> bool;

auto CompactAuthHint(const auth::AuthSessionManager* auth) -> QString;

auto MakeStatusWidget(const QString& initial_text, QWidget* parent)
    -> std::tuple<QWidget*, oclero::qlementine::StatusBadgeWidget*, QLabel*>;

// Label text for the native Import/Export controls (issue #96), naming the concrete file kind
// ("Import .ipynb" / "Import .excalidraw") rather than a generic "Import" — the kWikiPage
// fallback is never actually shown since MainWindow hides both buttons for that kind.
auto ImportButtonLabel(document::DocumentKind kind) -> QString;
auto ExportButtonLabel(document::DocumentKind kind) -> QString;

}  // namespace cppwiki::gui::main_window_helpers

#endif  // CPPWIKI_SRC_GUI_MAIN_WINDOW_HELPERS_H_