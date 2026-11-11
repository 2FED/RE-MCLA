# l90 A/B of 2026-11-11 (invalid: ran l88) and the all-runs table for KI-074

Read-only analysis by an Opus subagent, accepted by the primary agent. Spark not used: log parsing and source reading.

**Headline.** INVALID A/B: the three owner runs of 2026-11-11 13:28-14:10 reported as l90 default / lead1 / lead1-rel all ran the l88 build (launch receipts D17C1E6B = l88; the launcher Build folder still pointed at l88; the log says "unknown name" for the l90 keys). They are three more l88-default flicker runs; the lead limit was not tested. All-runs table (l86, l88 x4, these three): the only quantity that separates flicker runs from clean runs without exception is the frame-end lead gauge reaching 2 (guest fences 2+ frames late, f2p > 0): exactly 0 over 28 clean driving minutes (steady-off, cap44), above 0 in every flicker run and every flicker press interval; f1 lateness, GPU ms/frame, fps and GPU % per minute all overlap. Code: the exempt path copies the previous frame of 039CA000 into guest RAM through the physical view (no write watch, no texture invalidation), and every sampled texture load of it is the GPU-side scaled copy, so data age cannot reach the screen through the texture cache; cap44 delivered the same F-1 data and was clean.

---

# l90 PC A/B (2026-11-11), read-only analysis notes

Evidence copy: private/owner-l90-flicker-20261111/{diagnostics,sessions}/<id> (robocopy /XD save-private before-user after-user /XF save-files-private.json *.dmp). Logs concatenated oldest-first as <id8>.all.log here.

## Identity: the A/B did not run the l90 build
- All three sessions: launch-receipt build D17C1E6B... = l88 build_manifest_sha256 (l88-internal/build). l90 is D6AF6995... (l90-internal/build, staged 13:27). l88 mcla.exe lacks the string frame_lead_limit_1; l90 mcla.exe has it.
- add76f72 13:28:31-13:42:46 (14.3 min): arm default, 12 values, 0 lines ignored.
- e7d72481 13:43:24-13:46:00 (2.6 min): arm lead1, "line 15 ignored: unknown name" (frame_lead_limit_1).
- 7585e793 13:46:52-14:10:12 (23.3 min): arm lead1-rel, "line 15/16 ignored: unknown name".
- => three l88-default runs; no lead_open*/lead_cap*/rb_release* counters exist in the logs.
- F10: add76 13:36:47, :48, :56, 13:39:17, :28, 13:42:31; e7d7 13:45:33; 7585 14:09:22, 14:10:04, :05.

## Per minute (pm2.py from l88-cap44-work)
add76 driving: 44.8-48.2 fps, GPU 85-96 %, lead max 2 in 10/11, lead2 0-178/min, f2p 0-736/min, fast waits 0.09-0.37/f.
7585 driving: 35.8-55 fps, GPU 87-97 %, lead max 2 in 17/21, lead2 0-337, f2p 0-1586, fast 0.01-0.38/f.
Minutes with lead2 = 0 in default: 13:36:35, 13:48:54, 13:52:55, 13:59:55, 14:01:55 (no F10 in them).
Heavy lead2 minutes (>100) have lower GPU/CP % (restart/load transitions inside the minute); steady 95-96 % minutes have 1-30.
Marks (cumulative since the minute line): every press interval has lead2 >= 1 (1..337).

## Per press (win2.py from l88-ab-work), fast-wait share 5 s before / flight ring
add76 33/36, 38/43, 0/29, 25/37, 53/57, 51/71 %; e7d7 30/31 %; 7585 95/93, 65/84, 70/81 %. prev_age 6 submissions (~1 frame) avg, max 12-19.

## All runs, driving minutes (GPU >= 75 %, fps >= 30; allruns.py)
run              flk n  fps        gpu%   gms/f      lead2/min med,max  min_w_l2 f1/f f2p/min med,max fastshare tw/f twms/f rbw/f fw/f
l86-default      Y  32 38.7-52.9  76-97  17.5-24.4  n/a (no field)            -   6.44  16,1584      22.6%    0.03 0.27 0.96 0.44
l88-default5a50  Y  11 41.0-51.7  79-96  17.6-20.8  3,254                  8/11  6.33   8,1070      23.0%    0.04 0.43 1.19 0.61
l88-fenced04a3   Y   8 41.6-51.3  89-96  17.4-22.1  14,287                 6/8   6.14  55,1470      19.9%    0.04 0.33 0.92 0.53
l88-steadyoff    N  14 39.0-46.3  83-90  19.2-21.4  0,0                    0/14  5.57   0,0         n/a     1.04 6.81 7.10 6.88
l88-cap44        N  14 35.8-43.5  81-89  19.3-22.5  0,0                    0/14  2.07   0,0         0.0%    0.04 0.27 0.42 0.40
l90x-add76(l88)  Y  11 44.8-48.2  77-96  16.9-21.1  18,178                10/11  6.36  74,736      22.4%    0.04 0.43 1.13 0.56
l90x-e7d7(l88)   Y   1 44.4       82     18.4       204                    1/1   5.16  936         27.2%    0.11 0.38 1.28 0.59
l90x-7585(l88)   Y  21 35.8-55.0  87-97  16.9-24.5  63,337                17/21  6.25 236,1586     21.7%    0.04 0.35 1.09 0.58
Separates without exception at run level: any frame-end lead 2 / any f2p (0 in clean runs over 28 driving minutes incl. restarts). f1, GPU ms/frame, fps range, GPU % per minute overlap.
No present/vsync interval histogram is logged.

## Code (private/l90-sdk-wt)
- Exempt delivery: command_processor.cpp 5102-5141 (plan kFast/kWait, AwaitReadbackSubmissionBounded on the PREVIOUS slot's submission), memcpy of rb.mapped_data[read_index] (previous slot = frame F-1 copy) 5518-5536. Non-exempt forced: 5436-5474 read_index = write_index, targeted wait for the own copy -> frame F data.
- Guest RAM write uses TranslatePhysical = physical_membase_ (xmemory.h:380); write watches protect membase_+heap_base_ views (xmemory.cpp:1905+, protect_base). Resolve: MarkRangeAsResolved (cache.cpp:324-370) -> RangeWrittenByGpu (shared_memory.cpp:460-476) fires watches by GPU and arms watches. So the memcpy fires no watch; texture cache never sees it.
- Logs: 039CA000 tex_load all scaled_resolve=1 (175 / 308 sampled); unscaled loads only livery atlases 05208000/05268000 (rb_overlap 0); tex_inval by_gpu=1 except one 05468000 mips.
- Lock probe (l76) hooks only the XDK lock core sub_82421CA0: it cannot see raw-pointer guest reads; no read detection exists in the SDK.
