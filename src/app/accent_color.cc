#include "app/accent_color.h"

namespace cppwiki {

auto ToAccentColorKey(AccentColor accent_color) -> QString {
  switch (accent_color) {
    case AccentColor::kBlue:
      return QStringLiteral("blue");
    case AccentColor::kViolet:
      return QStringLiteral("violet");
    case AccentColor::kOrange:
      return QStringLiteral("orange");
    case AccentColor::kGreen:
      return QStringLiteral("green");
  }
  return QStringLiteral("blue");
}

auto AccentColorFromKey(const QString& key) -> AccentColor {
  if (key == QStringLiteral("violet")) {
    return AccentColor::kViolet;
  }
  if (key == QStringLiteral("orange")) {
    return AccentColor::kOrange;
  }
  if (key == QStringLiteral("green")) {
    return AccentColor::kGreen;
  }
  return AccentColor::kBlue;
}

auto AccentColorBaseColor(AccentColor accent_color) -> QColor {
  switch (accent_color) {
    case AccentColor::kBlue:
      // Matches QlementineStyle's hardcoded primaryColor (dark.json), see ADR-016.
      return QColor(0x50, 0x86, 0xff);
    case AccentColor::kViolet:
      return QColor(0x9b, 0x6b, 0xff);
    case AccentColor::kOrange:
      return QColor(0xff, 0x9f, 0x40);
    case AccentColor::kGreen:
      return QColor(0x3d, 0xdc, 0x84);
  }
  return QColor(0x50, 0x86, 0xff);
}

}  // namespace cppwiki
