#ifndef CPPWIKI_SRC_AUTH_AI_API_KEY_STORE_H_
#define CPPWIKI_SRC_AUTH_AI_API_KEY_STORE_H_

#include <QObject>
#include <QString>

namespace cppwiki::auth {

// Stores the user-supplied AI provider API key used by the local-key fallback
// transport (ADR-012 addendum): when no `cppwiki_server` backend is
// configured, EditorBridge calls the AI provider directly from C++ using a
// key read from the OS keychain here — never from JS, and never persisted in
// plaintext QSettings. Mirrors AuthTokenStore's qt-keychain-backed pattern.
class AiApiKeyStore final : public QObject {
  Q_OBJECT

 public:
  explicit AiApiKeyStore(QString service_name, QObject* parent = nullptr);

  void Load();
  void Save(const QString& api_key);
  void Clear();

 signals:
  void apiKeyLoaded(const QString& api_key);
  void apiKeyMissing();
  void storageError(const QString& operation, const QString& message);

 private:
  [[nodiscard]] auto StorageKey() const -> const QString&;

  QString service_name_;
};

}  // namespace cppwiki::auth

#endif  // CPPWIKI_SRC_AUTH_AI_API_KEY_STORE_H_
