#include "holonight/gtk3_palette.h"

#include "gtk_palette_common.h"

namespace Holonight::Adapters {

std::string generateGtk3PaletteCss(const Snapshot &snapshot) {
  return GtkPalette::definitions(snapshot) + R"CSS(
/* CTV-006 GTK 3 palette only: native metrics and application CSS remain authoritative. */
window, dialog, .background { background-color: @holonight_window_surface; color: @holonight_primary_text; }
textview text, treeview.view, iconview.view, viewport, list, grid { background-color: @holonight_view_surface; color: @holonight_primary_text; }
popover, menu, .context-menu { background-color: @holonight_elevated_surface; color: @holonight_primary_text; }
headerbar, toolbar, actionbar { background-color: @holonight_raised_surface; color: @holonight_primary_text; }
label.dim-label, .dim-label { color: @holonight_secondary_text; }
*:selected { background-color: @holonight_strong_selection; color: @holonight_strong_selection_foreground; }
row:selected, treeview.view:selected { background-color: @holonight_strong_selection; color: @holonight_strong_selection_foreground; }
entry selection, textview text selection { background-color: @holonight_subtle_selection; color: @holonight_subtle_selection_foreground; }
entry selection:focus, textview text selection:focus { background-color: @holonight_subtle_selection_hover; color: @holonight_subtle_selection_foreground; }
button:hover, row:hover { background-color: @holonight_hover_surface; }
*:disabled { color: @holonight_disabled_text; }
button:disabled, entry:disabled { background-color: @holonight_disabled_surface; }
button:active, button:checked { background-color: @holonight_active_border; color: @holonight_inverse_text; }
button, entry, spinbutton, combobox { border-color: @holonight_passive_border; }
button:focus, entry:focus, spinbutton:focus, combobox:focus { border-color: @holonight_focus_border; outline-color: @holonight_focus_border; }
.destructive-action { background-color: @holonight_error; color: @holonight_error_foreground; border-color: @holonight_destructive_border; }
.success { color: @holonight_success; }
.warning { color: @holonight_warning; }
.error { color: @holonight_error; }
)CSS";
}

} // namespace Holonight::Adapters
