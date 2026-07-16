#include "auth/ai_api_key_store.h"

#include <qtkeychain/keychain.h>

#include <QString>

namespace cppwiki::auth {

AiApiKeyStore::AiApiKeyStore(QString service_name, QObject* parent)
    : QObject(parent), service_name_(std::move(service_name)) {}

void AiApiKeyStore::Load() {
  auto* job = new QKeychain::ReadPasswordJob(service_name_, this);
  job->setKey(StorageKey());
  connect(job, &QKeychain::Job::finished, this, [this](QKeychain::Job* base_job) {
    auto* job = qobject_cast<QKeychain::ReadPasswordJob*>(base_job);
    if (job == nullptr) {
      emit storageError(QStringLiteral("load"), QStringLiteral("Read job finished unexpectedly."));
      return;
    }

    if (job->error() == QKeychain::EntryNotFound) {
      emit apiKeyMissing();
      return;
    }

    if (job->error() != QKeychain::NoError) {
      emit storageError(QStringLiteral("load"), job->errorString());
      return;
    }

    emit apiKeyLoaded(job->textData());
  });
  job->start();
}

void AiApiKeyStore::Save(const QString& api_key) {
  auto* job = new QKeychain::WritePasswordJob(service_name_, this);
  job->setKey(StorageKey());
  job->setTextData(api_key);
  connect(job, &QKeychain::Job::finished, this, [this](QKeychain::Job* base_job) {
    if (base_job->error() != QKeychain::NoError) {
      emit storageError(QStringLiteral("save"), base_job->errorString());
    }
  });
  job->start();
}

void AiApiKeyStore::Clear() {
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

auto AiApiKeyStore::StorageKey() const -> const QString& {
  static const QString kStorageKey = QStringLiteral("ai_provider_api_key");
  return kStorageKey;
}

}  // namespace cppwiki::auth
