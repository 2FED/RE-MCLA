#pragma once

#include <cstdint>

// l95 (M11, docs/evidence/M11-l95-sdk-low-latency-20261112.md): the host side
// of the SDK low-latency layer (NVIDIA Reflex / AMD Anti-Lag 2, default off;
// d3d12_low_latency_mode in SDKs from l95). The host owns the game's frame
// timer hook (mcla_raw_timer.cpp), which is the simulation-start / sleep
// point; it calls this before the original timer runs on the guest main
// thread. The SDK entry points are resolved by name from rexruntime.dll, so
// the host still builds and runs against an SDK without them (the call is
// then a no-op). No lock is held by the host around the call.

// The simulation-start / sleep point. Cheap while the mode is off (one
// indirect call that reads an atomic).
void MclaLowLatencySimulationStart();

// l99 (docs/evidence/M11-l99-sdk-hitch-20261113.md): the SDK's MCLA_HITCH and
// video-memory entry points (rexruntime.dll l99 and later), resolved by name
// like the low-latency ones; with an SDK without them every call is a no-op
// (the sequence reads 0). One MCLA_HITCH_HOST kind=entry line at the first
// call.
// The number of MCLA_HITCH lines the SDK has written (lock-free).
uint64_t MclaSdkHitchLoggedSequence();
// The SDK's last hitch; false without the entry point or before a hitch.
bool MclaSdkHitchLast(uint64_t &seq, uint64_t &logged_seq, uint64_t &frame,
                      uint64_t &interval_us);
// The host's DXGI video-memory sample (MB) for the SDK's MCLA_HITCH
// vram_over_mb and MCLA_VRAM_SDK lines.
void MclaSdkVramHostSample(uint64_t budget_mb, uint64_t usage_mb, uint64_t nonlocal_mb);

// l116 (KI-087, docs/evidence/M11-l116-sdk-guard-and-thin-frames-20261119.md):
// the play state the SDK's lead-2 guard, MCLA_HITCH / MCLA_DIP cause and
// thin-frame probe use (rex_play_window_focus / rex_play_game_paused,
// rexruntime.dll l116 and later), resolved by name like the others; with an
// SDK without them every call is a no-op. One MCLA_PLAY_HOST kind=entry line
// at the first call. The window focus (UI thread, at every focus event).
void MclaSdkPlayWindowFocus(bool focused);
// The game's own pause / menu-limiter state (the menu-state hook, at every
// change of the paused flag).
void MclaSdkPlayGamePaused(bool paused);
