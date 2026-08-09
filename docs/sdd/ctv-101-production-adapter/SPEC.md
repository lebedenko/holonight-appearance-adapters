# CTV-101: Production Tier 1 appearance adapter

Status: Implemented

Baseline: `holonight-appearance-adapters@d977aaa`, `holonight-config@5cd36ec`, and `holonight-qt@e30ff79`.

## Contract

`holonight-appearance-adapter` loads the canonical TOML file with `HoloNight::Config`, resolves it with the installed
Qt appearance provider, serializes semantic appearance v1, and validates that wire representation with the neutral
consumer before any mutation. It provides `apply --appearance PATH --json`, `status --json`, `revert --json`, and
`query --appearance PATH --field cursor-theme`.

The JSON protocol is version 1. Mutating operations report `operation`, `result` (`success`, `degraded`, or `error`),
booleans `success` and `degraded`, and an `outputs` array. Each output has a stable name, status, apply mode, and a
redacted diagnostic. Invalid canonical input and writable-output failures are hard errors.

## Owned outputs and recovery

Available keys in `org.gnome.desktop.interface` are applied independently. Missing schemas and keys are unavailable
native fallbacks. The adapter atomically edits only `gtk-application-prefer-dark-theme`, `gtk-icon-theme-name`,
`gtk-cursor-theme-name`, and `gtk-font-name` in GTK 3 and GTK 4 `settings.ini`, preserving unrelated lines and file
permissions.

Original and last-applied values are stored below `$XDG_STATE_HOME/holonight/appearance-adapters.json` (or the XDG
state fallback). Apply captures the original once and is idempotent. An invocation that encounters a hard failure
restores mutations already completed by that invocation. Revert uses compare-and-swap semantics: it restores only
values still equal to the last HoloNight value and reports external drift as a conflict.

The Settings portal is delegated to Shell, XSettings is unavailable unless an existing owner exposes it, and
application-owned/libadwaita styling is reported rather than replaced. No `GTK_THEME` is exported and no XSettings
manager or GTK palette is installed.
