# M5-012 race-results transition and restart evidence

Decision: PASS as `first-series-results-return-and-release-restart-pass`.

## Physical route

The accepted private route is `20260816-132209-a316f851`. From the actual
post-OOBE roster, the owner selected Ian and completed the whole available
multi-event series. The title reached all three ordered, operator-confirmed
checkpoints:

- two-car race start (`present_seq=5580`);
- final series rewards/results after every `NEXT RACE` event
  (`present_seq=427185`);
- controllable free roam after leaving results (`present_seq=427649`).

Each checkpoint is a canonical 1280x720 BGRA capture. Their SHA-256 values are
`17F48B882837B67C8A22B5D20C8AD8BEB4F9F79C99451A969F2A7324E74882C5`,
`9843B1E6EA1BF2ED6B474AF52A0CCAB44FF3320EB416D224DA8F5DA850C06EDD`,
and `F07B1157F1451E55E756FC3684D29004D6F67E45E59E94241023A7EF02381A1D`
respectively. The complete route log is
`7493ABD0B2D261F79B052C9AE6197EBB5EB79538779E1C47A4F08EBA7EEC49EB`.
The route exited normally through external `WM_CLOSE`, with no assertion,
unregistered-function, guest-crash, or device-loss marker.

The route was enabled by four minimal runtime-discovered function intervals:
`[0x822C9FE8,0x822CA04C)`, `[0x82264760,0x82264770)`,
`[0x82264770,0x82264780)`, and `[0x82262320,0x8226233C)`. Each boundary is
supported by a private Ghidra audit, non-force generated body, terminal branch,
and dispatcher registration. Earlier failed runs remain private calibration
evidence and are not reclassified as passes.

## Fresh Release restart

The accepted restart result is `20260817-001225-ade395f8`. It binds the route
tree as `A139661BE22E77ADFA284AA288EC0489EB05CCACFDDEF174960B23FA67522603`
and copies the exact completed save
`711B94303445034A3B127F83E8143FF6DBA3FC96C2B6176F8720E49829AD5021`
into a fresh user root. The optimized Release process autonomously reached
controllable gameplay and recorded:

- 300 guest presents / 30,000 mHz;
- 600 vblanks / 60,000 mHz;
- 300 fixed-step timer calls at 33,333 microseconds;
- 999,990-ppm simulated-time/wall ratio;
- 153,615 sampled pixels differing between start and end frames.

The restart runtime log is
`F3C7B5B37C6D99866DAC0921AD97595DE7261A4117DBF213A40EEAE497D600B9`;
the restart tree is
`8CF9C2F91136D82501CCF10C0A57048AE13F45C54C1576EF214F9C1B3021F43E`.
It also exited normally by exact external `WM_CLOSE`.

## Scope

This proves one complete Ian event series, its final results-to-free-roam
transition, and loading the completed save into fresh stock-speed Release
gameplay. It does not claim a fixed number of events, every race/opponent,
whole-frame parity, five repeated completions, or bounded resource growth.
Repeated-race resource evidence remains M5-013.
