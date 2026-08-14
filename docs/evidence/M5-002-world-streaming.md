# M5-002 world-streaming RPF evidence

M5-002 is closed for the pinned post-OOBE saved route. The native host resolves
the cache archive through lowercase, uppercase, and deliberately mixed-case
`game:`/`D:` paths to the same VFS entry before guest launch. One isolated
saved route then reaches title, gameplay, pause, and Options and exits through
external `WM_CLOSE`; the title has no internal Exit command and none is
claimed.

The accepted private result is
`private/evidence/M5-002/20260814-093131-ddca5b9d/result.json`, SHA-256
`A582A41BB254D9187BC6F8147E89E2FF2E92D0EA4B79945760BA6FBCF334BC28`.
Raw noisy logs, frames, shaders, saves, and paths remain ignored below
`private/`.

The seven-file private runtime log set is 50,672,730 bytes with manifest hash
`231E784CDA31717D7EA2A0E56104BFE2EBDF56366C611A04DA28F2800DE3F100`.
After the guest module launch it records exactly two successful opens of each
archive and no archive open/read failures:

- `xarchive_cache.rpf`: 8,011 successful reads, 262,821,908 bytes, highest end
  2,130,116,608 of 2,130,739,200 bytes (999,707 ppm);
- `xarchive_audlo.rpf`: 41 successful reads, 3,290,825 bytes, highest end
  1,461,945,469 of 1,463,189,504 bytes (999,149 ppm).

The trace contains exactly seven known retail-safe development-path misses:
five `test_dt_railyard.loc` and two `test_sc_exposition_park.loc` requests
under the absent `t:\mc4\art\city` device, all with the expected status. There
are no additional guest file-open failures, fatal markers, unsupported PPC
markers, guest crashes, or D3D12 device-loss markers.

The result rehashes the pinned save tree, complete source-game manifest/tree,
four runtime artifacts, clean-build and focused-test logs, all rotated runtime
logs, and all four frontend BMPs. Focused SDK VFS tests pass 33 assertions in
2 cases; the world-streaming verifier fixtures pass one positive and 30
fail-closed negatives.

This proves archive-backed streaming and case-insensitive path resolution for
the canonical saved free-roam route. It does not prove every city region,
first-run OOBE/cutscene streaming, race streaming, RPF format completeness, or
long-session stability; those remain M5-003/M5-010 and M6-001 scope.
