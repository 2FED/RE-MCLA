# Owner readiness console repair

Owner4 failed because the console required uppercase READY and treated `reaDY`
as cancellation. Backspace changed the buffer but did not erase displayed text.
Version 0.20.27.1 repairs these console behaviors without changing game code,
the native gate, capture selection, or timing limits. M8-001 remains open.

## Preserved failed attempt

`private/evidence/M8-001/game-work-owner4-20261015-1/supervisor/result.json`
records `ValueError: owner cancelled readiness`. Cleanup received the window-close
acknowledgment, exited normally with code 0 and zero active job processes; no
forced cleanup or pending ownership. The native output contains only its failure
receipt, no captured images. This was not a game crash or a gameplay collection.

Before changing sources, all 1852 immutable manifest pins were independently
rechecked. The nine-file MAIN and normal l12 before/after baseline matched.
The consumed supervisor is disarmed; its armed archive and original console
helper/tests remain in `private/guest-work-owner4-preparation-20261015`.
No diagnostic save is merged back into MAIN.

## Corrected input contract

READY accepts any ASCII case and surrounding spaces. Backspace visibly erases
characters (including terminals that encode it as ASCII DEL). Invalid and empty lines clear the buffer and ask again;
they cannot signal readiness, reset clocks, or extend the original deadline.
Explicit CANCEL, Esc and Ctrl+C still stop the attempt through owned cleanup.

The buffer retains at most 16 characters. Overflow or unsupported characters
invalidate the entire line rather than allowing a truncated READY. Extended
console keys are ignored without printing their scan-code bytes. Each supervisor
heartbeat consumes at most 32 keystrokes; event signaling remains exactly once.

## Verification

Before the fix, the regression suite failed on mixed case, invisible editing,
invalid-line retry, explicit cancellation and overflow. After the fix all 13
tests in `scripts/test_mcla_owner_ready.py` pass, including actual kernel-event
collision/cleanup coverage. The 48 owner4 supervisor lifecycle tests also pass.

A real Windows PTY running ConsoleReady received `wrong`, Enter, then
`reaDx`, Backspace, `Y`, Enter. It displayed the retry prompt and corrected
`reaDY`, and returned `CONSOLE_FIXTURE_RESULT READY`, exit 0. No game was launched.
Independent read-only review found no blocker and separately ran 12 pure tests.
The project AST scan and all three AST rule tests pass.

The fresh owner5 package uses the same native binary and a new attempt identity.
Its owner gameplay collection and all-frame scene review remain pending.
