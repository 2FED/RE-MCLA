# M1-011 private manifest validation

Date: 2026-08-10

Command:

```powershell
scripts\verify-game-manifest.ps1 -VerifyHashes
```

Result:

```text
Valid           : True
FileCount       : 15
PayloadBytes    : 6569586392
RpfCount        : 4
RpfBytes        : 5989990400
BikCount        : 6
BikBytes        : 560946852
HashesVerified  : 15
SourceIsoSha256 : AFCAAE68593246B1F83328681B690E254A13C37DD2E8DC34AB0DEB6C0F471FDB
```

The validator rejects unsupported manifest schema/source hashes, unsafe or duplicate relative paths, missing or unexpected files, size/total mismatches, wrong RPF/BIK counts, and—when requested—per-file SHA-256 mismatches. Neither the private payload nor its local JSON manifest is tracked.
