# AST Node Implementation Matrix

**Branch:** `refactor/c-core-roadmap`  
**Registry:** `AST_PROGRAM` … `AST_ASM_LABEL` (append-only, 105 node kinds)  
**Audit date:** 2026-11-05

## Meaning of columns

- **Parser:** `Direct` means a direct `parser_ast_create(p, AST_...)` call was found in `parser.c`; `Helper/indirect` means no such direct call was found. It is *not* a claim that the parser cannot produce the node; helper constructors, dynamic type selection, and shared representations must be audited separately.
- **Factory / Destroy / Serialize / Deserialize:** the AST lifecycle implementation is centralized in `ast.c`; the generic fixture suite exercises node tags and populated payloads. ✓ is registry/lifecycle presence, not proof every field or malformed path is covered.
- **Graph:** graph validator has explicit contracts for all current tags and fails closed for unknown kinds.
- **OIR map / Writer:** AST-to-OIR registry and AST frontend writer coverage.
- **Fixture:** `test_ast_deserialize.c` has a populate switch for the type. Context-bound node tests must use legal parent contexts; no standalone-root assumption.

## Coverage snapshot

| Area | Audit result |
|---|---:|
| AST enum kinds | 105/105 |
| AST → OIR mapping | 105/105 |
| OIR writer kind cases | 105/105 |
| Populated fixture switch | 105/105 |
| Direct parser construction calls | 62 kinds (not a parser completeness metric) |
| Graph validator | Explicit kind contracts; invalid/unregistered kind fails closed |

## Per-node matrix

| AST node | Parser creation | Factory | Destroy | Serialize | Deserialize | Graph | OIR map | OIR writer | Fixture | 
|---|---|---|---|---|---|---|---|---| 
| `AST_PROGRAM` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_BLOCK` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_VAR_DECL` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_FUNC_DECL` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_STRUCT_DECL` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_ENUM_DECL` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_UNION_DECL` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_NEWTYPE_DECL` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_ENUM_VARIANT` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_GROUP_DECL` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_METHOD` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_EXTERN_DECL` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_LITERAL` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_IDENTIFIER` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_BINARY_EXPR` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_UNARY_EXPR` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_POSTFIX_EXPR` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_ASSIGNMENT` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_FUNC_CALL` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_MEMBER_ACCESS` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_INDEX_ACCESS` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_CAST` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_TUPLE` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_IF` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_LOOP` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_RETURN` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_BREAK` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_CONTINUE` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_ASM_BLOCK` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_NEXUS_CAPTURE` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_NEXUS_PIPE` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_NEXUS_FLOW` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_NEXUS_LABEL` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_NEXUS_JUMP` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_NEXUS_HALT` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_NEXUS_INTENT` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_AI_LOGIC` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_UNPACK` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_PANIC` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_SPAWN` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_LISTEN` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_ECHO` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_SEND` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_RECEIVE` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_DONE` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_AREA` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_ADDR` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_LEN` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_PEEK` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_POKE` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_INB` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_OUTB` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_IRQ` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_INTR` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_REG` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_PRNT` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_PRMT` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_WRITE` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_FWRITE` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_TYPEOF` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_SIZEOF` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_IS_OK` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_IS_ERR` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_IS_SOME` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_IS_NONE` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_IS_NULL` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_EXIT` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_SWAP` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_DEFER` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_THREAD_BLOCK` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_USE` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_ZONE` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_DATE` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_DATENOW` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_TIME` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_CLOCK` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_FREE` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_ZONE_SYNC` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_ZONE_VIEW` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_ZONE_LOCK` | Helper/indirect | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_SELECT` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_SELECT_ARM` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_SCAN` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_SCAN_ARM` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_PATTERN_BINDING` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_PATTERN_CONSTRUCTOR` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_RULESET_DECL` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_RULE_BARRIER` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_RULE_TRIGGER` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_RULE_ALWAYS` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_RULE_DEFAULT` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_APPLY_RULES` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_ON_BLOCK` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_LAMBDA` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_RELOCATE` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_UNROLL` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_ARRAY_LITERAL` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_ARRAY_TYPE` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_ARRAY_DIMENSION` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_BLOCK_HALT` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_VARIADIC_PARAM` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_NAMED_ARG` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_MUST_DECL` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_FASTEXEC_BLOCK` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |
| `AST_ASM_LABEL` | Direct | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | fixture |

## Context-specific codec fixtures

| Node | Required owner/context | Test |
|---|---|---|
| `AST_MUST_DECL` | `AST_PROGRAM.declarations` | Contextual serialize → deserialize → validate → canonical re-encode |
| `AST_VARIADIC_PARAM` | `AST_FUNC_DECL.params`, terminal parameter | Contextual round-trip |
| `AST_BLOCK_HALT` | `AST_BLOCK` / allowed cleanup block, final statement | Dedicated block-halt round-trip |
| `AST_ASM_LABEL` | `AST_FASTEXEC_BLOCK.statements` | FASTEXEC raw ASM payload round-trip |
| `AST_NAMED_ARG` | Call/intrinsic args list | Populated fixture; call-parent validation is separately exercised |

## Remaining AST closure criteria

1. Parser production inventory: map every syntax production in `NEXSUS_AST_SYNTAX_MATRIX.md` to parser function(s), resulting AST kind(s), and positive/negative test(s). The direct-constructor count above is only a source scan.
2. Validate every source-bearing parser production's start/end offsets and line/column, including nested and synthesized nodes.
3. Complete contextual codec tests for named arguments and any other context-restricted nodes; ensure malformed parent/position cases reject.
4. Add deterministic allocation-failure injection for AST construction, codec decode and partial-child cleanup where feasible.
5. Reconcile this matrix with `OCC_PIPELINE_PROGRESS.md` and `AST_NODE_OWNERSHIP_AUDIT.md`; mark AST complete only after tests and CI confirm all required cases.

**Gate:** Semantic/OIR completion work does not replace the AST closure checklist. Codegen remains gated.
