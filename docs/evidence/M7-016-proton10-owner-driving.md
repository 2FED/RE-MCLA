# M7-016: owner driving on Proton 10 / Type0 ON

Date: 2026-09-06. **NO-GO for usable stock Deck gameplay/audio.**
The owner reports a slight subjective improvement, but approximately 10–17 FPS
during driving; sound and music still interrupt, stutter and crackle as before.
This is an owner observation, not an instrumented FPS or audio-underflow count.
Controller fidelity was not independently accepted by this report.

## Exact session and outcome

The [Type0 control](M7-016-proton10-type0-control.md) candidate ran once under
installed official Proton10.0-4b / Sniper / FSync, Type0 batching ON,
Vulkan, readback fast and 1280x720 windowed SDL. The nine binaries and shared
runtime manifest are identical to the preceding automated control. No profiler,
automatic input, pause, benchmark or automatic title exit was enabled.
A fresh isolated prefix and complete copied seed/cache live outside the entire
Syncthing root. This direct session does not exercise the portable publisher.

Remote root: /home/deck/MCLA-Diagnostics-20260905/proton10-driving-20260906.
Title PID126813 / birth162224957 exited normally with code0; the harness elapsed time of
251.261980918 seconds includes startup, observation and cleanup. Unified-exec session21340 is finished. No forced
termination, cleanup error, survivor, or source prefix/profile/cache/bundle drift
was reported. The terminal four-file profile remains identical to the seed.
It is retained privately and provides no persisted-progression or successor proof.

## Evidence and review

- Helper: 5A000A7E63B560E3B8724F2A0E95AEFDAAACC289946BD15F94B01BE70ADF5666.
- Preparation: 0248CC0BF4F9FC12A8B9927B69BFB78459AE10F895E389F1DFB474190CD79B5A.
- Fingerprint: 8352C05891749DFF2A28838004C5431256D1E1B387BA195E3729440F8C2FE6C0.
- Launch: 9D706D05934A63FD635B05CE87AE2B2BBA768A82B7080CF5A5DFFD46013969E5.
- Collector: B8FB0B8E1B0C44E12551BF3287E15361570C10FE6D3E255D34EFE3D8DBC478AA.
- Returned archive: 4DDC2D6BDE7A2ECB830762927A25066300F91CD2CA73DCA45B8BAB065B03B2B8.
- Inventory: 873B2662F1507FA1D7228CDEE3E29838833CB19F0740BD8E669344956C5A42D6.

Private return: private/evidence/M7-016/proton10-driving-return-20260906.
All16 regular files /4,493,472 bytes pass bounded archive/member verification.
Primary and independent review reproduce the manifest, all member hashes,
11 loaded-image identities, cached startup-ON evidence, lifecycle and unchanged
terminal seed. The collector prunes prefix/cache before traversal; neither is
part of the returned archive. Startup evidence was frozen before log rotation.

Frozen machine receipts retain their original observation-pending fields.
The owner's subsequent feedback is recorded here, without rewriting raw data.
There is no PCM capture or quantitative underflow counter in this return.
Audio initialization and a normal exit do not contradict the audible failure.

## Disposition

KI-027 and M7-016 remain open, M7 0/16. Do not repeat this unchanged manual
session, promote Type0 by default, or accept the pause gain as gameplay success.
The [implemented and locally validated gather experiment](M7-016-inline-float-gather.md) removes imported bit-scan calls from Vulkan
vertex/pixel float-constant packing, based on the exact residual PC evidence.
It preserves byte order, bank selection, allocation and descriptor/dirty
state, passes an independent scalar oracle, and shows the intended optimized
instructions. Its matched Deck comparison is now complete and shows no cadence gain; see the linked report.
The existing measured candidate, clean SDK install, normal publication and
owner profile remain separate and unchanged.
