# NEXSUS Flow Syntax ↔ AST Coverage Matrix

**Branch:** `refactor/c-core-roadmap`  
**Status:** AST source-span field/validator and versioned legacy codec implemented; FASTEXEC/ASM and Struct literal AST nodes, raw Lexer token, Parser productions and OIR node bridges 101–107 are now present on the working branch. Parser span stamping remains under per-production audit; OIR optional span TLVs and FASTEXEC Semantic validation remain open. Existing semantic/lowering gaps remain gated.  
**Gate:** This matrix must be complete before Codegen expansion.

## Comparison rule

A syntax form is complete only if Lexer → Parser → AST payload → AST codec → semantic rules/artifact → lowering has a matching contract and tests. An AST enum tag alone is not completion. “Missing node” below means the current AST model has no dedicated representation; it may not yet prove the parser cannot encode it indirectly.

## Syntax-to-AST comparison

| NEXSUS Flow syntax / construct | Existing AST representation | Initial result | Required action |
|---|---|---|---|
| Program / block `{...}` | `AST_PROGRAM`, `AST_BLOCK` | Present; program root spans full source file, blocks include delimiters and `}?;` | Finish per-node span audit and OIR span TLV |
| Mutable / constant declaration `v:`, `c:` | `AST_VAR_DECL` | Present | Verify type/hard/public/export/inline type fields |
| Function `f:name(...)` | `AST_FUNC_DECL` | Present | Audit parameter nodes, named args, return type and body |
| Extern function `exf:name(...)` | `AST_EXTERN_DECL` / `ASTFuncDecl` | Present/shared payload | Verify parser, codec, semantic signature |
| Pointer/reference types `*T`, `**T`, `&T` | Existing `ASTVarDecl.type_name`, `ASTFuncDecl.return_type`, `ASTNewTypeDecl.target_type` strings | Parser accepts these in local, parameter, return and NewType contexts; AST string preservation/codec regression passed | Dedicated type descriptor decision; Semantic nullability, mutability, borrow/lifetime and ownership rules |
| Lambda `f:(a,b)>expr` / block lambda `f:(a,b){...}` | `AST_LAMBDA` | Parser/AST/OIR implemented; expression/block bodies, parameter scopes, semantic-result scope visibility and source-span tests present | Function-type model, capture rules, return inference and lowering remain open |
| Anonymous function `f:(a,b){...}` | `AST_LAMBDA` shared with expression lambda | Parser/AST/OIR and block-body codec implemented | Function-type representation, captures and return inference remain open |
| Callback/function type (canonical proposal) `f:(id!u32)!bool` | Existing type-name string in NewType/variable/parameter/return fields | Parser accepts and normalizes the signature; AST validator/codec regression passed | Dedicated `AST_FUNCTION_TYPE` node decision, callback compatibility, closure conversion and function-pointer ABI remain open |
| Implicit generic type variable `a!t`, return `!t` | Existing type-name strings | Parser preserves spelling; `test_implicit_type_variable_spelling` covers parameters, return, validator and codec | Semantic type-variable binding, scope, unification and inference; explicit `<T>` list is not normative |
| Struct / packed struct | `AST_STRUCT_DECL` | Present | Confirm packed syntax and field type representation |
| Struct literal `Type { field = expr, ... }` | `AST_STRUCT_LITERAL` (OIR 106) owns nominal type name and ordered `AST_STRUCT_FIELD_INIT` (OIR 107) nodes | Parser, AST lifecycle/graph validator/legacy codec, OIR map/schema/index/writer/reader and byte-stable round-trip are implemented; Semantic validation covers type visibility, unknown/duplicate/missing fields, value compatibility, Group factory return, nested Group isolation and source-order preservation | OIR instance initialization/layout lowering and independent Verifier rules remain open; field resolution uses SemanticResult `AGGREGATE_FIELD` relations |
| Enum + variants/data | `AST_ENUM_DECL`, `AST_ENUM_VARIANT` | Present | Validate payload variants and discriminants |
| Union | `AST_UNION_DECL` / `ASTStructDecl` | Present/shared | Confirm union field layout/type rules |
| NewType `nt:` | `AST_NEWTYPE_DECL` | Present | Confirm target type and nominal semantics |
| Group / methods | `AST_GROUP_DECL`, `AST_METHOD` | Present | Confirm member dispatch and method receiver |
| Module import `use` | `AST_USE` | Parser supports comma-separated module names, aliases and dotted paths such as `use msvcrt.exf as crt;`; dotted path AST/codec test added | Module resolution and malformed path diagnostics remain |
| Literals (number/string/char/bool/null) | `AST_LITERAL` | Parser/AST/codec supports decimal, `0x`/`0b`/`0o`, scientific exponent, digit separators and `null` | Numeric conversion/overflow and null-pointer compatibility remain Semantic work |
| Identifier | `AST_IDENTIFIER` | Present | Symbol resolution + use-site metadata |
| Unary / postfix | `AST_UNARY_EXPR`, `AST_POSTFIX_EXPR` | Present | Operator coverage and type rules |
| Binary / assignment | `AST_BINARY_EXPR`, `AST_ASSIGNMENT` | Present | Full operator/precedence/associativity matrix |
| Exponentiation `^^` | `parse_power()` builds `AST_BINARY_EXPR(TOKEN_POWER)`; right-associative; tighter than unary minus | Parser production + basic parser fixture added | Semantic matching numeric operand validation is present/tested; promotion/overflow and OIR opcode/lowering remain open |
| Call / member / index | `AST_FUNC_CALL`, `AST_MEMBER_ACCESS`, `AST_INDEX_ACCESS` | Present | Call target, named args, lvalue and bounds contracts |
| Cast `cast(value,type)` | `AST_CAST` | Present | Type resolution and conversion record |
| Tuple | `AST_TUPLE` | Present | Tuple typing/destructuring contract |
| Array literal `[a, b, ...]` (including nested/empty arrays) | `AST_ARRAY_LITERAL` with ordered element chain | Parser/AST codec and OIR kind 97 implemented; nested/empty and malformed-delimiter tests added; homogeneous literal element checks and generic aggregate inference implemented | Map Semantic constraints and vector typing remain open |
| Array indexing `array[index]` and indexed assignment `array[index] = value` | `AST_INDEX_ACCESS` with separate target/index children; assignment keeps index node as its lvalue | Parser, AST codec, semantic array-target and integer-index checks; parser/codec and semantic valid/invalid fixtures added | Bounds checking, multidimensional flattening and load/store OIR lowering remain open |
| Array type declarators `T[]`, `T[n]`, `T[m][n]` and function parameter arrays | `AST_ARRAY_TYPE` (OIR 98) with ordered `AST_ARRAY_DIMENSION` nodes (OIR 99); attached through the existing `ASTVarDecl.inline_type` child while the base element type remains in `type_name` | Parser, AST binary codec, OIR frontend writer/reader and semantic aggregate type records added; dynamic/fixed/multidimensional and parameter forms have dedicated tests | Fixed extents accept positive integer literals and compile-time integer arithmetic (`+`, `-`, `*`, `/`, `%`) with overflow/divide-by-zero rejection; modulo requires integer operands; function-call arguments validate element type, rank, fixed extents and dynamic `[]` extents; executable lowering remains open |
| Unified `loop` (range/count/foreach/conditional/infinite) + `unroll` | `AST_LOOP`, `AST_UNROLL`; rolling retry is represented by `AST_NEXUS_FLOW.retry_count/retry_delay` | Parser/AST/OIR forms implemented; `loop {}` and legacy `(loop) {}` infinite forms, foreach `in`, `unroll(count)->loop`, and `?(count, delay ms)->target` are parsed | Runtime retry semantics, constant-bound validation and canonical loop normalization/lowering remain open |
| `if / elsif / els` | `AST_IF` condition/then/else | Partial | Verify elsif chain shape and nesting semantics |
| Conditional action `(cond) -> break/continue/...` | `AST_IF` canonical form | Parser path exists and is disambiguated from Flow pipe | Complete semantic loop-context and round-trip coverage; no separate node required unless source-form preservation becomes a language requirement |
| Labels `label:>`; jumps `?>(label);` and `jump(label);` | `AST_NEXUS_LABEL`, `AST_NEXUS_JUMP` | Parser supports explicit operator form and contextual call alias; new AST/codec regression added | Resolve label IDs and validate scope/control-flow |
| `select` | `AST_SELECT` + ordered `AST_SELECT_ARM`; literal, binding and constructor patterns; `when` guard is canonicalized as an `AST_IF` body wrapper | Parser/AST/OIR and semantic arm-local binding, guard bool-check and duplicate-binding checks implemented | Closed-enum constructor/type resolution, exhaustiveness and lowering remain gated |
| `scan` | `AST_SCAN` + ordered `AST_SCAN_ARM`; binding and constructor pattern nodes; `when` guard is canonicalized as an `AST_IF` body wrapper | Parser/AST/OIR and semantic arm-local binding, guard bool-check and duplicate-binding checks implemented | Subject-aware pattern binding type inference and scan execution/lowering remain gated; final `else` catch-all is implemented as a null pattern |
| Multi-rule engine / ruleset | `AST_RULESET_DECL`, `AST_RULE_BARRIER`, `AST_RULE_TRIGGER`, `AST_RULE_ALWAYS`, `AST_RULE_DEFAULT` | Parser/AST/OIR implemented; semantic ruleset symbols/signatures, boolean conditions, string barriers and trigger event registration implemented | Rule ordering/termination/effect execution and lowering remain open |
| Rule application | `AST_APPLY_RULES` | Parser/AST/OIR implemented; semantic ruleset lookup and argument count/type checks implemented | Validate active apply context and model injected effects in semantic AST/lowering |
| `on` blocks | `AST_ON_BLOCK` | Parser/AST/OIR implemented; semantic ruleset and trigger-event name validation implemented | Validate active apply context and lower trigger dispatch |
| Handler capture `(h)<-expr` | `AST_NEXUS_CAPTURE` | Present | Ownership, occupancy, move semantics annotations |
| Flow / pipe `->, >>, <<` | `AST_NEXUS_FLOW`, `AST_NEXUS_PIPE` | Present/shared | Separate operator semantics, preserve retry/catch payload |
| Relocate `_>` | `AST_RELOCATE` | Lexer/parser/AST/OIR implemented; semantic source/destination type compatibility check implemented | Source invalidation, destination ownership initialization and explicit lowering effect remain open |
| Unpack `...>` | `AST_UNPACK` | Present | Verify target/value and ownership behavior |
| Halt `(resource)?;` / block `}?;` | `AST_NEXUS_HALT` for targeted/targetless statement Halt; `AST_BLOCK_HALT` (OIR 100) for block-scope cleanup | Parser and AST graph contract implemented; OIR node mapping/schema/writer implemented; end-to-end OIR regression pending CI | `}?;` is appended as the final block child. Validator permits it only as the final child of `AST_BLOCK`/`AST_ASM_BLOCK`/`AST_FASTEXEC_BLOCK`; root-level and non-final markers are rejected. Semantic cleanup ordering and lowering remain later-stage work. |
| Intent `@` | `AST_NEXUS_INTENT` | Declared / deferred | Do not invent behavior until Intent design is active |
| AI logic `??` | `AST_AI_LOGIC` | Declared / deferred | Define payload and semantics before implementation |
| Panic `panic("...")` | `AST_PANIC` | Present | Current documented literal-only message restriction must be tested |
| Listen/send/receive/done | Matching AST tags | Parser/AST payloads present | Thread/event semantics, payload and effects remain open |
| Spawn | Legacy `AST_SPAWN` tag only | Intentionally not emitted by core Parser; `spawn(...)` rejected | Separate Thread module owns spawn/start/join grammar and runtime semantics |
| Area/zone/address/len/peek/poke/inb/outb/irq/intr/reg | Matching AST tags | Present/shared call payload | Signature, type, effect and target constraints |
| I/O/reflection/time/free/zone | Matching AST tags | I/O/reflection/time/free intrinsic calls are parsed; `zone.sync/view/lock` member forms are generic member calls, not dedicated AST_ZONE_* nodes | Complete intrinsic registry and semantic signatures; reserved AST_ZONE_SYNC/VIEW/LOCK tags must not be counted as active productions |
| Defer | `AST_DEFER` | Present | Scope exit ordering and cleanup semantics |
| Thread block `(name)>{...}` | `AST_THREAD_BLOCK` | Parser stores name/body; `mode` and `timeout` remain NULL/reserved | Scheduling, timeout syntax, start/join, capture/lifetime, cancellation and synchronization semantics |
| Inline ASM (`fastexec`, `asm: LABEL { ... }`) | `AST_FASTEXEC_BLOCK` (OIR 104) owns statements; `AST_ASM_LABEL` (OIR 105) owns label and exact raw body; legacy `AST_ASM_BLOCK` kind 29 unchanged | Lexer emits opaque `TOKEN_ASM_BODY` (OIR token 164); Parser constructs FASTEXEC/ASM nodes, preserves source spans and rejects ASM declarations outside FASTEXEC | C nodes, lifecycle, AST codec, OIR 101–105 bridge and parser/lexer tests added; workflow `42411942789` passed all 5 jobs. Semantic must still validate `asmcall`/`asmjmp`, primitives, placeholders, register/ABI and target capability constraints |
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
| Range `..` | `parse_range()` creates one `AST_BINARY_EXPR(TOKEN_RANGE)`; positive `1..10` and negative chained `1..2..3` fixtures pass CI run `42039331167` (all five jobs) | Parser rejects chained range operators, including parenthesized nested range operands; range endpoints are inclusive (`1..10` iterates 1 through 10, 10 values). Endpoint type validation and runtime iteration implementation remain Semantic/runtime work |

