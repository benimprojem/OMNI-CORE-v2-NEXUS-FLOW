# OIR_MODULE — Lowered Intermediate Representation

**Status:** Opcode registry 1.0, OIR_MODULE payload codec 1.1, independent scalar IR-lowering DLL and independent OIR Verifier DLL are implemented. OCC runs the verifier after lowering and before Codegen. The lowerer currently emits a narrow scalar subset; function signatures, source maps, conversions, calls, multi-function lowering and general control-flow lowering remain future work.  
**Artifact kind:** `OIR_KIND_MODULE` (stable kind ID 5)  
**Producer:** IR lowering stage  
**Input:** one validated `OIR_KIND_SEMANTIC_AST` artifact  
**Output:** one `OIR_KIND_MODULE` artifact  
**Related:** [OIR_FORMAT.md](OIR_FORMAT.md), [OIR_SEMANTIC_AST.md](OIR_SEMANTIC_AST.md), [COMPILER_PIPELINE.md](COMPILER_PIPELINE.md)

## 1. Purpose and boundary

Canonical OIR_MODULE is target-neutral. Physical registers, target stack offsets, concrete ABI register assignments and object-format details belong to target-aware backend stages. The selected target descriptor is a separate validated compilation input as defined in [TARGET_PIPELINE_CONTRACT.md](TARGET_PIPELINE_CONTRACT.md); it must not be silently inferred from the host inside the OIR payload.

`OIR_MODULE` is the first lowered, machine-independent representation of a program. It is not a serialized parser tree and it is not target assembly. Its purpose is to express executable operations, typed values, functions and control flow in a form that can be validated and passed between independent compiler DLLs.

The intended handoff is:

```text
AST
  -> Semantic DLL
SEMANTIC_AST
  -> IR Lowering DLL
OIR_MODULE
  -> OIR Verifier DLL
verified OIR_MODULE
  -> Optimizer DLLs (zero or more)
verified OIR_MODULE
  -> Target Lowering / Backend
TARGET_IR or ASSEMBLY
```

The new lowering stage now exists as an independent shared module: it consumes `SEMANTIC_AST` and emits an OIR container carrying an `OIR_MODULE` 1.1 payload. Its current scalar subset is tested through the host module loader. However, `main.c` does not yet invoke this stage. The current CLI still uses `parser_save_ir()` / `ast_serialize()` and the legacy Codegen DLL's `ast_deserialize()` path. That native-layout AST serializer is an internal compatibility format, not an `OIR_MODULE`. The stage is therefore implemented and independently tested, but the complete OCC lowering handoff is not yet integrated.

## 2. Design principles

1. **Artifact-only boundary.** No AST pointers, semantic context, Slot objects, allocator state or compiler-private C structs cross the DLL boundary.
2. **Explicit typing.** Every value-producing operation has a declared result type. Operand compatibility is checked by the verifier, not guessed by codegen.
3. **Explicit control flow.** Blocks end in a terminator; branches and successors are represented as IDs, not inferred from instruction order.
4. **Stable identities.** Functions, blocks, values, instructions and local tables use artifact-local 1-based IDs. Zero is reserved for null/absent where the field permits it.
5. **Determinism.** Identical semantic input and lowering options produce byte-identical logical tables and deterministic ordering.
6. **Bounded decoding.** Counts, offsets, string spans, table ranges, nesting and cross-references are validated before allocation or exposure.
7. **SSA canonical model.** OIR values use SSA form. The schema remains neutral about physical registers, stack-machine versus register-machine implementation, and target-specific memory placement.
8. **Target independence.** Target register names, physical addresses, instruction encodings and ABI register assignments do not belong in the generic module.
9. **Provenance.** Lowered operations retain source-node identity and optional source-span mapping so diagnostics and debug information remain traceable.
10. **Versioned evolution.** Published IDs are never silently renumbered or reused. New optional records must be safely skippable; semantic changes require a schema-major change.

## 3. Minimal payload v1 implementation boundary

