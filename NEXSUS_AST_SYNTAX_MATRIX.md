# NEXSUS Flow Syntax ↔ AST Coverage Matrix

**Branch:** `refactor/c-core-roadmap`  
**Status:** Parser, AST codec and OIR bridge extensions implemented for select/scan, rules, apply/on, lambda, relocate and unroll; semantic/lowering coverage remains open  
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
| Lambda `f:(a,b)>expr` / block lambda `f:(a,b){...}` | `AST_LAMBDA` | Parser/AST/OIR implemented; expression/block bodies, parameter scopes and semantic-result scope visibility tested | Function-type model, capture rules, return inference and lowering remain open |
| Anonymous function `f:(a,b){...}` | `AST_LAMBDA` shared with expression lambda | Parser/AST/OIR and block-body codec implemented | Function-type representation, captures and return inference remain open |
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
| Exponentiation `^^` | `parse_power()` builds `AST_BINARY_EXPR(TOKEN_POWER)`; right-associative; tighter than unary minus | Parser production + basic parser fixture added | Semantic matching numeric operand validation is present/tested; promotion/overflow and OIR opcode/lowering remain open |
| Call / member / index | `AST_FUNC_CALL`, `AST_MEMBER_ACCESS`, `AST_INDEX_ACCESS` | Present | Call target, named args, lvalue and bounds contracts |
| Cast `cast(value,type)` | `AST_CAST` | Present | Type resolution and conversion record |
| Tuple | `AST_TUPLE` | Present | Tuple typing/destructuring contract |
| Array literal `[a, b, ...]` (including nested/empty arrays) | `AST_ARRAY_LITERAL` with ordered element chain | Parser/AST codec and OIR kind 97 implemented; nested/empty and malformed-delimiter tests added; homogeneous literal element checks and generic aggregate inference implemented | Map type/signature and vector typing remain open |
| Array indexing `array[index]` and indexed assignment `array[index] = value` | `AST_INDEX_ACCESS` with separate target/index children; assignment keeps index node as its lvalue | Parser, AST codec, semantic array-target and integer-index checks; parser/codec and semantic valid/invalid fixtures added | Bounds checking, multidimensional flattening and load/store OIR lowering remain open |
| Array type declarators `T[]`, `T[n]`, `T[m][n]` and function parameter arrays | `AST_ARRAY_TYPE` (OIR 98) with ordered `AST_ARRAY_DIMENSION` nodes (OIR 99); attached through the existing `ASTVarDecl.inline_type` child while the base element type remains in `type_name` | Parser, AST binary codec, OIR frontend writer/reader and semantic aggregate type records added; dynamic/fixed/multidimensional and parameter forms have dedicated tests | Fixed extents accept positive integer literals and compile-time integer arithmetic (`+`, `-`, `*`, `/`, `%`) with overflow/divide-by-zero rejection; modulo requires integer operands; function-call arguments validate element type, rank, fixed extents and dynamic `[]` extents; executable lowering remains open |
| Unified `loop` (range/count/foreach/conditional/infinite) + `unroll` | `AST_LOOP`, `AST_UNROLL`; rolling retry is represented by `AST_NEXUS_FLOW.retry_count/retry_delay` | Parser/AST/OIR forms implemented; foreach `in`, `unroll(count)->loop`, and `?(count, delay ms)->target` are parsed with bounded integer validation | Runtime retry semantics, constant-bound validation and canonical loop normalization/lowering remain open |
| `if / elsif / els` | `AST_IF` condition/then/else | Partial | Verify elsif chain shape and nesting semantics |
| Conditional action `(cond) -> break/continue/...` | `AST_IF` canonical form | Parser path exists and is disambiguated from Flow pipe | Complete semantic loop-context and round-trip coverage; no separate node required unless source-form preservation becomes a language requirement |
| Labels `label:>`, jump `label?>` | `AST_NEXUS_LABEL`, `AST_NEXUS_JUMP` | Present | Resolve label IDs and validate scope/control-flow |
| `select` | `AST_SELECT` + ordered `AST_SELECT_ARM`; literal, binding and constructor patterns; `when` guard is canonicalized as an `AST_IF` body wrapper | Parser/AST/OIR and semantic arm-local binding, guard bool-check and duplicate-binding checks implemented | Closed-enum constructor/type resolution, exhaustiveness and lowering remain gated |
| `scan` | `AST_SCAN` + ordered `AST_SCAN_ARM`; binding and constructor pattern nodes; `when` guard is canonicalized as an `AST_IF` body wrapper | Parser/AST/OIR and semantic arm-local binding, guard bool-check and duplicate-binding checks implemented | Subject-aware pattern binding type inference and scan execution/lowering remain gated; final `else` catch-all is implemented as a null pattern |
| Multi-rule engine / ruleset | `AST_RULESET_DECL`, `AST_RULE_BARRIER`, `AST_RULE_TRIGGER`, `AST_RULE_ALWAYS`, `AST_RULE_DEFAULT` | Parser/AST/OIR implemented; semantic ruleset symbols/signatures, boolean conditions, string barriers and trigger event registration implemented | Rule ordering/termination/effect execution and lowering remain open |
| Rule application | `AST_APPLY_RULES` | Parser/AST/OIR implemented; semantic ruleset lookup and argument count/type checks implemented | Validate active apply context and model injected effects in semantic AST/lowering |
| `on` blocks | `AST_ON_BLOCK` | Parser/AST/OIR implemented; semantic ruleset and trigger-event name validation implemented | Validate active apply context and lower trigger dispatch |
| Handler capture `(h)<-expr` | `AST_NEXUS_CAPTURE` | Present | Ownership, occupancy, move semantics annotations |
| Flow / pipe `->, >>, <<` | `AST_NEXUS_FLOW`, `AST_NEXUS_PIPE` | Present/shared | Separate operator semantics, preserve retry/catch payload |
| Relocate `_>` | `AST_RELOCATE` | Lexer/parser/AST/OIR implemented; semantic source/destination type compatibility check implemented | Source invalidation, destination ownership initialization and explicit lowering effect remain open |
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
| Inline ASM (`fastexec`, `asm: LABEL { ... }`) | `AST_ASM_BLOCK` exists as generic block payload only | **Parser gap confirmed:** current `parser.c` has no consumption of `TOKEN_KW_FASTEXEC`, `TOKEN_KW_ASM`, or `AST_ASM_BLOCK`; ASM label/raw instruction payload has no dedicated AST representation | Define C-safe ASM payload/label node contract and token/source capture before Parser implementation; `asmcall`/`asmjmp` tokens also have no Parser consumption |
| `break / continue / return` | `AST_BREAK`, `AST_CONTINUE`, `AST_RETURN` | Present | Validate loop/function targets and return typing |

