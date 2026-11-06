# MCLA-R Project Rules

These repository-specific rules supplement the global Codex working rules.

## Durable context and handoff

- At the start of a new task or after context compaction, read `docs/current-state.md`, then the relevant `README-AI` sections and linked evidence. Check Git status and actual artifact identities before relying on recorded versions.
- `README-AI` owns the milestone ledger and stable workflow; `docs/current-state.md` is a concise, replace-in-place handoff, not another chronological log. Keep detailed history in linked evidence reports and the existing checkpoint log.
- Before handing off, refresh the active task, tested versus pending artifacts, save lineage, blockers, next action, and relevant test results. Never infer a physical PASS from implementation, old evidence, or chat memory.
- Prefer one chat per coherent investigation or outcome. A new chat is optional at a clean boundary; it is not a reason to abandon an active experiment or repeat completed owner testing.

## Milestone persistence

### Temporary owner usage override

- Owner instruction received 2026-10-11, subsequently clarified to have no time
  limit: the 30% remaining weekly-allowance stop is suspended. Work may
  consume the available allowance down to zero; do not stop solely at 30%.
- This override remains active until the owner explicitly cancels it. There is
  no automatic expiry; the ordinary 30% rule resumes only after cancellation.
- Owner explicitly reaffirmed this after the 28%-remaining handoff: "досі можна
  вижигать в ноль". Repeated ordinary-policy text is not a cancellation.
- Continue observing/reporting actual usage. This changes only the quota stop,
  not safety, review/testing/commit/push gates, user-only blockers or permission
  boundaries. It does not authorize buying credits or redeeming usage resets.
- Owner clarification on 2026-10-17: existing paid credits may fund continued
  work after the weekly allowance reaches zero. Stop at approximately 5,000
  remaining credits (use 5,000 as the conservative floor); check the live credit
  balance after closed tasks and before substantial new runs. No credit purchases,
  automatic top-ups, or usage-reset redemption are authorized. This remains active
  until the owner changes it; weekly zero alone is not a stop while credits remain
  above that floor and execution is available.

### Temporary owner usage override

- Latest owner instruction, 2026-11-01: the weekly-allowance stop rules are suspended while the
  work mostly waits on owner-side physical checks and measurement runs. The only usage stop is
  when about 5% of the 5-hour limit remains. Check the live value when the environment shows it;
  do not guess it. No purchases or reset redemption. Earlier percentage/credit floors (October 11,
  17, 18, 21, 23) are historical and not additional active stops.
- Before stopping at that limit or at a genuine owner-only blocker, refresh the applicable current
  documentation (README-AI ledger/checkpoint, current-state handoff, evidence, tested versus pending
  identities, blockers, exact next actions). Preserve dated history and raw receipts.

### Ordinary policy

- Continue working autonomously on the active milestone until one of these terminal conditions is reached: the milestone is fully reviewed, closed, committed, and pushed; further progress genuinely requires user input or authorization; or the observable remaining weekly usage allowance reaches 30%.
- Do not stop merely because an individual task is complete while safe, in-scope milestone work remains.
- After every closed task, read the live ChatGPT Settings -> Usage value (or the interactive Codex `/status` fallback) before starting the next task. Continue only while the observable weekly allowance remains above 30%.
- When the environment does not expose weekly usage, do not guess or fabricate it. Continue until milestone closure or a genuine user blocker, and let the user stop the run if their external usage display approaches the threshold.
- This persistence rule does not broaden authorization: preserve the repository's safety, review, testing, commit-per-task, and milestone-push gates.

## DGX Spark routine-work routing

- Owner reaffirmed on 2026-10-17: use Spark regularly for suitable routine work,
  not merely as an installed integration. At each coherent task's planning step,
  identify bounded extraction, log triage, test-inventory or documentation/diff
  checks that can be offloaded. Route suitable work through `scripts/spark_worker.py`
  and `docs/dgx-spark-worker.md`; if none is suitable, record a short reason in the
  task evidence. Do not manufacture busywork, duplicate completed analysis, or
  send a task whose validation costs more than doing it deterministically.
- Use the owner's DGX Spark endpoint and short model aliases, never PC-local
  inference. Send explicit in-scope source packets, excluding API keys and unrelated
  credentials. Keep responses advisory: primary checks source facts and tests;
  no model-generated shell execution, automatic edits, or engineering acceptance.
- Record each job's receipt and accepted/rejected contribution in task evidence.
  Report actual job counts and results; do not invent token savings or offload
  percentages. Codex subagents are not Spark and must not be counted as such.
