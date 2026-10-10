# M7-016: export existing draw counters

Status: diagnostic build, native/Deck captures and all source/return reviews
complete. The existing counters leave CPU draw stages as the next question.
M7 remains 0/16. This is an attribution experiment, not an optimization or
evidence of stable 30 FPS.

## Question and scope

The [caller-yield control](M7-016-caller-yield-candidate.md) produced no positive
cadence signal. Before adding timers to the draw path, this experiment exports
21 fields that the current SDK already measures and the application already
captures. They cover descriptor writes/reuse, submission and fence calls,
command waits, frame intervals and additional watch/fault work.

Private evidence J is `private/evidence/M7-016/draw-counter-export-20260907`.
The baseline is the E54 published-scope host with strict hardware FMA in TU0.
J changes only cost-sample serialization. It adds no clocks, snapshots,
per-draw logging, synchronization or SDK changes. Serialization still occurs
after each measured window and frame capture. The caller-yield experiment is
not included.

## Source, build and review

Private schema v3 exports 47 counters. The private parser retains strict v1/v2
support, identifies unavailable older fields instead of substituting zero,
and rejects inconsistent schemas within or across paired analyses. Its 37
Python test methods pass. The C++ serializer test passes 4,688 checks with no
failures, both standalone and through the configured build.

A two-file Clang VFS overlay maps the original serializer and unit-test paths
to private copies. Preprocessed paths and relative includes retain their
original identities. The application and serializer test alone receive the
overlay; the 63 guest scope settings, TU0 FMA options and link flags remain
unchanged. Explicit dependency edges include the overlay and both private
files. All frozen source/toolchain inputs and configured commands verify.

The full build closes normally after 158.047 seconds. Independent emitted-code
review finds 71 other host objects unchanged except COFF timestamps. Of 2,046
app-object function spans, 2,040 are identical including relocation references;
changes are confined to the serializer and its five exception funclets. Its
four callsites remain in the unchanged diagnostic probe, with default-off
routing preserved. All 29,788 aliases and 30,034 mapping associations verify.
Imports remain exactly 665, with no yield import. Shared helper instructions
and all 96 hardware FMAs remain intact.

The nine-file package changes only the host executable and retains the eight
baseline companions, including the existing crash helper.

| Artifact | SHA-256 |
|---|---|
| Source manifest | `609F64F0F639077B2607E8BA2832B76CD441BBC34B4F5586F6C2FCEFF582D497` |
| Frozen build inputs | `2B4031C217C4678414DD741635D109200C9CDF0C9FC5335AE03E3E8807936364` |
| Host | `C74A0AA50E56456641B35462E6982740103C4F9D67C43F36BBF7A27F31115C5D` |
| Linker map | `204917D07E24ED26D584A1BEAE55281C1CC563D4B20202BBD04B3BA3D35CB9EC` |
| Package manifest | `37B61C213AC852822EEAD888C44BFED3AE837E52DD2DE1B14EC9FDB11379756C` |
| Private sample parser | `7E7DCF26EF5FDA4976FC283917B77284B640B82853320ADC3DDDEDEA456C7E94` |

## Native control

Execution 78796, PID42172, closes normally with exit0 after174.0359239 seconds.
There is no forced cleanup, surviving owned process, source drift or seed
change. Seven expected loaded images and six captured frames verify against
the run receipt. Root views the five gameplay/unpaused/pause frames: the
initial rear view shows HUD0mph; the idle camera moves to a front view after
the unpaused window. Gameplay and pause are rendered. This is not an equal-work
performance comparison, and no audio claim is made.

The raw windows contain592 outputs/20.000719 seconds unpaused and600 outputs/
20.000667 seconds paused. Both provide the v3 fields. These are native PC
observations, not Deck FPS.

Native result SHA-256 is
`B82559716DAB45BBE3152750223A0055C558096232A199D8D45B208CA848FA63`;
independent verification is
`1637550E69F4E55C0A4478FC2060D1F6032B47AB1E81838843EA1DBCBB9656F1`.

## Deck capture and observed costs

Execution89839 closes normally with exit0 after178.330295193 seconds, title
PID152790/start167484035. The exact Proton10/runtime package, twelve selected
mapped images, source and probe profiles remain unchanged. There is no forced
cleanup, error or surviving owned process. The reviewed collector returns36
files/31,256,641 bytes, all verified before extraction.

