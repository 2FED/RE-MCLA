# M2-015 Complete Edition Xenia patch byte audit

Date: 2026-08-11
Result: ALL PUBLIC ADDRESSES MATCH; NO PATCH ENABLED

## Source identity

- Upstream: `xenia-canary/game-patches` commit `84d6682caf1b75b2fdb7adcd197c6559c09b2ed4`
- File: `545407F8 - Midnight Club Los Angeles (Complete Edition).patch.toml`
- Upstream file SHA-256: `AA9873984BEAE91FD68152CBFC34A07A17D1E4C6231B88F749048489728B06B6`
- Declared module hash: `1984A3354B78CE19` (exact pinned Xenia baseline match)
- Declared Media ID: `5940C9DB` (exact local dump match)
- Local XEX SHA-256: `C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432`
- Private Ghidra loaded-byte audit SHA-256: `CF2EC8252E3B670998AB2DF32C0FC05FD2306E5922BC595F01B40EB53E56FDDD`

The upstream file contains six patch groups and eight writes. Every group is explicitly `is_enabled = false`; this task validates provenance and bytes only.

## Address audit

| Patch | Scope | Address/type | Original bytes | Replacement | Original instruction | Patched meaning | Decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 60 FPS - Game Speed Fix | requested enhancement | `0x821BDB08` / be32 | `409A00C0` | `4800012C` | bne cr6,0x821bdbc8 | b 0x821bdc34 | VERIFIED, disabled |
| 60 FPS - Game Speed Fix | requested enhancement | `0x82419AA3` / be8 | `02` | `01` | li r11,0x2 | li r11,0x1 | VERIFIED, disabled |
| Skip Intro | requested enhancement | `0x821F7F64` / be32 | `419A0048` | `48000048` | beq cr6,0x821f7fac | b 0x821f7fac | VERIFIED, disabled |
| Disable Motion Blur | requested enhancement | `0x8260D0B8` / be32 | `4BB7D4B1` | `38600000` | bl 0x8218a568 | li r3,0x0 | VERIFIED, disabled |
| Disable Motion Blur | requested enhancement | `0x8260D0D4` / be32 | `4BB7D495` | `38600000` | bl 0x8218a568 | li r3,0x0 | VERIFIED, disabled |
| Disable Imposter Shadows - Performance Mode | requested enhancement | `0x8230C87C` / be16 | `419A` | `4800` | beq cr6,0x8230ca3c | b 0x8230ca3c | VERIFIED, disabled |
| Disable MSAA | requested enhancement | `0x822E4B80` / be32 | `816A0004` | `39600001` | lwz r11,0x4(r10) | li r11,0x1 | VERIFIED, disabled |
| DbgPrint | diagnostic extra | `0x821BD618` / be32 | `7D8802A6` | `485FF8DC` | mfspr r12,LR | b 0x827bcef4 | VERIFIED, disabled |

Partial writes were validated in their containing words: `0x82419AA3` changes `39600002` to `39600001`; `0x8230C87C` changes `419A01C0` to `480001C0`. The DbgPrint branch target resolves to the existing import thunk at `0x827BCEF4`.

## Decision

- Requested enhancement groups byte-verified: **5/5** (7 writes).
- Additional upstream diagnostic group byte-verified: **1/1** (1 write).
- Rejected addresses: **0**.
- Enabled patches: **0**.
- M8 owns any future opt-in 60 FPS, visual, or skip-intro enablement and behavioral testing.

M2-015 acceptance: PASS. Every current Complete Edition upstream write is byte-verified against the loaded local image and remains disabled.