This table is an implementation audit, not a change to the language design. Unsupported forms must not be marked complete merely because the Lexer emits tokens. `&&` and `||` are now consumed by the C Parser (`parse_logical_and()` / `parse_logical_or()`) with AND precedence above OR and left associativity; Parser/Semantic fixtures cover AST shape and bool operand checks. Runtime short-circuit behavior remains an OIR/lowering requirement and is not implied by parsing.

## Confirmed model-level gaps to address

1. **Array type declarators:** `AST_ARRAY_TYPE` and `AST_ARRAY_DIMENSION` represent dynamic, fixed and multidimensional shapes, including function parameters. Function calls validate element type, rank and fixed/dynamic extents; remaining work is executable lowering.
2. **Map typing:** ordinary `map` type; prefer function parameter spelling `a:map`, retain `a!map` as hard-type compatibility; local declarations currently use `v:name!map`. Generic `map<key,value>` is not implemented and remains an open optional-constraint design question.
3. **Vector typing:** `Vec2/Vec3/Vec4` currently rely on array-shaped literals; dimensionality and SIMD type rules must be checked semantically.
4. **Loop normalization:** legacy `loop_type = Infinite/While/For/Foreach` remains the frontend payload; canonical lowering and runtime semantics are still open.
5. **Conditional action:** parser canonicalizes it to `AST_IF`; loop-context semantic checks and source-form round-trip remain open.
6. **Ownership/effects:** relocate invalidation, handler capture, unpack, defer and halt cleanup need complete semantic effect records and lowering.
7. **Intent/AI logic:** declared/deferred; do not invent semantics before their specs are activated.
8. **Pattern binding type inference:** `semantic_check_pattern_bindings()` currently creates `any` slots without the select/scan subject type. Enum declaration analysis already records payload field types in Semantic Result relations, but the arm checker does not resolve a constructor pattern against that subject enum/variant. This is the reason a guard such as `when a > 0` cannot yet rely on the payload field's numeric type; subject-aware constructor resolution must close before those guards are claimed semantically complete.

