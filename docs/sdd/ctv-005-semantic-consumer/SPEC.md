# CTV-005: Semantic export consumer and GTK Tier 1 prototypes

Status: Implemented for discovery; all targets remain non-installed pending CTV-007.

Baselines: `holonight-appearance-adapters@656cfd1`, `holonight-config@5cd36ec`, and
`holonight-qt@fb0acc6`.

## Ownership and scope

This repository owns the semantic JSON encoding, toolkit-neutral validation, and candidate external-toolkit
projections. `holonight-qt` owns the resolved semantic values. Settings owns user apply/revert orchestration, and the
existing Shell Settings backend remains the portal owner. This prototype performs no filesystem, GSettings, portal,
XSettings, or environment mutation and defines no install rules.

GTK theme names are deliberately absent: semantic v1 exports no GTK theme identifier. Tier 2 palette work belongs to
CTV-006. Production persistence, atomic updates, diagnostics surfaces, and reload orchestration belong to CTV-101 and
later Settings/Shell packages after CTV-007 acceptance.

## Semantic JSON v1

The root is a JSON object. All fields below are required. Unknown fields are ignored in v1 so additive optional
extensions remain compatible. Removing, renaming, changing a field's meaning, or changing its type requires another
contract version.

- `contract_version`: integer `1`.
- `scheme_id`, `accent_id`: non-empty producer identifiers.
- `color_mode`: `dark` or `light`.
- Colors: `accent`, `accent_foreground`, `window_surface`, `view_surface`, `elevated_surface`, `raised_surface`,
  `hover_surface`, `disabled_surface`, `strong_selection`, `strong_selection_foreground`, `subtle_selection`,
  `subtle_selection_hover`, `subtle_selection_foreground`, `primary_text`, `secondary_text`, `disabled_text`,
  `inverse_text`, `passive_border`, `active_border`, `focus_border`, `destructive_border`, `success`, `warning`,
  `error`, and `error_foreground`.
- Each color is canonical lowercase sRGB `#rrggbbaa`; alpha is retained even when opaque.
- Typography: non-empty `ui_font_family` and `monospace_font_family`, with integer point sizes from 1 through 512.
- Identifiers: non-empty `icon_theme`, `fallback_icon_theme`, and `cursor_theme`.

The Qt bridge uses Qt JSON solely to serialize `Holonight::SemanticAppearance`. Insertion order is fixed for readable,
deterministic compact output, though consumers must not depend on object member order. The neutral parser uses
JSON-GLib and no Qt types. Parse failures return no partial snapshot and one or more diagnostics containing stable
`code`, field `path`, and a human-readable `message`. Codes are `malformed_json`, `missing_field`, `wrong_type`,
`invalid_value`, `invalid_color`, and `unsupported_version`.

## Tier 1 candidate projection

The projection is a pure value transformation. It proposes, but does not apply:

| Mechanism | Candidate values | Decision |
|---|---|---|
| GNOME GSettings | color scheme, icon/cursor themes, UI/monospace fonts, limited accent | Accept when schema and individual key exist; absence is a supported native fallback. |
| Settings portal | standardized color scheme and exact accent RGB | Existing Shell backend owns publication and change notifications; do not add a competing backend. Portal consumers are read-only. |
| XSettings | icon/cursor/font/dark preference for X11 and XWayland | Accept only through the session's existing settings manager; never compete for manager selection and never use as canonical storage. |
| `settings.ini` | per-major GTK 3 and GTK 4 icon/cursor/font/dark preference | Accept as relaunch fallback. Production must atomically preserve unrelated keys; GTK has no include mechanism for a HoloNight fragment. |
| Environment | `XCURSOR_THEME` only | Session-start fallback. Reject session-wide `GTK_THEME`; GTK documents environment controls as debugging facilities without stable end-user guarantees. |

GNOME accent mapping is `cyan→teal`, `blue→blue`, `violet→purple`, and `yellow→yellow`. Canonical `default` omits
the limited GSettings key, preserving the native fallback; the portal candidate always exposes exact semantic accent
RGB. No projection invents a GTK theme name.

## Probe evidence and applicability

GTK 3 and GTK 4 probes are separate binaries and therefore cannot accidentally link both majors. The isolated test
harness creates a temporary home and per-major `settings.ini`, selects the GSettings keyfile backend in a private
D-Bus session, and uses an isolated X server. Each probe observes startup settings and proves an in-process
`GtkSettings` property notification path by changing the dark preference. It never touches the user's home or live
display. CI installs Xvfb and runs both probes. Local runs without Xvfb report the matrix as unavailable instead of
falling back to the live desktop.

GSettings schema/key discovery must happen at apply time: distributions legitimately omit `accent-color` or even
the GNOME interface schema. Such absence is not a contract error and means omit that output. Keyfile isolation proves
the mechanism without writing dconf. XSettings supports small live X11 values and notifications, but only the owner
of the existing selection may publish them.

| Consumer | GSettings/portal | XSettings | `settings.ini` | Environment | Tier 1 result |
|---|---|---|---|---|---|
| GTK 3 Wayland | live where observed | unsupported | relaunch-required | session-start-only | live plus relaunch fallback |
| GTK 4 Wayland | live where observed | unsupported | relaunch-required | session-start-only | live plus relaunch fallback |
| GTK 3/4 XWayland | live where observed | live via existing manager | relaunch-required | session-start-only | prefer standards owner |
| libadwaita | portal color scheme is live; palette/accent may be application-owned | unsupported | limited/relaunch | session-start-only | application-owned beyond standardized hints |
| sandboxed apps | portal live and readable | unsupported | generally inaccessible | constrained/session-start | portal-first |
| Applications overriding toolkit settings | notification may be ignored | may be ignored | may be ignored | may be ignored | application-owned |

References: [XDG Settings portal](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Settings.html),
[GTK runtime guidance](https://docs.gtk.org/gtk4/running.html), and
[XSettings 0.5](https://specifications.freedesktop.org/xsettings/0.5/).

## Diagnostics, reload, and handoff

CTV-101 must retain validation before mutation, surface field diagnostics without logging full user payloads, discover
GSettings schemas and keys independently, and make repeated application idempotent. It must preserve unrelated
`settings.ini` keys with an atomic replace, coordinate XSettings with the existing manager, and report each output as
applied, unavailable, or application-owned. Live notifications are best effort; `settings.ini` requires relaunch and
environment changes require session restart.

CTV-006 may consume the neutral snapshot to evaluate GTK-major-specific Tier 2 CSS/palette mappings, but must not
reinterpret semantic roles or add fields to v1 without an optional-extension review. It must keep GTK 3 and GTK 4 in
separate targets and record an explicit accept/reject decision for each major.

## Verification criteria

- Exact deterministic Qt JSON, Unicode metadata, every role, every built-in scheme/accent, and Qt-to-JSON-GLib
  round trips pass.
- Missing fields, wrong types, malformed colors, invalid sizes/modes, unsupported versions, and unknown v1 extensions
  behave as specified.
- Projection mapping, native accent fallback, idempotence, and absence of mutations pass.
- GTK-major probes pass in isolated X/D-Bus/home environments where Xvfb is available.
- Format and clang-tidy checks pass, and no target has an install rule.
