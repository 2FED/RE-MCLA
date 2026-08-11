# M3-003 guest image and dispatch configuration

Date: 2026-08-11

Result: PASS — XEX IMAGE, ENTRY POINT, AND DISPATCH RANGE VERIFIED BEFORE GUEST LAUNCH

## Static generated contract

`scripts/verify-module-config.ps1` parses the ignored generated configuration
without publishing generated code. It requires the exact accepted Complete
Edition layout and a bounded, ordered function map:

| Property | Verified value |
| --- | --- |
| Image range | `82000000-829E0000` |
| Code range | `82130000-827CD054` |
| Function-table range | `829E0000-8373A0A8` |
| Executable entry | `821322B8` |
| Function mappings | 30,008 |
| Duplicate/out-of-range/null mappings | 0 |
| Final sentinel | exactly one |
| Entry mapping | exactly one |

The function-table range includes the SDK's 64 KiB thunk reserve and its
two-byte-per-code-byte dispatch layout. All range arithmetic is checked in a
64-bit host type before accepting a 32-bit guest range. One positive and five
negative fixtures pass; the negative cases cover a wrong image base, missing
entry, out-of-code mapping, duplicate/unordered mapping, and missing sentinel.

`MclaApp::OnFinalizePaths()` repeats the critical checks in the native process
before constructing `Runtime`. The mapping walk is bounded by the maximum
possible aligned instructions in the accepted code span. Invalid metadata
queues controlled shutdown and cannot reach SDK registration.

## Loaded-image contract

After `Runtime::Setup()` and `Runtime::LoadXexImage()` succeed, the project
overridden `LaunchModule()` verifies all of the following before delegating to
the SDK guest-launch implementation:

- a Runtime, KernelState, executable module, and FunctionDispatcher exist;
- the loaded XEX base is `0x82000000`;
- the loaded executable entry is `0x821322B8`;
- the dispatcher owns both ends of the accepted code range;
- the executable entry resolves to a registered host function.

Any mismatch logs `loaded image contract rejected; guest launch blocked`,
queues host shutdown, and never calls `ReXApp::LaunchModule()`.
`--mcla_module_config_probe` follows the same successful path but exits before
`PrepareModuleLaunch()`, so its evidence makes no guest-execution claim.

## Shutdown defect found by the first probe

The first Release probe successfully loaded the XEX, registered all 30,008
functions with zero duplicates/rejections, validated the entry, and logged
project shutdown. The process then terminated with `0xC0000374`. A
RelWithDebInfo CDB run localized the underlying first-chance access violation:

```text
rex::ui::Window::RemoveInputListener
rex::input::mnk::MnkInputDriver::~MnkInputDriver
rex::input::InputSystem::Shutdown
rex::Runtime::Shutdown / ~Runtime
rex::ReXApp::OnDestroy
```

`ReXApp::OnDestroy()` destroyed `window_` before `runtime_`. The default mouse
and keyboard driver retains the attached Window and removes itself from the
listener list during Runtime/InputSystem teardown, producing a deterministic
use-after-free. Project-fork ReXGlue v0.9.0.3 reverses only those two resets so
input drivers detach while their Window is still alive:

- release: `v0.9.0.3`
- commit: `b5d268d7352a956d21d0fdb10ecc9f1f36e0455b`
- subject: `fix(ui): destroy runtime before attached window`

The SDK source passes clang-format `--Werror`, installs in Debug, Release, and
RelWithDebInfo, and the project static lifecycle test requires Runtime teardown
to precede Window teardown.

## Final host verification

| Gate | Release | RelWithDebInfo |
| --- | --- | --- |
| Configure/package | exact SDK 0.9.0.3 | exact SDK 0.9.0.3 |
| Compile/link | pass | pass |
| Process exit | 0 | 0 |
| Ordered markers | 5/5 | 5/5 |
| Guest launch | skipped | skipped |
| Executable SHA-256 | `7B59A6A0BEF6D4A7D9CBF580F6130B97FDE04D693B796FE34D62F13C5822B8CA` | `8BE5CB489B01E88849BDE330088E6009F4B50BB6D4701CFA85807B612289EE97` |
| Private log SHA-256 | `A3094B2E576975902F37514AE699D8DFDED743396F3AFE39456074422847D9C3` | `F06A4BEF3D7D2060E67911B0ECF6705A3D222428B1493B472198B4A3E702DC53` |

Raw logs and debugger transcripts remain under ignored
`private/evidence/M3-003/`. Probe user/cache writes are redirected into the
same ignored per-run directory. The game root is used read-only to load
`default.xex`; formal VFS mount/write-containment evidence remains M3-004.

## Upstream duplicate audit

The audit searched open and closed issues and pull requests for ReXApp
shutdown, Runtime/Window teardown, InputSystem, `MnkInputDriver`, listener
removal, heap corruption, access violations, and use-after-free. Connector
searches were followed by a full authenticated enumeration of all 217 issues
and 183 pull requests. The exact failure belongs on existing open issue #336,
"Non-graceful shutdown possibly related to Fibers and the heap"; creating a
new issue would duplicate that entity. A sanitized root-cause comment is
prepared privately and awaits explicit approval before publication.

## Acceptance decision

M3-003 is complete. Static layout is fail-closed, the real XEX base and entry
match the accepted source, both ends of the code range and the entry resolve
through the dispatcher, and the probe reaches clean host shutdown without
starting guest code. M3-004 owns the complete game-root VFS and write-policy
contract.
