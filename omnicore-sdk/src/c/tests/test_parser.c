#include "parser.h"
#include "ast.h"
#include "lexer.h"

#include <stdio.h>
#include <string.h>

static int write_invalid_source(const char* path) {
    static const char source[] = "v:x = ;\n";
    FILE* file = fopen(path, "wb");
    if (!file) return 0;
    int ok = fwrite(source, 1, sizeof(source) - 1u, file) == sizeof(source) - 1u;
    if (fclose(file) != 0) ok = 0;
    return ok;
}

static int write_invalid_second_line_source(const char* path) {
    static const char source[] = "v:x = 1;\nv:x = ;\n";
    FILE* file = fopen(path, "wb");
    if (!file) return 0;
    int ok = fwrite(source, 1, sizeof(source) - 1u, file) == sizeof(source) - 1u;
    if (fclose(file) != 0) ok = 0;
    return ok;
}

static int write_invalid_lexical_source(const char* path) {
    static const char source[] = "v:x = \xC2\xA7;\n";
    FILE* file = fopen(path, "wb");
    if (!file) return 0;
    int ok = fwrite(source, 1, sizeof(source) - 1u, file) == sizeof(source) - 1u;
    if (fclose(file) != 0) ok = 0;
    return ok;
}

static int test_cast_and_panic_ast(void) {
    static const char source[] =
        "f:main() { v:x = cast(1, i32); panic(\"boom\"); }";
    ParserContext* ctx = parser_init_source(
        "cast-panic-test.nxf", (const uint8_t*)source, sizeof(source) - 1u);
    ASTProgram* program;
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTVarDecl* variable;
    ASTCast* cast;
    ASTPanic* panic;
    int ok = 0;
    if (!ctx) return 0;
    if (parser_run(ctx) != PARSER_STATUS_OK || parser_get_status(ctx) != PARSER_STATUS_OK || ctx->error_count != 0 || !ctx->ast_root) goto done;
    program = (ASTProgram*)ctx->ast_root;
    function = (ASTFuncDecl*)program->declarations;
    if (!function || function->base.type != AST_FUNC_DECL) goto done;
    body = (ASTBlock*)function->body;
    if (!body || body->base.type != AST_BLOCK) goto done;
    variable = (ASTVarDecl*)body->statements;
    if (!variable || variable->base.type != AST_VAR_DECL ||
        !variable->initializer || variable->initializer->type != AST_CAST) goto done;
    cast = (ASTCast*)variable->initializer;
    if (!cast->value || cast->value->type != AST_LITERAL ||
        !cast->target_type || cast->target_type->type != AST_IDENTIFIER ||
        strcmp(((ASTIdentifier*)cast->target_type)->name, "i32") != 0) goto done;
    panic = (ASTPanic*)variable->base.next;
    if (!panic || panic->base.type != AST_PANIC ||
        !panic->message || strcmp(panic->message, "\"boom\"") != 0) goto done;
    ok = 1;
done:
    parser_destroy(ctx);
    return ok;
}

static int test_parser_rejects_excessive_expression_nesting(void) {
    enum { NESTING = 300 };
    char source[2 * NESTING + 32];
    size_t offset = 0u;
    ParserContext* ctx;
    int result;
    int ok;

    memcpy(source + offset, "v:x = ", 6u);
    offset += 6u;
    for (int i = 0; i < NESTING; ++i) source[offset++] = '(';
    source[offset++] = '1';
    for (int i = 0; i < NESTING; ++i) source[offset++] = ')';
    source[offset++] = ';';

    ctx = parser_init_source("deep-expression.nxf",
        (const uint8_t*)source, offset);
    if (!ctx) return 0;

    result = parser_run(ctx);
    ok = result == PARSER_STATUS_SYNTAX_ERROR &&
        parser_get_status(ctx) == PARSER_STATUS_SYNTAX_ERROR &&
        ctx->ast_root != NULL && ctx->error_count > 0;
    parser_destroy(ctx);
    return ok;
}

static int test_parser_rejects_excessive_operator_chain(void) {
    enum { OPERATORS = 100 };
    char source[OPERATORS * 2 + 32];
    size_t offset = 0u;
    ParserContext* ctx;
    int result;
    int ok;

    memcpy(source + offset, "v:x = 1", 7u);
    offset += 7u;
    for (int i = 0; i < OPERATORS; ++i) {
        source[offset++] = '+';
        source[offset++] = '1';
    }
    source[offset++] = ';';

    ctx = parser_init_source("deep-operator-chain.nxf",
        (const uint8_t*)source, offset);
    if (!ctx) return 0;
    result = parser_run(ctx);
    ok = result == PARSER_STATUS_SYNTAX_ERROR &&
        parser_get_status(ctx) == PARSER_STATUS_SYNTAX_ERROR &&
        ctx->ast_root != NULL && ctx->error_count > 0;
    parser_destroy(ctx);
    return ok;
}

static int parser_source_reports_syntax_error(
    const char* filename, const char* source, size_t source_length
) {
    ParserContext* ctx = parser_init_source(
        filename, (const uint8_t*)source, source_length);
    int result;
    int ok;
    if (!ctx) return 0;
    result = parser_run(ctx);
    ok = result == PARSER_STATUS_SYNTAX_ERROR &&
        parser_get_status(ctx) == PARSER_STATUS_SYNTAX_ERROR &&
        ctx->ast_root != NULL && ctx->error_count > 0;
    parser_destroy(ctx);
    return ok;
}

