# M11 l106 (2026-11-15): the game's MSAA off (default-off arm), dev-override file 8 KiB, SDK Manifest test race

**Headline.** The game decides its scene MSAA once, when it creates its renderer (`sub_822E4AB0`): the
mode word renderer+384 is 2 (2x) on an HD output, 4 on an SD output, 1 when one of two command-line
parameters is set. l106 adds the owner key `guest_msaa_off` (host init-only bool
`mcla_guest_msaa_off`, default off): a function-level override of `sub_822E4AB0` runs the original
and then stores 1 at +384, the value the game's own no-MSAA branch stores, before any render target
exists. No guest code was regenerated, no mid-function patch. Always-on `MCLA_GMSAA` lines (renderer
creation and the first render-target set-up) and an F10 block `guest_msaa`. Also: the dev-override
file limit `kMaxBytes` 4 KiB -> 8 KiB (host and launcher), and the SDK Manifest unit tests no longer
share one temp folder (SDK commit `6a4b82e`, test only). Built and unit-tested only; not signed, not
run, no physical PASS. Spark not used (deterministic static reading of our generated code, source
edits and builds; no bounded extraction or triage task suited it).

**Licence statement.** The research note (`M11-aa-and-scaling-research-20261115.md`, lever 4) names
the guest address 0x822E4B80 and the field renderer+384 from GPL-3.0 projects; only those facts were
used. No third-party code was opened, copied or paraphrased. Every statement below comes from our own
recompiled guest code (`generated/default`, read only) and our own analysis; the host code is
original.

## 1. How the game selects its MSAA mode (static analysis of our generated code)

| Function (TU) | Role | What it does with the mode |
|---|---|---|
| `sub_822E47E0` (15) | renderer constructor | `li r11,2` / `stw r11,384(r31)`: default 2. The parameter words +4 of 0x8287E0BC (`-8004`) and 0x8287E094 (`-8044`), when non-zero, store 4 and the size 640x480 / 960x720 (+416 / +420; otherwise the display size from 0x827D4274 / 0x827D4278). |
| `sub_822E4AB0` (15) | renderer creation | allocates 464 bytes (`li r3,464`, `sub_82130528`), constructs, publishes the pointer at **0x8287E064** (`lis r11,-32120` / `stw r3,-8092(r11)`). Then: if `sub_82178880` (XGetVideoMode `0x827BCD84`, returns fIsHiDef, byte +16) is false and the display is at most 720x576: +384 = 4. At `loc_822E4B78`: `lis r11,-32120`, `addi r10,r11,-7944`, **0x822E4B80** `lwz r11,4(r10)`; if that word or the word +4 of 0x8287E10C (`-7924`) is non-zero, `loc_822E4BA0` stores +384 = 1. From `loc_822E4BAC` to the two returns nothing reads +384. |
| `sub_827ACBB0`, `sub_827ACBD0` (61) | static registration of the two parameters at 0x8287E0F8 / 0x8287E10C | through `sub_821C06C8` with value 0 (the pattern of the l84 parameter 0x8288E8F4): only a command line could set them. |
| `sub_823041E8` (16) | graphics start-up, the one direct caller | `bl 0x822e4ab0`, return **0x823042BC**; it is called by `sub_82304708`, which creates the object at 0x8288B9AC first. |
| `sub_822E4CC8` (15) | render-target set-up, one direct caller `sub_82305230` (`lwz r3,-8092(r31)`, return **0x82305250**), whose caller `sub_822C79A0` passes the 0x8288B9AC object | `lwz r9,384(r31)` / `stw r9,100(r1)`: the mode is the multisample word of the target descriptor; +396 and +400 (and +404 when a size parameter is set) are created at +416 x +420 through the factory call (vtable +56) with it; the display-sized +412 gets 0 (`stw r28,100(r1)`). |
| `sub_82310E40` (16) | start-up targets | passes +384 (r6) and +416 / +420 to `sub_825F4750` (the depth / shadow-collector sizes). |
| `sub_822E5ED8` (15), per frame | scene begin | `lwz r8,384(r31)`, `mr r6,r8`, 3 -> 4, then `bl 0x8217a470` (BeginTiledRendering keeps r6 in r18); renderer bytes +393 / +373 / +432 / +433 can force 4 independently. Skipped when +384 <= 0. |
| `sub_822E5218`, `sub_823115E8` | per frame | test +384 > 0 (end tiled rendering `sub_8217B430` / a tiled branch). |
| `sub_822E55D0` | per frame | render state 15 (`sub_8217F768(15, r4)`) on only when +384 > 1 (likely the MSAA-dependent state such as alpha-to-mask; inference). |

