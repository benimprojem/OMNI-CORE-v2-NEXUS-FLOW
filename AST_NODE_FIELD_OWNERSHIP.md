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
| Variable | VAR_DECL | ASTVarDecl | name, type_name; initializer; inline_type |
| Function | FUNC_DECL, EXTERN_DECL | ASTFuncDecl | name, return_type; params chain; body |
| Aggregate | STRUCT_DECL, UNION_DECL | ASTStructDecl | name; fields chain |
| Enum | ENUM_DECL / ENUM_VARIANT | ASTEnumDecl / ASTEnumVariant | name; variants chain; variant data |
| Newtype | NEWTYPE_DECL | ASTNewTypeDecl | name; underlying/type child per struct |
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
| Nexus Flow | CAPTURE / PIPE / FLOW / LABEL / JUMP / HALT / INTENT | dedicated structs | per-node expression/label/target payloads |
| Other special forms | AI_LOGIC / UNPACK / PANIC / SPAWN / LISTEN / ECHO / SEND / RECEIVE / DONE / AREA / ADDR / LEN / PEEK / POKE / INB / OUTB / IRQ / INTR / REG / PRNT / PRMT / WRITE / FWRITE / TYPEOF / SIZEOF / IS_* / EXIT / SWAP / ZONE / DATE / DATENOW / TIME / CLOCK / FREE / ZONE_SYNC / ZONE_VIEW / ZONE_LOCK | AST-specific or ASTCall-backed | owned expression/list/string fields defined in each struct and destroy switch |

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

## Known gaps (do not treat as complete)

- Universal byte source spans (start/end offsets) and end line/column.
- Generated authoritative field schema; current switches/tables can drift.
- Parser recovery ownership stress tests for every node family.
- Exhaustive AST tag-to-parser/OIR/semantic/lowering coverage report.
- Explicit semantic status for syntax-only nodes that have not yet received type rules.
- OIR partial-index/view lifetime representation.
