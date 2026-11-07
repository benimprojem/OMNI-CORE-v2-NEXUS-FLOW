# AST Parser Production Matrix

**Branch:** `refactor/c-core-roadmap`  
**Audit:** 2026-11-05  
**Scope:** Nexus Flow source production → C Parser production/function → AST shape → regression fixture.

> A parser function existing is not proof of production completeness. A test is considered evidence only for the form/assertions it actually checks. This matrix does not mark Semantic/OIR/Codegen completion.

## Production and AST mapping

| Source production / family | Parser entry / builder | AST representation | Existing test evidence | Audit status / next action |
|---|---|---|---|---|
| Translation unit / declarations | parser root + declaration dispatch | `AST_PROGRAM` + declaration sibling chain | parser source accept/reject; AST↔OIR canonical roundtrip | ~ Root/declaration inventory needs full syntax-positive fixture |
| `v:`, `c:` declaration + initializer/type modifiers | `parse_var_decl()` | `AST_VAR_DECL`, optional inline type child | array types, parameter forms, parser/semantic fixtures | ~ Every modifier and initializer shape needs focused assertions |
| `f:name(...)`, `exf:name(...)`, main CLI args and array returns | `parse_func_decl()`, simple parameter parser | `AST_FUNC_DECL` / `AST_EXTERN_DECL`; `AST_ARRAY_TYPE` for argv and optional `return_array_type` | parameter separator; main CLI parameter AST; `!i32[]`, `!i32[n]`, `!f32[m][n]` return fixtures; AST stream v2 round-trip + v1 function-stream compatibility; OIR field 8 round-trip | ~ Array-return representation/codec is verified; named/default/variadic parameter cases and full signature-span matrix remain |
| Lambda / anonymous function | `parse_lambda()` | `AST_LAMBDA` with params and expression/block body | rules/lambda combined fixture + AST codec | ~ Isolate capture/parameter/body cases; closure semantics later |
| Block `{...}`, nested blocks | `parse_block()` | `AST_BLOCK` | nested-block recovery; halt codec/context test | ~ Source span + legal block-halt position fixtures |
| `if / elsif / els` | `parse_if_statement()`, `parse_if_branch()` | `AST_IF` chain/nested else | structured if-elsif-els AST test | ✓ shape asserted; negative missing/duplicate branch cases expand |
| Conditional action `(cond) -> break/continue/...` | `parse_conditional_action()` | canonical `AST_IF` | conditional-action arrow disambiguation | ~ More action kinds and expression-vs-flow ambiguity |
| Unified `loop` / foreach / unroll | statement dispatch + `parse_unroll()` | `AST_LOOP`, `AST_UNROLL` | rules/lambda/relocate/unroll fixture | ~ Isolate all loop forms, malformed headers and nesting |
| `return`, `break`, `continue` | `parse_return_statement()` + statement dispatch | `AST_RETURN`, `AST_BREAK`, `AST_CONTINUE` | parser general fixtures; validator context tests | ~ Need explicit parser shape tests for each target/operand form |
| Struct / union / enum / newtype | `parse_struct_decl()`, `parse_union_decl()`, `parse_enum_decl()`, `parse_newtype_decl()` | matching declaration tags + fields/variants | aggregate declaration AST; invalid aggregate members | ✓ primary forms tested; malformed variants/data and span checks expand |
| Group / method | `parse_group_decl()` | `AST_GROUP_DECL`, `AST_METHOD` | `test_group_method_ast`: group member, public method name, implementation, return type/body and AST codec round-trip | ~ Positive shape/codec covered; malformed group/member and span cases remain |
| Map type | `parse_var_decl()`, function parameter parser | `AST_VAR_DECL.type_name` (`map` for ordinary parameter; `!map` for explicit hard parameter; local declaration currently hard-typed) | `test_parser_accepts_and_rejects_sources` inline fixture verifies ordinary `a:map` and compatibility `a!map`; generic syntax is currently unsupported, not normatively banned | ✓ Unparameterized map type represented; per-entry key/value semantics remain Semantic work |
| Module `use` | `parse_use()` | `AST_USE` | syntax matrix / generic parser acceptance | ~ Alias/path/invalid path AST tests |
| `must` declaration | `parse_must_decl()` | contextual `AST_MUST_DECL` | `test_must_top_level_ast` asserts program-scope name/source/span; `test_must_scope_rejected` rejects function-block scope; graph validator context tests | ~ Parser positive/negative scope behavior is now tested; malformed source expression and diagnostic-span cases remain |
| Defer / thread | `parse_defer()`, `parse_thread_block()` | `AST_DEFER`, `AST_THREAD_BLOCK` | `test_thread_block_ast` asserts named body, optional NULL mode/timeout, source span and AST codec round-trip | ~ Thread-block declaration shape is tested; defer and malformed thread-block forms remain |
| Select / scan / patterns | `parse_select_scan()`, `parse_pattern()` | `AST_SELECT/SCAN`, arms, pattern nodes | select-scan AST test | ~ Constructor/guard/catchall/duplicate pattern matrix |
| Ruleset / apply / on / rule clauses | `parse_ruleset()`, `parse_apply_rules()`, `parse_on_block()` | `AST_RULESET_DECL`, rule nodes, `AST_APPLY_RULES`, `AST_ON_BLOCK` | combined rules/lambda/relocate/unroll fixture | ~ Split each production and malformed clause tests |
| FASTEXEC / ASM label raw body | `parse_fast_exec_block()`, `parse_asm_label()` | `AST_FASTEXEC_BLOCK`, `AST_ASM_LABEL` | `test_fast_exec_raw_asm_ast`, `test_fast_exec_asm_token_stream`, contextual codec fixture; exact raw bytes and source spans asserted; outside-FASTEXEC ASM rejected | ~ Parser/AST/codec shape is tested; semantic variable/register/clobber/label/ABI validation is not AST work and remains a later Semantic/target contract |
| Assignment / compound / string append | `parse_assignment()` | `AST_ASSIGNMENT` retaining operator | power/bitwise/compound fixture | ~ `+=`, `&=` covered; complete operator set and `.=` need tests |
| Nexus Flow / pipe / relocate / unpack / capture / halt / jump / label | `parse_nexus_flow()` + statement/primary dispatch | flow/pipe/capture/relocate/unpack/halt/label/jump tags | syntax matrix + select/rules combined fixtures; graph contextual validator | ~ Explicit AST payload tests for each operator/target form; distinguish `<< / >>` language flow from bit shifts |
| Expressions and precedence | `parse_expression()`; assignment → flow → range → logical → bitwise → equality → comparison → additive → term → power → unary → postfix → primary | `AST_BINARY_EXPR`, unary, postfix, call/member/index, cast, tuple, literal, identifier | `test_expression_precedence_and_associativity_ast` checks logical/bitwise/comparison precedence, binary associativity, power associativity, inclusive range AST (`1..10`) and rejection of chained ranges | ✓ Precedence/associativity CI `42034256447` and range CI `42039331167` passed all five jobs; Ubuntu and Valgrind report 60/60. |
| Array literal / type / indexing | `parse_primary()`, var/type parser, `parse_postfix()` | `AST_ARRAY_LITERAL`, `AST_ARRAY_TYPE`, dimensions, `AST_INDEX_ACCESS` | array literal, declarator, index tests | ✓ parser structural cases covered; negative nested delimiters/rank/span expansion |
| Intrinsic calls | `parse_intrinsic_call()` plus special `cast`/`panic` branches; `listen` has a dedicated statement form | 32 generic intrinsic tags are checked against exact AST tags and AST serialize→deserialize→validate; `echo`/`listen` use a separate statement fixture; `cast`/`panic` have dedicated payload/span assertions | `test_intrinsic_ast_family` (32 generic mappings + codec round-trip + source-span presence), `test_extern_echo_and_listen_keywords`, `test_cast_and_panic_ast` | ~ Parser tag/codec baseline now covered; remaining work is signature-specific positive/negative grammar, exact source-span endpoints, and intrinsic-specific payload semantics. |
| Panic | statement/primary dispatch | `AST_PANIC` | cast/panic AST test | ✓ literal message path asserted; invalid message forms |
| Literal / identifier / tuple / cast / postfix | `parse_primary()`, `parse_postfix()`, `parse_unary()` | corresponding AST leaf/expression nodes | expression-focused tests above | ~ char/bool/string escapes, tuple edge and postfix lvalue forms |
| Error recovery / limits / OOM | parser recovery and allocation helpers | diagnostics; partial AST must not be accepted | panic recovery, lexer-error recovery, nesting/operator bounds, allocation injection | ✓ infrastructure tested; expand grammar-specific recovery boundaries |

## Test inventory (actual C test names)

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
