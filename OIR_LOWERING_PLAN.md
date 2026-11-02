# OIR Lowering — staged implementation plan

**Status:** Implementation in progress; scalar literals, recursive arithmetic, initialized local-variable reads and straight-line assignment SSA versioning are implemented, with parser-to-semantic-to-OIR integration coverage added (build/test execution still pending)  
**Input:** validated `OIR_KIND_SEMANTIC_AST`  
**Output:** validated `OIR_KIND_MODULE`  
**Module:** independent IR Lowering DLL/SO/dylib, invoked by OCC  
**Related:** [OIR_MODULE.md](OIR_MODULE.md), [OIR_SEMANTIC_AST.md](OIR_SEMANTIC_AST.md), [COMPILER_PIPELINE.md](COMPILER_PIPELINE.md)

## 1. Purpose

This plan defines a safe route from the current semantic artifact to canonical OIR. It does not treat the existing AST-walking code generator as a design constraint. The lowering stage will consume only the validated semantic artifact and produce a target-independent SSA module.

The first implementation is deliberately small and verifiable. The current executable slice accepts one non-extern, parameterless function with a straight-line sequence of initialized scalar local declarations and simple assignments to previously declared mutable locals, followed by exactly one return. Local initializers, assignment right-hand sides and the return expression may contain annotated integer, floating-point or boolean literals, supported arithmetic expressions, and reads of previously declared locals. Local variables are promoted directly to SSA values; each simple reassignment creates a new SSA value and updates the symbol-to-value mapping. This slice does not yet emit memory loads/stores for locals. The supported binary operators are +, -, *, /. Every other shape is rejected; unsupported AST nodes are never silently skipped, approximated or emitted as no-ops.

## 2. Preconditions before executable lowering

The following infrastructure is required before the first production lowering DLL is integrated:

1. A canonical `OIR_MODULE` payload writer and reader. Schema 1.1 codec, including explicit signedness and hard/const type qualifiers, is implemented; see [OIR_MODULE.md](OIR_MODULE.md).
2. A module verifier for table bounds, IDs, references, function/block ownership, terminators, opcode signatures and SSA rules.
3. An initial structural opcode registry is now implemented (registry 1.0); verifier-level type/effect semantics are still required.
4. The block-parameter SSA contract, including loop-carried values and predecessor-edge validation.
5. Semantic annotations sufficient to resolve every lowered identifier, expression type and declaration.
6. A source-map policy that preserves AST/source provenance without fabricating missing spans.
7. Deterministic lowering tests: same semantic artifact + same options = same logical module bytes.

The IR Lowering DLL must validate its input artifact itself even when OCC has already validated the envelope. It must not retain pointers into the input after the invocation ends.

## 3. Proposed language coverage by increment

The following is a proposed implementation order, not a claim that these constructs are already supported by semantic analysis.

### Increment 1 — scalar function core

Candidate constructs:

- Program and function definitions.
- Function parameters and return declarations.
- Local variable declarations and initializers.
- Integer, floating-point, boolean and character literals.
- Identifier reads.
- Unary and binary arithmetic.
- Comparisons and boolean operations, once their exact token/type rules are registered.
- Simple variable assignment.
- Direct function calls.
- Return statements.

Required OIR capabilities:

- Function declarations/definitions and typed parameters.
- Constants and typed SSA values.
- Integer/floating arithmetic and comparison opcodes.
- Explicit conversions only when present in validated semantic conversion records.
- Calls with checked signatures.
- Return terminators.
- A verifier that rejects type-inconsistent operands and use-before-definition.

The semantic artifact currently has useful annotations for literals, identifiers, binary expressions, assignments, unary expressions and function calls. However, this coverage is not yet sufficient to claim that every expression form or every call result is fully typed. Lowering must check required annotations and reject missing information rather than infer semantics from source spelling.

### Increment 2 — structured control flow

Candidate constructs:

- `if` / `else`.
- `while`, `for` and infinite loops.
- `break` and `continue`.
- Loop initialization, condition and increment expressions.

Required OIR capabilities:

- Basic blocks and explicit conditional/unconditional branches.
- Explicit CFG successor edges and terminators.
- Block parameters with one incoming value per predecessor edge for variables assigned along multiple incoming paths.
- Loop back-edges and loop-carried values.
- Dominance and predecessor-aware merge verification.

This increment depends on semantic analysis reliably recording scopes, declarations, identifier resolutions and condition types inside all control-flow branches. The current checker enters scopes for blocks and loops, but its durable annotations are not yet comprehensive for control-flow nodes. This must be closed before enabling the corresponding lowering.

