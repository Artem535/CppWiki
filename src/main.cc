#include <QWebEngineUrlScheme>

#include "app/application.h"
#include "core/logging.h"

namespace {

void RegisterAttachmentUrlScheme() {
  QWebEngineUrlScheme scheme(QByteArrayLiteral("cppwiki-attachment"));
  scheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
  scheme.setFlags(QWebEngineUrlScheme::SecureScheme | QWebEngineUrlScheme::LocalScheme |
                  QWebEngineUrlScheme::LocalAccessAllowed);
  QWebEngineUrlScheme::registerScheme(scheme);
}

}  // namespace

int main(int argc, char* argv[]) {
  RegisterAttachmentUrlScheme();
  cppwiki::logging::ConfigureBaseLogging();
  cppwiki::Application application(argc, argv);
  return application.Run();
}
