#include "holonight/tier1_projection.h"

namespace Holonight::Adapters {
namespace {

std::string fontDescription(const std::string &family, int size) { return family + " " + std::to_string(size); }

} // namespace

std::optional<std::string> gnomeAccent(std::string_view accent_id) {
  if (accent_id == "cyan")
    return "teal";
  if (accent_id == "blue")
    return "blue";
  if (accent_id == "violet")
    return "purple";
  if (accent_id == "yellow")
    return "yellow";
  return std::nullopt;
}

Tier1Projection projectTier1(const Snapshot &snapshot) {
  Tier1Projection result;
  const std::string ui_font = fontDescription(snapshot.ui_font_family, snapshot.ui_font_point_size);
  const std::string mono_font = fontDescription(snapshot.monospace_font_family, snapshot.monospace_font_point_size);
  const std::string prefer_dark = snapshot.color_mode == "dark" ? "true" : "false";

  result.gsettings = {
      {"org.gnome.desktop.interface/color-scheme", snapshot.color_mode == "dark" ? "prefer-dark" : "prefer-light"},
      {"org.gnome.desktop.interface/icon-theme", snapshot.icon_theme},
      {"org.gnome.desktop.interface/cursor-theme", snapshot.cursor_theme},
      {"org.gnome.desktop.interface/font-name", ui_font},
      {"org.gnome.desktop.interface/monospace-font-name", mono_font}};
  if (const auto accent = gnomeAccent(snapshot.accent_id)) {
    result.gsettings["org.gnome.desktop.interface/accent-color"] = *accent;
  }

  result.xsettings = {{"Net/ThemeName", ""},
                      {"Net/IconThemeName", snapshot.icon_theme},
                      {"Gtk/CursorThemeName", snapshot.cursor_theme},
                      {"Gtk/FontName", ui_font},
                      {"Gtk/MonospaceFontName", mono_font},
                      {"Gtk/ApplicationPreferDarkTheme", prefer_dark}};
  result.portal = {{"org.freedesktop.appearance/color-scheme", snapshot.color_mode == "dark" ? "1" : "2"},
                   {"org.freedesktop.appearance/accent-color", encodeColor(snapshot.colors[0])}};
  // GTK_THEME is intentionally absent: environment projection is session-start-only metadata.
  result.environment = {{"XCURSOR_THEME", snapshot.cursor_theme}};

  const std::map<std::string, std::string> settings{{"gtk-application-prefer-dark-theme", prefer_dark},
                                                    {"gtk-icon-theme-name", snapshot.icon_theme},
                                                    {"gtk-cursor-theme-name", snapshot.cursor_theme},
                                                    {"gtk-font-name", ui_font}};
  result.gtk3_settings = settings;
  result.gtk4_settings = settings;
  return result;
}

std::map<std::string, std::string> availableGSettings(const Tier1Projection &projection,
                                                      const std::set<std::string> &available_keys) {
  std::map<std::string, std::string> result;
  for (const auto &[key, value] : projection.gsettings) {
    if (available_keys.contains(key)) {
      result.emplace(key, value);
    }
  }
  return result;
}

} // namespace Holonight::Adapters
