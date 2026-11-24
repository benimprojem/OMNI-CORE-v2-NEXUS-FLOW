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
| 3 | SEM-003 | Primitive, named and composite type system | VERIFIED | SEM-001, SEM-002 |
| 4 | SEM-004 | Type inference, conversion, promotion and forced types | VERIFIED | SEM-003 |
| 5 | SEM-005 | Pointer, reference, borrow, lifetime and initialization | IN_PROGRESS | SEM-001–004 |
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

### SEM-004 forced bit/byte assignment conversion — CI verified (2026-11-20)

- Refactored hard representation conversion validation to accept the actual source expression, hard-type flag and destination representation. Variable initializers and identifier assignments now share the same range/width decision logic; the assignment path no longer accidentally evaluates the declaration's original initializer.
- Added assignment regressions for an in-range constant, an out-of-range constant and a runtime narrowing that cannot be proven safe. Existing initializer regressions cover equivalent cases.
- Exact implementation/test head: `405f9b45baa9ad83b0cbc58ac65bf371d39f213b`.
- CI run [44700923242](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44700923242) passed all five jobs: Windows, Ubuntu, macOS, ASan/UBSan and Valgrind. All platform CTest suites passed 74/74. Valgrind reported zero errors and zero definitely/indirectly lost bytes.
- **SEM-004 closure decision:** Keep implicit numeric promotion conservative and deterministic. Integer operands follow the existing width/signedness promotion; same-family `f*` and `d*` operands widen by width. Mixed integer/binary-float, integer/decimal, and binary-float/decimal operations are rejected; explicit `cast()` is required for cross-family conversion. `!ReturnType` remains a compatibility check, not a coercion instruction. Return conversion and projected index/raw-pointer Hard-Lock propagation are not specified by the AST/language contract and are explicitly deferred; no semantics are guessed.


### SEM-004 — forced-type context expansion and conversion-result audit (2026-11-20)

- Applied the documented Hard-Lock range policy to packed bit-vector and byte-block element writes. `Nbit[index]` and `Nbyte[index]` are explicitly defined as `bit` and `byte` elements in the syntax reference; constant range checks, safe unsigned widening and rejected runtime narrowing now apply at those indexed write sites.
- Extended semantic regression coverage for bit and byte index writes (in-range/out-of-range constants, safe unsigned source and unsafe runtime narrowing). CI [44738188277](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44738188277) and [44738193873](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44738193873) passed all five jobs.
- Extended end-to-end SemanticResult/OIR artifact tests. Conversion records are now asserted for a variable initializer, identifier assignment, packed bit index write, aggregate field initializer and aggregate member assignment; the encoded artifact is checked field-by-field against the original SemanticResult. CI [44738199830](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44738199830) and [44738378158](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44738378158) passed all five jobs.
- Closed the integer Hard-Lock gap for signed/unsigned integer targets (`i8`…`i128`, `u8`…`u128`): compile-time constants are checked against the target's signed/unsigned range; runtime signed-to-unsigned and unsafe narrowing are rejected; proven widening is accepted. SemanticResult conversion classification now records width changes and equal-width signedness changes. CI [44738936515](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44738936515), [44738941707](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44738941707) and [44738949056](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44738949056) passed all five jobs.
- Added safe same-family floating/decimal Hard-Lock widening (`f32`→`f64`, `d32`→`d64`); narrowing is rejected unless the user makes an explicit cast. SemanticResult/OIR metadata round-trip for both conversions passed all five CI jobs in [44739346886](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44739346886).
- Added hard integer assignment tests for in-range constants, out-of-range constants and unsafe runtime narrowing. All five CI jobs passed in [44739394050](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44739394050).
- **Wide literal evaluator deferred by design (2026-11-20):** implementing full 128-bit contextual integer literal range support would require a wider constant representation and coordinated Semantic/test changes. Do not make that large change in this phase. Keep existing `i128`/`u128` type recognition and width/type checks; do not claim arbitrary 128-bit literal range support. Values beyond the existing signed-64 evaluator boundary are not guaranteed to be accepted/range-checked correctly. Revisit later with a portable integer utility library (128-bit, possibly 256-bit) rather than invasive Semantic changes. `f128`/`d128` literal expansion also remains deferred.
- The syntax reference does not currently define a universal runtime arithmetic overflow policy; it explicitly leaves literal overflow and target-specific lowering as separate checks. Semantic does not invent wrapping/saturation/trap behavior here. This remains a documented boundary for the later OIR/target contract.
- Hard-Lock diagnostics now include source type, target type and a corrective action. The dedicated diagnostic regression passed all five CI jobs in [44738643809](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44738643809).
- Scope boundary: `!ReturnType` is a function return declaration, not a conversion instruction; no return cast is inferred.
- **SEM-004 return boundary audit (2026-11-20):** `AST_RETURN` currently type-checks returned values through `are_types_compatible()` and does not synthesize or record numeric/Hard-Lock representation conversions. The syntax reference defines `!ReturnType` as a function return declaration; no separate forced-return-conversion rule is specified. Keep this as a type compatibility check and do not invent implicit return coercions. Explicit return casts remain the source-level conversion path. Raw-pointer dereference targets have no hard-type/provenance field in the current contract, so no implicit Hard-Lock rule is invented there. Ordinary array element types likewise do not inherit the container's hard flag; the explicit packed `Nbit`/`Nbyte` indexing contract is handled separately.

#### SEM-004 closure matrix

| Area | Status | Evidence / boundary |
|---|---|---|
| Numeric promotion and generic inference | Covered | Existing positive/negative Semantic regressions; included in cross-platform CI |
| Cross-family numeric operands | Verified conservative rejection | `i32 + f64`, `f32 + d32`, and other cross-family mixes are rejected; explicit cast is required |
| Hard bit/byte initializers, assignments, fields and packed indexes | Implemented and CI-verified | Constant range, safe widening, runtime narrowing and SemanticResult/OIR records |
| Hard signed/unsigned integer conversions | Implemented and CI-verified | Signedness, width, constant range and assignment regressions |
| Hard float/decimal widening | Implemented and CI-verified | Same-family widening allowed; narrowing requires explicit cast |
| Explicit casts and conversion metadata | Implemented and CI-verified | Explicit flag, identity/signedness classification and OIR artifact round-trip |
| Wide integer literal evaluation | DEFERRED | Avoid broad Semantic refactor; later portable 128/256-bit utility library |
| Runtime arithmetic overflow | Deferred by language contract | No wrapping/saturation/trap behavior is specified for Semantic to enforce |
- Explicit numeric casts now produce SemanticResult conversion records with source/target type IDs, conversion classification and the explicit flag. A dedicated SemanticResult → OIR artifact round-trip test passed all five CI jobs in [44740741699](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44740741699). Identity classification now compares the underlying SemanticResult type shape while ignoring only the Hard-Lock flag; tests cover `u8`→`i64` signedness and `u8`→`u8` identity. The fix passed all five CI jobs in [44742242545](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44742242545). The cleaned regression run [44742540343](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44742540343) has Windows, Ubuntu, macOS and ASan/UBSan green; Valgrind is still running.
- Wide integer literal evaluation is explicitly deferred to a future portable 128/256-bit utility library. Runtime arithmetic overflow is deliberately left undefined by the current language contract; no overflow behavior is invented here. SEM-004 closure is gated on final CI for the cross-family rejection regression.

### SEM-004 audit finding — mixed integer/float promotion (decision pending)

- The Semantic promotion helper currently accepts integer/integer combinations and same-family float/decimal widening, but rejects mixed integer/float operands (e.g. `i32 + f64`).
- Arithmetic regression tests currently assert that mixed integer/float operands fail. The older syntax example implied that `i * 2.5` produces a floating result, so the documentation was clarified to avoid promising behavior that the implementation does not support.
- **No language behavior changed in this audit step.** Before implementation, resolve whether mixed numeric families should promote (and define decimal-vs-binary float interactions). Once decided, update promotion helper, conversion metadata, diagnostics and tests together. This remains an SEM-004 closure blocker.


### SEM-005 — Shared dereference reborrow checkpoint (2026-11-22)

