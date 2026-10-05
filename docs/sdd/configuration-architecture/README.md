# Configuration architecture: holonight-appearance-adapters

Work package: CA-004. Upstream baseline: `7c8bcb89ae90ac1db818eb4aa786ce356ff333f2`.

## Scope

v1/v2 compatibility and isolated application.

Follow the accepted [shared contract](../../../../docs/initiatives/configuration-architecture/README.md).
Keep this repository independently buildable; do not modify another repository in its implementation commit.

## Design and acceptance

Each application owns its configuration schema, file, settings UI and behavior. Global appearance is shared;
application preferences are separate. Files and Viewer must remain usable without Shell or Settings. AI and Packages
retain their own configuration. Infrastructure requires neither Shell, Settings nor a running daemon.

Snapshots retain original bytes, parsed typed values, override presence, source spans and content revisions.
Paths are vectors of key segments, including quoted keys containing dots. Schemas declare typed defaults,
constraints, descriptions and reload policy, with domain validators for related values and dynamic collections.
Edit batches carry set/remove operations and baseline values/presence. Save outcomes distinguish success,
per-key conflicts, invalid documents/edits, unsupported patches, pre-replacement storage failures and
post-replacement durability failures.

Use toml++ and a TOML-aware lexical editor; never serialize an existing document wholesale or substitute by regex.
Preserve unrelated bytes, comments, ordering, whitespace and unknown fields. Reset removes an assignment and retains
comments/sections. Arrays and arrays of tables are whole conflict values. Unsupported safe patches fail unchanged.
Reparse and validate every candidate. Establish preservation fixtures before consumer adoption.

Merge baseline, pending and current values: unrelated edits merge, identical edits converge, different changes to the
same value conflict. Lock a stable sibling file for cooperating writers; read/patch under the lock and recheck the
revision immediately before replacement. Arbitrary editors do not participate in the lock: a race remains between
the final check and rename. Follow existing symlinks, abort on retargeting, preserve existing permissions, create new
files as 0600, sync a same-directory temporary file, rename, and sync the directory.

Appearance v2 uses sparse defaults, rejects invalid known fields and warns about preserved unknown fields. Retain the
v1 decoder and explicit v1 serialization APIs. Document version is metadata, separate from the effective appearance
model. First successful Settings save upgrades valid v1 by changing only version and requested values. New editing
documents use v2; unsupported versions are read-only. Enable GUI v2 writes only after readers/adapters pass compatibility.

Shell owns defaults/validation in its exported configuration package. Preserve paths and meanings. Reads never create
files or write defaults. Missing overrides use defaults; reset removes the override. Reject invalid known values.

Settings retains Save/Discard, tracks baseline/pending edits, refreshes untouched controls on external changes and
retains pending edits. Expose baseline/disk/pending values and per-value keep-pending/accept-external resolution;
recheck on save. Show default/override status and diagnostics. Discard loads latest disk. Invalid external documents
block saves and running consumers retain last valid values; startup errors use defaults with diagnostics. Missing
files use defaults without writes. Watch files and nearest existing parents through replacement/deletion/recreation;
publish only differing effective values. Domain saves have independent outcomes. Rollback is conditional on the staged
revision still being current; concurrent changes survive and must not be reported successfully applied.

- [ ] Preservation fixtures cover comments, unknown fields, quoted/dotted keys, inline tables, multiline strings, Unicode, CRLF, arrays/AoT, insertion/reset and rejected patches.
- [ ] Merge, convergence, conflicts/reset, cooperating locks and revision-change aborts pass.
- [ ] Unreadable files, permissions, interrupted writes, replacement failures, symlink retargeting and durability outcomes pass.
- [ ] v1 behavior remains compatible; sparse v2/reset and surgical first-save upgrades pass; unsupported versions cannot be overwritten.
- [ ] Runtime invalid/startup/missing/delete/recreate and unchanged-signal scenarios pass.
- [ ] Settings Save/Discard, external updates, per-value resolution, partial saves and adapter/rollback concurrency pass.
- [ ] Each repository passes required clean acceptance and installed-package checks at accepted provider revisions.
- [ ] Files and Viewer pass standalone checks without Shell or Settings installed.
- [ ] Every participating submodule is clean and pinned to a canonical published implementation commit.
- [ ] Dependency-order integration and user-operated concurrent-edit/appearance checks are recorded with dates and revisions.

## Accepted provider revisions

Config `733781607124fc9bec0820c880e7467d08b34a50`; Qt `98803bca05e16ae0d0784a6cb43b0ace561385de`; existing Qt compositor provider System Services `398804a7cce5a57f9f6870c4e7ec99e9b1f3ddaa`. Adapters do not consume Shell configuration. Capture the source document and physical target when loading apply input; verify both before state publication and after application. A changed input must return failure and preserve the canonical file. Isolated fixtures exercise v1/v2 projections and controlled source mutation before and after adapter state publication.

The apply transaction captures one resolved appearance for GTK/KDE/labwc. Canonical target and exact content revision are checked during loading, before state publication, after publication and after reload. A changed source triggers owned-output/state recovery and an error; canonical content is never reverted by adapters. A final-check race with nonparticipating editors remains. Fixtures reproduced false success on the original adapter and now verify recovery across GTK, KDE, labwc assets/XML and prior state.
