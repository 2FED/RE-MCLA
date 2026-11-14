# l100 PC: Reflex-only fast run, cp-verify results and the per-draw lever redesign (2026-11-14)

Read-only analysis by an Opus subagent, accepted by the primary agent; proven and inferred points are marked in the body; the savings are estimates, not measurements. Spark not used.

**Headline.** Owner runs on l100 (2026-11-14): pc/reflex-nostrict, 10 min of fast driving: no flicker; at equal draw counts vs the morning default: ~4400 draws 49.6 vs 48.8 fps, ~5800: 45.1-45.4 vs 44.6, ~6200: 42.6 vs 42.1, ~7200: 41.3 vs 37.7 (the strict wait of 3-3.7 ms/frame is inherent to strict even when CP-bound; Reflex removes it); lead2 17, f2p 58. pc/cp-verify: reg_side_mismatch 0; cb_mismatch 35,241 of 32.4 M would-be-saved uploads; draws qualifying for the strict constants-only path only 3.4 % (63 % change vertex fetch constants, 31 % other state), comparators all 0. Analysis: the 35 k cb mismatches are most likely a FALSE POSITIVE of the verifier (a stage with no float constants uploads empty data while the shadow is not marked; such a draw reads no constants), not a skip bug: to be decided by splitting the counter by kind and size class; until then cp-reg is NOT to be run. Per-draw cost ranking from 78 windows: texture binding memo misses dominate p_tex (512-entry direct-mapped memo, hit rate 52-66 % and falling with draws; every texture creation bumps the global generation unnecessarily): memo v2 expected -2.0 ms at 7.2 k draws; render-target Update memo -1.1 ms (rt unchanged on 99.5 % of draws); register skip -0.8 ms; PSO reuse -0.4 ms. GPU busy ~21 ms at 7.2 k draws caps a dip at about 47 fps.

---

# l100 verify analysis notes (2026-11-14)

## Evidence preserved (robocopy /IPG:5, save-private / save-files-private.json / *.dmp / before-user / after-user excluded)
- private/owner-l100-20261114/{diagnostics,sessions}/5f69c8b7a8b831fc35e5b4f7c7dcff65  = arm pc/cp-verify (cp_draw_fast_verify + cp_reg_skip_verify), 13:10-13:19
- private/owner-l100-20261114/{diagnostics,sessions}/63fec055abbda333637fe99955d4486c  = arm pc/reflex-nostrict (low_latency_reflex + frame_lead_strict_off), 11:48-11:58
- (34aec854... from 11:11 was already present, copied earlier by primary)
- private/owner-hwinfo-20261114/: HWiNFO_LOG_771613000.CSV (11:58), HWiNFO_LOG_776469687.CSV (13:19); proc-sampler-20261114-114820.csv, proc-sampler-20261114-130916.csv (from the launcher folder)

## Scripts (this folder)
parse.py (per-window ratios), fit.py (pure-python least squares of phase ms/frame), memo.py (memo hit vs generation bumps).

## Findings
- cb_mismatch: verifier false-positive class. ConstantSkipVerifier::OnUpload (cp_l100_levers.h:199-213) compares size+bytes with the previous upload of the kind. A stage with no float constants changes current map without marking (command_processor.cpp:8313 guard `if (float_constant_count_vertex)`, 8327 pixel, 8334 memset for null PS; PS nulled at 5007-5008 when not needed). If the real buffer was marked by a would-skip write (2280/2288 marks real, shadow not), the empty upload (gather size 0, 8414-8417 / 8377) != previous non-empty bytes -> kMismatch, though the draw reads no float constants and the next non-empty map marks both (8309-8316). No hidden constant store found: MMIO GraphicsSystem::WriteRegister r<0x4000 (graphics_system.cpp:350-364); all PM4 constant paths go through WriteRegistersFromMem/WriteRegisterSkipUnchanged (command_processor.cpp:691-766, 1883-1969).
- Phase fits (78 windows, 3 sessions): p_tex = 0.20 us x memo_hit + 0.87 us x memo_miss + 0.033 us x draws (r=0.982); p_rt 0.188 us/draw; p_pso 0.094; p_vb 0.176; p_bind ~0.46 us/draw (verify run 0.64: verify overhead ~0.18 us/draw); upload 2.12 us/upload_call.
- Memo: direct-mapped 512 entries (fetch_identity.h:140 kLog2Entries=9; cache.cpp:226), global generation bumped on every texture creation (cache.cpp:1115, "conservative"), 4.2 bumps/frame ~ creations; hit 52-66 %, falls with draws, weakly with bumps.
- Reflex-nostrict: fence_wait 0.2-0.5 ms/frame in dips vs 3.0-3.7 default.