The first codec increment uses a deliberately small, fixed-width table model. It is a foundation for scalar functions, not the final language-wide IR.

The implemented payload schema is version 1.1 and contains a 36-byte little-endian header followed by contiguous tables in this order: types, functions, blocks, values, instructions, operands and UTF-8 strings. Each table count is bounded; the string table is capped at 16 MiB.

| Table | Record size | Initial fields |
|---|---:|---|
| Type | 12 bytes | ID, kind, bit width, type flags |
| Function | 20 bytes | ID, name offset/length, function-type ID, entry-block ID |
| Block | 16 bytes | ID, owner function ID, first instruction, instruction count |

`first instruction` is a zero-based offset into the instruction table (not an instruction ID); `instruction count` bounds the contiguous range. The block, function and instruction identity fields themselves remain 1-based.
| Value | 16 bytes | ID, type ID, defining instruction ID, flags |
| Instruction | 24 bytes | ID, block ID, opcode, result value ID, operand range |
| Operand | 16 bytes | kind, referenced ID, 64-bit immediate |

Initial type kinds are integer, float, boolean and function. Type flags define `OIR_MODULE_TYPE_FLAG_SIGNED` (integer only; unset means unsigned), `OIR_MODULE_TYPE_FLAG_HARD` (forced-width/hard type) and `OIR_MODULE_TYPE_FLAG_CONST` (const qualification). Non-integer types reject the signed flag; hard/const qualifiers are representation-independent. The first semantic primitive mapper is implemented in `oir_type_map.h/.c`: it maps `i8/i16/i32/i64`, `u8/u16/u32/u64`, `f32/f64` and `bool`, preserving hard/const flags. `str`, `char`, `byte`, `void` and `any` are deliberately rejected until their OIR representations are specified. Function types are currently identity placeholders; parameter and return signature tables are a later extension. Table IDs are 1-based and must appear in canonical sequential order. The codec validates schema version, bounded counts, total payload length, UTF-8 strings, table ranges, basic cross-table IDs and opcode operand/result shape before exposing a zero-copy view. The writer serializes fields individually in little-endian order and validates its output through the reader.

The 1.1 codec validates the integer signedness flag and rejects signedness on non-integer kinds; hard/const qualifiers are preserved independently of scalar kind. It deliberately remains a bounded wire decoder: function/block ownership consistency, complete instruction-range coverage, type compatibility, SSA dominance and predecessor edges are checked by the independent OIR Verifier DLL, not by the codec. Function signatures remain incomplete because the current function-type records are identity placeholders.

Aggregate types, full function signatures, block parameters, debug/source maps, memory metadata, entry-point metadata and NEXSUS Flow-specific operations remain later extensions.

## 3. Logical module model

The following sections describe the logical meaning and intended evolution of the tables. The byte-level v1.0 layout is defined in Section 3; later fields must use a versioned extension rather than changing these record widths in place.

### 3.1 Module header and identity

The payload identifies:
- Module schema version and feature flags.
- Module flags (library/executable fragment, partial/debug-capable state as later defined).
- Entry-point function ID, or zero when no entry point is declared.
- Counts and bounded ranges for all tables.
- Optional compilation-unit identity and dependency fingerprints.

The outer OIR header remains authoritative for artifact kind, producer stage, job ID and validity. Payload fields must not duplicate those values with conflicting meanings.

### 3.2 Functions

Each function record should contain:
- Stable function ID.
- Name/string-table reference.
- Function flags (declaration, definition, external/imported, exported, intrinsic, etc.; numeric registry to be defined).
- Function type/signature reference.
- Calling-convention ID and ABI attribute set.
- First/count ranges for its basic blocks and parameters, or explicit table references.
- Entry block ID for a definition; zero for a declaration.
- Optional semantic symbol ID as provenance only.

A function declaration has no body blocks. A function definition has exactly one entry block and a valid block graph. Duplicate definitions and invalid import/export combinations are verifier errors.

### 3.3 Basic blocks and control flow

