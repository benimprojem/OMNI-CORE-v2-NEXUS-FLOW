# SEMANTIC_AST Consumer Contract

**Status:** Draft implementation contract for the NEXSUS Flow semantic-stage output.

This document defines the artifact contract that a downstream DLL must consume. It is a wire-format contract, not permission to depend on C structure layouts or on the semantic DLL's private memory.

## 1. Producer and identity

A valid semantic output must satisfy all of the following:

- OIR artifact kind: `OIR_KIND_SEMANTIC_AST`.
- Producer stage: `OCC_STAGE_SEMANTIC`.
- Payload schema: `OIR_SEMANTIC_AST_SCHEMA_MAJOR.MINOR` (currently 1.0).
- Artifact validity: `OIR_VALIDITY_VALID`.
- Artifact `job_id` must equal the invocation/job identity assigned by the host.
- The embedded AST payload is the exact AST payload accepted by the semantic stage; semantic analysis annotates it rather than replacing its node identity space.

A consumer must reject a mismatched kind, producer, schema, validity, or job identity before using any semantic record.

## 2. Section contract

The following sections are mandatory and unique. Each uses section version 1 and must carry `OIR_SECTION_FLAG_REQUIRED`:

| Section | Meaning |
|---|---|
| `OIR_SECTION_PAYLOAD` | Embedded AST node table and field data |
| `OIR_SECTION_SEMANTIC_SCOPES` | Scope IDs, parent scope, depth, flags and owning AST node |
| `OIR_SECTION_SEMANTIC_TYPES` | Stable-in-artifact type IDs and referenced type/symbol metadata |
| `OIR_SECTION_SEMANTIC_SYMBOLS` | Symbol IDs, scope/declaration node, kind, flags and type |
| `OIR_SECTION_SEMANTIC_ANNOTATIONS` | Per-node scope, resolved symbol/type and value category |
| `OIR_SECTION_SEMANTIC_RELATIONS` | Ordered relationships such as function parameters |

`OIR_SECTION_SEMANTIC_CONVERSIONS` is optional. If present, it uses version 1; it is not required for a consumer that does not need explicit conversion records.

Unknown optional sections may be skipped. Unknown required sections must cause rejection. Duplicate known sections, missing mandatory sections, unsupported versions, or incorrect required flags must cause rejection.

## 3. Validation before consumption

The consumer must validate the complete artifact before interpreting semantic data:

1. Open and validate the OIR envelope and section boundaries.
2. Open the embedded AST node table.
3. Validate AST field schema and graph ownership/reachability/cycle constraints.
4. Open each semantic section with its corresponding OIR codec.
5. Validate cross-table references: scope parents and owners, type references, symbol scopes/types, annotation node/scope/symbol/type references, relation owners and targets, and optional conversion type references.

The canonical gate is `oir_semantic_ast_artifact_open()`. It performs these checks using caller-owned scratch arrays. A consumer must not skip this gate and assume that a valid outer OIR header makes the inner tables trustworthy.

## 4. Memory and lifetime

- Artifact bytes are host-owned and immutable for the invocation.
- The `OirSemanticAstArtifactView` and all section views borrow pointers into those bytes. They become invalid when the artifact buffer is released or changed.
- The open/validation API does not allocate heap memory. The caller supplies scratch arrays with capacities of at least:
  - `ast.node_count` entries for incoming-edge counts;
  - `ast.node_count` bytes for visited flags;
  - `ast.node_count` entries for the AST graph stack;
  - `ast.node_count + 1` entries for owner-scope mapping.
- A consumer must not retain artifact or view pointers after its DLL invocation returns. If data must survive, copy it into memory owned by the consumer/host under the documented allocator rules.
- Do not serialize or compare native C struct memory. Use the OIR section codecs; wire records are explicitly encoded and versioned.

## 5. IDs and ordering

IDs are scoped to one artifact. They are not process-global handles and must not be reused across jobs without resolving them in the corresponding artifact. Consumers must use explicit IDs and relation ordinals rather than assuming that C array positions are permanent external identities. Where a codec exposes records in canonical order, preserve that order when producing deterministic downstream artifacts.

## 6. Stage invocation and publication

- The host passes one complete AST OIR artifact as the semantic stage's sole input; its job ID must match the invocation job ID.
- The semantic stage returns one complete SEMANTIC_AST artifact only on `OCC_MODULE_STATUS_OK`.
- `OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER` is a retry request. The host may repeat the invocation with the same input and a larger output buffer.
- The stage must not publish diagnostics or other externally visible side effects before output capacity is sufficient. Diagnostics are reported only after successful artifact writing, or once on a terminal failure that will not request a retry.
- A consumer must treat output as unpublished unless the module result says success, `output_validity == OIR_VALIDITY_VALID`, and `bytes_written` identifies the complete artifact.

This contract does not define IR lowering semantics, machine code layout, or linker behavior. Those are separate downstream specifications.
