#include "gui/attachment_url_scheme_handler.h"

#include <QBuffer>
#include <QUrl>
#include <QWebEngineUrlRequestJob>
#include <algorithm>
#include <array>
#include <string_view>

#include "storage/attachment.h"

namespace cppwiki::gui {
namespace {

auto IsSafeInlineImageMime(std::string_view mime_type) -> bool {
  constexpr std::array<std::string_view, 4> kSafeInlineImageMimes{"image/png", "image/jpeg",
                                                                  "image/gif", "image/webp"};
  return std::ranges::find(kSafeInlineImageMimes, mime_type) != kSafeInlineImageMimes.end();
}

}  // namespace

AttachmentUrlSchemeHandler::AttachmentUrlSchemeHandler(
    std::shared_ptr<storage::LocalDocumentRepository> repository,
    std::function<QString()> workspace_id_provider, QObject* parent)
    : QWebEngineUrlSchemeHandler(parent),
      repository_(std::move(repository)),
      workspace_id_provider_(std::move(workspace_id_provider)) {}

void AttachmentUrlSchemeHandler::requestStarted(QWebEngineUrlRequestJob* job) {
  if (job == nullptr || repository_ == nullptr || !workspace_id_provider_) {
    if (job != nullptr) {
      job->fail(QWebEngineUrlRequestJob::RequestFailed);
    }
    return;
  }

  const auto attachment_id =
      storage::ParseAttachmentUri(job->requestUrl().toString().toStdString());
  if (!attachment_id) {
    job->fail(QWebEngineUrlRequestJob::UrlInvalid);
    return;
  }
  const auto attachment =
      repository_->LoadAttachment(*attachment_id, workspace_id_provider_().toStdString());
  if (attachment.error || !attachment.attachment ||
      !IsSafeInlineImageMime(attachment.attachment->metadata.mime_type)) {
    job->fail(QWebEngineUrlRequestJob::RequestDenied);
    return;
  }

  auto* buffer = new QBuffer(job);
  buffer->setData(reinterpret_cast<const char*>(attachment.attachment->bytes.data()),
                  static_cast<int>(attachment.attachment->bytes.size()));
  buffer->open(QIODevice::ReadOnly);
  job->reply(QByteArray::fromStdString(attachment.attachment->metadata.mime_type), buffer);
}

}  // namespace cppwiki::gui