## AST structural audit findings (updated 2026-11-04)

- **All 107 current ASTNodeType values have explicit Graph Validator dispatch.** The validator's default branch now fails closed with AST_VALIDATE_INVALID_KIND; a future appended node cannot silently pass as a childless leaf. The populated codec round-trip fixtures also pass through `ast_validate()` because serialization validates before writing.
- **Source spans are implemented but not complete end-to-end.** `ASTNode` now stores `OccSourceSpan`; the graph validator checks byte-offset and line/column monotonicity plus `line/col` mirrors. Parser stamping covers program root, blocks/Halt, literals/identifiers, unary/binary/assignment/postfix/call/index/tuple/array/array dimensions, declarations/parameters, if/loop/select/scan, Nexus Flow capture/jump and several aggregate/intrinsic forms. Exact tests cover UTF-8 byte columns and multiline endpoints. The AST binary codec writes `OCCAST` version 1 with fieldwise spans and reads legacy unprefixed streams (including non-seekable streams via replay buffer). **Remaining:** finish per-production parser audit, then implement OIR optional span TLVs (`0xFFF0`–`0xFFF3`) and round-trip/backward-compatibility tests. Do not mark the full source-span column complete yet.
- **Block-scoped Halt representation added.** `AST_BLOCK_HALT` is a payload-free final child of its owning block; parser and graph validator enforce the placement contract. OIR uses append-only kind ID 100. Semantic cleanup ordering and lowering are not part of this AST change.
- **FASTEXEC/Inline ASM AST and Parser path is implemented but not complete as a pipeline feature.** `AST_FASTEXEC_BLOCK` and `AST_ASM_LABEL` have owned payloads, graph validation, AST stream codec and OIR 104–105 mapping; `TOKEN_ASM_BODY` preserves raw text and Parser builds the nodes. `AST_ASM_BLOCK` remains legacy kind 29. Semantic scope checks for asmcall/asmjmp and hardware primitives, placeholder/register/clobber analysis, target capability and ABI checks remain open.
- **Normative syntax document path reconciled.** The active branch now uses `docs/language/syntax-reference.md` as the single normative reference. The duplicate `omnicore-sdk/docs/syntax.md` was removed. `main` was read-only and remains untouched.

