# AST Parser Production Matrix

**Branch:** `refactor/c-core-roadmap`  
**Audit:** 2026-11-08  
**Scope:** Nexus Flow source production → C Parser production/function → AST shape → regression fixture.

> A parser function existing is not proof of production completeness. A test is considered evidence only for the form/assertions it actually checks. This matrix does not mark Semantic/OIR/Codegen completion.

## Production and AST mapping

| Source production / family | Parser entry / builder | AST representation | Existing test evidence | Audit status / next action |
|---|---|---|---|---|
| Translation unit / declarations | parser root + declaration dispatch | `AST_PROGRAM` + declaration sibling chain | parser source accept/reject; full-source `AST_PROGRAM` byte range/line-column span and AST codec round-trip; AST↔OIR canonical roundtrip | ~ Root span is asserted; full syntax-positive declaration inventory remains |
| `v:`, `c:` declaration + initializer/type modifiers, pointer/reference types | `parse_var_decl()`, `parser_parse_type_name()` | `AST_VAR_DECL.type_name` preserves `*T`, `**T`, `&T`; arrays remain a separate inline child | `test_pointer_reference_type_declarators` covers local pointer, function params/return, NewType target and AST codec | ~ Parser spelling and AST string preservation covered; pointer validity, borrow/lifetime and ownership remain Semantic work |
| `f:name(...)`, `exf:name(...)`, main CLI args, pointer/reference parameters/returns and array returns | `parse_func_decl()`, simple parameter parser, `parser_parse_type_name()` | `AST_FUNC_DECL` / `AST_EXTERN_DECL`; type-name strings preserve pointer/reference prefixes; `AST_ARRAY_TYPE` for argv and optional `return_array_type`; default parameter `a:10` is `AST_VAR_DECL.initializer` | parameter separator; main CLI parameter AST; `!i32[]`, `!i32[n]`, `!f32[m][n]` return fixtures; AST stream v2 round-trip + v1 function-stream compatibility; OIR field 8 round-trip; function/root/block spans, variadic position, default initializer and named-call-argument shape/codec and parameter spans | ~ Function/root and parameter span samples plus named/default/variadic parameter AST forms are verified; full signature-span matrix remains |
| Lambda / anonymous function | `parse_lambda()` | `AST_LAMBDA` with params and expression/block body; expression body remains a direct expression child (no synthetic `AST_RETURN`) | combined rules/lambda fixture; exact expression/block span cases; `test_parser` now checks expression-vs-block body shape, malformed missing-body rejection, graph validation and AST codec round-trip (commits `c4177107`, `ccb4983a`, `bad267e6`; CI validated by workflow run `42411942789` (all 5 jobs passed)) | ~ Isolate parameter-list edge cases and capture representation; closure semantics and Semantic return-type inference remain later |
| Block `{...}`, nested blocks | `parse_block()` | `AST_BLOCK` | nested-block recovery; exact outer and nested block byte ranges, line/column endpoints, child order, graph validation and AST codec preservation; `}?;` final-child position | ~ Nested source spans now have an explicit regression; additional mixed-depth, empty-block and malformed-close recovery cases remain |
| Block-scoped resource cleanup `}?;` | `parse_block()` close-marker handling | `AST_BLOCK_HALT` as final child of the owning block | Exact marker/block source spans; final-child graph rule; root/non-final/wrong-edge rejection; legacy AST codec round-trip | ✓ Parser span, graph context/position and codec behavior are covered; cleanup execution order remains Semantic/OIR work |
| `if / elsif / els` | `parse_if_statement()`, `parse_if_branch()` | `AST_IF` chain/nested else | structured if-elsif-els AST test | ✓ shape asserted; negative missing/duplicate branch cases expand |
| Conditional action `(cond) -> break/continue/...` | `parse_conditional_action()` | canonical `AST_IF` | conditional-action arrow disambiguation | ~ More action kinds and expression-vs-flow ambiguity |
| Unified `loop` / foreach / unroll | statement dispatch + `parse_unroll()` | `AST_LOOP`, `AST_UNROLL` | rules/lambda/relocate/unroll fixture; `test_canonical_loop_and_jump_call_forms` covers `loop {}` | ~ `loop {}` is now accepted as the canonical infinite-loop form; isolate all other loop forms and malformed headers |
| `ret`, `break`, `continue` | `parse_return_statement()` + statement dispatch | `AST_RETURN`, `AST_BREAK`, `AST_CONTINUE` | `test_parser` asserts void/value return operands, distinct break/continue nodes, source-span presence, graph validation, AST codec round-trip, and missing-semicolon rejection (commit `7113e195`; CI validated by workflow run `42411942789` (all 5 jobs passed)) | ~ Expand target/operand forms only where normative syntax defines them; control-flow semantics remain later Semantic work |
| Struct / union / enum / newtype | `parse_struct_decl()`, `parse_union_decl()`, `parse_enum_decl()`, `parse_newtype_decl()` | matching declaration tags + fields/variants | aligned Struct, packed Struct and Union flags/field order; Enum payload/no-payload variants; malformed Enum payload names/types/trailing fields rejected; NewType target and exact declaration/variant/field spans; AST codec round-trip | ~ Primary aggregate AST shapes and selected ranges are tested; additional malformed aggregate forms and complete per-production span coverage remain |
| Struct literal expression `Type { field = expression, ... }` | `parse_struct_literal()` | `AST_STRUCT_LITERAL` + ordered `AST_STRUCT_FIELD_INIT` sibling chain | `Point { y = 2, x = 1 }` source-order and exact literal/field spans; nested `Outer { inner = Inner { ... } }` parser→AST→codec→validate round-trip; legacy AST codec | ✓ Nested payload writer/reader and appended-tag bound are tested; Semantic completeness and OIR execution lowering remain separate |
| Function type / callback signature | `parser_parse_type_name()`, function-type signature parser | Normalized `f:(name!T, ...)>!R` string in existing `type_name` / `target_type`; no dedicated AST node yet | `test_function_type_alias_and_callback_parameter` checks alias, parameter, invocation, malformed rejection, validator and codec round-trip | ~ Syntax and string preservation covered; dedicated type node, callback compatibility and ABI remain open |
| Generic type-variable spelling | Existing type-name parsing | `a!t` and return `!t` are preserved in `ASTVarDecl.type_name` / `ASTFuncDecl.return_type` strings | `test_implicit_type_variable_spelling` checks both parameters, return type, graph validation and codec round-trip; CI workflow `42411942789` passed 5/5 | ~ Syntax spelling is retained; Semantic type-variable binding, scope, unification and inference remain open. Explicit `<T>` list is not normative |
| Group / method | `parse_group_decl()` / `parse_group_body()` | `AST_GROUP_DECL`, `AST_METHOD`; nested group is child `AST_GROUP_DECL`; `default` is `AST_METHOD("default", AST_BLOCK)` | `test_group_method_ast` + `test_nested_group_and_default_ast`: method payload, nested alias/name, default blocks, AST validation and codec round-trip | ~ Nested/default syntax is now Parser-covered; `self` binding, group dispatch, struct↔group method association, constructor semantics, visibility and OIR lowering remain Semantic/OIR gaps |
| Map type | `parse_var_decl()`, function parameter parser | `AST_VAR_DECL.type_name` (`map` for ordinary parameter; `!map` for explicit hard parameter; local declaration currently hard-typed) | `test_parser_accepts_and_rejects_sources` inline fixture verifies ordinary `a:map` and compatibility `a!map`; generic syntax is currently unsupported, not normatively banned | ✓ Unparameterized map type represented; per-entry key/value semantics remain Semantic work |
| Module `use` | `parse_use()` | `AST_USE` | `test_dotted_module_use_path` covers `use msvcrt.exf as crt;`, alias, graph validation and AST codec round-trip | ~ Dotted path and alias shape covered; malformed dotted segment, comma-list and span boundary cases remain |
| `must` declaration | `parse_must_decl()` | contextual `AST_MUST_DECL` | `test_must_top_level_ast` asserts exact byte offsets and line/column endpoints; `test_must_scope_rejected` rejects function-block scope; graph validator context tests | ~ Parser positive/negative scope behavior is now tested; malformed source expression and diagnostic-span cases remain |
| Defer / named thread block | `parse_defer()`, `parse_thread_block()` | `AST_DEFER`, `AST_THREAD_BLOCK` | `(name)>{...}` emits named body; `test_thread_block_ast` asserts exact byte offsets and line/column endpoints, NULL mode/timeout and AST codec round-trip; Defer source-span assertion remains exact; `test_spawn_is_outside_core_parser` rejects core `spawn(...)` | ~ Thread block is a definition, not a start operation; malformed defer/thread forms remain |
| Select / scan / patterns | `parse_select_scan()`, `parse_pattern()` | `AST_SELECT/SCAN`, arms, pattern nodes | select-scan AST test | ~ Constructor/guard/catchall/duplicate pattern matrix |
| Ruleset / apply / on / rule clauses | `parse_ruleset()`, `parse_apply_rules()`, `parse_on_block()` | `AST_RULESET_DECL`, rule nodes, `AST_APPLY_RULES`, `AST_ON_BLOCK` | combined rules/lambda/relocate/unroll fixture | ~ Split each production and malformed clause tests |
| FASTEXEC / ASM label raw body | `parse_fast_exec_block()`, `parse_asm_label()` | `AST_FASTEXEC_BLOCK`, `AST_ASM_LABEL` | `test_fast_exec_raw_asm_ast`, `test_fast_exec_asm_token_stream`, contextual codec fixture; exact raw bytes and source spans asserted; outside-FASTEXEC ASM rejected | ~ Parser/AST/codec shape is tested; semantic variable/register/clobber/label/ABI validation is not AST work and remains a later Semantic/target contract |
| Assignment / compound / string append | `parse_assignment()` | `AST_ASSIGNMENT` retaining operator | power/bitwise/compound fixture | ~ `+=`, `&=` covered; complete operator set and `.=` need tests |
| Nexus Flow / pipe / relocate / unpack / capture / halt / jump / label | `parse_nexus_flow()` + statement dispatch | flow/pipe/capture/relocate/unpack/halt/label/jump tags | `test_canonical_loop_and_jump_call_forms` covers `jump(label);`; existing fixture covers `?>(label);` | ~ Both `?>(label);` and contextual `jump(label);` produce `AST_NEXUS_JUMP`; verify all flow operands and `<< / >>` collision separately |
| Expressions and precedence | `parse_expression()`; assignment → flow → range → logical → bitwise → equality → comparison → additive → term → power → unary → postfix → primary | `AST_BINARY_EXPR`, unary, postfix, call/member/index, cast, tuple, literal, identifier | `test_expression_precedence_and_associativity_ast` checks logical/bitwise/comparison precedence, binary associativity, power associativity, inclusive range AST (`1..10`) and rejection of chained ranges | ✓ Precedence/associativity CI `42034256447` and range CI `42039331167` passed all five jobs; Ubuntu and Valgrind report 60/60. |
| Array literal / type / indexing | `parse_primary()`, var/type parser, `parse_postfix()` | `AST_ARRAY_LITERAL`, `AST_ARRAY_TYPE`, dimensions, `AST_INDEX_ACCESS` | array literal, declarator, index tests; exact dynamic `[]`, local `[3][3]` and function-return `[]`/`[n]`/`[m][n]` array/dimension spans | ✓ Array-type/dimension source ranges are asserted for dynamic, fixed and multidimensional parameter/local/return forms; array literal/index spans and negative nested delimiters/rank expansion remain |
| Intrinsic calls | `parse_intrinsic_call()` plus special `cast`/`panic` branches; `listen` has a dedicated statement form | 32 generic intrinsic tags are checked against exact AST tags and AST serialize→deserialize→validate | `test_intrinsic_ast_family` checks all 32 mappings, exact start/end byte offsets, line/column endpoints and codec round-trip; `echo`/`listen` and `cast`/`panic` have separate payload/span fixtures | ~ Parser tag, exact span and codec baseline covered; signature-specific positive/negative grammar and intrinsic-specific payload semantics remain. |
| Panic | statement/primary dispatch | `AST_PANIC` | cast/panic AST test | ✓ literal message path asserted; invalid message forms |
| Literal / identifier / tuple / cast / postfix | `parse_primary()`, `parse_postfix()`, `parse_unary()` | corresponding AST leaf/expression nodes; `TOKEN_NULL` maps to `AST_LITERAL` | `test_null_and_numeric_literal_ast`; `test_lexer` accepted/rejected radix, exponent and separator cases; expression-focused tests above | ~ Numeric value conversion/overflow, null pointer compatibility, char/string escape and postfix lvalue semantics remain |
| Error recovery / limits / OOM | parser recovery and allocation helpers | diagnostics; partial AST must not be accepted | panic recovery, lexer-error recovery, nesting/operator bounds, allocation injection | ✓ infrastructure tested; expand grammar-specific recovery boundaries |

