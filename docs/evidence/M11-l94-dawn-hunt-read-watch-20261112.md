# l94 dawn hunt: read watch result (2026-11-12)

Read-only analysis by an Opus subagent, accepted by the primary agent. Spark not used: log parsing and generated-code reading.

**Headline.** Owner hunt on l94 (2026-11-12, COPY profile, Arcade race restarted, 05:30 start, clock x10): arm dawn (strict default on) 0 of 3; arm dawn-off (strict off) 1 of 3 (a tree, slightly); arm dawn-off-watch 5 of 10: restarts 1-5 clean, 6-10 all with trees blinking once or twice, never the sidewalk; every F10 falls 0.2-1 s after that restart's 06:00 switch. Read watch: 569 guest reads of read-back data in the whole session, ALL from one reader: the car-livery bake (CRT memcpy called from sub_82431E10, an in-place untile of the 1024x1024 livery composite 05368000 and its mips), identical in clean and flicker restarts; ~144 keys armed per minute, only those 8 ever read, 0 collateral. So hypothesis 1 (a CPU reader consuming stale read-back data) is dead for this repro. No counter separates restarts 1-5 from 6-10 (strict is off in both: lead2_frames and fence_late_f2p are high in both). Candidate for the trees: a day-only family of ~28 single-channel k_8 128x128/256x256 colour resolves at 0x0317C000-0x03566000 that appears at 06:00 (refreshed round-robin every 49 frames, sampled GPU-side; probably shadow or impostor tiles). Next: a narrow probe on that family.

---

# l94 dawn-off-watch analysis notes (2026-11-12)

Evidence copied to private/owner-l94-20261112/{sessions,diagnostics}/<id> (save-private and
save-files-private.json excluded; before-user/after-user not copied).
Build D82D1F3C... (l94) sessions: 91f8262a dawn, ea95fc40 dawn-off, 497bf804 dawn-off-watch.
Sessions 2471789b / b2cd9956 are build 0F3E2F19 (not l94); copied anyway.

Watch session: 569 MCLA_RB_READ hits, all guest_pc 82431808 (CRT memcpy, TU28:13901),
lr 82431ED0 (sub_82431E10 TU28:14810, in-place XGUntileSurface-like: src==dst -> alloc temp + memcpy),
thread 23, write=0, in_range=1, collateral=0. Keys: livery composite 1024x1024 fmt6 at 05368000 and
its 512..4x4 mip chain (05368000/05468000/054A8000/054B8000/054BC000-054BF000), 70 hits each.
Output: probably the CPU-baked DXT5 1024x1024 texture at 05268000 (+mips 05208000), loaded unscaled.
144 keys armed per minute, 8 hit; suppressed=0 full=0 mismatch=0 dropped=0.
Bursts at race start (+1..3 s) and ~5-7 s after each 06:00 switch, identical in restarts 1-5 and 6-10.

Accumulating across session: texheap used_mb 597->1550 (no heap released), VRAM local 4183->5227 MB.
But dawn-off session flickered (1/3) at 590-740 MB, so not necessary.
Day-only family: k_8 (fmt 2) 128x128/256x256 colour resolves 0x0317C000-0x03566000, every 49 frames,
GPU-sampled only (no CPU hit).
