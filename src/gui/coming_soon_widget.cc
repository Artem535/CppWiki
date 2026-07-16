#include "gui/coming_soon_widget.h"

#include <QVBoxLayout>
#include <QWidget>
#include <oclero/qlementine/widgets/Label.hpp>

namespace cppwiki::gui {

ComingSoonWidget::ComingSoonWidget(const QString& title, const QString& subtitle, QWidget* parent)
    : QWidget(parent) {
  setObjectName(QStringLiteral("comingSoonWidget"));

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 24, 24, 24);
  layout->setSpacing(8);
  layout->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

  auto* title_label = new oclero::qlementine::Label(title, oclero::qlementine::TextRole::H2, this);
  title_label->setAlignment(Qt::AlignHCenter);
  layout->addWidget(title_label);

  auto* subtitle_label =
      new oclero::qlementine::Label(subtitle, oclero::qlementine::TextRole::Caption, this);
  subtitle_label->setAlignment(Qt::AlignHCenter);
  subtitle_label->setWordWrap(true);
  layout->addWidget(subtitle_label);
}

}  // namespace cppwiki::gui
