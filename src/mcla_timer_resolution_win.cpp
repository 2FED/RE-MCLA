// Request 1 ms Windows timer resolution for the whole process.
//
// ReXGlue's emulated guest vblank thread sleeps 1 ms per loop, and the GPU
// command processor sleeps while a WAIT_REG_MEM waits for that vblank. With
// the default ~15.6 ms timer resolution each of those sleeps can last a whole
// tick, so vblanks arrive in bursts and frames slip: about 24 updates/s with
// VSync on. At 1 ms the same build reaches 29.6 updates/s and the stock 1/30 s
// timer runs at 0.98x real time. See docs/evidence/M8-001-timebox-20261025.md.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <timeapi.h>

#pragma comment(lib, "winmm.lib")

namespace {

// Process exit releases the request; no timeEndPeriod is needed.
const bool kTimerResolutionRequested = timeBeginPeriod(1) == TIMERR_NOERROR;

}  // namespace