### Control-flow frontend gate (updated 2026-11-02)

The active C frontend now distinguishes three forms without merging their source-language meanings:

1. **Structured conditional:** `if (condition) { ... }`, `elsif(condition) { ... }` and `els { ... }` are explicit branching statements. They use the existing `AST_IF` node with condition/then/else references. Lexer, parser and semantic validation are implemented and covered by tests.
2. **Conditional action shorthand:** `(condition) -> break;`, `(condition) -> continue;` and other unambiguous statement-only right-hand actions are parsed as conditional control flow. The parser recognizes a parenthesized condition followed by `TOKEN_PIPE` and a statement-start token that cannot be an expression pipe target. It normalizes this one-arm branch to `AST_IF`; the source syntax remains distinct from the `if` keyword.
3. **Nexus Flow pipe:** ordinary expression transfers such as `source -> target;` and `source -> process();` remain `AST_NEXUS_PIPE`. A pipe block `source -> { ... }` is not reclassified as a conditional action. Rule-set trigger syntax is parsed in its own rule context.

Parser regression tests verify `break`, `continue`, `ret` conditional actions and ensure an ordinary `a == b -> target` expression remains an `AST_NEXUS_PIPE`. Semantic tests verify that conditional `break`/`continue` are accepted inside loops and rejected outside loops. CI run `41364158463` passed on Windows, Ubuntu and macOS, including Ubuntu ASan/UBSan and Valgrind jobs.

**Remaining control-flow gap:** the current OIR lowerer does not yet lower `AST_IF`, loops, conditional branches or loop-control statements into CFG blocks. Semantic annotations/merge information for all control-flow nodes, short-circuit logical operators, block parameters and predecessor-aware verification remain prerequisites. No control-flow construct is advertised as supported by the OIR lowerer until the full parser → semantic artifact → CFG lowering path is tested.

Implementation order:
1. Complete semantic annotations and definite-assignment/merge rules for branches and loops.
2. Freeze OIR basic-block, terminator, successor-edge and block-parameter contracts.
3. Implement CFG lowering and predecessor-aware merge tests.
4. Add verifier mutation tests for malformed edges, missing terminators, invalid block parameters and loop back-edges.

### Increment 3 — aggregates and addressable data

Candidate constructs:

- Struct/union declarations and field access.
- Enum declarations and variants.
- Tuples.
- Array/index access.
- Explicit casts and address operations.

Required OIR capabilities:

- Canonical aggregate and tuple types.
- Field/index addressing and layout-neutral access operations.
- Explicit load/store or address operations where source semantics require addressable storage.
- Defined cast/conversion behavior.
- Layout metadata separated from target-specific physical layout.

No target byte offsets or register assignments are introduced in generic OIR.

### Increment 4 — NEXSUS Flow control and ownership

Candidate constructs include:

- Handler capture and ownership transfer.
- Flow success/catch/retry operations.
- Labels and jumps.
- Halt/panic/unpack.
- Area/zone and memory-region operations.

These constructs must not be translated as ordinary C assignments, calls or branches by assumption. Each needs a language-level contract for ownership transfer, failure behavior, lifetime, ordering and observable side effects. The lowering design must preserve those semantics explicitly in OIR.

### Increment 5 — concurrency and hardware-facing operations

Candidate constructs include spawn/listen/send/receive, thread blocks, interrupts, register and port operations, and other hardware-facing intrinsics.

These require explicit concurrency, memory-ordering, privilege and target-capability contracts. They are not part of the initial scalar/control-flow lowering subset.

## 3.1 AST 1.0 field contract for the first scalar slice

The AST frontend writer and field-schema validator establish these stable field IDs. Lowering reads them through `oir_ast_node_table_get_field()`; it must not depend on frontend C structs or enum ordinals.

