# Round 5 Report

## Scope

Round 5 focused on turning UESpec from "can generate UI" into "can diagnose a failed UI spec, propose a minimal patch, and retry automatically".

Delivery order stayed `5A -> 5B -> 5C -> 5D -> 5E -> 5F -> 5G -> 5H -> 5I`.

Final status:

- `UESpec` supplied the structured failure surface and headless runtime fixes needed by the loop.
- `UESpec-Mcp` shipped the handler stack, orchestrator loop, eval harness, and real-LLM hardening.
- the latest curated real-eval suite finished at `20/20 fixed`, `100%`, `avgAttempts=1.0`

## 1. Task Implementation Files And Code Volume

### 5A: Structured failure reports + UE-side JSON output

- `UESpec` files: `Source/UESpecRuntime/Public/TestSpec/UESpecTestRunner.h`, `Source/UESpecRuntime/Private/TestSpec/UESpecTestRunner.cpp`, `Source/UESpecEditor/Private/Commandlets/UESpecRunTestsCommandlet.cpp`, `Source/UESpecEditor/Private/Commandlets/UESpecCommandletUtils.cpp`, `Source/UESpecTests/Private/TestSpec/UESpecTestRunnerTests.cpp`
- `UESpec-Mcp` files: `src/uespec_mcp/bridge.py`, `src/uespec_mcp/tools/test.py`, `tests/test_server.py`
- Code volume: `UESpec +696/-48`, `UESpec-Mcp +164/-9`
- What shipped: structured `FailureContext`, `failure-report.json`, bridge parsing, MCP `get_failure_report(...)`

### 5B: LLM adapter layer

- Files: `src/uespec_mcp/llm/base.py`, `src/uespec_mcp/llm/types.py`, `src/uespec_mcp/llm/factory.py`, `src/uespec_mcp/llm/adapters/claude.py`, `src/uespec_mcp/llm/adapters/openai.py`, `src/uespec_mcp/llm/adapters/mock.py`, `src/uespec_mcp/llm/prompts/templates.py`, `tests/test_llm_adapter.py`
- Code volume: `+604/-1`
- What shipped: `Claude + OpenAI + Mock` adapters, env-driven factory, prompt-template loader, mock-based CI coverage

### 5C: `COMPILE_ERROR` handler

- Files: `src/uespec_mcp/orchestrator/handlers/compile_error.py`, `src/uespec_mcp/llm/prompts/templates/compile_error.j2`, `tests/test_compile_error_handler.py`
- Code volume: `+355`
- What shipped: compiler-error-context ingestion, support-surface slicing, RFC 6902 patch generation and validation

### 5D: `WIDGET_MISSING` handler

- Files: `src/uespec_mcp/orchestrator/handlers/widget_missing.py`, `src/uespec_mcp/orchestrator/handlers/_json_patch.py`, `src/uespec_mcp/llm/prompts/templates/widget_missing.j2`, `tests/test_widget_missing_handler.py`
- Code volume: `+552/-40`
- What shipped: widget-tree compaction, typo-vs-add heuristic, JSON Patch validation reuse

### 5E: `ASSERTION_FAILED` handler

- Files: `src/uespec_mcp/orchestrator/handlers/assertion_failed.py`, `src/uespec_mcp/llm/prompts/templates/assertion_text.j2`, `src/uespec_mcp/llm/prompts/templates/assertion_state.j2`, `src/uespec_mcp/llm/prompts/templates/assertion_style.j2`, `src/uespec_mcp/llm/prompts/templates/assertion_generic.j2`, `tests/test_assertion_failed_handler.py`
- Code volume: `+561/-2`
- What shipped: text/state/style sub-prompts, widget-context extraction, assertion-specific patch proposals

### 5F: `SLATE_EVENT_NOT_HANDLED` handler

- Files: `src/uespec_mcp/orchestrator/handlers/slate_event_not_handled.py`, `src/uespec_mcp/llm/prompts/templates/slate_event_not_handled.j2`, `tests/test_slate_event_not_handled_handler.py`
- Code volume: `+722`
- What shipped: event-routing diagnosis for wrong `on`, missing method declaration, unsupported host widget, and signal-based actions

