# AST Node Ownership / Lifecycle Contract

**Branch:** `refactor/c-core-roadmap`  
**Source of truth:** `omnicore-sdk/src/c/parser/ast.h` and `ast.c`  
**Status:** static audit documented; exhaustive automated ownership validation remains open.

## Rules

- Every AST allocation is exclusively owned. A child or sibling node must not be attached to multiple parents/lists.
- `next` is an owned sibling chain and is walked iteratively by destruction/codec routines.
- Child pointers below own the complete child subtree and its sibling chain unless explicitly noted.
- String fields listed as owned are heap-owned and freed by `ast_destroy`.
- Null child/string fields are accepted by the current zero-initialized AST factory; semantic/parser layers decide which are required.
- `ast_destroy` is tolerant of NULL fields. This is a lifecycle property, not proof that a node is semantically valid.

## Payload ownership map

| AST kind(s) | Owned child nodes | Owned strings / scalar payload |
|---|---|---|
| `AST_PROGRAM` | `declarations` | — |
| `AST_BLOCK`, `AST_ASM_BLOCK` | `statements` | — |
| `AST_VAR_DECL` | `initializer`, `inline_type` | `name`, `type_name`; const/hard/public/export flags |
| `AST_FUNC_DECL`, `AST_EXTERN_DECL` | `params`, `body` | `name`, `return_type`; extern/public/export flags |
| `AST_STRUCT_DECL`, `AST_UNION_DECL` | `fields` | `name`; packed/union/public/export flags |
| `AST_ENUM_DECL` | `variants` | `name` |
| `AST_ENUM_VARIANT` | `data` | `name` |
| `AST_NEWTYPE_DECL` | — | `name`, `target_type` |
| `AST_GROUP_DECL` | `members` | `name`; public/export flags |
| `AST_METHOD` | `implementation` | `public_name` |
| `AST_USE` | — | `module_name`, `alias` |
| `AST_DEFER` | `body` | — |
| `AST_THREAD_BLOCK` | `body`, `mode`, `timeout` | `name` |
| `AST_LITERAL` | — | `string_value`; token/literal kind, integer and float values |
| `AST_IDENTIFIER` | — | `name` |
| `AST_BINARY_EXPR`, `AST_ASSIGNMENT` | `left`, `right` | `op` |
| `AST_NEXUS_FLOW`, `AST_NEXUS_PIPE` | `left`, `right`, `catch_block` | `op`, retry count/delay |
| `AST_UNARY_EXPR`, `AST_POSTFIX_EXPR`, `AST_RETURN` | `operand` | `op` |
| `AST_CAST` | `value`, `target_type` | — |
| `AST_FUNC_CALL` and call-shaped intrinsic nodes | `callee`, `args` | — |
| `AST_MEMBER_ACCESS` | `object` | `member_name` |
| `AST_INDEX_ACCESS` | `target`, `index` | — |
| `AST_TUPLE` | `elements` | — |
| `AST_IF` | `condition`, `then_branch`, `else_branch` | — |
| `AST_LOOP` | `init`, `condition`, `increment`, `body`, `collection`, `variable` | `loop_type` |
| `AST_NEXUS_CAPTURE`, `AST_LISTEN` | `expr` | `handler_name` |
| `AST_NEXUS_LABEL` | `statement` | `label_name` |
| `AST_NEXUS_JUMP` | — | `label_name` |
| `AST_NEXUS_HALT` | `expr` (optional) | — |
| `AST_NEXUS_INTENT` | `target` | `intent_name` |
| `AST_AI_LOGIC` | — | `query` |
| `AST_UNPACK` | `expr` | — |
| `AST_PANIC` | — | `message` |
| `AST_SELECT` | `selector`, `arms` | — |
| `AST_SELECT_ARM` | `pattern`, `body` | `is_default` |
| `AST_SCAN` | `subject`, `arms` | — |
| `AST_SCAN_ARM` | `pattern`, `body` | — |
| `AST_PATTERN_BINDING` | — | `name` |
| `AST_PATTERN_CONSTRUCTOR` | `bindings` | `name` |
| `AST_RULESET_DECL` | `params`, `rules` | `name` |
| `AST_RULE_BARRIER` | `condition`, `message` | — |
| `AST_RULE_TRIGGER` | `condition`, `action` | — |
| `AST_RULE_ALWAYS`, `AST_RULE_DEFAULT` | `action` | — |
| `AST_APPLY_RULES` | `args` | `ruleset_name` |
| `AST_ON_BLOCK` | `body` | `ruleset_name`, `event_name` |
| `AST_LAMBDA` | `params`, `body` | `body_is_expression` |
| `AST_RELOCATE` | `source`, `target` | — |
| `AST_UNROLL` | `count`, `loop` | — |
| `AST_ARRAY_LITERAL` | `elements` | — |
| `AST_ARRAY_TYPE` | `dimensions` | — |
| `AST_ARRAY_DIMENSION` | `length` (NULL = dynamic dimension) | — |
| `AST_BLOCK_HALT` | no payload; final marker in owning block | — |
| `AST_BREAK`, `AST_CONTINUE` | — | Base node only; no payload |

