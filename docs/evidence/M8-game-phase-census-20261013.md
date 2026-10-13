# M8 game phase census and worker render route

The bounded stock census observes rendering on a separate worker thread, with
swap callbacks inside explicit render scopes. It does not yet bind those scopes
to individual captured source tokens or to simulation updates. M8-001 stays open;
the normal l12 build and MAIN save are unchanged.

## Offline decoder acceptance

`scripts/mcla_phase_census_decode.py` validates immutable bounded JSON, exact
schemas, run token, source window and QPF before interpreting explicit TLS
references. Parent zero means unobserved ancestry, not a proven root. Invalid,
incomplete or overflowed journals remain inconclusive. No timestamp-nearest
matching, image access, native admission, FPS or simulation claim is emitted.

Its21 synthetic tests pass. Combined regression passes153 tests. The supervisor
also passes26 tests, including a real phase-sidecar success and malformed final
scope/callback rejection before any image read. Native integration remains
separate from the offline decoder commit.

## Isolated preparation

Fresh165-file host snapshot `private/host-phase-s` uses immutable `sdk-swap-s`.
The initial full build completes794 steps. Three source files then receive
review corrections for clock admission, unknown ancestry and concurrent drain.
The original files and before/after hashes remain in `host-phase-initial-source`
and `host-phase-correction-receipt.json`.

Copying preserved old modification times, initially causing an incorrect Ninja
no-op. After verifying content hashes, advancing only those three input times
forces a seven-step corrected compile/link. Corrected native tests pass110 phase,
129 swap and973 scout checks. AST scan and three rules pass. The link map selects
all three strong public wrappers and preserves their distinct originals; startup
also verifies the live dispatch entries. Independent source/package review passes.

Package `private/gphase1-bin` host is52839424 bytes, SHA256
`C18F26CC69C7259750CF53D272BDA1FE985DB1CE14C7AA78D097B58A18646DC3`.
Fresh missing/invalid/expired-ticket rejection tests pass with originals unchanged.
Preparation and final165-file source receipt are under
`private/guest-phase-preparation-20261013`. All113 final input pins were verified
by both reviewers before disabling the consumed entry gate.

## Actual stock census

Evidence: `private/evidence/M8-001/game-phase-scout-20261013-1`.
Only an isolated copy of the historical six-file seed was writable. No title
input, desktop capture, game-setting change or normal-save write occurred.

PID34812, birth134363720259091806, closes with normal WM_CLOSE acknowledgement,
exit0 and Job0. No forced cleanup, cleanup error or pending ownership. Seven loaded
package modules and six unchanged seed files verify. Primary and independent
replay of all91 BMPs and three metadata analyses agrees with the stored result.

| Observed scope | Count | Native thread | Caller LR |
| --- | ---: | ---: | --- |
| Update | 243 | 44300 | 0x821316A0 |
| Coordinator | 243 | 44300 | 0x821316B8 |
| Render | 243 | 36032 | 0x82305F80 |

All729 scopes complete; two exits just beyond the window are retained and drained.
All240 swap callbacks reference an active render scope with matching context/base.
No scope/callback drops, overflow or observer health faults occur. The observed
render caller matches the statically inspected worker route. Equal counts do not
associate individual updates, coordinator calls and renders across threads.

All91 sampled sources have available swap bindings; sources1065 through1282 are
distinct. SDK statistics are240 issued/239 matched, zero rejected/callback failures;
these statistics are not quiescent. Images00/45/90 show the Hollywood-sign/highway
PRESS START scene with changing traffic, not gameplay or a controlled scene match
to earlier runs. The passive scene is not a performance benchmark.

| Artifact | SHA256 |
| --- | --- |
| Supervisor result | EBFA68F6B4984A9AC77F0C51A19B29F20B99E19FAC988A58C2C87CF77D37BD53 |
| Source receipt | 21B1174442658475352A22BC6FA7A1FB1438BAD140DB6E7E095FF313FE81CD35 |
| Swap sidecar | D6DF1D55072F0E4966B0D42ED08A5D46378813B5C44680F4D64EADABB482B7B3 |
| Phase sidecar | 98B84BB75CD91F5C3C57ADD0F30F259E29F0807CBC84F3105E594EC7DE07D3BE |

The original receipts retain REVIEW_PENDING. This audit accepts only the bounded
observations above. `mcla_game_phase_scout_probe.py` is disabled and the attempt
is consumed; never rerun it.

## Next binding

The existing SDK callback runs before token issuance and receives no token.
Neither current statistics nor a latest serial can provide identity. Next, add
an explicit callback ID through a fresh SDK snapshot and versioned metadata,
preserving existing timer meanings. Callback order can differ from issuance
order; join only exact IDs, leaving rejected/unavailable cases as holes.
The subsequent worker semaphore handoff remains a separate causal proof.