static int test_parser_bounds_recursive_expression_forms(void) {
    enum { DEPTH = 300 };
    char unary_source[DEPTH + 32];
    char assignment_source[DEPTH * 2 + 32];
    char postfix_source[DEPTH * 2 + 32];
    char block_source[DEPTH * 2 + 32];
    size_t unary_length = 0u;
    size_t assignment_length = 0u;
    size_t postfix_length = 0u;
    size_t block_length = 0u;

    memcpy(unary_source + unary_length, "v:x = ", 6u);
    unary_length += 6u;
    for (int i = 0; i < DEPTH; ++i) unary_source[unary_length++] = '!';
    unary_source[unary_length++] = '1';
    unary_source[unary_length++] = ';';

    memcpy(assignment_source + assignment_length, "v:x = 1", 7u);
    assignment_length += 7u;
    for (int i = 0; i < DEPTH; ++i) {
        assignment_source[assignment_length++] = '=';
        assignment_source[assignment_length++] = '1';
    }
    assignment_source[assignment_length++] = ';';

    memcpy(postfix_source + postfix_length, "v:x = a", 7u);
    postfix_length += 7u;
    for (int i = 0; i < DEPTH; ++i) {
        postfix_source[postfix_length++] = '.';
        postfix_source[postfix_length++] = 'b';
    }
    postfix_source[postfix_length++] = ';';

    memcpy(block_source + block_length, "f:main() {", 10u);
    block_length += 10u;
    for (int i = 0; i < DEPTH; ++i) block_source[block_length++] = '{';
    block_source[block_length++] = ';';
    for (int i = 0; i < DEPTH + 1; ++i) block_source[block_length++] = '}';

    return parser_source_reports_syntax_error(
            "deep-unary.nxf", unary_source, unary_length) &&
        parser_source_reports_syntax_error(
            "deep-assignment.nxf", assignment_source, assignment_length) &&
        parser_source_reports_syntax_error(
            "deep-postfix.nxf", postfix_source, postfix_length) &&
        parser_source_reports_syntax_error(
            "deep-blocks.nxf", block_source, block_length);
}

static int test_parser_context_initialization(void) {
    static const uint8_t empty_source[] = "";
    ParserContext* memory = parser_init_source("empty.nxf", empty_source, 0u);
    ParserContext* missing = parser_init(NULL);
    ParserContext* status_ctx = (ParserContext*)1;
    ParserStatus invalid_status = parser_init_source_ex("bad.nxf", NULL, 1u, &status_ctx);
    ParserStatus io_status = parser_init_ex("__omni_core_missing_parser_input__.nxf", &status_ctx);
    ParserStatus argument_status = parser_init_ex(NULL, &status_ctx);
    int ok = memory != NULL && missing == NULL &&
        invalid_status == PARSER_STATUS_INVALID_ARGUMENT && status_ctx == NULL &&
        io_status == PARSER_STATUS_IO_ERROR && status_ctx == NULL &&
        argument_status == PARSER_STATUS_INVALID_ARGUMENT && status_ctx == NULL;
    if (memory) {
        ok = ok && memory->source_buffer != NULL &&
            memory->source_len == 0u && memory->pos == 0u &&
            memory->current_line == 1 && memory->current_col == 1 &&
            memory->token_start_offset == 0u &&
            memory->token_start_line == 1 && memory->token_start_col == 1 &&
            memory->ast_root == NULL && memory->error_count == 0 &&
            memory->error_head == NULL && memory->error_tail == NULL &&
            memory->file_handle == NULL && memory->token_stream == NULL &&
            memory->token_stream_count == 0u && memory->token_stream_index == 0u &&
            memory->status == PARSER_STATUS_OK;
        parser_destroy(memory); /* Destroying a context before parser_run is valid. */
    }
    return ok;
}

static int test_aggregate_declaration_ast(void) {
    static const char source[] =
        "f:main() {"
        " struct: Point { v:x!i32, v:y!i32 }"
        " nt:id = u64;"
        " enum:State { Idle, Running(pid!u32), Error(code!u16) }"
        " union:Register32 { v:full!u32, v:parts!u16 }"
        "}";
    ParserContext* ctx = parser_init_source(
        "aggregate-declarations.nxf", (const uint8_t*)source, sizeof(source) - 1u);
    ASTProgram* program;
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTNode* node;
    int seen_struct = 0, seen_newtype = 0, seen_enum = 0, seen_union = 0;
    int ok = 0;
    if (!ctx) return 0;
    if (parser_run(ctx) != 0 || ctx->error_count != 0 || !ctx->ast_root) goto done;
    program = (ASTProgram*)ctx->ast_root;
    function = (ASTFuncDecl*)program->declarations;
    if (!function || function->base.type != AST_FUNC_DECL) goto done;
    body = (ASTBlock*)function->body;
    if (!body || body->base.type != AST_BLOCK) goto done;
    for (node = body->statements; node; node = node->next) {
        if (node->type == AST_STRUCT_DECL) {
            ASTStructDecl* d = (ASTStructDecl*)node;
            if (!d->name || !d->fields || !d->fields->next) goto done;
            seen_struct = 1;
        } else if (node->type == AST_NEWTYPE_DECL) {
            ASTNewTypeDecl* d = (ASTNewTypeDecl*)node;
            if (!d->name || !d->target_type) goto done;
            seen_newtype = 1;
        } else if (node->type == AST_ENUM_DECL) {
            ASTEnumDecl* d = (ASTEnumDecl*)node;
            ASTEnumVariant* v;
            if (!d->name || !d->variants) goto done;
            v = (ASTEnumVariant*)d->variants;
            if (!v->base.next || !((ASTEnumVariant*)v->base.next)->data) goto done;
            seen_enum = 1;
        } else if (node->type == AST_UNION_DECL) {
            ASTStructDecl* d = (ASTStructDecl*)node;
            if (!d->name || !d->fields) goto done;
            seen_union = 1;
        }
    }
    ok = seen_struct && seen_newtype && seen_enum && seen_union;
done:
    parser_destroy(ctx);
    return ok;
}

static int test_rejects_invalid_aggregate_members(void) {
    static const char* sources[] = {
        "f:main() { struct:Broken { ; } }",
        "f:main() { enum:Broken { ; } }",
        "f:main() { group Broken { ; } }"
    };
    size_t i;
    for (i = 0; i < sizeof(sources) / sizeof(sources[0]); ++i) {
        ParserContext* ctx = parser_init_source("invalid-aggregate-member.nxf",
            (const uint8_t*)sources[i], strlen(sources[i]));
        int rejected;
        if (!ctx) return 0;
        rejected = parser_run(ctx) == PARSER_STATUS_SYNTAX_ERROR &&
            parser_get_status(ctx) == PARSER_STATUS_SYNTAX_ERROR &&
            ctx->error_count > 0 && ctx->ast_root != NULL;
        parser_destroy(ctx);
        if (!rejected) return 0;
    }
    return 1;
}

