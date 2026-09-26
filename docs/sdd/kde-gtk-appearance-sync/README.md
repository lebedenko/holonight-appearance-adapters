# KDE and GTK appearance synchronization

The adapter reads canonical `appearance.toml` and owns native toolkit outputs. `apply` writes supported GSettings keys, GTK 3/4 settings, and, when KF6 ConfigCore and ConfigGui are available, five `kdeglobals` entries. ConfigGui supplies KConfig's typed `QFont` codec. The KDE color scheme key uses the canonical scheme ID, matching the installed `.colors` filename.

The state record retains original and last applied values for targeted rollback and conflict safe revert. Bare `status --json` checks the last applied outputs; `status --appearance PATH --json` also reports a conflict when the canonical value has changed. Missing KF6 support is reported as unavailable. KDE and GTK file updates preserve unrelated entries.

GTK synchronization covers dark preference, icon and cursor themes, and UI font. GSettings updates cover available interface keys. The adapter does not install a GTK widget theme or override application owned palettes.

Verification: run `ctest --test-dir build -R adapter_cli_tests --output-on-failure` with isolated XDG directories, the full adapter CTest suite, a KF6 disabled build, format check, and REUSE lint.