- Confirmed and retained the supported form `v:s!&i32 = &*r;` when `r` is a tracked shared safe reference. The reborrow preserves every known owner/projection origin rather than treating the reference variable as storage.
- Expanded regression cases for chained shared reborrows, shared-reference parameters, referent type mismatch, raw-pointer-to-safe-reference rejection, and conflicts with a later overlapping mutable borrow.
- Array-element borrow tests now use direct lvalues such as `&items[3]`; they no longer pretend that an array reference is needed to borrow one element. Four-element fixtures explicitly use index 3.
- Normative syntax reference now states the supported `&*r` boundary and the array-element borrow distinction.
- **Known limitation:** mutable-parent reborrow is not implemented. `&*r` from `&mut T` is currently rejected because correct support requires suspending the exclusive parent while child borrows are alive and resuming it on child release. Do not relax the rejection without implementing and testing that state transition. Raw-pointer dereference cannot be promoted to a safe reference.
- Test expansion commits: `1e8e5caa392b492362922db179c72b6fa5eb574d` and `41599377f39a1c08c888089e1bd6b5fb187609e5` (adds uninitialized-source and branch-merged-origin cases). Normative documentation commit: `593f87fd20232c896db0f4ba7dc931c046d980b6`. The previous array-element test/documentation CI runs passed; CI evidence for the latest `&*r` test expansion is still pending and this checkpoint is not a closure claim.

### SEM-005 — Inline aggregate-array borrow projection regression (2026-11-23)

- Extended `check_borrow_aggregate_provenance_semantics()` with four cases for inline fixed arrays stored in aggregate fields: disjoint constant-index mutable borrows are independent; same-index mutable borrows conflict; a shared borrow of one element permits a write to a distinct element; dynamic indexes conservatively conflict.
- Test commit: `b9d4e11e6e8277ddcce639113c5ebe9c4822a71f`.
- Exact-head CI has been triggered; the new tests are not considered passing until all platform, sanitizer and Valgrind jobs complete successfully. If a case fails, investigate whether the issue is array member type projection, lvalue provenance path construction, or conservative overlap handling before changing the implementation.


### SEM-005 — Nested array/struct borrow projection tests (2026-11-23)

- Added three nested projection cases for arrays of nominal structs: distinct fields of the same element are independent; overlapping same-field borrows are rejected; the same field at different constant element indices is independent.
- Added two conservative dynamic-index cases: dynamic/dynamic and dynamic/constant mutable borrows of the same nested field must reject.
- Nested constant-projection tests commit: `0062a93832194ca7160a8780adbb43cf544dcf4c`; exact-head CI passed all five jobs (Linux, macOS, Windows, ASan/UBSan and Valgrind).
- Dynamic nested projection tests commit: `7f6b119a60df8554df041fd9a50b9dca72c23659`; exact-head CI run `45256718023` passed all five jobs.
- These are focused SEM-005 regression checkpoints, not a completion claim.
### SEM-005 — Pointer and reference safety
#### SEM-005 allocation identity checkpoint (implementation in progress)

- The active-allocation registry is the sole source of truth for allocation liveness. Each successful area()/zone() expression receives a monotonically increasing identity independent of its numeric address.
- Slot.allocation_identity carries provenance through direct handle aliases. free() removes the identity from the active registry; every alias with that identity then fails the identifier liveness check.
- Reusing a numeric address does not reuse an identity. A later allocation gets a fresh identity and does not revive older aliases.
- ?; and }?; currently clear the active registry when recording Halt. This is an initial conservative implementation; execution-context ownership and nested function isolation still require review before this behavior can be accepted.
- Focused regression sources cover alias use after free, fresh allocation after free, and the continued invalidity of an old alias after a new allocation.
- Not verified yet: pointer/address-derived aliases (&, addr, dereference, member/index projections), double-free diagnostic cardinality, branch/loop joins, scope/function ownership, and exact-head CI. Keep SEM-005 IN_PROGRESS.

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


### SEM-003 primitive type matrix — 2026-11-20

- Cross-checked the C Semantic primitive registry against the normative syntax reference and tests: signed/unsigned integer widths through 128-bit, `f32/f64/f128`, `d32/d64/d128`, `bit`, `byte`, fixed `Nbit/Nbyte` representations, `char`, `bool` and `str`.
- The syntax reference previously listed `hex` as a storage type and described 128-bit types as unsupported. The C compiler accepts hexadecimal numeric literals using `0x`; `hex` itself is not a storage type. The reference now reflects that and records the implemented 128-bit types.
- Added Semantic regressions confirming `char` acceptance and rejecting `hex` as an unknown storage type. Existing tests cover decimal widths, 128-bit arithmetic/bitwise constraints and fixed bit/byte widths.
- Exact test commit `9933c4d7a17f23e2fc8772ecc9aa330dc0d6cfbf`; CI [44658543349](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44658543349) passed Windows, Ubuntu, macOS, ASan/UBSan and Valgrind; 74/74 CTests passed and Valgrind reported zero errors/zero lost bytes.
- **SEM-003 remains IN_PROGRESS.** Remaining audit: nested type expressions, NewType chain identity/base metadata and malformed type-expression payloads.


### SEM-003 NewType identity and nested type-variable update (2026-11-20)

- Added NewType chain identity regressions: assignments within the same chained NewType are accepted, while assignments between the base NewType and its derived NewType are rejected in both directions.
- Existing recursive type-variable tests cover `*t`, `&t`, repeated type-variable unification and nested callback parameter/return signatures. The normative reference now reflects these supported forms; generic aggregate element types remain outside the contract.
- Exact test commit `7484b6323f0c74843874c2d492c1a05774165a42`; CI [44658892345](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44658892345) passed Windows, Ubuntu, macOS, ASan/UBSan and Valgrind; 74/74 CTests passed, Valgrind zero errors/zero lost bytes.
- **SEM-003 remains IN_PROGRESS.** Remaining work is to assert persistent SemanticResult NewType `base_type_id`/symbol metadata directly and finish malformed nested type-expression coverage.


### SEM-003 malformed type-expression payloads and kind-specific flags (2026-11-20)

- Added direct malformed-AST Semantic regressions for missing pointer/reference targets, non-type child nodes, missing function return types, invalid qualifier combinations, and invalid flag bits on named, pointer, reference, array and function type expressions.
- Semantic now validates AST type-expression flags against the node kind: `AST_TYPE_REFERENCE` permits only `AST_TYPE_FLAG_MUTABLE_REFERENCE`; `AST_TYPE_QUALIFIED` permits only `AST_TYPE_FLAG_HARD_TYPED`; all other type-expression kinds reject nonzero flags. This preserves parsed `&mut T` while preventing malformed AST metadata from bypassing the type contract.
- Added a positive direct-AST case for mutable reference flags and retained source-level mutable-reference tests.
- Exact code commit `c4a6663c2a94cf6b291670c082554856b43fde9c`; exact final fixture/test commit `bd588a5fb673a2981713d3005927133ddcd38ece`.
- CI [44674844504](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44674844504) passed all five jobs (Windows, Ubuntu, macOS, ASan/UBSan, Valgrind). Each platform passed 74/74 CTests; Valgrind reported zero errors and zero definitely/indirectly/possibly lost bytes.
- **SEM-003 remains IN_PROGRESS.** The malformed AST and kind-specific flag checks are now verified; remaining closure audit is to finish type spelling/type-result mapping and inspect the NewType target forms/nominal metadata against the normative reference. Do not advance to SEM-004 until this package is formally closed.

#### SEM-003 failure/retest record

| Failure ID | Failed evidence | Finding | Fix | Retest |
|---|---|---|---|---|
| SEM-003-F04 | Run `44674721333`, Semantic CTest 69 | A malformed reference AST fixture set the hard-typed flag while testing a non-type child; after kind-specific flag validation, this correctly emitted an additional flag diagnostic and invalidated the fixture's one-error expectation | Correct fixture flags to zero so it isolates the invalid child-node case (`bd588a5`) | Run `44674844504`: 74/74 CTests on Windows/Ubuntu/macOS; all five CI jobs green; Valgrind clean |


### SEM-003 NewType structural base metadata — verified test evidence (2026-11-20)

