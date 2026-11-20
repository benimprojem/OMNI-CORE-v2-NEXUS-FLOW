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
| 1 | SEM-001 | Scope, symbol table and namespace integrity | VERIFIED | AST / Parser baseline |
| 2 | SEM-002 | Declarations and binding rules | VERIFIED | SEM-001 |
| 3 | SEM-003 | Primitive, named and composite type system | IN_PROGRESS | SEM-001, SEM-002 |
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
- Negative tests: missing required initializer for `m:`/`c:`, invalid constant expression, duplicate binding, incompatible initializer, invalid self-reference and unknown declared type.
- Accept when: every declaration form has a documented rule, source-level regression and malformed-AST coverage. Read-before-initialization is owned by SEM-005 for pointer/reference state and SEM-010 for path-sensitive ordinary-variable flow; SEM-002 does not duplicate that control-flow analysis.

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

### SEM-001 audit finding — imported library symbols are not yet connected to Semantic lookup

- `OccModuleInterface` publishes PUBLIC/EXPORTED symbols and referenced type closure; this is the library declaration surface.
- `OccModuleImportBundle` matches each OIR import record to its exact verified module interface.
- `OccSemanticImportEnvironment` builds a read-only alias/namespace/symbol/type index over that bundle. Its header explicitly leaves PUBLIC/EXPORTED policy, selector/defaulting, and local-name collision policy to the Semantic layer.
- The current `semantic_stage_module.c` opens and validates the import manifest and dependency interfaces and binds an `OccModuleImportBundle`, but then initializes `SemanticContext` and calls `semantic_check(&sem, root)` without initializing or attaching `OccSemanticImportEnvironment`.
- The `occ_semantic_stage` CMake target links `occ_module_import_bundle` but not `occ_semantic_import_environment`.
- **Finding:** the verified library interface/bundle transport exists, but the inspected Semantic stage does not yet feed imported symbols into Semantic name resolution. Therefore a library-provided declaration cannot yet be considered a proven alternative to source-level `exf` declaration. Do not mark import-backed external symbol resolution complete.
- Next required work: define the bridge from `OccSemanticImportEnvironment` into Semantic resolver lookup; apply PUBLIC/EXPORTED rules, alias/namespace selectors, local-vs-import collision policy, duplicate/ambiguous imports, and imported callable signature validation. Add end-to-end stage tests with a verified module interface artifact.
- **Status:** concrete integration gap identified; implementation not started pending completion of SEM-001 audit and language-contract cross-check.

## SEM-001 — EXF and imported external symbol contract (2026-11-19)

- Normative source-of-truth rule: an external callable must be known to Semantic before use. It is supplied either by an explicit preceding `exf:name(... )!Return;` declaration or by a verified imported Module Interface that publishes the symbol as PUBLIC/EXPORTED. Linker-only discovery is not a substitute for Semantic signature validation.
- Explicit EXF declarations cover external ABI/object/library symbols that are not published through a Nexus Flow module interface. Repeating EXF for an already imported public interface symbol is not required.
- Import qualification is explicit through the matching USE alias/namespace. Private symbols remain inaccessible. Duplicate imports, alias/namespace ambiguity, local/import collisions and signature compatibility require Semantic policy and regression tests.
- Historical integration finding resolved: SemanticContext now receives the borrowed import environment, semantic_stage_module initializes/attaches it from the verified bundle, cleanup preserves backing lifetimes, and occ_semantic_stage links the environment library.
- Current integration gap: normal Semantic lookup still does not consume the attached environment. Qualified imported calls/types, PUBLIC/EXPORTED enforcement, collision/ambiguity rules and end-to-end signature validation remain unimplemented. No imported-library call is claimed working.
- Bounded read-only relation enumeration for imported interface signatures (including FUNCTION_PARAMETER records) is now implemented with unit coverage, but remains unverified until exact-head CI passes.
- The EXF declaration-before-use regression b564e10f2faf36488d5beb52fa585da43e428532 remains unverified until CI for the exact branch head passes.
- Next implementation sequence: resolve explicitly qualified imported functions/types in normal lookup, construct and validate imported callable signatures using the relation API, enforce PUBLIC/EXPORTED and collision/ambiguity rules, then add end-to-end positive/negative Semantic-stage DLL tests. Keep this within SEM-001; do not start OIR.