| Node kind | Field ID | Meaning | Wire type |
|---|---:|---|---|
| `AST_PROGRAM` (1) | 1 | Declaration-list head | node ref |
| `AST_FUNC_DECL` (4) | 1 | Function name | string ref |
| `AST_FUNC_DECL` (4) | 2 | Declared return-type spelling | string ref |
| `AST_FUNC_DECL` (4) | 3–5 | extern / public / exported flags | bool |
| `AST_FUNC_DECL` (4) | 6 | Parameter-list head | node ref |
| `AST_FUNC_DECL` (4) | 7 | Function body | node ref |
| `AST_BLOCK` (2) | 1 | Statement-list head | node ref |
| `AST_RETURN` (26) | 1 | Return token ID | token ID |
| `AST_RETURN` (26) | 2 | Returned expression | node ref |
| `AST_LITERAL` (13) | 1 | Literal token ID | token ID |
| `AST_LITERAL` (13) | 2 | Literal category (0 integer, 1 float, 2 string, 3 char, 4 bool) | u32 |
| `AST_LITERAL` (13) | 3 | Integer payload | i64 |
| `AST_LITERAL` (13) | 4 | Floating payload | f64 |
| `AST_LITERAL` (13) | 5 | Original literal text | string ref |
| `AST_BINARY_EXPR` (15) | 1 | Operator token ID | token ID |
| `AST_BINARY_EXPR` (15) | 2–3 | Left / right expression | node ref |
| `AST_IDENTIFIER` (14) | 1 | Identifier spelling | string ref |

Node references use 1-based AST node IDs; zero denotes an absent optional child. String references are byte offset + byte length into the AST UTF-8 string table. Token IDs are the stable IDs from `OIR_TOKEN_REGISTRY.md`, not `OmniTokenType` values.

### First executable slice boundary

The first lowerer implementation is intentionally narrower than the full scalar-core candidate list:

- Exactly one non-extern function definition in the program.
- No parameters.
- Zero or more initialized scalar local declarations and simple `=` assignments to earlier mutable local variables, followed by exactly one return statement.
- Local initializers, assignment right-hand sides and the returned expression may be supported scalar literals, recursively supported arithmetic expressions, or reads of earlier local variables. Supported operators are +, -, *, /; exponentiation is not yet lowered.
- Assignment targets must be resolved local variable identifiers already present in the SSA local table. Compound/forced assignment and non-identifier lvalues are rejected.
- Return type and expression type must be established by the semantic artifact and must map to a currently registered OIR primitive type.
- The function body must contain no additional executable statement; comments or source trivia are not AST statements and are not represented here.

This slice is a bring-up target for the artifact-to-module path, not a claim that parameters, calls, comparisons, control flow, or the complete numeric promotion rules are supported. The lowerer must reject multiple functions, parameters, declarations, unknown literal categories, unsupported operators, missing type annotations, unresolved type names, and any extra body statement.

The AST schema preserves the declared return-type spelling, while the semantic tables preserve canonical type records. Lowering must cross-check both sources; it must not infer a canonical type solely from the spelling. The initial implementation must also reject a semantic type whose primitive width/signedness mapping is not explicitly registered. This avoids accidentally treating the C frontend's current type spelling rules as the final NEXSUS Flow numeric contract.

## 3.2 Arithmetic semantic gate and current lowering

The semantic checker performs a deliberately conservative check for arithmetic binary expressions: both operands must resolve to the same primitive numeric type. The current check does not implement the full NEXSUS Flow numeric promotion/conversion lattice, and it does not authorize implicit widening or narrowing.

The expression lowerer recursively walks the validated AST expression tree in left-to-right postorder, preserving parser-established precedence and associativity. Each literal emits a typed constant; identifier reads resolve through the current local SSA mapping; each supported binary operation emits a new SSA value. Initialized local declarations and simple reassignment update the symbol-to-value mapping. The return terminator consumes the final SSA value. AST token IDs are read using their 16-bit wire width and stable OIR token registry IDs.

Current supported operators: `+`, `-`, `*` and `/`. The first executable slice accepts exactly one parameterless, non-extern function, zero or more initialized scalar locals, simple `=` reassignment to earlier mutable locals, and one final return. Calls, parameters, compound/forced assignment, non-local lvalues, multiple functions and control flow remain unsupported.

The legacy literal-only API remains available as a compatibility entry point. The `oir_lower_semantic_ast_return()` API handles literals, recursive arithmetic, initialized scalar locals, identifier reads and straight-line reassignment. Integration tests include `ret 10 + 20 * 3;`, `v:x!i32 = 40; x = x + 2; ret x;`, and assert SSA references plus multiplication-before-addition order. CI run `41325592201` passed on Windows/Ubuntu/macOS and Ubuntu ASan/UBSan/Valgrind. The implementation is still a static library/test path, not an independent lowering DLL or OCC stage handoff.

## 4. Fail-closed lowering behavior

For every AST node in the input semantic artifact, the lowerer must do exactly one of the following:

- Lower it to a fully specified OIR operation/control-flow structure.
- Consume it as a compile-time-only construct under an explicit language rule.
- Reject it with a diagnostic identifying the unsupported construct and source provenance.

