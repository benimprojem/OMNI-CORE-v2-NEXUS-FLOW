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