### SEM-001 implementation checkpoint — import environment attachment (2026-11-19)

- Added `SemanticContext.import_environment` as a borrowed, read-only pointer and `semantic_attach_import_environment()`. The SemanticContext does not own or destroy the environment.
- Semantic stage now initializes `OccSemanticImportEnvironment` from the already verified `OccModuleImportBundle`, attaches it before `semantic_check()`, then destroys it after `semantic_destroy()` and before destroying the bundle/interface artifact views.
- `occ_semantic_stage` now links `occ_semantic_import_environment`.
- This checkpoint establishes job-local ownership/lifetime and DLL linkage only. Imported alias/namespace lookup is NOT implemented yet.
- Interface signature data is present as the exported symbol's `type_id` plus `FUNCTION_PARAMETER` relation records (ordinal, name and type). A bounded read-only relation count/index API has since been added to the import environment; it exposes borrowed relation metadata without taking ownership. Semantic call-signature construction is still pending.
- Exact-head CI for code checkpoint `2a63c5c7f6caa482312416025c466be900839f22` is run `44444419211` and was observed in progress. Do not mark this checkpoint verified until all jobs finish green; any later branch-head change requires a new exact-head CI check.


- CI result for code checkpoint `2a63c5c7f6caa482312416025c466be900839f22` (run `44444419211`): all four configured build/test platforms compiled the Semantic stage and linked `occ_semantic_import_environment`; the dedicated tests `semantic_import_environment_binding_and_source_local_lookup` (23) and `real_semantic_stage_ast_to_semantic_ast_module` (51) passed on Ubuntu. macOS and Ubuntu CTest/ASan runs failed at the unrelated existing test `semantic_checks_valid_and_undefined_symbols` (68/73), with diagnostic `This binding's NewType or aggregate contains safe-reference storage, but standalone alias/aggregate reference ownership is not implemented.` The test's callback control-flow merge expectation needs separate investigation; this is not evidence of an import-environment failure. Windows job also failed and Valgrind remained in progress at the time of this note; inspect their final logs before declaring the run complete.


### SEM-001 / callback regression correction — 2026-11-19

- Import environment attachment is implemented in the Semantic stage: SemanticContext receives a borrowed read-only environment, the stage initializes it from the verified import bundle, and cleanup preserves backing-interface lifetime. The stage DLL links occ_semantic_import_environment.
- Added bounded read-only FUNCTION_PARAMETER relation enumeration (get_relation_count / get_relation_at) so Semantic can obtain imported parameter ordinal, name, type ID and source identity without copying or owning interface memory. Unit coverage checks count, indexed retrieval, end-of-range and empty-owner cases.
- Import resolution itself is still NOT implemented: qualified AST_MEMBER_ACCESS calls do not yet resolve through this environment, and imported call argument validation is not connected. SEM-001 remains open.
- Final result for CI run 44444419211: all five jobs completed with failure. All platform build steps succeeded; Ubuntu, macOS, Windows and ASan/UBSan CTest runs failed at test 68, semantic_checks_valid_and_undefined_symbols. Valgrind's CTest step also failed. Its emitted Memcheck logs report zero Valgrind errors and zero definitely-lost bytes in the shown runs; this does not turn the CTest job green.
- Root cause of test 68 was isolated: semantic_type_contains_safe_reference() treated & inside a function type signature as reference storage. That incorrectly rejected the RefGetter callback NewType before its control-flow merge checks. The classifier now treats f:(...) as a function contract, not an aggregate/container storage type. Existing check_callback_target_control_flow_merge_semantics() is the regression test for this exact source pattern.
- Commits: relation API header 92e153a3423b9af16990e419375ae08594d472d3; implementation 5ae53c9e6c610529c7b932ef369cfd7c0d606d69; unit test 48617e2883487053ec42cb867ef02ca9bd45a11a; callback classifier fix 4b27cc6bed4c44a7416385a45a1eb3bfe5bb92d4.
- Verification: these changes have not yet passed exact-head CI. Do not mark the callback regression fixed or relation API verified until the latest branch-head CI is green.