## AST node payload/codec audit (all 107 current enum tags)

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

1. Finish parser grammar and source-span trace for all syntax rows above (including README/spec examples).
2. Add the missing AST node contracts in a stable, append-only manner; update OIR AST node registry IDs without renumbering existing IDs.
3. Implement parser construction, allocation, printer, codec, destroy, source-span and round-trip tests for each added node.
4. Complete semantic typing/scope/effect contracts and semantic artifact validation.
5. Add lowering coverage and verifier contracts only after frontend rows are complete.
6. Keep Codegen expansion blocked until the matrix is green.

## Source anchors

- `README.md`: control flow (Section 5), functions (Section 6), data structures (Section 7), Nexus Flow operators (Section 3), intrinsic/core operations (Section 14).
- `omnicore-sdk/src/c/parser/ast.h`: current `ASTNodeType` and payload definitions.
- `docs/language/syntax-reference.md`: canonical NEXUS Flow syntax and language-semantics reference.
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
| `asm:`, `fastexec`, `asmcall()`, `asmjmp()` | `AST_FASTEXEC_BLOCK`, `AST_ASM_LABEL`; `asmcall`/`asmjmp` remain `AST_FUNC_CALL` with intrinsic callee names | Lexer/Parser now consume FASTEXEC and raw ASM declaration syntax; stable token ID 164 and OIR AST IDs 104–105 | Parser scope diagnostic exists for ASM declarations; Semantic label resolution and scope checks for call/jump remain open |

**Fail-closed requirement:** these forms must not be reported as implemented because their Lexer tokens exist. Until their AST/semantic contracts are established, parser rejection is preferable to silent or lossy acceptance.


## Normative syntax forms: AST representation audit (2026-11-04)

This section separates **missing AST representation** from syntax that can legitimately reuse an existing node and from preprocessing/runtime constructs that do not belong in executable AST.

