# MCLA-R Project Rules

These repository-specific rules supplement the global Codex working rules.

## Milestone persistence

- Continue working autonomously on the active milestone until one of these terminal conditions is reached: the milestone is fully reviewed, closed, committed, and pushed; further progress genuinely requires user input or authorization; or the observable remaining weekly usage allowance reaches 20%.
- Do not stop merely because an individual task is complete while safe, in-scope milestone work remains.
- After every closed task, read the live ChatGPT Settings -> Usage value (or the interactive Codex `/status` fallback) before starting the next task. Continue only while the observable weekly allowance remains above 20%.
- When the environment does not expose weekly usage, do not guess or fabricate it. Continue until milestone closure or a genuine user blocker, and let the user stop the run if their external usage display approaches the threshold.
- This persistence rule does not broaden authorization: preserve the repository's safety, review, testing, commit-per-task, and milestone-push gates.

## Scoped multi-agent workflow

- Use subagents when an active task has at least two genuinely independent, bounded tracks, or when a narrow independent review materially reduces reverse-engineering, test-design, privacy, or upstream-reporting risk. Do not delegate trivial work or serial steps whose coordination cost exceeds the benefit.
- The primary agent remains the single owner of the active task and milestone. It owns the current plan, scope decisions, integration, README-AI/task status, final review, required tests, live usage-limit check, task commit, milestone commit, and push.
- Give every subagent an explicit named scope, owned and prohibited paths/actions, expected deliverable and evidence, mutation authority, and stop condition. Default subagents to read-only research or review unless a disjoint implementation scope has been deliberately assigned.
- Because all agents share one worktree, allow only one writer for overlapping files or generated state at a time. Do not run concurrent edits, formatting, code generation, dependency installation, clean builds, or Git index/commit operations that can observe or overwrite another agent's work.
- Serialize GUI/native runtime probes, private-evidence capture, toolchain installation, and any operation that launches `mcla.exe` or mutates `private/`, `generated/`, or `out/`. The primary agent coordinates these operations and records the accepted evidence.
- Subagents must not commit, push, open upstream issues, or change milestone/task status unless the primary agent explicitly delegates that exact action. Upstream issue research remains read-only until duplicate searches are complete and the primary agent has reviewed the proposed report.
- Before accepting delegated work, the primary agent reviews the returned findings and any diff, reconciles them with the repository rules and current milestone criteria, and runs the relevant project gates. Delegation does not replace the required periodic milestone review and re-evaluation.
- Every subagent handoff reports its result status, files inspected or changed, commands and tests run, unresolved risks, and any remaining user action or authorization. Subagent completion does not close a task or trigger the usage-limit check; only the primary agent can do that after acceptance and commit.
