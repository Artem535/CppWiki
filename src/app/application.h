#ifndef CPPWIKI_SRC_APP_APPLICATION_H_
#define CPPWIKI_SRC_APP_APPLICATION_H_

#include <QApplication>

#include "gui/main_window.h"

namespace cppwiki {

class Application final {
 public:
  Application(int& argc, char** argv);
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  ~Application();

  int Run();

 private:
  QApplication qt_application_;
  MainWindow main_window_;
};

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_APP_APPLICATION_H_
