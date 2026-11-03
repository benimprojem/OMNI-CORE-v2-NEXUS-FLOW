# AST Node / Field / Ownership Inventory

**Status:** Baseline inventory for AST IDs and structural ownership. Keep this document synchronized whenever a node or field changes.

## Global invariants

- `ASTNode.type` is the append-only node tag; never renumber published tags.
- `ASTNode.line/col` are currently the source location baseline. Byte spans and end positions are not yet universal.
- `ASTNode.next` is an owned sibling-chain link. A node/subtree has exactly one owner; never share child pointers.
- Child pointers and sibling chains are destroyed recursively by `ast_destroy_recursive`; flat sibling traversal is iterative.
- String fields allocated by parser are node-owned and freed by the matching destroy case.
- A field not listed as an owned child/string is scalar metadata or borrowed input and must not be freed by AST destruction.

## Node families

| Family | Tags | Primary representation | Owned child fields / strings |
|---|---|---|---|
| Root / blocks | PROGRAM, BLOCK, ASM_BLOCK | ASTProgram / ASTBlock | declarations/statements sibling chain |
| Payload-free control | BREAK, CONTINUE | ASTNode | no payload fields; only the common owned sibling link |
| Variable | VAR_DECL | ASTVarDecl | name, type_name; initializer; inline_type |
| Function | FUNC_DECL, EXTERN_DECL | ASTFuncDecl | name, return_type; params chain; body |
| Aggregate | STRUCT_DECL, UNION_DECL | ASTStructDecl | name; fields chain |
| Enum | ENUM_DECL / ENUM_VARIANT | ASTEnumDecl / ASTEnumVariant | name; variants chain; variant data |
| Newtype | NEWTYPE_DECL | ASTNewTypeDecl | owned `name` and `target_type` strings; no child pointer in the current C struct |
| Group / method | GROUP_DECL / METHOD | ASTGroupDecl / ASTMethod | group name/members; public_name/implementation |
| Use / defer / thread | USE / DEFER / THREAD_BLOCK | dedicated structs | module_name/alias; body; thread name/body/mode/timeout |
| Scalar expressions | LITERAL / IDENTIFIER | ASTLiteral / ASTIdentifier | literal string_value; identifier name |
| Binary / assignment | BINARY_EXPR / ASSIGNMENT | ASTBinaryExpr | left, right |
| Unary / postfix / return | UNARY_EXPR / POSTFIX_EXPR / RETURN | ASTUnaryExpr | operand |
| Cast | CAST | ASTCast | value, target_type |
| Call family | FUNC_CALL and ASTCall-backed builtins | ASTCall | callee; args chain |
| Member | MEMBER_ACCESS | ASTMemberAccess | object; member_name |
| Index | INDEX_ACCESS | ASTIndexAccess | target; index |
| Tuple / array literal | TUPLE / ARRAY_LITERAL | ASTTuple / ASTArrayLiteral | elements chain |
| Array type | ARRAY_TYPE / ARRAY_DIMENSION | ASTArrayType / ASTArrayDimension | dimensions chain; dimension length |
| If / loop | IF / LOOP | ASTIf / ASTLoop | condition/branches; loop fields |
| Selection | SELECT / SELECT_ARM / SCAN / SCAN_ARM | dedicated structs | selector/subject; arms; pattern/body |
| Pattern | PATTERN_BINDING / PATTERN_CONSTRUCTOR | dedicated structs | binding name; constructor name/bindings |
| Rules | RULESET_DECL / RULE_* / APPLY_RULES / ON_BLOCK | dedicated structs | names; params/rules/conditions/actions/args/body |
| Lambda / relocate / unroll | LAMBDA / RELOCATE / UNROLL | dedicated structs | params/body; source/target; count/loop |
| Nexus Flow | CAPTURE / LISTEN / PIPE / FLOW / LABEL / JUMP / HALT / INTENT | dedicated structs | per-node expression/label/target payloads |
| Other special forms | AI_LOGIC / UNPACK / PANIC / SPAWN / LISTEN / ECHO / SEND / RECEIVE / DONE / AREA / ADDR / LEN / PEEK / POKE / INB / OUTB / IRQ / INTR / REG / PRNT / PRMT / WRITE / FWRITE / TYPEOF / SIZEOF / IS_* / EXIT / SWAP / ZONE / DATE / DATENOW / TIME / CLOCK / FREE / ZONE_SYNC / ZONE_VIEW / ZONE_LOCK | AST-specific or ASTCall-backed | owned expression/list/string fields defined in each struct and destroy switch |