- Extended the persistent SemanticResult test beyond primitive-to-NewType chains. It now verifies that NewTypes targeting raw pointers, safe references and callback signatures each retain a nonzero structural `base_type_id` that resolves to the corresponding pointer/reference/function type record.
- The same test confirms each NewType has its own nonzero referenced symbol identity; a chained NewType points to the immediately preceding nominal NewType rather than flattening its identity to the primitive.
- Exact code/test head: `803c22f8a92ce01259515fe7942ff7baaefecedc`.
- CI [44675153773](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44675153773) passed all five jobs (Windows, Ubuntu, macOS, ASan/UBSan and Valgrind); 74/74 CTests on all three platforms; Valgrind zero errors and zero lost bytes.
- Cross-check of `is_type_defined` and the SemanticResult primitive registry found matching exact-name sets for supported scalar, fixed-width, vector, map and built-in types. Both use exact comparisons for primitive names; the only prefix checks are intentional structured forms (`array|` and `f:(`), not partial primitive matching.
- **SEM-003 remains IN_PROGRESS** until the final acceptance checklist and normative-reference comparison are recorded as closed. No OIR work is authorized before SEM-001–SEM-014 closure.


### SEM-003 encoded array type-result mapping (2026-11-20)

- Found that internal array spellings (`array|Element[dimensions]`) were accepted by type validation but fell through the SemanticResult type classifier as unknown types. The classifier now recognizes both the generic `array` form and encoded `array|` forms as `SEMANTIC_RESULT_TYPE_AGGREGATE`.
- Added a direct AST regression for `array|i32[3]`, verifying the emitted SemanticResult type record is aggregate rather than unknown.
- Exact final code/test head: `1220aa71934aa1ead091b5e141e43f0ad4e16c13`.
- CI [44675632586](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44675632586) passed all five jobs (Windows, Ubuntu, macOS, ASan/UBSan, Valgrind); 74/74 CTests on all three platforms; Valgrind zero errors and zero definitely/indirectly/possibly lost bytes.

#### SEM-003 failure/retest record

| Failure ID | Failed evidence | Finding | Fix | Retest |
|---|---|---|---|---|
| SEM-003-F05 | Run `44675522890`, ASan/UBSan and Valgrind build | The first array-classifier patch called `semantic_is_array_type_name` before its static declaration, producing an implicit-declaration/static-redeclaration build error | Use the exact local `array` / `array|` spelling checks at the classifier site, avoiding a forward dependency (`1220aa7`) | Run `44675632586`: all five CI jobs green, 74/74 platform CTests, Valgrind clean |


### SEM-003 closure — primitive, named and composite type system (2026-11-20)

- Completed the full supported primitive spelling matrix in a source-level Semantic regression: signed/unsigned integer widths, float and decimal widths, bit/byte representations, `char`, `bool`, `str`, vectors, maps and `any`. `void` is covered through function return signatures rather than as a storage declaration.
- Verified nested raw-pointer, safe-reference and callback/NewType target forms; fixed-size and dynamic arrays; tuple and callback use; nominal NewType assignment and chain identity; self-recursive NewType rejection; by-value aggregate recursion rejection; malformed recursive type-expression AST rejection.
- Persistent SemanticResult tests assert nominal symbol identity and primitive/pointer/reference/function base-type links. Encoded `array|...` spellings now produce aggregate result types.
- Cross-checked primitive lookup and SemanticResult primitive registration. Primitive names use exact comparisons; only intentional structured encodings use prefix recognition (`array|`, `f:(`).
- Normative syntax reference was corrected for supported 128-bit types and hexadecimal literal notation; type-variable nesting documentation was aligned with implementation.
- Final verified code/test head: `977a1d42e07c7674b2e4d318cb412e7efadc1d6a`; docs are updated in subsequent commits on the same branch.
- CI [44675909159](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44675909159) passed Windows, Ubuntu, macOS, ASan/UBSan and Valgrind. All three platform suites passed 74/74 CTests; Valgrind reported zero errors and zero lost bytes.
- **SEM-003: VERIFIED.** Next work package: **SEM-004 — Type inference, conversion, promotion and forced types**. OIR remains gated until SEM-001 through SEM-014 are verified or explicitly deferred by approved design rationale.


### SEM-003 final metadata test reinforcement (2026-11-20)

- Extended the direct SemanticResult metadata test to assert the structural base IDs for primitive NewType, chained NewType, raw-pointer NewType, safe-reference NewType, and callback NewType; also asserts distinct nominal symbol IDs.
- Exact test commit: `69ecb4a4a96b73f64925cff0e16dd922e7c6a557`.
- CI run [44659320679](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44659320679) completed successfully: Windows, Ubuntu, macOS, ASan/UBSan and Valgrind all green; 74/74 CTests per platform; Valgrind zero errors and zero definitely-lost bytes.
- SEM-003's prior closure remains **VERIFIED**; this is additional direct metadata evidence, not a scope reopening.


### SEM-004 conversion and inference audit — coverage inventory (2026-11-20)

- Rechecked `test_semantic_arithmetic.c`: 145 declared fixtures. Existing coverage includes signed/unsigned integer promotion, mixed-sign promotion, float/decimal width promotion, explicit widening/narrowing casts, invalid casts, bit/byte width mismatches, all declared shift-width boundaries, and vector arithmetic. These are existing tests; this audit does not claim new implementation.
- Rechecked `test_semantic.c`: generic identity inference, inference from a later parameter, named-argument inference, same-type generic arithmetic, and conflicting generic arithmetic are already exercised with positive/negative cases.
- Located the active language syntax specification at `docs/language/syntax-reference.md` (the requested legacy filename `NEXUS-FLOW-SYNTAX-REFERENCE.md` is not present). Its Hard-Lock contract states that `!Type` is an explicit representation constraint: constant values must fit, safe unsigned widening is allowed, unproven runtime narrowing is rejected, and successful conversions are recorded in SemanticResult/OIR metadata. `!ReturnType` is a function return-type declaration, not an implicit cast instruction; return-expression conversion remains governed by ordinary compatibility until the language contract explicitly says otherwise.
- Audit found that `ASTVarDecl.is_hard_typed` was retained for aggregate-field SemanticResult relations but discarded when fields were copied into `FieldInfo`. Consequently, struct-literal initialization and member assignment could not enforce the documented Hard-Lock rule. Added `FieldInfo.is_hard_typed`, applied the same constant-range/runtime-narrowing policy to struct-literal field initializers and member writes, and emitted explicit conversion records for successful cases.
- Added regression coverage for valid/invalid constant field initialization, safe unsigned field assignment, out-of-range member assignment, and rejected runtime narrowing. Extended SemanticResult integration coverage to verify the hard bit/byte identifier-assignment conversion record and its OIR artifact round-trip.
- Intermediate failure record: CI run [44736533178](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44736533178) exposed that the first member-write test also depended on hard-typed struct-literal initialization. The missing initializer path was implemented in commit `3dfa882`; the final regression suite is running under [44736707950](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44736707950). Ubuntu, macOS, Windows and ASan/UBSan have passed so far; Valgrind is still running.
- Index and raw-pointer dereference targets do not carry an equivalent hard-type field contract in the current AST/semantic model. Their behavior remains explicitly deferred rather than receiving guessed semantics. SEM-004 is closed; these are documented scope boundaries.


### SEM-004 closure — final audit and policy (2026-11-21)

- Confirmed CI run [44923744347](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44923744347) passed all five jobs: Windows, Ubuntu, macOS, ASan/UBSan, and Valgrind.
- The new `f32 + d32` negative regression is green. Existing arithmetic regressions cover mixed integer/float rejection (`i32 + f64`), integer promotion, same-family float and decimal widening, forced-type constant range/runtime narrowing, explicit casts, shift bounds, bit/byte width checks, and vector operators.
- Inference/unification tests cover generic identity, inference from later parameters, named-argument inference, same-type generic arithmetic, and conflicting generic arguments. Forced-type conversion metadata is covered by SemanticResult and OIR artifact round-trip tests.
- Closed the unresolved policy conservatively: **implicit promotion is limited to existing integer promotion and same-family float/decimal widening. All cross-family numeric combinations are rejected; explicit `cast()` is required.** Normative syntax reference updated to match.
- Explicitly deferred (not marked supported): 128/256-bit literal evaluation beyond existing signed-64 evaluator boundary; runtime overflow policy; implicit return conversion; Hard-Lock propagation to indexed/raw-pointer targets lacking AST contract fields. These are scope boundaries, not silent acceptance.
- **SEM-004: VERIFIED.** No OIR advancement: SEM-005 onward remains the active Semantic roadmap.


### SEM-005 — Initial audit checkpoint (2026-11-21)

