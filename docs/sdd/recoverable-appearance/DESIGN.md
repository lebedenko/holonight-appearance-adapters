# Recoverable appearance adapter

[Coordinated contract](../../../../docs/initiatives/recoverable-appearance/DESIGN.md).

Preserve skips owned fields whose current value differs from the last write.
Labwc fonts are independent of the generated asset group. Replace updates controlled
values, with current manual values becoming their restore baselines. Ordinary
successive saves retain the baseline. Null legacy ownership is pruned. Restore
releases ownership of later customizations and restores unchanged managed values.

Before replacement, a unique `appearance-backup-*` directory alongside adapter
state stores file before-images, GSettings values and a manifest mapping absolute
paths to numbered images or null for absence. It includes ownership state. Backup
failure prevents writes. These backups persist after success and rollback.

Generated labwc assets retain same-filesystem directory staging and recovery.
Fonts and the tracked override block join the existing journal and ownership commit.
Rollback failure retains the journal for recovery. Extra theme files and unrelated
XML survive; another selected theme is never changed. Unsafe targets and malformed
XML remain errors.

Concrete theme assignments are appended in a marked block after user override
rules. Existing rules remain, including wildcards for uncontrolled properties.
Restore removes unchanged managed assignments and retains later edits. In labwc
0.20.2, `theme_init` reads the theme then `themerc-override`; `theme_read` processes
assignments in file order. See [upstream source](https://github.com/labwc/labwc/blob/0.20.2/src/theme.c).
The isolated visual smoke includes conflicting exact/wildcard title colors and
asserts rendered titlebar colors before and after live reload.
