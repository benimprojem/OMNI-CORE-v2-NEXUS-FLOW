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
| `select` | `AST_SELECT` + ordered `AST_SELECT_ARM`; literal, binding and constructor patterns | **Implemented in C parser/AST/OIR bridge; CI pending** | Semantic pattern binding/exhaustiveness and lowering remain gated |
| `scan` | `AST_SCAN` + ordered `AST_SCAN_ARM`; binding and constructor pattern nodes | **Implemented in C parser/AST/OIR bridge; CI pending** | Semantic pattern binding and scan execution/lowering remain gated |
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
3. **Select and scan:** implemented in C AST/parser and AST→OIR bridge; semantic binding/exhaustiveness and lowering remain pending.
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


## Parser source trace — confirmed coverage (2026-11-02)

The parser source was inspected by grammar entry points and AST construction sites. This narrows several initial hypotheses:

| Syntax | Parser evidence | AST encoding | Status after source trace |
|---|---|---|---|
| `if / elsif / els` | `parse_if_statement()` recursively parses `elsif` | `AST_IF.else_branch` contains nested `AST_IF` or final else branch | Parser representation exists; semantic/roundtrip chain tests still required |
| Conditional action `(cond) -> break/continue/...` | `parse_conditional_action()`, gated by `parser_looks_like_conditional_action()` | `AST_IF` with condition and then branch | Explicit separate node is not necessary if this canonical encoding is documented/tested; parser ambiguity tests remain |
| Unified `loop` forms | `parse_primary()` contains loop construction; `ASTLoop.loop_type` currently 0..3 | `AST_LOOP` fields init/condition/increment/collection/variable/body | Partial: range/foreach/while/for/infinite parser paths exist, but README's rolling/timed loop form and `unroll` need distinct syntax/metadata audit |
| `select / scan` | Contextual `parse_select_scan()` at statement position; `parse_pattern()` handles literal, binding and constructor patterns | `AST_SELECT`, `AST_SELECT_ARM`, `AST_SCAN`, `AST_SCAN_ARM`, `AST_PATTERN_BINDING`, `AST_PATTERN_CONSTRUCTOR` | **Parser, AST codec and OIR frontend bridge implemented; CI pending** |
| Multi-rule `rules ...` | No rules parser or statement branch found | No AST tag | **Not implemented in C parser/model** |
| `apply rules(...)` | No apply-rules grammar branch found | No AST tag | **Not implemented in C parser/model** |
| `on Rule.Event { ... }` | No on-block grammar branch found | No AST tag | **Not implemented in C parser/model** |
| Lambda `f:(a,b)>expr` / anonymous `f:(a,b){...}` | `parse_func_decl()` handles declarations; no function-expression/lambda production found | No lambda node | **Not implemented as expression in C parser/model** |
| Relocate `_>` | No dedicated relocate parse production found in parser entry points | No relocate node/tag | **Not implemented as a distinct AST operation**; token/lexer may exist but AST contract absent |
| Intrinsics | `parse_intrinsic_call()` plus keyword dispatch in `parse_primary()` | Mostly shared `ASTCall`; `AST_CAST` is dedicated | Present in parser; intrinsic signature/effect registry still required |
| Index/postfix/binary | `parse_postfix()`, `parse_unary()`, precedence functions, `parser_make_binary()` | Index shares `ASTBinaryExpr`; postfix/unary share `ASTUnaryExpr` | Shared layouts intentional-looking but must be field-verified in codec/semantic code |

### Important structural finding

`ASTNodeType` now includes dedicated select/scan and pattern kinds. It still lacks ruleset/rule, apply-rules, on-block, lambda/function-expression and relocate nodes. These are README language constructs, not speculative additions. Their grammar and semantic design must be completed before Codegen. Conditional action, in contrast, already has a parser path and can remain an `AST_IF` canonical form unless the language contract requires preserving source form.

### Scope boundary

This source trace proves parser/model presence or absence only. It does **not** claim semantic, AST wire codec, artifact or OIR lowering completion. Those layers remain separately gated in the matrix.


## README examples → minimum AST grammar contracts

This is the minimum structure required by the language examples (not a claim that the parser already implements it).

### Select / scan

- `select(expr) { arm* default? }`: selector expression plus ordered arm list; each arm has pattern/label and body; default is explicit and unique.
- `scan(expr) { pattern -> body; ... }`: scan input plus ordered pattern arms; patterns can be enum variants/constructor patterns (e.g. `Range(s,e)`) and bind variables.
- Pattern matching must bind `s,e,i,list,cond` into an arm-local scope; AST cannot encode these only as ordinary call expressions without semantic pattern metadata.

### Multi-rule engine

- `rules Name(params) { rule* }`: named rule declaration, parameters, ordered rules.
- Barrier `condition :=> message`: condition + message expression; barrier is a terminating/failure effect, not ordinary assignment.
- Trigger `condition -> ActionName`: condition + trigger identifier.
- `always -> Action` and `default -> Action`: explicit rule kinds with distinct execution semantics (unconditional vs no-match fallback).
- `apply rules(Name(args))`: named rule-set invocation and argument list; function-local rule injection/effect context must be represented.
- `on Name.Event { body }`: qualified event name + body; handler binding is scoped to the apply context.

