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

1. A canonical `OIR_MODULE` payload writer and reader. Initial schema 1.0 codec is implemented; see [OIR_MODULE.md](OIR_MODULE.md).
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
- [x] Initial `OIR_MODULE` payload codec 1.0: fixed-width little-endian records, bounded reader/writer, UTF-8 and basic cross-reference checks, opcode-shape validation, and zero-copy table views. Implemented in `src/c/oir/oir_module.h/.c` and integrated as `occ_oir_module`.
- [x] AST node-local TLV field lookup and string-span access: `oir_ast_node_table_get_field()` and `oir_ast_node_table_get_string()` let lowering inspect validated node fields and resolve string references without duplicating TLV parsing.
- [ ] Module verifier for function/block ownership, complete instruction coverage, terminators, type rules and SSA.
- [ ] Semantic AST reader + scalar IR lowering pass.
- [ ] Independent IR Lowering DLL and OCC handoff.

The codec and AST field accessor are infrastructure only. No source construct is yet claimed as successfully lowerable; executable lowering remains fail-closed until the first supported subset and its tests are implemented.

## 7. Decisions still required

- Block-parameter incoming-value ordering and verifier rules, including loop back-edges.
- [x] Initial scalar/control-flow opcode IDs and structural operand schemas are assigned in [OIR_OPCODE_REGISTRY.md](OIR_OPCODE_REGISTRY.md).\n- Opcode-specific type constraints, detailed memory effects and full semantic verification rules remain open.
- Canonical numeric type registry and full NEXSUS Flow conversion lattice.
- Integer overflow, floating-point rounding and forced-width behavior at operation boundaries.
- Exact boolean/comparison typing rules.
- Function return-type representation in semantic annotations.
- Memory and ownership operations required by the first NEXSUS Flow subset.

These decisions must be made explicitly before the corresponding feature is implemented. Do not copy C semantics by default.
