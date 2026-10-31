# OIR AST Node Registry v1

**Status:** Initial stable-ID registry proposal for AST payload schema 1.0  
**Source inventory:** 80 node kinds in the current C frontend ASTNodeType enum  
**Rule:** OIR IDs are explicit wire identifiers. They are not C enum ordinals, sizeof values, or compiler pointers.

## Registry rules

- IDs 1–80 are assigned to the current node-kind inventory below.
- New node kinds append after 80; published IDs are never renumbered or reused.
- A removed node kind remains reserved.
- Unknown required node kinds are rejected.
- Each node kind has a field-ID namespace local to that kind. Existing field IDs are never reused.
- New fields append; new optional fields may be skipped by compatible readers.
- Operator and literal token types use OIR_TOKEN_REGISTRY.md IDs, not OmniTokenType ordinals.

## Node-kind IDs

| OIR ID | C node kind | Mapping note |
|---:|---|---|
| 1 | `AST_PROGRAM` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 2 | `AST_BLOCK` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 3 | `AST_VAR_DECL` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 4 | `AST_FUNC_DECL` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 5 | `AST_STRUCT_DECL` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 6 | `AST_ENUM_DECL` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 7 | `AST_UNION_DECL` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 8 | `AST_NEWTYPE_DECL` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 9 | `AST_ENUM_VARIANT` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 10 | `AST_GROUP_DECL` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 11 | `AST_METHOD` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 12 | `AST_EXTERN_DECL` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 13 | `AST_LITERAL` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 14 | `AST_IDENTIFIER` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 15 | `AST_BINARY_EXPR` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 16 | `AST_UNARY_EXPR` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 17 | `AST_POSTFIX_EXPR` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 18 | `AST_ASSIGNMENT` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 19 | `AST_FUNC_CALL` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 20 | `AST_MEMBER_ACCESS` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 21 | `AST_INDEX_ACCESS` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 22 | `AST_CAST` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 23 | `AST_TUPLE` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 24 | `AST_IF` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 25 | `AST_LOOP` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 26 | `AST_RETURN` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 27 | `AST_BREAK` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 28 | `AST_CONTINUE` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 29 | `AST_ASM_BLOCK` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 30 | `AST_NEXUS_CAPTURE` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 31 | `AST_NEXUS_PIPE` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 32 | `AST_NEXUS_FLOW` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 33 | `AST_NEXUS_LABEL` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 34 | `AST_NEXUS_JUMP` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 35 | `AST_NEXUS_HALT` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 36 | `AST_NEXUS_INTENT` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 37 | `AST_AI_LOGIC` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 38 | `AST_UNPACK` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 39 | `AST_PANIC` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 40 | `AST_SPAWN` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 41 | `AST_LISTEN` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 42 | `AST_ECHO` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 43 | `AST_SEND` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 44 | `AST_RECEIVE` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 45 | `AST_DONE` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 46 | `AST_AREA` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 47 | `AST_ADDR` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 48 | `AST_LEN` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 49 | `AST_PEEK` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 50 | `AST_POKE` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 51 | `AST_INB` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 52 | `AST_OUTB` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 53 | `AST_IRQ` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 54 | `AST_INTR` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 55 | `AST_REG` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 56 | `AST_PRNT` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 57 | `AST_PRMT` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 58 | `AST_WRITE` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 59 | `AST_FWRITE` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 60 | `AST_TYPEOF` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 61 | `AST_SIZEOF` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 62 | `AST_IS_OK` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 63 | `AST_IS_ERR` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 64 | `AST_IS_SOME` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 65 | `AST_IS_NONE` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 66 | `AST_IS_NULL` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 67 | `AST_EXIT` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 68 | `AST_SWAP` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 69 | `AST_DEFER` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 70 | `AST_THREAD_BLOCK` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 71 | `AST_USE` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 72 | `AST_ZONE` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 73 | `AST_DATE` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 74 | `AST_DATENOW` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 75 | `AST_TIME` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 76 | `AST_CLOCK` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 77 | `AST_FREE` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 78 | `AST_ZONE_SYNC` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 79 | `AST_ZONE_VIEW` | Current C enum member; OIR ID is fixed independently of its C ordinal |
| 80 | `AST_ZONE_LOCK` | Current C enum member; OIR ID is fixed independently of its C ordinal |