- Unavailable Spark or an unsuitable answer is not permission to weaken gates
  or blindly retry. Preserve the failure, use a justified fallback, and continue
  safe independent milestone work. Existing credit floor remains 5,000.

## Owner-authorized storage maintenance

- Owner requested recurring cleanup every two days on2026-10-18. Heartbeat
  automation `temp` owns this schedule; do not create duplicate schedules.
- Scope: stale `%LocalAppData%\Temp` and genuinely obsolete temporary builds or
  artifacts under this repository's `private/`. Age/completion alone does not
  make project evidence disposable. General Temp cleanup uses a7-day quiet age.
- Preserve active builds and dependency closures, MAIN and all save lineage,
  game sources/assets, secrets, clipboard attachments, open-bug raw evidence,
  and representative original/fixed captures. Check current-state and the
  storage-cleanup report before selecting exact files. No active-test interference.
- Record exact paths, hashes, sizes, reasons and recovery limits before deleting;
  keep derived reports and explicitly document lost raw replay. A hash is not a
  substitute for raw evidence. Validate all ancestors; never follow reparse
  points or recursively remove a broad root. Skip uncertain/in-use targets.
- Maintenance alone does not authorize renewed M8 runtime tests or a milestone
  acceptance claim. See docs/evidence/M8-storage-cleanup-20261013.md.

## Scoped multi-agent workflow

- Use subagents when an active task has at least two genuinely independent, bounded tracks, or when a narrow independent review materially reduces reverse-engineering, test-design, privacy, or upstream-reporting risk. Do not delegate trivial work or serial steps whose coordination cost exceeds the benefit.
- The primary agent remains the single owner of the active task and milestone. It owns the current plan, scope decisions, integration, README-AI/task status, final review, required tests, live usage-limit check, task commit, milestone commit, and push.
- Give every subagent an explicit named scope, owned and prohibited paths/actions, expected deliverable and evidence, mutation authority, and stop condition. Default subagents to read-only research or review unless a disjoint implementation scope has been deliberately assigned.
- Because all agents share one worktree, allow only one writer for overlapping files or generated state at a time. Do not run concurrent edits, formatting, code generation, dependency installation, clean builds, or Git index/commit operations that can observe or overwrite another agent's work.
- Serialize GUI/native runtime probes, private-evidence capture, toolchain installation, and any operation that launches `mcla.exe` or mutates `private/`, `generated/`, or `out/`. The primary agent coordinates these operations and records the accepted evidence.
- Subagents must not commit, push, open upstream issues, or change milestone/task status unless the primary agent explicitly delegates that exact action. Upstream issue research remains read-only until duplicate searches are complete and the primary agent has reviewed the proposed report.
- Before accepting delegated work, the primary agent reviews the returned findings and any diff, reconciles them with the repository rules and current milestone criteria, and runs the relevant project gates. Delegation does not replace the required periodic milestone review and re-evaluation.
- Every subagent handoff reports its result status, files inspected or changed, commands and tests run, unresolved risks, and any remaining user action or authorization. Subagent completion does not close a task or trigger the usage-limit check; only the primary agent can do that after acceptance and commit.


## Standing owner authorization for non-destructive tests

On 2026-09-08 the owner explicitly authorized ALL such non-destructive tests in
this MCLA optimization task: "якщо тест не деструктивний – МОЖНА, РОЗРІШАЮ".
This standing authorization includes preparing and transferring experimental
binaries, helpers and diagnostic proofs between this workspace and the owner's
paired Steam Deck, running isolated controlled comparisons, and collecting their
results. It includes the prepared U package and future non-destructive experiments
of the same kind; do not ask for package-by-package confirmation again.
Preserve the normal bundle and save lineage, review/test gates, bounded owned
process cleanup and the owner's 30% remaining weekly usage stop threshold. This
permission does not authorize destructive actions or unrelated external sharing.

## No builds while the owner plays (2026-11-06)

- The owner's PC is both the build machine and the test machine. KI-067/KI-072 showed that the project's own parallel builds and test runs (ninja/clang at 8-19 jobs, 51 test executables) halve the game's CPU under the laptop's shared 35 W power cap and were the cause of every "host slowdown" episode.
- Before starting any build, test run, SDK ctest, archive extraction or other sustained CPU work on this PC, run `private/tools/wait-for-idle-pc.ps1` (or check for `mcla.exe` / `MCLA-Launcher.exe` yourself) and wait while a game session is running. Subagent prompts must carry this rule. The primary gate scripts already enforce it.
- If the owner is testing and a build is urgent, say so and let the owner decide; never start it silently.
