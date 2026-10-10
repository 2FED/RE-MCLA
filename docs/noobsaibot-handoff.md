# NoobSaibot / DGX Spark handoff

Updated 2026-10-02. This is the owner-requested machine migration and local-model
experiment handoff. Read `docs/current-state.md`, `README-AI`, `AGENTS.md`, then
the relevant evidence before doing project work. Migration is verified; exact
receipts are in `C:\BDU\MCLA-migration-20261002-r2` on both machines.

## Verified destination and limitations

- Final Noob roots: `C:\BDU\MCLA-Recomp` and `C:\BDU\local-llm-pilot`.
- Initial full snapshot:100,589 files,131,864,956,508 bytes,21 links; all SHA256,
  sizes, mtimes, readonly bits, link targets/types and exact-case inventory passed.
- Main/SDK HEAD, branches and dirty status match source;26 submodule identities
  match; both Git fsck checks passed. `.git` was frozen to avoid live Codex
  housekeeping-ref changes. External wheel worktree registration is retained.
- ast-grep scan and3 rule tests passed; pilot guard7/7 and parser32/32 passed
  using the copied data and destination Python. These are NOT game runtime tests.
- A four-document handoff overlay after the initial snapshot is recorded and
  SHA-verified separately, mirrored to ITX11. Original binary/save evidence is
  unchanged. No commit/push/version bump or new game run occurred.
- Noob verified tools: Git2.55.0.windows.3, Codex bundled PowerShell7.6.5,
  standalone Python3.12.14 and ast-grep0.45.0 in the migration `tools/` folder.
  Bare `python` on PATH was a Windows Store alias; use the explicit tools path.
- The Oct2 local toolchain gate passed: VS Build Tools 2022 `17.14.37`, MSVC,
  CMake `3.31.6`, Ninja `1.12.1`, Windows SDK/CDB `10.0.26100.3916`, LLVM/LLD
  `20.1.8`, Temurin `21.0.12+8`, RenderDoc `1.45.0`, and the pinned ReXGlue SDK
  `0.19.0.0` are installed and verified. Fresh Debug and RelWithDebInfo
  application builds passed. Continue to use fresh build roots for experiments.
- Noob hardware: Ryzen AI7 350,32GB RAM, RTX5060 Laptop8GB (driver617.14),
  plus AMD860M. Local-on-PC model inference is deliberately excluded.
- Regular hardlinks were expanded into independent files. Source NTFS ACLs,
  ADS and creation times are not a forensic clone; destination files are owned
  by orange. Logical content and the listed metadata were verified.

Evidence files: `destination-verification.json`, `destination-checks.json`,
`git-and-tests-comparison.json`, `source-stability.json`, `setup-status.json`,
`handoff-overlay.json`. `START-HERE.txt` is the prompt for the new Codex chat.

## Current owner direction

- Transfer the entire current MCLA workspace and its context from ITX11 to the
  office Windows computer NoobSaibot, accessible in the owner's Tailscale network.
- Preserve the original machine as a fallback until the copied state is verified.
- Continue local-model experiments from NoobSaibot, which can reach the office
  DGX Spark locally. This turn is migration/setup, not permission to promote a
  pending game-timing candidate or close M8.
- The owner explicitly authorizes sending ALL MCLA data, including `private/`,
  to the owner's office Spark. Do not ask again for this same data-transfer scope.
- QUALITY AND LOCAL/COST-FREE EXECUTION TAKE PRIORITY OVER LATENCY. The owner
  accepts a task taking an hour instead of ten minutes. Do not reject a useful
  local model just because it is slower. The Oct1 pilot's output/time caps are
  historical experimental bounds, not permanent acceptance limits.
- Existing review, exact artifact/save lineage, non-destructive-test and weekly
  usage gates still apply. Sept28's recorded25% is historical, not a live quota
  reading for Oct2. Check current account allowance before resuming milestones;
  never invent its value. Long local inference still needs observable progress,
  bounded owned processes and an explicit job/output budget.

## Source state that must survive migration

Source roots:
- `C:\BDU\MCLA-Recomp` (full workspace, including ignored/untracked files).
- `C:\BDU\local-llm-pilot` (tests/results/scripts/context ONLY).

Owner's later Oct2 correction: no local-on-PC models are needed; inference will
run on DGX Spark. EXCLUDE `models/`, `engines/`, `downloads/` and local model
launchers from the migration. Preserve results and benchmark/client source.
The owner removed source weights manually after automatic review blocked local
deletion. Canceled model partials on Noob were removed and absence verified;
they are not in either final root. Two obsolete source-side model archives were
still present at the last check; an exact manual removal command was supplied.
Both r1 transfer and public model prefetch were stopped. Migration used the owned
`C:\BDU\MCLA-migration-20261002-r2` management/staging plan.

