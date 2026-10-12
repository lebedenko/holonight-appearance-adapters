# External dependency management — holonight-appearance-adapters

Work package: EDM-007
Audit baseline: `95a9078e8d4f0b2d9c5a69ee6040388558eb9251`
Implementation baseline: `37508f71ed37a16b824329c303083a08cf88aa2f`
Umbrella audit: `38146d8f474317f3f8d72699f60e6d16a71dd165`
Status: Local acceptance complete; publication and umbrella pin/handoff are authorized.
Canonical handoff accepted in local umbrella checkpoint `80e17e4`.

## Requirements

- Configuration and normal build commands shall never acquire dependencies or prepare source providers.
- `task deps` shall explicitly prepare supplied local providers and check installed external prerequisites.
- Internal providers shall resolve only from approved prefixes; external packages shall resolve from an explicit
  external prefix or normal system locations.
- Enabled dependency-dependent features shall require their dependencies; minimal profiles shall disable them explicitly.
- Where production components use Qt, discovery shall require >=6.11; private Qt components shall match
  the selected public version exactly.
- Where tests use GTest, consumers shall require installed GTest >=1.14 CONFIG with both `GTest::gtest` and `GTest::gtest_main`; Adapters does not use GMock.
- Existing public APIs, component switches, install-tree consumers and acceptance coverage shall be preserved.

## Design

Adopt canonical `tooling/modules/holonight-appearance-adapters.json` and `.cmake`, then synchronize bundle 2.0.0 into this repository.
The copied metadata is owned by this standalone checkout. CMake performs authoritative discovery; shared probes
provide prerequisite diagnostics and record resolved versions/paths. Incompatible caches and local provider install
trees are archived before reuse. Package acquisition occurs only in explicit environment preparation. CI switches
build/runtime/release lanes together to qualified immutable images and runs acceptance offline after preparation.

Add BUILD_KCONFIG default ON and require both KF6::ConfigCore and KF6::ConfigGui when enabled. Disable GTK probe requirements in the production installation profile. Remove stale fallback provider directories.

## Implementation and verification tasks

- [x] Confirm the exact baseline and canonical handoff before implementation.
- [x] Adopt declarations, prefix helpers, explicit preparation and compatibility signatures.
- [x] Apply the repository-specific discovery changes above.
- [x] Update Task, CI preparation/entrypoints, Dockerfiles, release lanes and contributor instructions.
- [x] Run declaration/CMake/profile consistency checks and offline missing-package regressions.
- [x] Run narrow regressions, clean acceptance, staged installation and required quality checks.
- [x] Inspect complete logs; record commands, versions, features, warnings and deferred manual checks.
- [x] Make repository-local commits only after local verification; publication is explicitly authorized.

## Accepted inputs and implementation

Published providers are Config `0ccd25f323662f50a47bb048be636160f2b7c135`,
System Services `70614eb70329af7014853c2973e5624acf0b251e`, and
Qt `7c3ce074a8c2e02e4ee8cad8cd19eb8627aa4f68`.
The qualified EDM-024 build image is
`ghcr.io/lebedenko/holonight-deps@sha256:37ca051c39d1688d6413c47d94b8c0f221152dac6981ec67d155f2c43ea17b40`.
Licensing retains
`docker.io/fsfe/reuse@sha256:85462a75c0f8efda09ddd190b92816b70e7662577c8427429e11e1b9f25a992e`.

Canonical declarations now classify GTK3/GTK4 as probe-gated build prerequisites,
and Xvfb/private D-Bus as probe-gated test prerequisites. The unused Qt Test
requirement is removed. The regenerated production profile is byte-identical:
Adapters' default-disabled probes do not contribute GTK dependencies. Bundle 2.0.0
is copied without repository-specific bundle edits. A CI-owned clang-tidy wrapper
retains the previous owned `.inc` diagnostic coverage while canonical tooling
enumerates every owned translation unit; the included fragment is also formatted.

CMake requires production Qt >=6.11, installed GTest >=1.14 CONFIG and both used
targets, plus both KConfig targets when `BUILD_KCONFIG=ON` (default). OFF skips
KConfig discovery/linking and preserves KDE-unavailable behavior. `BUILD_GTK_PROBES`
and `BUILD_TESTING` default OFF; enabled probes require both GTK majors, and enabled
probe tests require both runners with no silent test omission. Internal package
paths come only from approved-prefix helpers; stale sibling fallback directories
are removed. No runtime implementation, C++23 contract, palette projection,
CLI/JSON format, ownership, recovery behavior or installed payload is changed.

`run.py --prepare` acquires immutable images and exports exact provider commits
from canonical clones or supplied local checkouts (working-tree edits are excluded).
Exports are compared before reuse and changed exports are archived. Providers are
installed exclusively through explicit `task deps PRESET=test`, with canonical
provider switches and Debug preparation. Consumer metadata/preset/image identities,
source content, toolchain, Qt/package exports, options and installed Arch package
inventory are recorded. Mismatched preparation is archived; failed preparation can
be retried and compatible providers reused. Each acceptance lane checks preparation
identities before compiling. Acceptance performs no acquisition or provider build;
all containers use `--pull=never --network=none`, read-only inputs, host UID/GID,
rootless Podman mapping and isolated runtime directories.

