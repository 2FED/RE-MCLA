# M7-016: Deck CPU profiling feasibility and next measurement

Date: 2026-09-05. Primary review plus independent read-only SDK/profiling
review. Includes the first direct userspace capture below. The subsequent
gameplay capture and reviewed GPU copy callers are retained in
[CPU attribution](M7-016-deck-cpu-attribution.md); read that for current results.
No performance fix
or platform PASS is claimed; the profiled game binary remains unchanged.

## Why change the experiment

The accepted [descriptor comparison](M7-016-deck-descriptor-reuse.md) measures
12.63 gameplay / 8.63 pause outputs/s versus 12.78 / 8.60 previously. About
37--39% texture-stage sets are reused, without a noticeable owner-reported
improvement. Existing nested wall counters and instantaneous F10 stacks
cannot identify an exclusive CPU hotspot. Some Wine stacks do not
unwind through missing modules. Another unchanged four-F10 run is not needed.

Keep the published Release 0.16.0.1 / SDK 0.18.0.1 binary and settings fixed.
First obtain actual sampled instruction locations, grouped by native thread
and module, before considering serial-overhead changes or parallel workers.
The broader [CPU scaling / matched Xenia package](../M7-priorities.md#separate-follow-up-deck-cpu-scaling--matched-xenia-baseline)
owns a controlled comparator and ordering/lifetime-safe optimization backlog.

## Existing profiling route: verified limitations

- SDK CMakeLists.txt defines REXGLUE_ENABLE_PROFILING only outside Release;
  include/rex/perf/counter.h gates Profiler::Startup and thread registration
  on that definition. The actual portable Release build.ninja lacks it.
- TracyClient is compiled with TRACY_ENABLE, ON_DEMAND, DELAYED_INIT and
  MANUAL_LIFETIME in thirdparty/CMakeLists.txt. Its DLL being shipped does
  not mean the client has been started or that zones/sampling are active.
- include/rex/dbg.h also removes the fine-grained GPU tracing zones in
  Release. Vulkan draw/UpdateBindings source zones exist; guest xthread
  registration is not comprehensive guest-function instrumentation.
- The Windows branch of thirdparty/tracy/public/client/TracySysTrace.cpp
  requests the SystemProfile privilege and calls Windows ETW APIs including
  TraceSetInformation and StartTrace. This Windows binary is not using Tracy's
  Linux perf path. Exact Proton 9.0-4 support has not been demonstrated.
  Enabling Tracy zones would not by itself prove statistical sampling works.
- Vendored thirdparty/tracy/manual/tracy.md warns that offline path-based
  symbol resolution does not verify symbol versions. Bind exact binary and
  symbol identities before trusting names. Source/code can be included in a
  Tracy capture; no capture or proprietary symbols may be uploaded publicly.

Paths above are relative to third_party/rexglue-sdk, pinned at
9098e122afa116f4316d6cec47e561ebc5186974. These are inspected source/build
facts, not evidence of a successful Linux or ETW profile.

## Capability gate: completed without security changes

In Steam Deck Desktop Mode, open Konsole. The game may be closed:

```sh
command -v perf
uname -r
cat /proc/sys/kernel/perf_event_paranoid
```

Owner returned /usr/bin/perf, kernel
6.16.12-valve24.5-1-neptune-616-gb2f7cfe85e45 and policy value 2. Paired SSH
subsequently verifies perf6.15-3 and installed flags. Unprivileged task-clock:u
counting succeeds first on an owned short child, then on the exact live game;
the bounded leaf recording below also succeeds. No sudo,
package installation, read-only-system change, capability grant or sysctl
change was performed or is implied by this capability check.

The [Linux perf security documentation](https://docs.kernel.org/admin-guide/perf-security.html)
explains per-process/userspace restrictions and credential/capability checks.
A permissive-looking value alone does not prove attachment works on this
distribution. Sampling can disclose paths, addresses and process data; keep
all outputs private in the existing Syncthing evidence workflow.

## Bounded capture protocol

1. Verify the exact unchanged bundle/PE identities and the Linux process
   corresponding to this MCLA session. Wine's Windows PID 336 is not assumed
   to be a Linux PID. Do not attach to all Wine processes or the whole system.
2. Start with a short (approximately 20-second), low-rate, userspace-only
   leaf-instruction sample of that process's threads in a settled pause.
   Record event/rate/duration, native PID/TID and creation identities, phase,
   lost samples and module-load information. No system-wide capture or raw
   stack-memory recording by default. Installed-tool flags were verified and
   the first recording used the command documented below.
3. Establish correct PE load bases/RVAs and exact PE/PDB or map identities;
   retain relevant Wine/ELF module build IDs and load mappings. Unknown or
   mismatched symbols stay unknown. Leaf-IP aggregation is not a validated
   call graph, and stopped-thread F10 contexts are not equivalent samples.
4. Compare sampling overhead using adjacent settled-pause output counters;
   check lost/insufficient samples and whether CPU samples represent useful
   execution, polling or unresolved modules. If incomplete, report limits
   instead of naming a root cause. Native CPU-time samples do not directly
   attribute blocked wall time or active GPU time.
5. Only if this yields useful attribution, add a short gameplay sample and
   decide a targeted regression-safe optimization. Do not require driving,
   races or a new soak merely to establish profiler availability.

If Linux sampling is absent/denied, stop for a supported alternative or new
authority; do not silently lower security or publish another speculative
runtime candidate. ETW/Tracy instrumented builds remain an unvalidated fallback,
not a reason to change the baseline before the Linux route is checked.

## First direct capture and limitations

Owner approved the narrow route and manually paired the official SteamOS
Devkit client. SSH is now verified with the paired key and pinned host key.
The existing Steam non-Steam shortcut was identified from its stored record
and launch log, then launched without modifying its Proton settings.
The installed runtime directory is named Proton9.0(Beta); the owner calls
the selected release9.0-4. Directory wording alone is not a stronger version
attestation. Exact sampled Wine file identities must accompany symbol analysis.

[Valve's Devkit documentation](https://partner.steamgames.com/doc/steamdeck/loadgames)
supports Add by IP and owner-confirmed registration of an SSH key. Across a
routed VPN, use explicit IP instead of relying on LAN multicast discovery.
[Tailscale's subnet routing documentation](https://tailscale.com/docs/features/subnet-routers?tab=macos)
distinguishes access to a remote LAN from exit-node internet routing. Avoid
unnecessarily redirecting this workstation's whole internet connection.
Private network addresses and credentials are not recorded in public evidence.

The owner confirmed settled PAUSE. Private evidence is at
private/evidence/M7-016/cpu-20260905T175611Z-93337, also retained under the
unchanged bundle's diagnostics directory. The private collector is
private/evidence/M7-016/collect-deck-leaf-profile.py; no installer or runtime
instrumentation build was published.

Recording: perf record -e task-clock:u -F99 -p<verified Linux PID>
--no-inherit --no-buildid-cache -o<private perf.data> -- sleep20.
The collector neutralizes perf configuration, verifies exact immutable hashes,
mapped PE device/inode, same-user TGID and process-start ticks, and preserves
before/after maps/thread identities. Timeout handling only targets its own
perf child, never the game. Decode uses explicit leaf fields; no callchains,
registers, raw stack bytes, kernel or system-wide samples were requested.

- 4,249 samples across19.992978seconds; header IP|TID|TIME|PERIOD confirms
  the requested scope. All transferred artifacts match their recorded hashes.
- Linux PID93337/start ticks156476661 remains identical;70threads at both
  ends with no added/departed/reused identities observed. No LOST or THROTTLE
  records found. The game remains running; capture completion is not a game
  session or gameplay-stability PASS.
- TID93415, XThread0210, has1,758 samples. The exact current PE's own
  function table and pdata bind921 samples to the PAUSE-loop/comparison in
  guest sub_82412F98:52.39% of that thread and21.68% overall. The sampled
  range includes the comparison after the PAUSE instructions. Static callers
  poll progress counters; leaf samples do not prove the actual caller.
- GPU Commands TID93367 has1,441 samples. Initial module attribution includes
 225 in Wine ucrtbase,215 in the Vulkan driver and180 in Wine Unix ntdll;
  exact SDK function attribution was pending at this first-capture boundary
  and is now completed in the linked attribution report. One hot ucrtbase RVA is
  verified against the installed DLL's own disassembly as its SSE memmove
  loop. Module totals are not all that function's time.
- Release has no embedded PDB identity; do not reuse older maps. The exact
  current executable embeds30,034 guest/native mapping entries, independently
  checked against every generated registration key. Own pdata bounds and
  aliases must be retained; nearest-guest labeling of gaps is invalid.

This establishes substantial userspace active waiting, not permission to
delete synchronization, sleep arbitrarily or alter core affinity. Useful GPU
work, copying and protection overhead need exact attribution. Leaf samples do
not establish call chains, blocked wall time, active GPU duration or a projected
FPS improvement. Live instruction bytes and a matched sampling-overhead control
were not captured; those limits remain explicit.

Startup initially hit a separate launcher policy bug: an incomplete shutdown
F10 blocked a completed game session. Original bytes were archived and the
specific orphan reversibly isolated; owner confirmed launch. See
[launcher recovery](M7-016-launcher-diagnostic-partial.md). No save, album or
profiled binary was changed. WSL remains unavailable, without system changes.
M7 stays0/16; KI-027 and evolved successor-save validation remain open.
