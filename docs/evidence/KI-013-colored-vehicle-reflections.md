# KI-013 saturated vehicle-lighting repro

KI-013 is an open S2 rendering defect. It was initially described from a title
frame as a minor green shadow, but a fixed-camera gameplay comparison on
2026-08-17 shows that the affected pixels are vehicle-paint highlights driven
by saturated local lights.

## Native repro

The current Release build was launched from an isolated user root containing the
exact completed M5-012 save (`711B94303445034A3B127F83E8143FF6DBA3FC96C2B6176F8720E49829AD5021`)
without a render probe. The owner held the same rear camera position near a traffic signal
and captured three consecutive 2395-2398 by 1340-1342 PNGs under
`private/evidence/M6-004/diagnostic-colored-reflections-20260817-000206/user-captures`:

- `traffic-light-000523.png`: SHA-256
  `0ECD702B94586BDE2E9C1566B8C19BE45F4796CBD21C3C736FECD1FED0DBE484`;
  the right rear body edge carries a strong red highlight;
- `traffic-light-000529.png`: SHA-256
  `72EA8F650313735F72C4C1E1F0B1EFA325F2402A682FDFDF4247E7197D2D6E9C`;
  after the signal turns green, the same body edge becomes an excessively bright
  neon-green strip;
- `traffic-light-000538.png`: SHA-256
  `D7391B5BA9F963721F178418395E0639FBF91A12DED2DEEDCB9189ECF48A36E7`;
  the saturated strip disappears after the signal phase changes.

The fixed geometry and phase-correlated red/green/disappeared sequence rules out
the original shadow interpretation. It demonstrates an excessive or otherwise
incorrect saturated local-light/specular contribution on vehicle paint.

The owner also observed that this diagnostic process did not offer a usable
signed-in save-progress flow. The filesystem contains the completed input save,
so this was not an empty-root launch, but the session is deliberately classified
as visual-only: it proves neither subsequent save availability nor profile-state
correctness. Those remain M6-006 scope.

## Console references

Two owner-provided console gameplay references show vehicles near mixed red and
green traffic signals without comparable body-color amplification:

- Shirrako full-game console capture, sampled near 4:57:40:
  <https://youtu.be/aPl2Qqjt_vo?t=17860>
- GameRiot Xbox One capture, sampled near 23:45:
  <https://youtu.be/sZqIExDtjSM?list=PLClY3bOF3ZUC35OkM3idqtcKTRuXN1VCT&t=1425>

The recordings are qualitative references rather than pixel-aligned oracle
frames, but they disprove treating the native amplification as intended console
art direction.

## Current diagnosis boundary

The defect is not localized yet. Candidate boundaries include HDR light
accumulation, Xenos `k_2_10_10_10_FLOAT`/7e3 conversion or blending precision,
EDRAM ownership/resolve conversion, and output gamma. Existing M4/M5 telemetry
only proves successful execution of the reached host-RTV, table-gamma, shader,
depth, ownership, and common-copy paths; it does not prove reflection parity or
the unexercised PWL/ROV/true-direct variants. M6-004 owns a controlled
single-variable comparison before any renderer behavior is changed.

A read-only comparison against the current Xenia D3D12 implementation found no
obvious project-only gamma fork: `gamma_render_target_as_unorm16` defaults true
in both, and the accepted route's output audit selected table gamma. The next
diagnostic should therefore keep gamma settings fixed and compare the same
camera/signal phase with explicit `--render_target_path_d3d12=rtv` and `rov`.
The capture burst must record the selected path and, for each bounded
first-seen lighting tuple, the pixel-shader hash, guest and storage render-target
formats, guest MSAA, color-write mask, blend enable/function/factors, ownership
mode, and resolve source/destination formats. Enum values and hashes are enough;
guest addresses, shader bytes, texture contents, and frame pixels remain
private. A visual difference localizes the path boundary; if there is no
difference, shared shader translation and game material data remain candidates.
