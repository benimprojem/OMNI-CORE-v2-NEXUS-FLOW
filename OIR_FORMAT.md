# OIR v1 — OmniCore Interchange Representation

**Status:** Proposed canonical artifact standard; format design is not yet frozen  
**Purpose:** One interoperable artifact contract for compiler-stage handoff and checkpoints  
**Related:** [COMPILER_PIPELINE.md](COMPILER_PIPELINE.md), [DIAGNOSTICS_AND_CONTEXT.md](DIAGNOSTICS_AND_CONTEXT.md)

## 1. Core decision

Every compiler stage exchanges a typed OIR artifact. A stage consumes one or more OIR artifacts, validates their envelope and payload kind/version, and produces a new OIR artifact. The orchestrator connects stage outputs to downstream inputs.

The same artifact representation is used:
- In memory between loaded modules.
- On disk for checkpoints and reproducible debugging.
- Between processes or tools when a serialized artifact is explicitly requested.

A module must not depend on another module's private C structs, allocator, pointers, or compiler-internal object layout.

The canonical file extension for serialized intermediate artifacts is `.oir`. The extension does not identify the payload kind; the artifact header does.

## 2. OIR is a container and a family of typed payload schemas

OIR has two layers:

1. **Container envelope:** common, versioned, platform-independent metadata and length-delimited sections.
2. **Typed payload:** a schema identified by artifact kind and schema version.

This lets the lexer, parser, semantic analyzer and later stages use one transport standard without pretending that tokens, AST nodes and lowered IR are the same data model.

Initial artifact kinds:

| Kind | Producer | Consumer examples |
|---|---|---|
| `SOURCE_UNIT` | source manager/import stage | lexer |
| `TOKEN_STREAM` | lexer | parser, token inspector |
| `AST` | parser | semantic analyzer, AST inspector |
| `SEMANTIC_AST` | semantic analyzer | IR lowering, semantic inspector |
| `OIR_MODULE` | IR lowering | OIR verifier, optimizer |
| `OIR_MODULE` | optimizer pass | next optimizer pass, target lowering |
| `TARGET_IR` | target lowering | backend |
| `ASSEMBLY` | backend | assembler |
| `OBJECT` | assembler | linker |
| `LINK_INPUT` | linker/packager input stage | linker/packager |

The list is extensible. New artifact kinds require a registered stable identifier and a schema version; they must not reuse an existing identifier with changed meaning.

## 3. Container envelope

The v1 envelope is a serialized byte stream, not a dumped C struct. All integer fields have explicitly specified widths and little-endian byte order. Readers must decode fields individually and must not assume host alignment, padding, pointer width or endianness.

The logical header contains:

| Field | Purpose |
|---|---|
| Magic | Identifies the stream as OIR |
| Container major/minor | Container compatibility version |
| Header size | Allows compatible header extension |
| Artifact kind | Identifies the payload schema family |
| Payload schema major/minor | Versions the selected payload |
| Producer stage ID | Identifies the module that produced this artifact |
| Producer module version | Reproducibility/debugging metadata |
| Job ID | Identifies the compilation job |
| Artifact validity | `VALID`, `PARTIAL`, `INVALID`, or `NONE` |
| Flags | Optional features; unknown required flags cause rejection |
| Section count | Number of typed sections |
| Total byte length | Bounds-checking and truncation detection |

The exact byte offsets and numeric IDs will be frozen in a separate binary layout appendix after review. This document intentionally defines the contract before locking offsets.

## 4. Section model

The container body is a sequence of length-delimited sections. Every section has a stable section type, section version, flags and byte length. Readers can skip unknown optional sections but must reject unknown required sections.

Initial logical sections:

- `PAYLOAD`: stage-specific token, AST, semantic AST, IR or target payload.
- `SOURCE_MAP`: source identities and location mapping.
- `DIAGNOSTICS`: diagnostics emitted while producing this artifact.
- `DEPENDENCIES`: imported modules and upstream artifact identities.
- `METADATA`: producer/build options and reproducibility data.
- `CHECKSUM`: optional integrity check over defined byte ranges.

A container must have exactly one primary payload section for its declared artifact kind. Additional sections are optional unless the artifact schema requires them.

## 5. Payload schema rules

- Every payload starts with its schema version and uses fixed-width, explicitly encoded values.
- Strings are UTF-8 with explicit byte lengths; null and empty are distinct.
- Collections have explicit counts or bounded lengths.
- References inside a payload use artifact-local IDs, never process pointers.
- AST node tags, token IDs, OIR opcodes and type IDs are stable numeric identifiers. Removing or renumbering a published ID is a breaking schema change.
- Source spans use 0-based byte offsets and 1-based line/column positions; source identity is explicit.
- Optional fields and extension records must be length-delimited so newer writers do not corrupt older readers.
- Floating-point encoding, NaN handling and canonicalization rules must be specified for each schema that carries floating-point values.
- Payloads must define maximum lengths/counts and reject overflow, truncation, invalid references, invalid tags and unreasonable nesting before allocation.