### Lambda / anonymous function

- Expression lambda `f:(a,b)>expr`: parameter list + expression body + inferred/declared return type contract.
- Block anonymous function `f:(a,b){...}`: parameter list + block body; may be used as group member or value depending on language rule. Do not conflate declaration `f:name(...)` with expression forms.
- Function types (README callback examples) need a type representation for parameter types and return type; a string-only `type_name` cannot carry nested signatures reliably.

### Relocate

- `source _> target`: distinct relocation/move operation with source and destination/context operands, ordered before flow operators as README states.
- Semantic contract must mark source moved/invalidated and destination initialized; lowerer must emit an explicit move/relocation effect or reject it. It must not be silently lowered as a normal binary arithmetic operation.

### Unified loop extensions

- Preserve existing four loop shapes only where syntax maps clearly: infinite, condition, init+condition+increment, foreach/range.
- Rolling `?(count, delay)->target` needs retry count/delay metadata and retry semantics; existing ASTNexusFlow has retry fields but parser assignment and units must be verified.
- `unroll(count) -> loop(...)`: compile-time directive/loop transform, not a runtime intrinsic call; define AST representation and constant-bound validation.

## C implementation audit (source-verified)

Implementation files are under `omnicore-sdk/src/c/parser/`:

- `ast.h`: `ASTNodeType`, base ownership/sibling contract and payload declarations.
- `ast.c`: `get_node_size()`, `ast_print_recursive()`, `ast_destroy_recursive()`, `ast_serialize_recursive()`, `ast_deserialize_one()` are all node-type switches. A new node is incomplete until all five paths are updated.
- `parser.c`: grammar productions and AST construction; adding a type to `ast.h` alone does not make syntax parseable.
- `lexer.h` / lexer implementation: keyword/token additions must be coordinated with parser token dispatch.
- `tests/test_parser.c`: parse assertions and AST round-trip tests are the acceptance surface; ownership tests should include destroy after deserialize.

**Implementation order:**
1. Select + scan + arm/pattern payloads, parser and AST codec round-trip.
2. Rule-set declaration + barrier/trigger/always/default + apply/on, with scoped semantic behavior.
3. Lambda/function-expression + function-type representation.
4. Relocate operator + move/ownership semantics.
5. Rolling/unroll loop parity and parser/semantic tests.
6. Run frontend suite and CI; only then mark syntax matrix complete.

## AST schema decision (proposed, pending implementation)

Append new node IDs only; do not alter IDs 1–80. Proposed family: `AST_LAMBDA`, `AST_SELECT`, `AST_SELECT_ARM`, `AST_SCAN`, `AST_SCAN_ARM`, `AST_RULESET_DECL`, `AST_RULE_BARRIER`, `AST_RULE_TRIGGER`, `AST_RULE_ALWAYS`, `AST_RULE_DEFAULT`, `AST_APPLY_RULES`, `AST_ON_BLOCK`, `AST_RELOCATE`, and `AST_UNROLL`. These names are a design proposal, not yet added to `ast.h`; exact enum/payload/codecs/tests should be implemented as one coherent patch after grammar and ownership rules are agreed.


### Select/scan concrete representation (implementation contract)

To avoid consuming otherwise legal identifiers as reserved words, the C parser will recognize `select` and `scan` contextually at statement position only when the identifier is immediately followed by `(`. The lexer token stream remains backward-compatible; ordinary identifiers named `select` or `scan` remain usable in other contexts.

Canonical AST payloads:

- `AST_SELECT`: `selector` expression and ordered `arms` sibling list.
- `AST_SELECT_ARM`: `pattern` expression/pattern node, `body` statement/block, and an explicit `is_default` flag. Default is represented as an arm (not a magic identifier); at most one default arm is valid.
- `AST_SCAN`: `subject` expression and ordered `arms` sibling list.
- `AST_SCAN_ARM`: `pattern` node and `body`; pattern bindings are represented by pattern nodes and later introduced into an arm-local semantic scope.

Pattern nodes are not equivalent to calls. A constructor pattern such as `Range(s,e)` must preserve constructor name and ordered binding children without resolving `Range` as a function. The first implementation will add `AST_PATTERN_BINDING` and `AST_PATTERN_CONSTRUCTOR`; literal and wildcard patterns reuse literal/identifier forms only where their semantics are unambiguous.

Parser grammar:
```text
select "(" expression ")" "{" select_arm* "}"
select_arm := "default" "->" statement
            | pattern "->" statement
scan "(" expression ")" "{" scan_arm* "}"
scan_arm := pattern "->" statement
pattern := identifier
         | identifier "(" binding ("," binding)* ")"
         | literal
```

