# HoloNight labwc theme

The adapter owns the Accent window-decoration theme and its specialized control
artwork. Qt supplies the semantic colors and resolved UI fonts; this component
does not maintain a second palette. The Settings page is deferred. Shell's existing
adapter invocation synchronizes the theme once the user has selected it.

## Install and enable

Build and install the adapter normally. CMake generates the default dark theme
from Qt's default resolved appearance and installs `themerc` and 36 SVGs below
`${CMAKE_INSTALL_DATADIR}/themes/HoloNight/labwc/`. The XML example is installed
below `${CMAKE_INSTALL_DATADIR}/doc/HoloNightAppearanceAdapters/`. Prefixes and
`DESTDIR` are honored; installation creates no user configuration. Cross compiling
requires a runnable generator, as this build executes the semantic Qt resolver.

Back up your `rc.xml` and `themerc-override` before enabling the theme. Merge
[labwc-theme.xml](labwc-theme.xml) into your existing `<theme>` section; do not
replace your complete configuration with the example. The default layout is
`:iconify,max,close`, with no application icon. Decoration layout, corner radius,
shadows, and maximized decoration policy stay user-managed in XML. Client-side
decoration policy is unchanged.

Review your `themerc-override` and manually remove superseded colors and geometry.
Overrides take precedence over the theme. `apply` and `status` report overlapping
keys, including wildcard keys, and always preserve that file. Keep unrelated
settings and your backup.

```sh
holonight-appearance-adapter apply --appearance ~/.config/holonight/appearance.toml --json
labwc --reconfigure
holonight-appearance-adapter status --appearance ~/.config/holonight/appearance.toml --json
```