A parent node cannot be considered successfully lowered if a child was ignored. No partially emitted executable module is published as VALID. If diagnostic recovery is later supported, partial module validity and downstream acceptance must be separately specified.

## 5. Semantic data requirements

Before a construct enters the supported subset, semantic analysis must provide or validate:

- A resolved symbol ID for every identifier reference.
- A canonical type ID for every value-producing expression.
- A value category where lvalue/rvalue distinction affects lowering.
- A resolved function symbol and checked argument list for each call.
- Explicit conversion records for inserted implicit/explicit conversions.
- Stable declaration and scope identity for variables and parameters.
- Sufficient control-flow context for labels, jumps, break and continue.
- Explicit language-defined ownership/effect annotations for NEXSUS Flow operations.

The current `SEMANTIC_AST` artifact already has stable scope, type, symbol, annotation, relation and optional conversion sections. The checker integration still needs broader coverage before these requirements can be treated as satisfied.

## 6. Test strategy

Each increment must include:

1. Positive source fixtures covering every supported construct.
2. Negative fixtures for unsupported or semantically invalid constructs.
3. Expected semantic-artifact fixtures.
4. Expected OIR logical fixtures, independent of target assembly.
5. Verifier mutation tests for malformed IDs, references, types, terminators and SSA edges.
6. Determinism tests across repeated runs.
7. DLL boundary tests proving no AST pointers, semantic pointers or allocator state cross the ABI.
8. Cross-platform CI for Windows, Linux and macOS.

The legacy AST-walking code generator may remain temporarily available for comparison during migration, but it is not the specification and does not define the new lowerer's behavior.

## Implementation progress

- [x] Initial opcode registry 1.0: stable IDs, names, operand arity/kinds, result-presence rules, and coarse purity/memory/trap/terminator flags. Implemented in `src/c/oir/oir_module_opcode.h/.c` and integrated as the `occ_oir_module_opcode` CMake target.
- [x] Initial `OIR_MODULE` payload codec 1.1: fixed-width little-endian records, bounded reader/writer, UTF-8 and basic cross-reference checks, opcode-shape validation, and zero-copy table views. Implemented in `src/c/oir/oir_module.h/.c` and integrated as `occ_oir_module`.
- [x] AST node-local TLV field lookup and string-span access: `oir_ast_node_table_get_field()` and `oir_ast_node_table_get_string()` let lowering inspect validated node fields and resolve string references without duplicating TLV parsing.
- [ ] Module verifier for function/block ownership, complete instruction coverage, terminators, type rules and SSA.
- [x] Validated Semantic AST artifact reader: `oir_semantic_ast_artifact_open()` opens the envelope, checks required section versions, validates AST field schema/graph and cross-table semantic references.
- [x] Constant opcode immediate contract: integer, IEEE-754 binary64 and boolean constants carry typed immediate operands.\n- [x] Initial semantic primitive-to-OIR type mapping for i8/i16/i32/i64, u8/u16/u32/u64, f32/f64 and bool, preserving hard/const qualifiers; unsupported primitive representations are rejected.
- [x] Initial scalar IR lowering pass: one parameterless function with one typed integer/float/bool literal return; caller-owned workspace; validated OIR_MODULE writer round-trip.
- [x] Real frontend-to-lowering integration test: Lexer/Parser AST -> semantic analysis -> validated SEMANTIC_AST artifact -> OIR_MODULE payload. Stable OIR token IDs are used at the artifact boundary, and hard return qualifiers are checked against the function's semantic type annotation.
- [x] Semantic arithmetic operand validation: arithmetic binary expressions now require matching primitive numeric operand types; mixed types and nonnumeric operands are rejected before lowering. Covered by `test_semantic_arithmetic`.
- [x] Initial structured conditional semantics: `if / elsif / els` branches are checked with branch-local scopes; conditions accept boolean literals, boolean expressions and comparison expressions, while non-boolean conditions are rejected. Comparison expressions are annotated as `bool`; comparison operands must be compatible, and ordered comparisons currently require numeric operands. Statement-level `(condition) -> break/continue/ret;` is separately recognized as conditional control flow; ordinary expression `->` pipes remain `AST_NEXUS_PIPE`.
- [x] Initial loop-control semantics: `break` and `continue` are parsed as dedicated AST statements and rejected outside loop bodies. Function bodies reset inherited loop context so nested functions cannot break or continue an enclosing function's loop. Conditional-action shorthand uses the same semantic checks.
- [ ] Control-flow semantic completion: logical short-circuit operators are not yet part of the active expression parser; branch definite-assignment/merge analysis and loop control-flow annotations remain pending.
- [x] Initializer-based local type inference in semantic results: an untyped variable initialized from a typed expression now receives a canonical type ID, and its declaration annotation is emitted after inference. `test_semantic_result_integration` covers inference from a resolved identifier. This is semantic groundwork only; local-variable OIR lowering is not enabled yet.
- [x] Initial local-variable OIR lowering: initialized scalar declarations are matched to their semantic variable symbols, their initializer expressions are lowered, and later identifier reads resolve through a caller-owned `symbol_id → SSA value_id` table. The end-to-end case is `v:x!i32 = 42; ret x;`. This is SSA promotion for straight-line locals, not memory `load/store` lowering.
- [x] Straight-line local reassignment SSA versioning: a simple `=` assignment to an existing mutable local lowers the right-hand side against the previous mapping and then updates the symbol's current SSA value. The integration fixture `v:x!i32 = 40; x = x + 2; ret x;` checks the new value reference. Forced assignment, const reassignment, compound assignment, non-local lvalues and control-flow merges are not enabled.
- [x] Recursive arithmetic expression lowering for the first bounded scalar slice: literals, arithmetic precedence, initialized scalar locals, identifier reads and straight-line reassignment.
- [ ] Broader scalar function coverage: parameters, calls, multiple functions, compound assignments and control-flow merges.
- [x] Independent `occ_oir_lowering_stage` shared module: accepts validated SEMANTIC_AST OIR, uses caller-local workspace, enforces a 65,536-node stage limit, and emits a complete OIR container with an OIR_MODULE payload. Stage ID is `OCC_STAGE_OIR_LOWERING = 7`; module ID is `0x00010004`.
- [x] Module-loader regression path: `test_oir_lowering` loads the shared library, forces the NEED_OUTPUT_BUFFER retry with a 64-byte initial buffer, validates the returned OIR_MODULE artifact and verifies malformed input is rejected.
- [ ] OCC CLI handoff and Codegen migration: invoke the lowering stage after Semantic DLL, then migrate Codegen to consume OIR_MODULE instead of the temporary host-materialized AST bridge.

