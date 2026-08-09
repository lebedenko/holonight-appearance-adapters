#include "gtk_palette_common.h"

#include <iomanip>
#include <sstream>

namespace Holonight::Adapters::GtkPalette {
namespace {

std::string cssColor(Color value) {
  std::ostringstream result;
  result << "rgba(" << static_cast<unsigned>(value.red) << ", " << static_cast<unsigned>(value.green) << ", "
         << static_cast<unsigned>(value.blue) << ", " << std::fixed << std::setprecision(6)
         << (static_cast<double>(value.alpha) / 255.0) << ')';
  return result.str();
}

} // namespace

std::string definitions(const Snapshot &snapshot) {
  std::string css;
  for (std::size_t index = 0; index < kColorRoleNames.size(); ++index) {
    css += "@define-color holonight_" + std::string(kColorRoleNames[index]) + " " + cssColor(snapshot.colors[index]) +
           ";\n";
  }
  return css;
}

} // namespace Holonight::Adapters::GtkPalette