- Confirmed SEM-004 documentation CI runs [44925443536](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44925443536) and [44925439525](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44925439525) completed successfully (all five platform/sanitizer jobs).
- Reviewed pointer/reference type compatibility, dereference and assignment checks, safe-reference erasure boundaries, reference-return origin validation, borrow exclusivity, field/index overlap rules, call temporary borrows, diagnostic locations, scope cleanup, reassignment owner tracking, and aggregate-storage rejection for safe references.
- Found an explicit coverage gap in loop definite-initialization: the loop implementation snapshots incoming reference/pointer initialization state and conservatively merges it with the loop body state (zero-or-more iterations), but no focused regression asserted both outcomes.
- Added two tests to `omnicore-sdk/src/c/tests/test_semantic.c`: (1) initializing a safe reference only inside a loop must not count as definite initialization after the loop; (2) an already initialized reference remains initialized after a loop that does not rebind it.
- Test commit: `fcbee19a92e3a36ebc0710ca5be6b9c4b48a1057`. SEM-005 remains **AUDIT_REQUIRED** pending CI and remaining independent coverage checks.

## Multi-agent audit log — SEM-005 mutable reference reassignment

- **Audit focus:** borrow-binding reassignment state consistency.
- **Architect pass:** preserve old binding on invalid reassignment; release old borrow only after the new borrow has passed conflict/provenance validation; confirm old and new owner tracking.
- **Implementer/source review:** `semantic_register_borrow_binding()` validates conflict conditions before releasing the target's previous borrow, then records the new owner/path. `semantic_copy_shared_reference_binding()` similarly validates before replacing the binding.
- **Test review:** existing regressions verify mutable reassignment releases the previous owner and shared reassignment tracks the new owner. A missing complementary assertion was identified: mutable reassignment must keep the new owner exclusively borrowed.
- **Regression added:** `mutable_reassignment_tracks_new_owner` in `omnicore-sdk/src/c/tests/test_semantic.c` expects a second mutable borrow of the new owner to be rejected.
- **Independent review status:** source ordering looks consistent with the intended atomic replacement rule; test syntax/runtime has not been independently executed in this environment.
- **Verification status:** **PENDING**. Commit `65124a7b247630bdb0109cdb2ababa746cd74625` has not yet been confirmed by exact-head CI. SEM-005 remains `AUDIT_REQUIRED`; no status is advanced.
- **Tooling note:** GitHub code search returned no matches for the targeted phrases; direct source fetch and repository directory listing were used instead. No test execution environment was available in this pass.

### SEM-005 — failed mutable-reference reassignment regression

- **Test Designer finding:** the existing failed-reassignment preservation test exercised only shared `&T`; mutable `&mut T` needed a direct equivalent.
- **Regression added:** `failed_mutable_reassignment_preserves_old_borrow` attempts an invalid reassignment from a plain value, then attempts a second exclusive borrow of the original owner. The test expects both the invalid assignment diagnostic and the retained-borrow conflict.
- **Implementation review:** the assignment path calls `semantic_register_borrow_binding()` only when the assignment has produced no new diagnostics. That helper validates owner, lifetime, path and conflicts before calling `semantic_release_borrow_binding()`. The intended atomic replacement ordering is therefore present; the new test guards it for mutable references.
- **Commit:** `d3ef70d67dd86624a70b7131555e60e91860c278`.
- **Independent execution / exact-head CI:** pending. This test has been committed but not run in a local build or confirmed by CI; SEM-005 remains open.

### SEM-005 — conflicting mutable reassignment preserves prior borrow

- **Independent reviewer finding:** type-invalid reassignment alone does not exercise the conflict path where the new borrow expression itself is rejected because another binding already borrows the proposed owner.
- **Regression added:** `conflicting_mutable_reassignment_preserves_old_borrow` holds a shared borrow on the proposed new owner, attempts to rebind an existing exclusive reference to it, then verifies that the original owner remains exclusively borrowed.
- **Expected diagnostics:** 2 — one for the conflicting reassignment and one for the subsequent conflicting borrow of the original owner.
- **Commit:** `84256bae0a96a9e102d272ab03eed1f2f024d330`.
- **Execution and exact-head CI:** pending; test has not been run locally or confirmed green by CI.

### SEM-005 — reassignment regression review checkpoint (2026-11-21)

- **Controller:** kept the task within SEM-005; no OIR/Verifier work was started.
- **Implementer:** added two targeted negative regressions: invalid-type mutable reassignment preserves the previous exclusive borrow; conflict-rejected mutable reassignment also preserves the previous exclusive borrow.
- **Test Designer:** both tests expect two diagnostics, covering the rejected reassignment and the later conflicting borrow against the original owner.
- **Independent Reviewer:** verified both test declarations are included in `check_reference_reassignment_and_lifetime_semantics()`, and that this test group is referenced by the test runner (two source occurrences: definition + invocation). Reviewed the assignment path and borrow registration ordering: the new binding is validated before the old binding is released.
- **Build/CI Verifier:** not green / not available. Exact-head workflow lookup returned no workflow runs and combined status returned no status checks for the test commits. The available workflow-run wrapper is PR-filtered; no local build/runtime execution was performed in this pass.
- **Current disposition:** **IN_PROGRESS / VERIFICATION_PENDING**. The new tests are committed but not proven passing. SEM-005 remains `AUDIT_REQUIRED`.
- **Current branch head at this checkpoint:** `65cdab6b6c717a7f6db8af3283dc56b1f0aecf85` (documentation commit following test commit `84256bae0a96a9e102d272ab03eed1f2f024d330`).


### SEM-005 — conditional mutable-borrow merge regression (2026-11-21)

- **Architect/source audit:** `SemanticReferenceInitSnapshot` captures initialization/nullability/callback-target state only; it does not capture `Slot.borrow_kind`, `borrowed_from`, or `borrow_path`. `AST_IF` restores/merges this snapshot between branches, while reference reassignment mutates borrow bindings immediately.
- **Risk identified:** when an outer `&mut` binding is reassigned to different owners in the `if` and `else` branches, sequential semantic traversal can leave only the final branch's owner recorded. A later exclusive borrow of the other possible owner may then be incorrectly accepted.
- **Regression added:** `branch_reassignment_preserves_all_possible_mutable_borrows` rebinds one outer mutable reference to `second` or `third` on mutually exclusive branches, then attempts exclusive borrows of both owners. The test expects both post-merge borrows to be rejected.
- **Test commit:** `d931458590f20e99b199a589b47ed4efbbd2ce9c`.
- **Implementation disposition:** no speculative semantic change was made. The current single-owner `Slot` borrow representation may be insufficient for a conservative may-own merge; implementation must first choose a sound representation (e.g. multi-origin borrow set or fail-closed unknown-origin state) and cover branch termination, nested branches, loops, and cleanup.
- **Verification:** pending. No local C test execution or exact-head CI result is available for this commit. SEM-005 remains **IN_PROGRESS / VERIFICATION_PENDING**; do not mark this regression green or advance to SEM-006.

- **Additional regression:** `conditional_mutable_reassignment_keeps_incoming_and_new_borrows` covers an `if` without `els`: both the incoming owner (false path) and reassigned owner (true path) must remain exclusively borrowed after the join. Expected diagnostics: 2.
- **Control-flow audit extension:** `AST_SELECT` and `AST_SCAN` also use the same initialization-only snapshot abstraction; loop analysis merges initialization state against the zero-iteration path. Borrow-origin merging therefore needs a shared control-flow state abstraction rather than an `AST_IF`-only patch. Preserve branch termination semantics and scope cleanup when implementing it.


### SEM-005 — borrow-state/control-flow representation audit (2026-11-21)

