# M7-016: owner manual H100/U8 drive

Status: CLOSED by owner. Owner explicitly confirmed self-exit; no title remains.
Direct title exit status is unobserved and wrapper-result.json is absent.

Best retained package42D0 uses ABF host/AEE runtime/U8 GPU8405; U4 is not promoted.
Fresh manual-h100-20260909 copies the complete3BBC four-file terminal owner seed
and original two-file cache. Originals were checked unchanged. The new profile
may legitimately evolve and must be returned intact, never rejected/reseeded
because it differs from the starting save.

Command3211C397660B54067D499DFEC5036BDCE7761B61697B6EE127621158407ED1DE
is adapted from the owner-tested manual Gaming Mode command. H100 CLI/environment
true; guestHUD off as previously requested; readback/unpaused/frontend probes
false in CLI/environment. No scripted input, automatic pause or duration limit.

Launcher actualC679461C5213B9107A14BC8E3FE15345010BEDEF7E330FEA1E4061FA8A93A529
passed independent review and AST. The initially printed6247 hash was the LF
string before Windows newline translation; actual CRLF bytes normalize to that
hash. Deployment/review binds actualC679, not the pre-write string.
The launcher verifies package/bin hashes and SDK IDs, removes only known dxgi=n,
and records wrapper exit separately. It does not claim direct title exit status.

Existing manual SDK entry mcla_diag_manual_20260908 uses AppId3460577213,
GameId14863065955151380480. Its shell now hashes
7ED74E82E62A8DA88BD21584D4E794CD31FA4FB9DBC434F34E1BA001401EAD94;
prior shell was preserved. No auth/token files were inspected.

Observed controller223848/wrapper223854/title224050(start189267368), exact ABF/
AEE/U8 mappings. Gamescope focus3460577213/window37748737 confirmed. Actual child
environment showsH100true/HUDfalse/all three automatic probesfalse.
The owner received the explicit start signal and drove manually. Actual guest H
was not read in this process; H100 was requested and previous automated controls
verified it. Owner later explicitly confirmed self-exit. Controller223848,
wrapper223854 and title224050 were observed gone; absent wrapper-result means no
direct or wrapper exit0 claim.

The complete terminal profile was preserved: four files3815088B, aggregate3BBC44D0
unchanged from the initial seed. Source seed unchanged. Local verified-return
contains16files4084028B (excluding its manifest), archive
AFFDCB81CEC1E2B207025C4B905DB2E100FC95C75139901E190A60E9AFD37482,
manifest30CA897947B8E0E41620B8A3A666A30A16F45C097CB6A8CBC7C647C1B4069FE5.
No progress evolution is established by this unchanged save; all files were
preserved rather than assumed equal.

## Owner feedback and next capture

Owner reports roughly15–20FPS during active driving, versus about25 while parked.
These are subjective estimates with guestHUD off, not measured minima/averages.
Sound did not break up during this drive. The owner's explanation that it stays
above10FPS is not a measured frame-rate floor or a demonstrated audio cause.

Traffic correlation is unclear: dips seem more likely approaching intersections
than while already on them. Owner suspects nearby traffic spawning or loading the
next area, rather than simply visible traffic count. Treat this as a hypothesis;
resource reads/decompression, object creation, shader/pipeline preparation and
uploads remain candidates until current moving-profile evidence distinguishes them.
Stock moving30 and robust audio acceptance remain open.

After a fresh owner start signal, one12-second99Hz all-title-thread user-space
leaf profile and process I/O brackets completed. Owner confirmed that the typical
dip occurred during that capture. No automated input/pause/title termination or
game settings changes. See [capture evidence](M7-016-manual-h100-profile.md).
Recording overhead excludes a clean FPS comparison. Logical read counters include
cached/non-file reads; physical-read volume alone cannot establish asset causes.
