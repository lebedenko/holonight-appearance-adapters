# Local CI rehearsal

Baseline: 4a226b1c3af836dc8a9bd452aace94417384b41e. Umbrella package CI-010.

Preserve the combined build-test/static job and separate licensing job with push/PR
triggers. Local and hosted call shared scripts in immutable build/REUSE 6.2.0
images. Preserve Config 5cd36ec9986801c4b831527a38e7b7414b9a1312 and Qt
fb0acc6520802d8f9c014bc9042b71b79d8c6d9a. Build providers without tests in
fresh trees; build all adapter/probe targets with BUILD_TESTING and BUILD_GTK_PROBES
on. Make Debug explicit, generate compile commands, run direct palette tests,
all CTest checks, format-check and existing full tidy. Require GTK/private D-Bus
checks to be available. Include CLI/package/GTK-major isolation acceptance.

Use the accepted checksum-pinned GTK/Xvfb runtime and rebase its pkg-config paths
inside the disposable prefix. Isolated bootstrap supplies Xvfb's fixed xkbcomp
path, then drops to host UID/GID before repository verification. Source mounts
remain read-only. Fresh snapshots include tracked edits/non-ignored new files,
preserve modes/symlinks and report new inputs to add before pushing.

Logs, source/dirty state, image/tool identities, results and test evidence are
host-owned under ignored build/ci. Failures/unavailable checks return nonzero.
No host service mutation, host installation, pushes or pin updates. Keep development
tasks. Real Podman integration is unavailable locally; launcher mapping is tested.
Acceptance: 2026-10-03. Both lanes passed in
`build/ci/20261003T165505Z-5atjx0r8/`: full Debug build, direct palette tests,
all 10 CTest registrations (including private GTK 3/4 D-Bus/Xvfb checks),
format-check, full tidy and REUSE 6.2.0. Four launcher regressions passed;
172 source/development files retained bytes, modes and modification times.
Logs and exported CTest evidence are owned by the invoking host user.

Host acceptance used freshly archived exact provider revisions, Release/Ninja
builds and recorded provenance under `build/ci/native-providers/`. The Debug
adapter build, all eight host-available CTest registrations, complete format-check
and `task tidy-src` with clang-tidy 23.1.1 passed. Host Xvfb is unavailable;
the two private GTK runtime registrations passed in the pinned container.
The Qt provider emits its expected GuiPrivate version-coupling notice; all
providers and consumers use the same Qt build within each environment.

Changes: `scripts/ci/` owns the launcher, runtime bootstrap, immutable image and
checksum-pinned supplemental package records, lane and regression checks;
Taskfile/README/workflows call and document it. Supplemental KConfig enables the
existing CLI acceptance assertions. GNOME desktop/system schemas are both pinned
and compiled strictly before a schema availability preflight. pkg-config metadata
is rebased without redirecting base-image GLib headers.

Required host diagnostic corrections update owned headers, source, probes and
tests to the checked-in formatting/static rules. Color parsing/encoding retains
canonical RGBA ordering, with boundary regressions. Apply/status/revert and labwc
transaction helpers retain output order, ownership checks, rollback/recovery and
protocol strings. GTK 4 probes select supported CSS loading APIs and narrowly
annotate required named-color compatibility APIs. CMake/tooling anchor tidy to
owned headers and included `.inc` fragments; host formatting includes `.inc`.
No static-check family is disabled. The rollback test avoids expected missing-file
grep noise. No API or production dependency requirement changes.

Failure evidence: missing supplemental GNOME enum definitions failed CLI acceptance;
strict schema compilation now catches that environment defect early. The full
formatter rejected an unformatted included fragment before its correction.
Launcher regressions cover deleted/edited/new/ignored/space-containing files,
executable bits, symlinks, unavailable runtime, every failed lane, continuation,
read-only input, account overlays and rootless Podman arguments. Real Podman
remains unavailable; no claim of real Podman execution.
