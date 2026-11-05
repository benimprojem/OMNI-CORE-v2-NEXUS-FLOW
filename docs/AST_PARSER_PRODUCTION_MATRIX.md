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
| `f:name(...)`, `exf:name(...)`, main CLI args | `parse_func_decl()`, simple parameter parser | `AST_FUNC_DECL` / `AST_EXTERN_DECL`; `AST_ARRAY_TYPE` for argv | parameter separator; main CLI parameter AST; aggregate fixture | ~ Named/default/variadic parameter cases and signature spans remain |
| Lambda / anonymous function | `parse_lambda()` | `AST_LAMBDA` with params and expression/block body | rules/lambda combined fixture + AST codec | ~ Isolate capture/parameter/body cases; closure semantics later |
| Block `{...}`, nested blocks | `parse_block()` | `AST_BLOCK` | nested-block recovery; halt codec/context test | ~ Source span + legal block-halt position fixtures |
| `if / elsif / els` | `parse_if_statement()`, `parse_if_branch()` | `AST_IF` chain/nested else | structured if-elsif-els AST test | ✓ shape asserted; negative missing/duplicate branch cases expand |
| Conditional action `(cond) -> break/continue/...` | `parse_conditional_action()` | canonical `AST_IF` | conditional-action arrow disambiguation | ~ More action kinds and expression-vs-flow ambiguity |
| Unified `loop` / foreach / unroll | statement dispatch + `parse_unroll()` | `AST_LOOP`, `AST_UNROLL` | rules/lambda/relocate/unroll fixture | ~ Isolate all loop forms, malformed headers and nesting |
| `return`, `break`, `continue` | `parse_return_statement()` + statement dispatch | `AST_RETURN`, `AST_BREAK`, `AST_CONTINUE` | parser general fixtures; validator context tests | ~ Need explicit parser shape tests for each target/operand form |
| Struct / union / enum / newtype | `parse_struct_decl()`, `parse_union_decl()`, `parse_enum_decl()`, `parse_newtype_decl()` | matching declaration tags + fields/variants | aggregate declaration AST; invalid aggregate members | ✓ primary forms tested; malformed variants/data and span checks expand |
| Group / method | `parse_group_decl()` | `AST_GROUP_DECL`, `AST_METHOD` | `test_group_method_ast`: group member, public method name, implementation, return type/body and AST codec round-trip | ~ Positive shape/codec covered; malformed group/member and span cases remain |
| Module `use` | `parse_use()` | `AST_USE` | syntax matrix / generic parser acceptance | ~ Alias/path/invalid path AST tests |
| `must` declaration | `parse_must_decl()` | contextual `AST_MUST_DECL` | contextual AST codec fixture; graph validator context tests | ~ Parser production needs positive + illegal scope tests |
| Defer / thread | `parse_defer()`, `parse_thread_block()` | `AST_DEFER`, `AST_THREAD_BLOCK` | syntax matrix / codec populate fixture | ~ Dedicated parser structure and malformed form tests |
| Select / scan / patterns | `parse_select_scan()`, `parse_pattern()` | `AST_SELECT/SCAN`, arms, pattern nodes | select-scan AST test | ~ Constructor/guard/catchall/duplicate pattern matrix |
| Ruleset / apply / on / rule clauses | `parse_ruleset()`, `parse_apply_rules()`, `parse_on_block()` | `AST_RULESET_DECL`, rule nodes, `AST_APPLY_RULES`, `AST_ON_BLOCK` | combined rules/lambda/relocate/unroll fixture | ~ Split each production and malformed clause tests |
| FASTEXEC / ASM label raw body | `parse_fast_exec_block()`, `parse_asm_label()` | `AST_FASTEXEC_BLOCK`, `AST_ASM_LABEL` | `test_fast_exec_raw_asm_ast`, `test_fast_exec_asm_token_stream`, contextual codec fixture; exact raw bytes and source spans asserted; outside-FASTEXEC ASM rejected | ~ Parser/AST/codec shape is tested; semantic variable/register/clobber/label/ABI validation is not AST work and remains a later Semantic/target contract |
| Assignment / compound / string append | `parse_assignment()` | `AST_ASSIGNMENT` retaining operator | power/bitwise/compound fixture | ~ `+=`, `&=` covered; complete operator set and `.=` need tests |
| Nexus Flow / pipe / relocate / unpack / capture / halt / jump / label | `parse_nexus_flow()` + statement/primary dispatch | flow/pipe/capture/relocate/unpack/halt/label/jump tags | syntax matrix + select/rules combined fixtures; graph contextual validator | ~ Explicit AST payload tests for each operator/target form; distinguish `<< / >>` language flow from bit shifts |
| Expressions and precedence | `parse_expression()`; assignment → flow → range → logical → bitwise → equality → comparison → additive → term → power → unary → postfix → primary | `AST_BINARY_EXPR`, unary, postfix, call/member/index, cast, tuple, literal, identifier | logical precedence; power/bitwise/compound; modulo; cast/panic; array/index tests | ~ Precedence table still incomplete; add associativity and AST-shape checks for every tier |
| Array literal / type / indexing | `parse_primary()`, var/type parser, `parse_postfix()` | `AST_ARRAY_LITERAL`, `AST_ARRAY_TYPE`, dimensions, `AST_INDEX_ACCESS` | array literal, declarator, index tests | ✓ parser structural cases covered; negative nested delimiters/rank/span expansion |
| Intrinsic calls | `parse_intrinsic_call()` | intrinsic-specific AST tags/shared `AST_CALL` payload | extern/echo/listen test + AST populated codec fixture | ~ Intrinsic token→node/signature inventory needed |
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
