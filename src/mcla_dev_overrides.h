#pragma once

// Owner-internal developer overrides (M10 l57 CPU A/B,
// docs/evidence/M10-l57-cpu-ab-build-20261101.md; M10-002 l58 read-back
// elision A/B, docs/evidence/M10-002-readback-elision-build-20261101.md;
// M10-004 l59 Vulkan Deck A/B,
// docs/evidence/M10-004-vulkan-deck-ab-build-20261101.md; M10-003 shader
// store seeding switch, docs/evidence/M10-prewarm-union-store-20261101.md;
// KI-047 targeted read-back wait A/B,
// docs/evidence/M10-ki047-targeted-wait-20261101.md; M10 watch census and
// steps 1-2 of the Deck watch/upload path,
// docs/evidence/M10-watch-steps0-2-20261101.md; M10 watch step 3 hot blocks,
// docs/evidence/M10-hot-blocks-sdk-20261103.md and
// docs/evidence/M10-hot-blocks-host-keys-20261103.md).
//
// Builds made with the CMake option MCLA_DEV_OVERRIDES (OFF by default; ON
// only for owner-internal measurement builds, never for volunteer builds) read
// the per-user file %LocalAppData%\MCLA-R-Launcher-v1\mcla-dev-overrides.txt
// once at game start, because the launcher cannot pass extra command-line
// flags. The file holds lines "name=true" or "name=false":
//
// - at most kMaxBytes (4 KiB since l81) bytes, ASCII only (no BOM); otherwise
//   the whole file is rejected and nothing is applied (the launcher parses the
//   same file with the same Parse, so the limit is the same there);
// - lines end in LF or CRLF; empty lines and lines starting with '#' are
//   skipped;
// - a line must be exactly name=true or name=false (no spaces); the name must
//   be one of kKeys. Malformed lines, unknown names and repeated names (the
//   first occurrence wins) are ignored and logged by line number.
// - shared_memory_upload_ahead_16 and shared_memory_upload_ahead_64 are bool
//   convenience keys for the integer SDK cvar shared_memory_upload_ahead_pages
//   (0 = off, 16 or 64): =true writes 16 or 64, =false changes nothing. Both
//   true in one file is contradictory: neither is applied (logged as a
//   conflict on the later line). Integer values are deliberately not accepted
//   in the file; any other value needs the command line.
// - shared_memory_hot_blocks_shadow and shared_memory_hot_blocks_on are the
//   same kind of pair for the integer SDK cvar shared_memory_hot_blocks
//   (0 = off, 1 = shadow, 2 = on): =true writes 1 or 2, =false changes
//   nothing, both true is a conflict (neither applied). The other hot-block
//   cvars (K, N, upload cap, probe frames) need the command line.
// - mcla_warp_probe (KI-035) is a plain host bool. mcla_warp_watchdog is a
//   bool convenience key for the integer host cvar mcla_warp_watchdog_seconds
//   (0 = off): =true writes 15, =false changes nothing. Other limits need the
//   command line.
// - mcla_warp_finish_fix_off=true writes false to the host bool cvar
//   mcla_warp_finish_fix (KI-035 finish-race fix, default on) for the A/B
//   control arm; =false changes nothing.
// - mcla_texture_budget_soft_cap_off=true writes 0 (off) to the host integer
//   cvar mcla_texture_budget_soft_cap_mb (M10-007 / KI-048 A/B); =false changes
//   nothing.
// - mcla_album_probe (KI-056 MCLA_ALBUM lines) and mcla_pursuit_probe (KI-066
//   MCLA_PURSUIT lines) are plain host init-only bools (M11-002,
//   docs/evidence/M11-002-diagnostics-triad-20261106.md).
// - mcla_album_preview_restore (KI-056 fix A) is a host init-only bool
//   (mcla_album_probe.cpp, docs/evidence/M11-ki056-fix-a-20261106.md); default
//   off until l80, on since l81 (owner decision 2026-11-08,
//   album::kPreviewRestoreDefault): =true writes true (the default now),
//   =false changes nothing (so an old arm line =false no longer turns the fix
//   off); the control is album_preview_restore_off below.
// - xam_content_stable_creation_time_off=true writes false to the SDK bool
//   cvar xam_content_stable_creation_time (M11-001 / KI-068 stable save
//   creation time, SDK default true; the Deck A/B control arm), and
//   xam_content_trace_off=true writes false to the SDK bool cvar
//   xam_content_trace (KI-068 content trace lines, SDK default true); =false
//   changes nothing. Both are rexruntime cvars, registered before startup.
// - KI-071 cut-scene audio A/B (docs/evidence/M11-ki071-cutscene-audio-analysis-20261106.md):
//   audio_speaker_config_stereo=true and audio_speaker_config_surround=true
//   write stereo or surround to the SDK string cvar audio_speaker_config
//   (default auto: follow the host playback device). Both true in one file is a
//   conflict (neither applied), as for the integer pairs. =false changes
//   nothing. audio_downmix_itu_off=true writes false to the SDK bool cvar
//   audio_downmix_itu (ITU Lo/Ro stereo downmix, default true; the A/B control
//   arm restores the legacy weights). audio_channel_rms_probe is the plain SDK
//   bool (1 Hz per-channel RMS log line). All are rexruntime cvars registered
//   before startup; an SDK without them logs the keys as unavailable.
// - M11-015 read-back sync kick-interval A/B
//   (docs/evidence/M11-cp-cost-plan-20261106.md item 1): readback_kick_256,
//   readback_kick_128 and readback_kick_1024 are bool convenience keys for the
//   integer SDK cvar d3d12_readback_sync_kick_draws (graphics plugin,
//   hot-reload; SDK default 512): =true writes 256, 128 or 1024, =false
//   changes nothing (all three false is the 512 default arm). The three form a
//   conflict group of three pairs: two or more true in one file is
//   contradictory and none of the true ones is applied.
// - l73 owner A/Bs (graphics plugin, hot-reload SDK bools):
//   fetch_identity_off=true writes false to d3d12_fetch_identity_cache
//   (M11-016 fetch-constant identity cache, SDK default true; the A/B control
//   arm, docs/evidence/M11-016-fetch-identity-cache-20261106.md);
//   fetch_identity_verify=true writes true to d3d12_fetch_identity_verify (the
//   cache's verify mode, SDK default false; may be combined with the cache on);
//   readback_steady_exempt=true writes true to
//   d3d12_readback_sync_steady_exempt (M11-007a steady-key exemption, SDK
//   default false until l77, true since the l78 SDK, so the key's true now
//   writes the default; docs/evidence/M11-007a-steady-key-exemption-20261106.md).
//   =false changes nothing for all three. They write three different cvars;
//   the steady key pairs only with its l78 control readback_steady_exempt_off.
// - KI-071 XMA doubling fixes (docs/evidence/M11-ki071-xma-doubling-re-20261106.md,
//   SDK branch mcla/m11-xma-doubling): xma_swap_carry_packet (C1: carry the
//   packet over an input-buffer swap for multi-stream waves),
//   xma_valid_only_when_written (C2), xma_work_lock_recheck (C3) and
//   xma_swap_trace (swap trace lines) are plain SDK rexruntime bools (default
//   false), registered before startup: =true writes true, =false changes
//   nothing (so an SDK default stays visible as the default).
// - KI-071 step 2 (docs/evidence/M11-ki071-xma-sibling-trace-sdk-20261107.md,
//   SDK branch mcla/m11-xma-sibling): xma_sibling_trace (sibling group and
//   MCLA_XMA_SIB lines), xma_stall_on_invalid_next (D2),
//   xma_honor_start_skip (D1) and xma_reset_codec_on_clear are plain SDK
//   rexruntime bools (default false) of the same kind: =true writes true,
//   =false changes nothing. mcla_xma_consume_probe is a plain host init-only
//   bool (MCLA_XMA_CONSUME lines, mcla_xma_consume_probe.cpp,
//   docs/evidence/M11-ki071-xma-consume-probe-20261107.md).
// - KI-071 step 3 (docs/evidence/M11-ki071-xma-frame-trim-sdk-20261107.md,
//   SDK branch mcla/m11-xma-trim; host keys
//   docs/evidence/M11-ki071-xma-dump-keys-20261107.md): xma_honor_frame_trim,
//   xma_frame_trim_block_granular and xma_dump_all_rates are plain SDK
//   rexruntime bools (default false) of the same kind: =true writes true,
//   =false changes nothing. The SDK cvar xma_dump_contexts is a string (a
//   directory), which a name=true|false line cannot carry:
//   xma_dump_contexts_on=true makes the host create the directory
//   <diagnostics>/xma-dump/<yyyyMMdd-HHmmss> (XmaDumpDirectory below; UTC
//   start time) and write its path to xma_dump_contexts, logged once;
//   =false changes nothing.
// - KI-071 step 4 (docs/evidence/M11-ki071-xma-frame-walk-sdk-20261107.md,
//   SDK branch mcla/m11-xma-walk): xma_frame_walk_trace (walk anomaly lines),
//   xma_frame_walk_fix (W1 split-header claim and dup guard) and
//   xma_dump_input (input packet / walk dump next to each PCM dump; needs
//   xma_dump_contexts_on) are plain SDK rexruntime bools (default false) of
//   the same kind: =true writes true, =false changes nothing. Since the l77
//   SDK (f6fc975) xma_frame_walk_fix defaults to true: the key's true now
//   writes the default; the control is xma_frame_walk_fix_off below.
// - M11-007d / KI-047 (src/mcla_lock_probe.cpp): mcla_lock_probe (MCLA_LOCK
//   lines for guest locks overlapping a read-back key) and
//   mcla_lock_readback_ensure (complete a pending read-back copy before the
//   lock returns) are plain host init-only bools: true/false written as is.
// - l77 (docs/evidence/M11-007e-deferred-fences-sdk-20261107.md):
//   fence_write_deferred=true writes true to d3d12_fence_write_deferred
//   (M11-007e deferred EVENT_WRITE_SHD fence writes, graphics plugin,
//   hot-reload, SDK default false until l77, true since the l78 SDK; the
//   control is fence_write_deferred_off below); =false changes nothing.
//   xma_frame_walk_fix_off=true writes false to xma_frame_walk_fix (the
//   KI-071 W1 walk fix, SDK default true since l77; the upstream-walk control
//   arm, before the guest); =false changes nothing. xma_frame_walk_fix=true
//   together with xma_frame_walk_fix_off=true is contradictory (a conflict
//   pair: neither is applied).
// - l77 KI-074 experiments (docs/evidence/M11-ki074-framerate-re-20261107.md
//   sections 5 and 6): raw_timer_off=true writes false to the host init-only
//   mcla_raw_timer (mcla_raw_timer.cpp, read on the first guest timer tick,
//   so the startup write is in time): the stock fixed 1/30 s step, E1,
//   diagnostic only (the game runs about 1.35x fast at 40 fps); =false
//   changes nothing. vblank_hz_72, vblank_hz_80 and vblank_hz_88 write 72,
//   80 or 88 to the SDK int mcla_guest_vblank_hz (graphics plugin,
//   graphics_system.cpp, hot-reload, read on every vblank tick): frame caps
//   of 36, 40 and 44 fps (E2); =false changes nothing. The three form a
//   conflict group like the kick keys. The launcher's Unlocked mode passes
//   --mcla_guest_vblank_hz=240 on the command line, so these three keys are
//   marked overrides_user: they replace a command-line or config value
//   (logged with the value replaced) instead of yielding to it.
// - l78 (docs/evidence/M11-007e-l77-fence-analysis-20261107.md section 5): the
//   l78 SDK turns d3d12_readback_sync_steady_exempt and
//   d3d12_fence_write_deferred on by default and adds the int
//   d3d12_fence_defer_min_gap_us (default 0). readback_steady_exempt_off=true
//   writes false to d3d12_readback_sync_steady_exempt and
//   fence_write_deferred_off=true writes false to d3d12_fence_write_deferred
//   (the A/B controls, graphics plugin, hot-reload); each forms a conflict
//   pair with its l73 / l77 on key (both true: neither applied).
//   fence_defer_gap_2000=true writes 2000 to d3d12_fence_defer_min_gap_us
//   (graphics plugin); =false changes nothing for all three.
// - l79 (docs/evidence/M11-l79-sdk-20261108.md; all four SDK cvars default
//   off, so =false changes nothing): xma_canary_fixes=true writes true to the
//   SDK rexruntime bool xma_canary_fixes (Xenia Canary XMA decoder fixes, read
//   on every XMA Work pass) and timer_queue_blocking_wait=true writes true to
//   the SDK rexruntime bool timer_queue_blocking_wait (the timer queue thread
//   blocks on a high-resolution waitable timer instead of spinning; read on
//   every dispatch pass), both at startup. texture_placed_heaps=true writes
//   true to d3d12_texture_placed_heaps and texture_heap_64mb=true writes 64 to
//   the int d3d12_texture_heap_block_mb (256 by default); both are registered
//   by the graphics plugin and are restart-only: D3D12TextureCache::Initialize
//   latches them when the runtime sets up the guest GPU, which follows
//   OnPostLoadGraphicsPlugin (ReXApp::SetupPresentation precedes
//   ConstructRuntime), so the graphics-plugin phase is in time.
//   texture_heap_64mb alone (without texture_placed_heaps) has no effect.
//   Since the l80 SDK the platform defaults turn xma_canary_fixes and
//   timer_queue_blocking_wait on everywhere and d3d12_texture_placed_heaps on
//   on Windows (off under Wine/Proton), so these keys' true then writes the
//   default there; the controls are the l80 keys below.
// - l80 (docs/evidence/M11-l80-sdk-20261108.md): blend_minmax_fix=true writes
//   true to the SDK bool d3d12_blend_minmax_fix (KI-013 MIN/MAX blend source
//   factor, default false; graphics plugin, read for every pipeline's pixel
//   shader modification, so a new modification key compiles new pipelines).
//   short_sleep_native=true writes native to the SDK string
//   ke_delay_short_sleep_mode (KI-060, latched at the first guest delay;
//   platform default native on Windows, hires_timer under Wine/Proton) and
//   apu_priority_normal=true writes normal to the SDK string
//   apu_worker_priority (read when the audio worker and XMA decoder threads
//   are created; platform default normal on Windows, above_normal under
//   Wine/Proton), both at startup: the Deck control arm for the l80 KI-060
//   defaults. texture_placed_heaps_off=true writes false to
//   d3d12_texture_placed_heaps (graphics plugin, restart-only like
//   texture_placed_heaps) and timer_blocking_off=true writes false to
//   timer_queue_blocking_wait (startup): the controls for the l80 platform
//   defaults; each forms a conflict pair with its l79 on key (both true:
//   neither applied). =false changes nothing for all five.
// - l81 (docs/evidence/M11-l80-deck-analysis-20261108.md section 4; owner
//   decisions 2026-11-08; fix design section 3): zero_delay_native=true
//   writes native to the SDK string ke_delay_zero_mode (KI-060 non-alertable
//   zero-duration guest delays, the F800068C Sleep(0) loop; platform default
//   backoff under Wine/Proton, native on Windows) and
//   guest_audio_prio_normal=true writes normal to the SDK string
//   guest_audio_thread_priority (the host priority of the guest audio engine
//   thread F8000534, set when that guest thread is created), both rexruntime
//   cvars at startup: the Deck control arm for the l81 KI-060 defaults. blend_minmax_fix_off=true
//   writes false to d3d12_blend_minmax_fix (KI-013 fix, default on since the
//   l81 SDK; graphics plugin): the control, a conflict pair with
//   blend_minmax_fix. album_preview_restore_off=true writes false to the host
//   bool mcla_album_preview_restore (KI-056 fix A, default on since l81;
//   startup): the control, a conflict pair with mcla_album_preview_restore.
//   =false changes nothing for all four; an SDK without the l81 cvars logs
//   the two KI-060 keys as unavailable.
// - l82 (docs/evidence/M11-l82-sdk-bulk-registers-20261108.md): cp_bulk_regs=true
//   writes true to the SDK bool cp_bulk_register_writes (CP bulk register
//   range writes, default false; registered by the graphics plugin, hot
//   reload, latched at setup and every frame start): the A/B arm for the
//   packets_other cost. =false changes nothing; an SDK without the l82 cvar
//   logs the key as unavailable.
// - l83 (docs/evidence/M11-ki054-l83-implementation-20261110.md; KI-054 design
//   docs/evidence/M11-ki054-traffic-jitter-design-20261110.md):
//   ambient_dt_probe=true writes true to the host init-only bool
//   mcla_ambient_dt_probe (MCLA_AMBDT lines and the F10 "ambient_dt" block,
//   read-only), ambient_dt_snapshot=true to mcla_ambient_dt_snapshot (the
//   ambient-traffic thread gets the kick-time frame delta at its two
//   register-visible reads) and raw_timer_hitch_hold=true to
//   mcla_raw_timer_hitch_hold (l85 v2: the step moves at most 10 % per update,
//   rest to the timer debt), all at startup; the two probes default off, the
//   hitch hold off until l85 and on since l86 (see l86 below). =false changes
//   nothing (so an old arm line raw_timer_hitch_hold=false no longer turns the
//   hold off). A build without mcla_raw_timer.cpp (the timer configuration
//   probe build) logs raw_timer_hitch_hold as unavailable.
// - l84 (docs/evidence/M11-l84-owner-build-20261110.md): raw_timer_stats=true
//   writes true to the host init-only bool mcla_raw_timer_stats (one
//   "MCLA timer stats" line per 300 raw timer updates, with the hitch count
//   and the hitch hold state), at startup and default off. =false changes
//   nothing; no conflict pair. ambient_dt_snapshot stays a valid key but is
//   inert in retail (the ambient worker it serves is never started).
// - l86 (docs/evidence/M11-l86-owner-build-20261110.md; owner decision
//   2026-11-10): the KI-054 hitch hold is default on
//   (raw_timer::kHitchHoldDefault). raw_timer_hitch_hold_off=true writes false
//   to the host init-only bool mcla_raw_timer_hitch_hold at startup: the
//   control, a conflict pair with raw_timer_hitch_hold (which still writes
//   true, the default now). =false changes nothing.
// - l87 (docs/evidence/M11-l87-owner-build-20261110.md; KI-060 Deck audio):
//   audio_filter_probe=true writes true to the host init-only bool
//   mcla_audio_filter_probe (one MCLA_AFILT line per second of XAudio engine
//   frames and the F10 "audio_filter" block, read-only),
//   audio_filter_native=true to mcla_audio_filter_native (the two-bank voice
//   filter sub_82146BE0 run natively with the recompiled arithmetic) and
//   audio_filter_native_verify=true to mcla_audio_filter_native_verify (one
//   call in 64 runs the original and is compared bit for bit), all at startup
//   and default off. =false changes nothing; no conflict pairs.
// - l88 (docs/evidence/M11-l88-sdk-20261110.md; KI-074 flicker):
//   readback_fenced_delivery=true writes true to the SDK bool
//   d3d12_readback_fenced_delivery (default false; registered by the graphics
//   plugin, hot reload): an every-4th read-back copy whose slot has not
//   completed is not made, completed copies reach guest memory before their
//   fences. =false changes nothing; an SDK without the l88 cvar logs the key
//   as unavailable; no conflict pair.
// - l89 (docs/evidence/M11-ki060-l89-implementation-20261110.md; KI-060):
//   audio_filter_native now also runs the per-channel filter sub_82147738
//   natively (both kernels without the library fma) and
//   audio_filter_native_verify checks both; audio_filter_native_ch_off=true
//   writes true to the host init-only bool mcla_audio_filter_native_ch_off
//   at startup (the control: sub_82147738 stays recompiled). Default off;
//   =false changes nothing; no conflict pair.
// - l90 (docs/evidence/M11-l90-sdk-20261111.md; KI-074 flicker):
//   frame_lead_limit_1=true writes 1 to the SDK int d3d12_frame_lead_limit
//   (default 0; registered by the graphics plugin, hot reload): a guest frame
//   opens only when at most one earlier frame is still on the GPU (frame F
//   waits, bounded, for frame F-2). readback_deferred_release=true writes true
//   to the SDK bool d3d12_readback_deferred_release (default false, graphics
//   plugin, hot reload): read-back slot buffers released by slot growth or
//   eviction go through the deferred deletion. =false changes nothing; an SDK
//   without the l90 cvars logs the key as unavailable; no conflict pairs.
// - KI-026 (docs/evidence/M11-ki026-implementation-20261111.md; Race Back
//   camera softlock): mcla_warp_stall_fix=true writes true to the host
//   init-only bool of the same name (default false: the state-19 callback
//   exit and the state-19/20 stall poll); =false writes nothing, so an arm
//   line =false stays a no-op if the default is flipped later (the control
//   would then be a separate _off key). mcla_ki026_probe is a plain host
//   init-only bool (the KI-026 edge / printf / warper-state log, formerly
//   command-line only); mcla_ki026_watchdog stays command-line only.
// - l91 (docs/evidence/M11-l91-sdk-20261111.md; KI-074 flicker):
//   frame_lead_strict=true writes 1 and frame_lead_adaptive=true writes 2 to
//   the SDK int d3d12_frame_lead_strict (default 0; registered by the graphics
//   plugin, hot reload). 1: a guest frame opens only when the previous frame
//   has completed on the GPU (frame F waits, bounded, for frame F-1), so the
//   frame-end lead gauge stays at 1 and no guest fence is 2+ frames late. 2:
//   the same wait only while armed by an imminent lead of 2. =false changes
//   nothing; an SDK without the l91 cvar logs the key as unavailable. The two
//   keys write one cvar, so they form a conflict pair; neither conflicts with
//   frame_lead_limit_1 (another cvar; strict implies limit 1).
// - l92 (docs/evidence/M11-l92-sdk-20261111.md; KI-074 / KI-076 probes),
//   all SDK cvars of the graphics plugin phase, default off, =false writes
//   nothing, no conflict pairs (each key writes its own cvar):
//   readback_read_watch=true writes true to d3d12_readback_read_watch (names
//   the guest reader of read-back bytes, MCLA_RB_READ lines);
//   readback_age_inject_2=true writes 2 to d3d12_readback_age_inject (due
//   cadence copies of non-exempt keys held 2 frames);
//   occlusion_query_bounded_4=true writes 4 to
//   d3d12_occlusion_query_bounded_ms (the occlusion query drain bounded to
//   4 ms); present_stats=true writes true to d3d12_present_stats
//   (MCLA_PRESENT_STATS lines); present_interval_1=true writes 1 to
//   d3d12_present_sync_interval; locked_tearing_off=true writes true to
//   mcla_locked_tearing_off (no ALLOW_TEARING in Locked either). An SDK
//   without the l92 cvars logs the keys as unavailable.
// - l93 (docs/evidence/M11-l93-time-of-day-control-20261112.md; time of day),
//   host init-only ints at startup, default off, =false writes nothing:
//   tod_rate_x10=true writes 10, tod_rate_x30=true 30 and tod_freeze=true 0
//   to mcla_tod_rate (default 1: the game's own rate; the control acts only
//   after an Arcade race preset warp in this session); tod_start_0430=true
//   writes 270 and tod_start_0530=true 330 to mcla_tod_start_minutes
//   (default 0: the Arcade race preset warp target becomes 04:30 / 05:30).
//   Keys of one cvar form conflict pairs (the three rate keys as a group of
//   three, the two start keys); a rate key and a start key combine.
// - l94 (docs/evidence/M11-l94-owner-build-20261112.md; owner decision
//   2026-11-12, KI-074): the strict frame lead bound is default on through
//   the m10 defaults (mcla_m10_defaults.h writes d3d12_frame_lead_strict=1 at
//   graphics-plugin load; the SDK default stays 0). frame_lead_strict_off=true
//   writes 0 to d3d12_frame_lead_strict after the plugin has loaded: the
//   control (the pre-l94 behaviour). frame_lead_strict (1, now the default)
//   and frame_lead_adaptive (2) stay; the three keys of the one cvar form a
//   group of three conflict pairs. =false changes nothing for all three (no
//   =false line turns the default off).
// - l95 (docs/evidence/M11-l95-sdk-low-latency-20261112.md; NVIDIA Reflex /
//   AMD Anti-Lag 2, default off): rexruntime cvars registered before
//   startup, read once at the first frame. low_latency_reflex=true,
//   low_latency_antilag2=true and low_latency_auto=true write reflex,
//   antilag2 and auto to the SDK string d3d12_low_latency_mode (a group of
//   three conflict pairs); low_latency_markers=true writes true to
//   d3d12_low_latency_markers_only (markers and the latency report, no
//   sleep); low_latency_boost=true writes true to d3d12_low_latency_boost.
//   fma_bench=true writes true to the host init-only bool mcla_fma_bench (the
//   FMA stage 0 start-up benchmark line). =false writes nothing for all six.
//   An SDK without the l95 cvars logs the five SDK keys as unavailable.
// - l96 (docs/evidence/M11-l96-sdk-measurement-20261113.md; Deck CP lever 1,
//   default off): SDK graphics-plugin cvars (hot reload, latched at every
//   frame start). cp_fence_poll_64=true writes 64 to the int
//   cp_fence_poll_interval (the deferred fence service at packet boundaries
//   and draws polls every 64 points or 20 us); cp_draw_packet_tsc=true,
//   shader_im_load_memo=true and shader_im_load_memo_verify=true write true
//   to the SDK bools of the same names. =false writes nothing for all four;
//   no conflict pairs (the verify key only acts with the memo key). An SDK
//   without the l96 cvars logs the four keys as unavailable.
// - l97 (docs/evidence/M11-l97-sdk-20261113.md; low latency replaces strict,
//   default off): SDK graphics-plugin int d3d12_frame_lead_strict_low_latency
//   (hot reload, read at every frame open). lead_ll_off_when_reflex=true
//   writes 1 (while the Reflex sleep is active the frame's strict mode is
//   off), lead_ll_adaptive_when_reflex=true writes 2 (adaptive instead); a
//   conflict pair. Whenever the sleep is not active the configured
//   d3d12_frame_lead_strict (the m10 default 1) applies, and a lead-2 guard
//   latches it back for the session. Both act only with a low_latency_* mode
//   key that starts Reflex. =false writes nothing. An SDK without the l97
//   cvar logs the two keys as unavailable.
// - l98 (docs/evidence/M11-l98-owner-build-20261113.md; owner verdicts on the
//   l96 Deck runs, docs/evidence/M11-l96-deck-analysis-20261113.md): three
//   defaults on. The native voice filters are default on in the host itself
//   (audio_filter::kNativeDefault, both platforms); audio_filter_native_off=true
//   writes false to mcla_audio_filter_native at startup: the control, a
//   conflict pair with audio_filter_native (which still writes true, the
//   default now). CP lever 1 is default on through the m10 defaults
//   (mcla_m10_defaults.h writes cp_draw_packet_tsc=true and
//   cp_fence_poll_interval=64 at graphics-plugin load; the SDK defaults stay
//   false / 0): cp_draw_packet_tsc_off=true writes false to cp_draw_packet_tsc
//   and cp_fence_poll_off=true writes 0 to cp_fence_poll_interval, each a
//   conflict pair with its l96 key. =false writes nothing for all three (no
//   =false line turns a default off). The IM_LOAD memo stays default off.
// - l99 (docs/evidence/M11-l99-sdk-hitch-20261113.md; texture churn and the
//   Reflex guard actions, all default off): texture_hard_margin_256=true
//   writes 256 to the host init-only int mcla_texture_hard_margin_mb at
//   startup (the budget monitor keeps the hard limit at least 256 MB above
//   the soft limit); texture_min_age_2000=true writes 2000 to the SDK
//   graphics-plugin int texture_cache_min_age_ms (hot reload; above the hard
//   limit a texture used within 2 s is not destroyed); lead_ll_guard_adaptive,
//   lead_ll_guard_rearm and lead_ll_guard_off write 1, 2 and 3 to the SDK int
//   d3d12_frame_lead_strict_low_latency_guard_action (a guard trip makes the
//   relaxed frames adaptive for the session / latches strict until 5400 clean
//   frames / observe only: the guard never changes the mode), a conflict
//   group of three. =false writes nothing for all five. An SDK without the
//   l99 cvars logs the four SDK keys as unavailable.
// - l100 (docs/evidence/M11-l100-sdk-deck-levers-20261114.md; command
//   processor levers and the guest spin backoff, all default off):
//   cp_reg_skip=true writes true to the SDK graphics-plugin bool
//   cp_reg_write_skip_unchanged (unchanged register writes skip the
//   per-register path and do not mark constant buffers); cp_reg_skip_verify
//   writes true to cp_reg_write_skip_verify (normal path, proof counters;
//   wins over cp_reg_skip); cp_draw_fast_verify writes true to
//   cp_draw_const_only_verify (constants-only draw measurement, no fast
//   path); guest_spin_backoff_64=true writes 64 to the host int
//   mcla_guest_spin_backoff at startup. =false writes nothing for all four.
//   An SDK without the l100 cvars logs the three SDK keys as unavailable.
// - l101 (docs/evidence/M11-l101-sdk-per-draw-levers-20261114.md; per-draw
//   levers, all default off): tex_memo_v2=true writes true to the SDK
//   graphics-plugin bool d3d12_fetch_identity_v2 (4096-entry 2-way texture
//   binding memo without the creation bump; its verify is the existing
//   fetch_identity_verify key); rt_memo=true writes true to
//   d3d12_rt_update_memo (skip a no-op render-target update); rt_memo_verify
//   writes true to d3d12_rt_update_memo_verify (the full update always runs,
//   mismatch counters; wins over rt_memo). =false writes nothing for all
//   three. An SDK without the l101 cvars logs them as unavailable.
// - Conflict rule (all groups): every key of a true-true kConflictPairs pair
//   (evaluated on the file as read, so a group of three is resolved as a
//   whole) loses its value; one kConflict issue is logged for each of those
//   lines except the earliest of its group.
//
// Each accepted value is applied to its cvar unless that cvar already holds a
// non-default value (set by the command line or the config file; keys marked
// overrides_user replace that value instead), and logged once as
// "MCLA dev override: name=value". Host init-only cvars are applied at
// startup (before the guest runs); SDK cvars of the graphics plugin are applied
// when the plugin has been loaded (they are registered by the plugin DLL).
// Without the option nothing is read or written, and the single-line l55 file
// read of mcla_readback_sync.cpp stays as before.
//
// Phase::kLauncher keys are not cvars: the host parses and ignores them; the
// launcher (MCLA_DEV_OVERRIDES launcher builds) reads the same file before a
// session and applies them itself (prewarm_seed_store=false: no shader store
// seeding, for cold baselines).
//
// The parser and the apply decision are pure and unit-tested
// (tests/mcla_dev_overrides_test.cpp); file and cvar calls are in
// mcla_dev_overrides.cpp.

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace mcla::dev_overrides {

inline constexpr std::string_view kDirectory = "MCLA-R-Launcher-v1";
inline constexpr std::string_view kFile = "mcla-dev-overrides.txt";
// 4 KiB since l81 (2 KiB l73-l80, 1 KiB before): a full 71-key l80 arm (every
// key explicit) was about 2.02 KB, so an editor re-save with CRLF would have
// pushed it past 2 KiB and the whole file would have been rejected; a full
// 75-key arm with CRLF is about 2.3 KB. Since l81 arms list only the keys
// that differ from the defaults (private/l81-CHECKLIST.txt).
inline constexpr size_t kMaxBytes = 4096;

enum class Key : uint8_t {
  kReadbackSyncComposite,
  kNativeD3dRefcount,
  kNativeXapiInterlocked,
  kWrmEventWake,
  kReplayBatchedWake,
  kReadbackElideUnread,
  kGpuBackendVulkan,  // M10-004 Deck A/B: native Vulkan for the session
  kPrewarmSeedStore,  // M10-003: launcher shader store seeding (default on)
  kReadbackSyncTargeted,  // KI-047 cost: targeted read-back wait + early submit
  kWatchCensus,           // M10 watch census counters (SDK default on)
  kWatchProbes,           // M10 watch F10 probes (SDK default off)
  kWatchDisarmBridge,     // M10 step 1: disarm safe-gap bridging
  kUploadAhead16,         // M10 step 2: shared_memory_upload_ahead_pages=16
  kUploadAhead64,         // M10 step 2: shared_memory_upload_ahead_pages=64
  kHotBlocksShadow,       // M10 step 3: shared_memory_hot_blocks=1 (shadow)
  kHotBlocksOn,           // M10 step 3: shared_memory_hot_blocks=2 (on)
  kWarpProbe,             // KI-035: warp state / camera logging
  kWarpWatchdog,          // KI-035: mcla_warp_watchdog_seconds=15
  kTextureSoftCapOff,     // M10-007: mcla_texture_budget_soft_cap_mb=0
  kWarpFinishFixOff,      // KI-035 fix A/B: mcla_warp_finish_fix=false
  kAlbumProbe,            // M11-002 / KI-056: album preview diagnostic
  kPursuitProbe,          // M11-002 / KI-066: police pursuit transition log
  kStableCreationTimeOff, // KI-068 A/B: xam_content_stable_creation_time=false
  kContentTraceOff,       // KI-068: xam_content_trace=false
  kAudioSpeakerStereo,    // KI-071 A/B: audio_speaker_config=stereo
  kAudioSpeakerSurround,  // KI-071 A/B: audio_speaker_config=surround
  kAudioDownmixItuOff,    // KI-071 A/B: audio_downmix_itu=false (legacy weights)
  kAudioChannelRmsProbe,  // KI-071: audio_channel_rms_probe (1 Hz RMS line)
  kAlbumPreviewRestore,   // KI-056 fix A: mcla_album_preview_restore (default on since l81)
  kReadbackKick256,       // M11-015 A/B: d3d12_readback_sync_kick_draws=256
  kReadbackKick128,       // M11-015 A/B: d3d12_readback_sync_kick_draws=128
  kReadbackKick1024,      // M11-015 A/B control: d3d12_readback_sync_kick_draws=1024
  kFetchIdentityOff,      // M11-016 A/B control: d3d12_fetch_identity_cache=false
  kFetchIdentityVerify,   // M11-016: d3d12_fetch_identity_verify=true
  kReadbackSteadyExempt,  // M11-007a A/B: d3d12_readback_sync_steady_exempt=true
  kXmaSwapCarryPacket,    // KI-071 C1: xma_swap_carry_packet=true
  kXmaValidOnlyWhenWritten,  // KI-071 C2: xma_valid_only_when_written=true
  kXmaWorkLockRecheck,    // KI-071 C3: xma_work_lock_recheck=true
  kXmaSwapTrace,          // KI-071: xma_swap_trace=true (swap trace lines)
  kXmaSiblingTrace,       // KI-071 step 2: xma_sibling_trace=true (group lines)
  kXmaStallOnInvalidNext,  // KI-071 D2: xma_stall_on_invalid_next=true
  kXmaHonorStartSkip,     // KI-071 D1: xma_honor_start_skip=true
  kXmaResetCodecOnClear,  // KI-071: xma_reset_codec_on_clear=true
  kXmaConsumeProbe,       // KI-071: mcla_xma_consume_probe (host, MCLA_XMA_CONSUME)
  kXmaHonorFrameTrim,     // KI-071 step 3: xma_honor_frame_trim=true
  kXmaFrameTrimBlockGranular,  // KI-071 step 3: xma_frame_trim_block_granular=true
  kXmaDumpAllRates,       // KI-071 step 3: xma_dump_all_rates=true
  kXmaDumpContextsOn,     // KI-071 step 3: xma_dump_contexts=<diagnostics>/xma-dump/<stamp>
  kXmaFrameWalkTrace,     // KI-071 step 4: xma_frame_walk_trace=true (walk lines)
  kXmaFrameWalkFix,       // KI-071 step 4: xma_frame_walk_fix=true (W1 fix)
  kXmaDumpInput,          // KI-071 step 4: xma_dump_input=true (input/walk dump)
  kLockProbe,             // M11-007d: mcla_lock_probe (host, MCLA_LOCK)
  kLockReadbackEnsure,    // M11-007d: mcla_lock_readback_ensure (host)
  kFenceWriteDeferred,    // M11-007e: d3d12_fence_write_deferred=true
  kXmaFrameWalkFixOff,    // KI-071 control: xma_frame_walk_fix=false (SDK default true)
  kRawTimerOff,           // KI-074 E1: mcla_raw_timer=false (stock fixed 1/30 s step)
  kVblankHz72,            // KI-074 E2: mcla_guest_vblank_hz=72 (36 fps cap)
  kVblankHz80,            // KI-074 E2: mcla_guest_vblank_hz=80 (40 fps cap)
  kVblankHz88,            // KI-074 E2: mcla_guest_vblank_hz=88 (44 fps cap)
  kReadbackSteadyExemptOff,  // l78 control: d3d12_readback_sync_steady_exempt=false
  kFenceWriteDeferredOff,    // l78 control: d3d12_fence_write_deferred=false
  kFenceDeferGap2000,        // l78 A/B: d3d12_fence_defer_min_gap_us=2000
  kXmaCanaryFixes,           // l79 A: xma_canary_fixes=true (Xenia Canary XMA fixes)
  kTimerQueueBlockingWait,   // l79 C: timer_queue_blocking_wait=true
  kTexturePlacedHeaps,       // l79 B: d3d12_texture_placed_heaps=true (restart-only)
  kTextureHeap64Mb,          // l79 B: d3d12_texture_heap_block_mb=64 (restart-only)
  kBlendMinmaxFix,           // l80 KI-013: d3d12_blend_minmax_fix=true
  kShortSleepNative,         // l80 KI-060 control: ke_delay_short_sleep_mode=native
  kApuPriorityNormal,        // l80 KI-060 control: apu_worker_priority=normal
  kTexturePlacedHeapsOff,    // l80 control: d3d12_texture_placed_heaps=false
  kTimerBlockingOff,         // l80 control: timer_queue_blocking_wait=false
  kZeroDelayNative,          // l81 KI-060 control: ke_delay_zero_mode=native
  kGuestAudioPrioNormal,     // l81 KI-060 control: guest_audio_thread_priority=normal
  kBlendMinmaxFixOff,        // l81 KI-013 control: d3d12_blend_minmax_fix=false
  kAlbumPreviewRestoreOff,   // l81 KI-056 control: mcla_album_preview_restore=false
  kCpBulkRegs,               // l82 A/B: cp_bulk_register_writes=true
  kAmbientDtProbe,           // l83 KI-054 Arm 0: mcla_ambient_dt_probe=true
  kAmbientDtSnapshot,        // l83 KI-054 Arm 1: mcla_ambient_dt_snapshot=true
  kRawTimerHitchHold,        // l83 KI-054 Arm 2: mcla_raw_timer_hitch_hold=true
  kRawTimerStats,            // l84 KI-054: mcla_raw_timer_stats=true
  kRawTimerHitchHoldOff,     // l86 KI-054 control: mcla_raw_timer_hitch_hold=false
  kAudioFilterProbe,         // l87 KI-060: mcla_audio_filter_probe=true
  kAudioFilterNative,        // l87 KI-060: mcla_audio_filter_native=true
  kAudioFilterNativeVerify,  // l87 KI-060: mcla_audio_filter_native_verify=true
  kReadbackFencedDelivery,   // l88 KI-074: d3d12_readback_fenced_delivery=true
  kAudioFilterNativeChOff,   // l89 KI-060: mcla_audio_filter_native_ch_off=true
  kFrameLeadLimit1,          // l90 KI-074: d3d12_frame_lead_limit=1
  kReadbackDeferredRelease,  // l90 KI-074: d3d12_readback_deferred_release=true
  kFrameLeadStrict,          // l91 KI-074: d3d12_frame_lead_strict=1
  kFrameLeadAdaptive,        // l91 KI-074: d3d12_frame_lead_strict=2
  kWarpStallFix,             // KI-026: mcla_warp_stall_fix=true
  kKi026Probe,               // KI-026: mcla_ki026_probe (plain bool)
  kReadbackReadWatch,        // l92 KI-074: d3d12_readback_read_watch=true
  kReadbackAgeInject2,       // l92 KI-074: d3d12_readback_age_inject=2
  kOcclusionQueryBounded4,   // l92: d3d12_occlusion_query_bounded_ms=4
  kPresentStats,             // l92 KI-076: d3d12_present_stats=true
  kPresentInterval1,         // l92 KI-076: d3d12_present_sync_interval=1
  kLockedTearingOff,         // l92 KI-076: mcla_locked_tearing_off=true
  kTodRateX10,               // l93: mcla_tod_rate=10
  kTodRateX30,               // l93: mcla_tod_rate=30
  kTodFreeze,                // l93: mcla_tod_rate=0
  kTodStart0430,             // l93: mcla_tod_start_minutes=270
  kTodStart0530,             // l93: mcla_tod_start_minutes=330
  kFrameLeadStrictOff,       // l94 KI-074 control: d3d12_frame_lead_strict=0
  kLowLatencyReflex,         // l95: d3d12_low_latency_mode=reflex
  kLowLatencyAntiLag2,       // l95: d3d12_low_latency_mode=antilag2
  kLowLatencyAuto,           // l95: d3d12_low_latency_mode=auto
  kLowLatencyMarkers,        // l95: d3d12_low_latency_markers_only=true
  kLowLatencyBoost,          // l95: d3d12_low_latency_boost=true
  kFmaBench,                 // l95 FMA stage 0: mcla_fma_bench=true
  kCpFencePoll64,            // l96 lever 1: cp_fence_poll_interval=64
  kCpDrawPacketTsc,          // l96 lever 1: cp_draw_packet_tsc=true
  kShaderImLoadMemo,         // l96 lever 1: shader_im_load_memo=true
  kShaderImLoadMemoVerify,   // l96 lever 1: shader_im_load_memo_verify=true
  kLeadLlOffWhenReflex,      // l97: d3d12_frame_lead_strict_low_latency=1
  kLeadLlAdaptiveWhenReflex, // l97: d3d12_frame_lead_strict_low_latency=2
  kAudioFilterNativeOff,     // l98 KI-060 control: mcla_audio_filter_native=false
  kCpDrawPacketTscOff,       // l98 lever 1 control: cp_draw_packet_tsc=false
  kCpFencePollOff,           // l98 lever 1 control: cp_fence_poll_interval=0
  kTextureHardMargin256,     // l99: mcla_texture_hard_margin_mb=256
  kTextureMinAge2000,        // l99: texture_cache_min_age_ms=2000
  kLeadLlGuardAdaptive,      // l99: d3d12_frame_lead_strict_low_latency_guard_action=1
  kLeadLlGuardRearm,         // l99: d3d12_frame_lead_strict_low_latency_guard_action=2
  kLeadLlGuardOff,           // l99: d3d12_frame_lead_strict_low_latency_guard_action=3
  kCpRegSkip,                // l100: cp_reg_write_skip_unchanged=true
  kCpRegSkipVerify,          // l100: cp_reg_write_skip_verify=true
  kCpDrawFastVerify,         // l100: cp_draw_const_only_verify=true
  kGuestSpinBackoff64,       // l100: mcla_guest_spin_backoff=64
  kTexMemoV2,                // l101: d3d12_fetch_identity_v2=true
  kRtMemo,                   // l101: d3d12_rt_update_memo=true
  kRtMemoVerify,             // l101: d3d12_rt_update_memo_verify=true
};
inline constexpr size_t kKeyCount = 131;

// When the cvar can be written: host init-only cvars before the guest starts,
// graphics-plugin cvars once the plugin has registered them. Launcher keys are
// read by the launcher only (no host cvar).
enum class Phase : uint8_t {
  kStartup,
  kGraphicsPlugin,
  kLauncher,
};

// Where the text written for true comes from: the literal true_text, or (one
// key) the per-session XMA dump directory composed by the runtime
// (XmaDumpDirectory), which a fixed text cannot express.
enum class TextSource : uint8_t {
  kLiteral,
  kXmaDumpDirectory,
};

struct KeyInfo {
  std::string_view name;
  Phase phase;
  // The cvar written (empty: the key's own name) and the text written for
  // true and false (empty: nothing is written for that value).
  std::string_view cvar = {};
  std::string_view true_text = "true";
  std::string_view false_text = "false";
  TextSource true_source = TextSource::kLiteral;
  // The value replaces a command-line or config value of the cvar (the
  // launcher's own --mcla_guest_vblank_hz) instead of yielding to it.
  bool overrides_user = false;
};

// The whitelist, indexed by Key.
inline constexpr std::array<KeyInfo, kKeyCount> kKeys = {{
    {"mcla_readback_sync_composite", Phase::kStartup},
    {"mcla_native_d3d_refcount", Phase::kStartup},
    {"mcla_native_xapi_interlocked", Phase::kStartup},
    {"d3d12_wrm_event_wake", Phase::kGraphicsPlugin},
    {"d3d12_replay_batched_wake", Phase::kGraphicsPlugin},
    {"d3d12_readback_elide_unread", Phase::kGraphicsPlugin},
    {"mcla_gpu_backend_vulkan", Phase::kStartup},
    {"prewarm_seed_store", Phase::kLauncher},
    {"d3d12_readback_sync_targeted", Phase::kGraphicsPlugin},
    // rexruntime cvars, registered before startup.
    {"memory_watch_census", Phase::kStartup},
    {"memory_watch_probes", Phase::kStartup},
    {"memory_watch_disarm_bridge_safe_gaps", Phase::kStartup},
    // Registered by the graphics plugin (shared memory).
    {"shared_memory_upload_ahead_16", Phase::kGraphicsPlugin, "shared_memory_upload_ahead_pages",
     "16", ""},
    {"shared_memory_upload_ahead_64", Phase::kGraphicsPlugin, "shared_memory_upload_ahead_pages",
     "64", ""},
    {"shared_memory_hot_blocks_shadow", Phase::kGraphicsPlugin, "shared_memory_hot_blocks", "1",
     ""},
    {"shared_memory_hot_blocks_on", Phase::kGraphicsPlugin, "shared_memory_hot_blocks", "2", ""},
    // KI-035 host init-only cvars (mcla_ki026_probe.cpp), before the guest.
    {"mcla_warp_probe", Phase::kStartup},
    {"mcla_warp_watchdog", Phase::kStartup, "mcla_warp_watchdog_seconds", "15", ""},
    // M10-007 host init-only cvar (mcla_texture_budget_win.cpp), read at
    // graphics-plugin load, so the startup phase is early enough.
    {"mcla_texture_budget_soft_cap_off", Phase::kStartup, "mcla_texture_budget_soft_cap_mb", "0",
     ""},
    // KI-035 finish-race fix (host init-only bool, default true), before the
    // guest.
    {"mcla_warp_finish_fix_off", Phase::kStartup, "mcla_warp_finish_fix", "false", ""},
    // M11-002 host init-only bools (mcla_album_probe.cpp,
    // mcla_pursuit_probe.cpp), before the guest.
    {"mcla_album_probe", Phase::kStartup},
    {"mcla_pursuit_probe", Phase::kStartup},
    // M11-001 / KI-068 SDK rexruntime bools (default true), before the guest.
    {"xam_content_stable_creation_time_off", Phase::kStartup, "xam_content_stable_creation_time",
     "false", ""},
    {"xam_content_trace_off", Phase::kStartup, "xam_content_trace", "false", ""},
    // KI-071 SDK rexruntime audio cvars, before the guest (the speaker config
    // is read at the title's first XAudioGetSpeakerConfig call).
    {"audio_speaker_config_stereo", Phase::kStartup, "audio_speaker_config", "stereo", ""},
    {"audio_speaker_config_surround", Phase::kStartup, "audio_speaker_config", "surround", ""},
    {"audio_downmix_itu_off", Phase::kStartup, "audio_downmix_itu", "false", ""},
    {"audio_channel_rms_probe", Phase::kStartup},
    // KI-056 fix A host init-only bool (mcla_album_probe.cpp, default on since
    // l81), before the guest; =false writes nothing (the control is
    // album_preview_restore_off).
    {"mcla_album_preview_restore", Phase::kStartup, "", "true", ""},
    // M11-015 kick-interval A/B: the integer SDK cvar is registered by the
    // graphics plugin (command_processor.cpp, hot-reload), so after the plugin
    // has loaded.
    {"readback_kick_256", Phase::kGraphicsPlugin, "d3d12_readback_sync_kick_draws", "256", ""},
    {"readback_kick_128", Phase::kGraphicsPlugin, "d3d12_readback_sync_kick_draws", "128", ""},
    {"readback_kick_1024", Phase::kGraphicsPlugin, "d3d12_readback_sync_kick_draws", "1024", ""},
    // l73 owner A/Bs: SDK bools registered by the graphics plugin
    // (command_processor.cpp, hot-reload), so after the plugin has loaded.
    {"fetch_identity_off", Phase::kGraphicsPlugin, "d3d12_fetch_identity_cache", "false", ""},
    {"fetch_identity_verify", Phase::kGraphicsPlugin, "d3d12_fetch_identity_verify", "true", ""},
    {"readback_steady_exempt", Phase::kGraphicsPlugin, "d3d12_readback_sync_steady_exempt", "true",
     ""},
    // KI-071 XMA doubling fixes: SDK rexruntime bools (default false), before
    // the guest; =false writes nothing.
    {"xma_swap_carry_packet", Phase::kStartup, "", "true", ""},
    {"xma_valid_only_when_written", Phase::kStartup, "", "true", ""},
    {"xma_work_lock_recheck", Phase::kStartup, "", "true", ""},
    {"xma_swap_trace", Phase::kStartup, "", "true", ""},
    // KI-071 step 2: SDK rexruntime bools (default false), before the guest;
    // =false writes nothing.
    {"xma_sibling_trace", Phase::kStartup, "", "true", ""},
    {"xma_stall_on_invalid_next", Phase::kStartup, "", "true", ""},
    {"xma_honor_start_skip", Phase::kStartup, "", "true", ""},
    {"xma_reset_codec_on_clear", Phase::kStartup, "", "true", ""},
    // KI-071 guest consume probe: host init-only bool
    // (mcla_xma_consume_probe.cpp), before the guest.
    {"mcla_xma_consume_probe", Phase::kStartup},
    // KI-071 step 3: SDK rexruntime bools (default false), before the guest;
    // =false writes nothing.
    {"xma_honor_frame_trim", Phase::kStartup, "", "true", ""},
    {"xma_frame_trim_block_granular", Phase::kStartup, "", "true", ""},
    {"xma_dump_all_rates", Phase::kStartup, "", "true", ""},
    // The SDK string cvar xma_dump_contexts (default empty, rexruntime):
    // =true writes the directory the host composed and created (true_text is
    // only the description); =false writes nothing.
    {"xma_dump_contexts_on", Phase::kStartup, "xma_dump_contexts",
     "<diagnostics>/xma-dump/<yyyyMMdd-HHmmss>", "", TextSource::kXmaDumpDirectory},
    // KI-071 step 4: SDK rexruntime bools (default false), before the guest;
    // =false writes nothing.
    {"xma_frame_walk_trace", Phase::kStartup, "", "true", ""},
    {"xma_frame_walk_fix", Phase::kStartup, "", "true", ""},
    {"xma_dump_input", Phase::kStartup, "", "true", ""},
    // M11-007d lock probe and read-back completion at the lock: host
    // init-only bools (mcla_lock_probe.cpp), before the guest.
    {"mcla_lock_probe", Phase::kStartup},
    {"mcla_lock_readback_ensure", Phase::kStartup},
    // l77: M11-007e deferred fence writes, an SDK bool registered by the
    // graphics plugin (command_processor.cpp, hot-reload), so after the
    // plugin has loaded; =false writes nothing.
    {"fence_write_deferred", Phase::kGraphicsPlugin, "d3d12_fence_write_deferred", "true", ""},
    // l77: the KI-071 walk fix control (SDK rexruntime bool, default true
    // since l77), before the guest; =false writes nothing.
    {"xma_frame_walk_fix_off", Phase::kStartup, "xma_frame_walk_fix", "false", ""},
    // l77 KI-074 E1: the host init-only raw timer switch (default on in owner
    // builds), before the guest; =false writes nothing.
    {"raw_timer_off", Phase::kStartup, "mcla_raw_timer", "false", ""},
    // l77 KI-074 E2: frame caps through the SDK int registered by the graphics
    // plugin (hot-reload), after the plugin has loaded; =false writes nothing.
    // They win over the launcher's Unlocked --mcla_guest_vblank_hz=240.
    {"vblank_hz_72", Phase::kGraphicsPlugin, "mcla_guest_vblank_hz", "72", "",
     TextSource::kLiteral, true},
    {"vblank_hz_80", Phase::kGraphicsPlugin, "mcla_guest_vblank_hz", "80", "",
     TextSource::kLiteral, true},
    {"vblank_hz_88", Phase::kGraphicsPlugin, "mcla_guest_vblank_hz", "88", "",
     TextSource::kLiteral, true},
    // l78: controls for the two SDK defaults turned on in l78 (steady-key
    // exemption, deferred fence writes) and the fence deferral minimum gap,
    // all registered by the graphics plugin (hot-reload), so after the plugin
    // has loaded; =false writes nothing.
    {"readback_steady_exempt_off", Phase::kGraphicsPlugin, "d3d12_readback_sync_steady_exempt",
     "false", ""},
    {"fence_write_deferred_off", Phase::kGraphicsPlugin, "d3d12_fence_write_deferred", "false",
     ""},
    {"fence_defer_gap_2000", Phase::kGraphicsPlugin, "d3d12_fence_defer_min_gap_us", "2000", ""},
    // l79: SDK rexruntime bools (default false), read on every pass, before
    // the guest; =false writes nothing.
    {"xma_canary_fixes", Phase::kStartup, "", "true", ""},
    {"timer_queue_blocking_wait", Phase::kStartup, "", "true", ""},
    // l79: placed texture heaps, restart-only SDK cvars registered by the
    // graphics plugin and latched by D3D12TextureCache::Initialize (guest GPU
    // setup, after the plugin-load phase); =false writes nothing.
    {"texture_placed_heaps", Phase::kGraphicsPlugin, "d3d12_texture_placed_heaps", "true", ""},
    {"texture_heap_64mb", Phase::kGraphicsPlugin, "d3d12_texture_heap_block_mb", "64", ""},
    // l80 KI-013: SDK bool registered by the graphics plugin (pipeline cache),
    // read per pixel shader modification; =false writes nothing.
    {"blend_minmax_fix", Phase::kGraphicsPlugin, "d3d12_blend_minmax_fix", "true", ""},
    // l80 KI-060 controls: SDK rexruntime strings, latched at the first guest
    // delay / when the audio threads are created, so before the guest;
    // =false writes nothing.
    {"short_sleep_native", Phase::kStartup, "ke_delay_short_sleep_mode", "native", ""},
    {"apu_priority_normal", Phase::kStartup, "apu_worker_priority", "normal", ""},
    // l80 controls for the platform defaults: placed heaps (graphics plugin,
    // restart-only) and the timer queue blocking wait (rexruntime, startup);
    // =false writes nothing.
    {"texture_placed_heaps_off", Phase::kGraphicsPlugin, "d3d12_texture_placed_heaps", "false",
     ""},
    {"timer_blocking_off", Phase::kStartup, "timer_queue_blocking_wait", "false", ""},
    // l81 KI-060 controls: SDK rexruntime strings, latched at the first guest
    // delay / when the guest audio thread is created, so before the guest;
    // =false writes nothing.
    {"zero_delay_native", Phase::kStartup, "ke_delay_zero_mode", "native", ""},
    {"guest_audio_prio_normal", Phase::kStartup, "guest_audio_thread_priority", "normal", ""},
    // l81 controls for the new defaults: the KI-013 blend fix (graphics
    // plugin) and KI-056 fix A (host init-only, startup); =false writes
    // nothing.
    {"blend_minmax_fix_off", Phase::kGraphicsPlugin, "d3d12_blend_minmax_fix", "false", ""},
    {"album_preview_restore_off", Phase::kStartup, "mcla_album_preview_restore", "false", ""},
    // l82: CP bulk register writes, an SDK bool registered by the graphics
    // plugin (hot reload), so after the plugin has loaded; =false writes
    // nothing.
    {"cp_bulk_regs", Phase::kGraphicsPlugin, "cp_bulk_register_writes", "true", ""},
    // l83 KI-054: host init-only bools (mcla_ambient_dt_probe.cpp,
    // mcla_raw_timer.cpp), before the guest; the probes default off, the hitch
    // hold on since l86 (the control is raw_timer_hitch_hold_off); =false
    // writes nothing.
    {"ambient_dt_probe", Phase::kStartup, "mcla_ambient_dt_probe", "true", ""},
    {"ambient_dt_snapshot", Phase::kStartup, "mcla_ambient_dt_snapshot", "true", ""},
    {"raw_timer_hitch_hold", Phase::kStartup, "mcla_raw_timer_hitch_hold", "true", ""},
    // l84 KI-054: host init-only bool (mcla_raw_timer.cpp), default off,
    // before the guest; =false writes nothing.
    {"raw_timer_stats", Phase::kStartup, "mcla_raw_timer_stats", "true", ""},
    // l86 control for the new KI-054 default: the hitch hold (host init-only,
    // startup); =false writes nothing.
    {"raw_timer_hitch_hold_off", Phase::kStartup, "mcla_raw_timer_hitch_hold", "false", ""},
    // l87 KI-060: host init-only bools (mcla_audio_filter.cpp), before the
    // guest; default off, the native filters on since l98 (the control is
    // audio_filter_native_off); =false writes nothing.
    {"audio_filter_probe", Phase::kStartup, "mcla_audio_filter_probe", "true", ""},
    {"audio_filter_native", Phase::kStartup, "mcla_audio_filter_native", "true", ""},
    {"audio_filter_native_verify", Phase::kStartup, "mcla_audio_filter_native_verify", "true", ""},
    // l88 KI-074: fenced read-back delivery, an SDK bool registered by the
    // graphics plugin (hot reload), so after the plugin has loaded; =false
    // writes nothing.
    {"readback_fenced_delivery", Phase::kGraphicsPlugin, "d3d12_readback_fenced_delivery", "true",
     ""},
    // l89 KI-060 control: sub_82147738 stays recompiled with the native
    // switches on (host init-only, startup); =false writes nothing.
    {"audio_filter_native_ch_off", Phase::kStartup, "mcla_audio_filter_native_ch_off", "true",
     ""},
    // l90 KI-074: the frame lead limit (SDK int) and the deferred read-back
    // slot release (SDK bool), registered by the graphics plugin (hot
    // reload), so after the plugin has loaded; =false writes nothing.
    {"frame_lead_limit_1", Phase::kGraphicsPlugin, "d3d12_frame_lead_limit", "1", ""},
    {"readback_deferred_release", Phase::kGraphicsPlugin, "d3d12_readback_deferred_release",
     "true", ""},
    // l91 KI-074: the strict (1) and adaptive (2) frame lead bound, one SDK
    // int registered by the graphics plugin (hot reload), so after the plugin
    // has loaded; =false writes nothing.
    {"frame_lead_strict", Phase::kGraphicsPlugin, "d3d12_frame_lead_strict", "1", ""},
    {"frame_lead_adaptive", Phase::kGraphicsPlugin, "d3d12_frame_lead_strict", "2", ""},
    // KI-026 host init-only bools (mcla_ki026_probe.cpp), before the guest:
    // the stall fix (default off; =false writes nothing) and the KI-026 probe
    // (plain bool, as mcla_warp_probe).
    {"mcla_warp_stall_fix", Phase::kStartup, "", "true", ""},
    {"mcla_ki026_probe", Phase::kStartup},
    // l92 probes (SDK, registered by the graphics plugin, default off); =false
    // writes nothing.
    {"readback_read_watch", Phase::kGraphicsPlugin, "d3d12_readback_read_watch", "true", ""},
    {"readback_age_inject_2", Phase::kGraphicsPlugin, "d3d12_readback_age_inject", "2", ""},
    {"occlusion_query_bounded_4", Phase::kGraphicsPlugin, "d3d12_occlusion_query_bounded_ms", "4",
     ""},
    {"present_stats", Phase::kGraphicsPlugin, "d3d12_present_stats", "true", ""},
    {"present_interval_1", Phase::kGraphicsPlugin, "d3d12_present_sync_interval", "1", ""},
    {"locked_tearing_off", Phase::kGraphicsPlugin, "mcla_locked_tearing_off", "true", ""},
    // l93 time of day: host init-only ints (mcla_tod.cpp), default off,
    // before the guest; =false writes nothing.
    {"tod_rate_x10", Phase::kStartup, "mcla_tod_rate", "10", ""},
    {"tod_rate_x30", Phase::kStartup, "mcla_tod_rate", "30", ""},
    {"tod_freeze", Phase::kStartup, "mcla_tod_rate", "0", ""},
    {"tod_start_0430", Phase::kStartup, "mcla_tod_start_minutes", "270", ""},
    {"tod_start_0530", Phase::kStartup, "mcla_tod_start_minutes", "330", ""},
    // l94 control for the new KI-074 default: the strict frame lead bound (SDK
    // int, graphics plugin, hot reload), after the plugin has loaded; =false
    // writes nothing.
    {"frame_lead_strict_off", Phase::kGraphicsPlugin, "d3d12_frame_lead_strict", "0", ""},
    // l95 low latency: SDK rexruntime cvars (registered before startup, read
    // once at the first frame), default off; =false writes nothing.
    {"low_latency_reflex", Phase::kStartup, "d3d12_low_latency_mode", "reflex", ""},
    {"low_latency_antilag2", Phase::kStartup, "d3d12_low_latency_mode", "antilag2", ""},
    {"low_latency_auto", Phase::kStartup, "d3d12_low_latency_mode", "auto", ""},
    {"low_latency_markers", Phase::kStartup, "d3d12_low_latency_markers_only", "true", ""},
    {"low_latency_boost", Phase::kStartup, "d3d12_low_latency_boost", "true", ""},
    // l95 FMA stage 0: host init-only bool (mcla_fma_bench.cpp), before the
    // guest; =false writes nothing.
    {"fma_bench", Phase::kStartup, "mcla_fma_bench", "true", ""},
    // l96 Deck CP lever 1: SDK graphics-plugin cvars (hot reload, latched at
    // every frame start), SDK default off; since l98 the fence poll interval
    // and the draw-packet clock are m10 defaults (controls below); =false
    // writes nothing.
    {"cp_fence_poll_64", Phase::kGraphicsPlugin, "cp_fence_poll_interval", "64", ""},
    {"cp_draw_packet_tsc", Phase::kGraphicsPlugin, "cp_draw_packet_tsc", "true", ""},
    {"shader_im_load_memo", Phase::kGraphicsPlugin, "shader_im_load_memo", "true", ""},
    {"shader_im_load_memo_verify", Phase::kGraphicsPlugin, "shader_im_load_memo_verify", "true",
     ""},
    // l97 low latency replaces strict: SDK graphics-plugin int (hot reload,
    // read at every frame open), default 0; =false writes nothing.
    {"lead_ll_off_when_reflex", Phase::kGraphicsPlugin, "d3d12_frame_lead_strict_low_latency",
     "1", ""},
    {"lead_ll_adaptive_when_reflex", Phase::kGraphicsPlugin,
     "d3d12_frame_lead_strict_low_latency", "2", ""},
    // l98 controls for the three new defaults: the native voice filters (host
    // init-only, startup) and CP lever 1 (SDK graphics-plugin cvars written by
    // the m10 defaults, after the plugin has loaded); =false writes nothing.
    {"audio_filter_native_off", Phase::kStartup, "mcla_audio_filter_native", "false", ""},
    {"cp_draw_packet_tsc_off", Phase::kGraphicsPlugin, "cp_draw_packet_tsc", "false", ""},
    {"cp_fence_poll_off", Phase::kGraphicsPlugin, "cp_fence_poll_interval", "0", ""},
    // l99 texture churn: the host init-only hard-limit margin (startup) and
    // the SDK minimum texture age (graphics plugin, hot reload); the guard
    // action of d3d12_frame_lead_strict_low_latency (SDK int, hot reload, read
    // at every frame open and end); default off, =false writes nothing.
    {"texture_hard_margin_256", Phase::kStartup, "mcla_texture_hard_margin_mb", "256", ""},
    {"texture_min_age_2000", Phase::kGraphicsPlugin, "texture_cache_min_age_ms", "2000", ""},
    {"lead_ll_guard_adaptive", Phase::kGraphicsPlugin,
     "d3d12_frame_lead_strict_low_latency_guard_action", "1", ""},
    {"lead_ll_guard_rearm", Phase::kGraphicsPlugin,
     "d3d12_frame_lead_strict_low_latency_guard_action", "2", ""},
    {"lead_ll_guard_off", Phase::kGraphicsPlugin,
     "d3d12_frame_lead_strict_low_latency_guard_action", "3", ""},
    // l100 command-processor levers (SDK bools, graphics plugin, latched at
    // every frame start) and the guest spin backoff (host int, startup);
    // default off, =false writes nothing.
    {"cp_reg_skip", Phase::kGraphicsPlugin, "cp_reg_write_skip_unchanged", "true", ""},
    {"cp_reg_skip_verify", Phase::kGraphicsPlugin, "cp_reg_write_skip_verify", "true", ""},
    {"cp_draw_fast_verify", Phase::kGraphicsPlugin, "cp_draw_const_only_verify", "true", ""},
    {"guest_spin_backoff_64", Phase::kStartup, "mcla_guest_spin_backoff", "64", ""},
    // l101 per-draw levers (SDK bools, graphics plugin, latched at every frame
    // start); default off, =false writes nothing.
    {"tex_memo_v2", Phase::kGraphicsPlugin, "d3d12_fetch_identity_v2", "true", ""},
    {"rt_memo", Phase::kGraphicsPlugin, "d3d12_rt_update_memo", "true", ""},
    {"rt_memo_verify", Phase::kGraphicsPlugin, "d3d12_rt_update_memo_verify", "true", ""},
}};

// Key pairs that write different values to one cvar (integer, among them
// d3d12_frame_lead_strict (l91 / l94), d3d12_frame_lead_strict_low_latency
// (l97) and the l93 mcla_tod_rate /
// mcla_tod_start_minutes, the audio_speaker_config and (l95)
// d3d12_low_latency_mode strings, or the xma_frame_walk_fix,
// d3d12_readback_sync_steady_exempt, d3d12_fence_write_deferred,
// d3d12_texture_placed_heaps, timer_queue_blocking_wait,
// d3d12_blend_minmax_fix, mcla_album_preview_restore and
// mcla_raw_timer_hitch_hold bools; since l98 also the int
// cp_fence_poll_interval and the mcla_audio_filter_native and
// cp_draw_packet_tsc bools; since l99 the int
// d3d12_frame_lead_strict_low_latency_guard_action, a group of three):
// both true in one file is contradictory and
// neither is applied. A group of three keys for one cvar is listed as its
// three pairs (first < second in Key order); see ResolveConflicts.
struct ConflictPair {
  Key first;
  Key second;
};
inline constexpr std::array<ConflictPair, 34> kConflictPairs = {{
    {Key::kUploadAhead16, Key::kUploadAhead64},
    {Key::kHotBlocksShadow, Key::kHotBlocksOn},
    {Key::kAudioSpeakerStereo, Key::kAudioSpeakerSurround},
    {Key::kReadbackKick256, Key::kReadbackKick128},
    {Key::kReadbackKick256, Key::kReadbackKick1024},
    {Key::kReadbackKick128, Key::kReadbackKick1024},
    {Key::kXmaFrameWalkFix, Key::kXmaFrameWalkFixOff},
    {Key::kVblankHz72, Key::kVblankHz80},
    {Key::kVblankHz72, Key::kVblankHz88},
    {Key::kVblankHz80, Key::kVblankHz88},
    {Key::kReadbackSteadyExempt, Key::kReadbackSteadyExemptOff},
    {Key::kFenceWriteDeferred, Key::kFenceWriteDeferredOff},
    {Key::kTimerQueueBlockingWait, Key::kTimerBlockingOff},
    {Key::kTexturePlacedHeaps, Key::kTexturePlacedHeapsOff},
    {Key::kAlbumPreviewRestore, Key::kAlbumPreviewRestoreOff},
    {Key::kBlendMinmaxFix, Key::kBlendMinmaxFixOff},
    {Key::kRawTimerHitchHold, Key::kRawTimerHitchHoldOff},
    {Key::kFrameLeadStrict, Key::kFrameLeadAdaptive},
    {Key::kFrameLeadStrict, Key::kFrameLeadStrictOff},
    {Key::kFrameLeadAdaptive, Key::kFrameLeadStrictOff},
    {Key::kTodRateX10, Key::kTodRateX30},
    {Key::kTodRateX10, Key::kTodFreeze},
    {Key::kTodRateX30, Key::kTodFreeze},
    {Key::kTodStart0430, Key::kTodStart0530},
    {Key::kLowLatencyReflex, Key::kLowLatencyAntiLag2},
    {Key::kLowLatencyReflex, Key::kLowLatencyAuto},
    {Key::kLowLatencyAntiLag2, Key::kLowLatencyAuto},
    {Key::kLeadLlOffWhenReflex, Key::kLeadLlAdaptiveWhenReflex},
    {Key::kAudioFilterNative, Key::kAudioFilterNativeOff},
    {Key::kCpFencePoll64, Key::kCpFencePollOff},
    {Key::kCpDrawPacketTsc, Key::kCpDrawPacketTscOff},
    {Key::kLeadLlGuardAdaptive, Key::kLeadLlGuardRearm},
    {Key::kLeadLlGuardAdaptive, Key::kLeadLlGuardOff},
    {Key::kLeadLlGuardRearm, Key::kLeadLlGuardOff},
}};

constexpr const KeyInfo &Info(Key key) {
  return kKeys[static_cast<size_t>(key)];
}

// The cvar a key writes.
constexpr std::string_view CvarName(const KeyInfo &info) {
  return info.cvar.empty() ? info.name : info.cvar;
}

// The text written to the cvar for a value; empty: nothing is written.
constexpr std::string_view CvarText(const KeyInfo &info, bool value) {
  return value ? info.true_text : info.false_text;
}

// KI-071 step 3 dump directory. The diagnostics root is the one the
// diagnostics manager uses (mcla_diagnostics_root; empty = <user-data>/
// diagnostics). The launcher passes <profile>/diagnostics/<session-id> there,
// so a managed session's dumps sit in its own session folder; a direct start
// uses <user-data>/diagnostics. The UTC start stamp keeps sessions that share
// a root apart (the SDK numbers its files per session from 0).
inline constexpr std::string_view kXmaDumpSubdirectory = "xma-dump";

// "yyyyMMdd-HHmmss"; empty when a field is out of range.
inline std::string FormatDumpStamp(int year, int month, int day, int hour, int minute,
                                   int second) {
  if (year < 1970 || year > 9999 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 ||
      hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 60) {
    return {};
  }
  std::string out;
  const auto digits = [&out](int value, int width) {
    char buffer[4] = {};
    for (int i = width - 1; i >= 0; --i) {
      buffer[i] = static_cast<char>('0' + value % 10);
      value /= 10;
    }
    out.append(buffer, static_cast<size_t>(width));
  };
  digits(year, 4);
  digits(month, 2);
  digits(day, 2);
  out.push_back('-');
  digits(hour, 2);
  digits(minute, 2);
  digits(second, 2);
  return out;
}

// <diagnostics root>/xma-dump/<stamp>; empty when the stamp is empty or no
// root is known (both roots empty).
inline std::filesystem::path XmaDumpDirectory(const std::filesystem::path &user_data_root,
                                              const std::filesystem::path &diagnostics_root,
                                              std::string_view stamp) {
  if (stamp.empty() || (diagnostics_root.empty() && user_data_root.empty())) {
    return {};
  }
  const std::filesystem::path root =
      diagnostics_root.empty() ? user_data_root / "diagnostics" : diagnostics_root;
  return root / std::filesystem::path(kXmaDumpSubdirectory) / std::filesystem::path(stamp);
}

constexpr std::optional<Key> FindKey(std::string_view name) {
  for (size_t i = 0; i < kKeyCount; ++i) {
    if (kKeys[i].name == name) {
      return static_cast<Key>(i);
    }
  }
  return std::nullopt;
}

enum class FileStatus : uint8_t {
  kAbsent,    // no file
  kRejected,  // too large, not ASCII or unreadable: nothing applied
  kParsed,    // read line by line (individual lines may be ignored)
};

enum class LineIssue : uint8_t {
  kMalformed,    // not exactly name=true or name=false
  kUnknownName,  // well formed, name not in kKeys
  kDuplicate,    // a name already given on an earlier line
  kConflict,     // contradicts an earlier line (two keys of one cvar true, kConflictPairs)
};

struct Issue {
  uint16_t line = 0;  // 1-based
  LineIssue issue = LineIssue::kMalformed;
};

inline constexpr size_t kMaxIssues = 16;

struct Parsed {
  FileStatus status = FileStatus::kAbsent;
  std::array<std::optional<bool>, kKeyCount> values{};
  std::array<uint16_t, kKeyCount> lines{};  // line of each value (1-based)
  std::array<Issue, kMaxIssues> issues{};
  size_t issue_count = 0;    // issues stored (at most kMaxIssues)
  size_t ignored_lines = 0;  // every ignored line, also beyond kMaxIssues

  constexpr std::optional<bool> Value(Key key) const {
    return values[static_cast<size_t>(key)];
  }
  constexpr size_t ValueCount() const {
    size_t count = 0;
    for (const auto &value : values) {
      count += value.has_value() ? 1 : 0;
    }
    return count;
  }
};

namespace detail {

constexpr void AddIssue(Parsed &parsed, uint16_t line, LineIssue issue) {
  if (parsed.issue_count < kMaxIssues) {
    parsed.issues[parsed.issue_count++] = Issue{line, issue};
  }
  ++parsed.ignored_lines;
}

constexpr void ParseLine(Parsed &parsed, uint16_t number, std::string_view line) {
  if (line.ends_with('\r')) {
    line.remove_suffix(1);
  }
  if (line.empty() || line.front() == '#') {
    return;
  }
  size_t equals = std::string_view::npos;
  for (size_t i = 0; i < line.size(); ++i) {
    const auto c = static_cast<unsigned char>(line[i]);
    if (c < 0x21 || c > 0x7E) {
      AddIssue(parsed, number, LineIssue::kMalformed);
      return;
    }
    if (c == '=') {
      if (equals != std::string_view::npos) {
        AddIssue(parsed, number, LineIssue::kMalformed);
        return;
      }
      equals = i;
    }
  }
  if (equals == std::string_view::npos || equals == 0) {
    AddIssue(parsed, number, LineIssue::kMalformed);
    return;
  }
  const std::string_view name = line.substr(0, equals);
  const std::string_view text = line.substr(equals + 1);
  bool value = false;
  if (text == "true") {
    value = true;
  } else if (text != "false") {
    AddIssue(parsed, number, LineIssue::kMalformed);
    return;
  }
  const std::optional<Key> key = FindKey(name);
  if (!key) {
    AddIssue(parsed, number, LineIssue::kUnknownName);
    return;
  }
  auto &slot = parsed.values[static_cast<size_t>(*key)];
  if (slot.has_value()) {
    AddIssue(parsed, number, LineIssue::kDuplicate);
    return;
  }
  slot = value;
  parsed.lines[static_cast<size_t>(*key)] = number;
}

// Two keys of a conflict pair true: none of the keys in a true-true pair
// applies. Pairs are evaluated on the values as parsed (before any reset), so
// a group of three (listed as its three pairs) is dropped as a whole when two
// or more of its keys are true. One conflict is logged on each dropped line
// that has an earlier-line partner in a true-true pair (for a pair: the later
// line), in line order.
constexpr void ResolveConflicts(Parsed &parsed) {
  std::array<bool, kKeyCount> dropped{};
  std::array<bool, kKeyCount> logged{};
  for (const ConflictPair &pair : kConflictPairs) {
    const size_t a = static_cast<size_t>(pair.first);
    const size_t b = static_cast<size_t>(pair.second);
    if (parsed.values[a] == true && parsed.values[b] == true) {
      dropped[a] = true;
      dropped[b] = true;
      logged[parsed.lines[a] > parsed.lines[b] ? a : b] = true;
    }
  }
  uint16_t previous = 0;
  for (;;) {
    // The next logged line after `previous` (lines are unique per key).
    uint16_t next = 0;
    for (size_t i = 0; i < kKeyCount; ++i) {
      if (logged[i] && parsed.lines[i] > previous && (next == 0 || parsed.lines[i] < next)) {
        next = parsed.lines[i];
      }
    }
    if (next == 0) {
      break;
    }
    AddIssue(parsed, next, LineIssue::kConflict);
    previous = next;
  }
  for (size_t i = 0; i < kKeyCount; ++i) {
    if (dropped[i]) {
      parsed.values[i].reset();
    }
  }
}

}  // namespace detail

// Strict parse of the whole file content (see the rules above).
constexpr Parsed Parse(std::string_view content) {
  Parsed parsed;
  if (content.size() > kMaxBytes) {
    parsed.status = FileStatus::kRejected;
    return parsed;
  }
  for (const char c : content) {
    if (static_cast<unsigned char>(c) > 0x7F) {
      parsed.status = FileStatus::kRejected;
      return parsed;
    }
  }
  parsed.status = FileStatus::kParsed;
  uint16_t number = 0;
  while (!content.empty()) {
    ++number;
    const size_t end = content.find('\n');
    if (end == std::string_view::npos) {
      detail::ParseLine(parsed, number, content);
      break;
    }
    detail::ParseLine(parsed, number, content.substr(0, end));
    content.remove_prefix(end + 1);
  }
  detail::ResolveConflicts(parsed);
  return parsed;
}

constexpr const char *LineIssueName(LineIssue issue) {
  switch (issue) {
    case LineIssue::kMalformed:
      return "malformed (expected name=true or name=false)";
    case LineIssue::kUnknownName:
      return "unknown name";
    case LineIssue::kDuplicate:
      return "repeated name (the first line wins)";
    case LineIssue::kConflict:
      return "conflicting keys for one cvar (neither is applied)";
  }
  return "unknown";
}

enum class ApplyAction : uint8_t {
  kApply,          // write the cvar
  kUnavailable,    // the cvar is not registered (e.g. an SDK without it)
  kUserSet,        // the command line or config set the cvar: left alone
  kReplaceUserSet, // the command line or config set the cvar; an
                   // overrides_user key replaces that value
};

constexpr ApplyAction DecideApply(bool cvar_registered, bool cvar_non_default,
                                  bool overrides_user = false) {
  if (!cvar_registered) {
    return ApplyAction::kUnavailable;
  }
  if (cvar_non_default) {
    return overrides_user ? ApplyAction::kReplaceUserSet : ApplyAction::kUserSet;
  }
  return ApplyAction::kApply;
}

// Runtime (mcla_dev_overrides.cpp). Both calls do nothing in builds without
// MCLA_DEV_OVERRIDES.

// True in builds made with MCLA_DEV_OVERRIDES.
bool Enabled();

// Reads and parses the file once and applies the Phase::kStartup keys. Call
// after logging is set up and before the guest starts. The roots are those of
// the diagnostics manager (XmaDumpDirectory; xma_dump_contexts_on only).
void OnStartup(const std::filesystem::path &user_data_root,
               const std::filesystem::path &diagnostics_root);

// Applies the Phase::kGraphicsPlugin keys. Call once after the graphics plugin
// has been loaded.
void OnGraphicsPluginLoaded();

// The value written to the key's cvar by this module (nullopt when the file
// has no valid line for it, the cvar was left alone, or the write failed).
std::optional<bool> AppliedValue(Key key);

// The value of the key's valid file line (nullopt without the option, without
// a parsed file or without a valid line), whether or not anything was
// written. Used for keys whose value writes nothing (upload-ahead, hot blocks,
// read-back kick, the l73 fetch-identity / steady-exempt and the KI-071 XMA
// SDK keys =false, also xma_dump_contexts_on=false and, since l81,
// mcla_album_preview_restore=false; since l86 raw_timer_hitch_hold=false; since l94
// the three d3d12_frame_lead_strict keys =false; since l95 the six l95 keys
// =false; since l96 the four l96 keys =false; since l97 the two l97 keys
// =false; since l99 the five l99 keys =false; since l100 the four l100 keys
// =false; since l101 the three l101 keys =false).
std::optional<bool> FileValue(Key key);

}  // namespace mcla::dev_overrides
