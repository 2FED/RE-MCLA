# M7-016: Gamescope and stock MangoApp telemetry

2026-09-08. User requests testing/build launches and hardware telemetry in Gaming
Mode, preferably using stock MangoHUD. Custom HUD visibility work is deferred.

The installed SteamOS SDK set_session implementation uses steamos-session-select.
Its current SteamOS shim delegates to steamosctl. The requested gamescope switch
returned0; session137 is gamescope/wayland, SSH remains available, and stock
MangoApp and Steam restarted. Defaultlogin was alreadygame; desktopdefault remains
plasma.desktop. No MCLA game was running during transition.

MangoApp0.8.3.rc1.r24.g33c2c7dd uses its managed per-session config withpreset4.
Its installed mangohudctl supports explicit set reload_config true and
set log_session true/false. The advertised reload-cfg alias returnedusage/1;
use the explicit setter. A firstsmoke stopped beforelogging and restoredconfig.

A second short SteamUI-only test successfully created CSV andsummary in remote
MCLA-Diagnostics-20260905/gamescope-mangohud-20260908/logging-smoke-2, using500ms
logging and6seconds duration. AllIPCcommands returned0, logging stopped, and the
original71-byte configurationSHA9AEFDC69AB40F2DD32F06261F5C24A0BFA9148BB9CC8C7E8122B384227B5B630
was restored. CSV SHA D93B0956003CC365EE0B352B02867105BF9ACC6553D319A661F47FA2E7C11DFE;
summaryF83EF7F16D676BBAF5732D7F353C4FD3395FB5AE91F6EFF8463CBA5E4A192018.

The actual16-columnCSV includes FPS/frame time, aggregateCPU load/power/frequency,
GPUload/clocks/power/temperature,CPUtemperature,VRAM/RAM/swap andprocessRSS.
It has no per-core columns. IdleSteamUI repeatedcached readings; this is control/
file-format evidence, not game-performance or telemetry-freshness acceptance.
Elapsed fields were3.058..6.059billionns. Do not assume a fixed3-secondwarmup or
reset the first row to elapsed0: source logger waits for validdata.

MangoApp receives Gamescope visible-frame timing, while our internal counter
counts published guestoutputs. Repeated hostpresentations can differ, so calibrate
the same game windows before removing the embedded diagnostic. Hardware telemetry
may be combined with internal guestcounters and a lightweight per-core sidecar.