The generated synchronized theme lives below
`${XDG_DATA_HOME:-~/.local/share}/themes/HoloNight/labwc/`, ahead of the installed
fallback in labwc's theme search. A custom installation prefix must be discoverable
through labwc's theme search paths / `XDG_DATA_DIRS`. Theme selection, override
precedence, and state filenames follow the [labwc theme manual](https://labwc.github.io/labwc-theme.5.html).

For sessions using `labwc -C DIR`, pass `--labwc-config DIR` to `apply`, `status`,
and `revert`. Otherwise the adapter reads the first `rc.xml` found in
`HOLONIGHT_LABWC_CONFIG_DIR` when supplied by a managed session, then
`${XDG_CONFIG_HOME:-~/.config}/labwc` and then the colon-separated
`${XDG_CONFIG_DIRS:-/etc/xdg}` locations, matching ordinary labwc configuration
lookup. Put a writable copy of a system `rc.xml` in your user configuration before
using font synchronization. The adapter does not create `rc.xml`. Configurations
using `--merge-config` or a separate `-c FILE` should use a single effective
`rc.xml` in an explicit configuration directory for synchronization.

## Projection and ownership

Title surfaces are solid window surfaces. Active borders use the selected accent;
inactive borders are neutral. Titles are left-aligned, inactive controls subdued.
Buttons have 30×30 logical-pixel targets, centered 14-pixel glyph canvases, 2-pixel
spacing, 6-pixel hover radii, and titlebar padding of 10 horizontal / 2 vertical
pixels. The resulting titlebar is approximately 34 pixels, growing for taller
fonts. Active/inactive shadows are 48/24 pixels. Menu and OSD colors use the same
semantic appearance. The mockup's perimeter gradient is represented by a solid
accent border.

Every active/inactive and hover state is provided for minimize, maximize, restore,
close, menu, shade/unshade, and all-desktops toggles. SVGs use explicit colors,
1.5-pixel rounded strokes, and labwc's state-specific filenames. Close hover uses
semantic error/error-foreground colors; other hover backgrounds use hover surface.

`ActiveWindow`, `InactiveWindow`, `MenuItem` and `MenuHeader` font `name` and
`size` values are updated. All four places follow resolved `ui_font` and `ui_font_size`; decorative title-font
settings do not affect server-side decorations. UI points convert to logical pixels at 96 DPI, rounded to the nearest integer
(e.g. 10 pt → 13 px, 18 pt → 24 px). OSD fonts, font weights/slants, comments,
and other settings are retained. XML formatting may change.

The existing JSON v1 protocol and supported statuses/modes remain unchanged.
Generated files and font values are recorded alongside GTK/KDE ownership in
`${XDG_STATE_HOME:-~/.local/state}/holonight/appearance-adapters.json`. Unowned
existing assets are not adopted except for a complete, byte-for-byte unchanged
default fallback installed at the synchronized path (e.g. a `~/.local` prefix); its
original files are retained for revert; modified managed assets or fonts produce a
`conflict` and block the labwc update as a unit. Changing config/data paths while
outputs remain owned also conflicts: revert using the original paths first.
Malformed XML is preserved and reported as a conflict; other adapters can still run.

A process lock serializes adapter operations. The entire theme is staged on the
same filesystem before directory replacement. Failed theme/font/state writes roll
back changes through the existing transaction. A durable labwc before-image journal
recovers an interrupted apply on the next operation; external edits during recovery
are preserved and reported as errors. The journal remains available until recovery
can succeed. Revert writes are rolled back if saving the updated ownership state fails.

After a successful labwc change, the adapter signals the live compositor once when
`LABWC_PID` identifies a same-user labwc process. This is the same SIGHUP mechanism
used by `labwc --reconfigure`. Reload failures report `unavailable` and retain valid
assets. Without an identified process, reload manually. Idempotent apply does not
reload.

## Disable and recover

Switching the XML name away from `HoloNight` stops subsequent synchronization.
To remove managed outputs, run:

```sh
holonight-appearance-adapter revert --json
# Add --labwc-config DIR if that was used when applying.
```

Revert restores original owned font values and removes unchanged generated assets,
exposing the installed fallback. Externally modified outputs remain in place with
conflict diagnostics. It also reverts the other outputs owned by this CLI. Theme
selection is never changed automatically; to disable HoloNight completely, change
the XML theme name after reverting. Restore your override backup manually if needed.

If a conflict prevents recovery, back up the affected file, compare it with the
`last` / `original` values in state or the recovery journal, and resolve the mismatch
before retrying. Do not discard ownership state to force adoption of existing assets.

## Verification

CTest includes semantic consumers, CLI regressions, labwc ownership/projection and
recovery checks, concurrent applies, and staged install/manifest uninstall checks
for `/usr`, user prefixes, and `DESTDIR`. Protocol statuses and modes are checked
against the Settings client's accepted sets.

Optional visual smoke tests require labwc **0.20.2**, GTK3 GI bindings, `grim`, and
`wlr-randr`, plus permission to create an isolated Wayland socket:

```sh
python3 tests/labwc_smoke.py build/test/holonight-appearance-adapter /tmp/labwc-review
```

The harness uses a headless pixman compositor, disposable XDG directories, two GTK
windows with server-side decorations, and a virtual pointer scoped to that compositor.
It captures active/inactive titles, long-title truncation, hover, maximize/restore, shade, native window menus and workspace submenus with long names, live scheme/accent
reload, sticky workspace indicators, right-edge submenu placement, and 1×/2× output rendering. Review the PNGs and compositor log; this is not
part of the default CTest suite. Screenshots from the initial implementation were
reviewed on labwc 0.20.2 / wlroots 0.20.2 / Qt 6.11.2.

Managed HoloNight sessions expose their persistent selected directory through
`HOLONIGHT_LABWC_CONFIG_DIR`; explicit `--labwc-config` takes precedence. Settings
needs no additional arguments. Native rc.xml, menu and theme changes remain visible
on `labwc --reconfigure`, including atomic replacements. Changing the selected
entrypoint requires a new login. Existing ownership of another directory produces
normal conflict guidance; revert against the original directory before switching.
An installed read-only fallback is not a writable personal configuration: create
`~/.config/labwc/rc.xml` and log in again to synchronize title fonts. HoloNight theme
selection remains opt-in.

Snapping region and edge overlays use the semantic `subtle_selection` background
(including its alpha) and `accent` border. Classic and thumbnail switchers use the same
selected colors. The adapter does not set overlay enablement keys; compositor defaults
and user enablement settings continue to apply. These generated colors participate in
the existing ownership, regeneration, override detection and conservative revert rules.

All native labwc menus share this styling, including the complete stock window menu
and workspace submenus. Menus use a 1 px border, 244–360 px width, 12 px horizontal /
6 px vertical item padding, and 1 px separators with 8 px horizontal / 5 px vertical
padding. Submenu offsets remain zero. Colors follow the semantic scheme and accent.
Labwc retains its panel, highlight and submenu shapes; these differ from QML menus.
The custom taskbar menu keeps its four supported actions: stock labwc's current
protocol cannot expose the complete native window menu through that custom menu.
No compositor patches, menu contents or shell bindings are changed.
