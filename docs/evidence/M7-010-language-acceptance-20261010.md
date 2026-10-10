# M7 English gameplay support boundary

M7 claims English UI/gameplay only. The owner's local Windows RTX5060 stock
gameplay and progressed-profile reload are accepted; the final package's F10
image directly confirms English in-game phone text and UI. Other disc languages
are untested for M7 gameplay. No localization, translation completeness or every
subtitle/voice-language combination is claimed.

## Artifact bound observation

The isolated p1 host is
`82A69B5E922DE80726F76B2345C9FFE1FE143FFB485E4269FB1586DE9230509A`,
with runtime `542DD347B56B0494F42A7B261EC8309897E867C14FE1D2456B1836AE6C12650F`.
The owner explicitly pressed F10 and saw recording complete on October10.
Preserved return `private/evidence/M7-002/profile-f10-return-20261010-2` contains
the package `diagnostics/live/live-20261010T124941Z-42540-1`.
Its1920x1080 gameplay image SHA256 is
`19F2FEE7D7ADE81811748B733154573006357EAE55EE950AA3DE277058CD11C2`.
Primary visual inspection shows a car on a city street with traffic and Karol's
English phone message, including the Mission Log label. This is gameplay UI,
not merely an English desktop launcher or an assumed language from the OS.

The packaged configuration and wrapper add no language override. The inspected
SDK defines `user_language=1` (English) by default. The visual observation is the
direct support evidence; absent environment/config logging is not used to claim
that every language input source was instrumented.

The current owner separately confirms displayed renamed profile and progress
after restart, pause/audio and Alt+Enter. No additional language replay is needed
to support this English-only boundary. Audio operation is not an exact English
voice-track or subtitle synchronization comparison.

## Historical locale tests

[M4-010](M4-010-locale-paths.md) tested EN/US, FR/FR and RU/RU title routes and
Unicode paths on its exact older artifact. Those results remain valid within
that title-only scope; they are not current French/Russian gameplay acceptance.
Any future supported language must obtain its own boot and gameplay evidence.

M7-010 is a claim audit, not a runtime change. Source version checkpoints do not
relabel the tested0.18.0.0 package. Full M7 closure and publication remain separate.
