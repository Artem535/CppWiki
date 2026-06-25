#ifndef CPPWIKI_SRC_GUI_MAIN_WINDOW_H_
#define CPPWIKI_SRC_GUI_MAIN_WINDOW_H_

#include <QMainWindow>

class QLabel;

namespace cppwiki {

struct AppContext;
class IPage;

class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  MainWindow(const MainWindow&) = delete;
  MainWindow& operator=(const MainWindow&) = delete;
  ~MainWindow() override;

  // Sets the application context and creates the initial page.
  void SetContext(AppContext* context);

 signals:
  void settingsChanged();

 private:
  void BuildUi();
  void CreateInitialPage();
  void ShowSettingsDialog();
  void UpdateBackendStatus();

  AppContext* context_ = nullptr;
  IPage* current_page_ = nullptr;
  QLabel* backend_status_label_ = nullptr;
};

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_GUI_MAIN_WINDOW_H_
