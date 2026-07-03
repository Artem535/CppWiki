#ifndef CPPWIKI_SRC_AUTH_AUTH_TOKEN_STORE_H_
#define CPPWIKI_SRC_AUTH_AUTH_TOKEN_STORE_H_

#include <QObject>
#include <QString>

#include "auth/auth_token_bundle.h"

namespace cppwiki::auth {

class AuthTokenStore final : public QObject {
  Q_OBJECT

 public:
  explicit AuthTokenStore(QString service_name, QObject* parent = nullptr);

  void Load();
  void Save(const AuthTokenBundle& tokens);
  void Clear();

 signals:
  void tokensLoaded(const QString& access_token, const QString& refresh_token,
                    const QString& id_token);
  void tokensMissing();
  void storageError(const QString& operation, const QString& message);

 private:
  [[nodiscard]] auto StorageKey() const -> const QString&;

  QString service_name_;
};

}  // namespace cppwiki::auth

#endif  // CPPWIKI_SRC_AUTH_AUTH_TOKEN_STORE_H_
