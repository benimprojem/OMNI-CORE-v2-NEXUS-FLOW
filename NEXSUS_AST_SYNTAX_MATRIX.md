# NEXSUS Flow Syntax ↔ AST Coverage Matrix

**Branch:** `refactor/c-core-roadmap`  
**Status:** Initial source-backed comparison; implementation audit continues  
**Gate:** This matrix must be complete before Codegen expansion.

## Comparison rule

A syntax form is complete only if Lexer → Parser → AST payload → AST codec → semantic rules/artifact → lowering has a matching contract and tests. An AST enum tag alone is not completion. “Missing node” below means the current AST model has no dedicated representation; it may not yet prove the parser cannot encode it indirectly.

## Syntax-to-AST comparison

| NEXSUS Flow syntax / construct | Existing AST representation | Initial result | Required action |
|---|---|---|---|
| Program / block `{...}` | `AST_PROGRAM`, `AST_BLOCK` | Present | Check ownership, spans and codec |
| Mutable / constant declaration `v:`, `c:` | `AST_VAR_DECL` | Present | Verify type/hard/public/export/inline type fields |
| Function `f:name(...)` | `AST_FUNC_DECL` | Present | Audit parameter nodes, named args, return type and body |
| Extern function `exf:name(...)` | `AST_EXTERN_DECL` / `ASTFuncDecl` | Present/shared payload | Verify parser, codec, semantic signature |
| Lambda `f:(a,b)>expr` | No dedicated lambda tag | **Missing model** | Add explicit lambda node and grammar/semantic support |
| Anonymous function `f:(a,b){...}` | No dedicated anonymous-function tag | **Missing model** | Decide shared lambda/function-expression payload or distinct node |
| Struct / packed struct | `AST_STRUCT_DECL` | Present | Confirm packed syntax and field type representation |
| Enum + variants/data | `AST_ENUM_DECL`, `AST_ENUM_VARIANT` | Present | Validate payload variants and discriminants |
| Union | `AST_UNION_DECL` / `ASTStructDecl` | Present/shared | Confirm union field layout/type rules |
| NewType `nt:` | `AST_NEWTYPE_DECL` | Present | Confirm target type and nominal semantics |
| Group / methods | `AST_GROUP_DECL`, `AST_METHOD` | Present | Confirm member dispatch and method receiver |
| Module import `use` | `AST_USE` | Present | Verify alias, path, module resolution |
| Literals (number/string/char/bool) | `AST_LITERAL` | Present | Confirm all token/literal variants round-trip |
| Identifier | `AST_IDENTIFIER` | Present | Symbol resolution + use-site metadata |
| Unary / postfix | `AST_UNARY_EXPR`, `AST_POSTFIX_EXPR` | Present | Operator coverage and type rules |
| Binary / assignment | `AST_BINARY_EXPR`, `AST_ASSIGNMENT` | Present | Full operator/precedence/associativity matrix |
| Call / member / index | `AST_FUNC_CALL`, `AST_MEMBER_ACCESS`, `AST_INDEX_ACCESS` | Present | Call target, named args, lvalue and bounds contracts |
| Cast `cast(value,type)` | `AST_CAST` | Present | Type resolution and conversion record |
| Tuple | `AST_TUPLE` | Present | Tuple typing/destructuring contract |
| Array / map / vector forms | No dedicated AST tags apparent | **Representation unclear** | Decide explicit aggregate/type nodes vs generic syntax; inventory parser |
| Unified `loop` (range/count/foreach/conditional/infinite/rolling) | `AST_LOOP` fields init/condition/increment/collection/variable/loop_type | Partial / legacy-shaped | Normalize loop kinds to NEXSUS loop variants; remove/retire while/for spellings from grammar if not language syntax |
| `if / elsif / els` | `AST_IF` condition/then/else | Partial | Verify elsif chain shape and nesting semantics |
| Conditional action `(cond) ? break : continue;` | No dedicated `AST_CONDITIONAL_CONTROL` | **Missing explicit model** | Add condition + true arm + optional false arm; parser must not confuse with Flow `->` |
| Labels `label:>`, jump `label?>` | `AST_NEXUS_LABEL`, `AST_NEXUS_JUMP` | Present | Resolve label IDs and validate scope/control-flow |
| `select` | No dedicated tag | **Missing** | Add select AST and branch/case/default payload |
| `scan` | No dedicated tag | **Missing** | Add scan AST and pattern/arm model |
| Multi-rule engine / ruleset | No dedicated tags (`BARRIER/TRIGGER/ALWAYS/DEFAULT`) | **Missing** | Add ruleset + rule-kind payloads and parser grammar |
| Rule application | No `AST_APPLY_RULES` | **Missing** | Add explicit apply node and semantic/effect contract |
| `on` blocks | No `AST_ON` | **Missing** | Add event/trigger block representation and binding fields |
| Handler capture `(h)<-expr` | `AST_NEXUS_CAPTURE` | Present | Ownership, occupancy, move semantics annotations |
| Flow / pipe `->, >>, <<` | `AST_NEXUS_FLOW`, `AST_NEXUS_PIPE` | Present/shared | Separate operator semantics, preserve retry/catch payload |
| Relocate `_>` | No obvious dedicated tag | **Needs exact parser audit** | Confirm token/parser AST representation; do not silently encode as ordinary binary op |
| Unpack `...>` | `AST_UNPACK` | Present | Verify target/value and ownership behavior |
| Halt `?;`, block `} ?;` | `AST_NEXUS_HALT` | Present | Confirm single-resource vs block cleanup semantics |
| Intent `@` | `AST_NEXUS_INTENT` | Declared / deferred | Do not invent behavior until Intent design is active |
| AI logic `??` | `AST_AI_LOGIC` | Declared / deferred | Define payload and semantics before implementation |
| Panic `panic("...")` | `AST_PANIC` | Present | Current documented literal-only message restriction must be tested |
| Spawn/listen/send/receive/done | Matching AST tags | Present/shared payloads | Thread/event semantics, payload and effects |
| Area/zone/address/len/peek/poke/inb/outb/irq/intr/reg | Matching AST tags | Present/shared call payload | Signature, type, effect and target constraints |
| I/O/reflection/time/free/zone sync/view/lock | Matching AST tags | Present/shared call payload | Complete intrinsic registry and semantic signatures |
| Defer | `AST_DEFER` | Present | Scope exit ordering and cleanup semantics |
| Thread block | `AST_THREAD_BLOCK` | Present | Name/body/mode/timeout and synchronization semantics |
| Inline ASM | `AST_ASM_BLOCK` | Present | ASM payload/constraints/clobbers and target validation |
| `break / continue / return` | `AST_BREAK`, `AST_CONTINUE`, `AST_RETURN` | Present | Validate loop/function targets and return typing |

