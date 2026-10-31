# OIR v1 — OmniCore Interchange Representation

**Status:** Proposed canonical artifact standard; format design is not yet frozen  
**Purpose:** One interoperable artifact contract for compiler-stage handoff and checkpoints  
**Related:** [COMPILER_PIPELINE.md](COMPILER_PIPELINE.md), [DIAGNOSTICS_AND_CONTEXT.md](DIAGNOSTICS_AND_CONTEXT.md), [OIR_SEMANTIC_AST.md](OIR_SEMANTIC_AST.md)

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

### Frozen v1.0 byte layout (initial implementation)

All offsets below are byte offsets from the beginning of the container. All multibyte integers are unsigned little-endian. There is no implicit alignment or padding.

**Fixed header: 44 bytes**

| Offset | Width | Field | v1.0 rule |
|---:|---:|---|---|
| 0 | 4 | Magic | ASCII `OIR1` |
| 4 | 2 | Container major | `1` |
| 6 | 2 | Container minor | `0` |
| 8 | 4 | Header size | `44` for v1.0 writer; readers may skip a larger extension header |
| 12 | 4 | Artifact kind | Stable ID from table below |
| 16 | 2 | Payload schema major | Must be non-zero |
| 18 | 2 | Payload schema minor | `0` initially |
| 20 | 4 | Producer stage ID | `0` means external/unspecified producer |
| 24 | 1 | Artifact validity | ID from validity table below |
| 25 | 1 | Header flags | Must be zero in v1.0 |
| 26 | 2 | Section count | Must be at least one |
| 28 | 8 | Total container length | Exact serialized byte count |
| 36 | 8 | Job ID | Opaque unsigned job identifier; zero means unspecified |

**Section header: 16 bytes**

| Offset (relative to section start) | Width | Field |
|---:|---:|---|
| 0 | 2 | Section type |
| 2 | 2 | Section version (must be non-zero) |
| 4 | 4 | Section flags |
| 8 | 8 | Section payload length in bytes |

Section payload bytes follow immediately. The next section starts immediately after the declared payload length. No padding is inserted. Exactly one section of type `PAYLOAD` is required. Container readers must validate the complete section chain and reject trailing bytes.

**Initial stable IDs**

| Artifact kind ID | Kind |
|---:|---|
| 1 | SOURCE_UNIT |
| 2 | TOKEN_STREAM |
| 3 | AST |
| 4 | SEMANTIC_AST |
| 5 | OIR_MODULE |
| 6 | TARGET_IR |
| 7 | ASSEMBLY |
| 8 | OBJECT |
| 9 | LINK_INPUT |

| Validity ID | State |
|---:|---|
| 1 | VALID |
| 2 | PARTIAL |
| 3 | INVALID |

`NONE` is a pipeline execution result meaning no artifact was produced; it is not a serializable artifact validity. A serialized OIR container always has exactly one primary payload and therefore may be VALID, PARTIAL or INVALID.

| Section type ID | Section |
|---:|---|
| 1 | PAYLOAD |
| 2 | SOURCE_MAP |
| 3 | DIAGNOSTICS |
| 4 | DEPENDENCIES |
| 5 | METADATA |
| 6 | CHECKSUM (reserved; checksum algorithm/coverage not yet standardized) |
| 7 | SEMANTIC_SCOPES |
| 8 | SEMANTIC_TYPES |
| 9 | SEMANTIC_SYMBOLS |
| 10 | SEMANTIC_ANNOTATIONS |
| 11 | SEMANTIC_RELATIONS |
| 12 | SEMANTIC_CONVERSIONS (optional in SEMANTIC_AST 1.0) |

Section flag bit 0 means REQUIRED. Unknown optional section types may be skipped using their length. Unknown required section types are rejected. Unknown section flag bits and non-zero header flags are rejected by v1.0 readers. Container major must equal 1; minor versions greater than 0 are rejected until compatibility behavior is explicitly implemented.

The C implementation is in `omnicore-sdk/src/c/oir/`. It provides a bounded in-memory writer, a validating read-only view and section access. The stable section IDs 7–12 are registered for SEMANTIC_AST schema 1.0; each section codec performs additional schema-specific validation. Filesystem checkpoint helpers are not part of the container layer.

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
- Typed SSA values and explicit value references.
- Operations/opcodes and explicit operands.
- Control-flow edges and terminators.
- Memory access, ownership/lifetime and side-effect semantics.
- Calling convention and ABI-relevant attributes where applicable.
- Source mapping and diagnostic attachment.

The opcode set, canonical type system, memory model and ownership semantics require dedicated OIR semantic specifications. The SSA direction is established: `OIR_MODULE` uses SSA as its canonical value model. The exact merge encoding (phi instructions or block parameters), opcode registry and detailed memory/effect rules remain open. See [OIR_MODULE.md](OIR_MODULE.md) for the current design contract.

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
7. Define the canonical OIR_MODULE contract in [OIR_MODULE.md](OIR_MODULE.md), then finalize its opcode/type registries and verifier after representative lowering cases are selected.
8. Add stage input/output contract validation to the orchestrator.
9. Migrate each stage to consume/produce OIR artifacts and add compatibility tests.

No module should be split into a public ABI until the artifact and ownership contracts it relies on are testable.
