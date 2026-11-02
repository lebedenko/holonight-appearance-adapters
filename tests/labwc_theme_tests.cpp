// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#include "holonight/appearance.h"
#include "holonight/theme_catalog.h"
#include "labwc_theme.h"

#include <gtest/gtest.h>

TEST(LabwcTheme, ExactSemanticColorsAcrossSchemesAndAccents) {
  for (const auto& variant : Holonight::themeVariants()) {
    for (const auto& accent : {"default", "cyan", "blue", "violet", "yellow"}) {
      SCOPED_TRACE((variant.id + QLatin1Char('/') + QString::fromLatin1(accent)).toStdString());
      Holonight::ResolvedAppearance input;
      input.scheme = variant.id;
      input.theme_scheme = variant.scheme;
      input.color_mode = variant.mode;
      input.accent = QString::fromLatin1(accent);
      auto appearance = Holonight::resolveSemanticAppearance(input);
      // Exercise nonopaque serialization independently of built-in token alpha.
      for (const int alpha : {appearance.subtle_selection.alpha(), 73}) {
        appearance.subtle_selection.setAlpha(alpha);
        const auto files = Holonight::Adapters::labwcTheme(appearance);
        const auto theme = files.value("themerc");
        auto expectColor = [&](const QString& key, const QColor& value) {
          const auto expected =
              key + ": " + value.name() + QStringLiteral("%1").arg(value.alpha(), 2, 16, QLatin1Char('0')) + '\n';
          EXPECT_TRUE(theme.contains(expected.toUtf8())) << expected.toStdString();
        };
        expectColor("window.active.title.bg.color", appearance.window_surface);
        expectColor("window.inactive.title.bg.color", appearance.window_surface);
        for (const auto& style : {"classic", "thumbnail"}) {
          const auto prefix = QString("osd.window-switcher.style-%1.item.active.").arg(style);
          expectColor(prefix + "bg.color", appearance.subtle_selection);
          expectColor(prefix + "border.color", appearance.accent);
        }
        for (const auto& kind : {"region", "edge"}) {
          const auto prefix = QString("snapping.overlay.%1.").arg(kind);
          expectColor(prefix + "bg.color", appearance.subtle_selection);
          expectColor(prefix + "border.color", appearance.accent);
        }
        EXPECT_FALSE(theme.contains(".enabled:"));
        for (const bool active : {false, true}) {
          for (const bool hover : {false, true}) {
            for (const auto& name :
                 {"close", "max", "iconify", "menu", "shade", "shade_toggled", "desk", "desk_toggled", "max_toggled"}) {
              const auto svg =
                  files.value(QString(name) + (hover ? "_hover" : "") + (active ? "-active.svg" : "-inactive.svg"));
              const auto foreground =
                  hover ? (QString(name) == "close" ? appearance.error_foreground : appearance.primary_text)
                        : (active ? appearance.primary_text : appearance.disabled_text);
              EXPECT_TRUE(svg.contains(("stroke=\"" + foreground.name() + "\"").toUtf8()));
              if (hover)
                EXPECT_TRUE(svg.contains(
                    ("fill=\"" + (QString(name) == "close" ? appearance.error : appearance.hover_surface).name() + "\"")
                        .toUtf8()));
            }
          }
        }
      }
    }
  }
}
