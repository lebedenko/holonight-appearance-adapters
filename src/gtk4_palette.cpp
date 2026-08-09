#include "holonight/gtk4_palette.h"

#include "gtk_palette_common.h"

namespace Holonight::Adapters {

std::string generateGtk4PaletteCss(const Snapshot &snapshot) {
  return GtkPalette::definitions(snapshot) + R"CSS(
/* CTV-006 GTK 4 palette only: no GTK 4.16 custom properties or native metric overrides. */
window, dialog, .background { background-color: @holonight_window_surface; color: @holonight_primary_text; }
textview text, columnview, listview, gridview, viewport { background-color: @holonight_view_surface; color: @holonight_primary_text; }
popover > contents { background-color: @holonight_elevated_surface; color: @holonight_primary_text; }
headerbar, toolbar, actionbar { background-color: @holonight_raised_surface; color: @holonight_primary_text; }
label.dim-label, .dim-label { color: @holonight_secondary_text; }
*:selected { background-color: @holonight_strong_selection; color: @holonight_strong_selection_foreground; }
row:selected, listview > row:selected { background-color: @holonight_strong_selection; color: @holonight_strong_selection_foreground; }
entry selection, textview text selection { background-color: @holonight_subtle_selection; color: @holonight_subtle_selection_foreground; }
entry selection:focus, textview text selection:focus { background-color: @holonight_subtle_selection_hover; color: @holonight_subtle_selection_foreground; }
button:hover, row:hover { background-color: @holonight_hover_surface; }
*:disabled { color: @holonight_disabled_text; }
button:disabled, entry:disabled { background-color: @holonight_disabled_surface; }
button:active, button:checked { background-color: @holonight_active_border; color: @holonight_inverse_text; }
button, entry, spinbutton, dropdown { border-color: @holonight_passive_border; }
button:focus-visible, entry:focus-within, spinbutton:focus-within, dropdown:focus-visible { border-color: @holonight_focus_border; outline-color: @holonight_focus_border; }
.destructive-action { background-color: @holonight_error; color: @holonight_error_foreground; border-color: @holonight_destructive_border; }
.success { color: @holonight_success; }
.warning { color: @holonight_warning; }
.error { color: @holonight_error; }
)CSS";
}

} // namespace Holonight::Adapters