## Test inventory (actual C test names)

- `test_canonical_loop_and_jump_call_forms`
- `test_dotted_module_use_path`
- `test_pointer_reference_type_declarators`
- `test_function_type_alias_and_callback_parameter`
- `test_null_and_numeric_literal_ast`
- `test_implicit_type_variable_spelling`
- `test_cast_and_panic_ast`
- `test_aggregate_declaration_ast`
- `test_rejects_invalid_aggregate_members`
- `test_parser_panic_mode_recovers_at_statement_boundaries`
- `test_parser_nested_block_recovery`
- `test_parser_malformed_function_recovery`
- `test_parser_preserves_lexer_errors_during_recovery`
- `test_parser_allocation_failure_injection`
- `test_function_parameter_type_separator_forms`
- `test_structured_if_elsif_els_ast`
- `test_conditional_action_arrow_disambiguation`
- `test_select_scan_ast`
- `test_rules_lambda_relocate_unroll_ast`
- `test_array_literal_ast`
- `test_array_type_declarators`
- `test_array_index_access_ast`
- `test_logical_operator_precedence_ast`
- `test_power_bitwise_and_compound_assignment`
- `test_extern_echo_and_listen_keywords`
- `test_modulo_expression_ast`
- `test_retry_metadata_rejects_overflow`
- `test_fast_exec_raw_asm_ast`
- `test_fast_exec_asm_token_stream`
- `test_main_cli_parameter_ast`
- `test_group_method_ast`
- `test_thread_block_ast`
- `test_spawn_is_outside_core_parser`

