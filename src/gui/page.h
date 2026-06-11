#ifndef CPPWIKI_SRC_GUI_PAGE_H_
#define CPPWIKI_SRC_GUI_PAGE_H_

#include <QWidget>

#include "gui/i_page.h"

class QWebEngineView;

namespace cppwiki {

class Page final : public QWidget, public IPage {
 public:
  explicit Page(QWidget* parent = nullptr);
  Page(const Page&) = delete;
  Page& operator=(const Page&) = delete;
  ~Page() override;

  QString Title() const override;
  QWidget* Widget() override;

 private:
  void BuildUi();
  void LoadEditor();

  QWebEngineView* editor_view_ = nullptr;
};

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_GUI_PAGE_H_
