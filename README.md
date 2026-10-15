# HoloNight Appearance Adapters

For current build, test, container acceptance and provider workflows, see [DEVELOPMENT.md](DEVELOPMENT.md).

Consumers and projections for HoloNight's semantic appearance contract. The installed
`holonight-appearance-adapter` applies accepted GTK Tier 1 outputs through GSettings and
owned GTK settings keys, plus KDE `kdeglobals` entries when KF6 Config is available.
The CTV-005 and CTV-006 palette/probe artifacts remain non-installed discovery evidence.

```sh
holonight-appearance-adapter apply --appearance ~/.config/holonight/appearance.toml --json
holonight-appearance-adapter status --json
holonight-appearance-adapter status --appearance ~/.config/holonight/appearance.toml --json
holonight-appearance-adapter revert --json
holonight-appearance-adapter query --appearance ~/.config/holonight/appearance.toml --field cursor-theme
```

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build/dev
ctest --test-dir build/dev --output-on-failure
```

See [the CTV-005 SDD](docs/sdd/ctv-005-semantic-consumer/SPEC.md) for the
contract, evidence, applicability matrix, and handoffs.

See [the CTV-006 SDD](docs/sdd/ctv-006-gtk-palette/SPEC.md) for the independent
GTK-major mappings, compatibility evidence, limitations, and recommendations.

See [the CTV-101 SDD](docs/sdd/ctv-101-production-adapter/SPEC.md) for the production
CLI protocol, output ownership, recovery, and fallback contract.

The opt-in [labwc Accent theme](docs/labwc-theme.md) synchronizes colors and UI
fonts for window titles when `HoloNight` is selected in `rc.xml`. Installation includes a generated
default dark fallback; see the guide for XML enablement, override conflicts,
custom configuration directories, recovery, and visual smoke tests.

## Dependency preparation and local CI

Production requires Qt >=6.11, JSON-GLib, GIO and KConfig. `BUILD_KCONFIG=OFF`
preserves KDE-unavailable reporting without discovering or linking KConfig.
`BUILD_GTK_PROBES` defaults OFF; enabling it requires GTK3 and GTK4 even without
tests. Tests require installed GTest >=1.14 CONFIG with `gtest` and `gtest_main`,
without GMock. Probe tests additionally require `xvfb-run` and `dbus-run-session`.

See [DEVELOPMENT.md](DEVELOPMENT.md) for current native and container commands.

Five clean lanes cover Full (Debug tests, KConfig and probes ON), Minimal (tests ON,
KConfig/probes OFF), Release (production defaults and install), Static (full-feature
compilation database, canonical formatting and strict tidy across all owned units),
and Licensing (the separate REUSE 6.2.0 image). Select lanes with `--lane full` etc.

See [DEVELOPMENT.md](DEVELOPMENT.md) for current native and container commands.
