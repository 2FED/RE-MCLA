# M8-001 CppStudio and Xbox 360 decomp reference trial

Date: 2026-09-12
Status: reference-only adoption; no tool rollout or generated-project migration

## CppStudio source snapshot

CppStudio was staged only as a private read-only reference from
`https://github.com/tarkansarim/CppStudio` at commit
`69ca731da7145b4c57e5282d69cfe30bda3bb2ba`.

The content was materialized from Git blobs rather than a mutable Windows
checkout. Receipt:
`private/evidence/M8-001/cppstudio-trial-20260912/reference-receipt.json`,
SHA-256 `F4B7ECF2B87B1E4646B0A8831B737F3291E86C58CFC024FF3CB1DB225706DE31`.
Its package manifest covers 258 regular, case-fold-unique files / 1,319,769
bytes and hashes to
`4ED1D8C1D3EB63D5B1BE3B989D3CAD1A978EABE6389541D6016C030EE865EE90`.

Only these guidance files were selected:

- router `SKILL.md`;
- `modules/studio-core.md`;
- `modules/technical-overlays.md`;
- `modules/process/investigative.md`;
- GPU profiling workstation guide and tool inventory;
- Vulkan compute synchronization guide.

The trial did not run CppStudio scripts, install or autoload its skill package,
write an AGENTS relay, start rollout/sync/watch behavior, scaffold a replacement
project, or adopt its planning/state files. Its full rollout assumes ownership
of user-level agent configuration and a generated-project backbone that does
not match this mature ReXGlue repository. Stock rollout is therefore a NO-GO.

The useful guidance retained here is narrower:

- measure a concrete cost owner before changing synchronization;
- keep CPU, GPU-command, queue/fence, presentation and scanout domains separate;
- change one variable per performance experiment;
- preserve workload, binary, configuration and rollback identities;
- use short GPU producer profiles only after existing counters are exhausted.

These principles reinforce the repository's existing evidence discipline and do
not replace `README-AI`, `docs/current-state.md`, task evidence, or repository
review gates.

## Requested xbox360-decomp skill

The exact requested command was executed after the CppStudio reference trial:

`npx skills use "https://github.com/dohmboy64bit/xbox360-decomp-skill" --skill "xbox360-decomp"`

Its complete stdout/stderr capture is
`C:/Users/vovan/AppData/Local/Temp/xbox360-decomp-skill-86a05f8bdc7346b1b22131331ab5a4b3.txt`,
SHA-256 `032A77EEDA65D8A3FDF54490BB33F3507B0A94B649A962D8D852D13EA10B6130`.
The command downloaded supporting files under
`C:/Users/vovan/AppData/Local/Temp/skills-use-rEC36v/xbox360-decomp`; relative
resource paths were resolved from that directory.

This project is Track A: a ReXGlue static recompilation. The applicable guidance
is:

- treat guest PPC, generated C++, ReXGlue runtime and host GPU/OS as separate
  diagnostic layers;
- prefer config, runtime hooks and bounded shims over editing `generated/` as the
  primary fix;
- verify guest addresses, return PCs, endianness and original PPC behavior;
- preserve original calls, registers, ordering, fences and watchdog behavior;
- use SDK source locations as the authority for hook and runtime semantics;
- after a repeated failure, gather new evidence before another patch.

The optional bundled SDK patches were not applied. None matched an evidence-led
symptom strongly enough to justify changing the pinned ReXGlue SDK. No
XenonRecomp output was introduced, and no Track D pipeline was mixed into the
Track A build.

The skill proposes a separate `XBOX360_PROJECT_STATE.md`. This repository already
requires `README-AI` plus replace-in-place `docs/current-state.md` as its durable
architecture and handoff sources. Creating a parallel state ledger would violate
the repository's single-handoff convention, so the applicable skill findings
are recorded in those existing documents and this evidence report instead.

## Decision

Migration refresh 2026-10-03: `START-HERE.txt` now links the skills handoff at
`C:/BDU/MCLA-migration-20261002-r2/skills-20261003/README-AI`. The xbox360-decomp
1.4.4 package has a durable reference root there under
`references/xbox360-decomp`; the old ITX11 Temp paths above are historical.
Primary reread both SKILL.md files and relevant Track A/guardrail/control-harness
resources; rehash259 CppStudio and37 Xbox reference files PASS. For the Windows
harness, apply discovery-before-control, exact process/window ownership, fresh
visual/sequence readback and separate transport/mock/physical claims. Existing
README-AI/current-state remain authority. No install, rollout, ledger creation,
bundled SDK patch application or Track D migration follows.

CppStudio and xbox360-decomp are reference inputs, not installed runtime
dependencies. Their useful profiling and layer-separation rules are adopted for
M8-001. Their rollout, generated-project backbone, parallel state file, optional
SDK patches and unrelated CUDA/greenfield lanes remain excluded.
