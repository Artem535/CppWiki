#ifndef CPPWIKI_SRC_APP_ACCENT_COLOR_H_
#define CPPWIKI_SRC_APP_ACCENT_COLOR_H_

#include <QColor>
#include <QString>

namespace cppwiki {

// The four accent-color presets from ADR-016 ("Configurable Accent Color and Unified Navigation
// Chrome"). kBlue is the default and matches QlementineStyle's hardcoded primaryColor
// (third_party/qlementine/showcase/resources/themes/dark.json's "primaryColor": "#5086ff"), so a
// user who never touches the setting sees zero visual change.
enum class AccentColor {
  kBlue,
  kViolet,
  kOrange,
  kGreen,
};

// Persistence key for a given preset (stored under constants::kSettingsAccentColorKey).
[[nodiscard]] auto ToAccentColorKey(AccentColor accent_color) -> QString;

// Inverse of ToAccentColorKey(); unknown/empty keys fall back to AccentColor::kBlue.
[[nodiscard]] auto AccentColorFromKey(const QString& key) -> AccentColor;

// The preset's base swatch color (fully opaque), used both for the settings-dialog swatch
// widgets and as the source color for the near-transparent QSS tints applied to the workspace
// rail's active-mode highlight and the editor topbar's tinted background.
[[nodiscard]] auto AccentColorBaseColor(AccentColor accent_color) -> QColor;

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_APP_ACCENT_COLOR_H_