- **Loop review:** `AST_LOOP` captures the incoming reference-init snapshot, analyzes the body once, and merges the post-body init state with the incoming state to model zero-or-more iterations. It does not snapshot or merge borrow ownership. The loop handler also analyzes the increment expression before the body; this ordering is recorded for a separate loop-semantics review and is not changed as part of this borrow fix.
- **Alternative-path review:** `AST_SELECT` and `AST_SCAN` restore the same incoming init snapshot before each arm and merge arm snapshots, adding the incoming state when no default/else arm exists. They likewise leave borrow bindings mutated by the last analyzed arm.
- **Branch-exit review:** `AST_IF` uses `semantic_branch_terminates_flow()` to select the surviving init state when one branch terminates. A borrow-state merge must follow the same reachable-branch selection; blindly unioning terminating paths would retain borrows that are not live after the join.
- **Borrow representation review:** conflict checks and shared-path counting scan active `Slot` bindings by `borrowed_from`, `borrow_kind`, and `borrow_path`. The owner-side `shared_borrow_count` and `mutable_borrow_active` fields are updated on registration/release but are not read by the current conflict checks. This makes a borrower-owned set of possible origin/path/kind tuples a viable design candidate, but all registration, copy/reborrow, lifetime, release, scope-exit, and snapshot paths must be migrated together.
- **Design constraint:** branch merge is a *may-borrow* union over reachable paths, while initialization remains a *must-initialize* intersection. Each borrow alternative must retain its own owner, projection path, and mutability. Unknown provenance must fail closed. Snapshot restore must replace the complete borrow set without double-adjusting owner counters; scope cleanup must release every origin exactly once.
- **Implementation decision:** no source patch in this audit. Before implementation, define a dedicated borrow-origin entry type and a unified flow snapshot that owns deep copies of borrow alternatives plus the existing initialization/callback state. Add tests for if/elsif/else termination, nested if, select/scan with and without default/else, loop zero-iteration and repeated-iteration conservatism, and scope cleanup. Keep SEM-005 **IN_PROGRESS / VERIFICATION_PENDING** until build/runtime and exact-head CI evidence exists.


### SEM-005 — borrow-origin storage and cleanup design (2026-11-21)

- **Authoritative ownership model:** replace the single `Slot.borrowed_from / borrow_kind / borrow_path` binding with a borrower-owned linked set of borrow-origin records. Each record contains a non-owning owner `Slot*`, an owned projection-path string, and the borrow kind (shared/exclusive). Keep the parameter/global `borrow_origin` lifetime anchor distinct from local storage-owner alternatives; it is not a local active borrow.
- **Why borrower-owned:** conflict checks already discover active borrows by scanning borrower Slots. The existing owner counters are updated during registration/release but are not consulted by the conflict predicates. A borrower-owned set therefore matches the current conflict-query direction and avoids trying to make owner counters represent mutually exclusive branch paths.
- **Record identity and merge:** two records are equivalent only when owner pointer, borrow kind, and normalized projection path all match. Merge is a deduplicating union over reachable control-flow paths. Different paths or owners remain distinct alternatives. If a source cannot be represented without losing provenance, reject/fail closed rather than silently dropping an alternative.
- **Atomic replacement:** build and validate a complete replacement set first. On allocation or validation failure, preserve the target's prior set unchanged. Only after successful construction release the old set and install the new one. This extends the already established validate-before-release reassignment invariant to multi-origin state.
- **Snapshot ownership:** the unified flow snapshot deep-copies every borrow-origin record and path string alongside initialization/nullability/callback state. Snapshot capture must clean up partially allocated copies on failure. Restore first releases the current binding set, then installs a deep copy of the snapshot; it must not share mutable linked-list nodes with the live Slots or another snapshot.
- **Merge rules:** initialization is a must-merge (AND) over continuing paths; nullable state is a may-merge (OR); callback identity is retained only when equal. Borrow alternatives are a may-merge (deduplicating OR) over continuing paths. If one branch terminates, exclude that branch and retain only the reachable branch state, matching the existing `semantic_branch_terminates_flow()` behavior.
- **Counters:** do not update legacy owner counters per snapshot clone/restore because that would double-count alternatives and branch copies. Before removing the fields, audit all readers and public/internal ABI constraints. Until then, treat them as derived/legacy metadata and ensure no correctness decision depends on them.
- **Scope and destruction:** preserve the current two-pass cleanup invariant: release every borrow record for all Slots at the exiting depth before freeing any Slot; on `semantic_destroy`, release all cross-Slot records before destroying any Slot. Each record owns only its path string and record allocation; owner pointers are non-owning and must never be dereferenced after scope teardown.
- **Registration/copy/reborrow migration:** migrate direct borrow initialization, assignment/reassignment, shared-reference copy, returned-reference propagation, source reborrow, conflict diagnostics, path-count checks, stable-return-origin checks, scope exit, and whole-context destruction together. A partial migration that leaves a single-owner consumer is not acceptable.
- **Implementation order:** (1) add the record type and Slot-owned set with allocation-failure tests; (2) migrate release and conflict/path scans; (3) migrate registration/copy/reborrow with atomic replacement; (4) extend the existing flow snapshot to deep capture/restore/merge; (5) apply the same snapshot to IF/SELECT/SCAN/LOOP; (6) add branch-termination, nested-branch, missing-default, zero-iteration, and scope-cleanup regressions; (7) run C build, semantic tests, and exact-head CI.
- **Current disposition:** design checkpoint only; no source implementation has been made. Existing branch regressions remain pending execution. SEM-005 remains **IN_PROGRESS / VERIFICATION_PENDING**.


### SEM-005 — shared dereference reborrow implementation checkpoint (2026-11-22)

- Implemented a conservative shared reborrow path for `&*r`: when `r` is a tracked shared-reference binding, the new binding copies the complete tracked origin set rather than treating the dereference as an unrelated computed lvalue.
- Added regressions for: valid shared `&*r`, preserving the owner conflict after reborrow (mutable borrow rejected), and rejecting shared reborrow from a mutable reference.
- Mutable reborrow parent suspension/resumption, projection reborrow (member/index), and unknown/raw-pointer origins remain unsupported pending separate provenance/lifetime work.
- Code/test commits: `eb9f368`, `f7f2594`, `5cb5d2b`, with validator correction `6271af8` and final narrow validation fix `aab4212`. The first CI run exposed that unary borrow validation rejected the dereference syntax before provenance checks; the validator now accepts the syntax shape and the registration stage applies tracked-origin/type restrictions. All five CI jobs for `aab4212` passed (Ubuntu, Windows, macOS, ASan/UBSan, Valgrind). Added chained shared-reborrow positive and owner-conflict regressions in `b14586b`; its CI is pending. This is not a SEM-005 closure.


### SEM-005 — multi-origin borrow flow implementation (2026-11-21)

