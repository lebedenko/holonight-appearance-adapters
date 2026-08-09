#pragma once

#include "holonight/appearance_contract.h"

#include <string>

namespace Holonight::Adapters::GtkPalette {

[[nodiscard]] std::string definitions(const Snapshot &snapshot);

} // namespace Holonight::Adapters::GtkPalette
