# M6-006 profile/settings persistence

Decision: `isolated-profile-controller-language-restart-matrix-pass`

The canonical build-only run is `20260818-211448-f6ec1436`; its private
`result.json` SHA-256 is
`D219DA53F394B2B480E808760D195894CFFE5208EBCB0B0D2223A2F86F44B410`.
No game process or controller interaction was required.

## Evidence

- Exact ReXGlue `v0.9.0.24` commit
  `1e4dbc0040c1eebbf78dca0b5679ac64f99b9f4d` clean-built and installed.
- Seven XAM profile cases / 40 assertions verify deterministic local defaults,
  standard scalar and Unicode restart, mismatched-metadata rejection, recovery
  from a committed backup after interruption, and separation of title-specific
  binary slots from the global profile root.
- Twenty-six CVar cases / 128 assertions verify TOML serialization, save/load,
  validation, lifecycle, restart tracking, and late flag registration. Static
  binding identifies `user_language`, `input_backend`, and `mnk_mode` as the
  language/controller preference keys.
- Immutable M4-004, M4-010, M5-006, and M6-005 results rebind the physically
  reached local-profile, language, controller, and save routes.
- The canonical HANGOUT save remained
  `126F7482878C7AACB09AA6795331C906DFB9C4218BE94EDB1D8E51B27CA78AB2`
  and its header remained
  `5827A913515AC0E5D55BB56AEC56DE99CACC0ABB7C8061F59336DF4CEA4A8731`
  before and after the isolated tests.
- The Release host clean-built against v0.9.0.24. One positive fixture, 41
  fail-closed negatives, and 30 source checks passed.

## Scope

MCLA does not import `XamUserWriteProfileSettings`; therefore this result does
not claim a native title write or a previously zero-hit profile-read route.
It also does not claim first-run OOBE, account selection, multiple profiles, or
general host configuration. Those boundaries remain M7-002 and M6-011 work.
The result proves isolated platform persistence/recovery and binds it to the
already accepted physical profile, language, controller, and save routes
without replaying or modifying the user's game progress.
