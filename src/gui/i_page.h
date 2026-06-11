#ifndef CPPWIKI_SRC_GUI_I_PAGE_H_
#define CPPWIKI_SRC_GUI_I_PAGE_H_

#include <QString>

class QWidget;

namespace cppwiki {

class IPage {
 public:
  IPage() = default;
  IPage(const IPage&) = delete;
  IPage& operator=(const IPage&) = delete;
  virtual ~IPage() = default;

  virtual QString Title() const = 0;
  virtual QWidget* Widget() = 0;
};

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_GUI_I_PAGE_H_
