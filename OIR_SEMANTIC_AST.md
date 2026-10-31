# OIR SEMANTIC_AST — Draft schema 1.0

**Status:** Design draft; not frozen  
**Artifact kind:** `OIR_KIND_SEMANTIC_AST` (reserved stable kind ID 4)  
**Producer stage:** `OCC_STAGE_SEMANTIC`  
**Input:** one validated `OIR_KIND_AST` artifact  
**Output:** one `OIR_KIND_SEMANTIC_AST` artifact

This document defines the semantic handoff contract. It deliberately separates the wire format from the current C implementation's `SemanticContext`, `Slot`, `FieldInfo`, pointer values and enum layouts.

## 1. Design goals

1. Preserve the parser's AST node IDs and AST payload byte-for-byte.
2. Make symbol resolution, scopes and inferred/declared types explicit data, not transient linked-list state.
3. Let Codegen consume the semantic artifact without depending on the Semantic DLL's allocator, AST pointers or private structs.
4. Keep deterministic output for identical input and compiler options.
5. Validate all counts, ranges, references and strings before exposing a view or allocating frontend objects.
6. Allow compatible future additions without changing existing field meanings.
7. Keep diagnostics invocation-scoped and host-owned through the existing module diagnostic sink.

The semantic stage must not rewrite or renumber AST nodes. A semantic annotation references the stable 1-based node ID from AST schema 1.0.

## 2. Container composition

The outer container remains OIR v1.0:

- Header artifact kind: `OIR_KIND_SEMANTIC_AST`.
- Payload schema: major 1, minor 0 for this draft.
- Producer stage: semantic.
- Job ID: exactly the input AST job ID.
- Validity: `VALID` only when the stage has no output-blocking diagnostics. `PARTIAL` is reserved until recovery semantics are implemented and tested. No artifact is published on a failed invocation.

Sections:

| Section | Required | Content |
|---|---:|---|
| `OIR_SECTION_PAYLOAD` | yes | The exact AST schema 1.0 node-table payload copied from the input AST artifact. |
| `OIR_SECTION_SEMANTIC_SCOPES` | yes | Scope table, including root scope. |
| `OIR_SECTION_SEMANTIC_TYPES` | yes | Canonical type table referenced by symbols and annotations. |
| `OIR_SECTION_SEMANTIC_SYMBOLS` | yes | Declared symbols and their stable IDs. |
| `OIR_SECTION_SEMANTIC_ANNOTATIONS` | yes | Per-node scope, symbol and type resolution results. |
| `OIR_SECTION_SEMANTIC_RELATIONS` | yes | Ordered function parameter, aggregate field and enum-variant relationships. |
| `OIR_SECTION_SEMANTIC_CONVERSIONS` | optional | Explicit conversion/promotion decisions. Empty/absent means no conversion was inserted or recorded; it does not imply that promotion rules are complete. |
| `OIR_SECTION_DIAGNOSTICS` | optional | Serialized semantic diagnostics for checkpoint/replay inspection. The host callback remains authoritative for the live job collector. |

The semantic section IDs are registered additions after the previously assigned OIR section IDs 1–6: SCOPES=7, TYPES=8, SYMBOLS=9, ANNOTATIONS=10, RELATIONS=11 and CONVERSIONS=12. A reader rejects unknown required sections and may skip unknown optional sections.

The AST payload is embedded as the payload section, not as a nested OIR container. This lets the existing AST node-table reader consume the exact payload slice and avoids duplicating the 44-byte OIR header.

## 3. Wire encoding rules

