# OIR AST Codec — Implementation Contract

**Status:** Implementation plan derived from the current C AST and OIR AST schema audit  
**Date:** 31 October 2026  
**Scope:** AST schema 1.0 writer/reader; no parser checkpoint migration or DLL ABI freeze yet.

## 1. Current frontend memory model

The C frontend AST has a common `ASTNode` header:

- `type`: frontend enum tag; converted through `oir_ast_map`, never copied as a wire integer.
- `line` and `col`: signed C integers; schema 1.0 stores unsigned 32-bit values. Zero/zero means unknown or synthetic; otherwise both must be positive and representable as `uint32_t`.
- `next`: owning sibling-list link. It is serialized as `next_node_id`, not as a node-specific TLV field.

All node-specific pointer fields in the current frontend are owning child edges. String members are node-owned allocations in the current AST cleanup implementation. The OIR reader must not preserve or serialize any host pointer value.

## 2. Ownership and graph rules

The serialized structure is a rooted, single-owner tree with sibling links represented as edges:

1. Root is an `AST_PROGRAM` node and has no incoming edge.
2. Every non-root node has exactly one incoming owning edge. The edge can be a child NODE_REF or a `next_node_id`.
3. A node may not be reached twice; cycles and shared child pointers are invalid.
4. Every node record must be reachable from the root.
5. Maximum node count is 1,000,000 and maximum edge depth is 512.
6. Child edges are visited in ascending field-ID order; the sibling edge is visited after the node's child fields. This is the canonical depth-first numbering rule.

This matches the ownership assumptions made by the existing `ast_destroy()` implementation. It is intentionally stricter than a general-purpose DAG format.

## 3. Writer: two-pass algorithm

### Pass A — validate and index

- Walk the AST without modifying it.
- Resolve each node tag through `oir_ast_node_id_from_frontend()`.
- Use a pointer-index table to detect cycles and duplicate ownership before assigning stable 1-based node IDs.
- Validate the node's expected C layout, line/column pair, booleans, enum ranges, string length, UTF-8 and child pointer structure.
- Enforce node/depth/string/artifact limits before size arithmetic or allocation.
- Visit child fields in registry field-ID order, then `next`.

### Pass B — emit canonical payload

- Emit the 32-byte schema header and fixed 32-byte node records in little-endian form.
- Emit each node's TLV fields in strictly ascending field-ID order.
- Convert every token-valued frontend enum through the stable OIR token map.
- Store UTF-8 strings in the string table and write STRING_REF(offset,length); preserve NULL separately from an empty string.
- Encode every child pointer and sibling pointer as a stable node ID; NULL is ID zero.
- Use checked arithmetic for every count, offset, length and output-capacity calculation. Never write a partial successful payload.

The first implementation should support caller-owned output storage with a size-query/preflight path, rather than growing a buffer through an allocator hidden inside the codec.

## 4. Reader: validate before constructing pointers

The reader must be split into validation and materialization:

1. Validate exact payload length, fixed header, node record size, node IDs, node kinds, flags, reserved fields, record ranges and TLV boundaries.
2. Validate each node's field IDs, required-field presence, value types, fixed widths, field order, scalar ranges, string references and UTF-8 bytes.
3. Validate all NODE_REF and sibling IDs, then verify root kind, one-owner incoming-edge counts, acyclicity, depth limit and full reachability.
4. Only after the complete payload passes validation, allocate all frontend nodes and owned strings.
5. Populate scalar/string members while nodes are still disconnected.
6. In a final no-fail linking pass, connect child pointers and sibling pointers.
7. Return the root only after all steps succeed.

Do not call `ast_destroy()` on a partially linked or unvalidated graph: its recursive cleanup assumes the graph is already a valid single-owner tree. Before linking, failure cleanup must free each allocated node and each independently allocated string exactly once. After linking, normal `ast_destroy(root)` ownership applies.

## 5. Field mapping hazards found during audit

