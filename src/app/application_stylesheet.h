#ifndef CPPWIKI_SRC_APP_APPLICATION_STYLESHEET_H_
#define CPPWIKI_SRC_APP_APPLICATION_STYLESHEET_H_

#include <QString>

#include "app/accent_color.h"

class QWidget;

namespace cppwiki {

[[nodiscard]] auto ResolveApplicationStylesheetPath() -> QString;

// Builds the ADR-016 accent-tint QSS block appended after the static app/cppwiki.qss content:
// the workspace rail's active-mode highlight and the editor topbar
// (collaborationPanel[collaborationState="viewing"])'s background/border, both as a
// near-transparent tint of `accent_color` (matching the existing rgba(..., 0.06-0.24) alpha
// range already used elsewhere in cppwiki.qss). Exposed separately from
// ApplyApplicationStylesheet() so it can be unit-tested without a QWidget/QApplication.
[[nodiscard]] auto BuildAccentColorStylesheet(AccentColor accent_color) -> QString;

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
void ApplyApplicationStylesheet(QWidget* target,
                                AccentColor accent_color = AccentColor::kBlue);

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_APP_APPLICATION_STYLESHEET_H_
