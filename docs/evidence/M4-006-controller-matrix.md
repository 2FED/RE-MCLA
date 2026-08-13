# M4-006 controller-matrix evidence

M4-006 is closed by three immutable, privacy-safe physical evidence layers on
the same supported Complete Edition route. It is deliberately classified as
split evidence, not as one monolithic run and not as multi-controller proof.

- `20260812-212030-5fc01c73` proves all fourteen standard digital controls as
  exact causal SDL-to-guest down/up pairs, the `F3FF` surface, and six accepted
  left/right/both host-rumble commands. The owner separately reported feeling
  the three patterns; that report was not recorded in the run or independently
  machine-verified. The run then exposed the previously missing indirect guest
  target `0x82554080`, so its abnormal termination remains explicit.
- `20260813-124600-293c07b3` proves all ten analog threshold/extreme/neutral
  pairs (`3FF`) and the causal focus lost/neutral/gained sequence. A later
  post-completion focus callback froze the old reducer before hotplug; the raw
  SDL removal happened only after that freeze and is not claimed as matrix
  evidence.
- `20260813-144406-2c1974da` proves the fresh hotplug continuation: slot-0
  removal, guest `DEVICE_NOT_CONNECTED`, slot-0 reconnect, guest success, one
  exact PASS summary, and controlled `WM_CLOSE -> Execution complete -> hard
  exit`. It emitted no current digital, analog, focus, input-ready, or nonzero
  rumble evidence.

The final run completed physically but the original post-run verifier still
expected `30025 mappings`. The bounded `sub_82554080` repair correctly changed
the generated dispatch count to `30026`, so JSON aggregation stopped after the
controlled exit. The recovered-evidence verifier binds the untouched run tree
(`3C60720AFE4D3B7935C0BDBD7DBC3EC74EC13A6145C49C8261B6B6CFE9FD59CE`),
three-log manifest
(`F6FBC73CA6303FD2B4BE3D5498FF575B2FE3D64A82ED92E80BAC6EE1DDCD8F19`),
title BMP
(`9262236A7F5D8C000315F1161E344E81E226F4C1BC8BA0C4CECF7ADC065815F4`),
clean ReXGlue `0.9.0.13` install, focused 13-case/90-
assertion test log, and clean 71-action host build. It records
`hotplug-pass_recovered-after-verifier-drift` and does not claim a runtime-
artifact snapshot that the interrupted aggregator never wrote.

Validation:

```powershell
scripts\test-controller-matrix.ps1
scripts\verify-controller-matrix.ps1 `
  -RecoveredHotplugEvidenceRun 20260813-144406-2c1974da `
  -RecoveredHotplugEvidenceOnly
```

The fixture suite passes one immutable digital positive, one immutable
analog/focus positive, one recovered-hotplug positive, physical and compact
continuation positives, 72 continuation negatives, and two corruption
negatives for each immutable evidence layer. Raw logs, BMPs, controller
identity, and result material remain ignored under `private/evidence/M4-006/`.

Scope exclusions: standard XInput values are audited raw against documented
dead-zone thresholds rather than host-filtered; multi-pad behavior is unit-
tested but not physically claimed; rumble is a host diagnostic rather than
title-driven force feedback; the historical gameplay-transition blocker is
preserved rather than laundered into a successful exit.