## Parser-to-AST closure checklist

1. Turn every syntax-reference production into a named row (including declaration modifiers, every intrinsic and every Nexus operator form).
2. For each row, assert AST tag, child order, payload/operator, source span and sibling ownership—not just parser success.
3. Add a paired invalid input for missing delimiter, invalid context, invalid payload or illegal child.
4. Keep contextual AST nodes tested under valid owners (e.g. `AST_MUST_DECL` under program declarations, `AST_VARIADIC_PARAM` in params, `AST_BLOCK_HALT` as final block child, `AST_ASM_LABEL` under FASTEXEC).
5. Use syntax-reference spellings only; do not introduce C/C++ or alternate-language examples.
6. AST phase closes only when syntax inventory, parser behavior, graph contract, codec, spans and tests are reconciled. Semantic/OIR implementation status remains a separate gate.


## Source-span test evidence (2026-11-05 audit)

Parser source-span assertions are distributed through `test_parser.c` rather than a single span test. The current explicit cases include:

- UTF-8 byte offsets and multiline program/variable endpoints.
- Cast expression boundaries.
- Struct/newtype/enum/union declaration and member/variant/data spans.
- Ruleset, barrier, apply-rules and on-block ranges.
- Lambda expression and block body boundaries.
- Relocate, unroll and rolling retry expression boundaries.
- FASTEXEC raw ASM declaration and attached label/body spans.
- Capture, function parameter, variadic marker, Nexus label, group/member and targetless Halt.
- `!(listen)` full statement span beginning at `!` and ending after its handler block; `!struct:Packed` full declaration span beginning at the hard-pack marker.
- `AST_USE` uses an import-item span (module identifier through optional alias), not the shared `use` keyword or semicolon; `AST_DEFER` uses the full `defer` statement span.\n- Named call argument `b:2` is asserted from the label start through the value expression end, including byte offsets and line/column endpoints.\n- Default function parameters `a:10` / `b:20` are asserted from parameter identifier through default expression; their values remain `AST_VAR_DECL.initializer`, distinct from `AST_NAMED_ARG` call arguments.\n- Struct literal `Point { y = 2, x = 1 }` and each field initializer are asserted from their own syntax start through the closing brace/value expression respectively.
- `asmcall`/`asmjmp` full call ranges.