- All integers are unsigned or signed fixed-width values explicitly named by the field; little-endian.
- No native C struct, pointer, `size_t`, C enum, padding or host ABI layout is serialized.
- Each section has a schema version in its OIR section header.
- Tables begin with a fixed-width little-endian schema header. The SCOPES section uses an 8-byte header (`record_count u32`, `record_size u32`) followed by fixed-width records. String-bearing tables additionally declare their string-table byte length in their table header. TYPES and SYMBOLS use a 12-byte header (`record_count u32`, `record_size u32`, `string_table_length u32`) followed by their fixed-width records and the exact UTF-8 string table. TYPES records are 24 bytes; SYMBOLS records are 36 bytes. ANNOTATIONS uses an 8-byte header (`record_count u32`, `record_size u32`) followed by 20-byte records. RELATIONS uses a 12-byte header (`record_count u32`, `record_size u32`, `string_table_length u32`) followed by 32-byte records and a UTF-8 string table. CONVERSIONS uses an 8-byte header (`record_count u32`, `record_size u32`) followed by 20-byte records. All header and record fields are encoded individually; section versions are carried by the outer OIR section header.
- Variable strings are UTF-8 byte spans with explicit 32-bit offset and length into a section-local string table. No NUL terminator is required; embedded NUL and invalid UTF-8 are rejected.
- IDs are 1-based. Zero is the only null/not-present reference where the field permits it.
- IDs are deterministic for a given AST and options: scopes are assigned in deterministic traversal order; symbols in declaration order within traversal; types in first-use order after canonicalization; annotations in AST node-ID order.
- Counts and byte ranges are bounded by the containing section. Arithmetic overflow, overlapping record ranges, duplicate IDs, dangling references and non-canonical ordering are rejected.
- Unknown record flags are rejected unless a future schema explicitly defines an optional extension mechanism.

The implemented codec slices fix SCOPES at an 8-byte header plus 20-byte records, TYPES at a 12-byte header plus 24-byte records and a trailing string table, SYMBOLS at a 12-byte header plus 36-byte records and a trailing string table, ANNOTATIONS at an 8-byte header plus 20-byte records, RELATIONS at a 12-byte header plus 32-byte records and a trailing string table, and CONVERSIONS at an 8-byte header plus 20-byte records. Logical C structs are API views only; they are never serialized by dumping their memory.

## 4. Scope records

Each scope record describes one lexical scope.

| Field | Wire type | Meaning |
|---|---|---|
| scope_id | u32 | Stable nonzero ID. |
| parent_scope_id | u32 | Parent ID; zero only for the root scope. |
| depth | u32 | Lexical depth; root is zero. |
| flags | u32 | Reserved; zero in schema 1.0. |
| owner_node_id | u32 | AST node that creates/owns the scope; zero for root. |

Invariants:
- Exactly one root scope exists; it has ID 1, parent 0 and depth 0.
- Every non-root parent exists and has depth exactly one less.
- Every non-root scope has a valid owner node ID.
- The scope graph is a tree with no cycles or duplicate ownership.
- Scope records remain after leaving a scope; symbol visibility/lifetime is represented by scope ownership, not by retaining the current implementation's popped `Slot*` list.

### Implemented codec slice