## Operator parser coverage — source audit (2026-11-03)

The syntax reference lists the language's designed operator inventory. The C Lexer token inventory is broader than the current Parser expression grammar. The following status is based on token-consumption sites in `omnicore-sdk/src/c/parser/parser.c`; a token declaration alone is not parser support.

| Operator family | Current C Parser status | Remaining contract |
|---|---|---|
| Assignment `=`, directive assignment `!!=` | Consumed by `parse_assignment()`; creates `AST_ASSIGNMENT` | Lvalue validation and semantic operator rules |
| Arithmetic `+`, `-` | Consumed by `parse_additive()` | Semantic numeric promotion and overflow behavior |
| Arithmetic `*`, `/` | Consumed by `parse_term()` | Division-by-zero and numeric semantic rules |
| Modulo `%` | Consumed by `parse_term()` at the same precedence as `*` and `/`; AST uses `AST_BINARY_EXPR` | Integer-only semantic rule; parser AST/codec regression covers expression and array extent |
| Exponentiation `^^` | `parse_power()` consumes `TOKEN_POWER`; right-associative (`right=parse_unary()`), precedence tighter than multiplicative and unary minus is applied outside power (`-2^^2` parses as `-(2^^2)`) | Parser production exists; confirm precedence/associativity with explicit AST-shape fixtures and ensure semantic/lowering coverage remains gated |
| Comparison `<`, `<=`, `>`, `>=` | Consumed by `parse_comparison()` | Semantic comparability rules |
| Equality `==`, `!=`, `===`, `==!` | Consumed by `parse_equality()` | Define and validate strict/forced equality semantics |
| Logical `&&`, `||` | Parser now consumes `&&` in `parse_logical_and()` and `||` in `parse_logical_or()`; AST uses `AST_BINARY_EXPR`; `&&` binds tighter than `||` | Semantic bool-operand checks are present and tested; short-circuit execution must be verified in OIR/lowering |
| Unary logical not `!` | Consumed by `parse_unary()` | Boolean operand/type rule |
| Unary bitwise not `~` | Consumed by `parse_unary()` | Integer operand/type rule |
| Binary bitwise `&`, `|`, `^` | Parser productions; precedence AND > XOR > OR; AST_BINARY_EXPR | Parser fixture added | Semantic matching integer operand validation is present/tested; shift-count range/width and OIR lowering remain open |
| Shift `<<`, `>>` | Consumed by Nexus Flow expression/flow parsing as flow operators; not implemented as bit-shift arithmetic | Resolve the existing language-level collision before adding bit-shift expression semantics |
| Prefix/postfix `++`, `--` | Consumed by unary/postfix parsing | Lvalue and numeric semantic checks |
| Compound assignment `+=`, `-=`, `*=`, `/=`, `|=`, `&=`, `^=` | `AST_ASSIGNMENT` keeps the original operator token; parser fixture covers `+=` and `&=` | Parser production/test added | Semantic lvalue + equivalence checks |
| String append `.=` | `AST_ASSIGNMENT` keeps `TOKEN_STRING_APPEND` | Parser production added; dedicated fixture/semantic test still needed |
| Range `..` | Consumed by `parse_range()` | Range type and endpoint semantics |

