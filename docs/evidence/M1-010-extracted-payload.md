# M1-010 extracted payload evidence

Date: 2026-08-10

Destination: ignored `private/game/`

The corrected safe wrapper completed with `Extracted=True`, 15 files, and 6,569,586,392 payload bytes. It verified the source before extraction, wrote through a contained random staging directory, and moved the completed result to the previously absent destination.

An ignored local manifest was generated at `private/game-manifest.json`. It contains schema version 1, the source ISO SHA-256, total count/bytes, and relative path/size/SHA-256 for each extracted file.

Sanitized manifest evidence:

| Relative path | Bytes | SHA-256 |
|---|---:|---|
| `$SystemUpdate/su20076000_00000000` | 7,938,048 | `7375547707B162FA935A56485D299C7EDE1D3628E47CC10E6C5864E96B71C57A` |
| `$SystemUpdate/system.manifest` | 2,100 | `CF2A36938F0F566E482391EEAC23BF35C1BFCBB1E361E2854BC4C440664A1C60` |
| `attract576_16x9.bik` | 63,140,548 | `03812C6EB376F62E37CA570207A767FC58C74B8DD32DFC191B1AB4759EE33924` |
| `attract576_4x3.bik` | 63,136,172 | `9FEA4CFE759245C18E2071AF787BF5D54C64B7710E5A658FB7915065539A6108` |
| `attract720.bik` | 63,168,256 | `8967304E343C26DF93B89C7E049F81F52355FCAC788CDA0BF6E3728E848690F1` |
| `default.xex` | 9,252,864 | `C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432` |
| `intro576_16x9.bik` | 123,833,536 | `BE55B718C804D6E1B9E1DB97A5F70385BD4FD3295979BF6694655EBC70C36239` |
| `intro576_4x3.bik` | 123,831,648 | `71E4B92F78E8F9E2C1C21A68A71B75287EA90224C1B12DBF7903E41CD8703AAB` |
| `intro720.bik` | 123,836,692 | `09435EA27E2117817F2E0E821E3F92C9036B76613B178EB812F65028A1168A79` |
| `nxeart` | 1,425,408 | `C70ACAB80EC383552C0B821BAEFF8BE7012115FD280A9AF01D0E711523656850` |
| `root_directory_padding.pad` | 30,720 | `4C7EEA521D2218C5965FD3666C694A967A939E29A6928D528B6ED1101176B8AC` |
| `xarchive_audio.rpf` | 1,615,757,312 | `5EEA63203E0D5C32C9EC59D68E46F7941171E32283DC56C344A4E31BF4E4726E` |
| `xarchive_audlo.rpf` | 1,463,189,504 | `704B00676EB09E406B5D83ADD366CA6E72D909867E4F627FEE1BCC1C1402BB4C` |
| `xarchive_cache.rpf` | 2,130,739,200 | `F46F9A39E797CC0A2F21730014E18E5762FDC0910595F440031E004CCE2DD420` |
| `xarchive_music.rpf` | 780,304,384 | `4D391ED71F132760BA4EC9CF5F26A58897FF7BC1BD78EE86ACDA3D84213EFF15` |

The source verifier was run again after extraction and returned the original ISO SHA-256 `AFCAAE68593246B1F83328681B690E254A13C37DD2E8DC34AB0DEB6C0F471FDB`, the original embedded XEX hash, and the expected Title/Media IDs. Both `private/game/` and the local manifest are ignored; staged-path audits reject either one.
