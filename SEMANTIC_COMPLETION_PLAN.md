# Semantic Completion & Verification Plan

**Branch:** `refactor/c-core-roadmap`  
**Owner / final gate:** OCC architecture controller  
**Scope:** C implementation of Nexus Flow Semantic analysis only  
**Status:** Active plan; individual task states require source and CI evidence.

> This is a traceability and closure plan, not a claim that every listed feature is missing. Existing implementation and test coverage must be audited first. Extend existing code where possible; do not duplicate working implementations. OIR work remains gated until every required Semantic task is verified.

## 1. Status model

| Status | Meaning |
|---|---|
| `NOT_AUDITED` | Source, callers, docs and tests have not yet been checked for this task |
| `AUDIT_REQUIRED` | Existing coverage is known to be partial or needs a fresh end-to-end trace |
| `IN_PROGRESS` | Implementation or regression work is active |
| `TEST_FAILED` | A test/build/sanitizer failure is recorded and awaits triage |
| `BLOCKED` | Requires a language or architecture decision, unavailable dependency, or reproducible unresolved issue |
| `VERIFIED` | Acceptance criteria met and required CI evidence is green |
| `DEFERRED_BY_DESIGN` | Explicitly outside the current language contract; rationale documented |

A feature is not `VERIFIED` merely because it parses, has an AST tag, compiles locally, or has a test committed.

## 2. Ordered work packages

The order below is a dependency order. Audit results may mark an item already implemented, but it still needs evidence before closure.

| Order | ID | Work package | Initial status | Depends on |
|---:|---|---|---|---|
| 1 | SEM-001 | Scope, symbol table and namespace integrity | AUDIT_REQUIRED | AST / Parser baseline |
| 2 | SEM-002 | Declarations and binding rules | AUDIT_REQUIRED | SEM-001 |
| 3 | SEM-003 | Primitive, named and composite type system | AUDIT_REQUIRED | SEM-001, SEM-002 |
| 4 | SEM-004 | Type inference, conversion, promotion and forced types | AUDIT_REQUIRED | SEM-003 |
| 5 | SEM-005 | Pointer, reference, borrow, lifetime and initialization | AUDIT_REQUIRED | SEM-001–004 |
| 6 | SEM-006 | Struct, union, enum, array, Map and Vec semantics | AUDIT_REQUIRED | SEM-003–005 |
| 7 | SEM-007 | Function, Group, method and call binding | AUDIT_REQUIRED | SEM-001–006 |
| 8 | SEM-008 | Lambda, callback and function-value semantics | AUDIT_REQUIRED | SEM-005, SEM-007 |
| 9 | SEM-009 | Ownership, move/copy, Handler and relocation | AUDIT_REQUIRED | SEM-005–008 |
| 10 | SEM-010 | Control-flow and path-sensitive validation | AUDIT_REQUIRED | SEM-005–009 |
| 11 | SEM-011 | Nexus Flow-specific constructs and module boundaries | AUDIT_REQUIRED | SEM-001–010 |
| 12 | SEM-012 | Diagnostics quality and diagnostic stability | AUDIT_REQUIRED | SEM-001–011 |
| 13 | SEM-013 | Semantic AST/result artifact integrity | AUDIT_REQUIRED | SEM-001–012 |
| 14 | SEM-014 | Cross-platform integration and Semantic release gate | NOT_AUDITED | SEM-001–013 |

### SEM-001 — Scope, symbols and namespace integrity
- Inspect: scope creation/destruction, symbol registration/lookup, module imports, aliases, namespace qualification, visibility/export, shadowing and duplicate detection.
- Positive tests: nested scopes, legal shadowing, imported aliases, qualified names, visibility from permitted scopes.
- Negative tests: duplicate declarations, inaccessible/private symbols, unresolved qualified names, invalid alias collisions, use outside lifetime/scope.
- Accept when: lookup rules are deterministic; diagnostics identify declaration and use sites; all relevant tests and memory checks pass.

### SEM-002 — Declarations and binding rules
- Inspect: variable/constant/global/local declarations, initializer ordering, inferred declarations, redeclaration, immutable bindings and compile-time-only declarations.
- Positive tests: valid initialized and inferred declarations, legal local/global forms, valid constants.
- Negative tests: missing required initializer, invalid constant expression, duplicate binding, incompatible initializer, read-before-initialization.
- Accept when: every declaration form has a documented rule, source-level regression and malformed-AST coverage.

