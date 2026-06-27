#ifndef CPPWIKI_SRC_GUI_MAIN_WINDOW_H_
#define CPPWIKI_SRC_GUI_MAIN_WINDOW_H_

#include <QMainWindow>

class QGridLayout;
class QLabel;
class QToolButton;
class QWidget;

namespace oclero::qlementine {
class StatusBadgeWidget;
}

namespace cppwiki::gui {
class PresenceStripWidget;
}

namespace cppwiki {

struct AppContext;
class Page;

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
  void UpdateDocumentStatus(const QString& message, bool is_error);
  void UpdateCollaborationStatus(const QString& summary, const QString& details, bool is_warning);

  AppContext* context_ = nullptr;
  Page* current_page_ = nullptr;
  QWidget* shell_widget_ = nullptr;
  QGridLayout* shell_layout_ = nullptr;
  QWidget* current_sidebar_widget_ = nullptr;
  QWidget* current_content_widget_ = nullptr;
  gui::PresenceStripWidget* presence_strip_widget_ = nullptr;
  QString fallback_editor_user_id_;
  bool fallback_editor_is_self_ = false;
  QToolButton* backend_refresh_button_ = nullptr;
  QWidget* document_status_widget_ = nullptr;
  oclero::qlementine::StatusBadgeWidget* document_status_badge_ = nullptr;
  QLabel* document_status_label_ = nullptr;
  QWidget* backend_status_widget_ = nullptr;
  oclero::qlementine::StatusBadgeWidget* backend_status_badge_ = nullptr;
  QLabel* backend_status_label_ = nullptr;
  QWidget* collaboration_status_widget_ = nullptr;
  oclero::qlementine::StatusBadgeWidget* collaboration_status_badge_ = nullptr;
  QLabel* collaboration_status_label_ = nullptr;
};

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_GUI_MAIN_WINDOW_H_