| Syntax in `docs/language/syntax-reference.md` | AST representation decision | Audit status | Required work |
|---|---|---|---|
| `m:name = "module"` (MUST dependency) | `AST_MUST_DECL` with owned dependency name and source expression | **AST/Parser implemented; OIR/Semantic pending** | Preserve program-scope-only rule; add OIR ID/schema/bridge and Semantic dependency resolution/load-failure diagnostics |
| `r:`, `t:`, `o:` reserved prefixes | No executable grammar contract | **Reserved, not missing executable AST** | Keep rejected/reserved; do not add nodes until language spec activates them |
| Variadic parameter `...` | Final zero-payload `AST_VARIADIC_PARAM` in function parameter list | **AST/Parser implemented; OIR/Semantic pending** | OIR ID/schema/bridge; Semantic variadic packing and signature checks |
| Named arguments `name:value` | `AST_NAMED_ARG` with owned label and value expression; positional expressions remain unchanged | **AST/Parser implemented; OIR/Semantic pending** | OIR ID/schema/bridge; Semantic parameter-name resolution and argument matching |
| Generic type-variable spelling `a!t`, return `!t` | `ASTVarDecl.type_name` and `ASTFuncDecl.return_type` preserve the exact type spelling, including `!t` | **AST string representation present; Semantic binding/unification pending** | Add Semantic type-variable binding/unification and inference rules; no new AST tag until the syntax defines explicit constraints or generic parameter declarations |
| Function pointer/callback type `nt:Callback = f:(id!u32)!bool` | **Syntax example exists, but its normative grammar is incomplete**: parameter separators, names vs types, variadic marker, return-type delimiter, nested function types and callable-value usage are not specified as a complete type grammar; `ASTNewTypeDecl.target_type` is scalar text and cannot retain a signature tree | **Missing AST type representation; parser contract blocked by syntax clarification** | First define the complete function-type grammar and AST payload (ordered typed parameters + return type + variadic flag); then append an AST kind, codec/validator/printer support, parser positive/negative fixtures and OIR schema mapping. Do not treat `AST_LAMBDA` as a function type. |
| Macro definition `:macro! Name(...){...}` and invocation `!!=Name!(...)` | No macro-definition/invocation AST node | **Missing macro/preprocessor model** | Define macro expansion phase and source-origin mapping first; macro bodies must preserve syntax/tree or token stream without pretending to be runtime nodes |
| Target/compiler directive `!!=[target=...]{...}` | No directive/conditional-compilation AST node | **Missing AST representation** | Dedicated directive node with condition/target expression and body; parser must preserve nested branches and directive assignment `!!=` separately |
| Link directive `!!=link name = "...";` | No explicit link-directive node | **Missing AST representation** | Model as compile-time/link metadata, not ordinary runtime assignment; codec and semantic target validation |
| `dllexp: entry(...){...}` | No DLL-export declaration node/flag in function payload | **Missing AST declaration contract** | Append export-entry metadata/node; parser, codec, symbol validation and module-output integration |
| `import("path")` / DLL, object and library import | Can parse as generic `AST_FUNC_CALL` only if call grammar accepts it; resource acquisition is not distinguished | **Representation ambiguous / semantic metadata missing** | Preserve import as explicit intrinsic/resource operation or typed call annotation; distinguish dynamic-library import from ordinary function call; verify parser fixture and Halt ownership |
| `asmcall(LABEL)`, `asmjmp("LABEL")` | Existing `AST_FUNC_CALL` plus intrinsic callee name and argument expression | Parser already preserves the call shape and exact source span; Semantic identifies intrinsic names | FASTEXEC-only scope, label resolution, argument shape and control-flow validation remain Semantic work |
| ASM substitutions `%var`, `%var:reg`, clobber list | Preserved byte-for-byte inside `AST_ASM_LABEL.source_text`; no speculative AST operand node | Lexer/Parser preserve exact raw body and source span | Semantic/target analysis must extract and validate variable bindings, direction, register hints, clobbers and ABI effects |
|| Map | `ASTVarDecl.type_name = "map"` for ordinary `a:map`; `"!map"` for explicit hard-type parameter compatibility | No dedicated `AST_MAP_TYPE` node is needed for the current unparameterized form; generic constraints remain an open design question |
| `Vec2/Vec3/Vec4` | Type name can be stored as string; literal is `AST_ARRAY_LITERAL` | **AST syntax can be represented, but dimension/type metadata is missing** | Semantic vector shape contract; decide whether AST needs dedicated vector literal/type node or typed array-literal annotation |
| Conditional target `!!=[target=...]` / target variants | Generic block/assignment cannot safely retain target condition semantics | **Missing directive node** | Same directive AST work as above; no heuristic parser |
| `elsif` and conditional action | Nested `AST_IF` and canonical `AST_IF` | **Existing representation** | Explicit parser/codec shape tests; no new node required |
| `select/scan when`, scan `else` | Arm body wrapper `AST_IF`; null pattern only final scan else | **Existing representation** | Subject-aware constructor typing remains Semantic gap |
| `T[]`, `T[n]`, `T[m][n]` | `AST_ARRAY_TYPE` + ordered `AST_ARRAY_DIMENSION` | **Existing representation** | Keep current parser/codec; lowering later |
| `<<` / `>>` bit shifts vs Flow operations | Current parser maps these to Flow pipe node only | **Missing numeric shift AST production** | Resolve grammar by operand productions/context, then produce `AST_BINARY_EXPR` for numeric shift and `AST_NEXUS_PIPE` for Flow; no token-only heuristic |

### Audit conclusion

Confirmed AST representation gaps (not just parser gaps): **dedicated function-type tree, macro phase nodes, conditional target/link directives, DLL export/import metadata, map semantic constraints, and numeric bit-shift expression**. Callback signature syntax itself is now parsed and preserved as a normalized type-name string. ASM calls/jumps intentionally remain intrinsic `AST_FUNC_CALL` nodes; operand/clobber data is preserved in the raw ASM body for Semantic/target analysis. Generic placeholder spelling `t` is already preserved losslessly in function parameter/return type strings; its binding and unification are Semantic work, not a reason to add a redundant AST node. MUST dependency, variadic marker and named argument now have AST/Parser representations; their OIR and Semantic stages remain open.

Some syntax can reuse existing AST shapes without a new enum tag (`elsif`, conditional action, array declarators, select/scan guard); these remain subject to lossless round-trip and semantic contract tests. `import(path)` is provisionally representable as a call expression but must carry an explicit import/resource kind to preserve its runtime ownership and DLL semantics.

**Ordering:** complete remaining AST payload contracts (dedicated function-type representation decision; directives/macro/import/export; map Semantic contract; vector typing; numeric-shift/Flow grammar resolution), finish the FASTEXEC Lexer/Parser edge-case audit and OIR source-span TLVs, then Semantic. Codegen stays blocked.


## AST contract — variadic and named arguments (implemented; OIR/Semantic pending, 2026-11-04)

Normative examples in `docs/language/syntax-reference.md` establish:

- Variadic declaration marker: `...` appears after fixed parameters, e.g. `f:liste(a!i32[], ...)`. The ellipsis is a parameter-list property, not a value expression. It is valid only once and in final position; a trailing comma or parameter after it is invalid.
- Named call argument: `callee(a:10, b:20)`. The name belongs to the argument slot, not to the value expression. It must survive Parser → AST codec → OIR AST bridge losslessly.
- Existing parser currently treats every call argument as `parse_expression()`; colon therefore is not represented as an argument label. Function declaration currently requires every parameter to begin with an identifier and has no ellipsis branch.
- Do not encode named arguments as assignments (`AST_ASSIGNMENT`), because that changes expression semantics and loses argument-slot identity.
- Do not encode variadic ellipsis as a fake `AST_VAR_DECL`; it has no identifier/type/value and is a list-level marker.

**Chosen append-only representation (implementation pending):**
1. Append `AST_VARIADIC_PARAM` as a zero-payload marker, legal only as the final child of `AST_FUNC_DECL.params` / `AST_EXTERN_DECL.params`. It represents the declaration's `...`; existing parameter nodes remain unchanged.
2. Append `AST_NAMED_ARG` with owned `char *name` and owned `ASTNode *value`; each named argument is a single list element. Positional arguments remain their current expression nodes, preserving old AST shape and compatibility.
3. Reject duplicate named labels in one call and reject positional arguments after the first named argument (deterministic rule; no positional-after-named ambiguity). Mixed positional-before-named remains valid.
4. AST graph validator checks marker context/finality, named argument non-empty name + required value, and duplicate named labels in a call. Codec and OIR node IDs are append-only; existing IDs and field IDs are not changed.
5. Tests required: declaration marker finality/duplicates; named+positional call parse; duplicate labels; positional-after-named; malformed `name:`; legacy codec compatibility; AST codec round-trip; OIR bridge round-trip and byte-stability; graph validator malformed graphs.

These are syntax-to-AST decisions only. Type/name resolution, overload matching, variadic packing and runtime ABI are Semantic/lowering work and remain gated.


## Process entry arguments: `main(argc, argv)` (2026-11-04)

The normative function section already declares `f:main(argc, argv)!i32 { ... }`; this is not a new syntax. Its OS/CLI delivery contract is currently missing and must not be confused with ordinary user function arguments.

**Contract:**
- `argc`: integer count of command-line argument strings delivered to the process, excluding the executable path (Nexus convention; runtime adapter normalizes platform conventions).
- `argv`: one-dimensional dynamic array/list of `str`, containing those arguments in order, also excluding executable path. Empty invocation means count 0 and empty list; no implicit NULL sentinel is exposed.
- Invariant: `argc == len(argv)`. Runtime constructs both values before entering `main`; program cannot mutate the OS-owned original vector (the language values may be copied/owned by the runtime).
- `main` signature is an entry-point special case: only `main(argc, argv)` (or the documented no-parameter form if later accepted) receives process arguments. Ordinary functions do not receive implicit CLI data.
- Runtime/launcher responsibility: Windows command-line parsing follows Windows process argument rules; POSIX targets use the OS argv vector. Runtime must provide a consistent decoded string sequence and define encoding (UTF-8 at the language boundary). Bare-metal targets may inject a boot/launcher argument vector or provide empty args.
- AST remains ordinary `AST_FUNC_DECL` + `AST_VAR_DECL` parameter nodes. No `AST_ARGC`/`AST_ARGV` nodes and no hard-coded names in AST. Semantic/entry-point validation will recognize the main signature; runtime/target ABI will materialize values.
- Implementation stages: syntax/AST parsing is existing; semantic signature validation, runtime ABI and OS adapters, `len(argv)` compatibility, and tests are pending. Add tests for zero/one/multiple args, spaces/quotes, Unicode, argc-length invariant, Windows/POSIX normalization, and bare-metal empty/injected args.

This defines CLI argument *delivery*, not the full `main` return/exit-code convention; the latter remains an explicit runtime contract item.


**Implementation update (2026-11-04):** `AST_VARIADIC_PARAM` and `AST_NAMED_ARG` have been appended after the existing AST kind inventory (IDs 101 and 102; prior IDs preserved). AST factory, printer, destructor, serializer/deserializer bounds and named-argument payload codec are wired. Parser now accepts final `...` in function/external parameter lists and `label:value` for intrinsic and ordinary calls; it rejects positional-after-named and duplicate labels. Graph validation checks variadic marker context/finality and named argument context, required fields, order and duplicate labels. Parser round-trip and validator tests have been added. **Validation:** CI run `41759489746` passed on Ubuntu, Windows and macOS; ASan/UBSan and Valgrind also passed. **Not complete:** OIR stable IDs/schema/bridge support and semantic variadic packing/name resolution remain pending.

## AST contract — MUST dependency declaration (2026-11-04)

The syntax reference defines `m:CoreModule = "CoreModule.nxf";` as a mandatory compile-time dependency, distinct from `use`. Lexer already emits `TOKEN_PRE_MUST`; the AST/Parser gap is now closed with appended `AST_MUST_DECL` (AST kind 103), carrying an owned dependency name and an owned source expression. Parser accepts the combined `m:Name` prefix token (and a separated name if lexed separately), requires `=` and a source expression, and rejects declarations outside program scope. AST printer/destructor/codec and graph validator are wired. Tests cover parse + codec round-trip, factory/codec tag boundaries, valid program scope and invalid nested scope. CI run `41760397845` passed on Ubuntu, Windows and macOS, plus ASan/UBSan and Valgrind. OIR registry/schema/bridge support and Semantic dependency resolution, load-failure behavior and diagnostic integration remain pending.