### SEM-003 — Type system
- Inspect: primitive widths/signs/floats, aliases, NewType nominal identity, pointer/reference layers, arrays, tuples/callback signatures and recursive type validation.
- Positive tests: every supported type spelling and valid nested type form.
- Negative tests: unknown type, illegal nesting, invalid alias target/cycle, incompatible nominal type and malformed type payload.
- Accept when: parser/type-expression/semantic behavior agrees with the normative language reference; no type is accepted solely by string-prefix or partial-name matching.

### SEM-004 — Inference and conversions
- Inspect: inference unification, numeric promotion, explicit casts, forced type syntax (`!`), bit/byte-width types, arithmetic/bitwise/shift operators and overflow policy where defined.
- Positive tests: compatible widening, explicit legal narrowing, consistent generic inference and valid operator/type pairs.
- Negative tests: conflicting inference, implicit unsafe narrowing, invalid forced type, invalid operator/type pair and width mismatch.
- Accept when: each conversion is explicit in the language contract and diagnostics identify source type, target type and required correction.

### SEM-005 — Pointer and reference safety
- Inspect: safe `&T` / `&mut T`, raw `*T`, borrow exclusivity, projection overlap, lifetime origins, initialization, nullability, escape and callback return provenance.
- Positive tests: independent struct fields/constant indexes, valid shared/exclusive borrow, initialized reference use, valid parameter-origin propagation.
- Negative tests: conflicting borrow, use-before-init, use-after-scope, longer-lived escape, unknown provenance, unsupported safe-reference storage and invalid raw-to-safe conversion.
- Accept when: conservative rejection is documented; no unsupported provenance is silently accepted; branch/loop/call/aggregate cases have positive and negative tests. Raw pointers must not be described as receiving safe-reference guarantees.

### SEM-006 — Aggregate and container semantics
- Inspect: Struct and packed Struct fields/layout metadata, Struct literals, C-like Union contract, Enum discriminants/payloads/patterns, arrays, Map and Vec/SIMD operations.
- Positive tests: valid construction, field access, enum payload binding, supported indexing and all declared Vec operators.
- Negative tests: missing/duplicate/unknown fields, invalid payload, invalid discriminant, unsupported key/value or element operation, dimension mismatch.
- Accept when: all supported operations and explicitly unsupported cases are documented and tested; storage/lifetime rules do not overpromise OIR/runtime behavior.

### SEM-007 — Functions, Groups, methods and call binding
- Inspect: signatures, defaults, variadics, named/positional arguments, return types, Group visibility, receiver rules, static/instance dispatch and duplicate public names.
- Positive tests: valid direct calls, default suffix, Group-qualified static call, instance call and correct implicit `self`.
- Negative tests: arity, duplicate/unknown/mixed arguments, invalid receiver, return mismatch and malformed AST.
- Accept when: declaration and call-site rules are symmetric and all error paths retain exact source locations.

### SEM-008 — Lambda, callback and function values
- Inspect: callback signatures, capture rules, function values, closure environment, parameter/return provenance and invocation validation.
- Positive tests: valid capture, non-capturing callback, supported callback invocation and known return-origin propagation.
- Negative tests: illegal capture, mismatched signature, unknown return provenance where safety requires rejection, invalid callback escape.
- Accept when: every supported callback form has a type and lifetime contract; unsupported closure semantics are explicitly deferred rather than guessed.

### SEM-009 — Ownership, move/copy, Handler and relocation
- Inspect: single-owner rules, explicit `copy(...)`, moved-from invalidation, Handler table lifetime, `<-`, `->`, `_>`, forward target binding and cleanup.
- Positive tests: valid move, valid copy, valid capture/processing and forward relocation.
- Negative tests: use-after-move, double move, self-relocation, unknown target, type mismatch, duplicate owner and invalid scope/lifetime transfer.
- Accept when: resource ownership and execution routing are distinct; relocation never acts as an implicit control-flow jump; all paths are validated.

### SEM-010 — Control flow and path-sensitive validation
- Inspect: if/else, select/scan, loops/ranges, break/continue, return, halt, unreachable paths, definite initialization and ownership/borrow state merges.
- Positive tests: all-path initialization, valid early return, valid range and branch-local resources.
- Negative tests: missing path initialization, invalid break/continue, use after terminating path, impossible pattern and inconsistent ownership across merges.
- Accept when: zero-iteration loops and branch joins are conservative; each control-flow construct has positive/negative tests and diagnostic location assertions.

