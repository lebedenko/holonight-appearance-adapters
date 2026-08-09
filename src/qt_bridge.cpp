#include "holonight/qt_bridge.h"

#include "semanticappearance.h"

#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>

namespace Holonight::Adapters {
namespace {

QString color(const QColor &value) {
  return QStringLiteral("#%1%2%3%4")
      .arg(value.red(), 2, 16, QLatin1Char('0'))
      .arg(value.green(), 2, 16, QLatin1Char('0'))
      .arg(value.blue(), 2, 16, QLatin1Char('0'))
      .arg(value.alpha(), 2, 16, QLatin1Char('0'));
}

} // namespace

QByteArray serializeSemanticAppearance(const SemanticAppearance &value) {
  QJsonObject object;
  object[QStringLiteral("contract_version")] = static_cast<qint64>(value.contract_version);
  object[QStringLiteral("scheme_id")] = value.scheme_id;
  object[QStringLiteral("accent_id")] = value.accent_id;
  object[QStringLiteral("color_mode")] =
      value.color_mode == ColorMode::Dark ? QStringLiteral("dark") : QStringLiteral("light");
#define COLOR_FIELD(name) object[QStringLiteral(#name)] = color(value.name)
  COLOR_FIELD(accent);
  COLOR_FIELD(accent_foreground);
  COLOR_FIELD(window_surface);
  COLOR_FIELD(view_surface);
  COLOR_FIELD(elevated_surface);
  COLOR_FIELD(raised_surface);
  COLOR_FIELD(hover_surface);
  COLOR_FIELD(disabled_surface);
  COLOR_FIELD(strong_selection);
  COLOR_FIELD(strong_selection_foreground);
  COLOR_FIELD(subtle_selection);
  COLOR_FIELD(subtle_selection_hover);
  COLOR_FIELD(subtle_selection_foreground);
  COLOR_FIELD(primary_text);
  COLOR_FIELD(secondary_text);
  COLOR_FIELD(disabled_text);
  COLOR_FIELD(inverse_text);
  COLOR_FIELD(passive_border);
  COLOR_FIELD(active_border);
  COLOR_FIELD(focus_border);
  COLOR_FIELD(destructive_border);
  COLOR_FIELD(success);
  COLOR_FIELD(warning);
  COLOR_FIELD(error);
  COLOR_FIELD(error_foreground);
#undef COLOR_FIELD
  object[QStringLiteral("ui_font_family")] = value.ui_font_family;
  object[QStringLiteral("ui_font_point_size")] = value.ui_font_point_size;
  object[QStringLiteral("monospace_font_family")] = value.monospace_font_family;
  object[QStringLiteral("monospace_font_point_size")] = value.monospace_font_point_size;
  object[QStringLiteral("icon_theme")] = value.icon_theme;
  object[QStringLiteral("fallback_icon_theme")] = value.fallback_icon_theme;
  object[QStringLiteral("cursor_theme")] = value.cursor_theme;
  return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

} // namespace Holonight::Adapters
