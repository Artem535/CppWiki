#ifndef CPPWIKI_SRC_GUI_MAIN_WINDOW_H_
#define CPPWIKI_SRC_GUI_MAIN_WINDOW_H_

#include <QMainWindow>

namespace cppwiki {

struct AppContext;
class IPage;

class MainWindow final : public QMainWindow {
 public:
  explicit MainWindow(QWidget* parent = nullptr);
  MainWindow(const MainWindow&) = delete;
  MainWindow& operator=(const MainWindow&) = delete;
  ~MainWindow() override;

  // Sets the application context and creates the initial page.
  void SetContext(AppContext* context);

 private:
  void BuildUi();
  void CreateInitialPage();

  AppContext* context_ = nullptr;
  IPage* current_page_ = nullptr;
};

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_GUI_MAIN_WINDOW_H_