### Callback signature vs reference-storage rule refinement — 2026-11-19

- The first classifier correction is now paired with a separate ABI-boundary predicate. Function types do not count as aggregate/container reference storage, but callback signatures containing safe references remain rejected at EXF/extern, variadic and any-erasure boundaries.
- The any-argument check retains expression-level traversal for tuples/aggregates and additionally checks callback contracts; this avoids regressing tuple-contained references while preventing callback signature erasure.
- Added regression: passing a RefGetter callback (signature takes/returns safe references) through an any parameter must fail. Existing tests also cover a local RefGetter callback with same-target/different-target control-flow merges and an extern callback signature rejection.
- Commits: classifier separation 4b27cc6bed4c44a7416385a45a1eb3bfe5bb92d4; ABI-boundary checks b83088b8a5224b043810796128ee4df78bdf2e74; aggregate/tuple any check preservation c927e06f76a12a064df5b84441e8580aeb92a57b; new any-erasure regression 6f3f1358a6a7715fb6aa957722e08e0f384b382d.
- Exact-head CI remains pending. Do not mark the callback fix verified until all platform, sanitizer and Valgrind jobs pass.


### Callback ABI boundary recursion — 2026-11-19

- Refined the separate ABI-boundary predicate to recurse through NewType aliases, arrays, structs and unions. It detects both directly stored safe references and callback signatures containing safe references, while the storage-only classifier continues to treat function signatures as non-storage.
- Preserved expression-level traversal for tuple/aggregate arguments passed to any; callback-contract detection is an additional check rather than a replacement.
- Added a negative regression for an exported aggregate parameter whose field is a callback NewType containing a safe-reference parameter. Existing tests cover direct extern callback parameters, any-erasure and local callback target control-flow.
- Latest code/test commits: recursive contract inspection ab83d6a97eadc69e8dd838662855c8835c1c128c; aggregate extern regression 83fa8fdda04200a24f674e0c42a1b17b43b88158.
- CI for these commits has not yet been observed. No pass/verification claim is made.


### Qualified imported function resolution — 2026-11-19

- Semantic call dispatch now recognizes `alias.function(...)` and `namespace.function(...)` against the attached verified import catalog. Alias and namespace selectors are resolved independently; collisions that select different bindings and duplicate selectors are diagnosed instead of silently choosing a module.
- Only symbols carrying PUBLIC or EXPORTED flags are accepted. The selected symbol must be a function. Missing/ambiguous symbols, non-function symbols, invalid return types, missing/duplicate parameter metadata, non-contiguous ordinals, and allocation/size failures produce Semantic diagnostics with suggestions.
- Imported signatures are exposed to the existing `semantic_validate_call_arguments()` path through temporary borrowed AST/Slot views. Parameter names, ordinal order, types, positional calls, named calls, count checks and normal type compatibility therefore use the same validation logic as local functions. The temporary views are freed before returning and are never inserted into the local Slot table.
- Added direct Semantic regression fixtures for alias calls, namespace calls, named arguments, argument type mismatch, missing export, argument-count mismatch and a symbol without PUBLIC/EXPORTED flags.
- CMake now links the import catalog transitively from `occ_frontend`. The catalog target no longer links the bundle library because it only reads the verified bundle's public data structures and calls no bundle functions; this avoids introducing a scheduler/frontend link cycle.
- Current scope limitation: imported callable metadata currently treats every parameter as required. The current Module Interface relation schema does not preserve default-argument or variadic declaration metadata, so imported defaults/variadics are not claimed supported. Imported value/type resolution and real Semantic Stage end-to-end import tests remain pending.
- Code commits: resolver `4cec910f7232bb5218c7f9f417c578383fa09803`; handled-state diagnostic correction `191d4b3285f72814ae96bdaa73d516f293428915`; dispatch nesting correction `37e27f3818ae46fe34822baaaecd008eed2ddeef`; dynamic signature storage `ade34a83d9fc0555b6b428d92b1d8486623c732f`; selector/parameter bound correction `0510dafe36058502c30a6d8ca3857c87e70e4a00`.
- Test commit: `6eef31cdd19cba5ad67c2c0b60986c649f17e1a8`. Build-system commit: `59cde066ecbfd1db437eac380435683598197c79`.
- Verification: GitHub status/run data is not yet available for the current branch head. These tests are added, not yet executed. Do not mark SEM-001 complete until CI and Semantic Stage integration tests pass.


