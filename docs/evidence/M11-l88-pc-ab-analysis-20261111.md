# l88 PC A/B (2026-11-11): KI-074 default vs fenced delivery

Read-only analysis by an Opus subagent, accepted by the primary agent. Spark not used: deterministic log parsing.

**Headline.** Owner l88 A/B on 3840x2160 120 Hz, 1080p Unlocked, MAIN save (2026-11-11): arm default flickers, arm fenced flickers too; no visible tearing at 120 Hz. In the fenced run the fix was fully active (fenced=1, cad_unfenced_served=0, fence_before_data=0, delivery latency <= 1 frame), so the cadence-copy / fence-ordering path is NOT the cause of KI-074; `d3d12_readback_fenced_delivery` stays off. The only counter that tracks the F10 presses across l86 and both l88 runs is the steady-key fast-wait share (GPU a frame behind the CP: 20-95 % at presses vs 7-25 % median), and it is not sufficient alone. Texture counters, uploads, memory watch and placed heaps are flat. No kept frame shows the bad frame (capture lands 0.1-0.2 s after the press). Bisection with existing keys: readback_steady_exempt_off; then 720p defaults or texture_placed_heaps_off / fetch_identity_off. Raw copy: private/owner-l88-flicker-20261111 (diagnostics and logs; copied private-save files removed unopened).

---

# l88 PC A/B (2026-11-11): default vs readback_fenced_delivery (read-only analysis)

Evidence copy: private/owner-l88-flicker-20261111/{diagnostics,sessions}/<id> (logs, receipts, F10 packages; no user folders).
Build D17C1E6B... = l88 candidate-receipt build_manifest_sha256 (both sessions).

- Run A = 5a50364b (09:10-09:52 local), 12 dev values (same as l86/run 1), fenced=0.
- Run B = 04a3509e (10:03-10:12 local), 13 values: + readback_fenced_delivery=true, fenced=1.

## Fix active in B
fenced=1 every line; cad_unfenced_served=0 (A: = cad_unfenced, 0.3-1.3/frame); cad_deferred 509-2991/min;
done_delivered ~= cad copies (17k-45k/min), done_lat_frames_max=1, done_superseded=0;
fence_before_data=0 (A: ~10.0-10.2 per frame, every minute). B still flickered -> cadence copy / fence order is not the cause.

## Per press (flight ring = last ~180 composite resolves of 039CA000; fwait = steady_fast_waits share)
A 09:12:55 fps~47 flight 1% | 09:18:57 27 fps 5% | 09:21:00 52 fps 0% (no frame: window capture failed)
| 09:28:13 24-37 fps 0% | 09:50:57 47 88% | 09:51:16 47 40% | 09:51:30 45 57% | 09:51:31 67% | 09:51:33 49%
| 09:52:01 40 20% | 09:52:03 57% | 09:52:04 70%.
B 10:08:05 49 95% | 10:08:06 91% | 10:08:09 45% | 10:12:20 42 51% | 10:12:22 78% | 10:12:36 26% | 10:12:38 68% | 10:12:40 60%.
Quiet (no press) 5-s windows of sampled composite lines: A 09:14-09:18 median 25%, p90 45%, max 95% (3/58 >= 50%);
B whole run median 7%, p90 55%. Per-minute driving 19-33% in both.
Flat in every minute and press (no tracking): tex loads 48-57/frame (99.99% overlap read-back ranges, all scaled_resolve=1),
reloads without inval 3.0/frame, GPU invals 45-54/frame, CPU invals ~0, tex_waiting 0.03, upload 4-10 MB/frame,
write faults 0.5-1.1 ms/frame, watch arm+disarm 0.6-1.2 ms/frame, tex_gen_bumps 1-4/frame, GPU 17-22 ms/frame,
cp_lead_frames max 2, fence_late_f1 55-75%.
TEXHEAP: no releases; A reaches the soft limit (~2120 MB incl. scaled add) at 09:30 and 09:50; B flickered at ~1.0 GB -> eviction not common.

## Frames (frames/*.png, 960x540; c*.png full-res crops)
Night, Mulholland / Laurel Canyon / Sunset Plaza. No captured frame shows an obvious wrong texture or missing geometry;
A pkg 9 near building flat pale (cf. l86 f9/f14), otherwise normal; B 1-3 dusk teal sky (time of day), B 4-8 night.
Capture is the guest frame ~0.1-0.2 s after the press: the transient flicker frame is probably not captured.

## Scripts
iv.py (interval table), win2.py (per-press fps + flight), slide.py (5-s windows), crop.py, stripes.py; cad.py from l88-tearing-work.
