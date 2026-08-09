#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Holonight::Adapters {

inline constexpr std::uint32_t kContractVersion = 1;

struct Color {
  std::uint8_t red{};
  std::uint8_t green{};
  std::uint8_t blue{};
  std::uint8_t alpha{};
  bool operator==(const Color &) const = default;
};

inline constexpr std::array<std::string_view, 25> kColorRoleNames{"accent",
                                                                  "accent_foreground",
                                                                  "window_surface",
                                                                  "view_surface",
                                                                  "elevated_surface",
                                                                  "raised_surface",
                                                                  "hover_surface",
                                                                  "disabled_surface",
                                                                  "strong_selection",
                                                                  "strong_selection_foreground",
                                                                  "subtle_selection",
                                                                  "subtle_selection_hover",
                                                                  "subtle_selection_foreground",
                                                                  "primary_text",
                                                                  "secondary_text",
                                                                  "disabled_text",
                                                                  "inverse_text",
                                                                  "passive_border",
                                                                  "active_border",
                                                                  "focus_border",
                                                                  "destructive_border",
                                                                  "success",
                                                                  "warning",
                                                                  "error",
                                                                  "error_foreground"};

struct Snapshot {
  std::uint32_t contract_version{};
  std::string scheme_id;
  std::string accent_id;
  std::string color_mode;
  std::array<Color, kColorRoleNames.size()> colors{};
  std::string ui_font_family;
  int ui_font_point_size{};
  std::string monospace_font_family;
  int monospace_font_point_size{};
  std::string icon_theme;
  std::string fallback_icon_theme;
  std::string cursor_theme;
  bool operator==(const Snapshot &) const = default;
};

struct Diagnostic {
  std::string code;
  std::string path;
  std::string message;
  bool operator==(const Diagnostic &) const = default;
};

struct ParseResult {
  std::optional<Snapshot> value;
  std::vector<Diagnostic> diagnostics;
  explicit operator bool() const noexcept { return value.has_value(); }
};

[[nodiscard]] ParseResult parseSemanticAppearance(std::string_view json);
[[nodiscard]] std::string encodeColor(Color color);

} // namespace Holonight::Adapters