This table is an implementation audit, not a change to the language design. Unsupported forms must not be marked complete merely because the Lexer emits tokens. In particular, `&&` and `||` occur in existing language examples but currently cannot pass the C Parser; their short-circuit semantics are a precondition for closing that gap.

## Confirmed model-level gaps to address

1. **Array type declarators:** `AST_ARRAY_TYPE` and `AST_ARRAY_DIMENSION` represent dynamic, fixed and multidimensional shapes, including function parameters. Function calls validate element type, rank and fixed/dynamic extents; remaining work is executable lowering.
2. **Map types and values:** `map key,value` type syntax and key/value ownership operations need an explicit semantic/type contract; no map literal syntax is specified in README.
3. **Vector typing:** `Vec2/Vec3/Vec4` currently rely on array-shaped literals; dimensionality and SIMD type rules must be checked semantically.
4. **Loop normalization:** legacy `loop_type = Infinite/While/For/Foreach` remains the frontend payload; canonical lowering and runtime semantics are still open.
5. **Conditional action:** parser canonicalizes it to `AST_IF`; loop-context semantic checks and source-form round-trip remain open.
6. **Ownership/effects:** relocate invalidation, handler capture, unpack, defer and halt cleanup need complete semantic effect records and lowering.
7. **Intent/AI logic:** declared/deferred; do not invent semantics before their specs are activated.
8. **Pattern binding type inference:** `semantic_check_pattern_bindings()` currently creates `any` slots without the select/scan subject type. Enum declaration analysis already records payload field types in Semantic Result relations, but the arm checker does not resolve a constructor pattern against that subject enum/variant. This is the reason a guard such as `when a > 0` cannot yet rely on the payload field's numeric type; subject-aware constructor resolution must close before those guards are claimed semantically complete.

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
- `omnicore-sdk/docs/syntax.md`: canonical NEXUS Flow syntax and language-semantics reference.
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
| Unified `loop` + rolling retry | `parse_primary()` builds loop kinds; `parse_unroll()` builds compile-time directive; `parse_nexus_flow()` handles `?(count, delay ms)->` | `AST_LOOP`, `AST_UNROLL`, `AST_NEXUS_FLOW.retry_count/retry_delay` | Foreach `in`, unroll and `ms` retry delay are parser/AST/OIR represented; runtime retry behavior, bounds and lowering remain open |
| `select / scan` | Contextual `parse_select_scan()` at statement position; `parse_pattern()` handles literal, binding and constructor patterns | `AST_SELECT`, `AST_SELECT_ARM`, `AST_SCAN`, `AST_SCAN_ARM`, `AST_PATTERN_BINDING`, `AST_PATTERN_CONSTRUCTOR` | Parser, AST codec, OIR bridge and arm-local semantic scopes implemented; exhaustiveness remains open |
| Multi-rule `rules ...` | `parse_ruleset()` plus explicit ruleset/barrier/trigger/always/default nodes | `AST_RULESET_DECL`, `AST_RULE_BARRIER`, `AST_RULE_TRIGGER`, `AST_RULE_ALWAYS`, `AST_RULE_DEFAULT` | Parser/AST/OIR and baseline semantic rule checks implemented; execution effects remain open |
| `apply rules(...)` | `parse_apply_rules()` and ruleset signature lookup | `AST_APPLY_RULES` | Parser/AST/OIR and semantic argument count/type checks implemented; injected effect semantics remain open |
| `on Rule.Event { ... }` | `parse_on_block()` and ruleset trigger-event registry | `AST_ON_BLOCK` | Parser/AST/OIR and semantic ruleset/event validation implemented; active-apply context and dispatch lowering remain open |
| Lambda `f:(a,b)>expr` / anonymous `f:(a,b){...}` | `parse_lambda()` from anonymous `f:(` prefix | `AST_LAMBDA` | Expression/block parser, codec, OIR and parameter scopes implemented; function types/captures remain open |
| Relocate `_>` | Lexer handles `_>` before identifiers; `parse_nexus_flow()` constructs dedicated move node | `AST_RELOCATE` | Parser/AST/OIR and source/destination type compatibility implemented; move invalidation remains open |
| Intrinsics | `parse_intrinsic_call()` plus keyword dispatch in `parse_primary()` | Mostly shared `ASTCall`; `AST_CAST` is dedicated | Present in parser; intrinsic signature/effect registry still required |
| Index/postfix/binary | `parse_postfix()`, `parse_unary()`, precedence functions, `parser_make_binary()` | Index shares `ASTBinaryExpr`; postfix/unary share `ASTUnaryExpr` | Shared layouts intentional-looking but must be field-verified in codec/semantic code |

