# l103 on the RTX 5060: verify-all and fast (2026-11-15)

Owner runs, l103 (manifest 082EBEA1), copy profile 68e5, 1080p Unlocked. Read by the primary agent with awk over the MCLA_PERF_SPLIT minute windows (windows with more than 600 frames and more than 4000 draws per frame). Spark not used: deterministic aggregation.

## Run A, pc/verify-all (session 2e79a43f, 11 minutes)
- Texture fetch memo v2: 15,425,178 lookups, 14,255,910 hits (92.4 %), mismatch 0.
- Render-target Update memo (verify): 17,223,197 lookups, 10,802,483 verified hits (62.7 %), rt_mm all 0.
- Register-write skip verifier: cb_mismatch 3930, all `cb_mm_new_size_zero=pf:3930`; `cb_mm_size_changed_nonzero` 0 and `cb_mm_same_size_diff_bytes` 0. The l100 "cb mismatch" block was a verifier false positive of the empty-stage class.

## Run B, pc/fast (session aaf16693, about 6 minutes; owner: no flicker, no artefacts)
Per-draw cost in microseconds, sampled phases of the draw path:

| Session | Arm | draws/frame | CP ms/frame | draw | tex | rt | pso | bind | setup |
|---|---|---|---|---|---|---|---|---|---|
| 63fec055 (11-14) | reflex-nostrict, no levers | 4934 | 15.3 | 1.59 | 0.490 | 0.206 | 0.114 | 0.466 | 0.092 |
| aaf16693 (11-15) | fast (tex memo v2 + rt memo) | 5020 | 14.9 | 1.42 | 0.399 | 0.127 | 0.120 | 0.476 | 0.095 |
| 34aec854 (11-14) | default (strict) | 4807 | 19.8 | 1.67 | 0.521 | 0.204 | 0.113 | 0.468 | 0.090 |
| 15c4d94f (11-14) | auto-noguard soak, daytime, MAIN | 5983 | 17.1 | 1.40 | 0.354 | 0.205 | 0.116 | 0.414 | 0.089 |
| 2e79a43f (11-15) | verify-all (verify overhead) | 5626 | 18.9 | 1.81 | 0.494 | 0.233 | 0.111 | 0.642 | 0.086 |

- RT memo: 0.206 -> 0.127 us per draw (-38 %). The rt phase is 0.204-0.207 in all four sessions without the memo, so this gain is robust: 0.4 ms per frame at 5 k draws, 0.7 ms at 8.5 k.
- Texture memo v2: 0.490 -> 0.399 (-19 %) against the like-for-like night run, but the tex phase varies with content (0.354 in the daytime soak without v2), so its size is not established from one 4-window run.
- Draw path total -11 %; about 1 ms per frame in a 25 ms dip frame (roughly 38 -> 39.5 fps). The levers are safe and real but do not remove the dips.
- The draw path is only about half of CP busy (1.42 us of about 2.97 us per draw): the rest is outside it (register writes, packet parsing). The register-write skip targets that part; arm `pc/fast2` (= fast + cp_reg_skip) was added by hand to the l103 arm set after the verifier result above.
- MCLA_DIP (first use): 13 dips in about 6 minutes, 1-3 s each, 38-42 fps against a 51-55 median; hints mostly cp_draws, some gpu and cp_other.
- Fourth clean Reflex-without-strict run by eye.

## Run C, pc/fast2 = fast + cp_reg_skip (session fb4bf9d9, 6 qualifying minute windows; owner: no artefacts)

| Session | Arm | draws/frame | CP ms/frame | CP us/draw | draw | tex | rt | bind | GPU ms/frame |
|---|---|---|---|---|---|---|---|---|---|
| 63fec055 | reflex-nostrict | 4934 | 15.3 | 3.10 | 1.59 | 0.490 | 0.206 | 0.466 | 17.4-18.9 |
| aaf16693 | fast | 5020 | 14.9 | 2.97 | 1.42 | 0.399 | 0.127 | 0.476 | 17.7-19.5 |
| fb4bf9d9 | fast2 | 4829 | 13.7 | 2.84 | 1.32 | 0.380 | 0.125 | 0.413 | 17.2-18.8 |

- Register-write skip in the session: 32,870,348 register writes and 462,955,005 constant dwords skipped.
- CP cost per draw 3.10 -> 2.84 us (-8.4 %) with all three levers; per window 2.70-2.97 against 3.02-3.18.
- fps is unchanged in normal windows (47-54) because they are GPU-bound: GPU busy 17-19 ms per frame against CP 12.5-14.6 ms. With the CP levers the dips are now capped by the GPU as well (GPU about 14.7 ms + 1.14 us x draws: about 23.8 ms at 8 k draws).
- MCLA_DIP: 15 dips in the session, 1-4 s, minimum 35.6-41.8 fps; hints cp_draws 10, gpu 2, unknown 3.
- Conclusion: the CP levers are safe and worth keeping, but the next gain on this GPU must come from GPU time (a 14.7 ms base at 1080p is the larger term).

## Run D, pc/fast2 at 720p (session a5cbe38e, 7 qualifying minute windows), against Run C at 1080p

| Mode | fps | draws/frame | CP ms/frame | CP us/draw | GPU ms/frame | copies/frame |
|---|---|---|---|---|---|---|
| 1080p (fb4bf9d9) | 51.5 | 4619 | 13.3 | 2.87 | 17.8 | about 60 |
| 720p (a5cbe38e) | 66.0 (windows 62-76; DIP medians 70-82) | 4277 | 12.3 | 2.86 | 9.0 | 46-60 |

- GPU time halves with 2.25x fewer pixels: 17.8 -> 9.0 ms (better than the 12-13 ms predicted from l53-era data). Two-point fit: about 2 ms fixed + 7 ms per 720p-worth of pixels, so nearly all GPU time on this card is per-pixel work, not per-draw work.
- At 720p the frame is CP-bound again (CP 12.3 ms, GPU 9.0 ms, frame 13-16 ms); CP cost per draw is identical in both modes (2.86-2.87 us), so the CP levers are what limits 720p.
- Dips at 720p: 49-56 fps at 6-7 k draws against medians of 70-82, hint cp_draws; F10 windows 46-64 fps at 5-6.7 k draws with CP 14.5-20.2 ms and GPU 9.5-11.4 ms.
- Consequence: at 1080p the lever is per-pixel GPU cost (MSAA at 2x2 draw scale, resolves, post passes); at 720p it is CP cost per draw.