### SEM-011 — Nexus Flow constructs and module boundaries
- Inspect: rules/apply/on, rolling/unroll, handler forms, thread blocks, intrinsics, OS module and Thread module boundaries.
- Positive tests: accepted core syntax and generic module calls.
- Negative tests: invalid intrinsic identity/shape, illegal thread capture, invalid handler target and core rules leaking into library-owned behavior.
- Accept when: Core Semantic validates only core language contracts; OS/Thread/runtime-specific behavior remains in its module boundary.

### SEM-012 — Diagnostics
- Inspect: stable diagnostic codes, severity, source spans, primary/secondary labels, cause context, expected/actual values, suggestions, warning policy and bounded diagnostic collection.
- Positive tests: representative failures produce actionable location/message/suggestion triples.
- Negative tests: malformed AST and cascading failures do not crash, mislocate or flood diagnostics.
- Accept when: each Semantic error family has stable, asserted diagnostics; warnings do not silently become errors unless the documented strict compilation policy requires output suppression.

### SEM-013 — Semantic result and artifact integrity
- Inspect: AST annotations, resolved symbols, inferred types, ownership/borrow metadata, source provenance, serialization boundaries and validation before downstream consumption.
- Positive tests: complete valid Semantic result with consistent cross-references.
- Negative tests: missing/stale annotations, dangling symbol/type references, malformed payload and inconsistent metadata.
- Accept when: downstream stages receive a validated, deterministic Semantic artifact; internal pointers or transient parser state are not leaked across module boundaries.

### SEM-014 — Cross-platform integration and release gate
- Run: clean configure/build/test on Windows, Ubuntu and macOS; ASan/UBSan; Valgrind where supported; full regression suite.
- Verify: no untracked generated source changes, no accidental C++ scope expansion, no main-branch changes, no duplicate implementation, docs and dashboard agree with code.
- Accept when: all required CI jobs pass on the exact branch-head commit and all prior work packages are `VERIFIED` or explicitly `DEFERRED_BY_DESIGN` with approved rationale.

## 3. Agent-role workflow (controlled review passes)

These are logical specialist roles in the OCC workflow. They are not independent authorities and do not replace the final architecture gate.

1. **Planner / auditor:** inspect current source, callers, tests, codebase index and language reference; split the task and record dependencies.
2. **Implementer:** modify only the assigned C module/API and add focused regression tests.
3. **Test reviewer:** review the change independently; run positive, negative, malformed-input and regression tests.
4. **Failure analyst:** record each failure with task ID, commit, CI run/job, file, function, line, failure class, reproduction command, expected/actual result and severity.
5. **Fixer:** address recorded failures without weakening or deleting valid tests; add a regression for each confirmed defect.
6. **Independent final reviewer:** rerun the focused suite and full CI after fixes; do not rely solely on the implementer's report.
7. **Architecture controller:** review API boundaries, language contract, scope, diff, test evidence and docs; only this gate can approve task closure.

### Failure record template

| Field | Required value |
|---|---|
| Failure ID | `SEM-xxx-Fnn` |
| Task / commit | Semantic task ID and exact commit SHA |
| CI evidence | Run ID, job name and step |
| Location | File, function, line/span |
| Classification | Compile, link, runtime, assertion, sanitizer, memory, semantic-rule, documentation |
| Reproduction | Exact command or minimal source |
| Expected / actual | Concrete behavior |
| Severity | Blocker / Critical / Major / Minor |
| Fix commit | Exact commit SHA, once fixed |
| Retest | Focused test and full CI result |

### Retry and escalation policy
- Every failed run creates a failure record; failures are never silently overwritten.
- Fixes must add or preserve a regression test for the defect.
- A failed independent final review returns the task to the fixer queue.
- If the same failure survives two fix/retest cycles, or the fix requires a language/architecture decision, set `BLOCKED` and escalate to the architecture controller instead of looping blindly.
- Continue until all required gates pass or a documented blocker is reached. Never label a blocked task complete.

## 4. Mandatory task completion gate

A task may become `VERIFIED` only when all are true:

- [ ] Scope and dependency audit is recorded.
- [ ] C implementation follows existing module/API boundaries.
- [ ] Positive and negative regression tests are committed.
- [ ] Existing tests remain intact unless a documented language decision changes their expected behavior.
- [ ] Focused tests pass.
- [ ] Full required CI matrix passes on the exact final commit.
- [ ] Failure records are resolved or explicitly linked to an approved blocker.
- [ ] Syntax reference, relevant technical docs, TASKS and dashboard are synchronized.
- [ ] Architecture controller approves the diff and evidence.

