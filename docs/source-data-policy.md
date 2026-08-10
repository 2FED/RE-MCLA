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
- Put generated recompilation output under ignored `generated/` paths.
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

Update this document whenever a new artifact type, tool, diagnostic format, package path, or distribution channel is introduced.