The scalar lowering slice is exercised through both the static lowering API and the independent `occ_oir_lowering_stage` module. The integration test builds a real frontend AST, runs semantic analysis, writes and reopens a validated SEMANTIC_AST artifact, invokes the DLL through `occ_module_loader_invoke()`, and validates the returned OIR_MODULE container and payload. It covers literal return, arithmetic precedence, initialized local reads, reassignment and multiple local SSA versions. A 64-byte initial host buffer exercises the module's output-capacity retry. The module rejects malformed input without publishing output. CI run `41365084763` passed on Windows, Ubuntu and macOS, including Ubuntu ASan/UBSan and Valgrind. The remaining gaps are OCC CLI handoff, Codegen's OIR_MODULE consumption, parameters/calls/multiple functions, conversions, source maps and the full module verifier.

## 7. Decisions still required

- Block-parameter incoming-value ordering and verifier rules, including loop back-edges.
- [x] Initial scalar/control-flow opcode IDs and structural operand schemas are assigned in [OIR_OPCODE_REGISTRY.md](OIR_OPCODE_REGISTRY.md).
- Opcode-specific type constraints, detailed memory effects and full semantic verification rules remain open.
- Canonical numeric type registry and full NEXSUS Flow conversion lattice.
- Integer overflow, floating-point rounding and forced-width behavior at operation boundaries.
- [x] Initial v1 boolean/comparison typing: conditions require `bool`; comparison results are `bool`; equality requires compatible operands and ordered comparisons currently require numeric operands. Full conversion and user-defined comparison rules remain open.
- Function return-type representation in semantic annotations.
- Memory and ownership operations required by the first NEXSUS Flow subset.

These decisions must be made explicitly before the corresponding feature is implemented. Do not copy C semantics by default.


## Array indexing follow-up

AST indexing is implemented as `AST_INDEX_ACCESS`; semantic analysis requires an array-valued target and an integer index and records the indexed expression as an lvalue. OIR lowering does **not** yet emit indexed memory operations. Before enabling it, define the bounds policy (runtime trap vs. proven-safe/elided check), row-major multidimensional offset calculation, index evaluation order, and canonical typed load/store representation. Until these are specified and verified, indexed reads/writes remain outside the executable lowering subset.