Recorded before migration:
- Main HEAD `201c3a63895a8431cc68c3217c3eb1da5631e93b`, branch `main`,18 commits
  ahead of the recorded upstream. Actual root VERSION is `0.18.0.0`.
- SDK HEAD `c2e67161f1207ddcee609507c399efe7a1981b72`, branch
  `mcla/mcla-r-hotfixes`, dirty. Main is also deliberately dirty.
- Preserve main and recursive submodule `.git` metadata, objects/refs/index,
  tracked edits, all untracked evidence, `private/`, `generated/`, both project
  and SDK `out/`, and the untracked Xbox360 source-image directory.
- Main `.git/worktrees/MCLA-Recomp-wheel-tester` references a separate sibling
  checkout. That checkout remains OUT OF SCOPE. Preserve registration/history as
  source metadata, but do not copy, prune, repair or activate that sibling.
- Do not clone/reset/checkout/update submodules over the copied dirty state.
  GitHub alone does not contain the full current project.

The selected stock control, hashes and save lineage remain exactly those in
`docs/current-state.md`. B256/V96 remains rejected. The interval1 candidate has
offline/static evidence only; the native capture/collector and animation-cycle
extraction work remains pending. Migration is NOT a runtime or performance PASS.

## Local inference pilot already performed

Read `C:\BDU\local-llm-pilot\RESULTS.txt`, its `README-AI`,
`results/final-receipts.json`, per-run `summary.json`, and
`tests/independent-review.md`. If roots differ, use the relocated sibling folder.

ITX11 was Ryzen5900X, RTX3090 24GiB, RAM32GiB. Both engines used the same
gpt-oss20b HF revision, with locally converted matched GGUF preserving MXFP4
experts and BF16 non-experts.109 generation requests,0 HTTP/transport errors,
8 output-budget truncations. Low/medium/high were compared.32 hidden parser
tests and native C++ witnesses found real correctness errors. No universal
quality winner; llama low parser32/32 vs FreeToken31/32, both low C++7/11;
FreeToken medium C++11/11 but its parser was truncated. Medium/high did not
uniformly improve completed task results. Treat these as limited synthetic
evidence, not a measured MCLA productivity or quota-saving claim.

llama.cpp `--load-mode none` reduced observed peak aggregate working set from
14.08GiB to2.02GiB with identical low final texts. FreeToken was~8.1–8.5GiB.
~4k input-token lookup tasks completed in~2.4s on low. Slow/local tradeoffs must
now be re-evaluated under the owner's new latency preference.

Engines were stopped and no Codex provider change/MCP routing was installed.
Both engines passed a small streaming Responses API function-call roundtrip;
that is not an end-to-end Codex agent validation. Default pilot scripts have
shorter historical caps; revise future Spark experiments deliberately rather
than assuming a length-truncated model cannot solve the task.

## Office DGX Spark information supplied by owner

Base URL supplied by owner: `https://192.168.113.90:4000/v1`.
Routing: Zed/VSCode -> HTTPS LiteLLM4000 -> per-model vLLM servers8000–8007.
`model` accepts ONLY the short aliases below, not HuggingFace repository names.
Authentication, runtime versions, exact quantization and current residency are
not yet established. Do not assume all models are loaded simultaneously.

Noob is intentionally on public WiFi192.168.115.120 for Parsec to ITX11; the
internal network blocks that Parsec route. Direct TCP/HTTPS to Spark timed out
there. The owner will return to the internal office network after this verified
migration. This expected isolation is not evidence of a broken Spark service;
run connectivity/alias probes after the network switch. TLS was not disabled.

| Alias | Model / purpose | Served context | Images | Sets |
| --- | --- | ---: | --- | --- |
| gemma4-26b | Gemma4 26B-A4B general |131072|yes|default, assistants|
| qwen3.8-27b | Qwen3.8-27B; owner reports~12tokens/s |262144|yes|default|
| qwen3.6-35b | Qwen3.6-35B-A3B, fast general/coding candidate |262144|yes|coding, assistants|
| gpt-oss | gpt-oss20b reasoning |131072|no|assistants|
| dots | dots.mocr document OCR, not general chat |32768|yes|default|

Owner-provided request controls:
- Qwen3.8 thinks by default; disable with
  `"chat_template_kwargs": {"enable_thinking": false}`.
- Gemma default non-thinking; enable by prepending `<|think|>` to its system
  prompt, retaining the other system instructions.
- gpt-oss uses `reasoning_effort: low|medium|high`.
- Reasoning is returned separately as `reasoning_content`.

