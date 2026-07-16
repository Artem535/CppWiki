#ifndef CPPWIKI_SRC_APP_APPLICATION_STYLESHEET_H_
#define CPPWIKI_SRC_APP_APPLICATION_STYLESHEET_H_

#include <QString>

class QWidget;

namespace cppwiki {

[[nodiscard]] auto ResolveApplicationStylesheetPath() -> QString;

// Applies the app's custom QSS (see kApplicationQssPath) to `target` only, rather than
// application-wide via qApp->setStyleSheet(). All of this stylesheet's selectors are scoped by
// objectName to widgets that live inside MainWindow's subtree, so scoping the QSS to that widget
// is equivalent in effect. Doing it this way, instead of qApp->setStyleSheet(), keeps widgets
// outside that subtree (e.g. SettingsDialog and its oclero::qlementine::SegmentedControl) from
// having their QStyle wrapped in Qt's internal QStyleSheetStyle proxy: Qt wraps *every* widget's
// style once a non-empty stylesheet is applied anywhere it can reach, and code that downcasts
// widget->style() to QlementineStyle* (as AbstractItemListWidget does) silently fails against
// that proxy, falling back to plain QPalette colors/metrics (see GetQlementineStyle() in
// application.h for the related, but insufficient on its own, per-widget setStyle() workaround).
void ApplyApplicationStylesheet(QWidget* target);

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_APP_APPLICATION_STYLESHEET_H_
