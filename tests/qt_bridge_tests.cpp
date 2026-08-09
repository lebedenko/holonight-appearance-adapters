#include "holonight/appearance_contract.h"
#include "holonight/gtk3_palette.h"
#include "holonight/gtk4_palette.h"
#include "holonight/qt_bridge.h"

#include "holonight/appearance.h"
#include "holonight/theme_catalog.h"
#include "semanticappearance.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <gtest/gtest.h>

namespace {

Holonight::ResolvedAppearance defaults() {
  return {.scheme = QStringLiteral("holonight-dark"),
          .accent = QStringLiteral("blue"),
          .theme_scheme = Holonight::ThemeSchemeKind::HoloNightDark,
          .color_mode = Holonight::ColorMode::Dark,
          .ui_font = QStringLiteral("Noto Sans 🌙"),
          .ui_font_size = 12,
          .monospace_font = QStringLiteral("JetBrains Mono"),
          .monospace_font_size = 13,
          .icon_theme = QStringLiteral("HoloNight"),
          .fallback_icon_theme = QStringLiteral("Papirus"),
          .cursor_theme = QStringLiteral("HoloNight")};
}

} // namespace

TEST(QtBridge, SerializesDeterministicallyAndRoundTripsThroughJsonGlib) {
  const auto semantic = Holonight::resolveSemanticAppearance(defaults());
  const QByteArray first = Holonight::Adapters::serializeSemanticAppearance(semantic);
  EXPECT_EQ(first, Holonight::Adapters::serializeSemanticAppearance(semantic));
  const QJsonObject object = QJsonDocument::fromJson(first).object();
  EXPECT_EQ(object.size(), 36);
  for (const auto role : Holonight::Adapters::kColorRoleNames) {
    EXPECT_TRUE(object.contains(QString::fromUtf8(role.data(), static_cast<qsizetype>(role.size())))) << role;
  }
  const auto parsed = Holonight::Adapters::parseSemanticAppearance(first.toStdString());
  ASSERT_TRUE(parsed) << (parsed.diagnostics.empty() ? "" : parsed.diagnostics.front().message);
  EXPECT_EQ(parsed.value->ui_font_family, "Noto Sans 🌙");
  EXPECT_EQ(parsed.value->colors.size(), 25U);
}

TEST(QtBridge, RoundTripsEveryBuiltInSchemeAndAccent) {
  const std::array accents{QStringLiteral("default"), QStringLiteral("cyan"), QStringLiteral("blue"),
                           QStringLiteral("violet"), QStringLiteral("yellow")};
  for (const auto &variant : Holonight::themeVariants()) {
    for (const auto &accent : accents) {
      auto input = defaults();
      input.scheme = variant.id;
      input.theme_scheme = variant.scheme;
      input.color_mode = variant.mode;
      input.accent = accent;
      const auto json = Holonight::Adapters::serializeSemanticAppearance(Holonight::resolveSemanticAppearance(input));
      const auto parsed = Holonight::Adapters::parseSemanticAppearance(json.toStdString());
      ASSERT_TRUE(parsed) << variant.id.toStdString() << '/' << accent.toStdString();
      EXPECT_EQ(parsed.value->scheme_id, variant.id.toStdString());
      EXPECT_EQ(parsed.value->accent_id, accent.toStdString());
      EXPECT_FALSE(Holonight::Adapters::generateGtk3PaletteCss(*parsed.value).empty());
      EXPECT_FALSE(Holonight::Adapters::generateGtk4PaletteCss(*parsed.value).empty());
    }
  }
}
