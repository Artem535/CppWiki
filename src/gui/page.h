#ifndef CPPWIKI_SRC_GUI_PAGE_H_
#define CPPWIKI_SRC_GUI_PAGE_H_

#include <QWidget>

#include "app/program_settings.h"
#include "gui/i_page.h"

class QWebChannel;
class QWebEngineView;

namespace cppwiki::bridge {
class QEditorBridge;
}

namespace cppwiki {

class Page final : public QWidget, public IPage {
 public:
  explicit Page(ProgramSettings settings, QWidget* parent = nullptr);

  Page(const Page&) = delete;
  auto operator=(const Page&) -> Page& = delete;

  ~Page() override;

  [[nodiscard]] auto Title() const -> QString override;
  auto Widget() -> QWidget* override;

 private:
  void BuildUi();
  void LoadEditor();
  void InstallWebChannelScript();

  QWebEngineView* editor_view_ = nullptr;
  QWebChannel* channel_ = nullptr;
  bridge::QEditorBridge* editor_bridge_ = nullptr;
  ProgramSettings settings_;
};

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_GUI_PAGE_H_