This is a **tested subset**, not blanket source-span coverage for all productions. AST closure requires each row to declare its span policy (token-only, full production, or synthesized/no source) and have assertions for both endpoints where location is required. UTF-8 offsets are byte offsets; line/column are separately validated.


## Intrinsic token → AST audit (2026-11-07)

The generic intrinsic dispatcher maps the following lexer keywords to AST tags. The expanded `test_intrinsic_ast_family()` covers each listed mapping with a one-expression source, AST validation, source-span presence, and binary serialize → deserialize → graph-validation round-trip.

| Source form | AST tag | Parser path |
|---|---|---|
| `len(...)` | `AST_LEN` | generic intrinsic call |
| `sizeof(...)` | `AST_SIZEOF` | generic intrinsic call |
| `typeof(...)` | `AST_TYPEOF` | generic intrinsic call |
| `is_ok(...)`, `is_err(...)`, `is_some(...)`, `is_none(...)`, `is_null(...)` | matching `AST_IS_*` tag | generic intrinsic call |
| `addr(...)` | `AST_ADDR` | generic intrinsic call |
| `clock()`, `datenow()`, `date()`, `time()` | `AST_CLOCK`, `AST_DATENOW`, `AST_DATE`, `AST_TIME` | generic intrinsic call |
| `prnt(...)`, `prmt(...)`, `write(...)`, `fwrite(...)` | matching `AST_PRNT`, `AST_PRMT`, `AST_WRITE`, `AST_FWRITE` | generic intrinsic call |
| `exit(...)`, `swap(...)` | `AST_EXIT`, `AST_SWAP` | generic intrinsic call |
| `peek(...)`, `poke(...)`, `inb(...)`, `outb(...)` | matching `AST_PEEK`, `AST_POKE`, `AST_INB`, `AST_OUTB` | generic intrinsic call |
| `irq(...)`, `intr(...)` | `AST_IRQ`, `AST_INTR` | generic intrinsic call |
| `reg.member(...)` | `AST_REG` with member `AST_IDENTIFIER` in `callee` | generic call with dedicated member parsing |
| `area(...)`, `zone(...)` | `AST_AREA`, `AST_ZONE` | generic intrinsic call |
| `free(...)`, `send(...)`, `receive(...)`, `done()` | `AST_FREE`, `AST_SEND`, `AST_RECEIVE`, `AST_DONE` | generic intrinsic call |

