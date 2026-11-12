# l94 dawn hunt: tree shadow impostors and the arm table (2026-11-12)

Read-only RE and analysis by an Opus subagent plus owner runs read by the primary; accepted by the primary agent. Proven and inferred points are marked in the body. Spark not used: generated-code reading and log parsing.

**Headline.** Owner hunt arms on l94 (2026-11-12, COPY profile, trees only, never the ground): dawn (strict on) 0/3; dawn-off 1/3; dawn-off-watch 5/10; dawn-inject2 (strict ON, cadence copies held 2 frames; verified: 15345 strict waits, lead2 0, f2p 0) 6/10; dawn-off-steady (strict off, steady exemption off; verified: lead max 1, lead2 0, f2p 0) 7/9. RE: the day-only k_8 family at 0x0317C000-0x03566000 is the TREE SHADOW IMPOSTOR set (sub_8230B778 creates ShadowImpostor 128/256 targets; sub_8230D988 re-renders one impostor per frame, or ALL of them in one call when the hour jumped by more than 1/120 h, which is what happens on the first day frame; night and day impostors share the same guest memory). Code proof: age inject cannot change what the GPU samples (it only delays guest-RAM copies; every family texture load is from the scaled resolve; no CPU invalidations), and the read watch has no blind spot. Therefore the tree blink at 06:00 is most likely INDEPENDENT of the frame lead and of read-back freshness, with a base rate of roughly 50-70 % per switch; the strict-on 0/3 is weak evidence (p about 0.13 at a 50 % rate). It may be the game's own impostor regeneration at the switch (it would look the same on a console) or an ordering issue inside that first day frame. This is a DIFFERENT phenomenon from the night surface flicker of buildings, walls and fences, which strict removed in long runs. Needed control: dawn (strict on, no inject) for 10 restarts; probe: a texture trace on that address range around 06:00.

---

# k8 family RE notes (2026-11-12, read-only)
Work: all.log (l94 dawn-off-watch 497bf804), inj.log (l94 dawn-inject2 26f27ab9), fam.py/night.py/bake.py.
Retail image expanded temporarily (tod-re/scripts/xex.ps1) for strings, deleted.

Game side (generated/default, TU16 = mcla_recomp.16.cpp):
- Strings: generate_impostor / generate_night_impostor / generate_shadow_impostor (0x82041AD8/AC0/AA4),
  ImpostorNormal/ImpostorColor/ShadowImpostor/ImpostorDepth (0x82043200..3230), PropFoliageImpostor,
  notreeshadows, noimpostors.
- RT creation sub_8230B778 TU16:39165: ImpostorDepth 256x256 32bpp shared (*0x8288D088);
  per impostor: ShadowImpostor 8bpp 128x128, or 256x256 if size float > const (obj+196);
  ImpostorColor (n*128)x128 32bpp (obj+188); ImpostorNormal (n*128)x128 16bpp (obj+192).
- Technique handles TU16:44861-44902: 0x827E58CC generate_impostor, 0x827E58D4 night, 0x827E58D0 shadow.
- Shadow impostor render sub_8230C860 TU16:41774: lock RT obj+196 + ImpostorDepth (vt+60), clear,
  technique shadow (:41948), draw sub_8230BC38 with -sun dir, sub_8230B9B0, unlock/resolve (vt+64).
- Update sub_8230D988 TU16:44430: last hour 0x8288D078 vs current (sub_822F10D0); if |d| < 1/120 h
  (0x820405D8 = 0.0083333) one impostor: idx 0x8288D044 = (idx+1) mod count (lhz list+12), entry stride
  224, render if +168 != 0 and +196 != 0 (:44568); else ALL (:44590-44613). Store hour.
- Called from sub_823120C8 TU16:56033 only if (*(r19-8092))+360 & 0x20 (pass mask bit; day gate inferred);
  sun dir sub_822F0F90; manager 0x8288D0A0. Night: sub_8230DDD0 (:45385, night/color+normal).
- Period 49 = impostor count; ~28 have a shadow RT.

Logs:
- Same pool 0317C000-03566000: night-only fmt3 128x128 src_base 288 (350) + fmt6 128x128 (197);
  day-only fmt2 128/256 src_base 224 (443). Night and day impostor RTs alias the same guest memory.
- All family tex_loads scaled_resolve=1 in both sessions (sampled: tex_load ~99.9% suppressed).
- 0 CPU (by_gpu=0) tex_inval in family range logged (inj 0, watch 0; minute totals 12-27 CPU invals elsewhere).
- Inject session F10s 0.4-0.9 s after 06:00 in restarts 2,4,5,6,8,9 (matches owner).
- Livery-range activity (bake, RB reads) is 4-8 s after 06:00, not at the blink.

Host:
- Scaled resolve writes only the scaled buffer (render_target_cache.cpp:1406-1445), MarkRangeAsResolved
  bumps fetch-memo generation + RangeWrittenByGpu (cache.cpp:318-366).
- Delivery memcpy uses TranslatePhysical (command_processor.cpp:5699-5707, 3460, 3750): no watch fault,
  no shared-memory re-upload (pages valid+gpu-written).
- Age inject (readback_slots.h:218-247): changes only copies_now/cadence/next_resolves_since_copy/
  age_due; record_gpu stays true (:255-258); no texture, validity, slot or submission side effect.
- Write-watch re-protect does not drop the read watch (xmemory.cpp EnableAccessCallbacks protects
  only pages without notify bit; arm refresh :807-826).

## Control run (2026-11-12, evening)

CONTROL 2026-11-12 (l94 arm dawn = strict ON, no inject; session 22fbf76c, verified: d3d12_frame_lead_strict=1, 8474 strict waits, lead2 0, f2p 0, rb_age_injected 0): trees blinked in 4 of 5 restarts (1,2,3,5). So the tree blink does not depend on the strict bound, on read-back freshness or on the frame lead: all arms together about 23 of 37. Owner: it is not exactly at 06:00 but about a second after, and it feels like the game's own behaviour. Consistent with the RE: all tree shadow impostors are re-rendered on the first day frame and then one per frame round-robin with a period of 49 frames (about one second). Treated as game behaviour until shown otherwise; the earlier 0/3 on this arm was chance. Also in that session the owner saw the ground drop to a low LOD for about a second at the start of the first attempt; the log has no KI-035 forced-finish line in that session, so that was not our warp fix.