Suggested larger candidates, NOT installed/tested on Spark yet:
- Qwen3-Coder-Next80B-A3B, official GGUF Q4_K_M~48.4GB or Q5_K_M~56.7GB.
  Coding-specific, native non-thinking. First suggested additional model.
- gpt-oss120b MXFP4~65.25GB safetensors. NVIDIA documents one-Spark deployment.
- NVIDIA-Nemotron-3-Super120B-A12B NVFP4~80.32GB weights. NVIDIA lists one Spark
  as supported minimum. Later candidate, not an automatic quality upgrade.

Weight sizes exclude runtime/KV/OS memory. Run large candidates sequentially,
start with controlled context/concurrency, and measure useful completed output.
Do not generalize 'more parameters' or 'faster' to proven coding correctness.
Reference model pages:
https://huggingface.co/Qwen/Qwen3-Coder-Next
https://huggingface.co/Qwen/Qwen3-Coder-Next-GGUF
https://build.nvidia.com/spark/sglang/overview
https://huggingface.co/nvidia/NVIDIA-Nemotron-3-Super-120B-A12B-NVFP4

## Relocation and verification constraints

Prefer the same two `C:\BDU\...` roots when destination disk/layout permits.
21 project reparse entries include relative library links and absolute private
path fixtures, one intentionally dangling. Preserve exact targets; do not follow
them while copying or rewrite historical evidence. The pilot's old Python
junction and venv are now excluded by owner request. Use destination standalone
or bundled Python for HTTP client and grader tasks.

Existing CMake caches bind source/build paths, exact compiler versions and VS
8.3 paths. Copy them as artifacts; verify destination toolchain and configure new
build directories before new compilation. A source-machine bootstrap PASS does
not transfer to NoobSaibot. Do not silently rebuild tested game binaries.

Never copy the source user's global Codex auth/session tokens or private SSH key
as 'project context'. The handoff documents and project evidence carry the work;
the destination Codex already has its own account/session. Windows environments
and global tools require destination verification independently of file hashes.

## First actions for Codex on NoobSaibot

Current direction, 2026-10-03: migration/setup above is historical; build readiness
and Spark smoke are already complete. M7 now targets representative local Windows
RTX 5060 compatibility, with Deck deferred and no required full playthroughs.
Use the [bounded Spark worker](dgx-spark-worker.md) for explicit routine tasks,
then finish the native M8-001 harness, close revised M7 and review/rethink M8.
The numbered migration sequence below is not a reason to repeat completed setup.

1. Read this handoff and the normal project entrypoint documents. Inspect the
   completed migration receipt, actual Git status/HEAD/submodules and artifacts.
2. Confirm copied regular-file hashes and reparse targets were verified. Keep
   ITX11 untouched as fallback. Do not substitute a clean GitHub clone.
3. Verify local tooling and identify the authorized office Spark API endpoint.
4. Reuse the pilot task harness as a baseline, adding small real MCLA analysis
   tasks with verifiable answers. Keep expected answers out of model prompts.
5. Declare generous local-job budgets consistent with the owner's hour-long
   tolerance. Compare correctness, completion, required corrections and cloud
   work avoided; latency is secondary. Preserve raw failures and caps.
6. Integrate local workers only after their tool/schema behavior is tested.
   Primary agent owns actual patches, tests, saves, milestone state and commits.
7. Resume the pending M8 experiment from its exact offline checkpoint only when
   current allowance and normal gates permit; do not repeat rejected experiments.
> **2026-10-02 scoped completion addendum:** after the live usage reading reached
> **92% used / 8% remaining**, the owner explicitly authorized this one Windows
> diagnostic build plus bounded DGX Spark smoke. The resulting private package is
> `private/bundles/M8-001-Windows-B256-Uncapped-Scale2-20261002`. Its historical
> B256/uncapped/2x candidate SHA-256 is
> `1F186BB07399717DDACF251D459A76CCD6CE1753F61D3B9E13BDE11BFC7651E9`; the fresh
> stock rollback SHA-256 is
> `2D7A949F61BD9E30A17D47ACA7888894CDF3BB1B5BB54B2ED9483D70A445BAF4`.
> Build/package gates passed, but the game was not launched; this diagnostic does
> not reverse B256's rejection. The ignored `private/config/dgx-spark.env` is now
> present and authenticated without exposing its value. Normal-TLS `/models` and
> chat smoke passed for `gemma4-26b`, `qwen3.8-27b`, and `dots`; an extra bounded
> 64-token Qwen control returned `SPARK_OK` after its first 16-token response spent
> the budget in `reasoning_content`. `qwen3.6-35b` and `gpt-oss` were not listed or
> called. Do not install local PC models. The general 30%-remaining stop gate still
> applies beyond this owner-authorized exception.