Special statement/expression forms are covered separately: `echo(...)` → `AST_ECHO`; `!(listen): {...}` → `AST_LISTEN`; `cast(value, type)` → `AST_CAST` with value/target-type children; `panic("...")` → `AST_PANIC` with a message payload. `spawn` remains rejected by the core Parser and belongs to the Thread module.

**CI evidence:** commit `237597ca5a64`, run `42401080171`: Windows, Ubuntu, macOS, ASan/UBSan and Valgrind all passed. Ubuntu CTest and Valgrind each report 60/60 tests, zero failures.

**Not implied by this table:** Semantic argument count/type rules, hardware permission checks, runtime effects, and target-specific legality are not established by parser/codec round-trip coverage.


## Source-span audit — packed struct and listen fixes (2026-11-07)

- The statement dispatcher had two direct-return paths that bypassed the common source-span stamping used by adjacent declarations: packed `!struct` and `!(listen)`.
- Both now stamp the full syntactic production range. `AST_LISTEN` line/column mirrors the opening `!` token instead of the consumed colon.
- `test_extern_echo_and_listen_keywords()` now asserts exact start/end byte offsets and start location for both `AST_LISTEN` and packed `AST_STRUCT_DECL`.
- CI run `42401418322` / commit `ac13f7056143`: all five jobs passed (Windows, Ubuntu, macOS, ASan/UBSan, Valgrind); Ubuntu and Valgrind CTest each report 60/60.
- The audit is incremental: other helper functions may receive their span from the enclosing statement dispatcher. Do not infer a missing span merely because the helper itself does not call a stamping function; trace the complete parser path.


