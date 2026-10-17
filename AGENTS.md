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
