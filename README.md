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

The opt-in [labwc Accent theme](docs/labwc-theme.md) synchronizes colors and UI
fonts for window titles when `HoloNight` is selected in `rc.xml`. Installation includes a generated
default dark fallback; see the guide for XML enablement, override conflicts,
custom configuration directories, recovery, and visual smoke tests.

## Standalone developer tooling

See [tooling/README.md](tooling/README.md) for presets, local dependency overrides, editor refresh,
`task tooling:doctor`, and the independent Serena project.


## Dependency preparation and local CI

Production requires Qt >=6.11, JSON-GLib, GIO and KConfig. `BUILD_KCONFIG=OFF`
preserves KDE-unavailable reporting without discovering or linking KConfig.
`BUILD_GTK_PROBES` defaults OFF; enabling it requires GTK3 and GTK4 even without
tests. Tests require installed GTest >=1.14 CONFIG with `gtest` and `gtest_main`,
without GMock. Probe tests additionally require `xvfb-run` and `dbus-run-session`.

Supply local provider checkouts through `HOLONIGHT_CONFIG_SOURCE`,
`HOLONIGHT_SYSTEM_SERVICES_SOURCE` and `HOLONIGHT_QT_SOURCE`, then run
`task deps PRESET=test` explicitly. Ordinary `task configure`, `task build` and
`task test` validate installed prerequisites and never prepare providers. Internal
packages resolve from `HOLONIGHT_DEPENDENCY_PREFIX` or `build/deps/prefix`;
`HOLONIGHT_EXTERNAL_PREFIX` supplies an optional separate external package prefix.
Incompatible dependency caches are archived by explicit preparation.

Run `task ci:prepare` once with network access and an accessible Docker daemon
(Podman is used when Docker is absent). It pulls immutable linux/amd64 images,
acquires exact published provider revisions, and installs providers through
`task deps` with canonical options and Debug builds. To supply existing checkouts:

```sh
python3 scripts/ci/run.py --prepare --config-source ../holonight-config \
  --system-services-source ../holonight-system-services --qt-source ../holonight-qt
```

Then run `task ci`. All acceptance containers use `--pull=never --network=none`,
read-only prepared inputs, host UID/GID and isolated runtime directories. Missing
or incompatible preparation fails; acceptance performs no package acquisition or
provider builds. Archive `build/ci-prepared` before changing preparation identities.

Five clean lanes cover Full (Debug tests, KConfig and probes ON), Minimal (tests ON,
KConfig/probes OFF), Release (production defaults and install), Static (full-feature
compilation database, canonical formatting and strict tidy across all owned units),
and Licensing (the separate REUSE 6.2.0 image). Select lanes with `--lane full` etc.

Tracked edits and non-ignored new inputs enter read-only snapshots; add reported
new files before pushing. Disposable source/build trees leave development builds
untouched. Complete logs, revision/dirty state and lane results are host-owned under
ignored `build/ci/`; preparation evidence is under `build/ci-prepared/`. Run focused
regressions with `python3 scripts/ci/test_launcher.py` and
`python3 scripts/ci/test_dependencies.py`. GTK probes remain non-installed.
