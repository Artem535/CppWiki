#ifndef CPPWIKI_SRC_GUI_ATTACHMENT_URL_SCHEME_HANDLER_H_
#define CPPWIKI_SRC_GUI_ATTACHMENT_URL_SCHEME_HANDLER_H_

#include <QString>
#include <QWebEngineUrlSchemeHandler>
#include <functional>
#include <memory>
#include <utility>

#include "storage/local_document_repository.h"

class QWebEngineUrlRequestJob;

namespace cppwiki::gui {

// Serves only safe raster images addressed by cppwiki-attachment://<id>. Generic attachments
// deliberately stay behind QEditorBridge::saveAttachmentToFile(), so document content cannot
// cause Chromium to open or download an arbitrary stored file.
class AttachmentUrlSchemeHandler final : public QWebEngineUrlSchemeHandler {
 public:
  AttachmentUrlSchemeHandler(std::shared_ptr<storage::LocalDocumentRepository> repository,
                             std::function<QString()> workspace_id_provider,
                             QObject* parent = nullptr);

  void requestStarted(QWebEngineUrlRequestJob* job) override;

 private:
  std::shared_ptr<storage::LocalDocumentRepository> repository_;
  std::function<QString()> workspace_id_provider_;
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_ATTACHMENT_URL_SCHEME_HANDLER_H_
