#pragma once

#include "holonight/appearance_contract.h"

#include <string>

namespace Holonight::Adapters {

[[nodiscard]] std::string generateGtk3PaletteCss(const Snapshot &snapshot);

} // namespace Holonight::Adapters
