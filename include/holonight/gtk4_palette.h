#pragma once

#include "holonight/appearance_contract.h"

#include <string>

namespace Holonight::Adapters {

[[nodiscard]] std::string generateGtk4PaletteCss(const Snapshot &snapshot);

} // namespace Holonight::Adapters