- Comparison fingerprint: `5C1B6DB0BF5C81EF4A6FF919D950987D022FB4EFFF9DD8511604D52E86F01BD8`
- Archive: `F4379A5C45A1648401DBCFD0D5BEFA2D46C636767CAA7535E20E4F934583659C`
- Inventory: `926887A32A9F19E7C0D512172C217A88654F3ED4B4BF8CB1A06E08089903C331`
- Analyzer: `3B921BF8D5D7B43244FF08D7FADD9973C0D4C0523B01B160023EC232BB17B489`
- Analysis: `FA4B9142F7B620D458CBE060B70F1FEB141371F4786ABCAE258C499C6B0E743F`
- Verification: `2E2753CB1A2FAF4920C7C5072CC177D16CC7EB93FC4C26A0154214CB65F5C683`

Primary source review and reproduction, plus an independent final review,
confirm the exact analyzer output, all94 raw counter deltas, normalized metrics
and retained provenance/lifecycle/phase gates. No runtime repetition was needed.

Root independently subtracts all47 raw counters and inspects five phase frames.
Unpaused endpoints show the rear view/HUD0mph on the parking area, with changing
traffic, pedestrians and minimap. Pause remains rendered at the same endpoint
angle. This does not prove identical work throughout either interval or usable
driving/audio performance.

| Observation | Unpaused | Paused |
|---|---:|---:|
| Outputs /20.000004 seconds |382|300|
| Outputs/s |19.1000|15.0000|
| Draw packets/output |3354.17|5352.06|
| Draw-packet ms/output |34.9479|40.9728|
| Binding-update ms/output |5.4179|8.4521|
| Descriptor-update ms/output, within bindings |2.1242|3.2565|
| Deferred-command execution ms/output |5.3913|6.3626|
| PM4 WAIT_REG_MEM ms/output |6.3319|11.0502|
| Queue-submission ms/output |0.4030|0.5681|
| Pipeline-end ms/output |0.0028|0.0039|
| Frame-fence check ms/output |0.0121|0.0080|
| Vulkan fence-call ms/output |0.1186|0.0552|
| Texture-stage reuse fraction |34.73%|32.86%|

The added fields do not identify a dominant Vulkan fence or pipeline-end stall.
They leave material draw work unattributed. The next bounded investigation is
CPU timing of IssueDraw stages, with nanosecond accumulation, safe snapshot
publication and no scheduling, cache or coherency change. The figures above are
nested observations, not an additive decomposition or a speedup comparison.

## Interpretation constraints

Counters are cumulative independent snapshots, not a transactional frame
record. Timing scopes overlap and can span threads; their per-output values
must not be added into an exclusive CPU budget. Existing timers may truncate
each completed scope to whole microseconds. The draw-packet count also includes
COPY, skipped and failed calls, so it is not a count of emitted Vulkan draws.
Descriptor reuse/write counts alone do not establish texture creation or cache
miss rates. A single diagnostic capture cannot establish an optimization gain.

The newly exported scopes have these specific meanings in the pinned SDK:

- Deferred execution is CPU replay of recorded commands into `vkCmd*` calls;
  it excludes later GPU execution, command-buffer ending and queue submission.
- Queue submission includes acquiring the queue and the driver call. Its timer
  may include an unsuccessful call, while the submission count increments after
  successful normal submission and bookkeeping.
- Frame fence checking includes reclamation and invalidation around completion;
  Vulkan fence-call counts include zero-timeout polls, blocking calls and errors.
- `command_wait_reg_mem_us` measures PM4 worker waits after a failed predicate.
  It is separate from the generated guest watchdog examined previously.
- Texture reuse/write counts concern successfully retained or written stage
  sets, with vertex and pixel stages counted separately. Their reuse fraction is
  reuse/(reuse+write), when that denominator is nonzero.
- Descriptor-update time measures the `vkUpdateDescriptorSets` call within
  binding updates, including constant descriptors. It excludes other descriptor
  calls elsewhere and is nested inside the binding timer.

Binding and descriptor timers accumulate nanoseconds before snapshot conversion;
most other timers truncate each completed scope or episode to microseconds.
Vulkan/packet counters normally accrue on the GPU Commands CPU thread, whereas
fault counters aggregate faulting threads. Existing full diagnostics also expose
fault-lock/query and output-failure counters if those become necessary.

The normal bundle, shortcut and accepted four-file profile remain unchanged.
No save-progress proof, promotion, task closure, version bump, commit or push
is claimed.
