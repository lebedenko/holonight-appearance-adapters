#pragma once

#include "holonight/appearance_contract.h"

#include <string>

inline std::string validJson() {
  std::string json = R"({"contract_version":1,"scheme_id":"holonight-dark","accent_id":"blue","color_mode":"dark")";
  for (const auto role : Holonight::Adapters::kColorRoleNames) {
    json += ",\"" + std::string(role) + "\":\"#123456ff\"";
  }
  return json +
         R"(,"ui_font_family":"Inter","ui_font_point_size":12,"monospace_font_family":"JetBrains Mono","monospace_font_point_size":13,"icon_theme":"HoloNight","fallback_icon_theme":"Papirus","cursor_theme":"HoloNight"})";
}