`omnicore-sdk/src/c/oir/oir_semantic_ast.h/.c` currently implements the SCOPES section's bounded writer, zero-copy reader view and field-by-field record decoder. It validates the exact payload length, record width, contiguous 1-based IDs, the unique root shape, parent-before-child ordering, depth consistency, zero schema-1.0 flags and the 512-level depth limit. The test `test_oir_semantic_ast.c` covers round-trip, deterministic bytes, truncation, bad record width/count, invalid root/parent/depth/flags and output-buffer failure. The cross-table validator now checks scope-owner node existence and uniqueness, symbol scope/declaration/type references, declaration node kinds, type-to-symbol references, annotation node/scope/type/symbol references and lexical visibility, relation owner/symbol/type/node references and relation node kinds, symbol relation slices, and conversion node/type references. The lower-level validator accepts already-opened local table views plus a validated AST node-table view and caller-owned scratch storage; the optional CONVERSIONS view may be absent. `oir_semantic_ast_artifact_open()` is the complete artifact gate: it checks the OIR kind/schema, `OCC_STAGE_SEMANTIC` producer and `VALID` validity, exactly one required PAYLOAD and each mandatory semantic section, section versions/required flags, the embedded AST node table, AST field schema and graph, and then all cross-table references. It rejects unconsumed required extensions while skipping optional ones. All graph and scope-owner scratch buffers are caller-owned; returned views borrow the original artifact buffer. The TYPES codec now provides field-by-field encoding/decoding, bounded 16 MiB string-table validation, UTF-8 checks, canonical IDs, registered kind/flag validation and earlier-type reference checks. The same test covers round-trip, deterministic bytes, malformed record widths/counts, duplicate IDs, invalid string offsets/UTF-8, invalid kinds, missing user-type symbol references, cyclic base references and output-buffer failure. SYMBOLS adds corresponding checks for scope/declaration IDs, symbol kinds/flags, name spans and relation-range overflow. ANNOTATIONS tests ascending node IDs, required scope IDs, value-category/flag validation, truncation, capacity failures and valid empty tables. RELATIONS tests canonical owner/kind/ordinal ordering, relation kinds/flags, required AST-node references, optional name spans/UTF-8, truncation, capacity failures and empty tables. CONVERSIONS tests canonical node/operand ordering, registered conversion kinds, exclusive implicit/explicit flags, nonzero type references, truncation, capacity failures and empty tables. `test_oir_semantic_ast_refs.c` constructs a mixed AST/semantic fixture and verifies valid cross-table resolution, optional conversions, duplicate scope owners, dangling scope/type/symbol references, declaration and relation node-kind mismatches, sibling-scope invisibility, incorrect relation slices and out-of-range conversion types. `test_oir_semantic_ast_artifact.c` verifies complete artifact acceptance, empty semantic tables, optional conversions/extensions, missing/duplicate required sections, unsupported section versions, missing required flags, unconsumed required extensions, invalid artifact kind/validity and malformed embedded AST payloads.

## 5. Type records

Type records make the current string-based type system transportable without exposing C pointers.

| Field | Wire type | Meaning |
|---|---|---|
| type_id | u32 | Stable nonzero ID. |
| kind | u16 | Primitive, user-defined, function, aggregate, tuple, inferred/unknown or reserved future kind. |
| flags | u16 | Type qualifiers; schema 1.0 defines const/hard-type only where applicable. |
| name_offset | u32 | Offset into type string table. |
| name_length | u32 | UTF-8 byte length. |
| referenced_symbol_id | u32 | User-defined type declaration; zero for built-ins. |
| base_type_id | u32 | Newtype/alias base type; zero when not applicable. |

The initial type-name registry must explicitly list all accepted primitive names. User-defined types reference their symbol record; the string name is retained for diagnostics and tooling, not used as the only identity. Type IDs must not depend on process addresses or hash-table iteration order.

Schema 1.0 type-kind IDs are: 1 primitive, 2 user-defined, 3 function, 4 aggregate, 5 tuple, 6 inferred and 7 unknown. Type flag bit 0 is const and bit 1 is hard-type; all other bits are rejected. The TYPES codec requires contiguous 1-based IDs, non-empty names to resolve to valid UTF-8 without embedded NUL, user-defined types to carry a nonzero referenced symbol ID, non-user-defined types to carry zero there, and base-type references to point to an earlier type ID.

