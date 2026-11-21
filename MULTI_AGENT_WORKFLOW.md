# OCC Multi-Agent Development Workflow

**Branch policy:** Active work stays on `refactor/c-core-roadmap`; `main` is not modified during roadmap work.
**Applies to:** All OCC/Nexus Flow work packages—architecture, C implementation, tests, diagnostics, docs, integration, and release verification.
**Status:** Adopted as the default project workflow.

## Purpose

Use multiple independent review roles for each task to reduce self-review blind spots, catch regressions early, and make changes evidence-driven. This workflow is mandatory regardless of task size; small tasks may combine stages into a compact pass, but none of the gates may be silently skipped.

These are role-separated reviews orchestrated by the project controller. Unless dedicated agent execution is explicitly available, roles are simulated as independent review passes—not represented as separate concurrently running agents.

## Roles

| Role | Responsibility | Must not |
|---|---|---|
| **Controller / Architect** | Defines task scope, prerequisites, acceptance criteria, dependency boundaries; resolves conflicts; owns final decision. | Skip required pipeline gates or treat suggestions as evidence. |
| **Implementer** | Inspects current code and makes the smallest targeted C change; adds tests and documentation updates. | Rewrite unrelated modules or mark work complete without test evidence. |
| **Test Designer** | Derives positive, negative, boundary, malformed-input and regression tests from requirements. | Reuse only the implementer's happy-path tests. |
| **Independent Reviewer** | Reviews diff and design against language contract, module boundaries, C safety, memory ownership, ABI/API, error paths, and maintainability. | Assume implementation is correct because tests pass. |
| **Build & Runtime Verifier** | Runs relevant build/test suites, platform checks, sanitizer or memory tools when applicable; checks exact branch/commit. | Report simulated or unrun tests as passed. |
| **Fault Triage / Repair** | Categorizes failures (compile, link, test, runtime, sanitizer, integration, docs), records reproduction, fixes, and re-runs affected checks. | Hide, downgrade, or delete a failure without rationale and regression coverage. |
| **Documentation Auditor** | Checks normative references, status dashboard, progress docs, task tracking, README links, and claims against evidence. | Advance status based on code presence alone. |

## Required cycle

1. **Task selection:** Read the project roadmap/status and relevant normative docs; verify branch and current source state. Confirm prerequisites and choose one bounded task.
2. **Architect pass:** Write scope, dependencies, non-goals, acceptance criteria, likely risk, and files/modules expected to change.
3. **Baseline pass:** Inspect current implementation and tests before editing. Reuse existing code; record baseline failures and tool errors.
4. **Test design pass:** Define expected behavior and regression cases before/alongside implementation. Include negative and boundary cases where meaningful.
5. **Implementation pass:** Make a focused C-only change. Keep unrelated changes out. Avoid introducing C++.
6. **Independent review pass:** Inspect the resulting diff separately for contract mismatch, omitted cases, unsafe C behavior, ownership/lifetime issues, API coupling, diagnostics, and regressions.
7. **Test/verification pass:** Run targeted tests first, then the relevant broader suite, cross-platform CI and memory/sanitizer checks as required by the task.
8. **Repair loop:** If any review or verification finds a problem, log it, fix it, and return to steps 6–7 until all required checks are green or the task is explicitly blocked.
9. **Documentation pass:** Update task/coverage/status documents and README index when applicable. Status changes require source + test + verification evidence.
10. **Controller closure:** Record files changed, test commands/results, commit/CI evidence, residual risks, and the next dependency-ordered task. Do not advance to a dependent phase while required gates are open.

## Verification gates and status discipline

- **VERIFIED** requires implementation, test coverage, independent review, and relevant CI evidence (exact commit/head and successful checks).
- **PARTIAL / IN_PROGRESS** means code or tests exist but some acceptance criteria remain.
- **BLOCKED** requires a documented reason and the smallest unblock action.
- **NOT VERIFIED / PENDING** is used for missing CI, unavailable tools, skipped actions, or ambiguous results.
- A test not run is never marked passing. A tool call that errors, is skipped, times out, or returns incomplete data is recorded as a tooling limitation, not as successful work.
- Existing green CI for an earlier commit does not verify later commits. Always confirm the exact head under review.
- On failed checks, preserve the failure signal, classify it, add a reproducer/regression, fix, and rerun.

## Review checklist

- [ ] Task is dependency-ordered and scope is bounded.
- [ ] Normative syntax/architecture documents checked.
- [ ] Existing code and baseline tests inspected.
- [ ] Test matrix covers success, invalid use, boundary and regression cases as relevant.
- [ ] C-only implementation; no unrelated churn.
- [ ] Independent review performed.
- [ ] Build, tests and memory/safety checks run as required.
- [ ] Exact commit/head CI verified; tool failures recorded.
- [ ] Docs/status/dashboard updated only with evidence.
- [ ] Closure note includes remaining risks and next task.

## Multi-agent iteration record

For each task, maintain a compact record with:
- Task ID and short objective
- Controller/architect scope and acceptance criteria
- Implementer change summary
- Test designer coverage
- Independent reviewer findings
- Build/verification results (exact commit, platform/check, pass/fail/pending)
- Faults and repair iterations
- Documentation updates
- Final disposition and next task

## OCC-specific constraints

- Preserve the required progression: **AST → Parser → Lexer → Semantic → OIR → Verifier → Codegen** as governed by the project roadmap and current gates.
- Keep stage responsibilities separated; general validation does not move into Codegen.
- OIR is the standard inter-module artifact; respect defined schemas, IDs and diagnostics contracts.
- Do not advance OIR/Verifier/Codegen work ahead of unresolved prerequisite gates.
- Preserve existing behavior unless a reviewed requirement or bug justifies a change.
- All source, build logic and tests for the active compiler are C-based. C++ is out of scope.
- Use the active development branch; do not change `main` unless explicitly authorized.

## Tool reliability and action log

Record action/tool failures, skipped operations, incomplete API responses, and unexplained delays in the task iteration log. Do not repeat a failing action blindly. Retry once with a corrected or narrower call; if still unsuccessful, use a different read path or leave the task pending with a clear blocker. User-visible claims must distinguish verified repository changes from plans, review notes, and simulated role passes.