- `AST_LITERAL.token_type` and operator fields use `OmniTokenType`; wire values must use OIR token registry IDs.
- `AST_LITERAL.lit_type` and `AST_LOOP.loop_type` have explicit semantic value ranges; they are not OIR IDs.
- `AST_FUNC_DECL` and `AST_EXTERN_DECL`, `AST_STRUCT_DECL` and `AST_UNION_DECL`, `AST_BINARY_EXPR` / `AST_ASSIGNMENT` / `AST_INDEX_ACCESS`, and `AST_NEXUS_FLOW` / `AST_NEXUS_PIPE` share C layouts but retain distinct OIR node kinds.
- Intrinsic call nodes share `ASTCall`; their nullable `callee` and `args` members still require explicit TLV values.
- `AST_LISTEN` shares `ASTNexusCapture`; `AST_ZONE` and the other intrinsic call kinds share `ASTCall`.
- C `int` booleans must be exactly 0 or 1 before encoding. Signed `int` fields must fit the schema's I32 representation.
- AST strings are NUL-terminated C strings, while OIR strings are explicit-length UTF-8. Embedded NUL cannot be represented and must be rejected at the codec boundary.

## 6. Test sequence

1. Schema descriptor coverage: all 80 node kinds and every registered field.
2. TLV primitive tests: each value type, NULL versus empty string, little-endian encoding, exact lengths and truncation.
3. Node-table tests: count/ID/root/kind/range/offset validation and overflow boundaries.
4. Graph tests: null edges, valid sibling chains, cycle, duplicate owner, orphan node, invalid reference and depth/count limits.
5. Field semantic tests: missing/duplicate/unknown required/unknown optional fields, wrong type, bad bool, invalid token ID, malformed UTF-8 and invalid scalar ranges.
6. Round-trip fixtures for all 80 node kinds and mixed realistic programs; compare semantic AST fields, not raw C struct bytes.
7. Windows, Ubuntu and macOS CTest; Ubuntu ASan/UBSan and Valgrind.

Parser checkpoint migration is allowed only after the codec and malformed-input matrix pass in CI.


## 7. Implemented foundation: bounded TLV primitives

Files:
- `omnicore-sdk/src/c/oir/oir_ast_tlv.h`
- `omnicore-sdk/src/c/oir/oir_ast_tlv.c`
- `omnicore-sdk/src/c/tests/test_oir_ast_tlv.c`

The first implementation slice provides:
- Caller-owned bounded writer; no hidden allocation.
- Fixed 8-byte TLV headers and little-endian field ID/value length encoding.
- Strictly ascending, non-zero field IDs; duplicate and out-of-order fields are rejected.
- Known value types 0–9 with exact fixed-width validation.
- Canonical BOOL values (0/1), explicit NULL distinct from empty STRING_REF, and stable token ID range validation (1–160).
- Reader returns zero-copy field views and rejects truncated headers/values, unknown types, invalid flags, invalid widths and malformed ordering.
- Writer failure leaves its cursor and prior bytes unchanged for capacity/order/type failures.

This layer intentionally does not yet validate STRING_REF offsets against the AST string table, NODE_REF IDs against the node table, per-node required-field schemas, or graph ownership. Those checks belong to the AST payload/node-table validator.

CTest target: `oir_ast_tlv_primitives_roundtrip_and_validation`. Cross-platform CI and sanitizer results are pending.


## 8. Implemented foundation: bounded AST node table

Files:
- `omnicore-sdk/src/c/oir/oir_ast_node_table.h`
- `omnicore-sdk/src/c/oir/oir_ast_node_table.c`
- `omnicore-sdk/src/c/tests/test_oir_ast_node_table.c`

The first node-table slice now implements:
- 32-byte AST payload header and 32-byte fixed node records, encoded field-by-field in little-endian form.
- Caller-owned bounded payload writer and zero-copy validated reader.
- Exact payload-size, record-size, reserved/flags, contiguous 1-based node ID, root ID/kind, node-kind range and line/column pair checks.
- Field-data slices must be ordered, non-overlapping, in-bounds and cover the complete field-data area exactly.
- Every node TLV stream is validated through the bounded TLV reader.
- NODE_REF values must be zero or within the node table; STRING_REF ranges must fit the string table, obey the 16 MiB per-string ceiling and contain valid UTF-8 without embedded NUL.
- Node sibling references are range-checked. This layer does not yet prove unique ownership, acyclicity, root reachability or maximum graph depth; those belong to the dedicated graph validator.
- Writer validates its logical input before writing and does not allocate internally. Input buffers must not overlap output storage.