## Source-span audit — use items and defer (2026-11-07)

- Confirmed `AST_USE` is one AST node per imported module in a comma-separated declaration. Its intended span is the individual import item: module identifier through optional `as Alias`; the shared `use` keyword and final semicolon are outside the individual node range.
- Extended the use/defer Parser fixture to assert exact offsets for both import-item shapes (aliased and unaliased) and the full `defer { ... }` statement.
- The first test iteration exposed a fixture delimiter typo (`Core;` instead of the actual `Core,` separator); corrected the test expectation. No Parser production change was needed.
- CI run `42401799340` / commit `41372b18b594`: all five jobs passed (Windows, Ubuntu, macOS, ASan/UBSan and Valgrind); Ubuntu and Valgrind report 60/60 tests.


## Parser/spec reconciliation item — expression-bodied named functions (2026-11-07)

- `parse_func_decl()` currently accepts a `>` immediately after a named function's parameter list and synthesizes an `AST_BLOCK` containing an `AST_RETURN` for the parsed expression.
- The normative `docs/language/syntax-reference.md` and the named-function row in `NEXSUS_AST_SYNTAX_MATRIX.md` document block-bodied named functions; the expression-body spelling is documented for `AST_LAMBDA` (`f:(a,b)>expr`), not for `f:name(...)`.
- This is a Parser/spec discrepancy, not permission to silently expand the language grammar. Preserve the current Parser behavior for now; decide whether the named-function shorthand is a legacy compatibility form or should be rejected only after reviewing existing examples and compatibility requirements.
- The generated block/return nodes have no literal source token of their own. Their span policy must be decided explicitly (synthesized/no-location vs expression-derived) before adding assertions or changing stamping behavior.


## 2026-11-07 — Syntax-to-Parser reconciliation

- Added the documented infinite-loop spelling `loop { ... }` to `parse_primary()`; existing `(loop) { ... }` compatibility form remains accepted.
- Added contextual statement alias `jump(label);`; it produces the same `AST_NEXUS_JUMP` payload as the explicit `?>(label);` form. The word `jump` remains an ordinary identifier outside this statement-call shape.
- Added `test_canonical_loop_and_jump_call_forms`: verifies AST shape, loop type/body, jump target, following label, graph validation and AST stream round-trip.
- CI validated by workflow run `42411942789`; all five jobs passed.
- Remaining type grammar gaps are function-type/callback AST and generic type-parameter declarations. Pointer/reference declarators have since been added to Parser; see the later 2026-11-07 entry.


## 2026-11-07 — Dotted module path production

- Extended `parse_use()` to accept dotted module paths such as `use msvcrt.exf as crt;`.
- Module path segments are joined into the existing owned `AST_USE.module_name` string; no AST tag or OIR field layout changed.
- Added `test_dotted_module_use_path` for the joined path, alias, graph validation and AST codec round-trip.
- CI validated by workflow run `42411942789`; malformed path segments and exact span boundaries remain in the test closure list.