- **Values:** 1 = one sample, 2 = 2x, 4 = 4x (sample counts; 3 is treated as 4 by the scene begin).
- **When:** once, in `sub_822E4AB0`. The renderer's unit stores +384 only in the constructor (3
  stores) and the creation (2 stores); no other function that loads 0x8287E064 writes it (scan of all
  65 units: `sub_822E4AB0`, `sub_822E6408`, `sub_82310E40`, `sub_823115E8` read; `sub_8233E790`'s
  +384 is another object). The scene targets, the start-up depth / shadow targets and every
  frame's tiling all read the same live value, so it must not change after the targets exist.
- **Not covered by +384:** the shadow map's 4x depth-only pass (`sub_825F48E8` calls BeginTiledRendering
  with `li r6,4`), the game's 4x clear rectangles and the small 2x / 4x reflection targets of the
  research note; the front-buffer-sized +412 target is 1x already.
- **Correction to the research note:** 0x822E4B80 is the load of the first parameter word
  (`lwz r11,4(r10)`), not the mode itself; forcing it non-zero takes the game's own branch that
  stores 1. The mode field is confirmed as renderer+384 with the renderer pointer at 0x8287E064.

## 2. The mechanism (T2, shipped)

- The host already overrides recompiled functions by strong symbol (`DEFINE_REX_FUNC` defines
  `sub_X` as a weak alias of `__imp__sub_X`; `REX_EXTERN(sub_X)` in the host replaces it, as
  `mcla_tod.cpp` does for `sub_822F1DB0`). Both callers are in another unit than the definitions,
  and the function table registers the symbols (`SetFunction(0x822E4AB0, sub_822E4AB0)`).