CLI and configuration-document tests receive normalized KConfig availability.
Full retains all KDE assertions; Minimal verifies unavailable reporting, byte-identical
existing KDE files and absence of newly created KDE files. Common GTK, labwc,
rollback/conflict, canonical-document mutation and symlink-retarget coverage remains.
Install tests retain staged uninstall checks and reject installed probe artifacts.

## Verification record (2026-10-13 local / 2026-10-12 UTC)

Focused commands:

```sh
python3 scripts/ci/test_launcher.py
python3 scripts/ci/test_dependencies.py
python3 ../tooling/sync.py --check --module .
python3 ../tooling/generate-install-profile.py --check
cmp ../tooling/modules/holonight-appearance-adapters.json tooling/module.json
cmp ../tooling/modules/holonight-appearance-adapters.cmake tooling/dependencies.cmake
python3 scripts/ci/run.py --prepare --config-source ../holonight-config \
  --system-services-source ../holonight-system-services --qt-source ../holonight-qt
python3 scripts/ci/run.py
```

Six launcher regressions cover source snapshots, exact committed exports and repair,
rootless offline arguments, missing runtime/preparation, incompatible manifest
rejection before container launch, and required lane failures. Sixteen dependency
regressions cover missing/old Qt/GTest, both GTest and KConfig targets, no GMock,
enabled GTK/runners, actual CMake failure for either missing runner, disabled-feature
isolation and probe-only configuration, prefixes with spaces, ambient-provider
rejection, changed package exports, compatibility archival/reuse and preservation of
stale caches by ordinary configure/build/test. Two installed-provider fixtures require
explicit preparation and run in acceptance; host-only runs report these skips.

Initial acceptance (`build/ci/20261012T220255Z-85jkrc3_`) passed Full 11/11 CTest,
Minimal 8/8 CTest, Release build/install and Licensing. Static discovered four
pre-existing raw-string-literal diagnostics in `tests/consumer_tests.cpp` and
`tests/test_data.h`; subsequent full enumeration also identified bounded indexing
and designated-initializer diagnostics in `tests/palette_tests.cpp`. These fixture-only
corrections preserve the generated bytes and values and are committed separately
from dependency adoption. The rollback fixture now guards its expected absent file
before grep, removing diagnostic noise without changing the assertion.
The obsolete QML preset variable was removed after complete-log review.

Preparation initially completed provider builds/install but package-inventory
recording failed because rootless account fixtures exclude Arch's `alpm` user.
The read-only `pacman --config /dev/null -Q` inventory command corrected this;
explicit preparation reused compatible providers successfully. A later preset
identity change archived the old preparation as `build/ci-prepared.previous-1`.
Provider preparation reports expected Qt private-module ABI notices; the same
immutable image/Qt build is required for acceptance. Optional Vulkan-header discovery
is informational. GTK under Xvfb reports unavailable DRI3 acceleration while the
probe exercises software rendering; no host display or desktop services are used.

Toolchain/package identity: GCC 16.2.1, Clang 22.1.8, CMake 4.4.3, Ninja 1.13.2,
Qt 6.11.2, GTest 1.18.0, KConfig 6.30.0, GTK3 3.24.52, GTK4 4.22.5,
JSON-GLib 1.10.8, GLib 2.88.3, D-Bus 1.16.2 and Xvfb 21.1.24.
Final clean acceptance passed. The last correction changed only fixture/static checks,
so the unaffected Release result was retained rather than rebuilt.

| Lane | Features / result | Complete local log |
| --- | --- | --- |
| Full | Debug; tests/KConfig/probes ON; 11/11 CTest, including isolated GTK majors and linkage | `build/ci/20261012T221651Z-3g3lkjxn/full.log` (399 lines) |
| Minimal | Debug; tests ON, KConfig/probes OFF; 8/8 CTest including unavailable KDE and staged install/uninstall | `build/ci/20261012T221651Z-3g3lkjxn/minimal.log` (335 lines) |
| Release | Production defaults; KConfig ON, tests/probes OFF; build and install pass | `build/ci/20261012T221251Z-ng_zahtb/release.log` (168 lines) |
| Static | Full features; canonical format and strict tidy across all 15 owned units plus included fragment; pass | `build/ci/20261012T221651Z-3g3lkjxn/static.log` (188 lines) |
| Licensing | Separate REUSE 6.2.0 image; pass | `build/ci/20261012T221651Z-3g3lkjxn/licensing.log` (27 lines) |

Every build lane also passed all six launcher and sixteen dependency regressions.
The final affected-lane command was
`python3 scripts/ci/run.py --lane static --lane full --lane minimal --lane licensing`.
Both Full and Minimal retain all 14 GTest cases across consumer, Qt bridge and palette
executables. Complete logs were reviewed: no remaining actionable diagnostics.
Expected provider ABI notices, optional Vulkan discovery and Xvfb DRI3 software-rendering
notices are described above. Canonical declaration equality, bundle drift, generated
profile drift, shell syntax, REUSE and final whitespace checks passed.

The fixture cleanup is commit `0b8efad`; the dependency adoption is committed separately.
The completed commits are to be sent in one publication push to canonical `origin/main`.
The exact published revision, latest available CI observation (queried once, without
waiting), and local-only umbrella pin/handoff are recorded in the umbrella EDM-007 ledger.
No umbrella publication is authorized by this handoff.

Debian qualification, manual ecosystem acceptance and final umbrella integration
remain deferred. EDM-007 completion does not mark the initiative Integrated.