References: [MangoHUD Gamescope route](https://github.com/flightlessmango/MangoHud#gamescope),
[logging implementation](https://github.com/flightlessmango/MangoHud/blob/master/src/logging.cpp),
[control implementation](https://github.com/flightlessmango/MangoHud/blob/master/src/app/control.c).

Next: one isolatedGamingMode launch of exact Vpackage withcustomHUDoff, preserving
accepted13-byteGPUconfiguration, fullsave and lifecycle gates, and correlate its
savedcounterwindows with stockMangoApp logs. This game calibration is not yet run.

## Actual calibration and manual SDK launch

Single calibration completednormally in178.432seconds, result
7C351BAE651A05ABF5E89E2BCCC32AD35F02C74EB296E0CA82554B32788D0D9F.
Guestoutputs19.9unpaused/18.7paused; fullprofile,94counterdeltas,6frames,2config
observations andnormalclosure independentlyverify. Return47files/29,477,295bytes.
CPUsidecar completed380samples across8logicalCPUs,3,040records/3,032validintervals
and8initialnulls, zeroerrors/misses; collectionCPU0.481899s over190s (~0.254%oneCPU).
MangoCSV437rows exists, but repeated/cached values do not attestguestFPS.

An owner-observed repeat exposed the cause needing attention: gameaudio/rendering
ran whileSteamremainedvisible andthegamewasabsentfromitswindowpicker. Gamescope
focuspropertiesshowedSteamBigPicture769 whileMCLAprocessran. Do nottreatthat
backgroundlaunch telemetry as a verified focusedgameFPS calibration.

A separate manual SDKentry mcla_diag_manual_20260908 now launches through the
officialrun-game RPC. SteamassignsAppId3460577213/GameId14863065955151380480.
MCLA191569 andGamescopefocusedApp3460577213/window35651585 wereverified.
Theuserwas explicitlynotified thatthegameisnowlaunched/focused. ALLautomaticprobe
flagsarefalse; customGuestFPSHUDtrue. UserexplicitlyrequestsDO NOTCLOSE.

Manualdata residesin remote gamescope-mangohud-20260908/manual-1, usingtheexisting
Vpackage anditsowncompleteprofile/cache/compatcopy. Itsprofilemayevolve; preserve
andreturnthefullprofileafterownerexit. NoautomaticMango/CPUloggingiscurrentlyactive.
SDKrequiresanunderscore/alphanumericidentifier andagame-relativeexecutable; initial
registrationfailuresarepreserved. Finalentryuses./launch.sh andpreservesSteam's
assignedAppIDs ratherthantheprevioussyntheticIDs. NeverreadSDKauthkeys/tokens or
exposeitsdebuglogging.

Owner physical confirmation: the SDK-launched manual game isvisible. With the
custom ImGuiHUD enabled, owner observes GuestFPSabout18 while stockMangoHUDshows
57–65. This confirms the two visible counters differ; it isconsistentwith host
presentation/UI repaint versus guestoutputs. The next focused launch should
disableourHUD while retaininginternalcounterlogging. Current gameMUST stayopen
untilownerchooses toexit; preserveitscompletepossiblyevolvedprofileforanysuccessor.
No audio,controller orstable30acceptance is inferred.


## Owner-authorized manual closure

Owner subsequently authorized closing the game and continuing automatic tests.
Root verified PID191569/birth181259501 and its own DISPLAY=:1 window35651585,
then sent one WM_PROTOCOLS/WM_DELETE_WINDOW request. Game disappeared in0.5s;
wrapper191372 also disappeared. No force or global input was used; SDK-launched
process exit code is unavailable. Full four-file profile3815088B was preserved
and returned with its closure/launch/log evidence; all four hashes are unchanged.
Private return archive87D60824E68F6C75A58082FB01C6B26B02BDA386805C4CA4764C346757AB9FC1,
nine files verified locally. Any successor seeds from this terminal manual profile.
Next: proper SDK-assigned display/AppIDs, custom HUD OFF, internal counters plus
stock MangoApp telemetry. Latest live weekly allowance67% remaining.


## Focused SDK HUD-OFF attempt2

Runner62A36193 and observerCD940 were independently reviewed. Attempt1 had stopped
before title launch on Steam-injected WINEDLLOVERRIDES=dxgi=n. A narrow environment
probe confirmed it; attempt2 explicitly accepts/removes only this known override,
records inheritance, and preserves other guards. Parent DISPLAY=:1 and actual
SDK SteamAppId2555166803/GameId10974357854743429120 are preserved.

Owner terminal manual profile is the actual seed, unchanged4files. MCLA192410,
birth181438510 and its own window37748737/class were verified. Game182.7736s,
exit0/no force/no survivors; configuration/full-profile gates PASS. Guest cadence
399/20.000004s=19.95 unpaused;370/20.000004s=18.50 paused. Result88A970848A05D4E770FA31A92EBD0C1DC1C571B1644B50B389E80A3F27A667CA.

Initial stock Mango IPC commands returned0 but produced no CSV; owner reported
no visible stock HUD. Explicit no_display=false then log_session=true at Linux
337590.323/337590.575s recovered visible/loggable telemetry.85CSV rows cover late
game and return-to-Steam; the latter90FPS rows are excluded from game rates.
Late pause proxy36rows averages18.5269FPS against internal18.50; this arithmetic
sample mean and observation-derived window are not an exact frame-weighted or
cross-clock comparison. Per-core sidecar completed; stockconfig restored exactly.
Return54payloads33566295B, archiveA098E7775B1A80842D11186A9C329E8627AE74C4A49946ABA7B3B02E0BC88EE2;
local sdk-off-return/verified has every payload hash verified. Independent audits pending.

The next isolated repeat adds a real complete-CSV-row readiness gate before game
launch, instead of accepting IPC exit0 as logging acknowledgement. This resolves
an observed missing-data failure; it is not a repetition of already-passed game tests.


## Full readiness-gated repeat3: feasibility accepted

Runner59148EA8 and observer6A1B0200 were independently reviewed. One actual,
newline-complete CSV row appeared before game launch; no retry/recovery needed.
Game193208/birth181503116 closed0 after178.682s, no force/survivors. SameVpackage,
HUDfalse,1x/pre-mask=true and full terminal manual-profile seed. Independent game
audit passed94deltas,2configuration/HUD records,2x12mapped-image sets,6framehashes
and4profilefiles. Root inspected4guestviews: parked rear view and pause orbit,
no obvious corruption. No physical driving/audio/stable30 acceptance.

| Focused overlap | Guest outputs/s | Mango sampled mean | Median | Rows |
|---|---:|---:|---:|---:|
| Unpaused |19.999993|20.1743|20.0331|40|
| Pause proxy |18.599997|18.2628|19.1651|39|

These quantities corroborate approximate cadence; Mango sampledFPSstatistics are
not exact frame-count rates. Logger epoch is conditionally338068557149596..
338068564206419Linuxns (7.056823ms uncertainty), using actual readiness/live rows.
The pause interval is an observation-derived proxy, with no guest/Linux clock
equality assumed. All included rows lie within a contiguous observed game-focus
segment; sampled1s focus is not continuous proof.421CSV rows total;29postexitSteam
rows average90.254FPS and are excluded, together with transition gaps.

CPU420samples across8logicalCPUs:3352valid raw-recomputed intervals plus8initial
nulls, no errors/misses/frequency nulls. CollectionCPU0.471692s/210.000089s =0.2246%
of one logicalCPU; p95/maxread1.258/1.795ms. Sidecar/observerexit0 and exact stock
config9AE restoration verified. No telemetry remains active. The runner retains
the understood ignored32-bit Steam-overlay preload warning; stderr is not empty.

Result50781BB878EFF08582D611741CC216EB04A40888DC2F49D6EA563594AA929522;
CSV CBFB7CCF983EB24357C7B4E40FF790DD2552A9851337EFF705C9068516EDF017;
CPU FB2F49746D56B7F57460CA78FA60367639FF1480EC556D13C4D00C039FEBB5C7.
Return53payloads33620346B archive19E3566AA3C8BD38B30C34D0464F6FC12DABFCB10FC8670DF5D1B32F05634B64,
inventory0B8E2210BCD30995AD0F960EDBCBC4EC2B74E9EC0A1C07193B301934A3302F5B,
local sdk-off-repeat-return/verified; every hash independently checked.
Root analysis02EB5F1368B9B725C505CBF273C875FDDCE54F5EF3634988E718A684EEE5B068.

Decision: stock MangoApp is usable for external cadence/hardware monitoring in
this SDK/HUD-off workflow, with real-CSV readiness and focus gates; retain internal
guest counters for quantitative acceptance. Per-core data needs the separate
bounded sidecar. This closes telemetry feasibility, not M7-016 performance gates.
Resume the separate8vs16-page endpoint-expansion experiment without promotion.
