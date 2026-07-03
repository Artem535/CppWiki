#ifndef CPPWIKI_SRC_SYNC_SYNC_STATE_PROVIDER_H_
#define CPPWIKI_SRC_SYNC_SYNC_STATE_PROVIDER_H_

#include <QString>

namespace cppwiki::sync {

// Minimal read-only interface for components that need to know whether sync is
// enabled without depending on the full SyncService.
class SyncStateProvider {
 public:
  virtual ~SyncStateProvider() = default;

  [[nodiscard]] virtual auto ShouldExpectRemoteDocuments(const QString& workspace_id) const
      -> bool = 0;
  [[nodiscard]] virtual auto ShouldCreateSyntheticWelcomePage(const QString& workspace_id) const
      -> bool = 0;
};

}  // namespace cppwiki::sync

#endif  // CPPWIKI_SRC_SYNC_SYNC_STATE_PROVIDER_H_