## 6. Canonical OIR module

`OIR_MODULE` is the machine-independent intermediate representation used after semantic analysis and lowering. It is not the same thing as the parser AST.

The canonical OIR module will describe, at minimum:
- Module identity, imports and exports.
- Functions and basic blocks.
- Typed values and SSA/value references (final SSA policy remains to be designed).
- Operations/opcodes and explicit operands.
- Control-flow edges and terminators.
- Memory access, ownership/lifetime and side-effect semantics.
- Calling convention and ABI-relevant attributes where applicable.
- Source mapping and diagnostic attachment.

The opcode set, type system, memory model, ownership semantics and SSA strategy require a dedicated OIR semantic specification. This document does not invent or freeze them.

Optimizer passes consume and produce the same `OIR_MODULE` schema family, with explicit schema/version compatibility and pass metadata. A pass must not silently change program semantics.

## 7. Pipeline handoff contract

Every stage descriptor declares:
- Accepted artifact kinds and schema-version ranges.
- Whether it accepts `PARTIAL` inputs.
- Produced artifact kind and schema version.
- Whether it can emit a partial artifact.
- Required and optional sections.
- Reentrancy/thread-safety capability.

At invocation, the runner validates the input envelope before calling the module. The module validates the payload details it owns. On return, the runner validates the output envelope and its declared kind/version before connecting it to the next stage.

Example:

```text
source.oir
  SOURCE_UNIT
    -> lexer.dll
tokens.oir
  TOKEN_STREAM
    -> parser.dll
ast.oir
  AST
    -> semantic.dll
semantic.oir
  SEMANTIC_AST
    -> ir_lower.dll
module.oir
  OIR_MODULE
    -> optimizer_*.dll
optimized.oir
  OIR_MODULE
    -> backend.dll
assembly.oir
  ASSEMBLY
    -> assembler.dll
object.oir
  OBJECT
    -> linker.dll
program.oir
  LINK_INPUT / packaged output
```

This is the logical pipeline; exact module boundaries and final executable packaging remain platform/backend-specific.

The runner may keep artifacts in memory for speed. A checkpoint serializes the exact same artifact to disk. A downstream module receives an opaque artifact handle or a read-only byte view defined by the ABI, never a pointer to another module's private AST/IR struct.

## 8. Compatibility and validation

- Container major mismatch: reject.
- Container minor version: accept only when all required features/sections are understood.
- Payload kind mismatch: reject before stage execution.
- Unsupported payload major version: reject.
- Payload minor version: accept only if unknown extensions are safely skippable.
- Unknown required flag/section: reject.
- Invalid, truncated, oversized or internally inconsistent artifact: reject and report a structured diagnostic.
- A stage may accept `PARTIAL` only when it explicitly declares partial-input support.

The runner distinguishes an incompatible/corrupt artifact (infrastructure or artifact validation failure) from source diagnostics embedded in a valid partial artifact.

## 9. Relationship to the current AST serializer

The current C AST serializer is a useful internal round-trip mechanism and test target, but it is not OIR v1:
- It writes native `int` and enum representations.
- It has no container magic, container version, artifact kind, stage identity or schema version.
- It serializes the current parser's AST layout directly.
- It cannot yet serve as a stable cross-platform module boundary.

Keep the existing serializer as a legacy/internal AST format until a versioned OIR AST payload reader/writer is implemented and tested. Do not silently reinterpret old files as OIR v1. A migration/import tool may be added later if old AST artifacts need to be preserved.

## 10. Implementation sequence

1. Review and freeze the container envelope fields and numeric identifiers.
2. Implement a shared OIR container reader/writer with bounds checking and round-trip/fuzz tests.
3. Implement SOURCE_UNIT and TOKEN_STREAM payload schemas.
4. Implement AST payload schema and migrate parser checkpoints.
5. Implement DIAGNOSTICS and SOURCE_MAP sections.
6. Implement SEMANTIC_AST payload and semantic metadata.
7. Design the canonical OIR_MODULE semantics and opcode/type registry in a separate specification.
8. Add stage input/output contract validation to the orchestrator.
9. Migrate each stage to consume/produce OIR artifacts and add compatibility tests.

No module should be split into a public ABI until the artifact and ownership contracts it relies on are testable.
