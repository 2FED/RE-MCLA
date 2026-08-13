# M4-010 locale selection and Unicode paths

Status: PASS

The exact-release canonical private run `20260813-213301-505ca1ca` completed one clean SDK
install, focused locale/filesystem tests, one clean host build, and isolated
EN/US (`1/103`), FR/FR (`4/34`), and RU/RU (`12/88`) title cycles. The sanitized
result SHA-256 is
`458B742FC1112DDAF39BDC8B81935CC8F2F9C616477B258B1E6FBA10058E096A`.

Each cycle returned the selected language through XConfig 25 times with no
failure or mismatch. `XGetLanguage` and XConfig country were zero-hit at this
pre-input title boundary; their selection/range behavior is covered by the
focused 3-case/22-assertion locale tests and is not claimed as runtime-reached.
The filesystem subset passed 3 cases/32 assertions, including a real
non-ASCII filename open.

All captures were nontrivial 1280x720 Complete Edition title frames. Logo edge
correlations were 0.983471-0.998879. EN retained the pinned stock `PRESS` gate;
FR `APPUYEZ SUR` was compared against a pinned private locale reference with
correlation 0.987314. The RU title route retained English `PRESS` and passed the
stock prompt gate rather than being misreported as translated. Normalized edge
comparison remains stable across the title animation's light/dark fade.

The physical cycles used exact non-ASCII roots equivalent to
`user-Локаль-é`, `cache-київ`, and `logs-Локаль`. Every cycle exited 0 through
the exact application window's `WM_CLOSE`, required no force cleanup, left no
canonical process, and preserved the source-game tree plus four runtime
artifacts. Private logs, BMPs, references, and result JSON remain ignored.

This closes the title-reached language and host Unicode-path boundary only. It
does not prove complete localized menus, subtitles, voice, or gameplay; those
remain M4-011/M4-012 and M7-010 scope.