### Important structural finding

`ASTNodeType` now includes dedicated select/scan/pattern and rules/apply/on/lambda/relocate/unroll kinds. Parser, AST codec and OIR bridge coverage exists for these syntax forms; baseline semantic scopes, rule signatures/events and type checks are also present. Exhaustiveness, full ownership/effect semantics and lowering remain pre-Codegen gates. Conditional action remains an `AST_IF` canonical form.

### Scope boundary

This source trace proves parser/model presence or absence only. It does **not** claim semantic, AST wire codec, artifact or OIR lowering completion. Those layers remain separately gated in the matrix.


## README examples → minimum AST grammar contracts

This section records the minimum structure required by the language examples and the implementation contracts now in place.

### Select / scan

- `select(expr) { arm* default? }`: selector expression plus ordered arm list; each arm has pattern/label and body; default is explicit and unique.
- `scan(expr) { pattern -> body; ... }`: scan input plus ordered pattern arms; patterns can be enum variants/constructor patterns (e.g. `Range(s,e)`) and bind variables.
- Pattern matching binds constructor arguments into arm-local semantic scopes; duplicate bindings in one arm are rejected. Closed-enum constructor resolution and exhaustiveness are still pending.

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
- Rolling `?(count, delay ms)->target` is parsed into `AST_NEXUS_FLOW.retry_count/retry_delay`; `ms` is consumed explicitly and numeric overflow is rejected. Runtime retry scheduling/semantics remain to be defined and lowered.
- `unroll(count) -> loop(...)`: represented by `AST_UNROLL`; semantic analysis currently accepts only a non-negative integer literal. Constant-expression evaluation and actual compile-time expansion remain pending.

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

## AST schema decision (implemented)

New node IDs append only; IDs 1–80 remain unchanged. IDs 81–86 are select/scan and pattern nodes; IDs 87–96 are ruleset/rule, apply/on, lambda, relocate and unroll. C payloads, parser construction, AST codec lifecycle and OIR bridge are implemented. Semantic AST/effect rules and lowering are intentionally not marked complete.


### Select/scan concrete representation (implementation contract)

To avoid consuming otherwise legal identifiers as reserved words, the C parser will recognize `select` and `scan` contextually at statement position only when the identifier is immediately followed by `(`. The lexer token stream remains backward-compatible; ordinary identifiers named `select` or `scan` remain usable in other contexts.

Canonical AST payloads:

- `AST_SELECT`: `selector` expression and ordered `arms` sibling list.
- `AST_SELECT_ARM`: `pattern` expression/pattern node, `body` statement/block, and an explicit `is_default` flag. Default is represented as an arm (not a magic identifier); at most one default arm is valid.
- `AST_SCAN`: `subject` expression and ordered `arms` sibling list.
- `AST_SCAN_ARM`: `pattern` node and `body`; pattern bindings are represented by pattern nodes and later introduced into an arm-local semantic scope.