CTest target: `oir_ast_node_table_roundtrip_and_validation`. Cross-platform CI verification is pending.


## 9. Implemented foundation: single-owner AST graph validator

Files:
- `omnicore-sdk/src/c/oir/oir_ast_graph.h`
- `omnicore-sdk/src/c/oir/oir_ast_graph.c`
- `omnicore-sdk/src/c/tests/test_oir_ast_graph.c`

The graph pass consumes a validated node-table view and caller-provided scratch arrays; it performs no allocation. It:
- Counts incoming edges from every NODE_REF field and each node's next_node_id.
- Requires root node 1 to have zero incoming owning edges and every other node to have exactly one.
- Traverses from the root, rejecting repeated visits, invalid depth and unreachable/orphan records.
- Enforces the schema maximum depth of 512.
- Tests a valid child+sibling tree, a duplicate-owner/cyclic sibling case, an unreachable self-cycle and a 513-node child chain.

CTest target: `oir_ast_graph_single_owner_reachability_and_depth`. Cross-platform CI verification is pending.

This is a structural graph check, not yet the complete AST semantic validator. It does not enforce per-node required fields, optional unknown-field policy, field value types beyond TLV primitive validity, or frontend C struct-specific scalar constraints.


## 10. Implemented foundation: per-node field schema validation

Files:
- `omnicore-sdk/src/c/oir/oir_ast_field_schema.h`
- `omnicore-sdk/src/c/oir/oir_ast_field_schema.c`
- `omnicore-sdk/src/c/tests/test_oir_ast_field_schema.c`

The validator checks every node against the v1 registry:
- Every registered node kind (1–80) has an explicit required-field count and field wire-type mapping.
- Required fields must be present in ascending order; missing or duplicate/out-of-order known fields are rejected.
- Known fields must use their registered wire type. NULL is accepted only for fields declared as nullable strings.
- Unknown fields are accepted only when the TLV OPTIONAL flag is set; unknown required fields are rejected.
- The test constructs schema-valid records for all 80 node kinds and checks missing fields, wrong known-field types, unknown required fields and unknown optional fields.

CTest target: `oir_ast_field_schema_all_node_kinds_and_compatibility`. CI run 41157314066 passed on Windows, Ubuntu and macOS; ASan/UBSan and Valgrind also passed.


## 11. Implemented foundation: frontend AST pointer index

Files:
- `omnicore-sdk/src/c/oir/oir_ast_frontend_index.h`
- `omnicore-sdk/src/c/oir/oir_ast_frontend_index.c`
- `omnicore-sdk/src/c/tests/test_oir_ast_frontend_index.c`

The index pass is read-only and uses a caller-owned, zero-initialized, power-of-two open-addressed pointer table. It has no allocator dependency and rejects load factors above 0.5.

Canonical traversal:
1. Assign the current node's 1-based ID.
2. Visit node-specific child pointers in ascending registry field-ID order.
3. Visit the current node's `next` sibling at the same graph depth.

It requires an AST_PROGRAM root with no sibling, validates nonnegative paired line/column values, maps node tags through the stable 1–80 registry, and rejects repeated pointers (cycles or multiple owners), unsupported node tags, over 1,000,000 nodes and depth over 512. Lookup returns zero for an absent pointer.

The test covers deterministic child/sibling numbering, Nexus Flow's sparse child field IDs (2, 3, 6), repeated ownership/cycles, invalid root/sourceloc and the depth boundary. It does not yet encode node records, TLVs or strings; those are the next writer pass.

CTest target: `oir_ast_frontend_index_deterministic_preorder_and_graph_rejection`. CI run 41158241944 passed: Windows, Ubuntu and macOS build/test; Ubuntu ASan/UBSan and Valgrind.
