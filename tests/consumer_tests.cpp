#include "holonight/appearance_contract.h"
#include "holonight/tier1_projection.h"
#include "test_data.h"

#include <gtest/gtest.h>

using Holonight::Adapters::parseSemanticAppearance;

TEST(Consumer, ParsesEveryRequiredFieldAndIgnoresExtensions) {
  std::string json = validJson();
  json.insert(json.size() - 1, R"(,"future":{"unicode":"Привіт 🌙"})");
  const auto result = parseSemanticAppearance(json);
  ASSERT_TRUE(result) << (result.diagnostics.empty() ? "" : result.diagnostics.front().message);
  EXPECT_EQ(result.value->colors.size(), 25U);
  EXPECT_EQ(result.value->ui_font_family, "Inter");
  EXPECT_EQ(result.value->colors.front(), (Holonight::Adapters::Color{0x12, 0x34, 0x56, 0xff}));
}

TEST(Consumer, RejectsMalformedJson) {
  const auto result = parseSemanticAppearance("{");
  ASSERT_FALSE(result);
  EXPECT_EQ(result.diagnostics.front().code, "malformed_json");
}

TEST(Consumer, RejectsMissingField) {
  std::string json = validJson();
  json.replace(json.find("scheme_id"), 9, "renamed");
  const auto result = parseSemanticAppearance(json);
  ASSERT_FALSE(result);
  EXPECT_EQ(result.diagnostics.front().code, "missing_field");
  EXPECT_EQ(result.diagnostics.front().path, "scheme_id");
}

TEST(Consumer, RejectsWrongTypesAndUnsupportedVersions) {
  std::string wrong_type = validJson();
  wrong_type.replace(wrong_type.find("\"blue\""), 6, "false");
  EXPECT_FALSE(parseSemanticAppearance(wrong_type));
  std::string version = validJson();
  version.replace(version.find(":1"), 2, ":2");
  const auto unsupported = parseSemanticAppearance(version);
  ASSERT_FALSE(unsupported);
  EXPECT_EQ(unsupported.diagnostics.front().code, "unsupported_version");
}

TEST(Consumer, RejectsMalformedColorsSizesAndModes) {
  for (const std::string replacement : {"#123456f", "#123456FF", "123456ff", "#gg3456ff"}) {
    std::string json = validJson();
    json.replace(json.find("#123456ff"), 9, replacement);
    EXPECT_FALSE(parseSemanticAppearance(json)) << replacement;
  }
  std::string size = validJson();
  size.replace(size.find("\":12"), 4, "\":0");
  EXPECT_FALSE(parseSemanticAppearance(size));
  std::string mode = validJson();
  const auto mode_value = mode.find("\"color_mode\":\"dark\"");
  mode.replace(mode_value, std::string("\"color_mode\":\"dark\"").size(), "\"color_mode\":\"auto\"");
  EXPECT_FALSE(parseSemanticAppearance(mode));
}

TEST(Projection, IsPureDeterministicAndMapsTierOneValues) {
  const auto parsed = parseSemanticAppearance(validJson());
  ASSERT_TRUE(parsed);
  const auto first = Holonight::Adapters::projectTier1(*parsed.value);
  EXPECT_EQ(first, Holonight::Adapters::projectTier1(*parsed.value));
  EXPECT_EQ(first.gsettings.at("org.gnome.desktop.interface/accent-color"), "blue");
  EXPECT_EQ(first.portal.at("org.freedesktop.appearance/accent-color"), "#123456ff");
  EXPECT_EQ(first.gtk3_settings, first.gtk4_settings);
  EXPECT_FALSE(first.environment.contains("GTK_THEME"));
  EXPECT_EQ(first.xsettings.at("Net/ThemeName"), "");
}

TEST(Projection, MapsExplicitAccentsAndPreservesDefaultFallback) {
  EXPECT_EQ(Holonight::Adapters::gnomeAccent("cyan"), "teal");
  EXPECT_EQ(Holonight::Adapters::gnomeAccent("blue"), "blue");
  EXPECT_EQ(Holonight::Adapters::gnomeAccent("violet"), "purple");
  EXPECT_EQ(Holonight::Adapters::gnomeAccent("yellow"), "yellow");
  EXPECT_FALSE(Holonight::Adapters::gnomeAccent("default"));

  auto parsed = parseSemanticAppearance(validJson());
  parsed.value->accent_id = "default";
  EXPECT_FALSE(
      Holonight::Adapters::projectTier1(*parsed.value).gsettings.contains("org.gnome.desktop.interface/accent-color"));
}

TEST(Projection, OmitsUnavailableGSettingsSchemasAndKeys) {
  const auto parsed = parseSemanticAppearance(validJson());
  ASSERT_TRUE(parsed);
  const auto projection = Holonight::Adapters::projectTier1(*parsed.value);
  EXPECT_TRUE(Holonight::Adapters::availableGSettings(projection, {}).empty());
  const std::set<std::string> available{"org.gnome.desktop.interface/icon-theme"};
  const auto filtered = Holonight::Adapters::availableGSettings(projection, available);
  ASSERT_EQ(filtered.size(), 1U);
  EXPECT_EQ(filtered.at("org.gnome.desktop.interface/icon-theme"), "HoloNight");
}
