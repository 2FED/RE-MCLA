# M7-016: draw/binding/descriptor timer-overhead control

The matched timed/count-only/timed Deck comparison completed and shows a modest
single-cohort cadence increase in count-only mode. This control targets the combined cost of clock
pairs, elapsed arithmetic/conversion and elapsed-counter atomics in three scope
families; completion counts remain enabled.

Retained U16 samples imply about392,803 clock calls/s unpaused and572,328/s
paused from draw packets, UpdateBindings and descriptor updates. Existing HUD
and guest-function instrumentation controls do not isolate these always-on
timers. Operation volume does not establish their actual overhead.

## Exact change

Two generic draw-scope declarations and the Vulkan binding and descriptor
declarations use a small count-only RAII helper. Lexical scopes, early returns,
unwind completion counting, renderer calls, protection and wait behavior remain
unchanged. No public header or ABI changes. One constructor log marker identifies
the intervention. Neither variant contains the wait census.

The three disabled elapsed fields are draw_packet_us, binding_update_us and
descriptor_update_us. Raw samples retain their schema and zero placeholders;
the availability adapter requires zero deltas and advancing completion counts,
then converts those elapsed deltas to null and lists them as unavailable.
They must never be interpreted as measured zero cost. Baseline requires positive
elapsed deltas and absence of the count-only marker. Two positive and eight
negative availability cases pass.

Both variants use FILEALIGN4096 and identical U16 SharedMemory, host/runtime and
other objects. Generic and retained clean Vulkan sources are compiled separately
for each variant; all six owned compile/link steps completed normally. The
baseline virtual image reproduces retained U16. Bounded binary review confirms
four exports, ABI2,360 ordered imports, unchanged vtables and320-byte TLS.
Clock references disappear only from the targeted draw/binding/descriptor scope
paths, with completion increments retained. Other function changes correspond
to logging-line immediates; exhaustive EH equivalence was not claimed.

## Native validation

Both private native controls passed with unchanged complete four-file profiles,
18 binary hashes,14 loaded mappings, four13-byte configuration observations,
12 frame hashes and188 raw counter deltas independently checked. No forced
cleanup, survivors or protection failures occurred.

| Native outputs/s | Timed | Count-only |
|---|---:|---:|
| Unpaused |29.799997|29.849925|
| Paused |29.999996|29.998190|

These near-cap values and differing draw workloads establish no speedup.
Candidate raw elapsed zeros become null/unavailable in both analysis files.
Timed baseline has one unpaused metadata gap-lock miss, correctly marking that
fragmentation evidence incomplete; it is not a protection failure.

## Identity and current run

Private root: draw-timer-overhead-20260909.
Source manifest6DE215ECFF5C783FAC234F73A4D2F4CA48B319A28184D19C81721A3760AE9178.
Build inputsD398ACB1DE7FD24CC559DFA40E319AA3DD54226675DFE31445D724230423FE29.
Build resultB4816CBE41DC9639F34347FA01AA6A9AF9965D48F7FAFC3C344D972E41409479.
Timed GPU528CAA0DE5C7F17032B039C5CF774FFFC2A3FDC311D6D1A2B8330D0895A2F234;
count-only GPU3D1B3DC9247804ADD45A9722A79FA52C914B0EA118CC5A6770E2D6A997F1B031.
PackagesF7E0189641D723BCA69C466060075E43F1841704C181A679CAB64A2E827D8B6B
andEEE1C4159514D009413CA52323F3C448FBEBB560B42DC378BB097737980BD300.
Config proof6A97F9EA/readerF5BB86C3 retain strict exact13-byte observations.
Availability adapter12D336E2; native helper7D6A3140.

Native resultsBEB190CD1DB9D519519B2FD444C838A24DE0FEFB5C2F5EE347942842A8434713
andDDE24C67D4A859C14DF4C14220912ECFD8AD5DD7B83FDAB525027567F2DBAFCF.
Deck runnerB9CCCBDA517703FDF2A18F83F93BF60F5999777354D583F59595C36EFE128C42;
supervisorFCB7603F6CC13CAAA77DFD5B292F37180773E75BD9A5BD43BCB886B2F121FD53.
All45 staged payloads/130,438,338bytes verified from archive06EA133A....
The order is a-timed/b-count-only/c-timed, with HUD off, Mango off for all arms,
validated CPU/focus collection, full seed,1x/pre-mask=true and normal-closure gates.
Independent return audit passed:141payloads/94,135,972bytes,282raw deltas,
six availability analyses, six13byte observations,72mapping records,18frame hashes,
unchanged profiles,1260CPU samples and phase focus observations. All three arms
and helpers exited normally, with no forced cleanup or survivors; Mango restored.

| Deck outputs/s | A timed | B count-only | C timed |
|---|---:|---:|---:|
| Unpaused |19.949994|20.299994|19.949995|
| Paused |18.749997|19.549996|18.599992|

B exceeds both controls by1.75% unpaused and4.27-5.11% paused. Unpaused draws/output
differ by only-0.13 to-0.18%; paused by-0.57 to-2.01%. B also has about5-6% fewer
write faults/output. The pattern is consistent with observer overhead, but does
not isolate clock-call cost from arithmetic, elapsed atomics, scheduling and
workload/interleaving differences.

The primary-buffer-minus-WAIT elapsed proxy is39.80/37.38/39.98ms per unpaused
output; it is not exclusive CPU time or a recoverable frame budget. Whole-arm
system CPU averages32.67/32.66/32.62%, not title CPU during the20s windows.
B unpaused has one metadata gap-lock miss, so its fragmentation evidence is
incomplete; all protection failures remain zero.

Root viewed six representative parked/pause frames across all arms without
obvious corruption. That does not establish continuous rendering or physical
driving/audio acceptance. Stable30FPS remains unmet.

ArchiveCA9C7D5B48EEFC3C9CD0DD74F4ED1D044AA9070F1800AC432A1A28EF58CE33EA;
manifest38C397281D1FB74A82E1F166D23454BE9DFB60F9339220485C35EE4F3BCBE3D2.
Root analysisF415D8AE93D33DB553A82D1CBFBAD0B0C966738AF2CB739299487427AF2A7ACD
was independently replayed. Its creation-time pending-review status is historical;
this report records acceptance.

Retain count-only as a private fast candidate for further bounded controls,
with all three elapsed fields explicitly unavailable. No normal package, SDK
source, version, commit or push promotion has occurred.