### 5G: `TIMEOUT` handler

- Files: `src/uespec_mcp/orchestrator/handlers/timeout.py`, `src/uespec_mcp/llm/prompts/templates/timeout_state.j2`, `src/uespec_mcp/llm/prompts/templates/timeout_visibility.j2`, `src/uespec_mcp/llm/prompts/templates/timeout_generic.j2`, `tests/test_timeout_handler.py`
- Code volume: `+739`
- What shipped: timeout-type branching, state/visibility/generic prompts, wait-context extraction

### 5H: closed-loop orchestrator

- Files: `src/uespec_mcp/orchestrator/core.py`, `src/uespec_mcp/orchestrator/patch_applier.py`, `src/uespec_mcp/orchestrator/safety.py`, `src/uespec_mcp/orchestrator/history.py`, `src/uespec_mcp/tools/orchestrator.py`, `tests/test_orchestrator.py`
- Code volume: `+1179/-6`
- What shipped: `run_with_autofix`, patch history, max-attempts, stall detection, rollback-on-regression, MCP tool exposure

### 5I: eval framework + demo

- Files: `evals/run_eval.py`, `evals/README.md`, `evals/report_template.md`, `examples/autofix_demo.py`, `evals/cases/**`, `tests/test_eval_runner.py`
- Code volume: `+2038`
- What shipped: 20-case eval corpus, expected-fix comparison, category summaries, markdown/json outputs, demo runner

### Post-5I hardening required by real eval

- `UESpec-Mcp` key files: `src/uespec_mcp/llm/adapters/claude.py`, `src/uespec_mcp/orchestrator/core.py`, `src/uespec_mcp/orchestrator/handlers/assertion_failed.py`, `src/uespec_mcp/orchestrator/handlers/widget_missing.py`, `src/uespec_mcp/orchestrator/handlers/timeout.py`, `src/uespec_mcp/orchestrator/handlers/slate_event_not_handled.py`, `evals/run_eval.py`, multiple `evals/cases/**` fixtures
- `UESpec` key files: `Source/UESpecRuntime/Private/TestSpec/UESpecTestRunner.cpp`, `Source/UESpecTests/Private/TestSpec/UESpecTestRunnerTests.cpp`, `Source/UESpecEditor/Private/Commandlets/UESpecRunTestsCommandlet.cpp`
- Code volume after `5I`: `UESpec-Mcp +3019/-1037`, `UESpec +465/-35`
- What shipped: relay base-url support, runtime asset handoff from compile to test, patch-path anchoring, invalid-fixture detection, timeout/slate deterministic fallbacks, headless Slate initialization, unbound-click failure classification

## 2. Real LLM Eval Success Rates

Latest real-eval artifacts:

- `compile-error-rerun3-summary.json`
- `widget-missing-rerun3-summary.json`
- `assertion-failed-rerun4-summary.json`
- `timeout-rerun4-summary.json`
- `slate-event-rerun4-summary.json`

| Category | Total | Fixed | Rate | Avg Attempts | Reported Input Tokens | Reported Output Tokens |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `COMPILE_ERROR` | 4 | 4 | 100% | 1.0 | 0 | 173 |
| `WIDGET_MISSING` | 4 | 4 | 100% | 1.0 | 8982 | 149 |
| `ASSERTION_FAILED` | 4 | 4 | 100% | 1.0 | 0 | 155 |
| `TIMEOUT` | 4 | 4 | 100% | 1.0 | 0 | 46 |
| `SLATE_EVENT_NOT_HANDLED` | 4 | 4 | 100% | 1.0 | 0 | 0 |
| `Total` | 20 | 20 | 100% | 1.0 | 8982 | 523 |

Notes:

- `inputTokens=0` in several categories does not mean "free". The Anthropic-compatible relay often returned usage via `cache_read_input_tokens` instead of normal billed `input_tokens`.
- `WIDGET_MISSING` consumed the largest prompt footprint by a wide margin because the prompt included compressed widget-tree context and insertion guidance.

## 3. Real LLM Token And Cost Summary

- Provider: `claude`
- Base URL: `<relay-base-url-redacted>`
- Model used in the successful reruns: `claude-haiku-4-5-20251001`
- Relay-reported totals: `input_tokens=8982`, `output_tokens=523`, `cache_read_input_tokens=13604`
- `estimatedUsd` stayed `null` in all final summary files

