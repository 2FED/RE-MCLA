# macXenos survey for the macOS feasibility gate (2026-11-13)

Read-only survey by an Opus subagent (GitHub API only; nothing built or run), accepted by the primary agent; feasibility statements are inference, marked as such in the body. Licence checked from the repository files. Spark not used: remote reading.

**Headline.** bourbonicfisky/macXenos (HEAD ba46014b, 2026-11-12) is an unofficial macOS port of Xenia Canary with the full imported Xenia history; licence BSD-3 (portable with attribution), with two exceptions: the Apple Metal Shader Converter dylib (proprietary; redistributable only for shader conversion) and an attribution request for its icon/UI. It lists Midnight Club: Los Angeles as "free roam, 30 fps, M4 mini" (emulated, native ARM64 JIT + a native Metal backend). Relevant facts for a macOS ARM64 port of MCLA-R: the 4 GB guest space is mapped with shm_open + fixed mmap views found via mach_vm_region; the host page is 16 KiB so write watches run at 16 KiB granularity (4x coarser, a watch only when all four guest pages are writable) and faults come as SIGSEGV + SIGBUS with the address decoded from the thread context; their Metal backend goes Xenos ucode -> DXBC -> DXIL -> Metal Shader Converter and targets Canary interfaces, so adopting it into our SDK is a large port, while MoltenVK on the SDK Vulkan backend is the cheap first try (they hit a texture-LOD problem there). Our SDK already has a POSIX layer with the same system-page watch design, SIMDe and an aarch64 FPCR path; the Win32-only host surface (97 of 229 source files) is the other big piece. Assessment (inference): months, not weeks; gate plan G1 compile on Apple clang, G2 SDK ctest on arm64, G3 headless boot to the identity markers, G4 MoltenVK to menu and free roam with watch-fault cost measured, G5 choose MoltenVK vs Metal. Useful now: their spin census / backoff for the guest fence-wait spin (BSD; relevant to CPU contention), and a note that Canary's texture point snap regresses MCLA (keep it off).

---

# macXenos survey notes (2026-11-13, read-only, GitHub API)

Repo: bourbonicfisky/macXenos, HEAD ba46014b (2026-11-12). Created 2026-11-11, not a GitHub fork.
Size 44,784 KB; 2,735 tree entries (src 2,488). 9,423 commits: full Xenia history imported
(first 30bde439 Ben Vanik 2013-01-10); contributors benvanik 2373, bourbonicfisky 1298, DrChat 1244,
Triang3l 1145, gibbed 950, Gliniak 684, has207 348. Last 100 commits all "Greg Gant" (2026-11-10..12).
README: "unofficial macOS fork and port of Xenia Canary, with a few tweaks from Xenia Edge".
Release macxenos-1.0 (2026-11-12): macXenos-1.0.dmg 56.8 MB + appcast.xml (Sparkle). Beta2..rc2 notes in docs/releases.

Build: CMake 3.20 + Ninja Multi-Config presets (default; debug/release/checked=ASan), xb / xenia-build.py
kept; Windows/Linux build files removed (124b342d), Win/Linux sources kept for merges.
Needs macOS 15+, full Xcode (xcrun metal; Xcode 26 MetalToolchain), brew cmake ninja python molten-vk
vulkan-loader. Apple silicon native; Intel x86_64 slice cross-compiled -> Universal 2. Intel needs AMD dGPU >= 4 GB.
Configure flag -DXENIA_GPU_METAL=ON. Dev signing: codesign with src/xenia/app/xenia.entitlements.

Licence: LICENSE = BSD-3 "Copyright (c) 2015, Ben Vanik" (upstream Xenia text). New files also carry the
Xenia header "Copyright 2026 Ben Vanik ... BSD license". README: icon + Blades UI are Greg Gant's original work,
credit required on reuse. .github/CONTRIBUTING.md (inherited): GPL code forbidden except dev tooling.
Code search: "General Public License" 0 hits; RPCS3 0; Dolphin only in CONTRIBUTING and a PPC test.
Third-party (docs/acknowledgements/manifest.json): FFmpeg LGPL-2.1+ (has207/FFmpeg xmaframes, no GPL parts),
libmspack LGPL-2.1, DXC dxilconv NCSA+Apache-LLVM (fork bourbonicfisky/DirectXShaderCompiler
dxilconv-ios-support-xe), **Apple Metal Shader Converter libmetalirconverter.dylib: Apple licence agreement,
redistribution only for shader conversion** (submodule xenios-jp/metal-shader-converter ios-support-3.1.1),
metal-cpp (bkaradzic fork, Apache-2.0 upstream), MoltenVK/Vulkan loader Apache-2.0, Sparkle MIT,
xbyak_aarch64 Apache-2.0, SDL2 Zlib, rest MIT/BSD/Zlib/BSL/OFL.

