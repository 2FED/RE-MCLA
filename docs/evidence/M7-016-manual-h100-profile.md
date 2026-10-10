# M7-016: owner-confirmed symptomatic H100/U8 moving profile

Status: capture validated; owner confirms characteristic dip occurred during the
recording, then explicitly exited the game himself. No clean FPS or causal claim.

Exact title224050/start189267368, retained ABF host/AEE runtime/U8 GPU8405.
Root helper9758A2546F66CEAC5460D1FFC08A9EE7AE57CFB61BA73A8F3AA8064FD4434FB4
passed independent critical review and read-only readiness before recording.
Twelve seconds99Hz task-clock:u, no inheritance, leaf-only (no stacks/registers),
clean eight-key profiler environment. All three owned profiler commands exited0.
No title signals, injected input, pause or game settings changes.

Returned14 regular files1744559B:
archive31F03CAE9049C2321EEA49E8C75E059B4EE37CB83C3AF4B1CA3A5F9E8BC53442,
manifestB44B38E8A09B2A6C52117FE251EBC46B52ECEBC57E70DBB020355D83FFD4AF1D,
result8769AC57A7BAFBB3316AADADE564CF33EEA83D11E8984218E7108ACD542CCBE5,
raw103521C8B7B97BBDBEA3C4F81B4926A01FDC08F8D9B30093E117338E15BA7BC9.
Local root private/evidence/M7-016/manual-h100-profile-20260909/verified.
Root strict unpack and independent capture integrity review passed.

3688 leaves span11.991387387s; record child12.09010812s. Outer snapshot bracket
12.56178755s. Seventy stable bracketed threads,21 sampled; endpoints cannot rule
out transient threads. No sampled CPU-id field: physical placement and SMT sibling
contention are not established.

| TID | User/kernel CPU seconds | Share of one CPU over outer bracket | Leaves |
|---|---|---|---|
|224112|10.97 /0.70|92.90%|1081|
|224078 (GPU)|10.34 /1.19|91.79%|1012|
|224120|6.38 /0.32|53.34%|628|
|224128|2.50 /1.67|33.20%|233|
|224116|1.20 /0.02|9.71%|103|

Process total37.78 user+6.35 kernel CPU seconds. Process I/O deltas:
rchar31,111,573; wchar9,518,733; syscr77,759; syscw71,157;
read_bytes262,144; write_bytes0; cancelled_write_bytes0.
Minor faults3458, major faults0. Logical counters include cached/non-file
operations. This does not measure read latency or exclude storage stalls.

Independent exact PE/MAP/range attribution found shared wait sub_82412F98
623 leaves on224112; GPU UpdateBindings69 and FindOrCreateTexture22.
On224120, guest sub_82146BE0 has126 leaves and sub_823F4450 has122; their subsystem
and callers remain unresolved. Exact AEE codec functions include decode_frame27,
ff_wma_run_level_decode14, ff_imdct_half_c11, fft8/pass7 each and
xmaframes_decode_packet3. This proves sampled codec work, not that the whole thread
or those guest functions belong to audio or traffic.

Retained VCRUNTIME memmove attribution162 GPU leaves is qualified: this capture
did not independently hash that mapping. ABF executable text is anonymous under
Wine; file-backed header plus stable geometry binds the static image, not a live
code-byte attestation. Leaf weights are not exclusive/recoverable frame time.

Owner now prioritizes a per-game affinity8/4/8 experiment. More cached assets or
SMT contention remain hypotheses; this profile alone proves neither.


Static follow-up supports audio DSP attribution for the two guest hotspots:
82146BE0 performs two conditional recursive filter banks on256float samples;
823F4450 mixes256samples with a gain ramp, called with1024-byte channel strides.
Coefficient caching and bypasses already exist. This interpretation follows
the bodies independently of the codec leaves; exact voice/object and callchain
remain unresolved. Together248leaves are6.72% of user samples, not exclusive time.
Mixer samples include endian shuffles and PPCContext stores; sampling skid and
existing differences between scalar FMA+rounding and VMX multiply/add prohibit
blind arithmetic fusion. Next read-only discriminator is exact instruction-class
attribution against the retained ABF image.
