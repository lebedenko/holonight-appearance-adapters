# CA-004 implementation

- [ ] Inspect existing implementation and preserve public contracts.
- [ ] Implement repository-owned scope from README.
- [ ] Add focused behavioral regression coverage.
- [ ] Run focused checks, clean acceptance and installed consumer checks.
- [ ] Review final diff, commit, publish and hand off exact revision.

## CA-004 verification

- [x] Reproduce false successful application after an external edit.
- [x] Capture one resolved source snapshot and guard revision/physical target through application.
- [x] Verify sparse v2/default reset/invalid and unsupported documents without canonical mutation.
- [x] Verify pre/post state-publication external edits and identical-byte symlink retargeting; restore native outputs and state.
- [x] Run all 11 host tests including isolated GTK probes; source tidy/format and focused interposer analysis passed after the libc declaration alignment.
- [ ] Review clean acceptance, commit, publish and hand off.

2026-10-06 local: host `task test` passed 11/11 (12.93 seconds); focused adapter/labwc/document cases passed 3/3. Provider preparation uses published Config `7337816` / Qt `98803bc` and System Services `398804a`. Source static analysis passed; only the new test interposer needed libc parameter spelling to match existing declarations, and its final focused analysis passed. Logs `/tmp/holonight-adapters-config-{focused,tests,final-tidy}.log` and `/tmp/holonight-adapters-hook-tidy.log`.

2026-10-06 local: clean `task ci` passed all 11 registrations, complete source analysis, formatting and licensing (`build/ci/20261005T222540Z-shqz1k8r/`). All 950 log lines reviewed. Clean providers: Config `733781607124fc9bec0820c880e7467d08b34a50`, Qt `98803bca05e16ae0d0784a6cb43b0ace561385de`, System `398804a7cce5a57f9f6870c4e7ec99e9b1f3ddaa`. Subsequently installed Config `d6a392b41991f70a004d58f7694c7b6115cb7280`: all 11 registrations passed (14.60 seconds) using isolated private D-Bus/GTK environments. The rollback-only provider correction does not alter adapter reads.
