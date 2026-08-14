# M5-005 representative material-pipeline evidence

## Decision

M5-005 is accepted as `representative-material-pipeline-pass`. The immutable
M5-003 saved night free-roam route exercises a broad, successful shader and
texture-load slice, and the owner visually accepted all eight labeled rendering
categories. No shader/texture behavior patch was made because the reached path
did not reproduce a translation, tiling, deswizzling, or material-rendering
failure.

## Immutable evidence

- Accepted M5-003 run: `20260814-104624-fde51a30`.
- Accepted M5-003 result SHA-256:
  `299392E59CB38AFB44256E773884856FF0C869A96D2045086A65396C1ED4EFCA`.
- M5-005 report run: `20260814-120535-ce9e215b`.
- M5-005 result SHA-256:
  `C6394CF63F7554B51CA0417795CDB98C3A4E6A2D949448F42940A3C679498F2F`.
- ReXGlue SDK: `v0.9.0.18`, commit
  `923c92d1d1cb721cb704ac603fba263a01ba06aa`.
- Runtime evidence commit:
  `c7ec3b672ff339228c5e53a805d8a92657642951`.

The final verifier re-hashes and re-parses the accepted M5-003 source game,
pinned save, runtime artifacts, build log, complete rotated logs, all captures,
contact sheet, and owner review. Raw texture traces, guest addresses, captures,
and private paths are not copied into the report or repository.

## Shader and pipeline coverage

| Measurement | Accepted value |
|---|---:|
| Successful vertex translations | 163 |
| Successful pixel translations | 197 |
| Translation failures | 0 |
| Bounded shader detail records | 256 |
| Vertex / pixel detail records | 111 / 145 |
| Explicit shader-record overflow | 104 |
| Successful unique PSOs | 332 / 332 |
| PSO failures / record overflow | 0 / 0 |

Every bounded shader record reports a successful result and a privacy-safe
ucode/modification hash. Every PSO record has a unique description hash,
successful result, and zero HRESULT. The 104-record overflow is explicitly
retained: the gate proves a large representative slice, not a complete list of
all shader variants.

## Successful texture-load coverage

| Measurement | Accepted value |
|---|---:|
| Successful loads | 122,830 |
| Tiled / linear | 122,823 / 7 |
| Packed-mip / unpacked-mip | 35,085 / 87,745 |
| Representative formats | 9 |
| Dimension classes | 45 |

| Format | Successful loads |
|---|---:|
| `k_1_5_5_5` | 18,186 |
| `k_16_16_16_16_FLOAT` | 23,957 |
| `k_24_8_FLOAT` | 3,232 |
| `k_32_FLOAT` | 9,674 |
| `k_8` | 3,049 |
| `k_8_8_8_8` | 27,567 |
| `k_DXT1` | 32,030 |
| `k_DXT2_3` | 17 |
| `k_DXT4_5` | 5,118 |

The source audit confirms that the common `Loaded` event follows successful
texture load completion. On D3D12, the tiled flag feeds the load constants
before compute dispatch, the output is copied into the texture resource, and
only then can the load report success. The accepted trace has no invalid texture
fetch, texture create/load failure, Xenos audit failure, fatal marker, or D3D12
device loss.

## Visual and scope boundary

The owner visual pass covers road, buildings, player vehicle, AI traffic, night
sky, shadows, particles, and HUD in the accepted 1280x720 saved free-roam slice.
It supports recognizable, usable representative materials in combination with
the successful shader/texture trace; it is not a pixel-perfect texture oracle.

This task does not claim:

- every guest texture or shader format and variant;
- raw texture bytes or addresses as public evidence;
- every city area, material, weather, or time of day;
- whole-frame Xenia equivalence;
- that a behavior patch was needed or applied.