Schema 1.0 symbol-kind IDs are: 1 variable, 2 function, 3 struct, 4 union, 5 enum, 6 newtype and 7 label. Symbol flag bits 0–4 are const, public, exported, packed and hard-type respectively. The SYMBOLS codec requires contiguous 1-based IDs, nonzero scope and declaration-node IDs, non-empty valid UTF-8 names, registered kinds/flags, and a valid zero-or-range pair for the relation slice. Duplicate-name/namespace rules remain subject to language namespace decisions. Cross-table scope/type/AST-node references are validated by `oir_semantic_ast_validate_references()`. Annotations use value-category IDs 0 unspecified, 1 lvalue, 2 rvalue and 3 function designator; their flags are zero in schema 1.0. Annotation node IDs must be nonzero, unique and strictly ascending, and their scope IDs must be nonzero. Symbol/type references may be zero for unresolved or inapplicable values. RELATIONS kind IDs are 1 function parameter, 2 aggregate field, 3 enum variant and 4 enum payload field. Relations are sorted by owner symbol, kind and zero-based ordinal; ordinals start at zero and are contiguous within each owner/kind pair. Owner symbol and related AST node IDs are nonzero; related symbol and type IDs may be zero when not applicable. Relation names may be empty; non-empty names must be valid UTF-8. CONVERSIONS kind IDs are 1 identity, 2 widening, 3 narrowing, 4 signedness, 5 explicit cast and 6 hard-type enforcement. Exactly one conversion flag must be set: bit 0 implicit or bit 1 explicit. Conversion records are sorted by AST node ID then strictly ascending operand ordinal; node/source-type/destination-type IDs are nonzero. Empty TYPES, SYMBOLS, ANNOTATIONS, RELATIONS and CONVERSIONS tables are valid when their record count is zero (and their string table is empty); SCOPES always contains its root.

## 6. Symbol records

Each declared name gets a stable symbol ID independent of its current `Slot*` address.

| Field | Wire type | Meaning |
|---|---|---|
| symbol_id | u32 | Stable nonzero ID. |
| scope_id | u32 | Declaring scope. |
| declaration_node_id | u32 | AST declaration node. |
| kind | u16 | Variable, function, struct, union, enum, newtype, label or future registered kind. |
| flags | u16 | Const, public, exported, packed and hard-typed flags as defined by the registry. |
| name_offset | u32 | Name offset in symbol string table. |
| name_length | u32 | Name byte length. |
| type_id | u32 | Declared/return/target type; zero when unresolved or not applicable. |
| first_relation | u32 | 1-based range start in RELATIONS, zero when no relations. |
| relation_count | u32 | Number of ordered related entries. |

A symbol is unique within the language-defined namespace of its scope and kind rules. The schema does not assume that all symbol kinds share one namespace; namespace rules must be made explicit by semantic tests before freeze.

## 7. Ordered relations

Relations retain information currently stored in `Slot.param_types` and `FieldInfo`.

Each relation contains:
- owner_symbol_id;
- relation_kind (function parameter, aggregate field, enum variant, enum payload field, or registered future relation);
- ordinal (zero-based within the owner);
- related_symbol_id (zero when the related item is represented only by AST);
- related_node_id (the parameter/field/variant AST node);
- name/type references where required.

The pair (owner_symbol_id, relation_kind, ordinal) is unique. Parameter order is preserved exactly. Aggregate and enum member order is preserved exactly. Every relation node reference must point to a node of the expected AST kind.

## 8. Node annotations

One annotation record may exist for any AST node; nodes with no semantic information may omit an annotation.

| Field | Wire type | Meaning |
|---|---|---|
| node_id | u32 | Existing AST node ID; unique and ascending. |
| scope_id | u32 | Scope in which the node is evaluated/declared. |
| resolved_symbol_id | u32 | Resolved identifier/callee/declaration symbol, or zero. |
| resolved_type_id | u32 | Resolved expression/declaration type, or zero if unknown/not yet inferred. |
| value_category | u16 | Unspecified, lvalue, rvalue, function designator or registered future category. |
| flags | u16 | Semantic state flags; zero until individually specified. |

A resolved symbol must be visible from the annotated node's scope under the language's lookup rules. A resolved type must exist in the type table. The annotation node ID must exist in the embedded AST payload.

## 9. Conversion records and NEXSUS Flow promotion

Conversions are separate from the original AST. The semantic stage must not silently rewrite literal text or AST node kinds to express a conversion.