Each block record should contain:
- Stable block ID and owning function ID.
- Instruction range or explicit ordered instruction references.
- Exactly one terminator reference.
- Optional block flags and source provenance.

Terminators are represented as explicit operations with typed successor operands. Initial control-flow classes to support include:
- Unconditional branch.
- Conditional branch.
- Multi-way selection/switch.
- Return (with optional value).
- Unreachable/trap.

Every successor must identify a block in the same function unless a future, separately specified control-flow construct explicitly permits otherwise. A block must have exactly one terminator, and no ordinary instruction may appear after it. Reachability is a verifier analysis; unreachable blocks are not automatically invalid unless a later policy says so.

### 3.4 Values and types

The value table represents the typed entities used by operations:
- Function parameters.
- Operation results.
- Constants and constant aggregates.
- Explicitly modelled addresses/references when required by an operation.
- Block parameters with predecessor-associated incoming values.

Each value has a stable local ID, a type-table reference, a value-kind tag and a defining relation (parameter, instruction result, constant, or other registered origin). Values are not physical machine registers.

The module owns a self-contained canonical type table. Semantic type IDs and symbol IDs may be retained as provenance links, but must not be treated as live pointers or unresolved references into a separately loaded `SEMANTIC_AST` buffer. Lowering resolves semantic types into module type records before the semantic input is released.

The type system must be able to represent at least:
- Integer and floating-point scalar widths.
- Boolean and character types.
- Pointer/reference/address-space distinctions where the language requires them.
- Arrays, vectors, tuples and aggregate/record layouts.
- Function signatures.
- Enumerations and tagged/untagged aggregate forms.
- Opaque or target-neutral intrinsic types when needed.

**NEXSUS Flow numeric-width behavior already established by the language design:**
- In automatic-width mode, an arithmetic result keeps the current operand/result width when representable; it promotes to the next supported width only when the result does not fit.
- Example: an FP16 × FP16 operation remains FP16 when its result is representable in FP16; otherwise the result is represented at the next supported width (for example FP32).
- A forced-width marker such as `!32` blocks automatic width promotion. It fixes the selected representation width; it does not promise that an out-of-range result cannot overflow.
- Automatic promotion may continue to a wider supported type (for example f32 to f64) when required by the result and the applicable type rules.

The complete promotion lattice is **not yet frozen**. Mixed signed/unsigned integer rules, mixed integer/floating-point rules, literal typing, intermediate precision versus stored result width, overflow behavior in forced mode, and the exact supported-width ordering still require a dedicated NEXSUS Flow type specification. The OIR type registry must encode the resolved conversion explicitly rather than silently imposing C's usual arithmetic conversions.

### 3.5 Operations and operands

An instruction record should contain:
- Stable instruction ID.
- Owning block ID.
- Opcode ID from a separately versioned stable opcode registry.
- Result value ID, or zero for no-result operations.
- Operand range or explicit operand references.
- Operation flags and optional metadata references.
- Source AST node ID and optional source-map reference.

Operand records must encode operand kind explicitly. A value operand references a value ID; a block operand references a block ID; a function operand references a function ID; an immediate operand carries a schema-defined fixed-width or string-table value. No operand may be interpreted by guessing from an integer's numeric range.

The initial opcode IDs and structural descriptors are implemented in [OIR_OPCODE_REGISTRY.md](OIR_OPCODE_REGISTRY.md) and `oir_module_opcode.h/.c`. The registry defines arity, operand kinds, result presence and coarse effect/terminator flags. The independent verifier additionally enforces the implemented type constraints, SSA rules and control-flow validity. Function-call signatures and full memory metadata remain deferred because the current schema does not yet encode those contracts. Unknown executable opcodes are rejected; they must never be silently ignored.

