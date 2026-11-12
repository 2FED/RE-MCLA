#pragma once

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