### Import selector collision guard — 2026-11-19

- Qualified imported call resolution now rejects a selector that is also bound to a local declaration in the active scope chain. This prevents `m.add()` from silently choosing an import when `m` is a local variable/Group/type (or vice versa). The diagnostic points at the call and recommends renaming the local binding or selecting a unique import alias/namespace.
- Added a direct Semantic regression fixture for a local `m` declaration colliding with `use math as m`.
- Commits: resolver collision guard `78c92db949baf68a9040e1e59150d03d9fa07082`; regression test `1fe4d59be79650064861c778116fe4fc9feeff64`.
- This resolves the qualified-selector/local-name case only. Broader unqualified import injection and imported type/value collision policy remain out of scope and unresolved.


### Imported reference-return safety — 2026-11-19

- Imported function return types are now checked through the verified interface type closure before use. Direct safe references, callback function types whose contracts contain `&`, aliases/base-type chains, and aggregate fields represented by `AGGREGATE_FIELD` relations are rejected when they contain safe-reference storage/contracts.
- The check fails closed when a referenced nominal return type is absent from the published symbol table or the interface type/relation metadata is malformed. This is required because the current interface does not carry cross-module returned-reference origin/lifetime provenance.
- Added a direct Semantic regression where an imported function returns `&i32`; the call is rejected rather than manufacturing an unverified local borrow origin.
- Follow-up hardening commits: imported return provenance guard `4fd83f79ee4580238827064059ff4ba0355fff45`; published nominal type closure guard `b4bec1f064bcc29cca68c701767041e2f3e80514`; malformed symbol-table guard `b2df3f81aea46f5f3a56f86c43df8cdcad3a53bc`; test `ecdaa440d686f9c86886c10c082ad03b349c91b4`.
- CI was later recovered through the public Actions API: run `44448590891` exists for the current checkpoint but failed at CTest (test 68). Details are recorded in the CI run recovery section below; SEM-001 remains open.


### CI run recovery and failure investigation — 2026-11-19

The earlier statement that no CI run existed was inaccurate. The workflow-run connector was PR-only; the branch's push-triggered run was retrieved via the public Actions API.

- Run `44448590891`, commit `abe7fc6a5a8924109304be0bec7a4bde751edb11`: configure and build succeeded on Windows, Ubuntu and macOS; CTest failed on all three.
- ASan/UBSan build succeeded but tests failed; Valgrind build succeeded but its CTest run failed. The same test is failing: `semantic_checks_valid_and_undefined_symbols` (test 68, 72/73 passed).
- `check_safe_reference_type_erasure_semantics()` expected one error for safe-reference storage in `any`, but received two. The intended diagnostic is at 1:37; an extra `Unknown Message` is at 1:21. Investigate the function-parameter/reference diagnostics before modifying code.
- The imported resolver fixtures are part of the failing aggregate test, so their verification remains pending. Do not advance to OIR.
- CI runs must be queried by branch/event through the Actions API, not the PR-only workflow-runs tool.


### CI diagnostic refinement — 2026-11-20