The current scalar lowerer emits typed integer/float/bool constants and recursively lowers `+`, `-`, `*` and `/` expressions. It also supports initialized scalar locals, local identifier reads and straight-line reassignment through a caller-owned symbol-to-SSA-value mapping. Binary nodes are traversed according to the validated AST, so parser precedence/associativity is preserved; each operation produces a new SSA value. The first executable slice is exactly one parameterless, non-extern function with initialized scalar locals, simple reassignment and one final return. This is not complete numeric semantics: implicit promotion, mixed-width conversions, parameters, calls, multiple functions and control flow remain outside the supported subset. Tests cover `ret 10 + 20 * 3;`, `v:x!i32 = 40; x = x + 2; ret x;` and multiple SSA versions.

The first opcode registry should cover the minimum needed to lower the currently supported language subset: constants, typed arithmetic, comparisons, casts/conversions, local reads/writes, calls, returns, branches, conditional branches and explicit memory operations. Intrinsics and concurrency operations must receive explicit semantics before being emitted.

### 3.6 Memory and effects

The base schema must distinguish values from memory locations and must not silently assume that every local is an SSA value. It should allow explicit memory operations and metadata for:
- Load and store.
- Allocation/deallocation or lifetime boundaries.
- Volatile/atomic access where specified.
- Address spaces and alignment.
- Read/write/unknown side-effect classification.
- Calls with declared effect summaries.

The initial memory model, aliasing rules, ownership/lifetime model and concurrency ordering are still open design items. A lowering pass must not invent stronger guarantees than the language semantics provide.

### 3.7 Source mapping and debug provenance

Every instruction may reference its originating AST node ID from the input semantic artifact. Source locations should be carried through the standard OIR `SOURCE_MAP` section or an explicitly versioned module-local mapping. Source offsets remain byte offsets into the exact associated source unit. A module must not fabricate line/column positions when the source map is absent.

Semantic symbol/type IDs retained for diagnostics are provenance only. They are not substitutes for the module's own function/value/type tables.

## 4. IDs, ordering and reference rules

- IDs are 1-based within their table; zero means absent only where documented.
- IDs are unique within a table and stable for the lifetime of one artifact.
- IDs are not promised to remain numerically identical after an optimizer rewrites the module; provenance/source identity must survive transformations where possible.
- All cross-table references must resolve and must point to the expected record kind.
- Table ranges use checked arithmetic; overlapping ranges are rejected unless the specific table contract explicitly allows shared slices.
- Functions, blocks, values, instructions and strings have deterministic canonical ordering.
- String references are byte offsets/lengths into a UTF-8 string table; no implicit NUL terminators.
- No native C struct dump, pointer, host-sized `size_t`, compiler enum ordinal or implicit padding is permitted on disk.

## 5. Validation layers

The OIR container reader validates the common envelope. The independent `occ_oir_verify_stage` DLL validates the following module-level invariants before the artifact can be passed to Codegen or a future optimizer:

1. Payload schema version, flags and exact declared length.
2. Table counts, record sizes, offset ranges and string-table boundaries.
3. Unique IDs and all cross-table references.
4. Function ownership, entry blocks and declaration/definition rules.
5. Instruction ownership and ordering within blocks.
6. Exactly one terminator per defined block and valid successor edges.
7. Opcode existence, arity, operand kinds and result rules.
8. Type references and opcode-specific type constraints.
9. Function references and call operand shape; full parameter/return signature matching is deferred until function signature tables exist.
10. Source-node references against the preserved semantic/AST provenance map when available.
11. Feature flags and required extensions supported by the consumer.

Validation must complete before a consumer exposes the module to optimization or code generation. A malformed module is rejected; the verifier must not repair it silently.

### Implemented verifier stage 1.0

The independent shared module is `occ_oir_verify_stage` (stable module ID `0x00010006`, diagnostic stage ID `OCC_STAGE_OIR_VERIFY`). It accepts exactly one valid OIR_MODULE 1.1 artifact produced by OIR Lowering, validates it, and republishes the same module sections with producer stage `OCC_STAGE_OIR_VERIFY`. It does not rewrite or repair the payload. The OCC host invokes it immediately after lowering; Codegen now rejects OIR_MODULE artifacts whose producer is not the verifier stage.