## Parser/AST symbol audit (2026-11-04)

The latest C AST inventory contains 107 node tags. FASTEXEC/Inline ASM now has a dedicated `AST_FASTEXEC_BLOCK` and `AST_ASM_LABEL` payload, explicit lifecycle/graph validation, legacy AST codec support, a dedicated `TOKEN_ASM_BODY` Lexer token (stable OIR token ID 164), Parser productions, and OIR AST bridge IDs 104–105. Existing `AST_ASM_BLOCK` kind 29 remains untouched. `asmcall` and `asmjmp` intentionally remain `AST_FUNC_CALL` nodes with intrinsic callee names.

- `AST_ZONE_SYNC`, `AST_ZONE_VIEW` and `AST_ZONE_LOCK` remain reserved AST kinds without matching productions in the current normative syntax reference; they are not treated as parser defects until a language rule defines them.
- The OIR frontend bridge includes `AST_VARIADIC_PARAM`, `AST_NAMED_ARG`, `AST_MUST_DECL` (101–103), FASTEXEC/ASM (104–105), and Struct literal nodes (106–107).
- The raw ASM body is preserved exactly; Parser does not parse target instructions as Nexus Flow. Lexer balances nested braces and ignores brace characters inside quoted strings/chars with backslash escapes. Target comment syntax is opaque, so unmatched braces in comments still participate in delimiter balancing.

**AST remains open:** finish the full Parser source-span audit and OIR optional source-span TLVs; complete remaining AST gaps (function types/callbacks, macro/directive metadata, map Semantic contract, vector typing, numeric shift/Flow grammar resolution). Semantic FASTEXEC checks for calls/primitives, labels, variable placeholders, register/clobber, ABI and control flow remain blocked until AST/Parser validation passes. Codegen remains blocked.


## AST representation audit cross-reference — 2026-11-05

The detailed representation decisions and unresolved type contracts are maintained in [AST_REPRESENTATION_GAPS.md](docs/AST_REPRESENTATION_GAPS.md). This matrix remains the syntax-to-node coverage inventory; the gap document deliberately does not add node kinds or imply Parser/Codec implementation.

| Area | Current AST representation | Gate |
|---|---|---|
| Function pointer / callback | `fptr` is currently an opaque type name; lambda has `AST_LAMBDA` | Signature-bearing function type model required |
| Map | Ordinary named type in `ASTVarDecl.type_name`; no key/value payload | Current syntax is represented; per-entry key/value constraints and runtime behavior belong to Semantic/runtime design. Optional generic constraint syntax remains undecided |
| Pointer/ref | Type spelling remains opaque in declaration type fields | Unified type constructors and ownership contract required |
| Function array return `f:name(...)!T[]` / `!T[n]` / `!T[m][n]` | `ASTFuncDecl.return_type` retains base type; optional owned `return_array_type: AST_ARRAY_TYPE` carries ordered dimensions | Parser, AST stream v2/v1-read compatibility, graph validation, OIR frontend field 8 and canonical OIR round-trip are implemented and CI-verified (run `42010711975`). Semantic return-type compatibility and lowering remain open. |
| Compile directives / macros | Must not be treated as executable statement nodes | Compile-unit/preprocessing representation separate from runtime AST |
| Numeric `<< / >>` | Existing parser emits Flow `AST_NEXUS_PIPE` | Numeric shift AST production waits deterministic grammar resolution |

No AST closure claim is made by this cross-reference.


## Syntax reconciliation — 2026-11-07

The normative reference was compared against the active Lexer/Parser source rather than token declarations alone.

| Syntax/document claim | Active source finding | Resolution |
|---|---|---|
| Infinite loop `loop { ... }` | Parser previously required `loop(...)` or `(loop)` | Implemented `loop { ... }`; legacy `(loop) { ... }` retained |
| `jump(label)` | Only `?>(label);` was parsed as a jump statement | Implemented contextual `jump(label);` alias to the same `AST_NEXUS_JUMP` |
| `label?>` / `out?>` | This postfix spelling was not a Parser production | Normative examples now use `?>(label);` or `jump(label);` |
| `return` examples | Lexer keyword is `ret`; `return` is not a keyword | Normative code examples changed to `ret` |
| Scientific literal `10E3` and hex literals `0x...` | Current Lexer rejects radix/exponent forms | Valid code examples now use supported decimal literals; numeric grammar remains an explicit open item |
| `d32` / `d64` | Not built-in Lexer type tokens | Removed from the built-in type list; not treated as built-ins |
| Generic `t` and callback `f:(...)>!R` type examples | No generic parameter declaration or function-type AST production exists | Marked as unsupported design gaps, not accepted syntax |
| Pointer/reference `*T`, `**T`, `&T` | Parser accepts them in variable, parameter, return and NewType type contexts; AST preserves spelling in existing type-name strings; dedicated regression added | Parser syntax/AST string preservation covered; Semantic ownership, mutability, nullability and lifetime rules remain open |
| `!!=[target...]`, `:macro`, `dllexp:` | These are not complete productions in the core C Parser | Marked as target/preprocessor/build-layer syntax, not core Parser support |
| `null` | Used in prose as a runtime state, but no dedicated null literal token exists | Clarified as not a current source literal |

