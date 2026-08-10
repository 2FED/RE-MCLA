# M1-014 reverse-engineering and graphics tools

Date: 2026-08-11
Result: PASS

## Selected toolchain

The project pins the mutually compatible reverse-engineering stack below instead of mixing the newest Ghidra release with an extension built for an older API:

- Eclipse Temurin JDK `21.0.12+8`
- Ghidra `12.0.4 PUBLIC` (`Ghidra_12.0.4_build`)
- SaveEditors XEXLoaderWV `13.0.0`, whose release explicitly targets Ghidra 12.0.4 and JDK 21
- RenderDoc `1.45.0`

Ghidra and XEXLoaderWV are installed below ignored `private/tools/ghidra/`. RenderDoc and the JDK are installed machine-wide through WinGet. The Ghidra `launch.properties` in the ignored local install points at the pinned JDK so headless runs do not depend on a stale shell `PATH`.

## Immutable inputs

| Artifact | SHA-256 |
| --- | --- |
| `ghidra_12.0.4_PUBLIC_20260303.zip` | `C3B458661D69E26E203D739C0C82D143CC8A4A29D9E571F099C2CF4BDA62A120` |
| `ghidra_12.0.4_PUBLIC_20260325_XEXLoaderWV.zip` | `498B9C2A2430585CC49A13DB33603B6A46CFE84B157985F9BE2C4360F917FA5A` |
| Installed `XEXLoaderWV.jar` | `6B0B2B470DF64300A0AE6E421A2593EA421FDB2795C018C8A21EEF92C9F3D339` |
| Installed `renderdoccmd.exe` | `273352017E23E890FE9134DE0157D1FE556676A4C6004BFE3265DB1A4648ED07` |
| Installed Temurin `java.exe` | `A38D821EFB69EF99C55D315B00D9E8B88F126743B8773E44154E1A3D193EFD41` |

The two archive hashes match the digests published on the corresponding GitHub release assets:

- <https://github.com/NationalSecurityAgency/ghidra/releases/tag/Ghidra_12.0.4_build>
- <https://github.com/SaveEditors/XEXLoaderWV/releases/tag/13.0.0>

## Representative-target smoke test

Ghidra's headless analyzer imported ignored `private/game/default.xex` into a temporary ignored project with `-noanalysis -deleteProject`. The run reported:

- `Using Loader: XEX Loader by Warranty Voider`
- `Using Language/Compiler: PowerPC:BE:64:A2ALT-32addr:default`
- retail image decryption and basic compression processing succeeded
- image base `0x82000000` and entry point `0x821322B8`
- `.rdata`, `.pdata`, `.text`, `.data`, `.tls`, import, relocation, and game-specific sections loaded
- 20,071 function symbols/functions and 257 import references were created
- `REPORT: Import succeeded`

The sanitized Ghidra log remains local at `private/tools/ghidra/logs/mcla-xex-smoke.log`. It contains no game bytes. Project data and caches remain ignored beneath `private/tools/ghidra/`.

RenderDoc was verified independently with:

```text
renderdoccmd x64 v1.45 built from 2fc0bc04cb95499635f63986a55bc6f67849dd9f
```

Capturing Xenia or the future native port is intentionally deferred until a graphical baseline/application exists. This task establishes and pins the capture tool; M2 and later milestones own representative captures.

## Review notes

- Ghidra 12.1.2 was not selected because XEXLoaderWV 13.0.0 is published specifically for Ghidra 12.0.4. Updating either side requires a repeated headless retail-XEX import.
- No source ISO, XEX, decrypted image, Ghidra project, cache, or RenderDoc capture is tracked.
- The test used the supported Complete Edition XEX hash already enforced by `scripts/verify-source.ps1`.
