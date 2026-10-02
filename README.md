# HoloNight Appearance Adapters

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
cmake --build build/debug
ctest --test-dir build/test --output-on-failure
```

See [the CTV-005 SDD](docs/sdd/ctv-005-semantic-consumer/SPEC.md) for the
contract, evidence, applicability matrix, and handoffs.

See [the CTV-006 SDD](docs/sdd/ctv-006-gtk-palette/SPEC.md) for the independent
GTK-major mappings, compatibility evidence, limitations, and recommendations.

See [the CTV-101 SDD](docs/sdd/ctv-101-production-adapter/SPEC.md) for the production
CLI protocol, output ownership, recovery, and fallback contract.

The opt-in [labwc Accent theme](docs/labwc-theme.md) synchronizes colors and title
fonts when `HoloNight` is selected in `rc.xml`. Installation includes a generated
default dark fallback; see the guide for XML enablement, override conflicts,
custom configuration directories, recovery, and visual smoke tests.

## Standalone developer tooling

See [tooling/README.md](tooling/README.md) for presets, local dependency overrides, editor refresh,
`task tooling:doctor`, and the independent Serena project.