## Field IDs and value types

Field IDs are scoped to a node kind. Unless noted otherwise, nullable pointers and nullable strings must still be represented explicitly: use NODE_REF with ID 0 for a null child, and NULL value type for a null string. An empty string is STRING_REF with length 0.

| Node kind(s) | Field ID | Field | Value type |
|---|---:|---|---|
| AST_PROGRAM | 1 | declarations | NODE_REF |
| AST_BLOCK, AST_ASM_BLOCK | 1 | statements | NODE_REF |
| AST_VAR_DECL | 1 | name | STRING_REF / NULL |
| AST_VAR_DECL | 2 | type_name | STRING_REF / NULL |
| AST_VAR_DECL | 3–6 | is_const, is_hard_typed, is_public, is_exported | BOOL |
| AST_VAR_DECL | 7 | initializer | NODE_REF |
| AST_VAR_DECL | 8 | inline_type | NODE_REF |
| AST_FUNC_DECL, AST_EXTERN_DECL | 1 | name | STRING_REF / NULL |
| AST_FUNC_DECL, AST_EXTERN_DECL | 2 | return_type | STRING_REF / NULL |
| AST_FUNC_DECL, AST_EXTERN_DECL | 3–5 | is_extern, is_public, is_exported | BOOL |
| AST_FUNC_DECL, AST_EXTERN_DECL | 6 | params | NODE_REF |
| AST_FUNC_DECL, AST_EXTERN_DECL | 7 | body | NODE_REF |
| AST_STRUCT_DECL, AST_UNION_DECL | 1 | name | STRING_REF / NULL |
| AST_STRUCT_DECL, AST_UNION_DECL | 2–5 | is_packed, is_union, is_public, is_exported | BOOL |
| AST_STRUCT_DECL, AST_UNION_DECL | 6 | fields | NODE_REF |
| AST_ENUM_DECL | 1 | name | STRING_REF / NULL |
| AST_ENUM_DECL | 2 | variants | NODE_REF |
| AST_ENUM_VARIANT | 1 | name | STRING_REF / NULL |
| AST_ENUM_VARIANT | 2 | data | NODE_REF |
| AST_GROUP_DECL | 1 | name | STRING_REF / NULL |
| AST_GROUP_DECL | 2–3 | is_public, is_exported | BOOL |
| AST_GROUP_DECL | 4 | members | NODE_REF |
| AST_METHOD | 1 | public_name | STRING_REF / NULL |
| AST_METHOD | 2 | implementation | NODE_REF |
| AST_NEWTYPE_DECL | 1 | name | STRING_REF / NULL |
| AST_NEWTYPE_DECL | 2 | target_type | STRING_REF / NULL |
| AST_LITERAL | 1 | token_type | TOKEN_ID |
| AST_LITERAL | 2 | lit_type | U32 |
| AST_LITERAL | 3 | int_value | I64 |
| AST_LITERAL | 4 | float_value | F64 |
| AST_LITERAL | 5 | string_value | STRING_REF / NULL |
| AST_IDENTIFIER | 1 | name | STRING_REF / NULL |
| AST_BINARY_EXPR, AST_ASSIGNMENT, AST_INDEX_ACCESS | 1 | op | TOKEN_ID |
| AST_BINARY_EXPR, AST_ASSIGNMENT, AST_INDEX_ACCESS | 2 | left | NODE_REF |
| AST_BINARY_EXPR, AST_ASSIGNMENT, AST_INDEX_ACCESS | 3 | right | NODE_REF |
| AST_UNARY_EXPR, AST_POSTFIX_EXPR, AST_RETURN, AST_CAST | 1 | op | TOKEN_ID |
| AST_UNARY_EXPR, AST_POSTFIX_EXPR, AST_RETURN, AST_CAST | 2 | operand | NODE_REF |
| AST_FUNC_CALL and intrinsic call kinds | 1 | callee | NODE_REF |
| AST_FUNC_CALL and intrinsic call kinds | 2 | args | NODE_REF |
| AST_MEMBER_ACCESS | 1 | member_name | STRING_REF / NULL |
| AST_MEMBER_ACCESS | 2 | object | NODE_REF |
| AST_TUPLE | 1 | elements | NODE_REF |
| AST_IF | 1–3 | condition, then_branch, else_branch | NODE_REF |
| AST_LOOP | 1 | loop_type | I32 |
| AST_LOOP | 2–7 | init, condition, increment, body, collection, variable | NODE_REF |
| AST_NEXUS_CAPTURE, AST_LISTEN | 1 | handler_name | STRING_REF / NULL |
| AST_NEXUS_CAPTURE, AST_LISTEN | 2 | expr | NODE_REF |
| AST_NEXUS_PIPE, AST_NEXUS_FLOW | 1 | op | TOKEN_ID |
| AST_NEXUS_PIPE, AST_NEXUS_FLOW | 2–3 | left, right | NODE_REF |
| AST_NEXUS_PIPE, AST_NEXUS_FLOW | 4–5 | retry_count, retry_delay | I32 |
| AST_NEXUS_PIPE, AST_NEXUS_FLOW | 6 | catch_block | NODE_REF |
| AST_NEXUS_LABEL | 1 | label_name | STRING_REF / NULL |
| AST_NEXUS_LABEL | 2 | statement | NODE_REF |
| AST_NEXUS_JUMP | 1 | label_name | STRING_REF / NULL |
| AST_NEXUS_HALT | 1 | expr | NODE_REF |
| AST_NEXUS_INTENT | 1 | intent_name | STRING_REF / NULL |
| AST_NEXUS_INTENT | 2 | target | NODE_REF |
| AST_AI_LOGIC | 1 | query | STRING_REF / NULL |
| AST_UNPACK | 1 | expr | NODE_REF |
| AST_PANIC | 1 | message | STRING_REF / NULL |
| AST_DEFER | 1 | body | NODE_REF |
| AST_THREAD_BLOCK | 1 | name | STRING_REF / NULL |
| AST_THREAD_BLOCK | 2–4 | body, mode, timeout | NODE_REF |
| AST_USE | 1 | module_name | STRING_REF / NULL |
| AST_USE | 2 | alias | STRING_REF / NULL |
| AST_BREAK, AST_CONTINUE | — | No node-specific fields | — |