Validation rules: reject duplicate defaults; reject empty constructor patterns; preserve arm order; require a body after each arrow; reject malformed/missing closing delimiters with parser diagnostics and recovery at the next arm boundary. Semantic analysis will check exhaustiveness only when the subject has a closed enum/union type; otherwise default remains optional and unmatched behavior is defined by the select/scan language contract. Pattern-bound names are arm-local and cannot leak into sibling arms.

Wire format: each node serializes only explicit scalar flags and owned child subtrees; no raw pointers, `size_t`, or compiler-private struct layout is written. New node IDs append after the current maximum and are never inserted among existing IDs. All four construct nodes plus pattern nodes must be registered in allocation-size, print, destroy, serialize and deserialize switches before parser construction is enabled.


### OIR frontend AST bridge — required integration (source-verified, 2 Kasım 2026)

The select/scan AST extension must also be registered in the canonical AST-to-OIR frontend bridge. The current bridge is a closed set of explicit node/field mappings; adding AST tags without these mappings causes indexing/materialization failure or silently omits child relationships.

Affected files and responsibilities:

- `omnicore-sdk/src/c/oir/oir_ast_frontend_index.c`: `oir_ast_frontend_child_at()` defines the canonical child-field mapping, and `oir_ast_frontend_child_field_count()` defines each node's field count. New select/scan nodes and pattern nodes need stable, ordered field IDs here.
- `omnicore-sdk/src/c/oir/oir_ast_frontend_writer.c`: node-specific scalar/string fields and child references must be emitted using the same field IDs.
- `omnicore-sdk/src/c/oir/oir_ast_frontend_reader.c`: reader must allocate the correct AST payload size, restore owned strings/scalars, and attach every child field with ownership checks.
- Existing AST binary codec in `parser/ast.c` remains a separate compatibility surface; OIR bridge support does not replace its print/destroy/serialize/deserialize integration.

Implementation status (2 Kasım 2026): the six node kinds are appended to `ast.h`; all five `ast.c` lifecycle/codec switches and contextual parser grammar are updated; OIR IDs 81–86, child index, writer, reader and field schema are registered. Parser/AST binary tests and OIR payload round-trip tests are added. **CI passed:** run 41451812569 — Windows, Ubuntu and macOS build/test; Ubuntu ASan/UBSan and Valgrind.

Recommended canonical field order for the proposed nodes (field IDs are OIR schema IDs, not C struct offsets):

| Node | Field 1 | Field 2 | Field 3 |
|---|---|---|---|
| `AST_SELECT` | selector | ordered arms | — |
| `AST_SELECT_ARM` | pattern | body | is_default scalar |
| `AST_SCAN` | subject | ordered arms | — |
| `AST_SCAN_ARM` | pattern | body | — |
| `AST_PATTERN_BINDING` | binding name | — | — |
| `AST_PATTERN_CONSTRUCTOR` | constructor name | ordered bindings | — |

The bridge must preserve arm order and pattern-binding order. The default flag is scalar metadata, not a child reference. These field assignments remain provisional until checked against the OIR node-kind and field-kind registries in `oir_ast_map.h` / `oir_ast_map.c`.


**OIR field-model correction from registry inspection:** `oir_ast_map.h` now fixes the registry at 86 stable OIR node kinds, and `oir_ast_map.c` maps AST tags to explicit stable IDs (it does not cast enum ordinals). The child-field counts in `oir_ast_frontend_index.c` count child references only; scalar flags and strings are encoded by the writer's scalar/string field records and must not be assigned child-field IDs. Therefore the provisional table above is refined as follows:

| Node | Child field 1 | Child field 2 | Scalar/string metadata |
|---|---|---|---|
| `AST_SELECT` | selector | ordered arms | — |
| `AST_SELECT_ARM` | pattern | body | `is_default` integer flag |
| `AST_SCAN` | subject | ordered arms | — |
| `AST_SCAN_ARM` | pattern | body | — |
| `AST_PATTERN_BINDING` | — | — | binding name string |
| `AST_PATTERN_CONSTRUCTOR` | ordered bindings | — | constructor name string |

The six new OIR node kinds must be appended to the stable registry with IDs 81–86 (in the agreed canonical order) and `OIR_AST_NODE_KIND_COUNT` increased accordingly. Existing IDs 1–80 must remain byte-for-byte stable. This is a format-level schema change and requires updating the published OIR AST registry and its compatibility/round-trip tests in the same implementation series.


### Implementation note — 2 Kasım 2026

`select` and `scan` are now implemented in the C frontend and OIR AST bridge. Recognition is contextual (identifier followed by `(` at statement position); `Range(s,e)` is represented by a constructor-pattern node, not a function call. Default arms have an explicit boolean flag. The C parser tests cover successful AST construction, duplicate defaults, empty constructor patterns and binary AST round-trip; the OIR round-trip test covers the new AST payloads. Semantic arm-local symbol introduction, type-aware exhaustiveness, and lowering are still separate pre-Codegen tasks. Cross-platform CI passed (run 41451812569: Windows, Ubuntu, macOS, ASan/UBSan and Valgrind).
