#include "holonight/gtk3_palette.h"
#include "holonight/gtk4_palette.h"

#include <gtest/gtest.h>

#include <array>
#include <iomanip>
#include <sstream>
#include <string>

namespace {

Holonight::Adapters::Snapshot snapshot() {
  Holonight::Adapters::Snapshot result;
  result.contract_version = Holonight::Adapters::kContractVersion;
  for (std::size_t index = 0; index < result.colors.size(); ++index) {
    result.colors[index] = {static_cast<std::uint8_t>(index + 1), static_cast<std::uint8_t>(index + 31),
                            static_cast<std::uint8_t>(index + 61), static_cast<std::uint8_t>(index + 91)};
  }
  return result;
}

void expectDefinitions(const std::string &css, const Holonight::Adapters::Snapshot &value) {
  for (std::size_t index = 0; index < Holonight::Adapters::kColorRoleNames.size(); ++index) {
    const auto color = value.colors[index];
    std::ostringstream encoded;
    encoded << "rgba(" << static_cast<unsigned>(color.red) << ", " << static_cast<unsigned>(color.green) << ", "
            << static_cast<unsigned>(color.blue) << ", " << std::fixed << std::setprecision(6)
            << (static_cast<double>(color.alpha) / 255.0) << ')';
    const std::string expected = "@define-color holonight_" + std::string(Holonight::Adapters::kColorRoleNames[index]) +
                                 " " + encoded.str() + ";";
    EXPECT_NE(css.find(expected), std::string::npos) << expected;
    EXPECT_EQ(css.find(expected), css.rfind(expected)) << expected;
  }
}

void expectPaletteOnly(const std::string &css) {
  constexpr std::array forbidden{"padding",      "margin",    "min-width",   "min-height", "font",
                                 "-gtk-icon-",   "animation", "transition",  "@import",    "url(",
                                 "settings.ini", "gsettings", "environment", "GTK_THEME",  "--"};
  for (const std::string_view token : forbidden) {
    EXPECT_EQ(css.find(token), std::string::npos) << token;
  }
}

} // namespace

TEST(GtkPalette, IsDeterministicAndPreservesEveryRoleAndAlpha) {
  const auto value = snapshot();
  const auto gtk3 = Holonight::Adapters::generateGtk3PaletteCss(value);
  const auto gtk4 = Holonight::Adapters::generateGtk4PaletteCss(value);
  EXPECT_EQ(gtk3, Holonight::Adapters::generateGtk3PaletteCss(value));
  EXPECT_EQ(gtk4, Holonight::Adapters::generateGtk4PaletteCss(value));
  expectDefinitions(gtk3, value);
  expectDefinitions(gtk4, value);
}

TEST(GtkPalette, KeepsMajorSpecificTemplatesDistinctAndPaletteOnly) {
  const auto value = snapshot();
  const auto gtk3 = Holonight::Adapters::generateGtk3PaletteCss(value);
  const auto gtk4 = Holonight::Adapters::generateGtk4PaletteCss(value);
  EXPECT_NE(gtk3, gtk4);
  EXPECT_NE(gtk3.find("treeview.view"), std::string::npos);
  EXPECT_EQ(gtk3.find("listview"), std::string::npos);
  EXPECT_NE(gtk4.find("listview"), std::string::npos);
  EXPECT_EQ(gtk4.find("treeview.view"), std::string::npos);
  expectPaletteOnly(gtk3);
  expectPaletteOnly(gtk4);
}

TEST(GtkPalette, MapsTierTwoStatesToTheirExactSemanticRoles) {
  const auto value = snapshot();
  for (const auto &css :
       {Holonight::Adapters::generateGtk3PaletteCss(value), Holonight::Adapters::generateGtk4PaletteCss(value)}) {
    EXPECT_NE(css.find("background-color: @holonight_window_surface"), std::string::npos);
    EXPECT_NE(css.find("background-color: @holonight_strong_selection"), std::string::npos);
    EXPECT_NE(css.find("background-color: @holonight_subtle_selection"), std::string::npos);
    EXPECT_NE(css.find("background-color: @holonight_hover_surface"), std::string::npos);
    EXPECT_NE(css.find("color: @holonight_disabled_text"), std::string::npos);
    EXPECT_NE(css.find("border-color: @holonight_focus_border"), std::string::npos);
    EXPECT_NE(css.find("border-color: @holonight_destructive_border"), std::string::npos);
  }
}
