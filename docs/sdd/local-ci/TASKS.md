# Local CI tasks

- [x] Inspect baseline, instructions, existing workflow and provider/GTK contracts.
- [x] Implement shared launcher/lane/task/workflows and pinned tool environment.
- [x] Resolve actionable diagnostics without changing adapter contracts.
- [x] Run regressions, clean acceptance and source/build isolation; inspect complete logs.
- [x] Record acceptance, review final diff and commit local handoff.

2026-10-04 publication merge: incoming `953397e` retained labwc UI-font
synchronization, XML preservation and updated CLI regressions. Resolved the overlap
with local brace/style corrections without reverting behavior. Clean `task ci`
passes both lanes in `build/ci/20261004T171843Z-cmn_k20z/`; host
`cmake --build build/test`, CTest and the full tidy target pass. Publication and
umbrella pinning are authorized by the user.