Why USD is still unavailable:

- the eval runner was not given explicit per-1K cost env vars for this relay
- the relay's usage payload mixed normal `input_tokens` with `cache_read_input_tokens`
- the current summary format does not convert relay-specific cache-read accounting into a billable USD estimate

Round 5 therefore produced a reliable success-rate report, but not a final trustworthy dollar-cost report.

## 4. Most Successful Demo Case

Best demo case: `widget_missing/case-04-add-cooldown-bar`

Why this case is the strongest demo:

- the patch is structural, not a one-word typo repair
- the fix adds a new widget node instead of only editing a scalar field
- the generated fix then passes the full `validate -> compile -> run-tests` loop
- it also exercised the UE-side headless `ProgressBar` path that previously required commandlet Slate initialization hardening

Final patch shape:

```json
[
  {
    "op": "add",
    "path": "/root/children/-",
    "value": {
      "id": "slotQ_cooldownBar",
      "type": "ProgressBar"
    }
  }
]
```

Artifact trail:

- Eval summary: `<HOME>\projects\uespec-mcp\evals\artifacts\widget-missing-rerun3-summary.json`
- PatchHistory: `<HOME>\projects\uespec-mcp\Saved\UESpec\Evals\PatchHistory\widget_missing\case-04-add-cooldown-bar\broken-spec\20260425T172351Z`

## 5. Hardest Case

There is no final failed case in the latest curated real-eval suite.

The hardest case during stabilization was `timeout/case-03-add-missing-expected-state`.

Why this case was the weakest boundary:

- the early fixture drift made it look like a handler-quality problem when it was partly an eval-quality problem
- timeout diagnosis can blur state-transition issues and visibility issues if the failure surface is not strict enough
- the stable fix path ended up depending on deterministic fallback logic, not only prompt quality

This means the latest result is green, but `TIMEOUT` remains the category most likely to need additional guardrails in Round 6.

Artifact trail:

- Eval summary: `<HOME>\projects\uespec-mcp\evals\artifacts\timeout-rerun4-summary.json`
- PatchHistory: `<HOME>\projects\uespec-mcp\Saved\UESpec\Evals\PatchHistory\timeout\case-03-add-missing-expected-state\broken-spec\20260426T072926Z`

## 6. Mock Tests Vs Real LLM Evals

- Mock tests validated handler control flow, schema contracts, rollback logic, and safety guards, but they did not expose path-shape drift from real models.
- Real Claude responses sometimes returned relative paths such as `/cooldownLabel/content` or `/visibility`; handler-side anchoring had to be added before these became reliable patches.
- Real relay traffic exposed fenced JSON, model-id instability, cache-read token accounting, and provider-specific response quirks that never appeared in mock runs.
- Real eval also showed that some categories should not be "LLM only". `TIMEOUT` and `SLATE_EVENT_NOT_HANDLED` became materially more stable after adding deterministic fallback repairs.
- The largest architecture correction came from eval-fixture hygiene. Once invalid fixtures were detected explicitly, the success-rate numbers stopped conflating bad data with bad model behavior.

## 7. Recommendations For Round 6

- Add relay-aware cost accounting. The current token totals are useful for comparison, but not enough for a credible USD report.
- Keep deterministic repair layers for `TIMEOUT` and `SLATE_EVENT_NOT_HANDLED`. Do not regress to prompt-only repair for those categories.
- Carry fixture preflight forward into every new eval suite. "Broken spec already passes" and "expected fix does not pass" must stay outside the success-rate denominator.
- Promote more runtime-only capability gaps into compile-time diagnostics where possible. Round 4's complex bind-path limitation is still a good example of a problem that should become explicit earlier.
- Expand beyond single-failure cases. Round 5 proved the single-loop repair path; Round 6 should start mixing compile + runtime + test failures inside the same scenario.

## 8. One-Sentence Surprise

The hardest part of "AI fixes UI specs" was not generating patches; it was converting headless runtime behavior, drifting fixtures, and provider quirks into a deterministic failure surface the patch loop could trust.
