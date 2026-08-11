# M3-011 conditional skip-intro decision

Date: 2026-08-11

Status: accepted — patch not justified

## Conditional scope

M3-011 authorizes a known-address development-only skip-intro patch only if
Bink blocks native startup. It does not authorize applying the public Xenia
enhancement merely because its address is known.

M2-015 already established the exact disabled candidate:

- address: `0x821F7F64`;
- original word: `0x419A0048` (`beq cr6,0x821f7fac`);
- candidate replacement: `0x48000048` (`b 0x821f7fac`);
- enabled patch count: zero.

## Unpatched native observation

Final private run: `20260811-143442-e8a011dd`.

The RelWithDebInfo executable ran for a bounded 15 seconds with the exact game
root and isolated user/cache/log roots. It reached the verified XEX/module-launch
boundary and remained alive until the wrapper terminated it and confirmed no
process survived.

The first explicit post-launch prerequisite failure is graphics configuration:

- `VdSetGraphicsInterruptCallback`: no GPU emulation loaded because
  `gpu_plugin` is not set;
- `VdInitializeRingBuffer`: the same no-GPU condition.

After the module-launch marker, the trace contains no Bink name, intro BIK path,
fatal marker, `PPC_UNIMPLEMENTED`, or structured guest crash. Earlier
`intro720.bik` resolutions belong to the project-owned VFS validation that runs
before module launch and are deliberately excluded from blocker classification.

Private runtime-log SHA-256:
`B65D1B8FB4E3B5D6400C49679F5933A295543F81EBAF4362A4C0CE623923F9C7`.

## Gates

- decision verifier: no `mcla_skip_intro`, candidate address, or replacement
  word exists in project source; the disabled M2-015 audit remains intact;
- trace fixtures: one positive and four negative cases passed;
- live observation: module launch reached, two GPU prerequisite markers, zero
  post-launch Bink evidence, zero patch implementation/enabled state, and
  cleanup confirmed;
- complete project PowerShell suite: 25/25 scripts passed;
- ast-grep scan: clean; rule tests: 3/3 passed;
- fresh prerequisite/bootstrap audit: 12/12 checks passed.

The negative fixtures reject a missing launch boundary, a missing GPU marker,
new post-launch Bink evidence, and a guest crash. This makes the N/A decision
fail closed: if Bink becomes observable after GPU integration, M3-011 must be
reopened and the candidate must then be implemented off by default with an exact
original-word guard and mismatch refusal.

## Decision and next owner

Do not implement skip-intro at the current execution boundary. It cannot fix the
observed earlier GPU prerequisite and would obscure startup diagnosis. KI-011
assigns GPU integration/configuration to M3-012/M3-013. Bink is re-evaluated only
after that route advances; enhancement-oriented skip-intro remains an M8 concern
when it is not required for compatibility.