static int test_parser_panic_mode_recovers_at_statement_boundaries(void) {
    static const char source[] =
        "f:main() {"
        " v:first = ;"
        " v:second = ;"
        " v:good = 3;"
        "}";
    ParserContext* ctx = parser_init_source(
        "parser-panic-recovery.nxf", (const uint8_t*)source, sizeof(source) - 1u);
    OccDiagnosticCollector diagnostics;
    ASTProgram* program;
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTNode* statement;
    size_t diagnostic_count;
    int found_good = 0;
    int ok = 0;

    if (!ctx) return 0;
    occ_diagnostics_init(&diagnostics);
    parser_attach_diagnostics(ctx, &diagnostics, OCC_STAGE_PARSER);
    if (parser_run(ctx) != PARSER_STATUS_SYNTAX_ERROR || !ctx->ast_root) goto done;

    diagnostic_count = occ_diagnostics_count(&diagnostics);
    if (diagnostic_count < 2u) goto done;

    program = (ASTProgram*)ctx->ast_root;
    function = (ASTFuncDecl*)program->declarations;
    if (!function || function->base.type != AST_FUNC_DECL) goto done;
    body = (ASTBlock*)function->body;
    if (!body || body->base.type != AST_BLOCK) goto done;

    for (statement = body->statements; statement; statement = statement->next) {
        if (statement->type == AST_VAR_DECL) {
            ASTVarDecl* variable = (ASTVarDecl*)statement;
            if (variable->name && strcmp(variable->name, "good") == 0 &&
                variable->initializer && variable->initializer->type == AST_LITERAL) {
                found_good = 1;
            }
        }
    }
    ok = found_good;

done:
    parser_destroy(ctx);
    occ_diagnostics_destroy(&diagnostics);
    return ok;
}

static int test_parser_nested_block_recovery(void) {
    static const char source[] =
        "f:main() {"
        " { v:broken = ; v:inside_good = 4; }"
        " v:after = 5;"
        "}";
    ParserContext* ctx = parser_init_source(
        "parser-nested-recovery.nxf", (const uint8_t*)source, sizeof(source) - 1u);
    ASTProgram* program;
    ASTFuncDecl* function;
    ASTBlock* outer;
    ASTBlock* nested;
    ASTNode* statement;
    int found_inside = 0;
    int found_after = 0;
    int ok = 0;

    if (!ctx) return 0;
    if (parser_run(ctx) != PARSER_STATUS_SYNTAX_ERROR || !ctx->ast_root) goto done;

    program = (ASTProgram*)ctx->ast_root;
    function = (ASTFuncDecl*)program->declarations;
    if (!function || function->base.type != AST_FUNC_DECL ||
        !function->body || function->body->type != AST_BLOCK) goto done;
    outer = (ASTBlock*)function->body;

    for (statement = outer->statements; statement; statement = statement->next) {
        if (statement->type == AST_BLOCK) {
            ASTNode* inner_statement;
            nested = (ASTBlock*)statement;
            for (inner_statement = nested->statements; inner_statement;
                 inner_statement = inner_statement->next) {
                if (inner_statement->type == AST_VAR_DECL) {
                    ASTVarDecl* variable = (ASTVarDecl*)inner_statement;
                    if (variable->name && strcmp(variable->name, "inside_good") == 0 &&
                        variable->initializer &&
                        variable->initializer->type == AST_LITERAL) {
                        found_inside = 1;
                    }
                }
            }
        } else if (statement->type == AST_VAR_DECL) {
            ASTVarDecl* variable = (ASTVarDecl*)statement;
            if (variable->name && strcmp(variable->name, "after") == 0 &&
                variable->initializer && variable->initializer->type == AST_LITERAL) {
                found_after = 1;
            }
        }
    }
    ok = found_inside && found_after;

done:
    parser_destroy(ctx);
    return ok;
}

