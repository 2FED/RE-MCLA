# M1-007 XDVDFS extractor review

Date: 2026-08-10

## Selection and provenance

- Project: XboxDev `extract-xiso`
- Upstream: `https://github.com/XboxDev/extract-xiso`
- Release: `build-202505152050`, published 2025-05-15
- Immutable release commit: `b72e5b60d598ec6df80534cda19cdcd4361aa18c`
- Asset: `extract-xiso-Win64_Release.zip`, 23,045 bytes
- Asset SHA-256: `FEC88D03C7EFD6205AB09BE4ABBA70C0AFD0EB27A5709F0A6235B828BA5AC11E`
- Extracted executable SHA-256: `7C7AF9C17E095C3C1E78E644DF5F0E72F01C4690B3117F038AAFE26EB5A8A2F4`
- Reported program version: `extract-xiso v2.7.1 (01.11.14)`
- License: modified four-clause BSD license included as `LICENSE.TXT` in the release asset
- Local install: ignored `private/tools/extract-xiso/`

The source at the release commit was reviewed around command-mode selection and ISO opening. List mode is selected by `-l`; Windows `READFLAGS` is `O_RDONLY | O_BINARY`; `decode_xiso` opens its input using `READFLAGS`. Rewrite mode `-r` is a distinct path that may rename or replace the source, and `-D` may delete the old image.

MCLA-R automation may invoke only list (`-l`) or extract (`-x`) modes. It must never invoke `-r`, `-D`, create mode, or any unreviewed option combination. Source verification must happen before extraction, and the source ISO hash must be checked again afterward.

## Supported-image read-only test

Command shape:

```powershell
private\tools\extract-xiso\artifacts\extract-xiso.exe -l `
  Midnight.Club.Los.Angeles.The.Complete.Edition.XBOX360\midmets4.iso
```

Result: exit code 0, 15 files, 6,569,586,392 payload bytes. Inventory:

```text
$SystemUpdate/su20076000_00000000                 7,938,048
$SystemUpdate/system.manifest                        2,100
attract576_16x9.bik                             63,140,548
attract576_4x3.bik                              63,136,172
attract720.bik                                  63,168,256
default.xex                                      9,252,864
intro576_16x9.bik                              123,833,536
intro576_4x3.bik                               123,831,648
intro720.bik                                   123,836,692
nxeart                                           1,425,408
root_directory_padding.pad                          30,720
xarchive_audio.rpf                            1,615,757,312
xarchive_audlo.rpf                            1,463,189,504
xarchive_cache.rpf                            2,130,739,200
xarchive_music.rpf                              780,304,384
```

The source ISO was hashed immediately before and after list mode. Both SHA-256 values were `AFCAAE68593246B1F83328681B690E254A13C37DD2E8DC34AB0DEB6C0F471FDB`; its 7,838,695,424-byte size and UTC last-write timestamp were also unchanged.

The binary and archive hashes above must be validated by the future non-destructive `bootstrap.ps1`. The actual extraction wrapper, containment checks, and overwrite policy are separate M1-009 work.