The intrinsic call-kind set is:
AST_SPAWN, AST_ECHO, AST_SEND, AST_RECEIVE, AST_DONE, AST_AREA, AST_ZONE, AST_ADDR, AST_LEN, AST_PEEK, AST_POKE, AST_INB, AST_OUTB, AST_IRQ, AST_INTR, AST_REG, AST_PRNT, AST_PRMT, AST_WRITE, AST_FWRITE, AST_TYPEOF, AST_SIZEOF, AST_IS_OK, AST_IS_ERR, AST_IS_SOME, AST_IS_NONE, AST_IS_NULL, AST_EXIT, AST_SWAP, AST_DATE, AST_DATENOW, AST_TIME, AST_CLOCK, AST_FREE, AST_ZONE_SYNC, AST_ZONE_VIEW and AST_ZONE_LOCK.

## Audit notes

- ASTNode.next is serialized as the node record's next_node_id, not as a node-specific field.
- AST_PROGRAM is the required root kind for a complete compilation-unit AST payload.
- AST_BINARY_EXPR/ASSIGNMENT/INDEX_ACCESS and AST_NEXUS_PIPE/FLOW share C struct layouts but retain distinct node-kind IDs.
- AST_FUNC_DECL/AST_EXTERN_DECL and AST_STRUCT_DECL/AST_UNION_DECL likewise retain distinct IDs.
- AST_BREAK and AST_CONTINUE use only the common ASTNode header.
- The current C AST stores line/column but no end span. AST schema 1.0 therefore carries only start line/column; byte spans can be added in a compatible extension after parser/AST source-span propagation.