static int test_parser_malformed_function_recovery(void) {
    static const char source[] =
        "f:broken(, value) { v:discarded = 1; }"
        "f:missing( f:good() { v:kept = 2; }";
    ParserContext* ctx = parser_init_source(
        "parser-malformed-function-recovery.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTProgram* program;
    ASTNode* declaration;
    int found_good = 0;
    int ok = 0;

    if (!ctx) return 0;
    if (parser_run(ctx) != PARSER_STATUS_SYNTAX_ERROR || !ctx->ast_root) goto done;

    program = (ASTProgram*)ctx->ast_root;
    for (declaration = program->declarations; declaration;
         declaration = declaration->next) {
        if (declaration->type == AST_FUNC_DECL) {
            ASTFuncDecl* function = (ASTFuncDecl*)declaration;
            if (function->name && strcmp(function->name, "good") == 0 &&
                function->body && function->body->type == AST_BLOCK) {
                ASTNode* statement;
                for (statement = ((ASTBlock*)function->body)->statements;
                     statement; statement = statement->next) {
                    if (statement->type == AST_VAR_DECL) {
                        ASTVarDecl* variable = (ASTVarDecl*)statement;
                        if (variable->name && strcmp(variable->name, "kept") == 0 &&
                            variable->initializer &&
                            variable->initializer->type == AST_LITERAL) {
                            found_good = 1;
                        }
                    }
                }
            }
        }
    }
    ok = found_good;

done:
    parser_destroy(ctx);
    return ok;
}

static int test_parser_preserves_lexer_errors_during_recovery(void) {
    static const char source[] =
        "f:main() { v:broken = ; $ v:good = 3; }";
    ParserContext* ctx = parser_init_source(
        "parser-recovery-lexer-error.nxf", (const uint8_t*)source, sizeof(source) - 1u);
    OccDiagnosticCollector diagnostics;
    OccDiagnosticView diagnostic;
    ASTProgram* program;
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTNode* statement;
    int found_good = 0;
    int found_lexer_error = 0;
    int ok = 0;

    if (!ctx) return 0;
    occ_diagnostics_init(&diagnostics);
    parser_attach_diagnostics(ctx, &diagnostics, OCC_STAGE_PARSER);
    if (parser_run(ctx) != PARSER_STATUS_SYNTAX_ERROR || !ctx->ast_root) goto done;

    {
        size_t diagnostic_index;
        for (diagnostic_index = 0u;
             diagnostic_index < occ_diagnostics_count(&diagnostics);
             ++diagnostic_index) {
            if (occ_diagnostics_get(&diagnostics, diagnostic_index, &diagnostic) &&
                diagnostic.record->stage_id == OCC_STAGE_LEXER) {
                found_lexer_error = 1;
                break;
            }
        }
    }
    program = (ASTProgram*)ctx->ast_root;
    function = (ASTFuncDecl*)program->declarations;
    if (!function || !function->body || function->body->type != AST_BLOCK) goto done;
    body = (ASTBlock*)function->body;
    for (statement = body->statements; statement; statement = statement->next) {
        if (statement->type == AST_VAR_DECL) {
            ASTVarDecl* variable = (ASTVarDecl*)statement;
            if (variable->name && strcmp(variable->name, "good") == 0 &&
                variable->initializer && variable->initializer->type == AST_LITERAL) {
                found_good = 1;
            }
        }
    }
    ok = found_lexer_error && found_good;

done:
    parser_destroy(ctx);
    occ_diagnostics_destroy(&diagnostics);
    return ok;
}

static int test_parser_allocation_failure_injection(void) {
    static const char source[] =
        "f:main(a!i32, b!i64) {"
        " v:first = a + b;"
        " { v:inner = combine(first, 7); }"
        " v:pair = (first, b);"
        " struct: Point { v:x!i32, v:y!i32 }"
        " nt:id = u64;"
        " enum:State { Idle, Running(pid!u32), Error(code!u16) }"
        " union:Register32 { v:full!u32, v:parts!u16 }"
        " v:casted = cast(1, i32);"
        " panic(\"boom\");"
        " ?? \"inspect\";"
        " ret;"
        "}";
    size_t fail_at;
    size_t oom_count = 0u;
    int reached_success = 0;
    for (fail_at = 1u; fail_at <= 256u; ++fail_at) {
        ParserContext* ctx = parser_init_source(
            "parser-oom-injection.nxf", (const uint8_t*)source, sizeof(source) - 1u);
        int result;
        ParserStatus status;
        if (!ctx) return 0;
        parser_test_fail_allocation_at(ctx, fail_at);
        result = parser_run(ctx);
        status = parser_get_status(ctx);
        if (status == PARSER_STATUS_OUT_OF_MEMORY &&
            result == PARSER_STATUS_OUT_OF_MEMORY &&
            ctx->test_allocation_count >= fail_at) {
            ++oom_count;
        } else if (status == PARSER_STATUS_OK && result == PARSER_STATUS_OK &&
                   ctx->test_allocation_count < fail_at) {
            reached_success = 1;
            parser_destroy(ctx);
            break;
        } else {
            fprintf(stderr,
                "OOM injection mismatch at allocation %zu (observed %zu, status %d, result %d).\\n",
                fail_at, ctx->test_allocation_count, (int)status, result);
            parser_destroy(ctx);
            return 0;
        }
        parser_destroy(ctx);
    }
    return oom_count > 0u && reached_success;
}

static int test_function_parameter_type_separator_forms(void) {
    static const char source[] =
        "f:forms(a:f32, b!f32, d:!f32) { ret 1; }";
    ParserContext* ctx = parser_init_source(
        "function-parameter-type-forms.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTProgram* program;
    ASTFuncDecl* function;
    ASTVarDecl* a;
    ASTVarDecl* b;
    ASTVarDecl* c;
    int ok = 0;

    if (!ctx) return 0;
    if (parser_run(ctx) != PARSER_STATUS_OK ||
        parser_get_status(ctx) != PARSER_STATUS_OK ||
        ctx->error_count != 0 || !ctx->ast_root) goto done;

    program = (ASTProgram*)ctx->ast_root;
    function = (ASTFuncDecl*)program->declarations;
    if (!function || function->base.type != AST_FUNC_DECL ||
        !function->params || function->params->type != AST_VAR_DECL) goto done;
    a = (ASTVarDecl*)function->params;
    b = (ASTVarDecl*)a->base.next;
    c = b ? (ASTVarDecl*)b->base.next : NULL;
    if (!b || !c || c->base.next ||
        strcmp(a->name, "a") != 0 || strcmp(a->type_name, "f32") != 0 ||
        a->is_hard_typed ||
        strcmp(b->name, "b") != 0 || strcmp(b->type_name, "!f32") != 0 ||
        !b->is_hard_typed ||
        strcmp(c->name, "d") != 0 || strcmp(c->type_name, "!f32") != 0 ||
        !c->is_hard_typed) goto done;
    ok = 1;

done:
    parser_destroy(ctx);
    if (ok) {
        static const char invalid_source[] = "f:bad(a f32) { ret 1; }";
        ParserContext* invalid = parser_init_source(
            "function-parameter-missing-separator.nxf",
            (const uint8_t*)invalid_source, sizeof(invalid_source) - 1u);
        if (!invalid) return 0;
        ok = parser_run(invalid) == PARSER_STATUS_SYNTAX_ERROR &&
            parser_get_status(invalid) == PARSER_STATUS_SYNTAX_ERROR &&
            invalid->error_count > 0;
        parser_destroy(invalid);
    }
    return ok;
}

static int test_structured_if_elsif_els_ast(void) {
    static const char source[] =
        "f:main() {"
        " if (a == b) { v:first = 1; }"
        " elsif (a >= 1) v:second = 2;"
        " els { v:third = 3; }"
        " if (a != b) v:fourth = 4;"
        " els v:fifth = 5;"
        "}";
    ParserContext* ctx = parser_init_source(
        "structured-if.nxf", (const uint8_t*)source, sizeof(source) - 1u);
    ASTProgram* program;
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTIf* first;
    ASTIf* second;
    ASTIf* simple;
    int ok = 0;

    if (!ctx) return 0;
    if (parser_run(ctx) != PARSER_STATUS_OK ||
        parser_get_status(ctx) != PARSER_STATUS_OK ||
        ctx->error_count != 0 || !ctx->ast_root) goto done;

    program = (ASTProgram*)ctx->ast_root;
    function = (ASTFuncDecl*)program->declarations;
    if (!function || !function->body || function->body->type != AST_BLOCK) goto done;
    body = (ASTBlock*)function->body;
    first = (ASTIf*)body->statements;
    if (!first || first->base.type != AST_IF || !first->condition ||
        !first->then_branch || !first->else_branch ||
        first->then_branch->type != AST_BLOCK) goto done;
    second = (ASTIf*)first->else_branch;
    if (second->base.type != AST_IF || !second->condition ||
        !second->then_branch || !second->else_branch ||
        second->then_branch->type == AST_BLOCK ||
        second->else_branch->type != AST_BLOCK) goto done;
    simple = (ASTIf*)first->base.next;
    if (!simple || simple->base.type != AST_IF ||
        !simple->then_branch || !simple->else_branch ||
        simple->then_branch->type == AST_BLOCK ||
        simple->else_branch->type == AST_BLOCK) goto done;
    ok = 1;

done:
    parser_destroy(ctx);
    return ok;
}

static int test_conditional_action_arrow_disambiguation(void) {
    static const char source[] =
        "f:main() {"
        " (a == b) -> break;"
        " ((c != d)) -> continue;"
        " (e == f) -> ret 1;"
        " a == b -> target;"
        "}";
    ParserContext* ctx = parser_init_source(
        "conditional-action-arrow.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTProgram* program;
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTIf* break_action;
    ASTIf* continue_action;
    ASTIf* return_action;
    ASTNexusFlow* ordinary_pipe;
    int ok = 0;

    if (!ctx) return 0;
    if (parser_run(ctx) != PARSER_STATUS_OK ||
        parser_get_status(ctx) != PARSER_STATUS_OK ||
        ctx->error_count != 0 || !ctx->ast_root) goto done;

    program = (ASTProgram*)ctx->ast_root;
    function = (ASTFuncDecl*)program->declarations;
    if (!function || !function->body || function->body->type != AST_BLOCK) goto done;
    body = (ASTBlock*)function->body;

    break_action = (ASTIf*)body->statements;
    if (!break_action || break_action->base.type != AST_IF ||
        !break_action->condition || !break_action->then_branch ||
        break_action->then_branch->type != AST_BREAK ||
        break_action->else_branch != NULL) goto done;

    continue_action = (ASTIf*)break_action->base.next;
    if (!continue_action || continue_action->base.type != AST_IF ||
        !continue_action->condition || !continue_action->then_branch ||
        continue_action->then_branch->type != AST_CONTINUE ||
        continue_action->else_branch != NULL) goto done;

    return_action = (ASTIf*)continue_action->base.next;
    if (!return_action || return_action->base.type != AST_IF ||
        !return_action->condition || !return_action->then_branch ||
        return_action->then_branch->type != AST_RETURN ||
        return_action->else_branch != NULL) goto done;

    ordinary_pipe = (ASTNexusFlow*)return_action->base.next;
    if (!ordinary_pipe || ordinary_pipe->base.type != AST_NEXUS_PIPE ||
        ordinary_pipe->op != TOKEN_PIPE || !ordinary_pipe->left ||
        !ordinary_pipe->right || ordinary_pipe->right->type != AST_IDENTIFIER) goto done;
    ok = 1;

done:
    parser_destroy(ctx);
    return ok;
}

static int test_select_scan_ast(void) {
    static const char source[] =
        "f:main() { v:x = 1;"
        " select(x) { 1 -> { v:a = 2; } default -> { v:b = 3; } }"
        " scan(x) { Range(s,e) -> { v:c = s; } item -> { v:d = item; } }"
        "}";
    ParserContext* ctx=parser_init_source("select-scan.nxf",(const uint8_t*)source,sizeof(source)-1u);
    ASTProgram* program;
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTNode* node;
    FILE* stream;
    ASTNode* decoded;
    int ok=0;
    if(!ctx)return 0;
    if(parser_run(ctx)!=PARSER_STATUS_OK || ctx->error_count || !ctx->ast_root)goto done;
    program=(ASTProgram*)ctx->ast_root;
    function=(ASTFuncDecl*)program->declarations;
    if(!function || function->base.type!=AST_FUNC_DECL)goto done;
    body=(ASTBlock*)function->body;
    if(!body || body->base.type!=AST_BLOCK)goto done;
    node=body->statements;
    if(!node || node->type!=AST_VAR_DECL)goto done;
    node=node->next;
    if(!node || node->type!=AST_SELECT)goto done;
    {
        ASTSelect* select_node=(ASTSelect*)node;
        ASTSelectArm* first=(ASTSelectArm*)select_node->arms;
        ASTSelectArm* fallback=first?(ASTSelectArm*)first->base.next:NULL;
        if(!select_node->selector || !first || first->base.type!=AST_SELECT_ARM ||
            !first->pattern || first->pattern->type!=AST_LITERAL ||
            !fallback || fallback->base.type!=AST_SELECT_ARM || !fallback->is_default ||
            fallback->pattern!=NULL)goto done;
    }
    node=node->next;
    if(!node || node->type!=AST_SCAN)goto done;
    {
        ASTScan* scan_node=(ASTScan*)node;
        ASTScanArm* first=(ASTScanArm*)scan_node->arms;
        ASTPatternConstructor* ctor;
        if(!scan_node->subject || !first || first->base.type!=AST_SCAN_ARM ||
            !first->pattern || first->pattern->type!=AST_PATTERN_CONSTRUCTOR)goto done;
        ctor=(ASTPatternConstructor*)first->pattern;
        if(strcmp(ctor->name,"Range")!=0 || !ctor->bindings ||
            ctor->bindings->type!=AST_PATTERN_BINDING ||
            strcmp(((ASTPatternBinding*)ctor->bindings)->name,"s")!=0 ||
            !ctor->bindings->next ||
            strcmp(((ASTPatternBinding*)ctor->bindings->next)->name,"e")!=0)goto done;
    }
    stream=tmpfile();
    if(!stream)goto done;
    if(!ast_serialize((ASTNode*)ctx->ast_root,stream) || fseek(stream,0,SEEK_SET)!=0) {
        fclose(stream); goto done;
    }
    decoded=ast_deserialize(stream);
    fclose(stream);
    if(!decoded || decoded->type!=AST_PROGRAM) { ast_destroy(decoded); goto done; }
    ast_destroy(decoded);
    ok=1;
done:
    parser_destroy(ctx);
    if(!ok)return 0;
    if(!parser_source_reports_syntax_error("select-duplicate-default.nxf",
        "f:main() { select(x) { default -> { } default -> { } } }",
        sizeof("f:main() { select(x) { default -> { } default -> { } } }")-1u))return 0;
    if(!parser_source_reports_syntax_error("scan-empty-constructor.nxf",
        "f:main() { scan(x) { Range() -> { } } }",
        sizeof("f:main() { scan(x) { Range() -> { } } }")-1u))return 0;
    return 1;
}

static int test_rules_lambda_relocate_unroll_ast(void) {
    static const char source[] =
        "f:main() {"
        " rules PrimeBorders(n) {"
        "  n < 2 :=> \"out of range\";"
        "  n == 2 -> true;"
        "  always -> true;"
        "  default -> false;"
        " }"
        " apply rules(PrimeBorders(5));"
        " on PrimeBorders.Success { return true; }"
        " v:callback = f:(x) > x;"
        " unroll(3) -> loop(item in data) { echo(item); }"
        " v:moved = source _> target;"
        "}";
    ParserContext* ctx=parser_init_source(
        "rules-lambda-relocate-unroll.nxf",(const uint8_t*)source,sizeof(source)-1u);
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTNode* node;
    int rules=0, apply=0, on_block=0, lambda=0, unroll=0, relocate=0;
    int ok=0;
    if(!ctx)return 0;
    if(parser_run(ctx)!=PARSER_STATUS_OK || ctx->error_count!=0 || !ctx->ast_root) { fprintf(stderr,"feature parser status=%d errors=%d\\n",(int)parser_get_status(ctx),ctx->error_count); goto done; }
    function=(ASTFuncDecl*)((ASTProgram*)ctx->ast_root)->declarations;
    if(!function || function->base.type!=AST_FUNC_DECL ||
       !function->body || function->body->type!=AST_BLOCK)goto done;
    body=(ASTBlock*)function->body;
    for(node=body->statements;node;node=node->next) {
        if(node->type==AST_RULESET_DECL) {
            ASTRulesetDecl* r=(ASTRulesetDecl*)node;
            ASTNode* rule=r->rules;
            int barriers=0,triggers=0,always=0,defaults=0;
            if(!r->name || strcmp(r->name,"PrimeBorders")!=0 || !r->params)goto done;
            for(;rule;rule=rule->next) {
                if(rule->type==AST_RULE_BARRIER)barriers++;
                else if(rule->type==AST_RULE_TRIGGER)triggers++;
                else if(rule->type==AST_RULE_ALWAYS)always++;
                else if(rule->type==AST_RULE_DEFAULT)defaults++;
            }
            if(barriers!=1 || triggers!=1 || always!=1 || defaults!=1)goto done;
            rules=1;
        } else if(node->type==AST_APPLY_RULES) {
            ASTApplyRules* a=(ASTApplyRules*)node;
            if(!a->ruleset_name || strcmp(a->ruleset_name,"PrimeBorders")!=0 || !a->args)goto done;
            apply=1;
        } else if(node->type==AST_ON_BLOCK) {
            ASTOnBlock* o=(ASTOnBlock*)node;
            if(!o->ruleset_name || !o->event_name || strcmp(o->event_name,"Success")!=0 || !o->body)goto done;
            on_block=1;
        } else if(node->type==AST_VAR_DECL) {
            ASTVarDecl* v=(ASTVarDecl*)node;
            if(v->name && strcmp(v->name,"callback")==0 && v->initializer && v->initializer->type==AST_LAMBDA)lambda=1;
            if(v->name && strcmp(v->name,"moved")==0 && v->initializer && v->initializer->type==AST_RELOCATE)relocate=1;
        } else if(node->type==AST_UNROLL) {
            ASTUnroll* u=(ASTUnroll*)node;
            if(!u->count || !u->loop || u->loop->type!=AST_LOOP)goto done;
            unroll=1;
        }
    }
    ok=rules && apply && on_block && lambda && unroll && relocate;
    if(!ok)fprintf(stderr,"feature AST flags rules=%d apply=%d on=%d lambda=%d unroll=%d relocate=%d\\n",rules,apply,on_block,lambda,unroll,relocate);
done:
    parser_destroy(ctx);
    return ok;
}

int main(int argc, char** argv) {
    if (!test_rules_lambda_relocate_unroll_ast()) {
        fprintf(stderr, "Rules/lambda/relocate/unroll AST parser test failed.\\n");
        return 1;
    }
    if (!test_select_scan_ast()) { fprintf(stderr, "select/scan AST parser and codec test failed.\\n"); return 1; }
    if (!test_structured_if_elsif_els_ast()) {
        fprintf(stderr, "Structured if/elsif/els parser test failed.\\n");
        return 1;
    }
    if (!test_conditional_action_arrow_disambiguation()) {
        fprintf(stderr, "Conditional action arrow was confused with a Nexus pipe.\\n");
        return 1;
    }
    if (!test_parser_rejects_excessive_expression_nesting()) {
        fprintf(stderr, "Parser did not safely reject excessive expression nesting.\\n");
        return 1;
    }
    if (!test_parser_rejects_excessive_operator_chain()) {
        fprintf(stderr, "Parser did not safely reject an excessive operator chain.\\n");
        return 1;
    }
    if (!test_parser_bounds_recursive_expression_forms()) {
        fprintf(stderr, "Parser did not bound recursive unary/assignment/postfix forms.\\n");
        return 1;
    }
    if (!test_parser_context_initialization()) {
        fprintf(stderr, "Parser context initialization/early destruction test failed.\\n");
        return 1;
    }
    if (!test_function_parameter_type_separator_forms()) {
        fprintf(stderr, "Function parameter type separator forms were not parsed consistently.\\n");
        return 1;
    }
    if (!test_cast_and_panic_ast()) {
        fprintf(stderr, "CAST/PANIC AST payload test failed.\n");
        return 1;
    }
    if (!test_aggregate_declaration_ast()) {
        fprintf(stderr, "Aggregate declaration AST ownership test failed.\n");
        return 1;
    }
    if (!test_rejects_invalid_aggregate_members()) {
        fprintf(stderr, "Invalid aggregate members were silently accepted.\n");
        return 1;
    }
    if (!test_parser_panic_mode_recovers_at_statement_boundaries()) {
        fprintf(stderr, "Parser panic-mode recovery did not preserve later valid statements.\n");
        return 1;
    }
    if (!test_parser_nested_block_recovery()) {
        fprintf(stderr, "Parser did not recover inside a nested block and preserve following statements.\\n");
        return 1;
    }
    if (!test_parser_malformed_function_recovery()) {
        fprintf(stderr, "Parser did not recover after a malformed function declaration.\\n");
        return 1;
    }
    if (!test_parser_preserves_lexer_errors_during_recovery()) {
        fprintf(stderr, "Lexer diagnostics were lost during parser panic-mode recovery.\\n");
        return 1;
    }
    if (!test_parser_allocation_failure_injection()) {
        fprintf(stderr, "Parser allocation failure injection did not preserve OOM status/cleanup.\n");
        return 1;
    }
    if (argc != 4) return 2;

    OccDiagnosticCollector diagnostics;
    occ_diagnostics_init(&diagnostics);

    ParserContext* valid = parser_init(argv[1]);
    if (!valid) return 1;
    parser_attach_diagnostics(valid, &diagnostics, OCC_STAGE_PARSER);
    int valid_result = parser_run(valid);
    int valid_ok = valid_result == 0 && valid->ast_root != NULL &&
        ((ASTNode*)valid->ast_root)->type == AST_PROGRAM;
    parser_destroy(valid);
    if (!valid_ok) {
        fprintf(stderr, "Parser rejected a valid source or failed to produce a program AST.\n");
        return 1;
    }

    if (!write_invalid_source(argv[2])) return 1;
    ParserContext* invalid = parser_init(argv[2]);
    if (!invalid) return 1;
    parser_attach_diagnostics(invalid, &diagnostics, OCC_STAGE_PARSER);
    int invalid_result = parser_run(invalid);
    OccDiagnosticView parser_diagnostic;
    int invalid_ok = invalid_result != 0 && invalid->error_count > 0 &&
        invalid->ast_root != NULL && occ_diagnostics_count(&diagnostics) > 0 &&
        occ_diagnostics_count_severity(&diagnostics, OCC_DIAG_ERROR) > 0 &&
        occ_diagnostics_get(&diagnostics, 0, &parser_diagnostic) &&
        parser_diagnostic.record->span.has_location &&
        parser_diagnostic.record->span.start_offset == 6u &&
        parser_diagnostic.record->span.end_offset == 7u;
    parser_destroy(invalid);
    occ_diagnostics_destroy(&diagnostics);
    if (!invalid_ok) {
        fprintf(stderr, "Parser did not report invalid syntax or retain its partial AST for cleanup.\n");
        return 1;
    }

    {
        OccDiagnosticCollector line_diagnostics;
        OccDiagnosticView diagnostic;
        ParserContext* second_line;
        int line_ok;
        occ_diagnostics_init(&line_diagnostics);
        if (!write_invalid_second_line_source(argv[2])) return 1;
        second_line = parser_init(argv[2]);
        if (!second_line) return 1;
        parser_attach_diagnostics(second_line, &line_diagnostics, OCC_STAGE_PARSER);
        (void)parser_run(second_line);
        line_ok = occ_diagnostics_get(&line_diagnostics, 0, &diagnostic) &&
            diagnostic.record->span.has_location &&
            diagnostic.record->span.start_offset == 15u &&
            diagnostic.record->span.end_offset == 16u &&
            diagnostic.record->span.start_line == 2u &&
            diagnostic.record->span.start_column == 7u &&
            diagnostic.record->span.end_line == 2u &&
            diagnostic.record->span.end_column == 8u;
        parser_destroy(second_line);
        occ_diagnostics_destroy(&line_diagnostics);
        if (!line_ok) {
            fprintf(stderr, "Parser diagnostic did not preserve the exact second-line token span.\n");
            return 1;
        }
    }

    {
        char memory_source[] = "v:x = ;\n";
        OccDiagnosticCollector memory_diagnostics;
        OccDiagnosticView diagnostic;
        ParserContext* memory_parser;
        int memory_ok;
        occ_diagnostics_init(&memory_diagnostics);
        memory_parser = parser_init_source("memory-test.nxf",
            (const uint8_t*)memory_source, sizeof(memory_source) - 1u);
        if (!memory_parser) return 1;
        memory_source[6] = '1'; /* The parser must retain its own source copy. */
        parser_attach_diagnostics(memory_parser, &memory_diagnostics, OCC_STAGE_PARSER);
        (void)parser_run(memory_parser);
        memory_ok = occ_diagnostics_get(&memory_diagnostics, 0, &diagnostic) &&
            diagnostic.record->span.has_location &&
            diagnostic.record->span.start_offset == 6u &&
            diagnostic.record->span.end_offset == 7u &&
            diagnostic.record->span.start_line == 1u &&
            diagnostic.record->span.start_column == 7u;
        parser_destroy(memory_parser);
        occ_diagnostics_destroy(&memory_diagnostics);
        if (!memory_ok) {
            fprintf(stderr, "In-memory parser did not retain source bytes and exact diagnostic location.\n");
            return 1;
        }
    }

    if (!write_invalid_lexical_source(argv[3])) return 1;
    {
        OccDiagnosticCollector lexical_diagnostics;
        OccDiagnosticView diagnostic;
        ParserContext* lexical = parser_init(argv[3]);
        if (!lexical) return 1;
        occ_diagnostics_init(&lexical_diagnostics);
        parser_attach_diagnostics(lexical, &lexical_diagnostics, OCC_STAGE_PARSER);
        (void)parser_run(lexical);
        int lexical_ok = occ_diagnostics_count(&lexical_diagnostics) > 0 &&
            occ_diagnostics_get(&lexical_diagnostics, 0, &diagnostic) &&
            diagnostic.record->stage_id == OCC_STAGE_LEXER &&
            diagnostic.record->span.has_location;
        parser_destroy(lexical);
        occ_diagnostics_destroy(&lexical_diagnostics);
        if (!lexical_ok) {
            fprintf(stderr, "Lexer error was not forwarded with its stage and source location.\n");
            return 1;
        }
    }

    {
        static const uint8_t invalid_lexical_source[] = "$";
        OccDiagnosticCollector failed_diagnostics;
        ParserContext* failed_lexer;
        int failed_result;
        occ_diagnostics_init(&failed_diagnostics);
        failed_diagnostics.allocation_failed = 1;
        failed_lexer = parser_init_source("diagnostic-oom.nxf",
            invalid_lexical_source, sizeof(invalid_lexical_source) - 1u);
        if (!failed_lexer) {
            occ_diagnostics_destroy(&failed_diagnostics);
            return 1;
        }
        parser_attach_diagnostics(failed_lexer, &failed_diagnostics, OCC_STAGE_PARSER);
        failed_result = parser_run(failed_lexer);
        int failed_ok = failed_result == PARSER_STATUS_OUT_OF_MEMORY &&
            parser_get_status(failed_lexer) == PARSER_STATUS_OUT_OF_MEMORY &&
            occ_diagnostics_count(&failed_diagnostics) == 0u;
        parser_destroy(failed_lexer);
        occ_diagnostics_destroy(&failed_diagnostics);
        if (!failed_ok) {
            fprintf(stderr, "Lexer diagnostic allocation failure was not propagated as OOM.\\n");
            return 1;
        }
    }

    {
        static const uint8_t token_source[] = "$";
        OccDiagnosticCollector token_diagnostics;
        ParserContext* token_parser = parser_init_source("token-stream.nxf",
            token_source, sizeof(token_source) - 1u);
        Token tokens[2];
        Token invalid_tokens[2];
        int token_ok;
        if (!token_parser) return 1;
        tokens[0] = lexer_next_token(token_parser);
        tokens[1] = lexer_next_token(token_parser);
        if (tokens[0].type != TOKEN_ERROR || tokens[1].type != TOKEN_EOF) {
            parser_destroy(token_parser);
            return 1;
        }
        /* A stale/nonexistent upstream diagnostic index must not hide this error. */
        tokens[0].diagnostic_id = 1u;
        invalid_tokens[0] = tokens[0];
        invalid_tokens[1] = tokens[1];
        invalid_tokens[0].source_offset = (size_t)-1;
        occ_diagnostics_init(&token_diagnostics);
        parser_attach_diagnostics(token_parser, &token_diagnostics, OCC_STAGE_PARSER);
        if (parser_attach_token_stream(token_parser, invalid_tokens, 2u)) {
            parser_destroy(token_parser);
            occ_diagnostics_destroy(&token_diagnostics);
            fprintf(stderr, "Malformed token source span was accepted.\\n");
            return 1;
        }
        if (!parser_attach_token_stream(token_parser, tokens, 2u)) {
            parser_destroy(token_parser);
            occ_diagnostics_destroy(&token_diagnostics);
            return 1;
        }
        (void)parser_run(token_parser);
        token_ok = parser_get_status(token_parser) == PARSER_STATUS_SYNTAX_ERROR &&
            occ_diagnostics_count(&token_diagnostics) == 1u;
        parser_destroy(token_parser);
        occ_diagnostics_destroy(&token_diagnostics);
        if (!token_ok) {
            fprintf(stderr, "Invalid token diagnostic reference suppressed a lexer error.\\n");
            return 1;
        }
    }

    {
        static const uint8_t source_text[] = "x";
        ParserContext* text_parser = parser_init_source("token-text.nxf",
            source_text, sizeof(source_text) - 1u);
        Token tokens[2];
        int rejected;
        if (!text_parser) return 1;
        tokens[0] = lexer_next_token(text_parser);
        tokens[1] = lexer_next_token(text_parser);
        tokens[0].start = "y";
        rejected = !parser_attach_token_stream(text_parser, tokens, 2u);
        parser_destroy(text_parser);
        if (!rejected) {
            fprintf(stderr, "Token text that differs from its source span was accepted.\\n");
            return 1;
        }
    }


    {
        static const uint8_t invalid_source[] = "v:x = ;";
        OccDiagnosticCollector limited;
        ParserContext* limited_parser;
        int limited_result;
        occ_diagnostics_init(&limited);
        limited.limit_exceeded = 1;
        limited_parser = parser_init_source("diagnostic-limit.nxf",
            invalid_source, sizeof(invalid_source) - 1u);
        if (!limited_parser) {
            occ_diagnostics_destroy(&limited);
            return 1;
        }
        parser_attach_diagnostics(limited_parser, &limited, OCC_STAGE_PARSER);
        limited_result = parser_run(limited_parser);
        {
            int limit_ok = limited_result == PARSER_STATUS_DIAGNOSTIC_LIMIT &&
                parser_get_status(limited_parser) == PARSER_STATUS_DIAGNOSTIC_LIMIT &&
                occ_diagnostics_count(&limited) == 0u;
            parser_destroy(limited_parser);
            occ_diagnostics_destroy(&limited);
            if (!limit_ok) {
                fprintf(stderr, "Parser diagnostic budget exhaustion was not reported distinctly.\\n");
                return 1;
            }
        }
    }

    return 0;
}