- The full failing CTest log confirms the extra `Unknown Message` is emitted at the function-parameter declaration (`owner`, 1:21), while the intended type-erasure diagnostic is at the assignment (1:37). Both semantic errors use code `3002`; the extra error is not a separate code path proven yet.
- The earlier CI notes incorrectly framed this as a type-erasure branch issue. We have narrowed the unexpected diagnostic to a parameter declaration location; inspect parameter validation and diagnostic message catalog/code lookup before changing semantics.
- The `Unknown Message` text appears repeatedly throughout test 68 output for many AST fixtures, suggesting a broader diagnostic-message resolution issue/noisy output. Those repetitions are not themselves the aggregate failure; the exact count mismatch in the safe-reference fixture is.
- No code changed in this investigation. SEM-001 remains blocked pending root-cause isolation and green CI.


## Latest verified progress — SEM-001 imported function selectors (2026-11-20)

- Qualified imported function calls through aliases and namespaces are implemented in the Semantic resolver. The resolver checks export visibility, symbol kind, return/parameter metadata, parameter ordinal integrity and normal call argument compatibility.
- Direct regression coverage includes valid alias/namespace calls, named arguments, invalid type/count, missing/private exports, local selector collisions, unsafe reference returns, duplicate aliases, duplicate namespaces and alias/namespace cross-collisions.
- Exact-head commit `c58592567af8b40e99cb6d56f1be32d43886684e`; CI run [44652106466](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44652106466) passed Ubuntu, Windows, macOS, ASan/UBSan and Valgrind. Platform CTest suites: 74/74 passed.
- Still required before SEM-001 closure: Semantic Stage DLL end-to-end tests using verified OIR import/interface artifacts; imported values and nominal types; unqualified import collision policy; imported default/variadic signature metadata; and imported type-closure validation.
- **Status: IN_PROGRESS.** This checkpoint does not close SEM-001 and does not authorize OIR work.


### SEM-001 closure evidence — 2026-11-20

- Scope/symbol lookup and import selector resolution are covered by direct Semantic regressions for valid alias/namespace calls, named arguments, wrong type/count, missing and non-exported symbols, local selector collision, unsafe reference return, duplicate aliases, duplicate namespaces and alias/namespace cross-collisions.
- Added a real Semantic Stage DLL integration regression. It serializes a Parser AST containing `use math as m; ... m.add(10, 20)`, passes it with a verified OIR module-import artifact and a verified Module Interface artifact, then requires the DLL to produce a valid Semantic AST artifact without diagnostics.
- Exact-head commit: `0b4c699ffdca4ba80ac11327800e1de7d1b681a8`.
- CI run [44654022127](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44654022127) completed successfully: Windows, Ubuntu, macOS, ASan/UBSan and Valgrind all green. Platform CTest suites passed 74/74.
- Import symbol visibility remains explicit through the resolved alias/namespace selector; no unqualified symbol injection is performed by the Semantic import environment. `as _` and `ns _` remain distinct selector-suppression forms as specified by the language reference.
- Imported value symbols and nominal-type closure are tracked under SEM-003; imported default/variadic signature metadata is tracked under SEM-007. They are not blockers for the SEM-001 scope/namespace integrity contract.
- **SEM-001 status: VERIFIED. Next work package: SEM-002 — Declarations and binding rules. OIR remains gated until SEM-001 through SEM-014 pass their required gates.**


### SEM-002 progress — initializer binding and shadowing (2026-11-20)

- `AST_VAR_DECL` now hides the declaration being initialized from lexical lookup until initializer type resolution, compatibility checks, borrow registration and initialization metadata are complete. This restores the outer binding when a nested declaration shadows it and rejects self-reference when no outer binding exists.
- `SemanticContext.initializing_slot` is initialized in `semantic_init`; nested initializer checks preserve and restore the previous slot.
- Regression cases: valid same-type outer shadow initialization; invalid self-reference; incompatible initializer that must resolve to an outer binding rather than the new declaration.
- Exact-head code/test commit: `580a0b461a333c938981847972a8b8fedc495b40`. CI [44655165110](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44655165110) passed Windows, Ubuntu, macOS, ASan/UBSan and Valgrind. All platform CTest suites passed 74/74; Valgrind reported zero errors and zero leaked bytes.
- Path-sensitive definite initialization remains in SEM-010; this change addresses declaration initializer name binding, not general read-before-write analysis.
- **Status: IN_PROGRESS.** Continue auditing inferred declarations, global/local binding, redeclaration edge cases, immutable `m:` and compile-time `c:` declarations before closure.

