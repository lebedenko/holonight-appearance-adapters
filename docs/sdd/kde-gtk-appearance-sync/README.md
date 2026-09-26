# KDE and GTK appearance synchronization

The adapter reads canonical `appearance.toml` and owns native toolkit outputs. `apply` writes supported GSettings keys, GTK 3/4 settings, and, when KF6 ConfigCore and ConfigGui are available, five `kdeglobals` entries. ConfigGui supplies KConfig's typed `QFont` codec. The KDE color scheme key uses the canonical scheme ID, matching the installed `.colors` filename.

The state record retains original and last applied values for targeted rollback and conflict safe revert. Bare `status --json` checks the last applied outputs; `status --appearance PATH --json` also reports a conflict when the canonical value has changed. Missing KF6 support is reported as unavailable. KDE and GTK file updates preserve unrelated entries.

GTK synchronization covers dark preference, icon and cursor themes, and UI font. GSettings updates cover available interface keys. The adapter does not install a GTK widget theme or override application owned palettes.

## Implementation tasks and files

- [x] `src/adapter_main.cpp`: add targeted KDE reads and writes, state records, rollback, revert, and canonical status comparison.
- [x] `CMakeLists.txt`: make KF6 ConfigCore and ConfigGui optional; ConfigGui registers the typed font codec.
- [x] `tests/adapter_cli_tests.sh`: exercise dark and light projections, repeated apply, drift, rollback, and conflict safe revert in isolated XDG directories.
- [x] `README.md`: document KDE support and the optional status path.

Verification on 2026-09-26: full adapter CTest suite 6/6; a KF6 disabled build reports KDE unavailable; format check, tidy, and REUSE lint pass. The isolated fixture reads `HoloNight-Dark` from KDE, GTK 3/4, and GSettings.
