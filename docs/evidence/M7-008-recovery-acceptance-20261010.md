# M7 representative recovery acceptance

M7-008 closes for the representative recovery boundary agreed with the owner:
Restart/Abandon and one recovery route, exercised here as police arrest, fine and
release with restored driving/pause. The owner also confirms the motorcycle and
tuning purchase, and purchases persisted after a fresh-process restart. This is
not a claim that every police, traffic, AI, collision, damage or respawn variant
has been tested. No additional recovery replay is needed.

## Exact tested branch

The isolated `M7-Windows-Stock-Sandbox-20261010-1` starts from an independent full
copy of main profile693B31EB. It uses host
`DCDE2F12014C29277BB76B919ED539119C88BC6D49B816F12F28290EF97E2F70`,
WGC helper `D4894275DC0311B905C8E666E8F585FC2D81883A638194537674C7208AA0C98D`,
RTX5060, stock timing,2x,VSync/fullscreen and local signin state1. The two sessions
ran14:13:12–14:31:19 and14:31:41–14:32:24 Kyiv on October10. They corroborate
restart; semantic recovery/purchase outcomes are the owner's observation, not
invented from save bytes or log counters.

The preserved return is
`private/evidence/M7-004/sandbox-owner-return-20261010-1`,27 payload files,
12,467,392 bytes. Receipt SHA256:
`ECD50AD091A1A311DECCCC69200E885A4C06C43A0A93B410D0D153DA85A5E3F9`.
Full profile/archive:
`5D6CB88BEA5A4867783E6CE2FD861D8FE2836E7EB6599D395E5821C7A9F5B69E`.
Main save:
`8D489F0FF20C5C6B15D448ADDC40F1012B297BE30DFFDDA1B08E0C5A80EF3E38`.
All27 payloads and the complete content-addressed archive were rehashed at
closure; earlier independent integrity and scoped acceptance reviews also pass.
The sandbox is not promoted into the main campaign; its purchases remain separate.

## Limits and review

Exact motorcycle/item identities, police escape, deliberate total-damage reset,
all traffic/AI situations and every respawn variant remain untested. Historical
Race Back camera/pause softlock KI-026 stays open under M7-003, not repaired by
this recovery sample. Partial automatic-exit diagnostics do not establish a new
crash or graceful diagnostics teardown. No agent launched the game or deliberately
interrupted an autosave for this closure.

Independent content audit accepts this representative recovery boundary. The
task commit changes documentation only and does not relabel the tested binary.
Whole-milestone closure, final-package F10, dependency integration and push remain
separate gates.