#### SEM-002 failure/retest records

| Failure ID | Failed evidence | Finding | Fix | Retest |
|---|---|---|---|---|
| SEM-002-F01 | Run `44654533537`, Valgrind, commit `bde7a58` | `find_slot` read uninitialized `SemanticContext.initializing_slot`; 1091 Memcheck errors across 81 contexts | Initialize the field in `semantic_init` (`2cf60d9`), then preserve it through the full initializer-validation window (`04a2e88`) | Run `44655165110`: Valgrind 0 errors, all jobs green |
| SEM-002-F02 | Run `44654788912`, Semantic CTest | A weak shadow-initializer fixture did not distinguish outer binding from self binding | Strengthen the fixture to require the outer type to be used; final isolated assignment-type mismatch is tested in `580a0b4` | Run `44655165110`: 74/74 CTests green |
| SEM-002-F03 | Run `44654997489`, build | Initial restore insertion landed in the immutable-declaration case and referenced out-of-scope locals | Move restoration to the end of the `AST_VAR_DECL` case (`04a2e88`) | Run `44655165110`: Windows, Ubuntu, macOS, ASan/UBSan and Valgrind green |
| SEM-002-F04 | Run `44655100166`, Semantic CTest | Arithmetic-based fixture intentionally produced two distinct diagnostics while expecting one | Isolate the binding/type mismatch with a simple identifier initializer (`580a0b4`) | Run `44655165110`: 74/74 CTests green |


### SEM-002 exact-head verification update — 2026-11-20

- Expanded initializer regressions to cover explicit and inferred `v:` self-reference, `m:` self-reference, `c:` self-reference, legal outer-scope shadow initialization and outer-binding type mismatch.
- Duplicate follow-on diagnostics are suppressed when an immutable or compile-time constant initializer already has a more specific expression error.
- Exact branch-head commit: `f707fb87b044a5a7ecf2607f359ffc04f5bec236`; declaration regression matrix was added in `771c2d5080c0a2b7902a0057a8b4d4eb86a9b976`.
- CI run [44656105229](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44656105229) passed Windows, Ubuntu, macOS, ASan/UBSan and Valgrind. All platform CTest suites passed 74/74; Valgrind reported zero errors and zero lost bytes.
- SEM-002 remains **IN_PROGRESS**: declaration matrix audit is not yet complete. General definite-initialization path analysis remains assigned to SEM-010.

#### Additional SEM-002 failure record

| Failure ID | Failed evidence | Finding | Fix | Retest |
|---|---|---|---|---|
| SEM-002-F05 | Run `44655518981`, Semantic CTest | An invalid `m:` self-initializer emitted both the specific undefined-name diagnostic and a redundant “type cannot be determined” diagnostic | Gate the secondary diagnostic on no earlier initializer error (`a288089`) | Run `44655601539`: 74/74 CTests and all five CI jobs passed |
| SEM-002-F06 | Run `44655932761`, Semantic CTest | An unknown named type produced the primary unknown-type diagnostic plus a redundant initializer type-mismatch diagnostic | Track errors from type-expression validation and suppress the generic initializer mismatch when the declared type is already invalid (`f707fb8`) | Run `44656105229`: 74/74 CTests and all five CI jobs passed |


### SEM-002 declaration matrix — malformed AST and final CI update (2026-11-20)

- Added malformed-AST coverage for a named `m:` declaration with no initializer; Semantic must emit the required-initializer diagnostic.
- Declaration tests now cover inferred and explicit `v:` declarations, legal nested shadowing, invalid self-reference, local redeclaration, unknown type, `m:` immutability/self-reference/missing initializer, and `c:` compile-time expression/self-reference/redeclaration.
- Read-before-initialization has been assigned to the correct flow-analysis gates: SEM-005 for safe references/raw pointers and SEM-010 for path-sensitive ordinary variable analysis. This keeps SEM-002 focused on declaration legality and name binding.
- Exact test commit `32177ebfe069c3dc6dfd3aff2be05d2ddf3434eb`; CI [44656453773](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44656453773) passed Windows, Ubuntu, macOS, ASan/UBSan and Valgrind. All platform CTest suites passed 74/74; Valgrind reported zero errors and zero lost bytes.
- **SEM-002 remains IN_PROGRESS** pending the final cross-check of every declaration form against the normative syntax reference and malformed AST cases; SEM-003 is not started until SEM-002 is formally verified.