## 5. Current baseline and first execution

- Branch head at plan creation: `df235c71ee09a2af7c76e7a01406bf4ed7ca1e2e`.
- Dashboard checkpoint records five green jobs at commit `3e15b23cef36fccb713b629738a48bd7138c77d6`, run `44415847403`, with 73/73 Ubuntu CTest tests. This is historical evidence for that commit, not proof for later commits.
- The current dashboard already records substantial Semantic coverage, including declarations/types, Group/Method, pointer/reference, enums, unions, Map/Vec and relocation checkpoints. Do not reimplement these wholesale.
- The pre-plan code head `df235c71ee09a2af7c76e7a01406bf4ed7ca1e2e` has failing CI run `44426912953`; all five jobs failed in test execution. The actual null dereference at `omnicore-sdk/src/c/parser/semantic.c:6341` is `strcmp(declared_slot->type_name, "tuple")` when `declared_slot->type_name == NULL` — **not** `decl->initializer->type`. The Valgrind trace reproduces it in `test_semantic_oom.c::check_allocation_failures`; the persistent-result integration trace reproduces it in `test_semantic_result_integration.c::test_immutable_vs_compile_time_constant_flags` for `c:CompileValue = 5; m:RuntimeValue = 6;`. The Ubuntu CTest job reports six failures: tests 15, 51, 68, 69, 70 and 73. Fixes were committed on the active branch: `5feba0fe9436333c2463a18f613ca87a7220e5ac` guards the nullable type name; `22c5ec7930461ab6e3dbcfdc87817c4f19761f7d` also recognizes inferred `AST_TUPLE` initializers; `650097083aae1ebf567316f4d7c84ee3e3abea90` adds a regression requiring inferred tuple storage of a safe reference to be rejected. The existing immutable-vs-constant integration case and OOM fault-injection test remain intact. CI for these fix commits has not yet been verified; do not close this failure until the exact branch-head run is green.
- First execution is **SEM-AUDIT-000**: reconcile the latest source, tests, dashboard, syntax reference and CI history; classify every existing row as verified, partial, deferred or unknown; then schedule only the real gaps.
- The Semantic closure gate remains closed until SEM-001 through SEM-014 meet their acceptance criteria. OIR must not be started as a substitute for unfinished Semantic work.

## 6. Progress log

| Date | Task | Result | Evidence |
|---|---|---|---|
| 2026-11-19 | SEM-AUDIT-000 | Branch, source, tests and failing CI logs reconciled; broader work-package classification remains open | `df235c71ee09a2af7c76e7a01406bf4ed7ca1e2e`; CI run `44426912953` |
| 2026-11-19 | SEM-CI-F001 | Fixed nullable `Slot.type_name` dereference; inferred tuple reference-storage guard and regression added; awaiting exact-head CI | Commits `5feba0fe9436333c2463a18f613ca87a7220e5ac`, `22c5ec7930461ab6e3dbcfdc87817c4f19761f7d`, `650097083aae1ebf567316f4d7c84ee3e3abea90` |


### SEM-001 audit note — external declaration ordering (`exf`)

- Parser accepts semicolon-form `exf` as an external declaration (`AST_EXTERN_DECL`); legacy body-form remains accepted for compatibility. Evidence: `omnicore-sdk/src/c/parser/parser.c` around 3515–3520.
- Semantic handles `AST_FUNC_DECL` and `AST_EXTERN_DECL` in the shared function-declaration path. Function-call validation resolves identifier callees via `find_slot()` and validates arguments when a function slot is found. Evidence: `semantic.c` around 7884 onward and 8700 onward.
- The observed top-level traversal routes `AST_PROGRAM` declarations through `semantic_check()` in sequence; no global top-level function predeclaration pass was found during this audit. This suggests extern declarations must precede their use, but it is not yet confirmed by an executable regression test.
- Added a regression in `test_semantic.c`: use `read(value)` in `main` before the later `exf:read(value!i32)!i32;` declaration and expect a Semantic error. Commit: `b564e10f2faf36488d5beb52fa585da43e428532`.
- **Status:** regression committed; runtime/CI validation pending. Do not mark SEM-001 VERIFIED until the test result is confirmed.
