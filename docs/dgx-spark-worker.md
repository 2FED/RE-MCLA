# Bounded DGX Spark routine worker

Owner-authorized workflow, 2026-10-03. Inference runs on the office Spark, not
on this PC. This is an advisory source-packet worker, not a replacement Codex
provider, autonomous shell agent, or automatic engineering acceptance.

## Connection and scope

- Fixed endpoint: `https://192.168.113.90:4000/v1`.
- Secret: ignored `private/config/dgx-spark.env`, one `LITELLM_API_KEY=...` entry.
  Do not put the key in packets, CLI arguments, logs or Git.
- Engineering aliases: `gemma4-26b`, `qwen3.8-27b`; availability is checked before
  each chat. OCR `dots` and unavailable aliases are not general workers.
- Normal system TLS validation; no redirect, proxy or automatic retry.
- Primary supplies explicit source text and a bounded task. No filesystem/tool
  access or model-returned commands are executed. Source instructions are untrusted.
- Owner allows project/private context on this endpoint; do not send unrelated
  account credentials, global Codex auth or out-of-scope worktrees.

## Standing routine routing (owner reaffirmed October 17)

At the start of a coherent task, select useful bounded routine work for Spark:
exact source extraction, test-case inventories, log triage, or documentation/diff
checks. This is an active workflow requirement, not a fixed percentage quota.
If nothing is suitable, record why; do not manufacture a request or duplicate
already completed work. Deterministic scripts remain preferable for hashing,
exact arithmetic and mechanical validation.

Each task evidence records job receipt, requested scope, accepted/rejected
findings and validation. Completed inference is not accepted engineering work.
Unavailable inference and rejected answers retain their evidence; do not blindly
retry or block independent safe progress. Report actual usage rather than guessed
cost savings; Codex subagents are separate from Spark. The owner's existing-credit
floor is 5,000, with no authority to purchase credits or redeem resets.

## Packet and invocation

Prepare a JSON file with exactly this topology; no implicit directory upload:

```json
{
  "task": "Find concrete inconsistencies; cite the named sources and state limits.",
  "sources": [{"name": "source label", "text": "explicit source contents"}]
}
```

Use a verified standalone Python (migration CPython is available) and a fresh
job directory. The script needs only the Python standard library:

```powershell
& C:/BDU/MCLA-migration-20261002-r2/tools/cpython-3.12.14-windows-x86_64-none/python.exe -B scripts/spark_worker.py --input private/evidence/task-packet.json --model gemma4-26b --output private/evidence/spark-task-unique --max-tokens 2048 --timeout 600
```

`--max-tokens` is explicit (default 2048; maximum 32768); `--timeout` accepts
1–3600 seconds. This is a shared request/deadline-check budget with socket
inactivity timeouts, **not an OS-enforced hard wall-time cap** against a slow-
trickle server. Keep the owning terminal process observable; inspect progress
through the saved job artifacts and stop only that owned process if needed.
Do not automatically retry a failure or infer that short-budget truncation
means the model cannot do the task. Larger deliberate budgets are allowed.

## Evidence and review

The new, ignored job directory stores the explicit packet/request, model-list
response, raw chat JSON, final text when complete, and a receipt with timestamps,
model, budgets and artifact hashes. Existing jobs cannot be overwritten. Reparse/
symlink ancestors and output escapes are rejected. Serialize private-state work;
the path checks are not an adversarial concurrent-filesystem sandbox.

Literal and JSON-escaped echoes of the bearer are redacted before persistence.
This does not promise detection of every transformed secret. Do not include
credentials in source packets, even though the owner authorized project data.

Exit 0 / `PENDING_PRIMARY_REVIEW` means complete final text was received, not
that facts, JSON schema, code or physical behavior are correct. Exit 2 rejects
errors, unavailable aliases, tool calls, truncation, empty/reasoning-only final
answers and malformed envelopes. HTTP error bodies are not printed or persisted.

For each useful job, primary checks references/numbers against source, validates
requested JSON/code with deterministic tests, records accepted/rejected findings
and corrections, then owns any patch. Start with lookup, log aggregation,
test suggestions, doc consistency or narrow diff review. Never delegate final
timing/ABI/GPU-sync/save acceptance or milestone closure to model text.

Offline tests:

```powershell
& C:/BDU/MCLA-migration-20261002-r2/tools/cpython-3.12.14-windows-x86_64-none/python.exe -B scripts/test_spark_worker.py
```

Initial client review: 34 offline tests PASS; independent privacy/TLS/no-tools/
path/status review passed within the trusted-owner serialized boundary. Actual
useful-job results: [Oct3 integration report](evidence/M7-scope-and-spark-integration-20261003.md).

Initial routine routing is provisional: Qwen with owner-specified non-thinking
control passed one structured source-extraction task. Gemma completed one larger
doc review but produced three false conflicts, all rejected. Different tasks/
contexts/budgets mean this is not a fair model comparison or universal winner.
Use reviewed extraction/test/log jobs first, and validate each requested schema;
do not rely on either model as an independent engineering approval authority.

Later Oct3 bridge job: explicit owner authorization for project source transfer;
four exact guard quotes accepted after source/artifact hash checks, constructor
classification and QPC wording rejected/corrected. This is partial extraction
usefulness, not code approval. [Native workflow report](evidence/M8-001-native-windows-harness-20261003.md).

Later same-packet descriptor-axis experiment: both aliases completed, but primary
rejected exact-JSON and source-fact failures (table addresses/counts/known slots).
No extraction adopted. This narrow arithmetic/binding result is not a general
model ranking; retain deterministic primary source checks and favor tightly
verified exact-quote/extraction jobs. [Preserved disposition](evidence/M7-001-representative-inventory-20261003.md).