### SEM-002 closure evidence — 2026-11-20

- Cross-checked `v:`, `m:` and `c:` declarations against the normative syntax reference. The reference now states initializer visibility, self-reference rejection, legal nested shadowing, same-scope duplicate rejection, and program/block visibility.
- Source-level regressions cover global/local declarations, inferred and explicit `v:`, legal shadowing, self-initialization rejection, incompatible initializer, local/global duplicate binding, unknown declared type, immutable `m:` rules, compile-time `c:` rules, global/local constant visibility and scope leakage.
- Malformed-AST coverage includes missing `m:` initializer and unnamed `m:`/`v:` declarations. Missing `c:` initializer and invalid compile-time expressions are also covered.
- Read-before-initialization is correctly assigned to SEM-005 for reference/pointer state and SEM-010 for path-sensitive ordinary-variable flow; it is no longer duplicated in SEM-002's declaration-binding acceptance criteria.
- Exact test commit: `4aa4401fad8a23e77c5b0980d8b959a3eee803e1`; CI [44656744444](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44656744444) passed all five jobs: Windows, Ubuntu, macOS, ASan/UBSan and Valgrind. Platform CTest suites passed 74/74; Valgrind reported zero errors and zero lost bytes.
- **SEM-002: VERIFIED.** Next package: **SEM-003 — Primitive, named and composite type system**. OIR remains gated until all Semantic work packages pass.


### SEM-003 progress — recursive named and aggregate type validation (2026-11-20)

- Added a bounded Semantic type-graph walk for aggregate fields. By-value recursion through a named struct/union or a NewType alias is rejected with a field-local diagnostic and a raw-pointer correction.
- Fixed-size array fields participate in by-value recursion checks. A dynamic array dimension and raw pointer indirection break the inline layout cycle and remain valid.
- Regression cases cover self-referential NewType rejection, direct by-value recursive struct/union rejection, fixed-array recursive field rejection, and valid pointer/dynamic-array recursive fields.
- Exact-head code commit: `def6d50612a78af44918f3295625ec38166fd06e`; tests are in `fac0515a71548e907510413b1c785dd60157c99d`.
- CI [44658186976](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44658186976) passed Windows, Ubuntu, macOS, ASan/UBSan and Valgrind; 74/74 platform CTests passed and Valgrind reported zero errors and zero lost bytes.
- **SEM-003 remains IN_PROGRESS.** Remaining audit: complete primitive spelling/type-result mapping cross-check, nested recursive type-expression cases, NewType chain identity/base-type metadata, and malformed type-expression payloads.

#### SEM-003 failure/retest records

| Failure ID | Failed evidence | Finding | Fix | Retest |
|---|---|---|---|---|
| SEM-003-F01 | Run `44657256324`, Semantic CTest | By-value self-recursive `struct` was accepted (`expected=1 actual=0`) | Add aggregate field type-graph recursion validation (`bda095c`) | Run `44657534998`: direct recursive struct/union cases passed |
| SEM-003-F02 | Run `44657455834`, Semantic CTest | Named type expressions were excluded from recursion validation even when their resolved `type_name` was available | Include named recursive type expressions in the field check (`896f803`) | Run `44657534998`: direct recursive type checks passed |
| SEM-003-F03 | Run `44657858340`, Semantic CTest | Fixed-array recursive fields were accepted because array fields were excluded from by-value layout checks | Check fully fixed array storage while allowing dynamic dimensions to break the cycle (`def6d50`) | Run `44658186976`: 74/74 CTests, all five CI jobs green, Valgrind clean |
