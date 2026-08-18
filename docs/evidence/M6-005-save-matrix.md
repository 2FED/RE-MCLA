# M6-005 save matrix

Decision: `native-autosave-load-overwrite-plus-isolated-creation-destructive-save-matrix-pass`

The canonical build-only run is `20260818-201702-5d089d9c`; its private
`result.json` SHA-256 is
`5F46FA6657F39EE5AF990BD505B67D5EDBE6A781E8283D1D51BE07F3543F169E`.
No game process or controller interaction was required.

## Evidence

- Accepted M6-002 result `20260817-155005-1dd57bd3` was rehashed at
  `21FCBE38695B45D22AE516E33E51AD0A292286985549673811E0FE3E33900644`
  and physically re-parsed. Its two-process save chain proves native autosave
  overwrite, exact process handoff, fresh-process load, and controlled
  external exits.
- Exact ReXGlue `v0.9.0.23` commit
  `2473e42d76f6aa0081c93e6745e91f4b35b393fa` clean-built and installed.
  The focused save suite passed 10 test cases / 117 assertions.
- Isolated temporary-root tests passed for truncated and identity-mismatched
  metadata, interrupted existing/new saves, idempotent interrupted recovery,
  marker-removal commit semantics, live enumeration, committed overwrite, and
  full/oversized dummy-HDD requests.
- The HANGOUT seed save remained
  `126F7482878C7AACB09AA6795331C906DFB9C4218BE94EDB1D8E51B27CA78AB2`
  and its header remained
  `5827A913515AC0E5D55BB56AEC56DE99CACC0ABB7C8061F59336DF4CEA4A8731`
  before and after all destructive tests.
- The Release host clean-built against v0.9.0.23, and the verifier rehashed the
  four staged runtime artifacts plus SDK/build/test logs and source manifest.
- Parser/source/fixture gates passed with one positive, 36 fail-closed negatives,
  and 28 source checks. Main ast-grep scan and all 3 rule tests passed.

## Scope

This closes native autosave/load/overwrite plus isolated creation, interruption,
corruption, and storage-full behavior. Clean-save creation is not represented as
native M6-002 evidence: it is the stock-Xenia baseline plus the SDK
creation/restart roundtrip. The reached title route has no applicable manual-save
action, so none is claimed. First-run OOBE, account UI, and generalized
profile/settings persistence remain open under M6-006 and M7-002.