## Confirmed shared payload layouts and destroy dispatch

The current `ast.h` / `ast.c` source audit confirms these intentional shared layouts. Keep the allocation, parser construction, printer, serializer/deserializer and destroy cases synchronized:

| AST tag(s) | C payload layout | Owned fields destroyed |
|---|---|---|
| `AST_PROGRAM`, `AST_BLOCK`, `AST_ASM_BLOCK` | `ASTProgram` / `ASTBlock` share the first payload pointer offset | program declarations / block statements sibling chain |
| `AST_FUNC_DECL`, `AST_EXTERN_DECL` | `ASTFuncDecl` | name, return_type, params chain, body |
| `AST_STRUCT_DECL`, `AST_UNION_DECL` | `ASTStructDecl` | name, fields chain |
| `AST_BINARY_EXPR`, `AST_ASSIGNMENT` | `ASTBinaryExpr` | left and right subtrees |
| `AST_UNARY_EXPR`, `AST_POSTFIX_EXPR`, `AST_RETURN` | `ASTUnaryExpr` | operand subtree |
| `AST_NEXUS_FLOW`, `AST_NEXUS_PIPE` | `ASTNexusFlow` (prefix-compatible with the binary fields) | left, right, catch_block; retry_count/retry_delay are scalars |
| `AST_NEXUS_CAPTURE`, `AST_LISTEN` | `ASTNexusCapture` | handler_name string and expr subtree |
| Call-backed intrinsic tags | `ASTCall` | callee subtree and args sibling chain |
| `AST_BREAK`, `AST_CONTINUE` | base `ASTNode` only | no payload; sibling link is still handled by the outer destroy loop |
| `AST_NEWTYPE_DECL` | `ASTNewTypeDecl` | name and target_type strings; target_type is not an AST child |

`AST_SCAN_ARM.pattern == NULL` is reserved for the final scan `else` catch-all arm; ordinary scan arms require a pattern. The OIR field remains present as a nullable NODE_REF, so no field ID or wire layout change is needed. A `when` guard is canonicalized as an `AST_IF` node stored in the arm's existing `body` pointer; the condition and guarded body are therefore owned and destroyed through the ordinary `AST_IF` child contract.

`AST_NEXUS_PIPE` and `AST_NEXUS_FLOW` are both allocated as `ASTNexusFlow`; the destructor's binary-prefix access is safe only while the declared field order remains `base, op, left, right, ...`. Do not replace this with a distinct payload without updating all lifecycle and codec paths.

## Cross-component checklist (per tag)

For every ASTNodeType, verify all of these before declaring it complete:

1. Parser construction and malformed-input cleanup.
2. `get_node_size` allocation mapping.
3. Printer/dump switch.
4. Destroy switch (every owned child and string exactly once).
5. Binary serializer field schema and sibling handling.
6. Deserializer allocation, field restoration and failure cleanup.
7. OIR AST kind mapping + field schema + writer/reader + graph traversal.
8. Semantic type/check/annotation behavior or explicit syntax-only status.
9. Lowering support state: supported / partial / fail-closed.
10. Positive, negative, round-trip and allocation-failure tests.

## AST debug printer coverage

The `ast_print_recursive` switch has an explicit case for every current `ASTNodeType`, including payload-free `AST_BREAK` and `AST_CONTINUE`. Shared-layout nodes are grouped only where their payload contracts match (for example, `AST_NEXUS_PIPE` with `AST_NEXUS_FLOW`, and call-backed intrinsic nodes through `ASTCall`). Keep the switch exhaustive when appending a new AST tag; the generic fallback is diagnostic protection, not feature coverage.

## Serialization regression coverage

`omnicore-sdk/src/c/tests/test_ast_deserialize.c` exercises every append-only AST tag from `AST_PROGRAM` through the current terminal tag `AST_ARRAY_DIMENSION` in both populated-field and sibling-chain round-trip tests. The populated fixtures must be extended whenever a new tag is appended; the invalid-tag boundary must move with the terminal tag. This proves AST allocation/serialization/deserialization symmetry for the exercised fields, but does not prove parser construction, semantic correctness, OIR support or lowering readiness.