**Call-shaped intrinsic kinds** in the `AST_FUNC_CALL` row are: `AST_SPAWN`, `AST_ECHO`, `AST_SEND`, `AST_RECEIVE`, `AST_DONE`, `AST_AREA`, `AST_ZONE`, `AST_ADDR`, `AST_LEN`, `AST_PEEK`, `AST_POKE`, `AST_INB`, `AST_OUTB`, `AST_IRQ`, `AST_INTR`, `AST_REG`, `AST_PRNT`, `AST_PRMT`, `AST_WRITE`, `AST_FWRITE`, `AST_TYPEOF`, `AST_SIZEOF`, `AST_IS_OK`, `AST_IS_ERR`, `AST_IS_SOME`, `AST_IS_NONE`, `AST_IS_NULL`, `AST_EXIT`, `AST_SWAP`, `AST_DATE`, `AST_DATENOW`, `AST_TIME`, `AST_CLOCK`, `AST_FREE`, `AST_ZONE_SYNC`, `AST_ZONE_VIEW`, `AST_ZONE_LOCK`.

## Codec/lifecycle audit findings

- Factory allocation is centralized in `get_node_size()`; all registered kinds are bounded by `AST_PROGRAM..AST_BLOCK_HALT` (100 tags).
- `AST_BREAK` and `AST_CONTINUE` intentionally allocate only `ASTNode`.
- `AST_FUNC_DECL` and `AST_EXTERN_DECL`, `AST_STRUCT_DECL` and `AST_UNION_DECL`, and `AST_NEXUS_FLOW` and `AST_NEXUS_PIPE` share payload structures; their discriminating kind remains in `base.type`.
- `AST_LISTEN` currently shares `ASTNexusCapture` storage and codec/destructor behavior. Confirm its exact semantic contract before changing this representation.
- Serializer, deserializer and destructor switches must remain field-symmetric. Current binary AST codec is a process/build-local artifact (it writes C enum/int representation); it is not the stable cross-version OIR format.
- Child pointers are raw pointers. `ast_validate()` checks uniqueness/acyclicity for all 100 kinds; `ast_serialize()` calls it before writing and `ast_deserialize()` calls it before returning a decoded root. `ast_destroy()` remains a low-level operation whose caller must provide an exclusively owned, acyclic graph.

## Remaining automated checks

1. Generate a compile-time/runtime table that compares each node kind's allocation size with its expected payload size.
2. For every owned child/string field, inject allocation failure/truncated codec input and verify partial-tree cleanup under ASan/Valgrind.
3. Keep serializer/deserializer graph validation covered by tests; retain the explicit exclusive-ownership precondition for direct `ast_destroy()` calls.
4. Keep factory, codec, registry and parser coverage counters independent; the 99 original tags and the context-sensitive block marker have distinct test fixtures; do not infer one from another.


## 2026-11-03 — AST codec graph validation

- Legacy stream serialization now calls `ast_validate()` before writing; invalid roots, shared ownership and cycles are rejected before the output stream is modified.
- Legacy stream deserialization validates the complete decoded graph after checking end-of-stream; structurally invalid payloads are rejected and the partially materialized graph is destroyed.
- Added regression tests for no-write rejection of shared ownership, malformed function declarations with missing bodies, and valid serialize/deserialize round-trip.
- Existing all-tag codec test was aligned with the enforced contract: each tag is populated with a valid child shape before round-trip; this preserves the 99-kind coverage without requiring the codec to accept malformed empty nodes.
- Current head `568ed8b`: GitHub Actions Linux CTest and ASan/UBSan passed; Windows, Valgrind and macOS jobs are still pending at record time. The earlier Linux failure in `ast_rejects_malformed_streams` was caused by an empty-node tag fixture conflicting with the new validator; the fixture was corrected to use `populate_ast_fixture()`.


## 2026-11-03 — Structural validation closure and remaining AST gates

- Graph Validator dispatch is explicit for all 100 enum tags; its switch default now returns AST_VALIDATE_INVALID_KIND rather than silently accepting an unregistered tag as a leaf.
- Current ASTNode source metadata is limited to line/column. Byte start/end offsets and end line/column are present in token/diagnostic infrastructure but are not persisted in AST nodes or the legacy AST stream codec.
- `AST_NEXUS_HALT` carries an optional target expression. `AST_BLOCK_HALT` now represents the block-closing `}?;` separately; Parser appends it as the final statement child. Graph validation rejects it at root level, outside a block, or before another sibling. Parser, graph-validator, legacy AST codec and OIR round-trip regression fixtures now cover the marker; latest CI is queued.
- AST_ASM_BLOCK is currently a generic ASTBlock layout; fastexec, asm: labels and asmcall/asmjmp do not have a C Parser construction path.
- Canonical syntax path is now reconciled on the active branch: `docs/language/syntax-reference.md` is the single normative document. It was restored from the branch's reconciled syntax content; the former duplicate `omnicore-sdk/docs/syntax.md` was removed. `main` remains untouched.
- These are explicit pre-Semantic AST/model gates. AST must not be marked complete until the source-span contract, block-Halt representation and assembly scope are resolved and the syntax-reference path is reconciled.