- `REX_EXTERN(sub_822E4AB0)` (`src/mcla_guest_msaa.cpp`): runs `__imp__sub_822E4AB0`, then reads the
  pointer at 0x8287E064 and +384. With `mcla_guest_msaa_off` on, a plausible pointer (non-null,
  aligned, room for 464 bytes) and a mode the game itself stores (2 or 4), it stores 1. Mode 1
  already (the game's own parameters), an implausible pointer or any other value: nothing is
  written (`outcome` already_1x / no_renderer / unknown_mode). This is the same state the game's own
  branch at `loc_822E4BA0` produces; the code between that branch and the return does not read +384,
  so writing after the return is equivalent, and no consumer has run yet.
- `REX_EXTERN(sub_822E4CC8)`: read-only; at the first call it logs the mode the scene targets are
  about to be created with, then runs the original.
- The cvar is applied by the dev-override module at startup, before the guest starts (Phase::kStartup),
  so it is in place long before the guest's graphics start-up. The order "creation before target
  set-up" is a static inference (above); the `rt_create` line proves it at run time.
- **Lines** (always on, one each; the first part is the agreed format, the fields after `where=` are
  extra):
  `MCLA_GMSAA v=1 requested=1 applied=1 mode_before=2 mode_after=1 where=renderer_create outcome=applied renderer=0x........ size=1280x720`
  and `MCLA_GMSAA v=1 requested=1 applied=1 mode_before=1 mode_after=1 where=rt_create renderer=0x........`.
  With the key off: `requested=0 applied=0 mode_before=2 mode_after=2` and `outcome=not_requested`.
  At `rt_create`, applied=1 means the creation hook wrote 1 and the set-up still sees 1 on the same
  renderer.
- **F10:** `"guest_msaa": {"version": 1, "requested", "created", "outcome", "applied", "renderer",
  "mode_before", "mode_after", "size", "rt_create_calls", "rt_create_mode", "mode_now",
  "mode_now_name"}` (`mode_now` is a fresh read of +384 when the report is written).
- Key: `guest_msaa_off` -> `mcla_guest_msaa_off` = `true`, Phase::kStartup, `=false` writes nothing, no
  conflict pair; the cvar is in `kNeverWrittenCvars` (104 -> 105).

## 3. Expected visual effects and risks

- **Aliasing:** edges of cars, buildings, power lines and thin poles step and crawl, more visibly at
  1920x1080 stretched to 4K. FXAA (`pc/msaa-off-fxaa`) or CAS can soften it.
- **Foliage and fences:** with the mode at 1 the game itself turns render state 15 off
  (`sub_822E55D0`); if that state is alpha-to-mask (inference), alpha-tested foliage and fences lose
  their coverage gradient and may dither, shimmer or blink (the KI-015 family; the research note
  reports the same with another project's switch).
- **Effects that relied on the MSAA resolve** (soft edges after the resolve, depth-based effects
  that read the resolved depth) may look harder; nothing is expected to go missing, because the game
  has its own 1x path (the parameters) and its SD path picks 4x by the same mechanism.
- **Our backend:** the guest now lays out the scene at 1x itself (EDRAM pitch and tile counts halve:
  960x540 at 1x is 408 tiles per 32-bpp surface instead of 816); the SDK render-target cache keys
  targets by sample count and handles 1x generically (`MsaaSamples::k1X` paths in
  `d3d12/render_target_cache.cpp`, used today by the 1x passes such as the final 8_8_8_8 target).
  Ownership transfers between the 1x scene and the 4x clears / shadow pass go through the same
  mixed-sample-count transfer code as the 2x scene does today. Not run: the first owner run is the
  test (`MCLA_GPU_SURF` and the `draws=1x/2x/4x` counters of `MCLA_GPU_CAT` should show the scene
  draws moving from 2x to 1x).
- **Do not switch during play:** the mode is read every frame by the tiling and once by the target
  creation; the arm writes only at creation. The renderer flags that force 4x in the tiling
  (`+393`, `+373` / `+432` / `+433`) are untouched, as with the stock 2x mode.
- **Expected gain:** an estimate only (research lever 4: -0.5 to -2 ms GPU at 1080p); Run 5 measures
  it against `pc/gpu-cat` on the same route.
- With the key off, the only effects are two log lines and one F10 block (no guest write).

## 4. Dev-override file limit (T3)

`kMaxBytes` 4096 -> **8192** (`src/mcla_dev_overrides.h`). Readers: the host
(`mcla_dev_overrides.cpp` `ReadOverrideFile`: a stack buffer of kMaxBytes + 1, the size check and
`Parse`) and the launcher (`mcla_launcher_shader_seed_win.cpp` `StoreSeedEnabled` reads the same file
with the shader-store file bound of 16 MiB and parses it with the same `Parse`, so the limit follows
kMaxBytes; `prewarm_seed_store` is read there). `mcla_readback_sync.cpp`'s own single-line reader
(`kOverrideMaxBytes`) is used only in builds without MCLA_DEV_OVERRIDES and is unchanged. The full
147-key CRLF arm is 4063 bytes, so about 4 KB of room remains. Tests pinning the bound were updated
(see 6).

## 5. SDK (T4)

- Branch `mcla/m11-l106` from `13ef300`, worktree `private\l106-sdk-wt`, build `private\l106-sdk-b` /
  `private\l106-sdk-o` (the l105 options, `REXGLUE_ENABLE_FFX_PRESENTER_SHADERS=ON`). Local commit
  **`6a4b82efbcd951a2be4a8c6115655e95ae3a7b9a`** "l106 (test only): Manifest tests use their own temp
  directory" (one file, `tests/unit/codegen/manifest_test.cpp`, blob `132061afd98bd2f620b9f05d6d96c51c1ca90d2a`,
  sha256 `222DEA5B6AF2B6F56FC64741FD9B83E44B264C6514E353D25C731CA572552FE4`; +30 / -4). Not pushed,
  no tag.
- Fix: each `TempDir` reserves `%TEMP%\manifest_test-<random>-<random>-<sequence>` with
  `create_directory` (fails when the name exists; 128 attempts, then FAIL) and its destructor removes
  only that directory (error code, no throw). The cases write plain files only.
- Full parallel ctest (`-C Release -j 8`, no exclusions): **2327 / 2327 passed three runs in a row**
  (28.2 s, 31.0 s, 30.9 s). Warnings: normalized set 212, **0 new / 0 gone** against l105.
  `git-clang-format` against 13ef300: no change. No `manifest_test*` folder left in Temp.
- **No SDK install for l106:** T2 needs nothing from the SDK and the commit changes a test only, so the
  l106 host is built against `private\l105-sdk-install` (13ef300) and stages the l105 SDK DLLs
  (identical hashes).

## 6. Tests

- New `tests/mcla_guest_msaa_test.cpp` (new target `mcla_guest_msaa_test`, appended to
  `private\l39-work\l41_targets.txt`): layout and addresses, mode names and tiling map, the decision
  (off never writes; 2 and 4 write 1; 1, no renderer, unknown values write nothing), both line
  formats and the F10 object; against the generated code: the constructor's +384 stores, the
  creation's SD and parameter branches and that nothing after `loc_822E4BAC` reads +384, exactly 5
  +384 stores in unit 15, XGetVideoMode / fIsHiDef, the one caller of the creation (return
  0x823042BC), the parameter registrations, the set-up descriptor and its one caller (return
  0x82305250), the per-frame scene begin -> BeginTiledRendering, the start-up target sizes and the
  symbol registrations. **49 checks.**
- `tests/mcla_dev_overrides_test.cpp`: 147 names; an l106 block (phase, cvar, text, last key, no
  conflict pair, no name variants, the arm lines parse, combinations with FXAA and gpu_cat, near
  misses); the mapped-key list; the all-keys `=false` arm (147 keys); the limit 8192, exactly 8192
  accepted, 8193 rejected, 4097 accepted; the full CRLF arm padded with comments to 8190 bytes.
  4214 -> **4267 checks**.
- `tests/mcla_m10_defaults_test.cpp`: `TestL106Keys` (the key writes a never-written cvar, wins over
  no default, the l106 arms leave every default alone on both platforms); 29 never-written counts
  104 -> 105. 11533 -> **11795 checks**.
- `tests/mcla_launcher_shader_seed_win_test.cpp`: the launcher limit (8192 read, 8193 rejected, through
  `Parse` and through the launcher's own file path for 4096 / 4097 / 8192 / 8193 bytes). 50 -> **52
  checks**.

## 7. Gates

- Main checkout (gate SDK `private/l67-sdk-install`): `private/l57-work/gate_host_build.ps1 build`
  rc 0, **0 warnings**; `private/l57-work/l57_host_tests.ps1`: `count 58` (57 + the new suite),
  **`failed executables: 0`**.
- `ast-grep scan -c sgconfig.yml` on the 12 changed and new files: rc 0, no findings. `git diff
  --check`: clean (the three new files checked separately: no trailing whitespace, no tabs).
- l106 host (`private/l106-host-build` from the scratch tree `l106o-host-src`, against
  `private/l105-sdk-install`; configure + 289 build steps): rc 0, **0 warnings**; its 58 test
  executables: `failed executables: 0` (4267, 49, 52 and 11795 checks).
- Temp test leftovers (`MclaStagingTest-*`, the other test prefixes and `manifest_test*`): count 1
  before, 1 after (the same pre-existing directory).
- Binary scan of `private/l106-int-input` (the l105 scan with 147 keys and the l106 strings): no
  personal path (only the known benign hits: `AppData` in help texts, `private/save-arc` in messages,
  random byte matches), `string check failures: 0`, `l104 ... 0`, `l105 ... 0`, `l106 string check
  failures: 0`.

## 8. Changed tracked files (working tree; the primary commits exactly these plus this document)

Diff stat of the tracked files: `CMakeLists.txt` 7, `src/mcla_app.cpp` 3, `src/mcla_dev_overrides.h`
31, `src/mcla_diagnostics.cpp` 3, `src/mcla_diagnostics.h` 2, `src/mcla_m10_defaults.h` 5,
`tests/mcla_dev_overrides_test.cpp` 91, `tests/mcla_launcher_shader_seed_win_test.cpp` 18,
`tests/mcla_m10_defaults_test.cpp` 97; 9 files, 194 insertions, 63 deletions; new: three files
(218 + 141 + 304 lines).

| file | sha256 (working-tree bytes) | git blob |
|---|---|---|
| CMakeLists.txt | 1BBDE2A481D7DD0930228633E3B9C49782CCD3123580D90DBF534CA5C0121FD2 | 0ef79b13be55d16b164cf57b3c6bb7065b8ece9f |
| src/mcla_app.cpp | E544DA6F4D19B1624130891B8C363A981335B1ADCE1F5BE1651256A72292AD2D | 9f59d237c75ff549f8c4fe8a89ec24a1439904b8 |
| src/mcla_dev_overrides.h | 89C1F756E6AB1C13993B6C5FFAB3A553A3BC985FD70C82CA57A16330945FC71C | 62ceafd8923dfd8abb46b8d2fb681099ded4c758 |
| src/mcla_diagnostics.cpp | 15A15B5D61DE157BA10233875EEB9F30CB70810535A731C870C14FB13938BAC3 | 2f3e8062846a77a1e7519d66b3a6161236f78f0e |
| src/mcla_diagnostics.h | DDBB47650DB67EDEB4329C5B17D0DDE21B64F00AF938D8346D163AC3AC9F405D | 43f9a1481c9118ef33599dc2dfcd46886b9f5a99 |
| src/mcla_m10_defaults.h | C41093CC800D6B0E5CFCB3F82A0D37973ADC8226B3A4CAAEA779C09355A30916 | f17bd4474eb096b74b21dc97cdd82031f656213c |
| src/mcla_guest_msaa.cpp (new) | 4A92872FDF915171EB4E6B07F446708BEAC0BC7DA56A1BB6C6EB6AF89F552F03 | 9987a28be75cf52dd31d3209bcbdd7e569f4730b |
| src/mcla_guest_msaa.h (new) | 7BDD6BEC3A9F6BAD3B685A129AD0D27C8866F68AAB6017B38ECA499163D115F3 | 05b4c2e3deab7bb5886138e3683f81e827001ec7 |
| tests/mcla_dev_overrides_test.cpp | A744DCBF4E126265E8B121D5FD2A82811326D22FEAC44160E0EDD049CECE67E1 | 5e9a7abfd71949d98b2aced478f493e2058b7407 |
| tests/mcla_guest_msaa_test.cpp (new) | FA98465D5C687F922B9424B104AC418148B0AA6FECC721626535B0943D836FEA | d3d4387623b09ce1d7dbef6357501917237c18c0 |
| tests/mcla_launcher_shader_seed_win_test.cpp | 74E1F1F461982FDB8B4376F101146EAC1BEE328ED211BC2B0E1CCF84FE17188D | ac0e6f964af550c25bd177db0886d2c7d7e26a31 |
| tests/mcla_m10_defaults_test.cpp | 053EFA8B1B5666F4D42B186308A44D2DFB9DC53BBAF48A0F589BD5802948C3B2 | ba5d9bb71520d8dca5da7a5e2f15cdd04ae1326c |

Untracked private change: `private\l39-work\l41_targets.txt` (+ `mcla_guest_msaa_test`).

## 9. Identities

`private/l106-int-input` (SHA256SUMS sha256 AB93FE26D2A930CFEDCDB861BBEDE5C790D3A304BA8D49A0908D6BE300781B8A):

| file | sha256 | vs l105 |
|---|---|---|
| MCLA-Launcher.exe | 6C9F3D90F4CC5A582EB413AC71F66838111FEE7414AE85B1E4FDFE1447749F5F | new |
| TracyClient.dll | BFF5B2DADDC2F574625A6E922E40B41083D284A855A92E28BE7E4D75F80E815F | same |
| mcla.exe | D569AE5B50775017428367283C8290084481C5F1306A8424CB38C6F851334B53 | new |
| mcla_crash_handler.exe | B3E9C0A8E7D2DFCAAF14E9925A25F7A57FBEA3A07FFEBD709ED7E0C305DCB857 | new |
| mcla_diagnostic_image_helper.exe | 0A3FB40BE7D388F4F92332F00F1ACE18A62DB765114B8C1362C0161C61929177 | new |
| rexgpu-xenos.dll | 3A71267522F0191183126ADA0F4B6518C79745A9922864EF7EEA2A1A75F28657 | same |
| rexruntime.dll | 74663325AFD8A4477CBE5C0CBFC8CB5ED7EA086FCEFB1FFAC871BD77CF369173 | same |

`private/l106-internal-prepare.ps1` (sha256 23ACF65B259D67C5373EAD758F2439684AF660729CF20146A4E49B43572C0C7F):
the l105 script with the l106 header, the four host pins and `$expected` new, the three SDK DLL pins
and `$expectedRuntime` unchanged (l105-sdk-install); parse errors 0 in PowerShell 7.4.22 and
5.1.26100; not run (it signs the V1 ticket, stages `private/l106-internal` and adds the admission
row: owner step). `private/l106-CHECKLIST.txt` (sha256
00F59C14730C2293C4DB64D2B531C71C2FA0E23312B74A174F7233BF94DD3DF8): the l105 checklist with l106 names,
the two new always-on lines in Run 1, and Run 5 (pc/gpu-cat then pc/msaa-off-gpu-cat on the same
night route; pc/msaa-off and pc/msaa-off-fxaa by eye with an F10 at one spot; the proof lines).

## 10. Arms and helpers

`private/l106-arms`: 67 sparse arms = the 62 l105 arms with the first line `# l106 arm <plat>/<name>`
and byte-identical bodies, plus five new ones on the default base:

| arm | body after the base | sha256 |
|---|---|---|
| pc/msaa-off | guest_msaa_off | 707D66D6451BF52B628CE94A3F130B4B8EB042D28423257AF46F7EA20A62B445 |
| pc/msaa-off-fxaa | guest_msaa_off, present_fxaa | D3E09F0E43AA9763A7ECA5D9335091E91BB9A133F95A18B2777CFE8AAAFC1BB2 |
| pc/msaa-off-gpu-cat | guest_msaa_off, gpu_cat | 23060F3C6A2F13ABE729CA830F26E76604D97F0D9437F49F5C91CB56BC14EFF6 |
| amd/msaa-off | guest_msaa_off | DD90FF9C190BED51F7A098EB22B2503C53C43F69F6A89FE0C13D1AADE7855F83 |
| deck/msaa-off | guest_msaa_off (Deck base) | A611BF98A303573112DC1883C6611AF8D1E4E4FB895A4C54EEC0450A9D9F957E |

Sparse checker (the production parser of the l106 tree, 147 keys, expected true list per arm, the
census the only false): 67 arms, `mismatches: 0`, every file parsed with 0 ignored lines. Helpers: 55
`private/l106-arm-*.cmd` (the 51 l105 helpers with l105 -> l106 and the evidence reference pointing
here, plus msaa-off, msaa-off-fxaa, msaa-off-gpu-cat, amd-msaa-off; deck/msaa-off is copied by hand);
dry run in a scratch LocalAppData: 55/55 rc 0, copies byte-identical, clear removes the file, the real
per-user override file unchanged (same hash before and after).

## 11. Risks and limits

- Nothing ran. The order "renderer created before the scene targets" is inferred from the call graph
  (both on the guest's start-up path); the `rt_create` line checks it on the first run. If, in an
  arm run, it shows `applied=0` or comes before the `renderer_create` line (it then also says
  `requested=0`, because it reports the creation hook's state), the targets were made before the
  write and the arm must not be used.
- The visual result (foliage, fences, MSAA-dependent effects) and the gain are unknown until Run 5.
- If any guest path re-creates the scene targets later (a display-mode change), it reads the same +384
  (1), so it stays consistent; no such path was found (`sub_822E4CC8` has one caller).
- The SDK commit is local only and not in this package; the l105 SDK DLLs are reused.
- Scratch folder `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l106-work`: `re_scan.py`, `re_ctx.py`,
  `edit_lib.py`, `sdk_manifest_edit.py`, `sdk-build.ps1`, `sdk-ctest3.ps1`, `normwarn.py`,
  `edit_host.py`, `edit_tests.py`, `make_src.py`, `l106_host_build.ps1`, `l106_host_tests.ps1`,
  `stage_input.py`, `mk_scan.py`, `l106_scan.py`, `header_l106.txt`, `mk_prepare.py`, `parse_ps.ps1`,
  `mk_arms.py`, `check_arms.py`, `check_sparse.cpp`, `build_check_sparse.ps1`, `mk_helpers.py`,
  `dry_run_helpers.ps1`, `mk_checklist.py`, `list_temp.ps1`, `new/` (the new files as written); logs
  `sdk-configure.log`, `sdk-build1.log`, `ctest1.log`..`ctest3.log`, `w106all.txt`, `gate_build1.log`,
  `gate_build2.log`, `gate_tests1.log`, `make_src.log`, `l106-configure.log`, `l106-build.log`,
  `l106-tests.log`, `stage_input.log`, `l106-scan.log`, `mk_prepare.log`, `mk_arms.log`,
  `l106-arms-check.log`, `mk_helpers.log`, `dry_run.log`, `temp-before.txt`, `temp-after.txt`.
