#include "holonight/appearance_contract.h"

#include <algorithm>
#include <charconv>
#include <iterator>
#include <json-glib/json-glib.h>
#include <memory>
#include <utility>

namespace Holonight::Adapters {
namespace {

using ParserPtr = std::unique_ptr<JsonParser, decltype(&g_object_unref)>;

void diagnose(ParseResult& result, std::string code, std::string path, std::string message) {
  result.diagnostics.push_back({.code = std::move(code), .path = std::move(path), .message = std::move(message)});
}

bool member(JsonObject* object, const char* name, JsonNodeType type, ParseResult& result) {
  if (json_object_has_member(object, name) == 0) {
    diagnose(result, "missing_field", name, "required field is missing");
    return false;
  }
  JsonNode* node = json_object_get_member(object, name);
  if (json_node_get_node_type(node) != type) {
    diagnose(result, "wrong_type", name, "field has the wrong JSON type");
    return false;
  }
  return true;
}

std::string stringMember(JsonObject* object, const char* name, ParseResult& result) {
  if (!member(object, name, JSON_NODE_VALUE, result) ||
      json_node_get_value_type(json_object_get_member(object, name)) != G_TYPE_STRING) {
    if (result.diagnostics.empty() || result.diagnostics.back().path != name) {
      diagnose(result, "wrong_type", name, "field must be a string");
    }
    return {};
  }
  const char* value = json_object_get_string_member(object, name);
  if (value == nullptr || *value == '\0') {
    diagnose(result, "invalid_value", name, "field must not be empty");
    return {};
  }
  return value;
}

int intMember(JsonObject* object, const char* name, ParseResult& result) {
  if (!member(object, name, JSON_NODE_VALUE, result) ||
      json_node_get_value_type(json_object_get_member(object, name)) != G_TYPE_INT64) {
    if (result.diagnostics.empty() || result.diagnostics.back().path != name) {
      diagnose(result, "wrong_type", name, "field must be an integer");
    }
    return 0;
  }
  const auto value = json_object_get_int_member(object, name);
  if (value < 1 || value > 512) {
    diagnose(result, "invalid_value", name, "point size must be between 1 and 512");
    return 0;
  }
  return static_cast<int>(value);
}

std::optional<Color> parseColor(std::string_view value) {
  if (value.size() != 9 || value.front() != '#') {
    return std::nullopt;
  }
  Color result;
  std::array<std::uint8_t*, 4> channels{&result.red, &result.green, &result.blue, &result.alpha};
  std::size_t offset = 1;
  for (auto* channel : channels) {
    unsigned parsed = 0;
    const auto digits = value.substr(offset, 2);
    const char* end = std::next(digits.data(), 2);
    const auto conversion = std::from_chars(digits.data(), end, parsed, 16);
    if (conversion.ec != std::errc{} || conversion.ptr != end) {
      return std::nullopt;
    }
    // Uppercase is deliberately rejected to keep one canonical wire representation.
    if (std::ranges::any_of(digits, [](char digit) { return digit >= 'A' && digit <= 'F'; })) {
      return std::nullopt;
    }
    *channel = static_cast<std::uint8_t>(parsed);
    offset += 2;
  }
  return result;
}

}  // namespace

ParseResult parseSemanticAppearance(std::string_view json) {
  ParseResult result;
  ParserPtr parser(json_parser_new(), &g_object_unref);
  GError* error = nullptr;
  if (json_parser_load_from_data(parser.get(), json.data(), static_cast<gssize>(json.size()), &error) == 0) {
    diagnose(result, "malformed_json", "$", error != nullptr ? error->message : "invalid JSON");
    g_clear_error(&error);
    return result;
  }
  JsonNode* root = json_parser_get_root(parser.get());
  if (root == nullptr || json_node_get_node_type(root) != JSON_NODE_OBJECT) {
    diagnose(result, "wrong_type", "$", "root must be an object");
    return result;
  }

  JsonObject* object = json_node_get_object(root);
  Snapshot snapshot;
  if (member(object, "contract_version", JSON_NODE_VALUE, result) &&
      json_node_get_value_type(json_object_get_member(object, "contract_version")) == G_TYPE_INT64) {
    const auto version = json_object_get_int_member(object, "contract_version");
    if (std::cmp_not_equal(version, kContractVersion)) {
      diagnose(result, "unsupported_version", "contract_version", "only semantic appearance contract v1 is supported");
    } else {
      snapshot.contract_version = static_cast<std::uint32_t>(version);
    }
  } else if (result.diagnostics.empty() || result.diagnostics.back().path != "contract_version") {
    diagnose(result, "wrong_type", "contract_version", "field must be an integer");
  }

  snapshot.scheme_id = stringMember(object, "scheme_id", result);
  snapshot.accent_id = stringMember(object, "accent_id", result);
  snapshot.color_mode = stringMember(object, "color_mode", result);
  if (!snapshot.color_mode.empty() && snapshot.color_mode != "dark" && snapshot.color_mode != "light") {
    diagnose(result, "invalid_value", "color_mode", "mode must be dark or light");
  }

  for (std::size_t index = 0; index < kColorRoleNames.size(); ++index) {
    const std::string name(kColorRoleNames.at(index));
    const std::string encoded = stringMember(object, name.c_str(), result);
    if (!encoded.empty()) {
      const auto color = parseColor(encoded);
      if (!color) {
        diagnose(result, "invalid_color", name, "color must be lowercase sRGB #rrggbbaa");
      } else {
        snapshot.colors.at(index) = *color;
      }
    }
  }

  snapshot.ui_font_family = stringMember(object, "ui_font_family", result);
  snapshot.ui_font_point_size = intMember(object, "ui_font_point_size", result);
  snapshot.monospace_font_family = stringMember(object, "monospace_font_family", result);
  snapshot.monospace_font_point_size = intMember(object, "monospace_font_point_size", result);
  snapshot.icon_theme = stringMember(object, "icon_theme", result);
  snapshot.fallback_icon_theme = stringMember(object, "fallback_icon_theme", result);
  snapshot.cursor_theme = stringMember(object, "cursor_theme", result);

  if (result.diagnostics.empty()) {
    result.value = std::move(snapshot);
  }
  return result;
}

std::string encodeColor(Color color) {
  constexpr std::string_view hex = "0123456789abcdef";
  std::string result = "#";
  result.reserve(9);
  const std::array values{color.red, color.green, color.blue, color.alpha};
  for (const auto channel : values) {
    result.push_back(hex.at(static_cast<std::size_t>(channel >> 4)));
    result.push_back(hex.at(static_cast<std::size_t>(channel & 0x0f)));
  }
  return result;
}

}  // namespace Holonight::Adapters
