#pragma once

#include "holonight/appearance_contract.h"

#include <map>
#include <optional>
#include <set>
#include <string>

namespace Holonight::Adapters {

struct Tier1Projection {
  std::map<std::string, std::string> gsettings;
  std::map<std::string, std::string> xsettings;
  std::map<std::string, std::string> portal;
  std::map<std::string, std::string> environment;
  std::map<std::string, std::string> gtk3_settings;
  std::map<std::string, std::string> gtk4_settings;
  bool operator==(const Tier1Projection &) const = default;
};

[[nodiscard]] Tier1Projection projectTier1(const Snapshot &snapshot);
[[nodiscard]] std::optional<std::string> gnomeAccent(std::string_view accent_id);
[[nodiscard]] std::map<std::string, std::string> availableGSettings(const Tier1Projection &projection,
                                                                    const std::set<std::string> &available_keys);

} // namespace Holonight::Adapters
