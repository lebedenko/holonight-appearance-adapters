// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#include "labwc_theme.h"
namespace Holonight::Adapters {
namespace {
QByteArray buttonSvg(const SemanticAppearance& appearance, const QString& name, const QString& path, bool active,
                     bool hover) {
  QColor foreground = active ? appearance.primary_text : appearance.disabled_text;
  if (hover) {
    foreground = name == "close" ? appearance.error_foreground : appearance.primary_text;
  }
  QString svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="30" height="30" viewBox="0 0 30 30">)";
  if (hover) {
    svg += QString(R"(<rect width="30" height="30" rx="6" fill="%1"/>)")
               .arg((name == "close" ? appearance.error : appearance.hover_surface).name());
  }
  svg += QString(
             "<path transform=\"translate(8 8)\" d=\"%1\" fill=\"none\" stroke=\"%2\" stroke-width=\"1.5\" "
             "stroke-linecap=\"round\" stroke-linejoin=\"round\"/></svg>\n")
             .arg(path, foreground.name());
  return svg.toUtf8();
}
}  // namespace

QMap<QString, QByteArray> labwcTheme(const SemanticAppearance& appearance) {
  QMap<QString, QByteArray> files;
  QString theme = QStringLiteral("# Generated HoloNight Accent theme\n");
  auto add = [&](const QString& key, const QString& value) { theme += key + ": " + value + '\n'; };
  auto color = [&](const QString& key, const QColor& value) {
    add(key, value.name() + QStringLiteral("%1").arg(value.alpha(), 2, 16, QLatin1Char('0')));
  };
  add("border.width", "1");
  add("window.titlebar.padding.width", "10");
  add("window.titlebar.padding.height", "2");
  add("window.label.text.justify", "Left");
  add("window.button.width", "30");
  add("window.button.height", "30");
  add("window.button.spacing", "2");
  add("window.button.hover.bg.corner-radius", "6");
  color("window.button.hover.bg.color", appearance.hover_surface);
  for (const QString& state : {QStringLiteral("active"), QStringLiteral("inactive")}) {
    bool active = state == "active";
    QString prefix = "window." + state + '.';
    add(prefix + "title.bg", "Solid");
    color(prefix + "title.bg.color", appearance.window_surface);
    color(prefix + "border.color", active ? appearance.accent : appearance.passive_border);
    color(prefix + "label.text.color", active ? appearance.primary_text : appearance.secondary_text);
    color(prefix + "button.unpressed.image.color", active ? appearance.primary_text : appearance.disabled_text);
    add(prefix + "shadow.size", active ? "48" : "24");
    add(prefix + "shadow.color", active ? "#00000060" : "#00000030");
  }
  color("menu.border.color", appearance.passive_border);
  color("menu.items.bg.color", appearance.elevated_surface);
  color("menu.items.text.color", appearance.primary_text);
  color("menu.items.active.bg.color", appearance.hover_surface);
  color("menu.items.active.text.color", appearance.primary_text);
  color("menu.separator.color", appearance.passive_border);
  color("menu.title.bg.color", appearance.raised_surface);
  color("menu.title.text.color", appearance.secondary_text);
  add("menu.title.text.justify", "Left");
  color("osd.bg.color", appearance.elevated_surface);
  color("osd.border.color", appearance.accent);
  color("osd.label.text.color", appearance.primary_text);
  color("osd.window-switcher.style-classic.item.active.bg.color", appearance.subtle_selection);
  color("osd.window-switcher.style-classic.item.active.border.color", appearance.accent);
  files["themerc"] = theme.toUtf8();
  const QMap<QString, QString> paths{
      {"iconify", "M2 11h10"},
      {"close", "M3 3l8 8M11 3l-8 8"},
      {"max", "M2 2h10v10H2z"},
      {"max_toggled", "M2 5h7v7H2zM5 5V2h7v7H9"},
      {"menu", "M2 3h10M2 7h10M2 11h10"},
      {"shade", "M3 9l4-4 4 4"},
      {"shade_toggled", "M3 5l4 4 4-4"},
      {"desk", "M7 2v10M2 7h10"},
      {"desk_toggled", "M2 7l3 3 7-7"},
  };
  for (auto it = paths.begin(); it != paths.end(); ++it) {
    for (bool active : {true, false}) {
      for (bool hover : {false, true}) {
        files[it.key() + (hover ? "_hover" : "") + (active ? "-active.svg" : "-inactive.svg")] =
            buttonSvg(appearance, it.key(), it.value(), active, hover);
      }
    }
  }
  return files;
}
}  // namespace Holonight::Adapters