Pattern nodes are not equivalent to calls. A constructor pattern such as `Range(s,e)` preserves constructor name and ordered binding children without resolving `Range` as a function. `AST_PATTERN_BINDING` and `AST_PATTERN_CONSTRUCTOR` are implemented; literal patterns use literal nodes, while identifier bindings are introduced into arm-local semantic scopes.

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

Validation rules: duplicate defaults and empty constructor patterns are rejected; arm order is preserved; bodies and delimiters are required with parser recovery. Semantic analysis creates arm-local pattern scopes and rejects duplicate bindings. Type-aware closed-enum exhaustiveness and unmatched behavior remain open.

Wire format: each node serializes only explicit scalar flags and owned child subtrees; no raw pointers, `size_t`, or compiler-private struct layout is written. New node IDs append after the current maximum and are never inserted among existing IDs. All new nodes are registered in allocation-size, print, destroy, serialize and deserialize switches.


### OIR frontend AST bridge — implemented integration (source-verified, 2 Kasım 2026)

All AST syntax extensions are registered in the canonical AST-to-OIR frontend bridge. The bridge is a closed set of explicit node/field mappings; every new tag has a matching index, writer, reader and field schema.

Affected files and responsibilities:

- `omnicore-sdk/src/c/oir/oir_ast_frontend_index.c`: `oir_ast_frontend_child_at()` defines the canonical child-field mapping, and `oir_ast_frontend_child_field_count()` defines each node's field count. Select/scan, rule, lambda, relocate and unroll nodes now have stable, ordered field IDs here.
- `omnicore-sdk/src/c/oir/oir_ast_frontend_writer.c`: node-specific scalar/string fields and child references must be emitted using the same field IDs.
- `omnicore-sdk/src/c/oir/oir_ast_frontend_reader.c`: reader must allocate the correct AST payload size, restore owned strings/scalars, and attach every child field with ownership checks.
- Existing AST binary codec in `parser/ast.c` remains a separate compatibility surface; OIR bridge support does not replace its print/destroy/serialize/deserialize integration.

Implementation status (2 Kasım 2026): select/scan, rules/apply/on, lambda, relocate and unroll node families are appended to `ast.h`; all five `ast.c` lifecycle/codec switches and parser productions are updated. OIR IDs 81–96, child index, writer, reader and field schema are registered. Parser AST shape tests and all-kind OIR payload tests are present. **CI passed:** run 41462178481 — Windows, Ubuntu and macOS build/test; Ubuntu ASan/UBSan and Valgrind. Semantic baseline tests also cover pattern scope isolation, ruleset argument/event validation, lambda scopes and relocate type compatibility. Rolling retry `ms` parsing and overflow regression are being validated in the subsequent run.

Canonical select/scan field order (field IDs are OIR schema IDs, not C struct offsets):

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

## Lexer → Parser token gaps (source audit)

The following are tokenized by the C Lexer but have no explicit Parser consumption in `parser.c`. Type-token families and prefixes may be handled by ordinal/range checks, so zero textual occurrences alone is not proof of a missing primitive type.

| Lexed form | Syntax-reference meaning | Current Parser state | Next action |
|---|---|---|---|
| `no:` / `TOKEN_KW_NO` | Dependency/OS-adapter exclusion directive (e.g. `no:CoreModule;`) | No token consumption; no AST node/directive representation | Specify directive scope and artifact representation; then implement parser + semantic/module-resolution test |
| `m:` / `TOKEN_PRE_MUST` | Mandatory dependency | No explicit Parser consumption | Resolve whether this is a declaration/import form or reserved syntax; do not map it to ordinary variable without contract |
| `r:`, `t:`, `o:` | Reserved prefixes | Deliberately reserved in syntax reference | Keep rejected/reserved; no AST node needed until activated |
| `...` / `TOKEN_ELLIPSIS` | Variadic function parameter marker (documented as trailing marker in signatures) | No Parser consumption; `ASTFuncDecl` has no variadic field | Add explicit function signature flag/AST codec + OIR field contract and tests before accepting |
| `asm:`, `fastexec`, `asmcall()`, `asmjmp()` | Inline assembly label/block and direct label call/jump | Lexer tokens exist; Parser does not consume them | Dedicated ASM payload/label representation and target validation required; current `AST_ASM_BLOCK` is only a generic block payload |

**Fail-closed requirement:** these forms must not be reported as implemented because their Lexer tokens exist. Until their AST/semantic contracts are established, parser rejection is preferable to silent or lossy acceptance.
