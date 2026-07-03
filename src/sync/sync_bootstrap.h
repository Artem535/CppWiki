#ifndef CPPWIKI_SRC_SYNC_SYNC_BOOTSTRAP_H_
#define CPPWIKI_SRC_SYNC_SYNC_BOOTSTRAP_H_

#include <QString>
#include <QStringList>

namespace cppwiki::sync {

struct SyncBootstrap final {
  bool available = false;
  bool enabled = false;
  QString gateway_url;
  QString database_name;
  QString auth_mode;
  bool token_passthrough = false;
  QString principal_subject;
  QString principal_username;
  QString principal_email;
  QStringList principal_roles;
  QStringList principal_groups;
  QStringList channels;
  QString status_text;
};

}  // namespace cppwiki::sync

#endif  // CPPWIKI_SRC_SYNC_SYNC_BOOTSTRAP_H_