CPU: native a64 JIT backend src/xenia/cpu/backend/a64 (from canary: "[ARM64] Initial commit" 0eca0ceb Herman S.
2026-03-20; Wunkolo commits 7374588d FEAT_LSE, c8822d28). x64 backend kept for Intel slice.
Code cache: shm-backed dual views RX 0xA0000000 / RW 0xB0000000 (W^X by aliasing, no pthread_jit_write_protect
seen); MAP_JIT for anonymous RWX (memory_posix.cc:152); __PAGEZERO shrunk to 0x1000.
Entitlements: allow-jit, allow-unsigned-executable-memory, disable-library-validation (+get-task-allow dev only).

Memory (src/xenia/memory.cc, base/memory_posix.cc):
- MapViewsMac(): probe from 8 GB with mach_vm_region for a hole, MAP_FIXED PROT_NONE reserve, then MAP_SHARED|MAP_FIXED
  views of one shm_open object (aliases physical heaps). macOS has no MAP_FIXED_NOREPLACE.
- page_size()=getpagesize() (16 KiB). ShouldSkipHostCommit(): heaps with guest page < host page (4 KiB heaps)
  skip host commit/protect, rely on RW shared mapping + software page table.
- PhysicalHeap: E0000000 heap host_address_offset 0x1000 when granularity > 4K (upstream logic).
  Access callbacks/write watches at SYSTEM page (16 KiB) granularity (system_page_flags_ bitmaps; upstream design).
  ReconcileHostProtection() added because E0 offset not 16K-aligned (Banjo N&B infinite refault).
  51dfd1e5: physical view always writable (GPU/DMA writes ignore guest protect; NG2 crash).
- gpu/watch_admission.h: system page admitted for watch only if all 4 KiB guest pages it covers are writable;
  documents the two granularities explicitly. No 4K emulation trick found; watches are coarser (16K).
- Faults: POSIX signals SIGSEGV+SIGBUS (not Mach exception ports); fault address/WnR from
  mcontext __es.__far / __esr (si_addr unreliable). exception_handler_posix.cc ~177-240.

GPU: native Metal backend src/xenia/gpu/metal (99 files; metal_command_processor.mm 1.1 MB, 517 commits from
2026-07-08 cab6a940; ~758 distinct XE_* env toggles in that file). Path: Xenos ucode -> DXBC (existing D3D12
translator) -> DXIL (dxilconv) -> Apple Metal Shader Converter -> metallib. Geometry/tess: MSC
IRCompilerEnableGeometryAndTessellationEmulation, VS->object stage, GS->mesh stage (metal_shader_converter.cc:323-411).
No ROV: host render-target path with EDRAM emulation in MSL compute (metal_edram_msl.h). BC1/2/3/5 native
when supported, CTX1/DXT3A CPU-decoded (metal_texture_cache.mm ~60-90, 258). Resolution scaling, MetalFX/FSR/CAS.
Presentation CAMetalLayer, vsync implicit (metal_presenter.mm ~205-232). Vulkan via MoltenVK kept for comparison.
docs/dev/metal-explainer.md: Apple interpolator not plane-exact -> derivative/LOD lattice; noperspective issue.
Compat: 151 games auto-tested (M4 mini / M4 Pro); MCLA "Free roam, 30 fps M4 mini". Commit 91881b4e: Canary
b709f6655 texture point snap regresses 545407F8 (MCLA) -> cvar off by default.

Platform: threading_posix.cc (mac branches), ticks=mach_absolute_time, clock CLOCK_MONOTONIC_RAW; P-core pinning
(QoS); audio apu/sdl (SDL2), XMA via FFmpeg; input hid/sdl + GameController home button; UI AppKit + WKWebView
Blades (React) replacing ImGui dialogs; Sparkle updater; signed+notarized.

Commits read:
- 8c2d95cc semaphore hand-off (POSIX only; counted condvar let a re-waiting thread steal tokens; NG2 hang).
  XE_SEMAPHORE_HANDOFF. NT/Wine semantics already hand off -> not a Windows bug.
- fbfd69fa alertable waits: APC wake via per-thread alert instead of signal (Quantum Theory).
- 7d67e4dd XE_CP_SENTINEL: check guest words before every PM4 packet, log last 64 packets; SCRATCH reg change log.
- 2c461904 WAIT_REG_MEM stall -> commit open command buffer holding armed memexport (BF3).
- 2a2dec60 stuck-wait logs SCRATCH regs.
- b0b75d2b a64 db16cyc pause census + optional backoff (one guest thread spins 0.7-1.0 core in D3D fence waits).
- 41a2ccc7 lwarx/stwcx same-value store does not claim granule (USF4 41->60 fps on a64).

Our side (private/l97-sdk-wt 6c5792d): SDK supports win-amd64/arm64 + linux-amd64/arm64 presets; CMake FATAL on
Apple ("Windows and Linux only"). Has src/graphics/vulkan, src/ui/vulkan, *_posix.cpp in src/core (memory, threading,
fiber, exception handler SIGILL+SIGSEGV only, no SIGBUS), window_sdl, surface_gnulinux, input sdl. xmemory.cpp has
the same system-page watch logic (2073-2330). Generated code: simde_mm_* (SIMDe) + arm_neon includes; fpscr.h
has aarch64 FPCR path; db16cyc -> simde_mm_pause(). Host: 229 files in src/, 97 *_win.*, 55 include windows.h;
65 generated TUs.
