# M6-009 retired-service unlock policy

M6-009 is a decision gate, not an unlock patch. The accepted private result is
`private/evidence/M6-009/20260819-171011-98b464f4/result.json`, SHA-256
`8CD1FD76F8C4E3449FFCB699BDBBCA05276088B8BA1E248BC889AC6EDAF57E25`.
The exact public policy file has canonical-LF SHA-256
`44FEDC36A8CE9575F9E2A53AAC84EDFE3765D24759E7D9A34CAD7BA8067B48CE`.

## Historical basis

[Rockstar's MCLA page](https://www.rockstargames.com/games/midnightclubLA)
states that Rate My Ride and Driving Test unlocks on Xbox 360 and PlayStation 3
are retired and no longer available. A contemporaneous
[GamesRadar MCLA guide](https://www.gamesradar.com/midnight-club-los-angeles-guide/)
records the 2008 Audi R8 as a $118,000 garage purchase available after all
twelve Social Club tasks. Together these establish a retired service
entitlement boundary; they do not establish that the current supported save
already contains that entitlement.

## Approved boundary

Canonical compatibility:

- preserves the retail lock and any authentic entitlement already present;
- may fix a defect only when it restores an offline or on-disc platform
  contract;
- does not fabricate service completion, Live state, leaderboard data, or a
  new vehicle entitlement;
- does not edit source game data or mutate a save to grant content;
- remains valid for canonical progression and achievement evidence.

Optional convenience access is classified as a cheat, not compatibility. A
future implementation is permitted only if it is explicit, default false,
InitOnly, separately named and documented, non-persistent, and excluded from
canonical progression/achievement claims. No such cheat is implemented by
M6-009.

## Verification

`scripts/run-retired-service-unlock-policy.ps1` rebinds immutable M6-008 result
`20260819-164950-4b322fe1`, validates the policy, audits owned runtime source for
an unlock implementation, and writes the one-file private decision result.
The independent fixture gate passes one positive, forty fail-closed negatives,
and twenty-four source-contract checks. The result explicitly records no Audi
R8 unlock, service emulation, fake Live state, save/source-game mutation, or
implemented cheat.