The implemented checks are:

- Unique, valid function/block ownership; every function entry block belongs to that function.
- Non-overlapping block instruction ranges that cover the instruction table exactly; instruction block IDs match their containing block.
- Every block ends in exactly one terminator; no instruction follows a terminator.
- Branch targets remain within the owning function.
- Opcode IDs, operand shape, result presence and the implemented opcode-specific type constraints.
- Every SSA value has one matching defining instruction; same-block uses follow their definition, and cross-block definitions dominate their uses.
- Reachable CFG blocks are analyzed with iterative dominators. Unreachable blocks are not rejected solely for being unreachable; their SSA uses are restricted to definitions in the same block because they cannot establish cross-block dominance.
- The verifier rejects unsupported schema versions, invalid producer/job identity, duplicate/missing payloads, and malformed or inconsistent artifacts.

The first implementation bounds the dominator analysis to **8,192 blocks per module** and the function/instruction/value tables to **1,000,000 records** each. The OIR_MODULE codec separately caps its string table at **16 MiB**. These are explicit implementation/resource limits, not source-language limits. Full function signatures, parameter typing, block parameters/phi nodes, complete memory semantics, source-map cross-checking and advanced effect validation remain future schema/verifier increments.

## 6. SSA and lowering policy

The NEXSUS Flow compiler architecture documents explicitly define Omni-IR as SSA-based. Therefore, **SSA is the intended canonical value model for OIR_MODULE**, rather than an undecided optional mode.

The exact SSA merge representation remains open:
- Conventional `phi` instructions.
- Block parameters with predecessor-associated incoming values.
- Another explicitly specified equivalent form.

The selected form must define dominance, use-before-definition, loop back-edge handling, edge-specific incoming values and verifier behavior. SSA values have one definition; mutable source variables are lowered into new SSA values and explicit memory operations only when their semantics require addressable storage. Physical target registers remain a later register-allocation concern.

The older OIR description of a DAG is interpreted as a restriction on ordinary, non-loop-carried instruction dependencies—not as a requirement that the whole program or its control-flow graph be acyclic. The control-flow graph (CFG) may contain cycles for loops, and functions may be recursive. Loop-carried values are represented explicitly as block parameters with incoming values supplied by the loop's preheader and back-edge; those back-edge relationships are not mistaken for ordinary acyclic expression dependencies. Within a basic block, the schedulable instruction-dependency graph must be acyclic. This distinction preserves dependency ordering without making loops impossible.

## 7. Calling convention and ABI policy

Calling convention is explicit metadata, not inferred from the host running the compiler. The module records a stable calling-convention identifier and generic function attributes. Target-specific lowering maps these to a target ABI descriptor.

The generic schema must not hard-code x86-64 registers, Windows shadow space, SysV register order, stack offsets or NEXSUS hardware registers. Those belong to target-specific lowering/backend contracts. The initial registry must include a well-defined language-internal convention and explicit external ABI identities only when their semantics are documented.

## 8. Optimizer contract

An optimizer consumes a validated `OIR_MODULE` and produces another validated `OIR_MODULE`. It declares:
- Accepted schema major/minor and required feature flags.
- Whether it preserves source mappings and debug metadata.
- Whether it may add/remove/reorder functions, blocks, values and instructions.
- Which semantic properties it preserves.
- Whether it requires SSA, a particular memory model or other optional features.

Pass order is orchestrator policy. Every pass output is revalidated. A pass that cannot preserve a required section must reject the input or explicitly report the dropped optional metadata; it must not silently erase required provenance.

## 9. Versioning and implementation sequence

The design sequence is:

1. Finalize a minimal language subset and representative source fixtures.
2. [x] Assign the initial scalar/control-flow opcode IDs and implement structural registry lookup; complete type/effect semantics in the verifier.
3. Specify and implement dominance, loop-carried values and predecessor-edge verification for the selected block-parameter SSA model.
4. Define canonical type records and the complete NEXSUS Flow numeric-promotion/conversion lattice, including forced-width behavior and unresolved mixed-type cases.
5. Define function, block, instruction, operand and source-map wire layouts.
6. Implement a standalone module writer/reader and verifier with malformed-input, overflow, cross-reference and deterministic round-trip tests.
7. [x] Implement IR lowering as its own shared stage module: `SEMANTIC_AST -> OIR_MODULE` (independent module test passes; not yet invoked by OCC CLI).
8. Switch OCC to pass the semantic artifact directly to lowering; remove the legacy AST serialization handoff only after parity tests pass.
9. Migrate codegen to consume validated `OIR_MODULE`; keep the AST-walking codegen only as a temporary compatibility path.
10. Add optimizer DLLs after the module verifier and opcode semantics are stable.

No schema is considered frozen merely because a first implementation or test fixture exists.

## 10. Current implementation status

| Component | Status |
|---|---|
| OIR container kind ID 5 (`OIR_KIND_MODULE`) | Registered |
| Generic OIR container reader/writer | Implemented |
| OIR_MODULE payload codec 1.1 (signed, hard and const type flags) | Implemented |
| Independent OIR Verifier stage 1.0 (CFG, type subset and SSA dominance) | Implemented; OCC host handoff active |
| SEMANTIC_AST primitive-to-OIR type mapper | Initial scalar allowlist implemented; unsupported types rejected |
| SSA value model | Required by NEXSUS Flow design; merge encoding open |
| NEXSUS Flow numeric width promotion | Partial language rule established; full conversion lattice open |
| OIR_MODULE opcode registry 1.0 / primitive type mapping | Initial registries implemented; complete semantic type rules pending |
| Module payload verifier | Implemented for the documented 1.0 subset; function signatures/memory semantics remain deferred |
| Initial SEMANTIC_AST scalar lowering pass | Implemented as a caller-workspace C pass; typed literals, arithmetic, initialized locals, identifier reads and straight-line reassignment supported |
| IR lowering DLL | Implemented and tested independently; OCC CLI handoff pending |
| OCC semantic-artifact-to-lowering handoff | Not implemented |
| Legacy AST serializer and AST-walking codegen | Existing compatibility path |

The legacy codegen currently emits a small NASM-like text subset directly from AST nodes. It is not yet a general backend and must not be treated as proof that the module IR is implemented.

### Initial lowering slice

`oir_lower_semantic_ast_return()` consumes a validated `OirSemanticAstArtifactView` and emits an `OIR_MODULE` payload. The independent `occ_oir_lowering_stage` wraps that payload in a complete OIR artifact container. The current accepted shape is exactly one non-extern, parameterless function with one block, initialized scalar locals, simple reassignment to earlier mutable locals and one final return. Expressions may contain annotated `i*`, `u*`, `f32`, `f64` or `bool` literals, local identifier reads and the active parser's `+`, `-`, `*` and `/` operators. The lowerer cross-checks the function's declared return spelling against the canonical semantic type record, maps supported primitives through the explicit allowlist, traverses expression trees in postorder and emits typed constants, arithmetic SSA values and a return instruction. The older `oir_lower_semantic_ast_literal_return()` entry point remains available for literal-only callers.

The pass uses caller-owned arrays and buffers. It rejects extra functions, parameters, unsupported statements, missing annotations, non-primitive/unregistered types and unsupported literal categories. The module stage validates the semantic artifact and graph before lowering, allocates per-invocation workspaces, enforces a 65,536 AST-node stage limit and publishes only a structurally valid OIR_MODULE artifact after output capacity is sufficient. The `test_oir_lowering.c` test loads the real shared module through `occ_module_loader`, exercises the 64-byte output-buffer retry, validates the OIR envelope and module payload, and verifies malformed input rejection. CI run `41365084763` passed on Windows, Ubuntu and macOS, including Ubuntu ASan/UBSan and Valgrind. The independent verifier and its OCC host handoff are now implemented. Source maps, conversions, parameters, calls, multiple functions and general control-flow lowering remain pending.
