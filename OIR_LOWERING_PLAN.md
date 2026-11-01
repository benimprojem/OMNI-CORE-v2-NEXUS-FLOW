# OIR Lowering — staged implementation plan

**Status:** Implementation in progress; codec foundation and AST field access are implemented, executable lowering coverage is not yet enabled  
**Input:** validated `OIR_KIND_SEMANTIC_AST`  
**Output:** validated `OIR_KIND_MODULE`  
**Module:** independent IR Lowering DLL/SO/dylib, invoked by OCC  
**Related:** [OIR_MODULE.md](OIR_MODULE.md), [OIR_SEMANTIC_AST.md](OIR_SEMANTIC_AST.md), [COMPILER_PIPELINE.md](COMPILER_PIPELINE.md)

## 1. Purpose

This plan defines a safe route from the current semantic artifact to canonical OIR. It does not treat the existing AST-walking code generator as a design constraint. The lowering stage will consume only the validated semantic artifact and produce a target-independent SSA module.

The first implementation must be deliberately small and verifiable. A construct is either lowered with fully defined semantics or rejected with a structured diagnostic. Unsupported AST nodes must never be silently skipped, approximated or emitted as no-ops.

## 2. Preconditions before executable lowering

The following infrastructure is required before the first production lowering DLL is integrated:

1. A canonical `OIR_MODULE` payload writer and reader. Schema 1.1 codec, including explicit integer signedness, is implemented; see [OIR_MODULE.md](OIR_MODULE.md).
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
- No parameters and no local declarations.
- Exactly one return statement in its body.
- The returned expression may be one supported scalar literal, or a recursively supported arithmetic expression whose operands are supported scalar literals.
- Return type and expression type must be established by the semantic artifact and must map to a currently registered OIR primitive type.
- The function body must contain no additional executable statement; comments or source trivia are not AST statements and are not represented here.

This slice is a bring-up target for the artifact-to-module path, not a claim that variables, identifiers, parameters, calls, comparisons, control flow, or the complete numeric promotion rules are supported. The lowerer must reject multiple functions, parameters, declarations, unknown literal categories, unsupported operators, missing type annotations, unresolved type names, and any extra body statement.

The AST schema preserves the declared return-type spelling, while the semantic tables preserve canonical type records. Lowering must cross-check both sources; it must not infer a canonical type solely from the spelling. The initial implementation must also reject a semantic type whose primitive width/signedness mapping is not explicitly registered. This avoids accidentally treating the C frontend's current type spelling rules as the final NEXSUS Flow numeric contract.

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
- [ ] Scalar IR lowering pass consuming the validated artifact view.
- [ ] Independent IR Lowering DLL and OCC handoff.

The codec, AST accessors and complete Semantic AST artifact reader are infrastructure only. No source construct is yet claimed as successfully lowerable; executable lowering remains fail-closed until the first supported subset and its tests are implemented.

## 7. Decisions still required

- Block-parameter incoming-value ordering and verifier rules, including loop back-edges.
- [x] Initial scalar/control-flow opcode IDs and structural operand schemas are assigned in [OIR_OPCODE_REGISTRY.md](OIR_OPCODE_REGISTRY.md).
- Opcode-specific type constraints, detailed memory effects and full semantic verification rules remain open.
- Canonical numeric type registry and full NEXSUS Flow conversion lattice.
- Integer overflow, floating-point rounding and forced-width behavior at operation boundaries.
- Exact boolean/comparison typing rules.
- Function return-type representation in semantic annotations.
- Memory and ownership operations required by the first NEXSUS Flow subset.

These decisions must be made explicitly before the corresponding feature is implemented. Do not copy C semantics by default.