## 2026-11-07 — Pointer/reference type spelling

- Added a shared Parser type-name reader for `T`, `*T`, `**T` and `&T` in variable declarations, function parameters, return types and NewType targets.
- The existing AST stores these forms in the existing owned type-name strings; no AST enum ID or OIR node ID changed.
- Added `test_pointer_reference_type_declarators` for local variable, typed parameters, return type, NewType alias, graph validation and AST stream round-trip.
- This closes the Parser spelling gap only. Nullability, pointer validity, mutability, borrow/lifetime, aliasing and ownership remain Semantic contracts.
- Parser commit: `0b87bd7b`; test commit: `df9ef675`. CI validated by workflow run `42411942789` (all 5 jobs passed).


## 2026-11-07 — Function type / callback signature production

- Added `f:(name!Type, ...)>!ReturnType` as a type expression in NewType targets, local type declarations, function parameters and return types.
- The signature is whitespace-normalized and retained in the existing owned type-name/target-type string fields; it is distinct from `AST_LAMBDA` expression bodies.
- Added `test_function_type_alias_and_callback_parameter` for NewType alias, callback parameter, invocation shape, AST validation, stream round-trip and malformed-signature rejection.
- This is syntax/AST string preservation, not full callback type compatibility, closure capture, ABI lowering or a dedicated `AST_FUNCTION_TYPE` node.
- Parser commit: `486a92ba`; regression test commit: `51198999`. CI validated by workflow run `42411942789` (all 5 jobs passed).


## 2026-11-07 — AST enum tags without active Parser productions

The active `ASTNodeType` enum contains 105 append-only tags. A source-to-AST audit found the following tags are intentionally legacy/reserved rather than active Parser outputs:

| AST tag | Current contract |
|---|---|
| `AST_SPAWN` | Legacy serialized tag only. New spawn syntax belongs to the separate Thread module and is intentionally rejected by the core Parser. |
| `AST_ASM_BLOCK` | Legacy tag. Current FASTEXEC/raw-ASM syntax emits `AST_FASTEXEC_BLOCK` and `AST_ASM_LABEL`. |
| `AST_ZONE_SYNC`, `AST_ZONE_VIEW`, `AST_ZONE_LOCK` | Reserved tags with no direct Parser production. `zone.sync()`, `zone.view()` and `zone.lock()` currently parse as generic member calls. |

These tags must not be counted as active syntax support merely because they exist in the AST enum or OIR registry. Their removal is not proposed; legacy serialization compatibility and future module boundaries are preserved.


## 2026-11-07 — Numeric and null literal grammar

- Lexer now accepts decimal integers/fractions, hexadecimal `0x`, binary `0b`, octal `0o`, decimal scientific notation and underscore digit separators.
- Numeric type suffixes remain rejected as a single lexical error; rolling `500ms` remains NUMBER + `ms` identifier.
- Added `TOKEN_NULL` at the end of the token enum to preserve existing token IDs. Parser emits `AST_LITERAL` with the original `null` spelling.
- Added Lexer positive/negative numeric fixtures and Parser AST/codec coverage for null, radix and scientific literals.
- Numeric value conversion, overflow/range and null-to-pointer compatibility remain Semantic work. `none` / Option is still not a literal/type contract.
- CI validated by workflow run `42411942789` (all 5 jobs passed).


## Remaining syntax/AST representation gaps after the 2026-11-07 reconciliation