The OIR AST frontend has separate writer/reader/index/schema tests under `omnicore-sdk/src/c/tests/`; those are a distinct wire contract and must not be conflated with the legacy native-layout AST stream tests.

## Known gaps (do not treat as complete)

- Universal byte source spans (start/end offsets) and end line/column.
- Generated authoritative field schema; current switches/tables can drift.
- Parser recovery ownership stress tests for every node family.
- Exhaustive AST tag-to-parser/OIR/semantic/lowering coverage report.
- Explicit semantic status for syntax-only nodes that have not yet received type rules.
- OIR partial-index/view lifetime representation.


## Structural nullability contract (AST storage vs valid syntax)

The zero-initialized factory deliberately permits every pointer field to be NULL. This is **storage/lifecycle tolerance**, not a statement that NULL is valid in a parsed program. Contracts are separated into three levels:

- **Required after successful Parser:** fields that a grammar production must populate. A missing value indicates a Parser construction bug or malformed manually-built AST.
- **Syntax-optional:** fields intentionally absent for a supported syntax form (e.g. variable initializer, function body for extern, dynamic array dimension length, scan-else pattern).
- **Semantic-required:** Parser may structurally create a node, but Semantic must reject a missing/invalid value before a successful semantic artifact.

### Explicit optional-field cases already established

| Node / field | NULL meaning | Boundary |
|---|---|---|
| `AST_VAR_DECL.initializer` | declaration without initializer | Semantic checks const/inference/definite initialization rules |
| `AST_VAR_DECL.inline_type` | no inline aggregate/declarator payload | Parser/type resolver decides from declaration syntax |
| `AST_FUNC_DECL.body` / `AST_EXTERN_DECL.body` | extern signature has no body | Non-extern function body is Parser-required |
| `AST_FUNC_DECL.return_type` | inferred/unspecified return type | Semantic return inference/validation |
| `AST_ARRAY_DIMENSION.length` | dynamic/unspecified dimension (`[]`) | Semantic parameter/array context rules |
| `AST_SCAN_ARM.pattern` | scan catch-all `else` arm only | Parser enforces final and unique else |
| `AST_NEXUS_HALT.expr` | halt without explicit expression | Halt syntax/semantic context |
| `AST_IF.else_branch` | no else/elsif continuation | Valid optional branch |
| `AST_LOOP.init/condition/increment/collection/variable` | depends on loop form | Parser loop_type selects required subset; Semantic validates form |
| `AST_NEXUS_FLOW.catch_block` | no explicit catch branch | Flow operator determines whether a catch is required |
| `AST_ENUM_VARIANT.data` | unit variant | Enum payload contract |
| `AST_MEMBER_ACCESS.member_name`, owned names generally | NULL is not a valid parsed name | Parser-required; Semantic must fail closed on malformed manual AST |

### Required child groups after successful Parser (baseline)

These are parser construction invariants, not a complete semantic typing specification:

- `AST_PROGRAM.declarations` may be empty; `AST_BLOCK.statements` may be empty.
- `AST_BINARY_EXPR` / `AST_ASSIGNMENT`: both `left` and `right` are required.
- `AST_CAST`: `value` and `target_type` are required.
- `AST_FUNC_CALL`: `callee` required; `args` may be empty.
- `AST_INDEX_ACCESS`: `target` and `index` required.
- `AST_TUPLE` / `AST_ARRAY_LITERAL`: empty element list is structurally representable; Semantic decides whether the empty form has an inferable type.
- `AST_IF`: `condition` and `then_branch` required; `else_branch` optional.
- `AST_SELECT` / `AST_SCAN`: selector/subject and arm list required by successful grammar; arm list must contain at least one arm.
- `AST_SELECT_ARM`: body required; pattern may be absent only when `is_default` is set.
- `AST_SCAN_ARM`: body required; pattern may be NULL only for the final else catch-all.
- `AST_PATTERN_CONSTRUCTOR.bindings`, function parameter lists and rule argument lists may be empty.
- `AST_ARRAY_TYPE.dimensions`: at least one dimension is required by the declarator production; each dimension node exists, while its `length` may be NULL for `[]`.

**Validation gap:** these invariants are currently documented and partially covered by parser fixtures, but there is no generic AST graph validator that checks required-child contracts, duplicate ownership, or cycles. Do not treat the table as automatically enforced until such a validator and negative tests are added.
