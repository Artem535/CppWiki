#include "auth/auth_token_store.h"

#include <qtkeychain/keychain.h>

#include <QString>

#include <rfl/json.hpp>

#include "core/constants.h"
#include "core/qt_string.h"

namespace cppwiki::auth {

AuthTokenStore::AuthTokenStore(QString service_name, QObject* parent)
    : QObject(parent), service_name_(std::move(service_name)) {}

void AuthTokenStore::Load() {
  auto* job = new QKeychain::ReadPasswordJob(service_name_, this);
  job->setKey(StorageKey());
  connect(job, &QKeychain::Job::finished, this, [this](QKeychain::Job* base_job) {
    auto* job = qobject_cast<QKeychain::ReadPasswordJob*>(base_job);
    if (job == nullptr) {
      emit storageError(QStringLiteral("load"), QStringLiteral("Read job finished unexpectedly."));
      return;
    }

    if (job->error() == QKeychain::EntryNotFound) {
      emit tokensMissing();
      return;
    }

    if (job->error() != QKeychain::NoError) {
      emit storageError(QStringLiteral("load"), job->errorString());
      return;
    }

    const auto parsed = rfl::json::read<AuthTokenBundle>(job->textData().toStdString());
    if (!parsed) {
      emit storageError(QStringLiteral("load"),
                        QStringLiteral("Stored auth session is malformed JSON."));
      return;
    }

    emit tokensLoaded(QString::fromStdString(parsed->access_token),
                      QString::fromStdString(parsed->refresh_token),
                      QString::fromStdString(parsed->id_token));
  });
  job->start();
}

void AuthTokenStore::Save(const AuthTokenBundle& tokens) {
  auto* job = new QKeychain::WritePasswordJob(service_name_, this);
  job->setKey(StorageKey());
  job->setTextData(QString::fromStdString(rfl::json::write(tokens)));
  connect(job, &QKeychain::Job::finished, this, [this](QKeychain::Job* base_job) {
    if (base_job->error() != QKeychain::NoError) {
      emit storageError(QStringLiteral("save"), base_job->errorString());
    }
  });
  job->start();
}

void AuthTokenStore::Clear() {
  auto* job = new QKeychain::DeletePasswordJob(service_name_, this);
  job->setKey(StorageKey());
  connect(job, &QKeychain::Job::finished, this, [this](QKeychain::Job* base_job) {
    if (base_job->error() == QKeychain::NoError ||
        base_job->error() == QKeychain::EntryNotFound) {
      return;
    }

    emit storageError(QStringLiteral("clear"), base_job->errorString());
  });
  job->start();
}

auto AuthTokenStore::StorageKey() const -> const QString& {
  static const QString kStorageKey =
      ToQString(constants::kAuthTokenStoreEntryKey);
  return kStorageKey;
}

}  // namespace cppwiki::auth