A conversion record is keyed by node ID and operand ordinal and includes:
- node_id;
- operand_ordinal (0 for left/first, 1 for right/second; registered values for call arguments);
- source_type_id;
- destination_type_id;
- conversion_kind (identity, widening, narrowing, signedness, explicit cast, hard-type enforcement, or future registered kind);
- flags describing whether the conversion is implicit or explicit.

This table is initially optional. The existing C semantic checker currently performs only limited string-equality compatibility checks (ignoring a leading hard-type marker); it does not yet implement a complete numeric promotion lattice. Therefore the first schema/codec must support conversion records without claiming that NEXSUS Flow promotion rules are already implemented. Promotion semantics will be specified and tested as a separate compiler-language task before emitting non-identity conversion records.

## 10. Diagnostics and validity

- Semantic diagnostics use the existing invocation-scoped host sink and are attributed to `OCC_STAGE_SEMANTIC`.
- The module must not trust source spans from an unvalidated AST. AST schema 1.0 currently preserves line/column start locations only; byte offsets and end spans are not available in all AST nodes. The semantic module must not fabricate them.
- A failed semantic analysis publishes no SEMANTIC_AST artifact under the default strict output gate.
- Warnings may be reported, but the host's current strict gate blocks final output on warnings as well. Artifact validity policy must remain consistent with `OccCompilationContext`.
- Output-buffer retry must not duplicate diagnostic callbacks. The semantic module must either complete analysis into invocation-local scratch and defer callbacks until output capacity is known, or use a host-visible two-phase strategy. It must not retain state between calls.

## 11. Current implementation gaps that affect the module

The current `semantic.c` implementation is a checker, not yet a durable semantic model:

- `SemanticContext` stores a linked list of transient `Slot` records and a numeric scope depth; `exit_scope()` frees scope-local slots.
- `Slot` has no stable symbol ID, and there is no persistent scope table or per-node annotation table.
- `get_expr_type()` returns borrowed string names and handles only a subset of expression kinds.
- Type compatibility currently strips a leading `!` and compares names; no complete promotion/conversion lattice is present.
- The checker does not annotate AST nodes with resolved symbols or types.
- AST schema 1.0 carries line/column start positions but not complete byte spans.
- Some AST kinds intentionally have no semantic checks yet; the schema must not imply those checks are complete.

The first implementation step is therefore a durable, caller-owned semantic result model and tests for scope/symbol/type resolution. The DLL adapter comes after the result model and codec can round-trip independently.

## 12. Required implementation/test order

1. Register semantic section IDs and add this draft to the OIR schema index.
2. Define C wire structs only as logical API views; encode/decode field-by-field.
3. Implement bounded readers/writers for scopes, types, symbols, relations and node annotations.
4. Add round-trip, deterministic-order, malformed-count, overflow, invalid UTF-8, duplicate-ID, dangling-reference, wrong-node-kind and mutation-sweep tests.
5. Refactor semantic analysis to produce durable scope/symbol/type/annotation results without changing existing language behavior.
6. Test the result model against current valid/invalid semantic fixtures and add nested-scope, shadowing, forward declaration, function parameter, struct/enum and type-compatibility cases.
7. Implement `occ_semantic_stage` shared module: exactly one AST input, SEMANTIC_AST output, same job ID, host diagnostic sink, bounded retry and no cross-call state.
8. Add real DLL/SO/dylib loader tests, then connect OCC: AST OIR → Semantic DLL → SEMANTIC_AST OIR.
9. Keep the old in-process semantic path only as a temporary compatibility consumer if a downstream component still requires a materialized frontend AST. Remove it after Codegen consumes SEMANTIC_AST directly.

## 13. Non-goals for schema 1.0

- Freezing the common module ABI.
- Defining SSA or OIR_MODULE instruction semantics.
- Implementing optimizer behavior.
- Defining the final linker or its security policy.
- Claiming complete type inference, numeric promotion, ownership or control-flow analysis.