The language comparison and prioritized gap inventory is maintained in `docs/language/NEXUS-FLOW-LANGUAGE-COMPARISON.md`. These entries are audit findings; they do not imply that deferred syntax has been implemented.


### Module path syntax audit — 2026-11-07

- `parse_use()` previously consumed only one identifier, while the normative DLL example used `use msvcrt.exf;`.
- Parser now consumes one or more identifier segments separated by dots and stores the joined path in `AST_USE.module_name`.
- Alias parsing remains `as identifier`; comma-separated imports remain supported.
- Regression: `test_dotted_module_use_path` checks `msvcrt.exf`, alias `crt`, AST validation and stream round-trip. CI validated by workflow run `42411942789` (all 5 jobs passed).


### Pointer/reference type audit — 2026-11-07

- Parser now accepts `*T`, `**T` and `&T` in local declarations, function parameters, return types and NewType targets.
- Existing type-name string fields preserve the complete spelling; no new AST node kind or OIR ID was introduced.
- `test_pointer_reference_type_declarators` checks all four contexts and AST codec round-trip. CI validated by workflow run `42411942789` (all 5 jobs passed).
- Syntax support does not imply pointer safety, borrow checking, mutability, nullability or lifetime semantics.


### Function type / callback syntax audit — 2026-11-07

- Parser accepts `f:(name!Type, ...)>!ReturnType` in NewType target, variable, parameter and return-type contexts.
- AST preserves the normalized complete signature in the existing owned type-name string field. Lambda expressions remain separate `AST_LAMBDA` nodes.
- `test_function_type_alias_and_callback_parameter` covers valid aliases/parameters, invocation syntax, AST validation, codec round-trip and malformed signature rejection. CI validated by workflow run `42411942789` (all 5 jobs passed).
- A dedicated function-type AST node, callback compatibility/type checking, closure capture and function-pointer ABI remain open.


### AST tag-to-Parser audit — 2026-11-07

- Active `ASTNodeType` enum count: 105 (append-only IDs).
- Intentionally not emitted by the core Parser: `AST_SPAWN` (legacy; Thread module owns spawn), `AST_ASM_BLOCK` (legacy; current syntax uses FASTEXEC/ASM_LABEL), and `AST_ZONE_SYNC/VIEW/LOCK` (reserved; member-call syntax currently produces generic call nodes).
- These are reserved/legacy tags, not missing Parser productions to be filled blindly. Keep their IDs and serialization compatibility.


### Numeric/null literal audit — 2026-11-07

- Supported number forms: decimal integer/fraction, hexadecimal `0x`, binary `0b`, octal `0o`, scientific exponent `e/E`, and underscore separators between digits.
- Numeric suffixes such as `u8` and `f32` remain invalid. The rolling-delay `ms` suffix remains a separate identifier token.
- `null` is now a dedicated `TOKEN_NULL` and `AST_LITERAL`; token ID was appended, not inserted, to preserve existing IDs.
- `test_lexer` covers accepted and malformed numeric forms; `test_null_and_numeric_literal_ast` covers null/radix/scientific AST preservation and codec round-trip. CI validated by workflow run `42411942789` (all 5 jobs passed).
- Numeric conversion/overflow, null pointer compatibility and Option/None semantics remain Semantic design work.


## 2026-11-08 — Callback signature syntax correction

- Signature-only spelling is `f:(params)!ReturnType`; `>` is reserved for the callable/lambda body expression.
- Example: `f:(id!u32)!bool > (id == 1) ? true : false;`.
- Existing Parser's older `f:(params)>!ReturnType` acceptance is legacy/compatibility, not the canonical form.
- Parser migration and type-expression AST are pending.
## 2026-11-08 — Struct literal AST / Parser / OIR bridge

- Appended `AST_STRUCT_LITERAL` and `AST_STRUCT_FIELD_INIT` without renumbering existing AST tags; AST factory, printer, destructor, legacy serializer/deserializer and graph validator now cover both payloads. Empty initializer lists remain structurally valid; Semantic decides whether all declared fields are initialized.
- Parser accepts `Type { field = expression, ... }`, preserves source initializer order and stamps each field-initializer source span. Parser, AST graph, legacy codec and OIR frontend round-trip fixtures cover valid shape, ownership and byte-stable re-encoding.
- OIR IDs 106 and 107 are append-only. Field schemas, map, child traversal, writer and reader were extended. SemanticResult field resolution reuses `AGGREGATE_FIELD` relations (owner Struct symbol, field declaration node ID, name, ordinal and type ID); it does not invent field Symbol IDs.
- First Semantic validation is implemented for visible Struct type, non-Struct rejection, unknown/duplicate/missing fields, initializer type compatibility and Group-owned nominal type visibility. OIR instance initialization, independent Verifier checks and executable lowering remain open.
- CI run `42455141289` passed Ubuntu/macOS/Windows, ASan/UBSan and Valgrind with 60/60 CTest. Follow-up run `42455300701` also passed all five jobs after the dedicated legacy AST codec round-trip and invalid-tag boundary regression were added.
- Follow-up run `42455583043` passed all five jobs with 60/60 CTest after verifying that a failed Struct literal cannot finalize SemanticResult.
- Follow-up run `42455856774` passed all five jobs with 60/60 CTest after adding Group factory-return and nested Group type-isolation cases.