| Area | Current state | Closure requirement |
|---|---|---|
| Generic type variables | `a!t` / `!t` spelling is preserved in existing type-name strings; explicit `<T>` declaration is not normative | Semantic binding/scope/unification/inference rules; decide only later whether explicit type parameter declarations are needed |
| Function type representation | Parser accepts and preserves signature strings | Decide whether dedicated `AST_FUNCTION_TYPE` node is required; define callback compatibility and ABI lowering |
| Pointer/reference type representation | Parser preserves `*T`, `**T`, `&T` in type-name strings | Decide unified type descriptor; Semantic nullability, mutability, borrow, aliasing and lifetime checks |
| Option/Result | `null` literal exists; `none`/Option/Result constructors not defined | Define Option/Result types, constructors, propagation and predicates |
| Numeric semantics | Lexer/Parser accepts radix/exponent/separator forms | Semantic conversion, overflow, precision, suffix policy and promotion |
| Shift vs Flow | `<<`/ `>>` are Flow tokens | Define a non-ambiguous numeric shift form; do not infer from operand shapes |
| Directive / macro / DLL export | Core Parser does not consume prefix target directives, macros or `dllexp:` | Separate Preprocessor/Target/Build grammar and metadata contract |
| Source spans / AST lifecycle | Several targeted tests exist | Complete per-production exact spans, factory/destructor/codec/validator parity and AST allocation-failure injection |


## 2026-11-07 — Named Thread Block syntax

- Normative syntax: `(name)>{ ... }`; `>{` is the dedicated thread-block opener token.
- Parser creates `AST_THREAD_BLOCK` with the name and body. `mode` and `timeout` are currently NULL/reserved fields; no syntax is defined for them.
- The block defines a named work body; it does not start a thread.
- `spawn(...)` remains outside core Parser syntax and belongs to the separate Thread module. The existing negative fixture rejects `spawn(worker)`.
- Existing `test_thread_block_ast` and exact source-span fixture cover the parser/AST/codec path; latest CI workflow `42411942789` passed all five jobs.
\n## 2026-11-08 — Enum and Group AST ownership regressions

- Enum fixture now follows the normative `nt:enum: Task { ... }` syntax. It checks payload and no-payload variants, ordered payload fields, declaration/variant/field byte ranges, AST validation and legacy codec round-trip.
- Group method AST fixtures cover instance method implementation, `default` block dispatch and nested Group method ownership. The validator now permits a missing/empty implementation function name only when that function is reached through an `AST_METHOD.implementation` edge; ordinary function declarations still require a name.
- Nested Group source-span assertion now points at the actual nested closing brace (not the following newline).
- The generic populated/all-tag AST codec tests now exercise all 107 tags, wrapping context-bound nodes in their legal owner edges before serialize/deserialize validation.
## 2026-11-08 — Parser allocation and aggregate-error closure

- The Struct literal exact-range parser fixture was moved out of an unreachable OOM-test error branch and registered as an independent parser test. It now runs in every CTest invocation and validates the resulting AST graph.
- Parser OOM injection now traverses named arguments, Struct literal, Enum, Union and Group method/default syntax. The error branch no longer double-destroys the parser context.
- Exact source-range assertions now cover top-level `must` and named Thread Block nodes, including byte offsets and start/end columns.
- Malformed Enum payload fixtures exposed a double-free/use-after-free and a separate pending-field leak. The payload list is now transferred to the Enum variant only after successful parsing; early error paths explicitly release the still-local pending field list.\n\n## 2026-11-08 — Enum canonical syntax and intrinsic span reconciliation\n\n- Enum parser fixture uses the normative `nt:enum: Task { IO(file!str) ... }` spelling and newline-separated variants exactly as in the syntax reference. It checks ordered payload fields, no-payload variants, byte spans and legacy codec round-trip.\n- The intrinsic family test already asserts exact start/end byte offsets and line/column for all 32 generic intrinsic tags; the production matrix now reflects that evidence rather than describing span presence only.\n- CI run `42480076522` passed Windows, Ubuntu, macOS, ASan/UBSan and Valgrind (5/5); Ubuntu CTest reported 60/60.\n\n- Nested block source-span regression now checks outer and inner `{...}` byte offsets, start/end line and column, child ownership and legacy AST codec preservation. CI run `42480323113` passed all five jobs (Windows, Ubuntu, macOS, ASan/UBSan and Valgrind); Ubuntu CTest 60/60.\n