## Confirmed model-level gaps to address

1. **Lambda and anonymous function expression:** no dedicated AST tag/payload currently declared.
2. **Conditional action:** no explicit condition/true-arm/optional-false-arm node.
3. **Select and scan:** no AST node/payload.
4. **Multi-rule engine:** no Ruleset/Barrier/Trigger/Always/Default node family.
5. **Rule application and on-block:** no `AST_APPLY_RULES` or `AST_ON`.
6. **Loop normalization:** AST currently has legacy `loop_type = Infinite/While/For/Foreach`; NEXSUS syntax decision is unified `loop`, so parser and payload contract need alignment.
7. **Aggregate/type syntax:** arrays/maps/vectors need explicit parser representation and semantic typing decision; current AST enum has no dedicated aggregate/type nodes.
8. **Relocate `_>`:** must be traced from token to parser representation and semantic ownership effect.

## AST node payload/codec audit (all current enum tags)

Every AST enum tag must be covered in all columns below. Shared payload is acceptable only when explicit and semantically valid.

| Check | Required evidence |
|---|---|
| Allocation | `ast_create(type)` returns correct payload size for every tag |
| Parser construction | every grammar production initializes required fields |
| Printer | every tag has intentional output or explicit opaque rendering |
| Serializer | every payload field serialized; no pointer/native-layout serialization |
| Deserializer | same fields restored; invalid tag/field rejected |
| Destroy | every owned string/child freed exactly once |
| Source span | line/column and source byte span preserved |
| Semantic | every executable node has a rule or explicit non-value-node classification |
| Semantic artifact | references/scopes/types/relations consistent |
| Lowering | explicit supported mapping or fail-closed diagnostic |
| Tests | parser + roundtrip + malformed + semantic + integration |

## Next implementation sequence

1. Finish parser grammar trace for all syntax rows above (including README/spec examples).
2. Add the missing AST node contracts in a stable, append-only manner; update OIR AST node registry IDs without renumbering existing IDs.
3. Implement parser construction, allocation, printer, codec, destroy and round-trip tests for each added node.
4. Complete semantic typing/scope/effect contracts and semantic artifact validation.
5. Add lowering coverage and verifier contracts only after frontend rows are complete.
6. Keep Codegen expansion blocked until the matrix is green.

## Source anchors

- `README.md`: control flow (Section 5), functions (Section 6), data structures (Section 7), Nexus Flow operators (Section 3), intrinsic/core operations (Section 14).
- `omnicore-sdk/src/c/parser/ast.h`: current `ASTNodeType` and payload definitions.
- `omnicore-sdk/src/c/parser/parser.c`: grammar and AST construction (114 KB; must be mapped function-by-function).
- `omnicore-sdk/src/c/parser/ast.c`: allocation, print, destroy, serialization (50 KB).
- `FRONTEND_COMPLETENESS_AUDIT.md`: layer-by-layer completion criteria.
- `OIR_AST_NODE_REGISTRY.md`: stable OIR node IDs; append-only rule.
