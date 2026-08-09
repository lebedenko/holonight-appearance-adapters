# CTV-006: GTK palette evaluation

Status: Implemented for discovery; both palette candidates are rejected for production pending CTV-007.

Baseline: `holonight-appearance-adapters@4cc57a6bcd273e9464f913c593952f8306cb10d9`.

## Scope and architecture

`generateGtk3PaletteCss(const Snapshot&)` and `generateGtk4PaletteCss(const Snapshot&)` are pure, deterministic
transformations of semantic appearance v1. Each generator is a separate static target. The only shared target
serializes the existing 25 colors as GTK symbolic color definitions; it has no GTK dependency. The GTK 3 and GTK 4
probe executables link exactly one toolkit major, and an automated linkage check enforces that boundary.

The artifacts are non-installed discovery output. They do not write files, mutate settings, change the environment,
add semantic roles, or set widget geometry, spacing, typography, icons, transitions, or animations. Application and
native toolkit behavior remains the authority for those properties.

## Semantic mapping

Every v1 color is exposed once as `@holonight_<role>`. Deterministic `rgba(r, g, b, a)` serialization preserves alpha
and remains compatible with GTK 3, which does not accept the contract's eight-digit hexadecimal wire notation.
The fixed rules map the roles as follows:

| Category | Semantic roles | GTK application |
|---|---|---|
| Base surfaces | `window_surface`, `view_surface`, `elevated_surface`, `raised_surface` | windows/backgrounds, content views, popovers/menus, header/tool/action bars |
| Text | `primary_text`, `secondary_text`, `inverse_text` | normal content, dim labels, active controls |
| Selection | `strong_selection`, `strong_selection_foreground`, `subtle_selection`, `subtle_selection_hover`, `subtle_selection_foreground` | selected rows/items and text selections |
| Interaction | `hover_surface`, `disabled_surface`, `disabled_text`, `active_border`, `focus_border` | hover, insensitive widgets, active controls, focus boundaries |
| Borders/status | `passive_border`, `destructive_border`, `success`, `warning`, `error`, `error_foreground` | controls, destructive actions, status classes |

GTK 3 uses its standard `treeview.view`, `iconview.view`, menu, and `:focus` nodes. GTK 4 uses `columnview`,
`listview`, `gridview`, `popover > contents`, `dropdown`, `:focus-visible`, and `:focus-within`. Both use symbolic
colors; the common path does not require the CSS custom properties added in GTK 4.16.

## Applicability and evidence

The compatibility floor is Ubuntu 24.04 (GTK 3.24.41 and GTK 4.14) plus the current Arch stack (GTK 3.24.52 and
GTK 4.22.4). CI builds on Ubuntu 24.04, loads each generated artifact in its matching toolkit under isolated
Xvfb/D-Bus/home state, fails on any CSS parser diagnostic, observes representative surface, text, selection,
disabled, focus, and status symbolic colors, and checks linkage. The same probe is run locally on the Arch stack.
Unit tests cover deterministic output, all exact role definitions including alpha, major-specific syntax, forbidden
geometry/mutation directives, and every built-in Qt scheme/accent round trip into both generators.

The prototypes remain applicable only to conventional GTK widgets that retain standard node names and permit a
global user palette. GTK CSS is not a stable application-facing theming API. Applications may add custom nodes,
classes, or higher-priority CSS, and the candidate deliberately does not override them. GIMP and similarly
application-owned styling therefore remain native fallbacks. Libadwaita is always Tier 1/native fallback: the GTK 4
fragment defines no libadwaita variables or private nodes and must never be presented as full libadwaita theming.

## Evaluation and decisions

### GTK 3 — Reject

The prototype parses and its standard widgets expose the intended semantic colors, but a global fragment cannot
guarantee all Tier 2 categories in application-owned widgets without broadening selectors into application CSS.
That would violate the preservation requirement. The representative Inkscape/GIMP gate therefore cannot be made
reliable by this adapter contract. Recommend Tier 1/native fallback and mark CTV-301 `Superseded` after CTV-007.

### GTK 4 — Reject

The conventional GTK 4 candidate parses without GTK 4.16-only syntax, but the required global applicability and
libadwaita coherence gates conflict. A fragment broad enough to guarantee all Tier 2 colors would override
application/libadwaita styling; the deliberately narrow fragment cannot guarantee those visible mappings. Keep
libadwaita and application-owned palettes native. Recommend Tier 1/native fallback and mark CTV-302 `Superseded`
after CTV-007.

These are discovery recommendations, not umbrella state changes. CTV-007 owns acceptance of the ecosystem outcome.

## Limitations and verification gates

The generated CSS is not a complete GTK theme, has no reload/persistence mechanism, and provides no contract for
custom application nodes. Parser and symbolic-color probes establish compatibility of the fragment itself, not
pixel identity across native themes. Contrast depends on the producer's semantic palette; the adapter preserves
colors exactly and must not synthesize replacements. Production must independently enforce 4.5:1 text/selection
contrast and 3:1 focus/control boundaries before applying a snapshot.

Visual checks use isolated Wayland sessions: Seahorse and Inkscape for GTK 3, with GIMP retained as an
application-owned fallback; Widget Factory and pavucontrol for GTK 4; and pwvucontrol and Ghostty for libadwaita or
application-native fallback. A failure to start, loss of coherent native styling, missing Tier 2 category, parser
diagnostic on either baseline, or contrast failure confirms the rejection and Tier 1 fallback.
