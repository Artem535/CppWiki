#include "app/application.h"
#include "core/logging.h"

int main(int argc, char* argv[]) {
  cppwiki::logging::ConfigureBaseLogging();
  cppwiki::Application application(argc, argv);
  return application.Run();
}
