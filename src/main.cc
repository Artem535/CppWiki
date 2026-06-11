#include "app/application.h"

int main(int argc, char* argv[]) {
  cppwiki::Application application(argc, argv);
  return application.Run();
}
