# Source-data and sensitive-artifact policy

Owner: MCLA-R maintainers

Purpose: prevent copyrighted game data, credentials, private machine data, and unsafe diagnostic artifacts from entering Git history or public packages.

## Never track or distribute

- Xbox 360 ISO/DVD images and descriptors
- XEX/XEXP executables and patches
- RPF archives, Bink videos, DLC, title updates, save exports, or extracted game assets
- generated C++ or binaries that reproduce proprietary game code unless an explicit legal review approves distribution
- encryption keys, signing material, credentials, tokens, browser cookies, or private service configuration
- raw guest-memory dumps, GPU captures, audio captures, or traces containing copyrighted memory/pages
- logs containing unredacted usernames, local paths, account identifiers, or secrets

## Required storage boundaries

- Put local game inputs and extracted data under ignored `private/` paths.
- Put generated recompilation output under ignored `generated/` paths. The sole tracked exception is ReXGlue's non-proprietary `generated/rexglue.cmake` build bootstrap; guest-derived headers, sources, source lists, caches, and binaries remain ignored.
- Put build and package output only in ignored build/artifact roots.
- Keep raw logs, dumps, captures, and traces in ignored local directories.
- Commit only sanitized evidence that is necessary to reproduce a result and safe to publish.

## Before staging

1. Review `git status --short --ignored`.
2. Stage explicit paths; never use broad staging without reviewing the resulting index.
3. Review `git diff --cached --name-status` and `git diff --cached`.
4. Reject extensions and paths listed by `.gitignore` and this policy.
5. Scan staged content for credentials, private paths, large binary blobs, and game identifiers beyond documented hashes/metadata.

## Before release

- Scan the full Git history and release staging directory, not only the current working tree.
- Produce an archive inventory and checksums.
- Verify the package starts with no bundled game data and requests a user-supplied supported dump.
- Test wrong hashes and unsupported revisions for safe rejection.
- Verify update/uninstall logic preserves the user's source image, extracted game data, saves, and unrelated files.

## Logs and evidence

Sanitize host usernames and absolute private paths. Prefer counts, hashes, symbolic guest addresses, and minimal excerpts over complete raw logs. Do not publish crash dumps or GPU captures unless they have been reviewed for proprietary memory and personal data.

When sanitization would make evidence misleading or incomplete, keep it private and commit only a summary with reproduction commands.

The private M8 exact-clock/swap-source diagnostics include copied saves, rendered
images, guest timer/packet words, token/callback/source identities and raw phase/
clock graphs. Keep WHOLE raw sidecars and immutable dependency archives under
ignored private evidence, including unknown/unavailable/failed records; publish
only reviewed aggregate descriptions and project-authored decoder/fixture code.
Do not upload raw gameplay, saves or these sidecars to routine model workers.
An accepted hash/derived inventory does not make the source raw disposable or
reconstructible; apply the active retention report before any cleanup.

MCLA-R live and native-crash packages are always local and explicitly marked
unsafe to share as a whole publicly or with anyone outside the project; sending a report folder to the project owner over a private channel is the intended volunteer flow (M9-005), and the owner reviews dumps, saves and images before anything goes further. Their manifests distinguish public metadata from
private dumps, rotating runtime journals, screenshots, and save snapshots;
every copied save file has a bounded SHA-256 inventory. The application never
uploads a package. Ask for or send the package folder path first, then inspect
and share only the individual artifacts required for diagnosis. Neither
`diagnostics/live`, `diagnostics/crash`, nor their latest-package pointers are
release inputs or publishable evidence roots.

The launcher's metadata export (written in M8-018, shipped through M9-002 to
M9-005) constructs a new typed report instead of copying diagnostic source files.
Its two-file local output contains only the explicit metadata projection and a
checksum receipt. It admits Stock and Unlocked sessions whose capability record
re-serializes exactly, including owner-file and signed-ticket admissions, and it
may add a numeric per-minute performance summary parsed from four counter lines
of the private log tail; no other log text is copied. The per-package
`report.txt` tester answer file is not a manifest artifact, and the export never
reads it. The export still requires visible preview and confirmation, followed
by review before any manual sharing. It does not authenticate source provenance,
certify excluded artifacts, export saves, or make the original diagnostic folder
shareable. No raw attachment is included. See
[local metadata export](diagnostic-metadata-export.md).

Private timing experiments may also retain exact clock-read tuples, caller
binding/status journals, experimental SDK DLLs/objects and precompile dependency
archives. Keep these under ignored private roots with their source/license,
input/output hashes and explicit proof limits. Dependency archives can contain
third-party/system headers: they are reproduction evidence, not distributable
source packets or release inputs. Never publish a whole experiment directory or
send its contents automatically to Spark. Routine Spark packets contain only
explicitly selected, in-scope project source, excluding secrets and unrelated
machine data; engineering acceptance remains local. A numeric clock tuple or a
source-consistency PASS is not proof of complete clock history, frame identity,
physical simulation or FPS. Preserve unique raw inputs while the linked timing
investigation remains open.

Update this document whenever a new artifact type, tool, diagnostic format, package path, or distribution channel is introduced.
