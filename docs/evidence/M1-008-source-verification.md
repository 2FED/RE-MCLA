# M1-008 supported-source verification evidence

Date: 2026-08-10

Script: `scripts/verify-source.ps1`

The verifier accepts an explicit ISO path, opens the image read-only, and fails on the first mismatch. It does not depend on an extracted game directory. It:

1. checks the exact ISO byte length and SHA-256;
2. scans for the XDVDFS media header and derives the partition base;
3. parses the root-directory tree to locate `default.xex`;
4. checks the XEX byte length;
5. parses the big-endian XEX2 optional-header table and execution-info record;
6. checks Media ID and Title ID; and
7. hashes the embedded XEX range directly from the ISO.

## Positive verification

```text
Valid             : True
IsoSize           : 7838695424
IsoSha256         : AFCAAE68593246B1F83328681B690E254A13C37DD2E8DC34AB0DEB6C0F471FDB
PartitionOffset   : 0x0FD90000
MediaHeaderOffset : 0x0FDA0000
DefaultXexOffset  : 0x12D85000
DefaultXexSize    : 9252864
DefaultXexSha256  : C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432
TitleId           : 545407F8
MediaId           : 5940C9DB
```

The full positive run completed in 10.3 seconds on the verified host.

## Negative hash fixture

An ignored sparse fixture was created with the expected 7,838,695,424-byte length but zero content. Its SHA-256 was `AC43FEBA8705B26C77509BF649BE99783F6670113392E2DD72178271FF2755D1`.

The verifier rejected it before attempting XDVDFS parsing:

```text
ISO SHA-256 mismatch. Expected 'AFCAAE68593246B1F83328681B690E254A13C37DD2E8DC34AB0DEB6C0F471FDB', got 'AC43FEBA8705B26C77509BF649BE99783F6670113392E2DD72178271FF2755D1'.
```

The exact fixture file was removed after the test; no source or generated game data was tracked.
