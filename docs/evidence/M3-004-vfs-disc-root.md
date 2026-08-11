# M3-004 VFS disc root and write containment

Date: 2026-08-11

Result: PASS — PRIVATE GAME ROOT MOUNTED READ-ONLY AND CONTAINED

## Runtime contract

Before a normal guest launch, `MclaApp::ValidateGameVfsContract()` requires:

- `game:` and `d:` both target `\Device\Harddisk0\Partition1`;
- representative `default.xex`, `intro720.bik`, and
  `xarchive_cache.rpf` entries resolve identically through both aliases and the
  physical guest device;
- each representative entry has its accepted size and reports read-only;
- read opens succeed;
- alias and physical-device paths that traverse above the mount root fail;
- write-open, create, delete, and writable-map requests fail;
- any failed check blocks guest launch.

`--mcla_vfs_probe` runs this contract after the real XEX is loaded and exits
before guest-thread creation. User and cache writes are redirected to a unique
ignored `private/evidence/M3-004/` directory.

## SDK defect and patch release

The audit found three gaps in upstream ReXGlue read-only enforcement:

1. write-intent VFS opens were silently downgraded to a read handle;
2. `Entry::Rename()` did not check the device read-only state;
3. `HostPathEntry::OpenMapped(kReadWrite)` did not check it either.

The same audit found that VFS canonicalized `..` before checking the selected
device boundary, so escape syntax could be clamped into a valid in-root path
instead of rejected. The project fork now checks traversal against the resolved
device root before canonicalization and fails every mutation path explicitly.

- release: `v0.9.0.4`
- commit: `51e997dc44185d3df205c4bd36e8042e99801871`
- subject: `fix(vfs): enforce read-only device containment`

A temporary-directory Catch2 fixture covers alias/device resolution, two root
escape forms, read and write opens, create, delete, rename, read-only mapping,
and writable mapping. The focused `[filesystem]` group passes 26 assertions in
two test cases. SDK Debug, RelWithDebInfo, and Release install successfully;
installed header and CLI both report `0.9.0.4`.

## Private-source integrity

The runtime wrapper snapshots every file's relative path, length, timestamp,
and attributes before and after each process, hashes that metadata, separately
hashes `default.xex`, and rejects any created probe file. Both configurations
preserved:

| Property | Verified value |
| --- | --- |
| File count | 15 |
| Payload bytes | 6,569,586,392 |
| Metadata SHA-256 | `1E0C2F2F1BA026F76731862ECDBF779E8948F4D33F9D253C9FC39AC7245CB625` |
| XEX SHA-256 | `C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432` |
| Full manifest hashes | 15/15 pass |
| Unauthorized game-root files | 0 |

## Final host verification

| Gate | Release | RelWithDebInfo |
| --- | --- | --- |
| Configure/package | exact SDK 0.9.0.4 | exact SDK 0.9.0.4 |
| Compile/link | pass | pass |
| Process exit | 0 | 0 |
| Ordered VFS markers | 5/5 | 5/5 |
| Guest launch | skipped | skipped |
| Executable SHA-256 | `E3EC30F2F7C8DD157E12DBDBD27030232A59DFF3A3568F3D7DCA3EB8964B1867` | `1BD232E8AA152F9C30F613B2B49DAD73784F522E1547965CADA9BE05F8352C0D` |
| Private log SHA-256 | `B6D37251BF438D6F31B74399C8E03EF2C90BE07DDA8B8C020E186694F2544CAA` | `D2EC8065068BC6C329D01D3805A046A878BC4FA211A67FE28E3F75A95CF30A4B` |

Raw logs remain ignored. No host path, profile identifier, proprietary file
content, generated game code, or memory dump is published by this evidence.

## Upstream duplicate audit

All 217 open/closed issues and 183 open/closed/merged pull requests in
`rexglue/rexglue-sdk` were searched for read-only devices, write access,
HostPath/VFS, traversal, mappings, rename, and
`allow_game_relative_writes`. Closed feature #266 enables intentional game-root
writes when configured; open issue #405 covers device-less relative paths.
Neither reports read-only mutation bypasses, and no duplicate was found. A
sanitized new-issue payload is prepared privately and requires explicit owner
approval before publication.

## Acceptance decision

M3-004 is complete. Expected disc files resolve through the guest mount,
root-escape syntax is rejected, every tested read-only mutation API fails
closed, and two real host configurations preserve the full private-source
snapshot without entering guest execution.