- Added borrower-owned `BorrowOrigin` records to `Slot`, retaining the scalar fields only as a compatibility view of the first record during migration.
- Migrated borrow release and conflict/path scans to enumerate origin sets. Shared-path counting counts a borrower once per queried path even if its set contains multiple alternatives.
- Direct borrow registration now allocates the replacement record before releasing the previous binding. Shared-reference assignment clones every shared origin, validates the complete replacement, and preserves the previous binding on failure. Mutable-reference copying remains rejected.
- Extended the existing reference-init snapshot to deep-copy borrow-origin records and path strings. Branch/arm/loop merges now union and deduplicate origin tuples by owner, kind, and path; initialization remains AND, nullability remains OR, and callback identity remains equality-based.
- Snapshot restore allocates all replacement sets before mutating live Slots, then releases and restores the complete set. Scope cleanup still releases all borrower links before freeing any Slot.
- Updated IF join handling to restore the merged snapshot after both continuing branches. Without that restore, the merged borrow set existed only in the snapshot and the live Slot retained the last analyzed branch.
- Borrow diagnostics now identify the exact matching origin/path. Returned-reference stability checks inspect every possible origin. Returned-call propagation preserves multi-origin shared sets; sets containing an exclusive origin fail closed rather than collapsing provenance.
- Regression coverage now includes IF with/without ELSE, SELECT, LOOP zero/one iteration, terminating branches, and returned shared references: `branch_reassignment_preserves_all_possible_mutable_borrows`, `conditional_mutable_reassignment_keeps_incoming_and_new_borrows`, `select_reassignment_preserves_all_possible_mutable_borrows`, `loop_reassignment_preserves_zero_and_one_iteration_borrows`, `terminating_branch_does_not_leak_unreachable_borrow`, and `returned_shared_reference_preserves_branch_origins`.
- Exact implementation/test head `f550a1b9130374ab7b4ec68839e2a50cd32e2fd4` passed all five CI jobs in run [44944622134](https://github.com/benimprojem/OMNI-CORE-v2-NEXUS-FLOW/actions/runs/44944622134): Ubuntu, Windows, macOS, ASan/UBSan, and Valgrind.
- **Remaining migration:** source reborrow/projection composition and a dedicated SCAN origin-set regression remain open. SEM-005 remains **IN_PROGRESS / VERIFICATION_PENDING**.


### SEM-005 — multi-origin verification update (2026-11-22)

- Re-polled the exact implementation/test head `f550a1b9130374ab7b4ec68839e2a50cd32e2fd4`; all five check-runs completed with `success` (Ubuntu, Windows, macOS, ASan/UBSan, Valgrind).
- The six focused multi-origin regressions listed above are part of the test commit. This verifies the implemented IF/SELECT/LOOP/termination/returned-shared paths on the tested CI matrix.
- This does not close SEM-005: source reborrow/projection composition and a dedicated SCAN origin-set regression are still outstanding. Do not advance to SEM-006 or OIR until the ordered Semantic gate is satisfied.


### SEM-005 — shared-reference projection reborrow checkpoint (2026-11-22)

- Added reference-target aggregate resolution for safe member reads (r.field when r is a safe reference to a struct/union). Unknown members retain a precise semantic diagnostic.
- Implemented shared projection reborrow composition for &r.field and &r[index]: the projection suffix is appended to every tracked source BorrowOrigin path. The reference binding itself is never recorded as the storage owner.
- Projection-origin replacement is atomic: all owner/path records are allocated and conflict-checked before the previous borrower binding is released. Union fields retain the existing wildcard projection semantics; dynamic indexes retain conservative wildcard paths.
- Mutable projection reborrows are deliberately rejected with an explicit diagnostic until parent-child suspension/resumption and lifetime restoration are implemented. Unknown/external origins and mutable source references are also rejected.
- Added regressions for valid safe-reference member access, unknown member rejection, overlapping mutable borrow rejection after a shared projection reborrow, and a disjoint-field case where the original shared reference covers only a nested aggregate.
- Commits: aca3b87 (safe-reference aggregate member resolution), 0882358 (member-resolution tests), 4ce93ae (multi-origin shared projection composition), ea1153b (projection path regressions), 54f6c53 (explicit unsupported mutable-projection diagnostic), and 8da1714 (correct disjoint-owner test setup).
- CI for aca3b87 and 0882358 passed all five jobs. The projection implementation/test run 44962201042 and test run 44962213757 both passed all five jobs. Follow-up diagnostic commit 54f6c53 also passed Ubuntu, Windows, macOS, and ASan/UBSan; its Valgrind job was still running at this checkpoint. Exact test head 8da1714 subsequently passed all five jobs (Ubuntu, Windows, macOS, ASan/UBSan, Valgrind).
- SCAN multi-origin regression is covered by commit 70d01b5 and its five-job CI pass; this item is no longer outstanding.
- The follow-up parent-reassignment regression commit 8c7009e passed all five CI jobs, verifying that a child projection keeps the original owner path after its source reference is rebound. Added branch-join projection regressions in bf6825c; its five-job CI passed. The latest branch documentation head 117ff89 also passed all five CI jobs (Ubuntu, Windows, macOS, ASan/UBSan, Valgrind). Still open: array-index projection test coverage; mutable parent-child suspension/resumption; parameter/external-origin projection policy; lifetime/escape/initialization cross-audit; independent review and exact latest-head CI. SEM-005 remains IN_PROGRESS / VERIFICATION_PENDING.


### SEM-005 — safe-reference array-index projection and test gate repair (2026-11-22)

- Added referent type unwrapping for indexing through safe shared/mutable reference types in expression type resolution. The array/vector index result type now derives from the referent, rather than defaulting to `any` when the indexed expression is a safe reference. Raw-pointer type behavior is intentionally unchanged.
- Added projection regressions for a fixed array: disjoint constant index, overlapping constant index, and dynamic index (conservative overlap).
- **Test runner repair:** the existing combined test gate incorrectly used `&&` with a negated first test, causing the member-projection suite to be skipped when the preceding shared-reborrow test succeeded. Changed it to an `||` gate so both suites run and either failure reports. This is a test-infrastructure defect, not a semantic behavior change.
- Implementation commit: `f384dc3c478f4f3126b81b593b80a9c503d1b31e`; test commit: `b457100a9ca2d6507aba03204439004e8c651245`.
- CI run `45029991098` for test commit is currently pending; all five jobs were queued/in progress at checkpoint. No verification claim until it completes.
- Next audit: parameter/external-origin projection propagation. Current projection helper requires tracked owner-origin metadata and rejects unknown/external provenance; parameter reference initialization may retain `borrow_origin` fallback metadata not yet composed by projection. Inspect lifetime guarantees before extending support.


### SEM-005 — Mutable parent reborrow contract documented (2026-11-22)

- Normative syntax reference now records mutable reborrow syntax as `&mut child!T = &mut *parent;` within a child lexical scope.
- While child is active, parent is suspended: direct read/write, reassignment, move/relocate, reborrow or call-passing is prohibited. At child scope exit, parent resumes.
- Nested reborrows must suspend only the immediate parent and resume it in reverse lexical order. Flow merges must preserve parent-child state and cannot duplicate active exclusivity.
- Documentation commit: `8ff1f5eaf278700a0ac4ae1993d74d4b42950ffc`.
- **Implementation remains pending.** Existing `Slot` and `BorrowOrigin` accounting is rooted at storage owners; current release/snapshot-restore paths independently toggle owner counters. Mutable reborrow cannot be safely implemented by merely relaxing the existing rejection. The next code step is to model suspension ownership explicitly and cover scope cleanup plus branch snapshot/merge before enabling the syntax.


### SEM-005 — Mutable reborrow relationship design checkpoint (2026-11-23)

- Added `docs/semantic/mutable-reborrow-design.md` as the implementation contract for explicit borrow-relationship tokens and parent-child suspension/resumption.
- Defined ten invariants covering immediate parent suspension, owner reservation, reverse-order resume, scope cleanup, dangling-parent prevention, atomic rebinding, branch snapshot/restore/join, and external provenance.
- Established an ordered implementation sequence: relationship lifecycle helpers, binding/reassignment integration, scope cleanup, flow snapshots/merges, then narrowly enabling syntax. Minimum positive/negative regression gates are listed.
- No runtime/compiler behavior changed; `&mut *parent` remains rejected. This is a safety checkpoint, not feature completion. SEM-005 stays IN_PROGRESS; OIR remains blocked.
- Design commit: `e519e545968466cf81a4c6aeb1b5aa9c194508bf`.


### SEM-005 — Mutable projection reborrow fail-closed regression (2026-11-23)

- Added a Semantic regression case for attempting a mutable member projection reborrow from an active mutable reference (`&mut child!i32 = &mut parent.x`).
- The regression expects Semantic to reject the unsupported operation; it does not enable parent-child suspension/resumption.
- The regression commit is `7f81d4b786e8cb856bd9d3c8fbbe3e7626bad2dc`.
- Runtime/CI validation has not yet been observed for this commit. SEM-005 remains **IN_PROGRESS**; OIR remains gated.


### SEM-005 — Explicit mutable dereference reborrow rejection (2026-11-23)

- Semantic now detects `&mut *parent` when `parent` is a mutable safe-reference binding and emits a dedicated diagnostic explaining that parent suspension/child resumption is not implemented.
- This prevents the expression from falling through to ordinary lvalue/provenance handling, where the parent-child relationship would be lost.
- Regression coverage now includes both `&mut *parent` and `&mut parent.field` rejection.
- Commits: implementation `06fb96f39360e86e5bcf841de5c05aef3e49497f`; tests `c631025bcb4671fcdbfc75cf6f15d3583ec0696d`.
- CI has not reported a run yet. No mutable reborrow support is enabled; SEM-005 remains **IN_PROGRESS**.

### SEM-005 — C lifecycle audit (2026-11-23)

Audited `BorrowOrigin`, atomic binding replacement, release, scope cleanup, and branch snapshot/restore/merge in `semantic.c`. The current origin model stores only storage-owner/path/kind; it cannot represent the immediate mutable parent or resume it on child release. Snapshot clone/union/restore also preserve only those existing fields. A bare parent `Slot*` addition would be unsafe/incomplete because cloned origins and scope destruction need coordinated identity/lifetime handling.

No semantic behavior was changed. Mutable dereference and projection reborrows remain explicitly rejected. SEM-005 remains IN_PROGRESS; OIR remains gated. Audit details: `docs/semantic/mutable-reborrow-design.md`.

### SEM-005 — stable borrow identity foundation (2026-11-23)

Added a monotonic context-local identity to reference-binding Slots and recorded borrower/parent identity fields in BorrowOrigin. Snapshot cloning and branch-origin union preserve these fields, and shared-copy/projection origin creation initializes the destination identity. Identity exhaustion fails closed. This is preparatory only: no parent-child relation or suspension/resumption is active, and mutable reborrows remain rejected. See `docs/semantic/mutable-reborrow-design.md`.


### SEM-005 — borrow identity initialization audit correction (2026-11-23)

- Fixed semantic_init() so the monotonic borrow identity counter always starts at zero.
- Preserved the Slot identity when legacy scalar borrow metadata is captured into a branch snapshot.
- Moved shared-projection borrower identity allocation ahead of BorrowOrigin construction so every created origin records a nonzero destination identity.
- Added a C regression that seeds the counter with UINT64_MAX before initialization and verifies it is reset.
- Implementation commit: 7619c4f; regression commits: 67d55e7, 356035e.
- CI has not been verified for these commits. Parent-child registration, suspension/resumption, graph snapshots/joins and their behavioral tests remain open; SEM-005 stays IN_PROGRESS and OIR remains gated.


- Follow-up regression added: two distinct tracked shared-reference bindings must receive nonzero, distinct Slot identities, and each BorrowOrigin must carry its own borrower's identity. Test commit: 4d32fd6. CI remains unverified.


### CI regression — mutable projection reborrow rejection (2026-11-23)

- Root cause: `semantic_register_borrow_binding()` routed only shared `TOKEN_BIT_AND` member/index projections to the projection handler. Mutable `TOKEN_BORROW_MUT` projections fell through to generic lvalue borrow registration, so `&mut parent.x` was incorrectly accepted.
- Fix: explicitly reject member/index projections whose root Slot is a mutable safe reference until parent-child suspension/resumption exists.
- Existing negative regression already covers `&mut parent.x` and `&mut *parent`.
- Fix commit: `4d00ef2`.
- Workflow run 45085185651: Ubuntu build/test job passed at initial check; macOS, Windows, ASan/UBSan and Valgrind were still running. SEM-005 remains IN_PROGRESS; OIR stays gated.


### SEM-005 — first scoped mutable reborrow implementation (2026-11-23)

- Added immediate parent identity to mutable child BorrowOrigin records and parent suspension by child identity.
- Added child-first recursive release: ending a child resumes its parent without releasing the storage owner's exclusive reservation; ending the root releases the owner reservation after descendants.
- Added nested reverse-order reborrow support for a single known exclusive origin.
- Enabled and tested &mut *parent and mutable member/index projection reborrows. Suspended parent identifier and member/index access is rejected; parent access resumes after child scope exit. Parent rebinding while suspended is rejected.
- Regression coverage includes child writes, direct and projection reborrows, post-scope parent use, nested reborrow release order and suspended parent reassignment.
- Verification: workflow run 45090068185 passed 5/5 jobs. Ubuntu CTest 74/74; ASan/UBSan 74/74; Valgrind reports zero errors.
- Commits: implementation `f68585c`, `ee81654`, `4f44554`, `b3e1612`, `abd51da`; regression tests `0541025`, `f1a616f`; design note `6dc3fa7`.
- This is a verified SEM-005 implementation slice, not completion. Branch snapshot/restore/join graph tests, OOM atomicity, loop/early-exit cleanup, full call/move/relocate rejection coverage and dangling-origin tests remain open. SEM-005 stays IN_PROGRESS and OIR remains gated.


### SEM-005 — mutable borrow branch graph consistency (2026-11-23)

- Mutable BorrowOrigin sets are now joined only when the full relationship records match: owner, borrower identity, immediate parent identity, kind and projection path. They are not unioned across divergent paths.
- Parent suspension identities must also match at the control-flow join; incompatible relationships produce a Semantic diagnostic and compilation remains blocked.
- Added if/els regressions: identical parent/child projection graphs survive snapshot restore/join; differing child projection paths are rejected.
- Workflow run 45090600776 passed all five jobs (Ubuntu, Windows, macOS, ASan/UBSan, Valgrind); Ubuntu and ASan/UBSan report 74/74 tests, Valgrind reports zero errors.
- Implementation commit `ceb70cc`; final regression commit `6ae2372` (an earlier fixture revision used invalid `else` spelling and was corrected to Nexus Flow `els`). Design update `26b4fcb`.
- This closes direct if/els graph-join coverage only. Nested graph branch cases, select/scan joins, loop back-edge invariants, and allocation-failure/restore atomicity remain open. SEM-005 stays IN_PROGRESS; OIR remains gated.


### SEM-005 — nested graph-safe snapshot restoration (2026-11-23)

- Fixed snapshot restore ordering: the complete current borrow graph is released in a first pass, then all cloned origin sets and suspension identities are installed in a second pass. This prevents a later parent restore from recursively deleting a child restored earlier in Slot-list order.
- Added post-restore graph validation in both directions: suspended parents must resolve to their active child, and child origins must resolve to a suspended parent with the same exclusive storage owner.
- Expanded branch regression to preserve root → child → grandchild across both if/els paths, write through the grandchild after the join, then verify child/root resumption in reverse scope order.
- Workflow run 45090990756 passed all five jobs; Ubuntu and ASan/UBSan 74/74, Valgrind successful.
- Implementation commit `e2cc0ae`; regression commit `462f528`; design update `89d69ec`.
- Remaining: select/scan branch joins, loop back-edge and early-termination cleanup, OOM atomicity for child registration and snapshot restoration. SEM-005 remains IN_PROGRESS; OIR remains gated.


### SEM-005 — select/scan joins and loop lifecycle regressions (2026-11-23)

- Added equal-graph mutable reborrow tests across `select/default` and `scan/else`; the child remains writable after each join.
- Added loop-body child lifetime tests: a nested child is released on each iteration and after `break`, and the parent is usable after the loop.
- Added a negative loop back-edge test: rebinding an outer child reference to a parent reborrow changes the mutable relationship graph across zero/one-iteration paths and is rejected.
- Workflow run 45091574297 passed all five jobs. Ubuntu and ASan/UBSan each passed 74/74; Valgrind reports zero errors.
- Test commit `1ea2581`; docs `9fd8b4c`.
- Remaining: allocation-failure atomicity for child registration and snapshot restoration, broader early-return paths, projection overlap/disjointness matrices, and dangling-origin/lifecycle stress. SEM-005 remains IN_PROGRESS; OIR remains gated.


### SEM-005 — mutable reborrow OOM atomicity (2026-11-23)

- Added a dedicated OOM fixture that starts with child → spare and attempts to replace it with child → parent.
- Swept every Semantic allocation site in the fixture. A focused assertion confirms that failures in child reborrow metadata creation preserve the prior child origin, leave the parent unsuspended, and keep both original exclusive reservations active.
- Mutable reborrow metadata allocation now sets `SEMANTIC_RESULT_OUT_OF_MEMORY`; direct borrow-origin installation also propagates OOM. Borrow identity exhaustion is reported separately as invalid data.
- Workflow run 45092219887 passed all five jobs; Ubuntu and ASan/UBSan 74/74; Valgrind zero errors.
- Commits: OOM propagation `731d773`, direct borrow-origin OOM `a1a39ef`; fixture/CMake `11bdcaf`, `0a3c6ca`, `8e56a75`; atomicity test `acd599e`, persistent-scope fixture correction `b9504e2`.
- Child-registration OOM atomicity is verified. Snapshot clone/restore OOM atomicity, broader early-return/projection matrices and dangling-origin stress remain open. SEM-005 remains IN_PROGRESS; OIR remains gated.


### SEM-005 — dangling allocation pointer / use-after-free safety requirement (2026-11-24)

- New required invariant: when an `area()`/`zone()` allocation or owned resource is released, every handle, pointer alias and projection derived from it becomes invalid, regardless of the release path. This includes explicit `free()` and global resource cleanup triggered by `?;` or `}?;` Halt. Halt cleanup is not a special exemption: later code cannot reuse a released handle or dereference any pointer derived from it.
- Reusing the same address for a later allocation must not revive stale aliases or let an old pointer read/write the new allocation. The identity is the allocation instance/generation, never the numeric address alone.
- Semantic must emit an explicit source-located error for every statically provable post-release read, write, dereference, borrow or handle use. At control-flow joins, if a resource is released on any reachable incoming path, access is rejected unless the analysis proves the resource is live on the path being used. Unknown provenance/liveness fails closed.
- Syntax reference now states this normative rule, but allocation identity/generation tracking, alias invalidation, Halt cleanup state propagation and use-after-release diagnostics are **not yet implemented/verified**. Current source audit found two distinct AST forms: `?;` becomes `AST_NEXUS_HALT`, while the block-closing `}?;` becomes `AST_BLOCK_HALT`. `AST_NEXUS_HALT` only checks its optional expression; `AST_BLOCK_HALT` currently records a `void` annotation and explicitly defers cleanup ordering/lowering to OIR. Neither form currently invalidates Semantic resource/pointer state. The lexical traversal gap has since been addressed in the C Semantic implementation: both forms record the current function/execution-context depth as halted, so sibling statements and enclosing blocks in that same execution context are skipped. The halt state is cleared only when that function body completes, allowing the next function declaration to be analyzed. This first control-flow patch is under exact-head CI; it does not yet invalidate resources or pointer aliases and is not a SEM-005 completion claim.
- Required tests: direct handle after free; raw pointer alias after free; pointer to a field/offset inside an allocation; allocation-address reuse simulation; aliases copied through assignment; explicit `?;` and `}?;` cleanup followed by stale handle/pointer access; code after Halt; nested-scope Halt cleanup; branch/control-flow merge with released-vs-live state; fail-closed unknown provenance. Include exact diagnostic code, line and column assertions.
- SEM-005 remains IN_PROGRESS; OIR remains gated.


### SEM-005 — snapshot OOM propagation coverage (2026-11-23)

- Added a branch-snapshot fixture with mutable parent/child reborrows on both if/els paths and a post-join child use.
- The semantic allocation-failure injector sweeps the snapshot workload and requires every injected failure to propagate `SEMANTIC_RESULT_OUT_OF_MEMORY`.
- Workflow run 45092521356 passed all five jobs; Ubuntu and ASan/UBSan 74/74; Valgrind zero errors.
- Fixture `875e910`; CMake registration `a2d799f`; docs `17ac609`.
- This proves OOM propagation across snapshot allocations, not full restore-state atomicity. A dedicated assertion that failed clone/restore leaves the exact prior graph unchanged remains open. SEM-005 remains IN_PROGRESS; OIR remains gated.


### SEM-005 — snapshot OOM graph-integrity checks (2026-11-23)

- Extended the snapshot OOM harness to process function-body statements individually, keeping parent/child Slots inspectable after branch snapshot operations.
- Every injected failure now checks graph integrity: parent suspension ↔ child identity, child parent identity ↔ suspended parent, common exclusive storage owner, and no suspended parent when the child is a direct borrow.
- Workflow run 45092909275 passed all five jobs; Ubuntu and ASan/UBSan 74/74; Valgrind zero errors.
- Test commit `fed89d5`; design update `53cee06`.
- This verifies structural graph integrity under injected OOM, not exact equality with the pre-restore graph snapshot. Exact rollback-equivalence tests remain open. SEM-005 remains IN_PROGRESS; OIR remains gated.


### SEM-005 — nested projection path composition (2026-11-23)

- Added a parent borrow of `outer.inner` followed by a child reborrow of `parent.left`; the resulting owner path is composed through both projections.
- Verified disjoint `outer.other` access remains valid while the child is active, while a second mutable borrow of overlapping `outer.inner.left` is rejected.
- Workflow run 45093228358 passed all five jobs; Ubuntu and ASan/UBSan 74/74; Valgrind zero errors.
- Test commit `922fade`; design update `55916fd`.
- Remaining projection coverage: member/index combinations, dynamic indices, deeper nested paths and a broader overlap/disjointness matrix. SEM-005 remains IN_PROGRESS; OIR remains gated.


### SEM-005 — early termination cleanup (2026-11-23)

- Added `ret` while a child mutable borrow is active; function-scope unwind must release the child without leaving parent suspension dangling.
- Added `break` inside a child scope; parent is writable after the loop.
- Added `continue` inside a child scope; child is released before the next iteration.
- Workflow run 45093659006 passed all five jobs; Ubuntu/ASan 74/74; Valgrind zero errors.
- Test commit `755ec7f`; design update `558a27e`.
- Remaining lifecycle gaps: panic/defer/other early exits, function-call transfer cases and dangling-origin stress. SEM-005 remains IN_PROGRESS; OIR remains gated.


### SEM-005 — suspended-parent transfer rejection (2026-11-23)

- Added a negative test passing a suspended parent as an `&mut T` call argument.
- Added a negative test attempting `_>` relocation of the suspended parent.
- Workflow run 45093904144 passed all five jobs; Ubuntu/ASan 74/74; Valgrind zero errors.
- Test commit `11c8302`; design update `503da37`.
- Remaining transfer/lifecycle gaps include other call forms, closure/thread capture and broader dangling-origin stress. SEM-005 remains IN_PROGRESS; OIR remains gated.


### SEM-005 — indexed parent-origin reborrow paths (2026-11-23)

- Verified a mutable parent borrowing `items[0]` can be reborrowed through `&mut *parent`; disjoint `items[1]` access remains valid and the parent resumes after child scope exit.
- Verified overlapping `items[0]` alias creation is rejected while the child is active.
- Verified a dynamic parent origin `items[index]` conservatively conflicts with a fixed-index alias.
- Workflow run 45094328449 passed all five jobs; Ubuntu/ASan 74/74; Valgrind zero errors.
- Test commit `689707c`; design update `9a9d427`.
- Remaining: mixed member/index projection chains (array fields in aggregates are not covered here), deeper paths, and broader dynamic-index cases. SEM-005 remains IN_PROGRESS; OIR remains gated.


### SEM-005 — mixed aggregate/index projection gap (2026-11-23)

- Attempted a fixture for `container.items[0]` where `items` is a fixed array field inside a struct, followed by a child field reborrow.
- CI exposed that Semantic does not currently treat array member projections as indexable arrays; it reports `Index target must be an array, map, tuple, bit vector, or byte block`, then member type resolution fails.
- Removed the unsupported fixture instead of broadening language semantics without implementation/tests. Standalone fixed/dynamic array index borrow tests remain.
- Status: nested member-array projection and mixed member/index composition are **UNSUPPORTED/UNVERIFIED**. SEM-005 remains IN_PROGRESS; OIR remains gated.


### SEM-005 — Aggregate inline-array member projection (2026-11-23)
- `FieldInfo` now keeps the declared nominal element type separately from the encoded inline-array shape. Member resolution returns the array signature for inline array fields, so `container.items[0]` resolves as an array element instead of treating `items` as a scalar.
- Bounds validation for constant indexes now reads fixed first-dimension extents from aggregate member signatures.
- Added Semantic regressions for a valid aggregate array member read/write (`container.items[0]`, `[2]`) and out-of-range fixed-array member index `[3]`.
- Verification: exact-head CI run `45181310995` is in progress; Ubuntu CTest, macOS CTest, and ASan/UBSan have passed so far. Windows and Valgrind remain pending. SEM-005 is not closed.


### SEM-005 — Halt lexical traversal implementation checkpoint (2026-11-25)

- Added `SemanticContext.halted_depth`, initialized to `UINT32_MAX` (no active Halt).
- `AST_NEXUS_HALT` and `AST_BLOCK_HALT` now terminate semantic traversal of the current lexical table. `exit_scope()` clears the marker when leaving that table, allowing the enclosing scope to continue.
- Added parser/Semantic regression fixtures for `?;` stopping the current execution context, including when nested, and `}?;` stopping the enclosing execution context as required by the normative syntax contract.
- Commits: `672fe3d2` (state), `4e8391cc` (traversal), `5138244a` (initialization), `6fd942a5` (tests). The first test attempt exposed a parser mismatch: `TOKEN_HALT` already represents the complete `?;` token, but the Parser incorrectly attempted to parse a following statement as an optional Halt expression. The parser now treats it as a complete statement token. Exact-head CI for the corrected tests is running; do not treat this checkpoint as passed until all jobs are green.
- Resource release and alias invalidation on either Halt form remain unimplemented. Next implementation step is allocation identity/generation metadata on `Slot`, propagation through aliases/address expressions, and invalidation on `free()` and scope Halt.
