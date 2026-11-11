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
    const char* cast_start = strstr(source, "cast");
    const char* cast_end = cast_start ? strstr(cast_start, "i32)") : NULL;
    const char* panic_start = strstr(source, "panic");
    const char* panic_end = panic_start ? strchr(panic_start, ')') : NULL;
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
        strcmp(((ASTIdentifier*)cast->target_type)->name, "i32") != 0 ||
        !cast->base.source_span.has_location || !cast_start || !cast_end ||
        cast->base.source_span.start_offset != (uint64_t)(cast_start - source) ||
        cast->base.source_span.end_offset != (uint64_t)(cast_end + 4 - source)) goto done;
    panic = (ASTPanic*)variable->base.next;
    if (!panic || panic->base.type != AST_PANIC ||
        !panic->message || strcmp(panic->message, "\"boom\"") != 0 ||
        !panic_start || !panic_end || !panic->base.source_span.has_location ||
        panic->base.source_span.start_offset != (uint64_t)(panic_start - source) ||
        panic->base.source_span.end_offset != (uint64_t)(panic_end + 1 - source)) goto done;
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

static int test_legacy_unpack_operator_is_rejected(void) {
    static const char source[] =
        "f:main() { v:result = data ...> target; }";
    return parser_source_reports_syntax_error(
        "legacy-unpack-operator.nxf", source, sizeof(source) - 1u);
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
    const char* struct_start = strstr(source, "struct: Point");
    const char* field_x = strstr(source, "v:x!i32");
    const char* field_y = strstr(source, "v:y!i32");
    const char* newtype_start = strstr(source, "nt:id");
    const char* newtype_end = strstr(source, "u64;");
    const char* enum_start = strstr(source, "enum:State");
    const char* enum_idle = strstr(source, "Idle");
    const char* enum_running = strstr(source, "Running");
    const char* enum_data = strstr(source, "pid!u32");
    const char* union_start = strstr(source, "union:Register32");
    const char* union_field = strstr(source, "v:full!u32");
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
            const char* struct_close = struct_start ? strchr(struct_start, '}') : NULL;
            if (!d->name || !d->fields || !d->fields->next ||
                !d->base.source_span.has_location ||
                d->base.source_span.start_offset != (uint64_t)(struct_start - source) ||
                d->base.source_span.end_offset != (uint64_t)(struct_close + 1 - source) ||
                !d->fields->source_span.has_location ||
                d->fields->source_span.start_offset != (uint64_t)(field_x - source) ||
                d->fields->source_span.end_offset != (uint64_t)(field_x + 7 - source) ||
                !d->fields->next->source_span.has_location ||
                d->fields->next->source_span.start_offset != (uint64_t)(field_y - source) ||
                d->fields->next->source_span.end_offset != (uint64_t)(field_y + 7 - source)) {
                fprintf(stderr, "struct spans root=%llu..%llu expected=%llu..%llu field1=%llu..%llu expected=%llu..%llu field2=%llu..%llu expected=%llu..%llu\n",
                    (unsigned long long)d->base.source_span.start_offset,
                    (unsigned long long)d->base.source_span.end_offset,
                    (unsigned long long)(struct_start-source),
                    (unsigned long long)(struct_close+1-source),
                    (unsigned long long)d->fields->source_span.start_offset,
                    (unsigned long long)d->fields->source_span.end_offset,
                    (unsigned long long)(field_x-source),
                    (unsigned long long)(field_x+7-source),
                    (unsigned long long)d->fields->next->source_span.start_offset,
                    (unsigned long long)d->fields->next->source_span.end_offset,
                    (unsigned long long)(field_y-source),
                    (unsigned long long)(field_y+7-source));
                goto done;
            }
            seen_struct = 1;
        } else if (node->type == AST_NEWTYPE_DECL) {
            ASTNewTypeDecl* d = (ASTNewTypeDecl*)node;
            if (!d->name || !d->target_type ||
                !d->base.source_span.has_location ||
                d->base.source_span.start_offset != (uint64_t)(newtype_start - source) ||
                d->base.source_span.end_offset != (uint64_t)(newtype_end + 4 - source)) {
                fprintf(stderr, "newtype span=%llu..%llu expected=%llu..%llu\n",
                    (unsigned long long)d->base.source_span.start_offset,
                    (unsigned long long)d->base.source_span.end_offset,
                    (unsigned long long)(newtype_start-source),
                    (unsigned long long)(newtype_end+4-source));
                goto done;
            }
            seen_newtype = 1;
        } else if (node->type == AST_ENUM_DECL) {
            ASTEnumDecl* d = (ASTEnumDecl*)node;
            ASTEnumVariant* v;
            const char* enum_close = enum_start ? strchr(enum_start, '}') : NULL;
            if (!d->name || !d->variants || !d->base.source_span.has_location ||
                d->base.source_span.start_offset != (uint64_t)(enum_start - source) ||
                d->base.source_span.end_offset != (uint64_t)(enum_close + 1 - source)) {
                fprintf(stderr, "enum root span=%llu..%llu expected=%llu..%llu\n",
                    (unsigned long long)d->base.source_span.start_offset,
                    (unsigned long long)d->base.source_span.end_offset,
                    (unsigned long long)(enum_start-source),
                    (unsigned long long)(enum_close+1-source));
                goto done;
            }
            v = (ASTEnumVariant*)d->variants;
            if (!v->base.next || !((ASTEnumVariant*)v->base.next)->data ||
                !v->base.source_span.has_location ||
                v->base.source_span.start_offset != (uint64_t)(enum_idle - source) ||
                v->base.source_span.end_offset != (uint64_t)(enum_idle + 4 - source) ||
                !v->base.next->source_span.has_location ||
                v->base.next->source_span.start_offset != (uint64_t)(enum_running - source) ||
                v->base.next->source_span.end_offset != (uint64_t)(enum_data + 8 - source) ||
                !((ASTEnumVariant*)v->base.next)->data->source_span.has_location ||
                ((ASTEnumVariant*)v->base.next)->data->source_span.start_offset != (uint64_t)(enum_data - source) ||
                ((ASTEnumVariant*)v->base.next)->data->source_span.end_offset != (uint64_t)(enum_data + 7 - source)) {
                fprintf(stderr, "enum variants idle=%llu..%llu running=%llu..%llu data=%llu..%llu\n",
                    (unsigned long long)v->base.source_span.start_offset,
                    (unsigned long long)v->base.source_span.end_offset,
                    (unsigned long long)v->base.next->source_span.start_offset,
                    (unsigned long long)v->base.next->source_span.end_offset,
                    (unsigned long long)((ASTEnumVariant*)v->base.next)->data->source_span.start_offset,
                    (unsigned long long)((ASTEnumVariant*)v->base.next)->data->source_span.end_offset);
                goto done;
            }
            seen_enum = 1;
        } else if (node->type == AST_UNION_DECL) {
            ASTStructDecl* d = (ASTStructDecl*)node;
            if (!d->name || !d->fields || !d->base.source_span.has_location ||
                d->base.source_span.start_offset != (uint64_t)(union_start - source) ||
                !d->fields->source_span.has_location ||
                d->fields->source_span.start_offset != (uint64_t)(union_field - source) ||
                d->fields->source_span.end_offset != (uint64_t)(union_field + 10 - source)) {
                fprintf(stderr, "union span root=%llu..%llu field=%llu..%llu expected=%llu..%llu\n",
                    (unsigned long long)d->base.source_span.start_offset,
                    (unsigned long long)d->base.source_span.end_offset,
                    (unsigned long long)d->fields->source_span.start_offset,
                    (unsigned long long)d->fields->source_span.end_offset,
                    (unsigned long long)(union_field-source),
                    (unsigned long long)(union_field+10-source));
                goto done;
            }
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

static int test_nested_block_exact_source_spans(void) {
    static const char source[] =
        "f:main() {\n"
        "  {\n"
        "    v:x = 1;\n"
        "  }\n"
        "}";
    const char* outer_open=strchr(source,'{');
    const char* outer_close=strrchr(source,'}');
    const char* nested_open=outer_open?strchr(outer_open+1,'{'):NULL;
    const char* nested_close=nested_open?strchr(nested_open,'}'):NULL;
    ParserContext* parser=parser_init_source("nested-block-spans.nxf",
        (const uint8_t*)source,sizeof(source)-1u);
    ASTNode* decoded=NULL;
    FILE* stream=NULL;
    int valid=0;
    if(!parser) return 0;
    if(parser_run(parser)!=PARSER_STATUS_OK || parser->error_count!=0 ||
       !parser->ast_root || !ast_validate(parser->ast_root,NULL) ||
       !outer_open || !outer_close || !nested_open || !nested_close) goto done;
    {
        ASTFuncDecl* function=(ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
        ASTBlock* outer=function && function->body && function->body->type==AST_BLOCK
            ? (ASTBlock*)function->body:NULL;
        ASTBlock* nested=outer && outer->statements &&
            outer->statements->type==AST_BLOCK?(ASTBlock*)outer->statements:NULL;
        ASTVarDecl* value=nested && nested->statements &&
            nested->statements->type==AST_VAR_DECL?(ASTVarDecl*)nested->statements:NULL;
        valid=outer && nested && value && value->name &&
            strcmp(value->name,"x")==0 &&
            outer->base.source_span.has_location &&
            outer->base.source_span.start_offset==(uint64_t)(outer_open-source) &&
            outer->base.source_span.end_offset==(uint64_t)(outer_close+1-source) &&
            outer->base.source_span.start_line==1u &&
            outer->base.source_span.end_line==5u &&
            outer->base.source_span.start_column==10u &&
            outer->base.source_span.end_column==2u &&
            nested->base.source_span.has_location &&
            nested->base.source_span.start_offset==(uint64_t)(nested_open-source) &&
            nested->base.source_span.end_offset==(uint64_t)(nested_close+1-source) &&
            nested->base.source_span.start_line==2u &&
            nested->base.source_span.end_line==4u &&
            nested->base.source_span.start_column==3u &&
            nested->base.source_span.end_column==4u;
    }
    if(!valid) goto done;
    stream=tmpfile();
    if(!stream || !ast_serialize(parser->ast_root,stream) ||
       fseek(stream,0L,SEEK_SET)!=0) goto done;
    decoded=ast_deserialize(stream);
    if(!decoded || !ast_validate(decoded,NULL)) goto done;
    {
        ASTFuncDecl* function=(ASTFuncDecl*)((ASTProgram*)decoded)->declarations;
        ASTBlock* outer=function && function->body?(ASTBlock*)function->body:NULL;
        ASTBlock* nested=outer && outer->statements?(ASTBlock*)outer->statements:NULL;
        valid=outer && nested &&
            outer->base.source_span.start_offset==(uint64_t)(outer_open-source) &&
            outer->base.source_span.end_offset==(uint64_t)(outer_close+1-source) &&
            nested->base.source_span.start_offset==(uint64_t)(nested_open-source) &&
            nested->base.source_span.end_offset==(uint64_t)(nested_close+1-source);
    }
done:
    if(!valid) fprintf(stderr,"Nested block exact source-span/codec test failed.\n");
    if(stream) fclose(stream);
    if(decoded) ast_destroy(decoded);
    parser_destroy(parser);
    return valid;
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
        "use db.mysql.sql as connector ns sql, db.mysql ns mysql, db.mysql.sql ns _;"
        "f:main(a!i32, b!i64) {"
        " v:first = a + b;"
        " { v:inner = combine(first, b:7); }"
        " v:pair = (first, b);"
        " struct: Point { v:x!i32, v:y!i32 }"
        " v:p!Point = Point { y = 2, x = 1 };"
        " nt:Callback = f:(buffer!u8[16], matrix!i32[4][4], dynamic!u8[])!u8[8];"
        " nt:NestedCallback = f:(test!f:(value!i32, buffer!u8[8])!bool)!bool;"
        " nt:id = u64;"
        " nt:enum: State { Idle, Running(pid!u32), Error(code!u16) }"
        " union:Register32 { v:full!u32, v:parts!u16 }"
        " group db { open => f:(path!str) { ret; } default { ret; } }"
        " v:casted = cast(1, i32);"
        " panic(\"boom\");"
        " ?? \"inspect\";"
        " ret;"
        "}";
    size_t fail_at;
    size_t oom_count=0u;
    int reached_success=0;
    for(fail_at=1u;fail_at<=512u;++fail_at) {
        ParserContext* ctx=parser_init_source("parser-oom-injection.nxf",
            (const uint8_t*)source,sizeof(source)-1u);
        int result;
        ParserStatus status;
        if(!ctx) return 0;
        parser_test_fail_allocation_at(ctx,fail_at);
        result=parser_run(ctx);
        status=parser_get_status(ctx);
        if(status==PARSER_STATUS_OUT_OF_MEMORY &&
           result==PARSER_STATUS_OUT_OF_MEMORY &&
           ctx->test_allocation_count>=fail_at) {
            ++oom_count;
            parser_destroy(ctx);
            continue;
        }
        if(status==PARSER_STATUS_OK && result==PARSER_STATUS_OK &&
           ctx->test_allocation_count<fail_at) {
            reached_success=1;
            parser_destroy(ctx);
            break;
        }
        fprintf(stderr,
            "OOM injection mismatch at allocation %zu (observed %zu, status %d, result %d).\\n",
            fail_at,ctx->test_allocation_count,(int)status,result);
        parser_destroy(ctx);
        return 0;
    }
    return oom_count>0u && reached_success;
}

static int test_struct_literal_parser_source_spans(void) {
    /* Struct literal parser shape: named fields retain source order. */
    {
        static const uint8_t source[] =
            "f:main() { v:p!Point = Point { y = 2, x = 1 }; }";
        ParserContext* parser = parser_init_source("<struct-literal>", source,
            sizeof(source) - 1u);
        ASTFuncDecl* function;
        ASTNode* statement;
        ASTVarDecl* variable;
        ASTStructLiteral* literal;
        ASTStructFieldInit* first;
        ASTStructFieldInit* second;
        const char* literal_start=(const char*)source;
        const char* literal_name=literal_start?strstr(literal_start,"Point {"):NULL;
        const char* literal_close=literal_name?strchr(literal_name,'}'):NULL;
        const char* first_start=literal_name?strstr(literal_name,"y = 2"):NULL;
        const char* second_start=literal_name?strstr(literal_name,"x = 1"):NULL;
        const char* first_value=first_start?strchr(first_start,'2'):NULL;
        const char* second_value=second_start?strchr(second_start,'1'):NULL;
        int valid = parser && parser_run(parser) == PARSER_STATUS_OK &&
            parser->error_count == 0 && parser->ast_root;
        if (valid) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            statement = function && function->body && function->body->type == AST_BLOCK
                ? ((ASTBlock*)function->body)->statements : NULL;
            variable = statement && statement->type == AST_VAR_DECL
                ? (ASTVarDecl*)statement : NULL;
            literal = variable && variable->initializer &&
                variable->initializer->type == AST_STRUCT_LITERAL
                ? (ASTStructLiteral*)variable->initializer : NULL;
            first = literal && literal->fields &&
                literal->fields->type == AST_STRUCT_FIELD_INIT
                ? (ASTStructFieldInit*)literal->fields : NULL;
            second = first && first->base.next &&
                first->base.next->type == AST_STRUCT_FIELD_INIT
                ? (ASTStructFieldInit*)first->base.next : NULL;
            valid = literal && literal->type_name &&
                strcmp(literal->type_name, "Point") == 0 &&
                literal_name && literal_close && literal->base.source_span.has_location &&
                literal->base.source_span.start_offset==(uint64_t)(literal_name-literal_start) &&
                literal->base.source_span.end_offset==(uint64_t)(literal_close+1-literal_start) &&
                literal->base.source_span.start_column==(uint32_t)(literal_name-literal_start)+1u &&
                literal->base.source_span.end_column==(uint32_t)(literal_close+1-literal_start)+1u &&
                first && strcmp(first->name, "y") == 0 && first->value &&
                first_start && first_value && first->base.source_span.has_location &&
                first->base.source_span.start_offset==(uint64_t)(first_start-literal_start) &&
                first->base.source_span.end_offset==(uint64_t)(first_value+1-literal_start) &&
                first->base.source_span.start_column==(uint32_t)(first_start-literal_start)+1u &&
                first->base.source_span.end_column==(uint32_t)(first_value+1-literal_start)+1u &&
                second && strcmp(second->name, "x") == 0 && second->value &&
                second_start && second_value && second->base.source_span.has_location &&
                second->base.source_span.start_offset==(uint64_t)(second_start-literal_start) &&
                second->base.source_span.end_offset==(uint64_t)(second_value+1-literal_start) &&
                second->base.source_span.start_column==(uint32_t)(second_start-literal_start)+1u &&
                second->base.source_span.end_column==(uint32_t)(second_value+1-literal_start)+1u &&
                second->base.next == NULL && ast_validate(parser->ast_root,NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Struct literal parser AST test failed.\\n");
            return 0;
        }
    }
    return 1;
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
    static const char* const invalid_sources[] = {
        "f:main() { if (true) }",
        "f:main() { if (true) {} elsif (false) }",
        "f:main() { if (true) {} els {} els {} }"
    };
    ParserContext* ctx = parser_init_source(
        "structured-if.nxf", (const uint8_t*)source, sizeof(source) - 1u);
    ASTProgram* program;
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTIf* first;
    ASTIf* second;
    ASTIf* simple;
    const char* first_start=strstr(source,"if (a == b)");
    const char* elsif_start=strstr(source,"elsif (a >= 1)");
    const char* first_end_text=strstr(source,"els { v:third = 3; }");
    const char* first_end=first_end_text?strchr(first_end_text,'}'):NULL;
    const char* simple_start=strstr(source,"if (a != b)");
    const char* simple_end_text=strstr(source,"v:fifth = 5;");
    const char* simple_end=simple_end_text?simple_end_text+strlen("v:fifth = 5;"):NULL;
    ASTNode* decoded=NULL;
    FILE* stream=NULL;
    size_t i;
    int ok = 0;
    if (!ctx) return 0;
    if (parser_run(ctx) != PARSER_STATUS_OK ||
        parser_get_status(ctx) != PARSER_STATUS_OK ||
        ctx->error_count != 0 || !ctx->ast_root ||
        !ast_validate(ctx->ast_root,NULL)) goto done;
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
    if(!first_start || !elsif_start || !first_end || !simple_start || !simple_end ||
       !first->base.source_span.has_location ||
       first->base.source_span.start_offset!=(uint64_t)(first_start-source) ||
       first->base.source_span.end_offset!=(uint64_t)(first_end+1-source) ||
       first->base.source_span.start_column!=(uint32_t)(first_start-source)+1u ||
       first->base.source_span.end_column!=(uint32_t)(first_end+1-source)+1u ||
       !second->base.source_span.has_location ||
       second->base.source_span.start_offset!=(uint64_t)(elsif_start-source) ||
       second->base.source_span.end_offset!=(uint64_t)(first_end+1-source) ||
       second->base.source_span.start_column!=(uint32_t)(elsif_start-source)+1u ||
       second->base.source_span.end_column!=(uint32_t)(first_end+1-source)+1u ||
       !simple->base.source_span.has_location ||
       simple->base.source_span.start_offset!=(uint64_t)(simple_start-source) ||
       simple->base.source_span.end_offset!=(uint64_t)(simple_end-source) ||
       simple->base.source_span.start_column!=(uint32_t)(simple_start-source)+1u ||
       simple->base.source_span.end_column!=(uint32_t)(simple_end-source)+1u) goto done;
    stream=tmpfile();
    if(!stream || !ast_serialize(ctx->ast_root,stream) || fseek(stream,0,SEEK_SET)!=0)goto done;
    decoded=ast_deserialize(stream);
    if(!decoded || !ast_validate(decoded,NULL))goto done;
    for(i=0u;i<sizeof(invalid_sources)/sizeof(invalid_sources[0]);++i) {
        if(!parser_source_reports_syntax_error("structured-if-invalid.nxf",
            invalid_sources[i],strlen(invalid_sources[i]))) goto done;
    }
    ok=1;
done:
    if(decoded)ast_destroy(decoded);
    if(stream)fclose(stream);
    parser_destroy(ctx);
    return ok;
}

static int test_conditional_control_question_disambiguation(void) {
    static const char source[] =
        "f:main() {"
        " (a == b) ? break;"
        " ((c != d)) ? continue;"
        " (e == f) ? ret 1;"
        " (g == h) ? { echo(\"yes\"); } : { echo(\"no\"); }"
        " (h == i) ? process(h);"
        " (j == k) ? process(j) : process(k);"
        " a == b -> target;"
        "}";
    ParserContext* ctx = parser_init_source(
        "conditional-control-question.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTProgram* program;
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTIf* break_action;
    ASTIf* continue_action;
    ASTIf* return_action;
    ASTIf* block_action;
    ASTIf* expression_action;
    ASTIf* expression_alternative_action;
    ASTNexusFlow* ordinary_pipe;
    const char* first_start = strstr(source, "(a == b) ? break;");
    const char* first_end = first_start ? first_start + strlen("(a == b) ? break;") : NULL;
    const char* block_start = strstr(source, "(g == h) ? {");
    const char* block_end_marker = block_start ? strstr(block_start, "} (h == i)") : NULL;
    const char* block_end = block_end_marker ? block_end_marker + 1 : NULL;
    const char* pipe_start = strstr(source, "a == b -> target");
    const char* pipe_target = pipe_start ? strstr(pipe_start, "target") : NULL;
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
        break_action->else_branch != NULL || !first_start || !first_end ||
        !break_action->base.source_span.has_location ||
        break_action->base.source_span.start_offset != (uint64_t)(first_start - source) ||
        break_action->base.source_span.end_offset != (uint64_t)(first_end - source))
        goto done;

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

    block_action = (ASTIf*)return_action->base.next;
    if (!block_action || block_action->base.type != AST_IF ||
        !block_action->then_branch || block_action->then_branch->type != AST_BLOCK ||
        !block_action->else_branch || block_action->else_branch->type != AST_BLOCK ||
        !block_start || !block_end ||
        !block_action->base.source_span.has_location ||
        block_action->base.source_span.start_offset != (uint64_t)(block_start - source) ||
        block_action->base.source_span.end_offset != (uint64_t)(block_end - source))
        goto done;

    expression_action = (ASTIf*)block_action->base.next;
    if (!expression_action || expression_action->base.type != AST_IF ||
        !expression_action->then_branch ||
        expression_action->then_branch->type != AST_FUNC_CALL ||
        expression_action->else_branch != NULL)
        goto done;

    expression_alternative_action = (ASTIf*)expression_action->base.next;
    if (!expression_alternative_action ||
        expression_alternative_action->base.type != AST_IF ||
        !expression_alternative_action->then_branch ||
        expression_alternative_action->then_branch->type != AST_FUNC_CALL ||
        !expression_alternative_action->else_branch ||
        expression_alternative_action->else_branch->type != AST_FUNC_CALL)
        goto done;

    ordinary_pipe = (ASTNexusFlow*)expression_alternative_action->base.next;
    if (!ordinary_pipe || ordinary_pipe->base.type != AST_NEXUS_PIPE ||
        ordinary_pipe->op != TOKEN_PIPE || !ordinary_pipe->left ||
        !ordinary_pipe->right || ordinary_pipe->right->type != AST_IDENTIFIER ||
        !pipe_start || !pipe_target || !ordinary_pipe->base.source_span.has_location ||
        ordinary_pipe->base.source_span.start_offset != (uint64_t)(pipe_start - source) ||
        ordinary_pipe->base.source_span.end_offset != (uint64_t)(pipe_target + strlen("target") - source) ||
        !ast_validate(ctx->ast_root, NULL)) goto done;
    ok = 1;

done:
    parser_destroy(ctx);
    if (!ok) return 0;
    if (!parser_source_reports_syntax_error(
        "conditional-control-missing-arm.nxf",
        "f:main() { (a == b) ? ; }",
        sizeof("f:main() { (a == b) ? ; }") - 1u)) return 0;
    return parser_source_reports_syntax_error(
        "legacy-arrow-conditional-control.nxf",
        "f:main() { (a == b) -> break; }",
        sizeof("f:main() { (a == b) -> break; }") - 1u);
}

static int test_select_scan_ast(void) {
    static const char source[] =
        "f:main() { v:x = 1;"
        " select(x) { 1 when x > 0 -> { v:a = 2; } default -> { v:b = 3; } }"
        " scan(x) { Range(s,e) when s < e && e > 0 -> { v:c = s; } item -> { v:d = item; } else -> { v:fallback = 0; } }"
        "}";
    ParserContext* ctx=parser_init_source("select-scan.nxf",(const uint8_t*)source,sizeof(source)-1u);
    ASTProgram* program;
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTNode* node;
    FILE* stream;
    ASTNode* decoded;
    const char* select_start=strstr(source,"select(x)");
    const char* select_first_start=strstr(source,"1 when x > 0");
    const char* select_first_end=strstr(source,"v:a = 2; }");
    const char* select_fallback_start=strstr(source,"default ->");
    const char* select_fallback_end=strstr(source,"v:b = 3; }");
    const char* select_end=strstr(source,"default -> { v:b = 3; } }");
    const char* scan_start=strstr(source,"scan(x)");
    const char* scan_first_start=strstr(source,"Range(s,e)");
    const char* scan_ctor_end=scan_first_start?strchr(scan_first_start,')'):NULL;
    const char* scan_binding_s=scan_first_start?strchr(scan_first_start,'('):NULL;
    const char* scan_binding_e=scan_binding_s?strchr(scan_binding_s,','):NULL;
    const char* scan_second_pattern=strstr(source,"item ->");
    const char* scan_first_end=strstr(source,"v:c = s; }");
    const char* scan_else_start=strstr(source,"else ->");
    const char* scan_else_end=strstr(source,"v:fallback = 0; }");
    const char* scan_end=strstr(source,"else -> { v:fallback = 0; } }");
    int ok=0;
    if(!ctx)return 0;
    if(!select_start || !select_first_start || !select_first_end ||
       !select_fallback_start || !select_fallback_end || !select_end ||
       !scan_start || !scan_first_start || !scan_ctor_end ||
       !scan_binding_s || !scan_binding_e || !scan_second_pattern ||
       !scan_first_end || !scan_else_start || !scan_else_end || !scan_end) goto done;
    select_first_end+=strlen("v:a = 2; }");
    select_fallback_end+=strlen("v:b = 3; }");
    select_end+=strlen("default -> { v:b = 3; } }");
    scan_first_end+=strlen("v:c = s; }");
    scan_else_end+=strlen("v:fallback = 0; }");
    scan_end+=strlen("else -> { v:fallback = 0; } }");
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
            !first->body || first->body->type!=AST_IF ||
            !((ASTIf*)first->body)->condition ||
            ((ASTIf*)first->body)->condition->type!=AST_BINARY_EXPR ||
            ((ASTBinaryExpr*)((ASTIf*)first->body)->condition)->op!=TOKEN_GT ||
            !fallback || fallback->base.type!=AST_SELECT_ARM || !fallback->is_default ||
            fallback->pattern!=NULL ||
            !select_node->base.source_span.has_location ||
            select_node->base.source_span.start_offset!=(uint64_t)(select_start-source) ||
            select_node->base.source_span.end_offset!=(uint64_t)(select_end-source) ||
            !first->base.source_span.has_location ||
            first->base.source_span.start_offset!=(uint64_t)(select_first_start-source) ||
            first->base.source_span.end_offset!=(uint64_t)(select_first_end-source) ||
            !fallback->base.source_span.has_location ||
            fallback->base.source_span.start_offset!=(uint64_t)(select_fallback_start-source) ||
            fallback->base.source_span.end_offset!=(uint64_t)(select_fallback_end-source))goto done;
    }
    node=node->next;
    if(!node || node->type!=AST_SCAN)goto done;
    {
        ASTScan* scan_node=(ASTScan*)node;
        ASTScanArm* first=(ASTScanArm*)scan_node->arms;
        ASTPatternConstructor* ctor;
        if(!scan_node->subject || !first || first->base.type!=AST_SCAN_ARM ||
            !first->pattern || first->pattern->type!=AST_PATTERN_CONSTRUCTOR ||
            !first->body || first->body->type!=AST_IF ||
            !((ASTIf*)first->body)->condition ||
            ((ASTIf*)first->body)->condition->type!=AST_BINARY_EXPR ||
            ((ASTBinaryExpr*)((ASTIf*)first->body)->condition)->op!=TOKEN_AND)goto done;
        ctor=(ASTPatternConstructor*)first->pattern;
        if(strcmp(ctor->name,"Range")!=0 || !ctor->bindings ||
            ctor->bindings->type!=AST_PATTERN_BINDING ||
            strcmp(((ASTPatternBinding*)ctor->bindings)->name,"s")!=0 ||
            !ctor->bindings->next ||
            strcmp(((ASTPatternBinding*)ctor->bindings->next)->name,"e")!=0 ||
            !ctor->base.source_span.has_location ||
            ctor->base.source_span.start_offset!=(uint64_t)(scan_first_start-source) ||
            ctor->base.source_span.end_offset!=(uint64_t)(scan_ctor_end+1-source) ||
            ((ASTPatternBinding*)ctor->bindings)->base.source_span.start_offset!=(uint64_t)(scan_binding_s+1-source) ||
            ((ASTPatternBinding*)ctor->bindings)->base.source_span.end_offset!=(uint64_t)(scan_binding_s+2-source) ||
            ((ASTPatternBinding*)ctor->bindings->next)->base.source_span.start_offset!=(uint64_t)(scan_binding_e+1-source) ||
            ((ASTPatternBinding*)ctor->bindings->next)->base.source_span.end_offset!=(uint64_t)(scan_binding_e+2-source))goto done;
        {
            ASTScanArm* second=(ASTScanArm*)first->base.next;
            ASTScanArm* fallback=second?(ASTScanArm*)second->base.next:NULL;
            ASTPatternBinding* simple_pattern=second && second->pattern &&
                second->pattern->type==AST_PATTERN_BINDING
                    ? (ASTPatternBinding*)second->pattern:NULL;
            if(!second || !simple_pattern || !simple_pattern->base.source_span.has_location ||
                simple_pattern->base.source_span.start_offset!=(uint64_t)(scan_second_pattern-source) ||
                simple_pattern->base.source_span.end_offset!=(uint64_t)(scan_second_pattern+4-source) ||
                !fallback || fallback->pattern ||
                !fallback->body ||
                !scan_node->base.source_span.has_location ||
                scan_node->base.source_span.start_offset!=(uint64_t)(scan_start-source) ||
                scan_node->base.source_span.end_offset!=(uint64_t)(scan_end-source) ||
                !first->base.source_span.has_location ||
                first->base.source_span.start_offset!=(uint64_t)(scan_first_start-source) ||
                first->base.source_span.end_offset!=(uint64_t)(scan_first_end-source) ||
                !fallback->base.source_span.has_location ||
                fallback->base.source_span.start_offset!=(uint64_t)(scan_else_start-source) ||
                fallback->base.source_span.end_offset!=(uint64_t)(scan_else_end-source))goto done;
        }
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
    if(!parser_source_reports_syntax_error("scan-duplicate-else.nxf",
        "f:main() { scan(x) { item -> { } else -> { } else -> { } } }",
        sizeof("f:main() { scan(x) { item -> { } else -> { } else -> { } } }")-1u))return 0;
    if(!parser_source_reports_syntax_error("scan-else-not-final.nxf",
        "f:main() { scan(x) { else -> { } item -> { } } }",
        sizeof("f:main() { scan(x) { else -> { } item -> { } } }")-1u))return 0;
    if(!parser_source_reports_syntax_error("select-default-guard.nxf",
        "f:main() { select(x) { default when true -> { } } }",
        sizeof("f:main() { select(x) { default when true -> { } } }")-1u))return 0;
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
        " on PrimeBorders.Success { ret true; }"
        " v:callback = f:(x) > x;"
        " v:callback_block = f:(x) { ret x; };"
        " unroll(3) -> loop(item in data) { v:copy = item; }"
        " v:moved = source _> target;"
        " v:retry = source?(5, 500ms)->target;"
        "}";
    ParserContext* ctx=parser_init_source(
        "rules-lambda-relocate-unroll.nxf",(const uint8_t*)source,sizeof(source)-1u);
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTNode* node;
    const char* rules_start=strstr(source,"rules PrimeBorders");
    const char* rules_close=rules_start?strstr(rules_start," } apply"):NULL;
    const char* barrier_start=strstr(source,"n < 2");
    const char* apply_start=strstr(source,"apply rules");
    const char* on_start=strstr(source,"on PrimeBorders");
    const char* on_end=on_start?strstr(on_start,"ret true; }"):NULL;
    const char* lambda_start=strstr(source,"f:(x) > x");
    const char* lambda_end=lambda_start?lambda_start+strlen("f:(x) > x"):NULL;
    const char* unroll_start=strstr(source,"unroll(3)");
    const char* unroll_end=unroll_start?strstr(unroll_start,"v:copy = item; }"):NULL;
    const char* relocate_start=strstr(source,"source _> target");
    const char* retry_start=strstr(source,"source?(5, 500ms)");
    int rules=0, apply=0, on_block=0, lambda=0, lambda_block=0, unroll=0, relocate=0, rolling=0;
    int ok=0;
    if(!ctx)return 0;
    if(parser_run(ctx)!=PARSER_STATUS_OK || ctx->error_count!=0 || !ctx->ast_root) { Error* e; fprintf(stderr,"feature parser status=%d errors=%d\n",(int)parser_get_status(ctx),ctx->error_count); for(e=ctx->error_head;e;e=e->next) fprintf(stderr,"  %d:%d %s\n",e->line,e->col,e->message?e->message:""); goto done; }
    function=(ASTFuncDecl*)((ASTProgram*)ctx->ast_root)->declarations;
    if(!function || function->base.type!=AST_FUNC_DECL ||
       !function->body || function->body->type!=AST_BLOCK)goto done;
    body=(ASTBlock*)function->body;
    for(node=body->statements;node;node=node->next) {
        if(node->type==AST_RULESET_DECL) {
            ASTRulesetDecl* r=(ASTRulesetDecl*)node;
            ASTNode* rule=r->rules;
            int barriers=0,triggers=0,always=0,defaults=0;
            if(!r->name || strcmp(r->name,"PrimeBorders")!=0 || !r->params ||
               !r->base.source_span.has_location ||
               r->base.source_span.start_offset!=(uint64_t)(rules_start-source) ||
               r->base.source_span.end_offset!=(uint64_t)(rules_close+2-source))goto done;
            if(!rule || !rule->source_span.has_location ||
               rule->source_span.start_offset!=(uint64_t)(barrier_start-source) ||
               rule->source_span.end_offset!=(uint64_t)(strchr(barrier_start,';')+1-source))goto done;
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
            if(!a->ruleset_name || strcmp(a->ruleset_name,"PrimeBorders")!=0 || !a->args ||
               !a->base.source_span.has_location ||
               a->base.source_span.start_offset!=(uint64_t)(apply_start-source) ||
               a->base.source_span.end_offset!=(uint64_t)(strchr(apply_start,';')+1-source))goto done;
            apply=1;
        } else if(node->type==AST_ON_BLOCK) {
            ASTOnBlock* o=(ASTOnBlock*)node;
            if(!o->ruleset_name || !o->event_name || strcmp(o->event_name,"Success")!=0 || !o->body ||
               !o->base.source_span.has_location ||
               o->base.source_span.start_offset!=(uint64_t)(on_start-source) ||
               o->base.source_span.end_offset!=(uint64_t)(on_end+11-source))goto done;
            on_block=1;
        } else if(node->type==AST_VAR_DECL) {
            ASTVarDecl* v=(ASTVarDecl*)node;
            if(v->name && strcmp(v->name,"callback")==0 && v->initializer && v->initializer->type==AST_LAMBDA && ((ASTLambda*)v->initializer)->body_is_expression==1) {
                ASTLambda* l=(ASTLambda*)v->initializer;
                if(!lambda_start || !lambda_end || !l->body ||
                   l->body->type!=AST_IDENTIFIER ||
                   !l->base.source_span.has_location ||
                   l->base.source_span.start_offset!=(uint64_t)(lambda_start-source) ||
                   l->base.source_span.end_offset!=(uint64_t)(lambda_end-source) ||
                   !l->body->source_span.has_location ||
                   l->body->source_span.start_offset!=(uint64_t)(lambda_end-1-source) ||
                   l->body->source_span.end_offset!=(uint64_t)(lambda_end-source))goto done;
                lambda=1;
            }
            if(v->name && strcmp(v->name,"callback_block")==0 && v->initializer && v->initializer->type==AST_LAMBDA && ((ASTLambda*)v->initializer)->body_is_expression==0) {
                ASTLambda* l=(ASTLambda*)v->initializer;
                if(!l->base.source_span.has_location || !l->body->source_span.has_location)goto done;
                lambda_block=1;
            }
            if(v->name && strcmp(v->name,"moved")==0 && v->initializer && v->initializer->type==AST_RELOCATE) {
                ASTRelocate* r=(ASTRelocate*)v->initializer;
                if(!r->base.source_span.has_location ||
                   r->base.source_span.start_offset!=(uint64_t)(relocate_start-source) ||
                   r->base.source_span.end_offset!=(uint64_t)(relocate_start+strlen("source _> target")-source))goto done;
                relocate=1;
            }
            if(v->name && strcmp(v->name,"retry")==0 && v->initializer && v->initializer->type==AST_NEXUS_FLOW) { ASTNexusFlow* f=(ASTNexusFlow*)v->initializer; if(f->retry_count==5 && f->retry_delay==500 && f->base.source_span.has_location &&
                f->base.source_span.start_offset==(uint64_t)(retry_start-source) &&
                f->base.source_span.end_offset==(uint64_t)(retry_start+strlen("source?(5, 500ms)->target")-source))rolling=1; }
        } else if(node->type==AST_UNROLL) {
            ASTUnroll* u=(ASTUnroll*)node;
            if(!u->count || !u->loop || u->loop->type!=AST_LOOP ||
               !u->base.source_span.has_location ||
               u->base.source_span.start_offset!=(uint64_t)(unroll_start-source) ||
               u->base.source_span.end_offset!=(uint64_t)(unroll_end+16-source))goto done;
            unroll=1;
        }
    }
    ok=rules && apply && on_block && lambda && lambda_block && unroll && relocate && rolling;
    if(!ok)fprintf(stderr,"feature AST flags rules=%d apply=%d on=%d lambda=%d lambda_block=%d unroll=%d relocate=%d rolling=%d\n",rules,apply,on_block,lambda,lambda_block,unroll,relocate,rolling);
    if(ok) {
        FILE* stream=tmpfile();
        ASTNode* decoded=NULL;
        if(!stream || !ast_serialize(ctx->ast_root,stream) || fseek(stream,0,SEEK_SET)!=0) ok=0;
        else {
            decoded=ast_deserialize(stream);
            if(!decoded || decoded->type!=AST_PROGRAM) ok=0;
        }
        if(decoded)ast_destroy(decoded);
        if(stream)fclose(stream);
    }
done:
    parser_destroy(ctx);
    return ok;
}

static int test_array_literal_ast(void) {
    static const char source[] =
        "f:main() { v:matrix = [[1, 2], [3, 4]]; v:empty = []; }";
    ParserContext* ctx=parser_init_source("array-literal.nxf",
        (const uint8_t*)source,sizeof(source)-1u);
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTNode* node;
    ASTArrayLiteral* matrix_node=NULL;
    ASTArrayLiteral* first_row=NULL;
    ASTArrayLiteral* empty_node=NULL;
    const char* matrix_start=strstr(source,"[[1, 2], [3, 4]]");
    const char* matrix_close=matrix_start?strstr(matrix_start,"]]"):NULL;
    const char* row_start=matrix_start?matrix_start+1:NULL;
    const char* row_close=row_start?strchr(row_start,']'):NULL;
    const char* empty_start=strstr(source,"[]");
    int matrix=0,empty=0,ok=0;
    if(!ctx)return 0;
    if(parser_run(ctx)!=PARSER_STATUS_OK || ctx->error_count!=0 || !ctx->ast_root) {
        Error* error;
        fprintf(stderr,"array literal parser status=%d errors=%d\n",
            (int)parser_get_status(ctx),ctx->error_count);
        for(error=ctx->error_head;error;error=error->next)
            fprintf(stderr,"  %d:%d %s\n",error->line,error->col,error->message?error->message:"");
        goto done;
    }
    function=(ASTFuncDecl*)((ASTProgram*)ctx->ast_root)->declarations;
    if(!function || !function->body || function->body->type!=AST_BLOCK)goto done;
    body=(ASTBlock*)function->body;
    for(node=body->statements;node;node=node->next) {
        if(node->type==AST_VAR_DECL) {
            ASTVarDecl* v=(ASTVarDecl*)node;
            if(v->name && strcmp(v->name,"matrix")==0 &&
               v->initializer && v->initializer->type==AST_ARRAY_LITERAL) {
                ASTNode* row=((ASTArrayLiteral*)v->initializer)->elements;
                if(row && row->type==AST_ARRAY_LITERAL &&
                   ((ASTArrayLiteral*)row)->elements &&
                   ((ASTArrayLiteral*)row)->elements->next) {
                    matrix=1;
                    matrix_node=(ASTArrayLiteral*)v->initializer;
                    first_row=(ASTArrayLiteral*)row;
                }
            }
            if(v->name && strcmp(v->name,"empty")==0 &&
               v->initializer && v->initializer->type==AST_ARRAY_LITERAL &&
               !((ASTArrayLiteral*)v->initializer)->elements) {
                empty=1;
                empty_node=(ASTArrayLiteral*)v->initializer;
            }
        }
    }
    matrix=matrix && matrix_start && matrix_close && row_start && row_close &&
        matrix_node && first_row && matrix_node->base.source_span.has_location &&
        matrix_node->base.source_span.start_offset==(uint64_t)(matrix_start-source) &&
        matrix_node->base.source_span.end_offset==(uint64_t)(matrix_close+2-source) &&
        matrix_node->base.source_span.start_column==(uint32_t)(matrix_start-source)+1u &&
        matrix_node->base.source_span.end_column==(uint32_t)(matrix_close+2-source)+1u &&
        first_row->base.source_span.has_location &&
        first_row->base.source_span.start_offset==(uint64_t)(row_start-source) &&
        first_row->base.source_span.end_offset==(uint64_t)(row_close+1-source) &&
        first_row->base.source_span.start_column==(uint32_t)(row_start-source)+1u &&
        first_row->base.source_span.end_column==(uint32_t)(row_close+1-source)+1u;
    empty=empty && empty_start && empty_node && empty_node->base.source_span.has_location &&
        empty_node->base.source_span.start_offset==(uint64_t)(empty_start-source) &&
        empty_node->base.source_span.end_offset==(uint64_t)(empty_start+2-source) &&
        empty_node->base.source_span.start_column==(uint32_t)(empty_start-source)+1u &&
        empty_node->base.source_span.end_column==(uint32_t)(empty_start+2-source)+1u;
    if(!matrix || !empty)
        fprintf(stderr,"array AST shape/span flags matrix=%d empty=%d\n",matrix,empty);
    if(matrix && empty) {
        FILE* stream=tmpfile();
        ASTNode* decoded=NULL;
        if(stream && ast_serialize(ctx->ast_root,stream) && fseek(stream,0,SEEK_SET)==0)
            decoded=ast_deserialize(stream);
        ok=decoded && decoded->type==AST_PROGRAM;
        ok=ok && parser_source_reports_syntax_error(
            "array-missing-close.nxf", "f:main() { v:a = [1, 2; }",
            sizeof("f:main() { v:a = [1, 2; }")-1u);
        ok=ok && parser_source_reports_syntax_error(
            "array-nested-missing-close.nxf",
            "f:main() { v:a = [[1, 2], [3, 4]; }",
            sizeof("f:main() { v:a = [[1, 2], [3, 4]; }")-1u);
        if(!ok) fprintf(stderr,"array AST binary roundtrip failed decoded=%p\n",(void*)decoded);
        if(decoded)ast_destroy(decoded);
        if(stream)fclose(stream);
    }
done:
    parser_destroy(ctx);
    return ok;
}

static int test_array_type_declarators(void) {
    static const char source[] =
        "f:consume(a![], b!i32[], third:i32[4]) {}"
        "f:main() { v:dynamic[]; v:fixed[5]!i32; v:matrix[3][3]!f32; }";
    ParserContext* ctx=parser_init_source("array-types.nxf",
        (const uint8_t*)source,sizeof(source)-1u);
    ASTNode* function;
    ASTNode* parameter;
    ASTNode* variable;
    const char* a_dimension=strstr(source,"a![]");
    const char* matrix_start=strstr(source,"matrix[3][3]");
    int ok=0;
    if(!ctx) return 0;
    if(!a_dimension || !matrix_start) goto done;
    a_dimension+=2;
    matrix_start+=6;
    if(parser_run(ctx)!=PARSER_STATUS_OK || ctx->error_count!=0 || !ctx->ast_root) {
        Error* error;
        fprintf(stderr,"array type parser status=%d errors=%d\n",
            (int)parser_get_status(ctx),ctx->error_count);
        for(error=ctx->error_head;error;error=error->next)
            fprintf(stderr,"  %d:%d %s\n",error->line,error->col,
                error->message?error->message:"");
        goto done;
    }
    function=((ASTProgram*)ctx->ast_root)->declarations;
    if(!function || function->type!=AST_FUNC_DECL) goto done;
    parameter=((ASTFuncDecl*)function)->params;
    if(!parameter || parameter->type!=AST_VAR_DECL ||
       !((ASTVarDecl*)parameter)->is_hard_typed ||
       !((ASTVarDecl*)parameter)->inline_type ||
       ((ASTVarDecl*)parameter)->inline_type->type!=AST_ARRAY_TYPE) goto done;
    {
        ASTArrayType* array=(ASTArrayType*)((ASTVarDecl*)parameter)->inline_type;
        ASTNode* dim=array->dimensions;
        if(!dim || dim->type!=AST_ARRAY_DIMENSION ||
           ((ASTArrayDimension*)dim)->length!=NULL || dim->next ||
           !array->base.source_span.has_location ||
           array->base.source_span.start_offset!=(uint64_t)(a_dimension-source) ||
           array->base.source_span.end_offset!=(uint64_t)(a_dimension+2-source) ||
           !dim->source_span.has_location ||
           dim->source_span.start_offset!=(uint64_t)(a_dimension-source) ||
           dim->source_span.end_offset!=(uint64_t)(a_dimension+2-source) ||
           dim->source_span.start_column!=(uint32_t)(a_dimension-source)+1u ||
           dim->source_span.end_column!=(uint32_t)(a_dimension+2-source)+1u) goto done;
    }
    parameter=parameter->next;
    if(!parameter || strcmp(((ASTVarDecl*)parameter)->type_name,"!i32")!=0 ||
       !((ASTVarDecl*)parameter)->inline_type ||
       ((ASTArrayType*)((ASTVarDecl*)parameter)->inline_type)->dimensions==NULL ||
       !((ASTVarDecl*)parameter)->type_expr ||
       ((ASTTypeExpr*)((ASTVarDecl*)parameter)->type_expr)->kind!=AST_TYPE_QUALIFIED ||
       !((ASTTypeExpr*)((ASTTypeExpr*)((ASTVarDecl*)parameter)->type_expr)->child) ||
       ((ASTTypeExpr*)((ASTTypeExpr*)((ASTVarDecl*)parameter)->type_expr)->child)->kind!=AST_TYPE_ARRAY ||
       ((ASTTypeExpr*)((ASTTypeExpr*)((ASTVarDecl*)parameter)->type_expr)->child)->dimensions!=
           ((ASTVarDecl*)parameter)->inline_type) goto done;
    parameter=parameter->next;
    if(!parameter || strcmp(((ASTVarDecl*)parameter)->type_name,"i32")!=0 ||
       !((ASTVarDecl*)parameter)->inline_type) goto done;
    {
        ASTNode* dim=((ASTArrayType*)((ASTVarDecl*)parameter)->inline_type)->dimensions;
        if(!dim || !((ASTArrayDimension*)dim)->length ||
           ((ASTArrayDimension*)dim)->length->type!=AST_LITERAL) goto done;
    }
    function=function->next;
    if(!function || function->type!=AST_FUNC_DECL || !((ASTFuncDecl*)function)->body) goto done;
    variable=((ASTBlock*)((ASTFuncDecl*)function)->body)->statements;
    if(!variable || variable->type!=AST_VAR_DECL ||
       !((ASTVarDecl*)variable)->inline_type) goto done;
    variable=variable->next;
    if(!variable || !((ASTVarDecl*)variable)->inline_type ||
       strcmp(((ASTVarDecl*)variable)->type_name,"i32")!=0 ||
       !((ASTVarDecl*)variable)->type_expr ||
       ((ASTTypeExpr*)((ASTVarDecl*)variable)->type_expr)->kind!=AST_TYPE_QUALIFIED ||
       !((ASTTypeExpr*)((ASTTypeExpr*)((ASTVarDecl*)variable)->type_expr)->child) ||
       ((ASTTypeExpr*)((ASTTypeExpr*)((ASTVarDecl*)variable)->type_expr)->child)->kind!=AST_TYPE_ARRAY ||
       ((ASTTypeExpr*)((ASTTypeExpr*)((ASTVarDecl*)variable)->type_expr)->child)->dimensions!=
           ((ASTVarDecl*)variable)->inline_type) goto done;
    variable=variable->next;
    if(!variable || !((ASTVarDecl*)variable)->inline_type ||
       strcmp(((ASTVarDecl*)variable)->type_name,"f32")!=0) goto done;
    {
        ASTArrayType* array=(ASTArrayType*)((ASTVarDecl*)variable)->inline_type;
        ASTNode* first=array->dimensions;
        ASTNode* second=first?first->next:NULL;
        const char* matrix_second=matrix_start+3;
        if(!first || !second || second->next ||
           !((ASTArrayDimension*)first)->length ||
           !((ASTArrayDimension*)second)->length ||
           !array->base.source_span.has_location ||
           array->base.source_span.start_offset!=(uint64_t)(matrix_start-source) ||
           array->base.source_span.end_offset!=(uint64_t)(matrix_second+3-source) ||
           !first->source_span.has_location ||
           first->source_span.start_offset!=(uint64_t)(matrix_start-source) ||
           first->source_span.end_offset!=(uint64_t)(matrix_start+3-source) ||
           !second->source_span.has_location ||
           second->source_span.start_offset!=(uint64_t)(matrix_second-source) ||
           second->source_span.end_offset!=(uint64_t)(matrix_second+3-source)) goto done;
    }
    {
        FILE* stream=tmpfile();
        ASTNode* decoded=NULL;
        if(stream && ast_serialize(ctx->ast_root,stream) && fseek(stream,0,SEEK_SET)==0)
            decoded=ast_deserialize(stream);
        ok=decoded && decoded->type==AST_PROGRAM;
        if(decoded) ast_destroy(decoded);
        if(stream) fclose(stream);
    }
done:
    parser_destroy(ctx);
    return ok;
}

static int test_function_array_return_ast(void) {
    static const char source[] =
        "f:empty()!i32[] {}"
        "f:one()!i32[n] {}"
        "f:matrix()!f32[m][n] {}";
    ParserContext* ctx=parser_init_source("function-array-return.nxf",
        (const uint8_t*)source,sizeof(source)-1u);
    ASTNode* fn;
    const char* empty_dimension=strstr(source,"!i32[]");
    const char* one_dimension=strstr(source,"!i32[n]");
    const char* matrix_dimension=strstr(source,"!f32[m][n]");
    int ok=0, stage=1;
    if(!ctx) return 0;
    if(!empty_dimension || !one_dimension || !matrix_dimension) goto done;
    empty_dimension+=4; one_dimension+=4; matrix_dimension+=4;
    if(parser_run(ctx)!=PARSER_STATUS_OK || ctx->error_count!=0 || !ctx->ast_root) goto done;
    stage=2; fn=((ASTProgram*)ctx->ast_root)->declarations;
    for(int expected=1;expected<=3;expected++) { stage=10+expected;
        ASTFuncDecl* f;
        ASTNode* dim;
        if(!fn || fn->type!=AST_FUNC_DECL) goto done;
        f=(ASTFuncDecl*)fn;
        if(!f->return_type || !f->return_array_type || f->return_array_type->type!=AST_ARRAY_TYPE ||
           !f->return_type_expr ||
           ((ASTTypeExpr*)f->return_type_expr)->kind!=AST_TYPE_QUALIFIED ||
           !((ASTTypeExpr*)((ASTTypeExpr*)f->return_type_expr)->child) ||
           ((ASTTypeExpr*)((ASTTypeExpr*)f->return_type_expr)->child)->kind!=AST_TYPE_ARRAY ||
           ((ASTTypeExpr*)((ASTTypeExpr*)f->return_type_expr)->child)->dimensions!=f->return_array_type) goto done;
        {
            ASTArrayType* array=(ASTArrayType*)f->return_array_type;
            const char* first_dimension=expected==1?empty_dimension:
                (expected==2?one_dimension:matrix_dimension);
            const int dimensions[3]={1,1,2};
            const size_t dimension_width=expected==1?2u:3u;
            size_t dimension_count=(size_t)dimensions[expected-1];
            if(!array->base.source_span.has_location ||
               array->base.source_span.start_offset!=(uint64_t)(first_dimension-source) ||
               array->base.source_span.end_offset!=(uint64_t)(first_dimension-source)+
                    dimension_count*dimension_width) goto done;
            dim=array->dimensions;
            for(int d=0;d<dimensions[expected-1];d++) {
                const char* expected_dimension=first_dimension+(size_t)d*dimension_width;
                if(!dim || dim->type!=AST_ARRAY_DIMENSION ||
                   !dim->source_span.has_location ||
                   dim->source_span.start_offset!=(uint64_t)(expected_dimension-source) ||
                   dim->source_span.end_offset!=(uint64_t)(expected_dimension-source)+dimension_width)
                    goto done;
                if(expected==1) {
                    if(((ASTArrayDimension*)dim)->length) goto done; /* [] */
                } else if(!((ASTArrayDimension*)dim)->length ||
                          ((ASTArrayDimension*)dim)->length->type!=AST_IDENTIFIER) goto done;
                dim=dim->next;
            }
        }
        if(dim) goto done;
        fn=fn->next;
    }
    stage=20; if(fn || !ast_validate(ctx->ast_root,NULL)) goto done;
    {
        FILE* stream=tmpfile(); ASTNode* decoded=NULL;
        if(stream && ast_serialize(ctx->ast_root,stream) && fseek(stream,0,SEEK_SET)==0)
            decoded=ast_deserialize(stream);
        stage=21; ok=decoded && ast_validate(decoded,NULL);
        if(ok) {
            ASTFuncDecl* decoded_fn=(ASTFuncDecl*)((ASTProgram*)decoded)->declarations;
            ASTTypeExpr* qualified=decoded_fn?(ASTTypeExpr*)decoded_fn->return_type_expr:NULL;
            ASTTypeExpr* array=qualified && qualified->kind==AST_TYPE_QUALIFIED
                ? (ASTTypeExpr*)qualified->child : NULL;
            ok=array && array->kind==AST_TYPE_ARRAY &&
               array->dimensions==decoded_fn->return_array_type;
        }
        if(decoded) ast_destroy(decoded); if(stream) fclose(stream);
    }
done:
    if(!ok) { Error* e; fprintf(stderr,"function array return test stage=%d status=%d errors=%d\n",stage,(int)parser_get_status(ctx),ctx->error_count); for(e=ctx->error_head;e;e=e->next) fprintf(stderr,"  %d:%d %s\n",e->line,e->col,e->message?e->message:"(null)"); }
    parser_destroy(ctx);
    return ok;
}

static int test_array_index_access_ast(void) {
    static const char source[] =
        "f:main() { v:items[4]!i32; v:x = items[1+2]; items[0] = x; v:y = object.field; }";
    ParserContext* ctx=parser_init_source("array-index.nxf",
        (const uint8_t*)source,sizeof(source)-1u);
    ASTNode* statement;
    ASTNode* decoded=NULL;
    const char* read_start=strstr(source,"items[1+2]");
    const char* read_end=read_start?read_start+strlen("items[1+2]"):NULL;
    const char* write_start=strstr(source,"items[0]");
    const char* write_end=write_start?write_start+strlen("items[0]"):NULL;
    const char* member_start=strstr(source,"object.field");
    const char* member_end=member_start?member_start+strlen("object.field"):NULL;
    FILE* stream;
    int ok=0;
    if(!ctx) return 0;
    if(parser_run(ctx)!=PARSER_STATUS_OK || ctx->error_count!=0 || !ctx->ast_root) goto done;
    statement=((ASTBlock*)((ASTFuncDecl*)((ASTProgram*)ctx->ast_root)->declarations)->body)->statements;
    statement=statement?statement->next:NULL;
    if(!statement || statement->type!=AST_VAR_DECL ||
       !((ASTVarDecl*)statement)->initializer ||
       ((ASTVarDecl*)statement)->initializer->type!=AST_INDEX_ACCESS) goto done;
    {
        ASTIndexAccess* index=(ASTIndexAccess*)((ASTVarDecl*)statement)->initializer;
        if(index->target->type!=AST_IDENTIFIER || index->index->type!=AST_BINARY_EXPR ||
           !read_start || !read_end || !index->base.source_span.has_location ||
           index->base.source_span.start_offset!=(uint64_t)(read_start-source) ||
           index->base.source_span.end_offset!=(uint64_t)(read_end-source) ||
           index->base.source_span.start_column!=(uint32_t)(read_start-source)+1u ||
           index->base.source_span.end_column!=(uint32_t)(read_end-source)+1u) goto done;
    }
    statement=statement->next;
    if(!statement || statement->type!=AST_ASSIGNMENT ||
       ((ASTBinaryExpr*)statement)->left->type!=AST_INDEX_ACCESS) goto done;
    {
        ASTIndexAccess* left=(ASTIndexAccess*)((ASTBinaryExpr*)statement)->left;
        if(!write_start || !write_end || !left->base.source_span.has_location ||
           left->base.source_span.start_offset!=(uint64_t)(write_start-source) ||
           left->base.source_span.end_offset!=(uint64_t)(write_end-source) ||
           left->base.source_span.start_column!=(uint32_t)(write_start-source)+1u ||
           left->base.source_span.end_column!=(uint32_t)(write_end-source)+1u) goto done;
    }
    statement=statement->next;
    if(!statement || statement->type!=AST_VAR_DECL ||
       !((ASTVarDecl*)statement)->initializer ||
       ((ASTVarDecl*)statement)->initializer->type!=AST_MEMBER_ACCESS) goto done;
    {
        ASTMemberAccess* member=(ASTMemberAccess*)((ASTVarDecl*)statement)->initializer;
        if(!member_start || !member_end || !member->member_name ||
           strcmp(member->member_name,"field")!=0 || !member->base.source_span.has_location ||
           member->base.source_span.start_offset!=(uint64_t)(member_start-source) ||
           member->base.source_span.end_offset!=(uint64_t)(member_end-source) ||
           member->base.source_span.start_column!=(uint32_t)(member_start-source)+1u ||
           member->base.source_span.end_column!=(uint32_t)(member_end-source)+1u) goto done;
    }
    stream=tmpfile();
    if(!stream) goto done;
    if(ast_serialize(ctx->ast_root,stream) && fseek(stream,0,SEEK_SET)==0)
        decoded=ast_deserialize(stream);
    fclose(stream);
    ok=decoded && decoded->type==AST_PROGRAM;
    if(decoded) ast_destroy(decoded);
    ok=ok && parser_source_reports_syntax_error(
        "array-index-missing-close.nxf","f:main() { v:x = items[1+2; }",
        sizeof("f:main() { v:x = items[1+2; }")-1u);
    ok=ok && parser_source_reports_syntax_error(
        "array-index-empty.nxf","f:main() { v:x = items[]; }",
        sizeof("f:main() { v:x = items[]; }")-1u);
done:
    parser_destroy(ctx);
    return ok;
}

static int test_expression_precedence_and_associativity_ast(void) {
    static const char source[] =
        "f:main() { v:guard = a > 0 && b > 0 || c == 1; "
        "v:bitwise_rhs = a && b | c; "
        "v:bitwise_lhs = a | b && c; "
        "v:bitwise_cmp_rhs = a < b | c; "
        "v:bitwise_cmp_lhs = a | b < c; "
        "v:sub_chain = a - b - c; "
        "v:div_chain = a / b / c; "
        "v:cmp_chain = a < b < c; "
        "v:eq_chain = a == b == c; "
        "v:bit_or_chain = a | b | c; "
        "v:bit_xor_chain = a ^ b ^ c; "
        "v:bit_and_chain = a & b & c; "
        "v:and_chain = a && b && c; "
        "v:or_chain = a || b || c; "
        "v:range = 1..10; }";
    ParserContext* ctx = parser_init_source(
        "logical-precedence.nxf", (const uint8_t*)source, sizeof(source) - 1u);
    ASTNode* statement;
    ASTNode* expression;
    ASTNode* decoded = NULL;
    FILE* stream = NULL;
    const char* guard_start=strstr(source,"a > 0 && b > 0 || c == 1");
    const char* guard_end=guard_start?guard_start+strlen("a > 0 && b > 0 || c == 1"):NULL;
    int ok = 0;
    if (!ctx) return 0;
    if (parser_run(ctx) != PARSER_STATUS_OK ||
        ctx->error_count != 0 || !ctx->ast_root) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }

    statement = ((ASTBlock*)((ASTFuncDecl*)
        ((ASTProgram*)ctx->ast_root)->declarations)->body)->statements;
    if (!statement || statement->type != AST_VAR_DECL) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    expression = ((ASTVarDecl*)statement)->initializer;
    if (!expression || expression->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)expression)->op != TOKEN_OR ||
        !guard_start || !guard_end || !expression->source_span.has_location ||
        expression->source_span.start_offset!=(uint64_t)(guard_start-source) ||
        expression->source_span.end_offset!=(uint64_t)(guard_end-source) ||
        expression->source_span.start_column!=(uint32_t)(guard_start-source)+1u ||
        expression->source_span.end_column!=(uint32_t)(guard_end-source)+1u) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    {
        ASTNode* and_expr = ((ASTBinaryExpr*)expression)->left;
        ASTNode* compare_right = ((ASTBinaryExpr*)expression)->right;
        if (!and_expr || and_expr->type != AST_BINARY_EXPR ||
            ((ASTBinaryExpr*)and_expr)->op != TOKEN_AND ||
            !compare_right || compare_right->type != AST_BINARY_EXPR ||
            ((ASTBinaryExpr*)compare_right)->op != TOKEN_EQ) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
        if (((ASTBinaryExpr*)and_expr)->left->type != AST_BINARY_EXPR ||
            ((ASTBinaryExpr*)((ASTBinaryExpr*)and_expr)->left)->op != TOKEN_GT ||
            ((ASTBinaryExpr*)and_expr)->right->type != AST_BINARY_EXPR ||
            ((ASTBinaryExpr*)((ASTBinaryExpr*)and_expr)->right)->op != TOKEN_GT) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    }

    statement = statement->next;
    if (!statement || statement->type != AST_VAR_DECL) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    expression = ((ASTVarDecl*)statement)->initializer;
    if (!expression || expression->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)expression)->op != TOKEN_AND ||
        !((ASTBinaryExpr*)expression)->right ||
        ((ASTBinaryExpr*)expression)->right->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)((ASTBinaryExpr*)expression)->right)->op != TOKEN_BIT_OR)
        { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }

    statement = statement->next;
    if (!statement || statement->type != AST_VAR_DECL) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    expression = ((ASTVarDecl*)statement)->initializer;
    if (!expression || expression->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)expression)->op != TOKEN_AND ||
        !((ASTBinaryExpr*)expression)->left ||
        ((ASTBinaryExpr*)expression)->left->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)((ASTBinaryExpr*)expression)->left)->op != TOKEN_BIT_OR)
        { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }

    statement = statement->next;
    if (!statement || statement->type != AST_VAR_DECL) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    expression = ((ASTVarDecl*)statement)->initializer;
    if (!expression || expression->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)expression)->op != TOKEN_LT ||
        !((ASTBinaryExpr*)expression)->right ||
        ((ASTBinaryExpr*)expression)->right->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)((ASTBinaryExpr*)expression)->right)->op != TOKEN_BIT_OR)
        { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }

    statement = statement->next;
    if (!statement || statement->type != AST_VAR_DECL) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    expression = ((ASTVarDecl*)statement)->initializer;
    if (!expression || expression->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)expression)->op != TOKEN_LT ||
        !((ASTBinaryExpr*)expression)->left ||
        ((ASTBinaryExpr*)expression)->left->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)((ASTBinaryExpr*)expression)->left)->op != TOKEN_BIT_OR)
        { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }

    statement = statement->next;
    if (!statement || statement->type != AST_VAR_DECL) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    expression = ((ASTVarDecl*)statement)->initializer;
    if (!expression || expression->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)expression)->op != TOKEN_MINUS ||
        !((ASTBinaryExpr*)expression)->left ||
        ((ASTBinaryExpr*)expression)->left->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)((ASTBinaryExpr*)expression)->left)->op != TOKEN_MINUS)
        { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }

    statement = statement->next;
    if (!statement || statement->type != AST_VAR_DECL) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    expression = ((ASTVarDecl*)statement)->initializer;
    if (!expression || expression->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)expression)->op != TOKEN_SLASH ||
        !((ASTBinaryExpr*)expression)->left ||
        ((ASTBinaryExpr*)expression)->left->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)((ASTBinaryExpr*)expression)->left)->op != TOKEN_SLASH)
        { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }

    statement = statement->next;
    if (!statement || statement->type != AST_VAR_DECL) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    expression = ((ASTVarDecl*)statement)->initializer;
    if (!expression || expression->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)expression)->op != TOKEN_LT ||
        !((ASTBinaryExpr*)expression)->left ||
        ((ASTBinaryExpr*)expression)->left->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)((ASTBinaryExpr*)expression)->left)->op != TOKEN_LT)
        { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }

    statement = statement->next;
    if (!statement || statement->type != AST_VAR_DECL) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    expression = ((ASTVarDecl*)statement)->initializer;
    if (!expression || expression->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)expression)->op != TOKEN_EQ ||
        !((ASTBinaryExpr*)expression)->left ||
        ((ASTBinaryExpr*)expression)->left->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)((ASTBinaryExpr*)expression)->left)->op != TOKEN_EQ)
        { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }

    statement = statement->next;
    if (!statement || statement->type != AST_VAR_DECL) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    expression = ((ASTVarDecl*)statement)->initializer;
    if (!expression || expression->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)expression)->op != TOKEN_BIT_OR ||
        !((ASTBinaryExpr*)expression)->left ||
        ((ASTBinaryExpr*)expression)->left->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)((ASTBinaryExpr*)expression)->left)->op != TOKEN_BIT_OR)
        { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }

    statement = statement->next;
    if (!statement || statement->type != AST_VAR_DECL) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    expression = ((ASTVarDecl*)statement)->initializer;
    if (!expression || expression->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)expression)->op != TOKEN_BIT_XOR ||
        !((ASTBinaryExpr*)expression)->left ||
        ((ASTBinaryExpr*)expression)->left->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)((ASTBinaryExpr*)expression)->left)->op != TOKEN_BIT_XOR)
        { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }

    statement = statement->next;
    if (!statement || statement->type != AST_VAR_DECL) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    expression = ((ASTVarDecl*)statement)->initializer;
    if (!expression || expression->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)expression)->op != TOKEN_BIT_AND ||
        !((ASTBinaryExpr*)expression)->left ||
        ((ASTBinaryExpr*)expression)->left->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)((ASTBinaryExpr*)expression)->left)->op != TOKEN_BIT_AND)
        { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }

    statement = statement->next;
    if (!statement || statement->type != AST_VAR_DECL) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    expression = ((ASTVarDecl*)statement)->initializer;
    if (!expression || expression->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)expression)->op != TOKEN_AND ||
        !((ASTBinaryExpr*)expression)->left ||
        ((ASTBinaryExpr*)expression)->left->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)((ASTBinaryExpr*)expression)->left)->op != TOKEN_AND)
        { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }

    statement = statement->next;
    if (!statement || statement->type != AST_VAR_DECL) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    expression = ((ASTVarDecl*)statement)->initializer;
    if (!expression || expression->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)expression)->op != TOKEN_OR ||
        !((ASTBinaryExpr*)expression)->left ||
        ((ASTBinaryExpr*)expression)->left->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)((ASTBinaryExpr*)expression)->left)->op != TOKEN_OR)
        { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }

    statement = statement->next;
    if (!statement || statement->type != AST_VAR_DECL) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    expression = ((ASTVarDecl*)statement)->initializer;
    if (!expression || expression->type != AST_BINARY_EXPR ||
        ((ASTBinaryExpr*)expression)->op != TOKEN_RANGE ||
        !((ASTBinaryExpr*)expression)->left ||
        ((ASTBinaryExpr*)expression)->left->type != AST_LITERAL ||
        !((ASTBinaryExpr*)expression)->right ||
        ((ASTBinaryExpr*)expression)->right->type != AST_LITERAL) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }

    {
        static const char* invalid_ranges[] = {
            "f:main() { v:bad = 1..2..3; }"
        };
        size_t invalid_index;
        for (invalid_index = 0;
             invalid_index < sizeof(invalid_ranges) / sizeof(invalid_ranges[0]);
             ++invalid_index) {
            ParserContext* chained_range = parser_init_source(
                "chained-range.nxf", (const uint8_t*)invalid_ranges[invalid_index],
                strlen(invalid_ranges[invalid_index]));
            int rejected = chained_range &&
                (parser_run(chained_range) != PARSER_STATUS_OK ||
                 chained_range->error_count != 0);
            if (chained_range) parser_destroy(chained_range);
            if (!rejected) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
        }
    }

    stream = tmpfile();
    if (!stream || !ast_serialize(ctx->ast_root, stream) ||
        fseek(stream, 0, SEEK_SET) != 0) { fprintf(stderr, "precedence fixture fail at line %d\\n", __LINE__); goto done; }
    decoded = ast_deserialize(stream);
    ok = decoded && decoded->type == AST_PROGRAM;
done:
    if (decoded) ast_destroy(decoded);
    if (stream) fclose(stream);
    parser_destroy(ctx);
    return ok;
}


static int test_power_bitwise_and_compound_assignment(void) {
    static const char source[] =
        "f:main() { v:p = 2 ^^ 3 ^^ 2; v:n = -2 ^^ 2; v:q = 2 ^^ -2; v:b = 1 | 2 ^ 3 & 4; v:x = 8; x += 2; x -= 1; x *= 2; x /= 2; x &= 3; x |= 1; x ^= 2; x .= \"z\"; }";
    ParserContext* ctx=parser_init_source("power-bitwise-compound.nxf",
        (const uint8_t*)source,sizeof(source)-1u);
    ASTProgram* program;ASTFuncDecl* function;ASTBlock* body;ASTNode* node;
    ASTNode* power_expression=NULL;
    ASTNode* unary_expression=NULL;
    ASTNode* plus_assignment=NULL;
    const char* power_start=strstr(source,"2 ^^ 3 ^^ 2");
    const char* power_end=power_start?power_start+strlen("2 ^^ 3 ^^ 2"):NULL;
    const char* unary_start=strstr(source,"-2 ^^ 2");
    const char* unary_end=unary_start?unary_start+strlen("-2 ^^ 2"):NULL;
    const char* assignment_start=strstr(source,"x += 2;");
    const char* assignment_end=assignment_start?assignment_start+strlen("x += 2"):NULL;
    int saw_power=0,saw_bitwise=0,ops=0,ok=0;
    if(!ctx)return 0;
    if(parser_run(ctx)!=PARSER_STATUS_OK||ctx->error_count||!ctx->ast_root)goto done;
    program=(ASTProgram*)ctx->ast_root;function=(ASTFuncDecl*)program->declarations;
    if(!function||function->base.type!=AST_FUNC_DECL)goto done;
    body=(ASTBlock*)function->body;if(!body||body->base.type!=AST_BLOCK)goto done;
    for(node=body->statements;node;node=node->next){
        if(node->type==AST_VAR_DECL){
            ASTVarDecl* variable=(ASTVarDecl*)node;
            if(variable->name&&strcmp(variable->name,"p")==0&&variable->initializer&&variable->initializer->type==AST_BINARY_EXPR){
                ASTBinaryExpr* root=(ASTBinaryExpr*)variable->initializer;
                if(root->op==TOKEN_POWER&&root->right&&root->right->type==AST_BINARY_EXPR&&((ASTBinaryExpr*)root->right)->op==TOKEN_POWER) {
                    saw_power=1;
                    power_expression=(ASTNode*)root;
                }
            }
            if(variable->name&&strcmp(variable->name,"n")==0&&variable->initializer&&variable->initializer->type==AST_UNARY_EXPR){
                ASTUnaryExpr* unary=(ASTUnaryExpr*)variable->initializer;
                if(unary->op==TOKEN_MINUS&&unary->operand&&unary->operand->type==AST_BINARY_EXPR&&((ASTBinaryExpr*)unary->operand)->op==TOKEN_POWER) {
                    saw_power|=2;
                    unary_expression=(ASTNode*)unary;
                }
            }
            if(variable->name&&strcmp(variable->name,"q")==0&&variable->initializer&&variable->initializer->type==AST_BINARY_EXPR){
                ASTBinaryExpr* root=(ASTBinaryExpr*)variable->initializer;
                if(root->op==TOKEN_POWER&&root->right&&root->right->type==AST_UNARY_EXPR&&((ASTUnaryExpr*)root->right)->op==TOKEN_MINUS)saw_power|=4;
            }
            if(variable->name&&strcmp(variable->name,"b")==0&&variable->initializer&&variable->initializer->type==AST_BINARY_EXPR){
                ASTBinaryExpr* root=(ASTBinaryExpr*)variable->initializer;
                if(root->op==TOKEN_BIT_OR&&root->right&&root->right->type==AST_BINARY_EXPR&&((ASTBinaryExpr*)root->right)->op==TOKEN_BIT_XOR){
                    ASTBinaryExpr* xor_node=(ASTBinaryExpr*)root->right;
                    if(xor_node->right&&xor_node->right->type==AST_BINARY_EXPR&&((ASTBinaryExpr*)xor_node->right)->op==TOKEN_BIT_AND)saw_bitwise=1;
                }
            }
        }else if(node->type==AST_ASSIGNMENT){
            ASTBinaryExpr* assignment=(ASTBinaryExpr*)node;
            if(assignment->op==TOKEN_PLUS_EQ) { ops|=1; plus_assignment=node; }
            if(assignment->op==TOKEN_MINUS_EQ)ops|=2;
            if(assignment->op==TOKEN_STAR_EQ)ops|=4;
            if(assignment->op==TOKEN_SLASH_EQ)ops|=8;
            if(assignment->op==TOKEN_BIT_AND_EQ)ops|=16;
            if(assignment->op==TOKEN_BIT_OR_EQ)ops|=32;
            if(assignment->op==TOKEN_BIT_XOR_EQ)ops|=64;
            if(assignment->op==TOKEN_STRING_APPEND)ops|=128;
        }
    }
    ok=(saw_power&1)&&(saw_power&2)&&(saw_power&4)&&saw_bitwise&&ops==255 &&
        power_start && power_end && power_expression &&
        power_expression->source_span.has_location &&
        power_expression->source_span.start_offset==(uint64_t)(power_start-source) &&
        power_expression->source_span.end_offset==(uint64_t)(power_end-source) &&
        power_expression->source_span.start_column==(uint32_t)(power_start-source)+1u &&
        power_expression->source_span.end_column==(uint32_t)(power_end-source)+1u &&
        unary_start && unary_end && unary_expression &&
        unary_expression->source_span.has_location &&
        unary_expression->source_span.start_offset==(uint64_t)(unary_start-source) &&
        unary_expression->source_span.end_offset==(uint64_t)(unary_end-source) &&
        unary_expression->source_span.start_column==(uint32_t)(unary_start-source)+1u &&
        unary_expression->source_span.end_column==(uint32_t)(unary_end-source)+1u &&
        assignment_start && assignment_end && plus_assignment &&
        plus_assignment->source_span.has_location &&
        plus_assignment->source_span.start_offset==(uint64_t)(assignment_start-source) &&
        plus_assignment->source_span.end_offset==(uint64_t)(assignment_end-source) &&
        plus_assignment->source_span.start_column==(uint32_t)(assignment_start-source)+1u &&
        plus_assignment->source_span.end_column==(uint32_t)(assignment_end-source)+1u;
done:parser_destroy(ctx);return ok;
}


static int test_extern_semicolon_declaration(void) {
    static const char source[] = "exf:add(a!i32, b!i32)!i32;";
    ParserContext* ctx = parser_init_source("extern-declaration.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTProgram* program;
    ASTFuncDecl* external;
    ASTVarDecl* a;
    ASTVarDecl* b;
    int ok = 0;

    if (!ctx) return 0;
    if (parser_run(ctx) != PARSER_STATUS_OK || ctx->error_count ||
        !ctx->ast_root) goto done;

    program = (ASTProgram*)ctx->ast_root;
    external = (ASTFuncDecl*)program->declarations;
    if (!external || external->base.type != AST_EXTERN_DECL ||
        !external->is_extern || !external->name ||
        strcmp(external->name, "add") != 0 || external->body != NULL ||
        !external->return_type || strcmp(external->return_type, "!i32") != 0)
        goto done;

    a = (ASTVarDecl*)external->params;
    b = a ? (ASTVarDecl*)a->base.next : NULL;
    if (!a || !b || b->base.next || !a->name || strcmp(a->name, "a") != 0 ||
        !a->type_name || strcmp(a->type_name, "!i32") != 0 ||
        !b->name || strcmp(b->name, "b") != 0 ||
        !b->type_name || strcmp(b->type_name, "!i32") != 0)
        goto done;

    if (!external->base.source_span.has_location ||
        external->base.source_span.start_offset != 0u ||
        external->base.source_span.end_offset != sizeof(source) - 1u)
        goto done;

    ok = ast_validate((ASTNode*)program, NULL);
done:
    parser_destroy(ctx);
    return ok;
}

static int test_extern_linkname_association_and_codec(void) {
    static const char source[] =
        "!!=linkname = \"math.obj\";\n"
        "exf:add(a!i32, b!i32)!i32;\n"
        "exf:sub(a!i32, b!i32)!i32;";
    ParserContext* ctx = parser_init_source("extern-linkname.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTProgram* program;
    ASTFuncDecl* add;
    ASTFuncDecl* sub;
    FILE* stream = NULL;
    ASTNode* decoded = NULL;
    int ok = 0;
    if (!ctx) return 0;
    if (parser_run(ctx) != PARSER_STATUS_OK || ctx->error_count ||
        !ctx->ast_root) goto done;
    program = (ASTProgram*)ctx->ast_root;
    add = (ASTFuncDecl*)program->declarations;
    sub = add ? (ASTFuncDecl*)add->base.next : NULL;
    if (!add || add->base.type != AST_EXTERN_DECL ||
        !sub || sub->base.type != AST_EXTERN_DECL || sub->base.next ||
        !add->linkname || strcmp(add->linkname, "math.obj") != 0 ||
        !sub->linkname || strcmp(sub->linkname, "math.obj") != 0 ||
        !ast_validate((ASTNode*)program, NULL)) goto done;

    stream = tmpfile();
    if (!stream || !ast_serialize((ASTNode*)program, stream) ||
        fseek(stream, 0, SEEK_SET) != 0) goto done;
    decoded = ast_deserialize(stream);
    if (!decoded || !ast_validate(decoded, NULL)) goto done;
    add = (ASTFuncDecl*)((ASTProgram*)decoded)->declarations;
    sub = add ? (ASTFuncDecl*)add->base.next : NULL;
    if (!add || !sub || !add->linkname || !sub->linkname ||
        strcmp(add->linkname, "math.obj") != 0 ||
        strcmp(sub->linkname, "math.obj") != 0) goto done;
    ok = 1;
done:
    if (decoded) ast_destroy(decoded);
    if (stream) fclose(stream);
    parser_destroy(ctx);
    return ok;
}

static int test_extern_echo_and_listen_keywords(void) {
    static const char source[] =
        "exf:external(a!i32)!i32 { ret a; }"
        "f:main() { echo(\"hello\"); !(listen): { echo(\"event\"); } "
        "!struct:Packed { v:a!u8, v:b!u32 } }";
    ParserContext* ctx=parser_init_source("extern-echo-listen.nxf",
        (const uint8_t*)source,sizeof(source)-1u);
    ASTProgram* program;ASTFuncDecl* external;ASTFuncDecl* main_fn;ASTBlock* body;
    ASTNode* echo_node;ASTNode* listen_node;ASTNode* packed_node;
    const char* listen_start=strstr(source,"!(listen)");
    const char* listen_body_end=listen_start?strstr(listen_start,"echo(\"event\"); }"):NULL;
    const char* packed_start=strstr(source,"!struct:Packed");
    const char* packed_close=packed_start?strchr(packed_start,'}'):NULL;
    int ok=0;
    if(!ctx)return 0;
    if(parser_run(ctx)!=PARSER_STATUS_OK||ctx->error_count||!ctx->ast_root)goto done;
    program=(ASTProgram*)ctx->ast_root;external=(ASTFuncDecl*)program->declarations;
    if(!external||external->base.type!=AST_EXTERN_DECL||!external->is_extern||
       !external->name||strcmp(external->name,"external")!=0)goto done;
    main_fn=(ASTFuncDecl*)external->base.next;
    if(!main_fn||main_fn->base.type!=AST_FUNC_DECL)goto done;
    body=(ASTBlock*)main_fn->body;if(!body||body->base.type!=AST_BLOCK)goto done;
    echo_node=body->statements;
    if(!echo_node||echo_node->type!=AST_ECHO)goto done;
    listen_node=echo_node->next;
    if(!listen_node||listen_node->type!=AST_LISTEN ||
       !listen_start || !listen_body_end ||
       !listen_node->source_span.has_location ||
       listen_node->source_span.start_offset!=(uint64_t)(listen_start-source) ||
       listen_node->source_span.end_offset!=(uint64_t)(listen_body_end+strlen("echo(\"event\"); }")-source) ||
       listen_node->source_span.start_line!=1u ||
       listen_node->source_span.start_column!=(uint32_t)(listen_start-source+1))goto done;
    packed_node=listen_node->next;
    if(!packed_node || packed_node->type!=AST_STRUCT_DECL ||
       !((ASTStructDecl*)packed_node)->is_packed ||
       !packed_start || !packed_close ||
       !packed_node->source_span.has_location ||
       packed_node->source_span.start_offset!=(uint64_t)(packed_start-source) ||
       packed_node->source_span.end_offset!=(uint64_t)(packed_close+1-source) ||
       packed_node->source_span.start_line!=1u ||
       packed_node->source_span.start_column!=(uint32_t)(packed_start-source+1))goto done;
    ok=1;
done:parser_destroy(ctx);return ok;
}

static int test_modulo_expression_ast(void) {
    static const char source[] =
        "f:main() { v:rem = 17 % 5 * 2; v:dimension[17 % 5]!i32; }";
    ParserContext* ctx = parser_init_source(
        "modulo-expression.nxf", (const uint8_t*)source, sizeof(source) - 1u);
    ASTNode* statement;
    ASTNode* decoded = NULL;
    FILE* stream = NULL;
    int ok = 0;
    if (!ctx) return 0;
    if (parser_run(ctx) != PARSER_STATUS_OK ||
        ctx->error_count != 0 || !ctx->ast_root) goto done;

    statement = ((ASTBlock*)((ASTFuncDecl*)
        ((ASTProgram*)ctx->ast_root)->declarations)->body)->statements;
    if (!statement || statement->type != AST_VAR_DECL) goto done;
    {
        ASTNode* expression = ((ASTVarDecl*)statement)->initializer;
        if (!expression || expression->type != AST_BINARY_EXPR ||
            ((ASTBinaryExpr*)expression)->op != TOKEN_STAR) goto done;
        expression = ((ASTBinaryExpr*)expression)->left;
        if (!expression || expression->type != AST_BINARY_EXPR ||
            ((ASTBinaryExpr*)expression)->op != TOKEN_PERCENT) goto done;
    }

    statement = statement->next;
    if (!statement || statement->type != AST_VAR_DECL ||
        !((ASTVarDecl*)statement)->inline_type ||
        ((ASTVarDecl*)statement)->inline_type->type != AST_ARRAY_TYPE) goto done;
    {
        ASTNode* dimension = ((ASTArrayType*)
            ((ASTVarDecl*)statement)->inline_type)->dimensions;
        ASTNode* length = dimension
            ? ((ASTArrayDimension*)dimension)->length : NULL;
        if (!length || length->type != AST_BINARY_EXPR ||
            ((ASTBinaryExpr*)length)->op != TOKEN_PERCENT) goto done;
    }

    stream = tmpfile();
    if (!stream || !ast_serialize(ctx->ast_root, stream) ||
        fseek(stream, 0, SEEK_SET) != 0) goto done;
    decoded = ast_deserialize(stream);
    ok = decoded && decoded->type == AST_PROGRAM;
done:
    if (decoded) ast_destroy(decoded);
    if (stream) fclose(stream);
    parser_destroy(ctx);
    return ok;
}

static int test_retry_metadata_rejects_overflow(void) {
    static const char source[] =
        "v:x = source?(999999999999999999999,1ms)->target;";
    return parser_source_reports_syntax_error(
        "retry-overflow.nxf",source,sizeof(source)-1u);
}

static int test_ast_create_rejects_invalid_node_kinds(void) {
    ASTNode* negative;
    ASTNode* past_end;

    /*
     * Exercise the complete append-only public node inventory. This catches
     * accidental gaps in the factory as new AST kinds are added.
     */
    for (int kind = (int)AST_PROGRAM;
         kind <= (int)AST_TYPE_EXPR; ++kind) {
        ASTNode* node = ast_create((ASTNodeType)kind);
        int valid = node != NULL &&
            node->type == (ASTNodeType)kind &&
            node->line == 0 && node->col == 0 && node->next == NULL;
        ast_destroy(node);
        if (!valid) return 0;
    }

    negative = ast_create((ASTNodeType)-1);
    past_end = ast_create((ASTNodeType)((int)AST_TYPE_EXPR + 1));
    if (negative != NULL || past_end != NULL) {
        ast_destroy(negative);
        ast_destroy(past_end);
        return 0;
    }
    return 1;
}

static int test_fast_exec_raw_asm_ast(void) {
    static const char source[] =
        "f:main()!i32 {\n"
        "    fastexec {\n"
        "        v:x!i32 = 10;\n"
        "        asm: ADD {\n"
        "            mov rax, %x:reg\r\n"
        "            ldm r0, {r1, r2}\n"
        "            db \"}\"\n"
        "        }\n"
        "        echo(x);\n"
        "    }\n"
        "    ret 0;\n"
        "}";
    static const char outside_source[] =
        "f:main() { asm: OUTSIDE { nop } }";
    static const char call_outside_source[]=
        "f:main() { asmcall(ADD); }";
    ParserContext* parser=parser_init_source("fast-exec-asm.nxf",
        (const uint8_t*)source,sizeof(source)-1u);
    const char* fast_start=strstr(source,"fastexec");
    const char* asm_start=strstr(source,"asm: ADD");
    const char* raw_start=asm_start?strchr(asm_start,'{')+1:NULL;
    const char* close_line=raw_start?strstr(raw_start,"\n        }\n"):NULL;
    const char* raw_end=close_line?close_line+9:NULL;
    ASTFuncDecl* function;
    ASTFastExecBlock* fast;
    ASTVarDecl* variable;
    ASTAsmLabel* label;
    int valid=0;
    if(!parser) return 0;
    if(parser_run(parser)!=PARSER_STATUS_OK || parser->error_count!=0 ||
        !parser->ast_root || !ast_validate((ASTNode*)parser->ast_root,NULL)) goto done;
    function=(ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
    if(!function || !function->body || function->body->type!=AST_BLOCK) goto done;
    fast=(ASTFastExecBlock*)((ASTBlock*)function->body)->statements;
    if(!fast || fast->base.type!=AST_FASTEXEC_BLOCK || !fast->statements ||
        fast->statements->type!=AST_VAR_DECL) goto done;
    variable=(ASTVarDecl*)fast->statements;
    label=(ASTAsmLabel*)variable->base.next;
    valid=label && label->base.type==AST_ASM_LABEL &&
        label->label && strcmp(label->label,"ADD")==0 &&
        raw_start && raw_end && label->source_length==(size_t)(raw_end-raw_start) &&
        memcmp(label->source_text,raw_start,label->source_length)==0 &&
        label->base.source_span.has_location==1u &&
        label->base.source_span.start_offset==(uint64_t)(asm_start-source) &&
        label->base.source_span.end_offset==(uint64_t)(raw_end+1-source) &&
        fast_start && fast->base.source_span.has_location==1u &&
        fast->base.source_span.start_offset==(uint64_t)(fast_start-source) &&
        ast_validate((ASTNode*)parser->ast_root,NULL);
done:
    parser_destroy(parser);
    if(!valid) {
        fprintf(stderr,"FASTEXEC/raw ASM Parser AST or exact source-span test failed.\n");
        return 0;
    }
    /* asmcall/asmjmp intentionally remain generic AST_FUNC_CALL nodes.
       FASTEXEC scope and label checks belong to Semantic, not Parser. */
    parser=parser_init_source("asmcall-generic-call.nxf",
        (const uint8_t*)call_outside_source,sizeof(call_outside_source)-1u);
    if(!parser) return 0;
    valid=parser_run(parser)==PARSER_STATUS_OK && parser->error_count==0 &&
        parser->ast_root &&
        ((ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations)->body &&
        ((ASTBlock*)((ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations)->body)->statements &&
        ((ASTBlock*)((ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations)->body)->statements->type==AST_FUNC_CALL;
    parser_destroy(parser);
    if(!valid) {
        fprintf(stderr,"asmcall generic AST_FUNC_CALL parsing failed.\n");
        return 0;
    }
    parser=parser_init_source("asm-outside-fast-exec.nxf",
        (const uint8_t*)outside_source,sizeof(outside_source)-1u);
    if(!parser) return 0;
    (void)parser_run(parser);
    valid=parser->error_count>0;
    parser_destroy(parser);
    if(!valid) fprintf(stderr,"ASM outside FASTEXEC was not diagnosed.\n");
    return valid;
}

static int test_fast_exec_asm_token_stream(void) {
    static const char source[]="f:main() { fastexec { v:x!i32=1; halt; asm: ADD {} } }";
    ParserContext* parser=parser_init_source("fast-exec-token-stream.nxf",
        (const uint8_t*)source,sizeof(source)-1u);
    Token tokens[32];
    size_t count=0u;
    int saw_body=0;
    int valid=0;
    if(!parser) return 0;
    while(count<sizeof(tokens)/sizeof(tokens[0])) {
        tokens[count]=lexer_next_token(parser);
        if(tokens[count].type==TOKEN_ERROR) goto done;
        if(tokens[count].type==TOKEN_ASM_BODY) saw_body=1;
        if(tokens[count].type==TOKEN_EOF) { ++count; break; }
        ++count;
    }
    if(count==0u || tokens[count-1u].type!=TOKEN_EOF || !saw_body ||
        !parser_attach_token_stream(parser,tokens,count)) goto done;
    if(parser_run(parser)==PARSER_STATUS_OK && parser->error_count==0 &&
        parser->ast_root && ast_validate((ASTNode*)parser->ast_root,NULL))
        valid=1;
done:
    parser_destroy(parser);
    if(!valid) fprintf(stderr,"FASTEXEC token-stream Parser test failed.\n");
    return valid;
}


/* Entry-point argv/argc remain ordinary typed AST parameters; runtime owns injection. */
static int test_main_cli_parameter_ast(void) {
    static const char source[] = "f:main(argc!i32, argv!str[])!i32 { ret argc; }";
    ParserContext* parser = parser_init_source("main-cli-params.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTFuncDecl* function;
    ASTVarDecl* argc_param;
    ASTVarDecl* argv_param;
    int valid = 0;
    if (!parser) return 0;
    if (parser_run(parser) == PARSER_STATUS_OK && parser->error_count == 0 &&
        ast_validate(parser->ast_root, NULL)) {
        function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
        argc_param = function ? (ASTVarDecl*)function->params : NULL;
        argv_param = argc_param ? (ASTVarDecl*)argc_param->base.next : NULL;
        valid = function && strcmp(function->name, "main") == 0 &&
            function->return_type && strcmp(function->return_type, "!i32") == 0 &&
            argc_param && strcmp(argc_param->name, "argc") == 0 &&
            argc_param->type_name && strcmp(argc_param->type_name, "!i32") == 0 &&
            !argc_param->inline_type &&
            argv_param && strcmp(argv_param->name, "argv") == 0 &&
            argv_param->type_name && strcmp(argv_param->type_name, "!str") == 0 &&
            argv_param->inline_type && argv_param->inline_type->type == AST_ARRAY_TYPE &&
            ((ASTArrayType*)argv_param->inline_type)->dimensions &&
            ((ASTArrayDimension*)((ASTArrayType*)argv_param->inline_type)->dimensions)->length == NULL &&
            argc_param->base.source_span.has_location &&
            argv_param->base.source_span.has_location;
    }
    parser_destroy(parser);
    if (!valid) fprintf(stderr, "main(argc, argv) typed AST contract failed.\n");
    return valid;
}

static int test_group_method_ast(void) {
    static const char source[] =
        "f:main() { group Pair { v:value!i32; get => f:get()!i32 { ret value; } } }";
    ParserContext* parser = parser_init_source("group-method.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTFuncDecl* function;
    ASTGroupDecl* group;
    ASTNode* member;
    ASTMethod* method;
    ASTFuncDecl* implementation;
    ASTNode* decoded = NULL;
    FILE* stream = NULL;
    int valid = 0;
    if (!parser) return 0;
    if (parser_run(parser) == PARSER_STATUS_OK && parser->error_count == 0 &&
        parser->ast_root && ast_validate(parser->ast_root, NULL)) {
        function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
        group = function ? (ASTGroupDecl*)((ASTBlock*)function->body)->statements : NULL;
        member = group ? group->members : NULL;
        method = member && member->next ? (ASTMethod*)member->next : NULL;
        implementation = method ? (ASTFuncDecl*)method->implementation : NULL;
        valid = group && group->name && strcmp(group->name, "Pair") == 0 &&
            member && member->type == AST_VAR_DECL &&
            method && method->base.type == AST_METHOD &&
            method->public_name && strcmp(method->public_name, "get") == 0 &&
            implementation && implementation->base.type == AST_FUNC_DECL &&
            implementation->name && strcmp(implementation->name, "get") == 0 &&
            implementation->return_type && strcmp(implementation->return_type, "!i32") == 0 &&
            implementation->body && implementation->body->type == AST_BLOCK;
    }
    if (valid) {
        stream = tmpfile();
        valid = stream && ast_serialize(parser->ast_root, stream) &&
            fseek(stream, 0L, SEEK_SET) == 0;
        if (valid) decoded = ast_deserialize(stream);
        valid = valid && decoded && ast_validate(decoded, NULL);
        if (valid) {
            function = (ASTFuncDecl*)((ASTProgram*)decoded)->declarations;
            group = function ? (ASTGroupDecl*)((ASTBlock*)function->body)->statements : NULL;
            member = group ? group->members : NULL;
            method = member && member->next ? (ASTMethod*)member->next : NULL;
            implementation = method ? (ASTFuncDecl*)method->implementation : NULL;
            valid = group && group->name && strcmp(group->name, "Pair") == 0 &&
                member && member->type == AST_VAR_DECL &&
                method && method->base.type == AST_METHOD &&
                method->public_name && strcmp(method->public_name, "get") == 0 &&
                implementation && implementation->name &&
                strcmp(implementation->name, "get") == 0 &&
                implementation->return_type &&
                strcmp(implementation->return_type, "!i32") == 0 &&
                implementation->body && implementation->body->type == AST_BLOCK;
        }
    }
    if (decoded) ast_destroy(decoded);
    if (stream) fclose(stream);
    parser_destroy(parser);
    if (!valid) fprintf(stderr, "Group method AST parser/codec ownership test failed.\n");
    return valid;
}

static int test_thread_block_ast(void) {
    static const char source[] =
        "f:main() { (worker)>{ v:value = 7; } }";
    ParserContext* parser = parser_init_source("thread-block.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTFuncDecl* function;
    ASTThreadBlock* thread;
    ASTBlock* body;
    ASTVarDecl* value;
    const char* thread_start=strstr(source,"(worker)>{");
    const char* thread_close=thread_start?strchr(thread_start,'}'):NULL;
    ASTNode* decoded = NULL;
    FILE* stream = NULL;
    int valid = 0;
    if (!parser) return 0;
    if (parser_run(parser) != PARSER_STATUS_OK || parser->error_count != 0 ||
        !parser->ast_root || !ast_validate(parser->ast_root, NULL)) goto done;
    function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
    thread = function && function->body
        ? (ASTThreadBlock*)((ASTBlock*)function->body)->statements : NULL;
    body = thread && thread->body ? (ASTBlock*)thread->body : NULL;
    value = body ? (ASTVarDecl*)body->statements : NULL;
    if (!thread || thread->base.type != AST_THREAD_BLOCK ||
        !thread->name || strcmp(thread->name, "worker") != 0 ||
        !body || body->base.type != AST_BLOCK ||
        !value || value->base.type != AST_VAR_DECL ||
        !value->name || strcmp(value->name, "value") != 0 ||
        !value->initializer || value->initializer->type != AST_LITERAL ||
        thread->mode != NULL || thread->timeout != NULL ||
        !thread_start || !thread_close || !thread->base.source_span.has_location ||
        thread->base.source_span.start_offset!=(uint64_t)(thread_start-source) ||
        thread->base.source_span.end_offset!=(uint64_t)(thread_close+1-source) ||
        thread->base.source_span.start_column!=(uint32_t)(thread_start-source)+1u ||
        thread->base.source_span.end_column!=(uint32_t)(thread_close+1-source)+1u)
        goto done;
    stream = tmpfile();
    if (!stream || !ast_serialize(parser->ast_root, stream) ||
        fseek(stream, 0L, SEEK_SET) != 0) goto done;
    decoded = ast_deserialize(stream);
    if (!decoded || !ast_validate(decoded, NULL)) goto done;
    function = (ASTFuncDecl*)((ASTProgram*)decoded)->declarations;
    thread = function && function->body
        ? (ASTThreadBlock*)((ASTBlock*)function->body)->statements : NULL;
    body = thread && thread->body ? (ASTBlock*)thread->body : NULL;
    value = body ? (ASTVarDecl*)body->statements : NULL;
    valid = thread && thread->base.type == AST_THREAD_BLOCK &&
        thread->name && strcmp(thread->name, "worker") == 0 &&
        body && value && value->name && strcmp(value->name, "value") == 0 &&
        thread->mode == NULL && thread->timeout == NULL;
done:
    if (!valid) fprintf(stderr, "Thread-block AST parser/codec ownership test failed.\n");
    if (decoded) ast_destroy(decoded);
    if (stream) fclose(stream);
    parser_destroy(parser);
    return valid;
}

static int test_spawn_is_outside_core_parser(void) {
    static const char source[] = "f:main() { spawn(worker); }";
    ParserContext* parser = parser_init_source("spawn-core-boundary.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    OccDiagnosticCollector diagnostics;
    OccDiagnosticView view;
    size_t i;
    int saw_boundary_diagnostic = 0;
    int valid;
    if (!parser) return 0;
    occ_diagnostics_init(&diagnostics);
    parser_attach_diagnostics(parser, &diagnostics, OCC_STAGE_PARSER);
    valid = parser_run(parser) == PARSER_STATUS_SYNTAX_ERROR &&
        parser_get_status(parser) == PARSER_STATUS_SYNTAX_ERROR &&
        parser->error_count > 0;
    for (i = 0u; i < occ_diagnostics_count(&diagnostics); ++i) {
        if (occ_diagnostics_get(&diagnostics, i, &view) &&
            view.record->stage_id == OCC_STAGE_PARSER &&
            view.message && strstr(view.message, "Thread module")) {
            saw_boundary_diagnostic = 1;
            break;
        }
    }
    valid = valid && saw_boundary_diagnostic;
    parser_destroy(parser);
    occ_diagnostics_destroy(&diagnostics);
    if (!valid) fprintf(stderr, "Core Parser accepted spawn or omitted the Thread-module boundary diagnostic.\n");
    return valid;
}

static int test_intrinsic_ast_family(void) {
    static const char* sources[] = {
        "f:main() { v:n = len(items); }",
        "f:main() { v:s = sizeof(x); }",
        "f:main() { v:t = typeof(x); }",
        "f:main() { v:o = is_ok(result); }",
        "f:main() { v:o = is_err(result); }",
        "f:main() { v:o = is_some(result); }",
        "f:main() { v:o = is_none(result); }",
        "f:main() { v:o = is_null(result); }",
        "f:main() { v:a = addr(x); }",
        "f:main() { v:c = clock(); }",
        "f:main() { v:d = datenow(); }",
        "f:main() { v:e = date(); }",
        "f:main() { v:f = time(); }",
        "f:main() { v:p = prnt(value); }",
        "f:main() { v:p = prmt(value); }",
        "f:main() { v:w = write(value); }",
        "f:main() { v:w = fwrite(value); }",
        "f:main() { v:x = exit(0); }",
        "f:main() { v:x = swap(a, b); }",
        "f:main() { v:x = peek(0); }",
        "f:main() { v:x = poke(0, 1); }",
        "f:main() { v:x = inb(0); }",
        "f:main() { v:x = outb(0, 1); }",
        "f:main() { v:x = irq(1); }",
        "f:main() { v:x = intr(1); }",
        "f:main() { v:x = reg.r(0); }",
        "f:main() { v:x = area(1); }",
        "f:main() { v:x = zone(1); }",
        "f:main() { v:x = free(value); }",
        "f:main() { v:x = send(value); }",
        "f:main() { v:x = receive(value); }",
        "f:main() { v:x = done(); }"
    };
    static const ASTNodeType expected[] = {
        AST_LEN, AST_SIZEOF, AST_TYPEOF, AST_IS_OK, AST_IS_ERR,
        AST_IS_SOME, AST_IS_NONE, AST_IS_NULL, AST_ADDR, AST_CLOCK,
        AST_DATENOW, AST_DATE, AST_TIME, AST_PRNT, AST_PRMT, AST_WRITE,
        AST_FWRITE, AST_EXIT, AST_SWAP, AST_PEEK, AST_POKE, AST_INB,
        AST_OUTB, AST_IRQ, AST_INTR, AST_REG, AST_AREA, AST_ZONE,
        AST_FREE, AST_SEND, AST_RECEIVE, AST_DONE
    };
    size_t i;
    if (sizeof(sources) / sizeof(sources[0]) !=
        sizeof(expected) / sizeof(expected[0])) return 0;

    for (i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        ParserContext* parser = parser_init_source("intrinsic-family.nxf",
            (const uint8_t*)sources[i], strlen(sources[i]));
        ASTFuncDecl* function;
        ASTNode* statement;
        ASTVarDecl* variable;
        ASTNode* decoded = NULL;
        FILE* stream = NULL;
        const char* assignment = strstr(sources[i], " = ");
        const char* expr_start = assignment ? assignment + 3 : NULL;
        const char* expr_end = expr_start ? strchr(expr_start, ';') : NULL;
        int valid = 0;
        if (!parser) return 0;
        if (parser_run(parser) == PARSER_STATUS_OK && parser->error_count == 0 &&
            parser->ast_root && ast_validate(parser->ast_root, NULL)) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            statement = function ? ((ASTBlock*)function->body)->statements : NULL;
            variable = statement && statement->type == AST_VAR_DECL
                ? (ASTVarDecl*)statement : NULL;
            valid = variable && variable->initializer && expr_start && expr_end &&
                variable->initializer->type == expected[i] &&
                variable->initializer->source_span.has_location &&
                variable->initializer->source_span.start_offset ==
                    (uint64_t)(expr_start - sources[i]) &&
                variable->initializer->source_span.end_offset ==
                    (uint64_t)(expr_end - sources[i]) &&
                variable->initializer->source_span.start_line == 1u &&
                variable->initializer->source_span.end_line == 1u &&
                variable->initializer->source_span.start_column ==
                    (uint32_t)(expr_start - sources[i] + 1) &&
                variable->initializer->source_span.end_column ==
                    (uint32_t)(expr_end - sources[i] + 1);
            if (valid) {
                stream = tmpfile();
                valid = stream && ast_serialize(parser->ast_root, stream) &&
                    fseek(stream, 0, SEEK_SET) == 0;
                if (valid) decoded = ast_deserialize(stream);
                valid = valid && decoded && decoded->type == AST_PROGRAM &&
                    ast_validate(decoded, NULL);
            }
        }
        if (decoded) ast_destroy(decoded);
        if (stream) fclose(stream);
        parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Intrinsic AST/codec mapping failed at index %lu.\\n",
                (unsigned long)i);
            return 0;
        }
    }
    return 1;
}

static int test_m_immutable_value_ast(void) {
    static const char source[] =
        "m:CoreValue = 42;\n"
        "f:main() { m:localValue = 7; ret 0; }";
    ParserContext* parser = parser_init_source("immutable-values.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTMustDecl* immutable;
    ASTFuncDecl* function;
    ASTMustDecl* local;
    ASTLiteral* literal;
    const char* declaration_start = strstr(source, "m:CoreValue");
    const char* declaration_end = declaration_start
        ? strchr(declaration_start, ';') : NULL;
    int valid = 0;
    if (!parser) return 0;
    if (parser_run(parser) == PARSER_STATUS_OK && parser->error_count == 0 &&
        parser->ast_root && ast_validate(parser->ast_root, NULL)) {
        immutable = (ASTMustDecl*)((ASTProgram*)parser->ast_root)->declarations;
        function = immutable ? (ASTFuncDecl*)immutable->base.next : NULL;
        local = function && function->body && function->body->type == AST_BLOCK
            ? (ASTMustDecl*)((ASTBlock*)function->body)->statements : NULL;
        literal = immutable && immutable->source &&
            immutable->source->type == AST_LITERAL
            ? (ASTLiteral*)immutable->source : NULL;
        valid = immutable && immutable->base.type == AST_MUST_DECL &&
            immutable->name && strcmp(immutable->name, "CoreValue") == 0 &&
            literal && literal->type == TOKEN_NUMBER && literal->string_value && strcmp(literal->string_value, "42") == 0 &&
            declaration_start && declaration_end &&
            immutable->base.source_span.has_location &&
            immutable->base.source_span.start_offset == (uint64_t)(declaration_start-source) &&
            immutable->base.source_span.end_offset == (uint64_t)(declaration_end+1-source) &&
            function && function->base.type == AST_FUNC_DECL &&
            local && local->base.type == AST_MUST_DECL &&
            local->name && strcmp(local->name, "localValue") == 0;
    }
    parser_destroy(parser);
    if (!valid) fprintf(stderr, "Immutable m: value AST contract failed.\n");
    return valid;
}

static int test_canonical_loop_and_jump_call_forms(void) {
    static const char source[] =
        "f:main() { loop { break; } jump(finish); finish:> { ret 0; } }";
    ParserContext* parser = parser_init_source(
        "canonical-loop-jump.nxf", (const uint8_t*)source, sizeof(source) - 1u);
    ASTNode* function = NULL;
    ASTNode* body = NULL;
    ASTNode* loop_node = NULL;
    ASTNode* jump_node = NULL;
    ASTNode* label_node = NULL;
    ASTNode* break_node = NULL;
    ASTNode* decoded = NULL;
    FILE* stream = NULL;
    const char* loop_start=strstr(source,"loop {");
    const char* loop_close=loop_start?strchr(loop_start,'}'):NULL;
    const char* break_start=strstr(source,"break;");
    const char* break_end=break_start?break_start+strlen("break;"):NULL;
    const char* jump_start=strstr(source,"jump(finish)");
    const char* jump_end=jump_start?jump_start+strlen("jump(finish);"):NULL;
    const char* label_start=strstr(source,"finish:>");
    const char* label_close=label_start?strchr(label_start,'}'):NULL;
    int parse_status = -1;
    int ast_status = -1;
    int serialize_status = 0;
    int decoded_status = -1;
    int valid = 0;

    if (!parser) return 0;
    parse_status = parser_run(parser);
    if (parse_status == 0 && parser->ast_root &&
        ((ASTNode*)parser->ast_root)->type == AST_PROGRAM) {
        function = ((ASTProgram*)parser->ast_root)->declarations;
        body = function && function->type == AST_FUNC_DECL
            ? ((ASTFuncDecl*)function)->body : NULL;
        loop_node = body && body->type == AST_BLOCK
            ? ((ASTBlock*)body)->statements : NULL;
        jump_node = loop_node ? loop_node->next : NULL;
        label_node = jump_node ? jump_node->next : NULL;
        break_node = loop_node && ((ASTLoop*)loop_node)->body &&
            ((ASTLoop*)loop_node)->body->type==AST_BLOCK
            ? ((ASTBlock*)((ASTLoop*)loop_node)->body)->statements : NULL;
        ast_status = ast_validate((ASTNode*)parser->ast_root, NULL);

        valid = function && function->type == AST_FUNC_DECL &&
            loop_node && loop_node->type == AST_LOOP &&
            ((ASTLoop*)loop_node)->loop_type == 0 &&
            ((ASTLoop*)loop_node)->body &&
            ((ASTLoop*)loop_node)->body->type == AST_BLOCK &&
            ((ASTBlock*)((ASTLoop*)loop_node)->body)->statements &&
            ((ASTBlock*)((ASTLoop*)loop_node)->body)->statements->type == AST_BREAK &&
            jump_node && jump_node->type == AST_NEXUS_JUMP &&
            ((ASTNexusJump*)jump_node)->label_name &&
            strcmp(((ASTNexusJump*)jump_node)->label_name, "finish") == 0 &&
            label_node && label_node->type == AST_NEXUS_LABEL &&
            strcmp(((ASTNexusLabel*)label_node)->label_name, "finish") == 0 &&
            loop_start && loop_close && loop_node->source_span.has_location &&
            loop_node->source_span.start_offset==(uint64_t)(loop_start-source) &&
            loop_node->source_span.end_offset==(uint64_t)(loop_close+1-source) &&
            loop_node->source_span.start_column==(uint32_t)(loop_start-source)+1u &&
            loop_node->source_span.end_column==(uint32_t)(loop_close+1-source)+1u &&
            break_start && break_end && break_node && break_node->type==AST_BREAK &&
            break_node->source_span.has_location &&
            break_node->source_span.start_offset==(uint64_t)(break_start-source) &&
            break_node->source_span.end_offset==(uint64_t)(break_end-source) &&
            break_node->source_span.start_column==(uint32_t)(break_start-source)+1u &&
            break_node->source_span.end_column==(uint32_t)(break_end-source)+1u &&
            jump_start && jump_end && jump_node->source_span.has_location &&
            jump_node->source_span.start_offset==(uint64_t)(jump_start-source) &&
            jump_node->source_span.end_offset==(uint64_t)(jump_end-source) &&
            jump_node->source_span.start_column==(uint32_t)(jump_start-source)+1u &&
            jump_node->source_span.end_column==(uint32_t)(jump_end-source)+1u &&
            label_start && label_close && label_node->source_span.has_location &&
            label_node->source_span.start_offset==(uint64_t)(label_start-source) &&
            label_node->source_span.end_offset==(uint64_t)(label_close+1-source) &&
            label_node->source_span.start_column==(uint32_t)(label_start-source)+1u &&
            label_node->source_span.end_column==(uint32_t)(label_close+1-source)+1u &&
            ast_status == 1;

        if (valid) {
            stream = tmpfile();
            serialize_status = stream
                ? ast_serialize((ASTNode*)parser->ast_root, stream) : 0;
            valid = stream != NULL && serialize_status != 0;
            if (valid) {
                rewind(stream);
                decoded = ast_deserialize(stream);
                decoded_status = decoded ? ast_validate(decoded, NULL) : -1;
                valid = decoded != NULL && decoded_status == 1;
            }
        }
    }

    if (!valid) fprintf(stderr, "Canonical loop/jump syntax AST contract failed.\n");


    if (decoded) ast_destroy(decoded);
    if (stream) fclose(stream);
    parser_destroy(parser);
    return valid;
}


static int test_dotted_module_use_path(void) {
    static const char source[] =
        "use msvcrt.exf as crt; f:main() { ret 0; }";
    ParserContext* parser = parser_init_source(
        "dotted-module-use.nxf", (const uint8_t*)source, sizeof(source) - 1u);
    ASTNode* use_node;
    ASTNode* function_node;
    ASTNode* decoded = NULL;
    FILE* stream = NULL;
    int valid = 0;

    if (!parser) return 0;
    if (parser_run(parser) == 0 && parser->ast_root &&
        ((ASTNode*)parser->ast_root)->type == AST_PROGRAM) {
        use_node = ((ASTProgram*)parser->ast_root)->declarations;
        function_node = use_node ? use_node->next : NULL;
        valid = use_node && use_node->type == AST_USE &&
            strcmp(((ASTUse*)use_node)->module_name, "msvcrt.exf") == 0 &&
            ((ASTUse*)use_node)->alias &&
            strcmp(((ASTUse*)use_node)->alias, "crt") == 0 &&
            function_node && function_node->type == AST_FUNC_DECL &&
            ast_validate((ASTNode*)parser->ast_root, NULL);
        if (valid) {
            stream = tmpfile();
            valid = stream != NULL &&
                ast_serialize((ASTNode*)parser->ast_root, stream) != 0;
            if (valid) {
                rewind(stream);
                decoded = ast_deserialize(stream);
                valid = decoded != NULL && ast_validate(decoded, NULL);
            }
        }
    }
    if (decoded) ast_destroy(decoded);
    if (stream) fclose(stream);
    parser_destroy(parser);
    if (!valid) fprintf(stderr, "Dotted module use path AST contract failed.\n");
    return valid;
}

static int test_use_path_malformed_and_exact_spans(void) {
    static const char valid_source[] = "use alpha.beta.gamma as mod;\n";
    static const char* invalid_sources[] = {
        "use alpha.;",
        "use alpha..beta;",
        "use alpha.beta as ;",
        "use alpha.beta"
    };
    ParserContext* parser = parser_init_source(
        "use-path-span.nxf", (const uint8_t*)valid_source, sizeof(valid_source)-1u);
    ASTUse* use_node;
    int valid = 0;
    size_t i;
    if (!parser) return 0;
    if (parser_run(parser) == PARSER_STATUS_OK && parser->error_count == 0 &&
        parser->ast_root && ((ASTNode*)parser->ast_root)->type == AST_PROGRAM) {
        use_node = (ASTUse*)((ASTProgram*)parser->ast_root)->declarations;
        valid = use_node && use_node->base.type == AST_USE &&
            strcmp(use_node->module_name, "alpha.beta.gamma") == 0 &&
            strcmp(use_node->alias, "mod") == 0 &&
            use_node->base.source_span.has_location &&
            use_node->base.source_span.start_offset == 4u &&
            use_node->base.source_span.end_offset == 27u &&
            use_node->base.source_span.start_line == 1u &&
            use_node->base.source_span.start_column == 5u &&
            use_node->base.source_span.end_line == 1u &&
            use_node->base.source_span.end_column == 28u &&
            ast_validate((ASTNode*)parser->ast_root, NULL);
    }
    parser_destroy(parser);
    if (!valid) {
        fprintf(stderr, "USE dotted path exact source-span assertion failed.\n");
        return 0;
    }
    for (i=0; i<sizeof(invalid_sources)/sizeof(invalid_sources[0]); ++i) {
        parser = parser_init_source("use-path-invalid.nxf",
            (const uint8_t*)invalid_sources[i], strlen(invalid_sources[i]));
        if (!parser) return 0;
        if (parser_run(parser) == PARSER_STATUS_OK || parser->error_count == 0) {
            fprintf(stderr, "Malformed USE path accepted: %s\n", invalid_sources[i]);
            parser_destroy(parser);
            return 0;
        }
        parser_destroy(parser);
    }
    return 1;
}

static int test_use_alias_and_namespace_ast_codec(void) {
    static const char source[] =
        "use db.mysql.sql as connector ns sql, db.mysql ns mysql, legacy as _, plain, db.mysql.sql ns _;\n";
    static const char* invalid_sources[] = {
        "use db.mysql ns ;",
        "use db.mysql ns mysql ns sql;",
        "use db.mysql as ;"
    };
    ParserContext* parser = parser_init_source("use-alias-namespace.nxf",
        (const uint8_t*)source, sizeof(source)-1u);
    ASTUse* first;
    ASTUse* second;
    ASTUse* third;
    ASTUse* fourth;
    ASTUse* fifth;
    ASTNode* decoded = NULL;
    FILE* stream = NULL;
    int valid = 0;
    size_t i;
    if (!parser) return 0;
    if (parser_run(parser)==PARSER_STATUS_OK && parser->error_count==0 &&
        parser->ast_root && ((ASTNode*)parser->ast_root)->type==AST_PROGRAM) {
        first=(ASTUse*)((ASTProgram*)parser->ast_root)->declarations;
        second=first?(ASTUse*)first->base.next:NULL;
        third=second?(ASTUse*)second->base.next:NULL;
        fourth=third?(ASTUse*)third->base.next:NULL;
        fifth=fourth?(ASTUse*)fourth->base.next:NULL;
        valid=first && second && third && fourth && fifth && !fifth->base.next &&
            strcmp(first->module_name,"db.mysql.sql")==0 &&
            strcmp(first->alias,"connector")==0 &&
            strcmp(first->namespace_name,"sql")==0 &&
            strcmp(second->module_name,"db.mysql")==0 &&
            second->alias==NULL && strcmp(second->namespace_name,"mysql")==0 &&
            strcmp(third->module_name,"legacy")==0 &&
            strcmp(third->alias,"_")==0 && third->namespace_name==NULL &&
            strcmp(fourth->module_name,"plain")==0 &&
            fourth->alias==NULL && fourth->namespace_name==NULL &&
            strcmp(fifth->module_name,"db.mysql.sql")==0 &&
            fifth->alias==NULL && strcmp(fifth->namespace_name,"_")==0 &&
            first->base.source_span.has_location &&
            first->base.source_span.start_offset==4u &&
            first->base.source_span.end_offset==
                (uint64_t)(strstr(source,"sql,")-source+3u) &&
            second->base.source_span.end_offset==
                (uint64_t)(strstr(source,"ns mysql")-source+8u) &&
            ast_validate((ASTNode*)parser->ast_root,NULL);
        if (valid) {
            stream=tmpfile();
            valid=stream && ast_serialize((ASTNode*)parser->ast_root,stream);
            if(valid) {
                rewind(stream);
                decoded=ast_deserialize(stream);
                valid=decoded && ast_validate(decoded,NULL);
            }
            if(valid) {
                ASTUse* roundtrip=(ASTUse*)((ASTProgram*)decoded)->declarations;
                valid=roundtrip && roundtrip->namespace_name &&
                    strcmp(roundtrip->namespace_name,"sql")==0 &&
                    roundtrip->alias && strcmp(roundtrip->alias,"connector")==0;
            }
        }
    }
    if(decoded) ast_destroy(decoded);
    if(stream) fclose(stream);
    parser_destroy(parser);
    if(!valid) {
        fprintf(stderr,"USE alias/namespace AST, span or codec test failed.\\n");
        return 0;
    }
    for(i=0;i<sizeof(invalid_sources)/sizeof(invalid_sources[0]);++i) {
        parser=parser_init_source("use-namespace-invalid.nxf",
            (const uint8_t*)invalid_sources[i],strlen(invalid_sources[i]));
        if(!parser) return 0;
        if(parser_run(parser)==PARSER_STATUS_OK || parser->error_count==0) {
            fprintf(stderr,"Malformed USE namespace accepted: %s\\n",invalid_sources[i]);
            parser_destroy(parser);
            return 0;
        }
        parser_destroy(parser);
    }
    return 1;
}

static int test_use_comma_list_spans(void) {
    static const char source[] = "use alpha as a, beta.gamma as b;\n";
    ParserContext* parser = parser_init_source("use-list.nxf",
        (const uint8_t*)source, sizeof(source)-1u);
    ASTUse* first = NULL;
    ASTUse* second = NULL;
    int valid = 0;
    if (!parser) return 0;
    if (parser_run(parser)==PARSER_STATUS_OK && parser->error_count==0 &&
        parser->ast_root && ((ASTNode*)parser->ast_root)->type==AST_PROGRAM) {
        first=(ASTUse*)((ASTProgram*)parser->ast_root)->declarations;
        second=first?(ASTUse*)first->base.next:NULL;
        valid=first && second && !second->base.next &&
            strcmp(first->module_name,"alpha")==0 && strcmp(first->alias,"a")==0 &&
            strcmp(second->module_name,"beta.gamma")==0 && strcmp(second->alias,"b")==0 &&
            first->base.source_span.has_location &&
            first->base.source_span.start_offset==4u &&
            first->base.source_span.end_offset==14u &&
            second->base.source_span.has_location &&
            second->base.source_span.start_offset==16u &&
            second->base.source_span.end_offset==31u &&
            first->base.source_span.start_line==1u &&
            second->base.source_span.start_line==1u &&
            ast_validate((ASTNode*)parser->ast_root,NULL);
    }
    if(!valid) {
        Error* diagnostic;
        fprintf(stderr,"USE comma-list ownership/span test failed (errors=%d, status=%d).\\n",
            parser->error_count,(int)parser_get_status(parser));
        for(diagnostic=parser->error_head;diagnostic;diagnostic=diagnostic->next)
            fprintf(stderr,"  parser error %d at %d:%d: %s\\n",
                diagnostic->code,diagnostic->line,diagnostic->col,
                diagnostic->message?diagnostic->message:"(null)");
        if(first) fprintf(stderr," first path=%s alias=%s span=%llu..%llu loc=%u\\n",
            first->module_name?first->module_name:"(null)",
            first->alias?first->alias:"(null)",
            (unsigned long long)first->base.source_span.start_offset,
            (unsigned long long)first->base.source_span.end_offset,
            (unsigned)first->base.source_span.has_location);
        if(second) fprintf(stderr," second path=%s alias=%s span=%llu..%llu loc=%u\\n",
            second->module_name?second->module_name:"(null)",
            second->alias?second->alias:"(null)",
            (unsigned long long)second->base.source_span.start_offset,
            (unsigned long long)second->base.source_span.end_offset,
            (unsigned)second->base.source_span.has_location);
    }
    parser_destroy(parser);
    return valid;
}

static int test_pointer_reference_type_declarators(void) {
    static const char source[] =
        "nt:CharPtr = *char; "
        "f:ptrs(p:*char, pp!**u8, ref:&Point)!*char { "
        "v:local!*char; ret p; }";
    ParserContext* parser = parser_init_source(
        "pointer-reference-types.nxf", (const uint8_t*)source, sizeof(source) - 1u);
    ASTNode* alias_node;
    ASTNode* function_node;
    ASTFuncDecl* function;
    ASTVarDecl* p;
    ASTVarDecl* pp;
    ASTVarDecl* ref_param;
    ASTNode* local_node;
    ASTNode* ret_node;
    ASTNode* decoded = NULL;
    FILE* stream = NULL;
    int valid = 0;

    if (!parser) return 0;
    if (parser_run(parser) == 0 && parser->ast_root &&
        ((ASTNode*)parser->ast_root)->type == AST_PROGRAM) {
        alias_node = ((ASTProgram*)parser->ast_root)->declarations;
        function_node = alias_node ? alias_node->next : NULL;
        function = function_node && function_node->type == AST_FUNC_DECL
            ? (ASTFuncDecl*)function_node : NULL;
        p = function ? (ASTVarDecl*)function->params : NULL;
        pp = p ? (ASTVarDecl*)p->base.next : NULL;
        ref_param = pp ? (ASTVarDecl*)pp->base.next : NULL;
        local_node = function && function->body && function->body->type == AST_BLOCK
            ? ((ASTBlock*)function->body)->statements : NULL;
        ret_node = local_node ? local_node->next : NULL;

        ASTNode* buffer_alias = function_node ? function_node->next : NULL;
        ASTNode* matrix_alias = buffer_alias ? buffer_alias->next : NULL;
        ASTNode* dynamic_alias = matrix_alias ? matrix_alias->next : NULL;
        ASTTypeExpr* buffer_type = buffer_alias && buffer_alias->type == AST_NEWTYPE_DECL
            ? (ASTTypeExpr*)((ASTNewTypeDecl*)buffer_alias)->target_type_expr : NULL;
        ASTTypeExpr* matrix_type = matrix_alias && matrix_alias->type == AST_NEWTYPE_DECL
            ? (ASTTypeExpr*)((ASTNewTypeDecl*)matrix_alias)->target_type_expr : NULL;
        ASTTypeExpr* dynamic_type = dynamic_alias && dynamic_alias->type == AST_NEWTYPE_DECL
            ? (ASTTypeExpr*)((ASTNewTypeDecl*)dynamic_alias)->target_type_expr : NULL;
        ASTVarDecl* buffer_param = buffer_type && buffer_type->kind == AST_TYPE_FUNCTION
            ? (ASTVarDecl*)buffer_type->params : NULL;
        ASTTypeExpr* buffer_array = buffer_param ? (ASTTypeExpr*)buffer_param->type_expr : NULL;
        ASTArrayType* buffer_dims = buffer_array && buffer_array->kind == AST_TYPE_ARRAY
            ? (ASTArrayType*)buffer_array->dimensions : NULL;
        ASTArrayDimension* buffer_dim = buffer_dims ? (ASTArrayDimension*)buffer_dims->dimensions : NULL;
        ASTVarDecl* matrix_param = matrix_type && matrix_type->kind == AST_TYPE_FUNCTION
            ? (ASTVarDecl*)matrix_type->params : NULL;
        ASTTypeExpr* matrix_array = matrix_param ? (ASTTypeExpr*)matrix_param->type_expr : NULL;
        ASTArrayType* matrix_dims = matrix_array && matrix_array->kind == AST_TYPE_ARRAY
            ? (ASTArrayType*)matrix_array->dimensions : NULL;
        ASTArrayDimension* matrix_dim1 = matrix_dims ? (ASTArrayDimension*)matrix_dims->dimensions : NULL;
        ASTArrayDimension* matrix_dim2 = matrix_dim1 ? (ASTArrayDimension*)matrix_dim1->base.next : NULL;
        ASTVarDecl* dynamic_param = dynamic_type && dynamic_type->kind == AST_TYPE_FUNCTION
            ? (ASTVarDecl*)dynamic_type->params : NULL;
        ASTTypeExpr* dynamic_array = dynamic_param ? (ASTTypeExpr*)dynamic_param->type_expr : NULL;
        ASTArrayType* dynamic_dims = dynamic_array && dynamic_array->kind == AST_TYPE_ARRAY
            ? (ASTArrayType*)dynamic_array->dimensions : NULL;

        valid = alias_node && alias_node->type == AST_NEWTYPE_DECL &&
            strcmp(((ASTNewTypeDecl*)alias_node)->target_type, "*char") == 0 &&
            ((ASTNewTypeDecl*)alias_node)->target_type_expr &&
            ((ASTTypeExpr*)((ASTNewTypeDecl*)alias_node)->target_type_expr)->kind==AST_TYPE_POINTER &&
            function && function->return_type &&
            strcmp(function->return_type, "!*char") == 0 &&
            function->return_type_expr &&
            ((ASTTypeExpr*)function->return_type_expr)->kind==AST_TYPE_QUALIFIED &&
            p && strcmp(p->name, "p") == 0 &&
            strcmp(p->type_name, "*char") == 0 && p->type_expr &&
            ((ASTTypeExpr*)p->type_expr)->kind==AST_TYPE_POINTER &&
            pp && strcmp(pp->name, "pp") == 0 &&
            strcmp(pp->type_name, "!**u8") == 0 && pp->type_expr &&
            ((ASTTypeExpr*)pp->type_expr)->kind==AST_TYPE_QUALIFIED &&
            ref_param && strcmp(ref_param->name, "ref") == 0 &&
            strcmp(ref_param->type_name, "&Point") == 0 && ref_param->type_expr &&
            ((ASTTypeExpr*)ref_param->type_expr)->kind==AST_TYPE_REFERENCE &&
            !ref_param->base.next &&
            local_node && local_node->type == AST_VAR_DECL &&
            strcmp(((ASTVarDecl*)local_node)->type_name, "*char") == 0 &&
            ((ASTVarDecl*)local_node)->type_expr &&
            ret_node && ret_node->type == AST_RETURN &&
            ast_validate((ASTNode*)parser->ast_root, NULL);

        if (valid) {
            stream = tmpfile();
            valid = stream != NULL &&
                ast_serialize((ASTNode*)parser->ast_root, stream) != 0;
            if (valid) {
                rewind(stream);
                decoded = ast_deserialize(stream);
                valid = decoded != NULL && ast_validate(decoded, NULL);
            }
        }
    }
    if (decoded) ast_destroy(decoded);
    if (stream) fclose(stream);
    parser_destroy(parser);
    if (!valid) fprintf(stderr, "Pointer/reference type declarator AST contract failed.\n");
    return valid;
}


static int test_function_type_alias_and_callback_parameter(void) {
    static const char source[] =
        "nt:Callback = f:(id!u32)!bool; "
        "nt:LegacyCallback = f:(id!u32)>!bool; "
        "nt:NestedCallback = f:(test!f:(value!i32)!bool)!bool; "
        "f:apply(callback!f:(id!u32)!bool, id!u32)!bool { ret callback(id); } "
        "nt:ZeroCallback = f:()!bool; "
        "nt:VariadicCallback = f:(first!u32,...)!void; "
        "nt:BufferCallback = f:(buffer!u8[16])!bool; "
        "nt:MatrixCallback = f:(matrix!f32[4][4])!bool; "
        "nt:DynamicArrayCallback = f:(items!u32[])!u32; "
        "nt:ReturnArrayCallback = f:(value!u32)!u8[8];";
    static const char invalid_source[] = "nt:Broken = f:(id!u32)!;";
    ParserContext* parser = parser_init_source(
        "callback-function-type.nxf", (const uint8_t*)source, sizeof(source) - 1u);
    ParserContext* invalid = NULL;
    ASTNode* alias_node = NULL;
    ASTNode* legacy_alias_node = NULL;
    ASTNode* nested_alias_node = NULL;
    ASTNode* function_node = NULL;
    ASTFuncDecl* function = NULL;
    ASTVarDecl* callback_param = NULL;
    ASTVarDecl* id_param = NULL;
    ASTTypeExpr* buffer_type = NULL;
    ASTTypeExpr* buffer_array = NULL;
    ASTArrayType* buffer_dims = NULL;
    ASTArrayDimension* buffer_dim = NULL;
    ASTTypeExpr* matrix_type = NULL;
    ASTTypeExpr* matrix_array = NULL;
    ASTArrayType* matrix_dims = NULL;
    ASTArrayDimension* matrix_dim1 = NULL;
    ASTArrayDimension* matrix_dim2 = NULL;
    ASTTypeExpr* dynamic_type = NULL;
    ASTTypeExpr* dynamic_array = NULL;
    ASTArrayType* dynamic_dims = NULL;
    ASTNode* decoded = NULL;
    FILE* stream = NULL;
    int valid = 0;
    int parse_status = -1;

    if (!parser) return 0;
    parse_status = parser_run(parser);
    if (parse_status == 0 && parser->ast_root &&
        ((ASTNode*)parser->ast_root)->type == AST_PROGRAM) {
        {
            ASTNode* declaration;
            ASTNode* buffer_alias_node = NULL;
            ASTNode* matrix_alias_node = NULL;
            ASTNode* dynamic_alias_node = NULL;
            ASTProgram* program = (ASTProgram*)parser->ast_root;

            for (declaration = program->declarations; declaration;
                 declaration = declaration->next) {
                if (declaration->type == AST_NEWTYPE_DECL) {
                    ASTNewTypeDecl* alias = (ASTNewTypeDecl*)declaration;
                    if (!alias->name) continue;
                    if (strcmp(alias->name, "Callback") == 0) alias_node = declaration;
                    else if (strcmp(alias->name, "LegacyCallback") == 0) legacy_alias_node = declaration;
                    else if (strcmp(alias->name, "NestedCallback") == 0) nested_alias_node = declaration;
                    else if (strcmp(alias->name, "BufferCallback") == 0) buffer_alias_node = declaration;
                    else if (strcmp(alias->name, "MatrixCallback") == 0) matrix_alias_node = declaration;
                    else if (strcmp(alias->name, "DynamicArrayCallback") == 0) dynamic_alias_node = declaration;
                } else if (declaration->type == AST_FUNC_DECL) {
                    ASTFuncDecl* candidate = (ASTFuncDecl*)declaration;
                    if (candidate->name && strcmp(candidate->name, "apply") == 0)
                        function_node = declaration;
                }
            }

            function = function_node ? (ASTFuncDecl*)function_node : NULL;
            callback_param = function ? (ASTVarDecl*)function->params : NULL;
            id_param = callback_param ? (ASTVarDecl*)callback_param->base.next : NULL;

            buffer_type = buffer_alias_node
                ? (ASTTypeExpr*)((ASTNewTypeDecl*)buffer_alias_node)->target_type_expr : NULL;
            matrix_type = matrix_alias_node
                ? (ASTTypeExpr*)((ASTNewTypeDecl*)matrix_alias_node)->target_type_expr : NULL;
            dynamic_type = dynamic_alias_node
                ? (ASTTypeExpr*)((ASTNewTypeDecl*)dynamic_alias_node)->target_type_expr : NULL;
            buffer_array = buffer_type && buffer_type->kind == AST_TYPE_FUNCTION
                ? (ASTTypeExpr*)((ASTVarDecl*)buffer_type->params)->type_expr : NULL;
            matrix_array = matrix_type && matrix_type->kind == AST_TYPE_FUNCTION
                ? (ASTTypeExpr*)((ASTVarDecl*)matrix_type->params)->type_expr : NULL;
            dynamic_array = dynamic_type && dynamic_type->kind == AST_TYPE_FUNCTION
                ? (ASTTypeExpr*)((ASTVarDecl*)dynamic_type->params)->type_expr : NULL;
            if (buffer_array && buffer_array->kind == AST_TYPE_QUALIFIED)
                buffer_array = (ASTTypeExpr*)buffer_array->child;
            if (matrix_array && matrix_array->kind == AST_TYPE_QUALIFIED)
                matrix_array = (ASTTypeExpr*)matrix_array->child;
            if (dynamic_array && dynamic_array->kind == AST_TYPE_QUALIFIED)
                dynamic_array = (ASTTypeExpr*)dynamic_array->child;
            buffer_dims = buffer_array && buffer_array->kind == AST_TYPE_ARRAY
                ? (ASTArrayType*)buffer_array->dimensions : NULL;
            matrix_dims = matrix_array && matrix_array->kind == AST_TYPE_ARRAY
                ? (ASTArrayType*)matrix_array->dimensions : NULL;
            dynamic_dims = dynamic_array && dynamic_array->kind == AST_TYPE_ARRAY
                ? (ASTArrayType*)dynamic_array->dimensions : NULL;
            buffer_dim = buffer_dims ? (ASTArrayDimension*)buffer_dims->dimensions : NULL;
            matrix_dim1 = matrix_dims ? (ASTArrayDimension*)matrix_dims->dimensions : NULL;
            matrix_dim2 = matrix_dim1 ? (ASTArrayDimension*)matrix_dim1->base.next : NULL;
        }

        valid = alias_node && alias_node->type == AST_NEWTYPE_DECL &&
            strcmp(((ASTNewTypeDecl*)alias_node)->target_type,
                "f:(id!u32)!bool") == 0 &&
            ((ASTNewTypeDecl*)alias_node)->target_type_expr &&
            ((ASTTypeExpr*)((ASTNewTypeDecl*)alias_node)->target_type_expr)->kind==AST_TYPE_FUNCTION &&
            ((ASTTypeExpr*)((ASTNewTypeDecl*)alias_node)->target_type_expr)->child &&
            legacy_alias_node && legacy_alias_node->type == AST_NEWTYPE_DECL &&
            strcmp(((ASTNewTypeDecl*)legacy_alias_node)->target_type,
                "f:(id!u32)!bool") == 0 &&
            ((ASTNewTypeDecl*)legacy_alias_node)->target_type_expr &&
            nested_alias_node && nested_alias_node->type == AST_NEWTYPE_DECL &&
            ((ASTNewTypeDecl*)nested_alias_node)->target_type_expr &&
            ((ASTTypeExpr*)((ASTNewTypeDecl*)nested_alias_node)->target_type_expr)->kind==AST_TYPE_FUNCTION &&
            ((ASTVarDecl*)((ASTTypeExpr*)((ASTNewTypeDecl*)nested_alias_node)->target_type_expr)->params)->type_expr &&
            ((ASTTypeExpr*)((ASTVarDecl*)((ASTTypeExpr*)((ASTNewTypeDecl*)nested_alias_node)->target_type_expr)->params)->type_expr)->kind==AST_TYPE_QUALIFIED &&
            ((ASTTypeExpr*)((ASTTypeExpr*)((ASTVarDecl*)((ASTTypeExpr*)((ASTNewTypeDecl*)nested_alias_node)->target_type_expr)->params)->type_expr)->child)->kind==AST_TYPE_FUNCTION &&
            function && function->return_type &&
            strcmp(function->return_type, "!bool") == 0 &&
            callback_param && strcmp(callback_param->name, "callback") == 0 &&
            strcmp(callback_param->type_name, "!f:(id!u32)!bool") == 0 &&
            callback_param->type_expr &&
            ((ASTTypeExpr*)callback_param->type_expr)->kind==AST_TYPE_QUALIFIED &&
            ((ASTTypeExpr*)((ASTTypeExpr*)callback_param->type_expr)->child)->kind==AST_TYPE_FUNCTION &&
            id_param && strcmp(id_param->name, "id") == 0 &&
            strcmp(id_param->type_name, "!u32") == 0 &&
            id_param->type_expr && ((ASTTypeExpr*)id_param->type_expr)->kind==AST_TYPE_QUALIFIED &&
            !id_param->base.next &&
            buffer_type && buffer_type->kind == AST_TYPE_FUNCTION &&
            buffer_array && buffer_array->kind == AST_TYPE_ARRAY &&
            buffer_dims && buffer_dim && buffer_dim->length &&
            ((ASTLiteral*)buffer_dim->length)->int_value == 16 &&
            matrix_type && matrix_array && matrix_array->kind == AST_TYPE_ARRAY &&
            matrix_dim1 && matrix_dim1->length && ((ASTLiteral*)matrix_dim1->length)->int_value == 4 &&
            matrix_dim2 && matrix_dim2->length && ((ASTLiteral*)matrix_dim2->length)->int_value == 4 &&
            dynamic_type && dynamic_array && dynamic_array->kind == AST_TYPE_ARRAY &&
            dynamic_dims && dynamic_dims->dimensions &&
            !((ASTArrayDimension*)dynamic_dims->dimensions)->length &&
            ast_validate((ASTNode*)parser->ast_root, NULL);

        if (valid) {
            stream = tmpfile();
            valid = stream != NULL &&
                ast_serialize((ASTNode*)parser->ast_root, stream) != 0;
            if (valid) {
                rewind(stream);
                decoded = ast_deserialize(stream);
                valid = decoded != NULL && ast_validate(decoded, NULL);
            }
        }
    }

    if (decoded) ast_destroy(decoded);
    if (stream) fclose(stream);
    parser_destroy(parser);
    if (!valid) {
        fprintf(stderr, "Function-type/callback AST contract failed.\\n");
        return 0;
    }

    invalid = parser_init_source(
        "malformed-callback-type.nxf", (const uint8_t*)invalid_source,
        sizeof(invalid_source) - 1u);
    if (!invalid) return 0;
    valid = parser_run(invalid) == PARSER_STATUS_SYNTAX_ERROR &&
        parser_get_status(invalid) == PARSER_STATUS_SYNTAX_ERROR &&
        invalid->error_count > 0;
    parser_destroy(invalid);
    if (!valid) fprintf(stderr, "Malformed callback function type was not rejected.\n");
    return valid;
}



static int test_callback_return_array_and_bad_dimensions(void) {
    static const char source[] = "nt:ReturnArrayCallback = f:(value!u32)!u8[8];";
    static const char* invalid[] = {
        "nt:Bad = f:(value!u32)!u8[abc];",
        "nt:Bad = f:(value!u32)!u8[8;",
        "nt:Bad = f:(value!u32)!u8[9223372036854775808];"
    };
    ParserContext* ctx = parser_init_source("callback-return-array.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTNode* decoded = NULL;
    FILE* stream = NULL;
    int ok = 0;
    if (!ctx) return 0;
    if (parser_run(ctx) == PARSER_STATUS_OK && ctx->ast_root &&
        ast_validate((ASTNode*)ctx->ast_root, NULL)) {
        ASTNewTypeDecl* alias = (ASTNewTypeDecl*)((ASTProgram*)ctx->ast_root)->declarations;
        ASTTypeExpr* fn = alias ? (ASTTypeExpr*)alias->target_type_expr : NULL;
        ASTTypeExpr* ret = fn && fn->kind == AST_TYPE_FUNCTION
            ? (ASTTypeExpr*)fn->child : NULL;
        if (ret && ret->kind == AST_TYPE_QUALIFIED)
            ret = (ASTTypeExpr*)ret->child;
        ASTArrayType* array = ret && ret->kind == AST_TYPE_ARRAY
            ? (ASTArrayType*)ret->dimensions : NULL;
        ASTArrayDimension* dim = array ? (ASTArrayDimension*)array->dimensions : NULL;
        ok = alias && strcmp(alias->name, "ReturnArrayCallback") == 0 &&
            fn && fn->kind == AST_TYPE_FUNCTION && ret && ret->kind == AST_TYPE_ARRAY &&
            dim && dim->length && ((ASTLiteral*)dim->length)->int_value == 8;
        stream = tmpfile();
        ok = ok && stream && ast_serialize((ASTNode*)ctx->ast_root, stream);
        if (ok) {
            rewind(stream);
            decoded = ast_deserialize(stream);
            ok = decoded && ast_validate(decoded, NULL);
            alias = decoded && decoded->type == AST_PROGRAM
                ? (ASTNewTypeDecl*)((ASTProgram*)decoded)->declarations : NULL;
            fn = alias ? (ASTTypeExpr*)alias->target_type_expr : NULL;
            ret = fn && fn->kind == AST_TYPE_FUNCTION ? (ASTTypeExpr*)fn->child : NULL;
            if (ret && ret->kind == AST_TYPE_QUALIFIED)
                ret = (ASTTypeExpr*)ret->child;
            array = ret && ret->kind == AST_TYPE_ARRAY ? (ASTArrayType*)ret->dimensions : NULL;
            dim = array ? (ASTArrayDimension*)array->dimensions : NULL;
            ok = ok && dim && dim->length && ((ASTLiteral*)dim->length)->int_value == 8;
        }
    }
    if (decoded) ast_destroy(decoded);
    if (stream) fclose(stream);
    parser_destroy(ctx);
    if (!ok) return 0;
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        ctx = parser_init_source("callback-bad-array-dimension.nxf",
            (const uint8_t*)invalid[i], strlen(invalid[i]));
        if (!ctx) return 0;
        ok = parser_run(ctx) == PARSER_STATUS_SYNTAX_ERROR &&
            parser_get_status(ctx) == PARSER_STATUS_SYNTAX_ERROR && ctx->error_count > 0;
        parser_destroy(ctx);
        if (!ok) return 0;
    }
    return 1;
}

static int test_callback_parameter_exact_source_spans(void) {
    static const char source[] =
        "nt:SpanCallback = f:(\n"
        "    first!u8[16],\n"
        "    second:i32\n"
        ")!void;";
    ParserContext* parser = parser_init_source(
        "callback-parameter-spans.nxf", (const uint8_t*)source,
        sizeof(source) - 1u);
    ASTNewTypeDecl* alias;
    ASTTypeExpr* function;
    ASTVarDecl* first;
    ASTVarDecl* second;
    const char* first_text = strstr(source, "first!u8[16]");
    const char* second_text = strstr(source, "second:i32");
    int valid = parser && parser_run(parser) == PARSER_STATUS_OK &&
        parser->error_count == 0 && parser->ast_root &&
        ast_validate((ASTNode*)parser->ast_root, NULL);
    if (valid) {
        alias = (ASTNewTypeDecl*)((ASTProgram*)parser->ast_root)->declarations;
        function = alias && alias->target_type_expr
            ? (ASTTypeExpr*)alias->target_type_expr : NULL;
        first = function && function->kind == AST_TYPE_FUNCTION &&
            function->params && function->params->type == AST_VAR_DECL
            ? (ASTVarDecl*)function->params : NULL;
        second = first && first->base.next &&
            first->base.next->type == AST_VAR_DECL
            ? (ASTVarDecl*)first->base.next : NULL;
        valid = alias && alias->name &&
            strcmp(alias->name, "SpanCallback") == 0 &&
            first && second && first_text && second_text &&
            first->base.source_span.has_location &&
            second->base.source_span.has_location &&
            first->base.source_span.start_offset ==
                (uint64_t)(first_text - source) &&
            first->base.source_span.end_offset ==
                (uint64_t)(first_text - source) + strlen("first!u8[16]") &&
            first->base.source_span.start_line == 2u &&
            first->base.source_span.start_column == 5u &&
            second->base.source_span.start_offset ==
                (uint64_t)(second_text - source) &&
            second->base.source_span.end_offset ==
                (uint64_t)(second_text - source) + strlen("second:i32") &&
            second->base.source_span.start_line == 3u &&
            second->base.source_span.start_column == 5u &&
            first->type_expr && first->type_expr->source_span.has_location &&
            first->type_expr->source_span.start_offset ==
                (uint64_t)(strchr(first_text, '!') - source) &&
            first->type_expr->source_span.end_offset ==
                (uint64_t)(first_text - source) + strlen("first!u8[16]");
    }
    parser_destroy(parser);
    return valid;
}

static int test_null_and_numeric_literal_ast(void) {
    static const char source[] =
        "f:main() { "
        "v:p!*u8 = null; "
        "v:a = 0xFF; "
        "v:b = 10E3; "
        "v:c = 1e-3; "
        "v:d = 0b1010; "
        "ret 0; }";
    ParserContext* parser = parser_init_source(
        "null-numeric-literals.nxf", (const uint8_t*)source, sizeof(source) - 1u);
    ASTNode* function_node;
    ASTNode* statement;
    ASTNode* decoded = NULL;
    FILE* stream = NULL;
    const char* expected[] = { "null", "0xFF", "10E3", "1e-3", "0b1010" };
    const OmniTokenType expected_types[] = {
        TOKEN_NULL, TOKEN_NUMBER, TOKEN_NUMBER, TOKEN_NUMBER, TOKEN_NUMBER
    };
    int valid = 0;

    if (!parser) return 0;
    if (parser_run(parser) == 0 && parser->ast_root &&
        ((ASTNode*)parser->ast_root)->type == AST_PROGRAM) {
        function_node = ((ASTProgram*)parser->ast_root)->declarations;
        statement = function_node && function_node->type == AST_FUNC_DECL &&
            ((ASTFuncDecl*)function_node)->body &&
            ((ASTFuncDecl*)function_node)->body->type == AST_BLOCK
            ? ((ASTBlock*)((ASTFuncDecl*)function_node)->body)->statements : NULL;

        valid = function_node && function_node->type == AST_FUNC_DECL;
        for (size_t i = 0u; valid && i < 5u; ++i) {
            ASTVarDecl* declaration = statement && statement->type == AST_VAR_DECL
                ? (ASTVarDecl*)statement : NULL;
            ASTLiteral* literal = declaration && declaration->initializer &&
                declaration->initializer->type == AST_LITERAL
                ? (ASTLiteral*)declaration->initializer : NULL;
            valid = literal && literal->type == expected_types[i] &&
                literal->string_value &&
                strcmp(literal->string_value, expected[i]) == 0;
            statement = statement ? statement->next : NULL;
        }
        valid = valid && statement && statement->type == AST_RETURN &&
            ast_validate((ASTNode*)parser->ast_root, NULL);

        if (valid) {
            stream = tmpfile();
            valid = stream != NULL &&
                ast_serialize((ASTNode*)parser->ast_root, stream) != 0;
            if (valid) {
                rewind(stream);
                decoded = ast_deserialize(stream);
                valid = decoded != NULL && ast_validate(decoded, NULL);
            }
        }
    }
    if (decoded) ast_destroy(decoded);
    if (stream) fclose(stream);
    parser_destroy(parser);
    if (!valid) fprintf(stderr, "Null/radix/scientific numeric AST contract failed.\n");
    return valid;
}


static int test_implicit_type_variable_spelling(void) {
    static const char source[] =
        "f:sum(a!t, b!t)!t { ret a; }";
    ParserContext* parser = parser_init_source(
        "implicit-type-variable.nxf", (const uint8_t*)source, sizeof(source) - 1u);
    ASTNode* function_node;
    ASTFuncDecl* function;
    ASTVarDecl* a;
    ASTVarDecl* b;
    ASTNode* decoded = NULL;
    FILE* stream = NULL;
    int valid = 0;

    if (!parser) return 0;
    if (parser_run(parser) == 0 && parser->ast_root &&
        ((ASTNode*)parser->ast_root)->type == AST_PROGRAM) {
        function_node = ((ASTProgram*)parser->ast_root)->declarations;
        function = function_node && function_node->type == AST_FUNC_DECL
            ? (ASTFuncDecl*)function_node : NULL;
        a = function ? (ASTVarDecl*)function->params : NULL;
        b = a ? (ASTVarDecl*)a->base.next : NULL;
        valid = function && function->return_type &&
            strcmp(function->return_type, "!t") == 0 &&
            a && strcmp(a->name, "a") == 0 &&
            strcmp(a->type_name, "!t") == 0 &&
            b && strcmp(b->name, "b") == 0 &&
            strcmp(b->type_name, "!t") == 0 &&
            !b->base.next &&
            ast_validate((ASTNode*)parser->ast_root, NULL);
        if (valid) {
            stream = tmpfile();
            valid = stream != NULL &&
                ast_serialize((ASTNode*)parser->ast_root, stream) != 0;
            if (valid) {
                rewind(stream);
                decoded = ast_deserialize(stream);
                valid = decoded != NULL && ast_validate(decoded, NULL);
            }
        }
    }
    if (decoded) ast_destroy(decoded);
    if (stream) fclose(stream);
    parser_destroy(parser);
    if (!valid) fprintf(stderr, "Implicit type-variable spelling AST contract failed.\n");
    return valid;
}



static int test_nested_group_and_default_ast(void) {
    static const char source[] =
        "f:main() { group db { nosql => group { open => f:open(path) { ret; } "
        "default { echo(1); } } default { echo(2); } } }";
    ParserContext* parser = parser_init_source("nested-group-default.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTFuncDecl* function;
    ASTGroupDecl* outer;
    ASTGroupDecl* nested;
    ASTMethod* nested_method;
    ASTMethod* nested_default;
    ASTMethod* outer_default;
    FILE* stream = NULL;
    ASTNode* decoded = NULL;
    int valid = 0;
    if (!parser) return 0;
    if (parser_run(parser) == PARSER_STATUS_OK && parser->error_count == 0 &&
        ast_validate(parser->ast_root, NULL)) {
        function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
        outer = function && function->body && function->body->type == AST_BLOCK
            ? (ASTGroupDecl*)((ASTBlock*)function->body)->statements : NULL;
        nested = outer && outer->members && outer->members->type == AST_GROUP_DECL
            ? (ASTGroupDecl*)outer->members : NULL;
        nested_method = nested && nested->members && nested->members->type == AST_METHOD
            ? (ASTMethod*)nested->members : NULL;
        nested_default = nested_method && nested_method->base.next &&
            nested_method->base.next->type == AST_METHOD
            ? (ASTMethod*)nested_method->base.next : NULL;
        outer_default = outer && outer->members && outer->members->next &&
            outer->members->next->type == AST_METHOD
            ? (ASTMethod*)outer->members->next : NULL;
        valid = outer && outer->name && strcmp(outer->name, "db") == 0 &&
            nested && nested->name && strcmp(nested->name, "nosql") == 0 &&
            nested_method && nested_method->public_name &&
            strcmp(nested_method->public_name, "open") == 0 &&
            nested_method->implementation &&
            nested_method->implementation->type == AST_FUNC_DECL &&
            nested_default && nested_default->public_name &&
            strcmp(nested_default->public_name, "default") == 0 &&
            nested_default->implementation &&
            nested_default->implementation->type == AST_BLOCK &&
            outer_default && outer_default->public_name &&
            strcmp(outer_default->public_name, "default") == 0 &&
            outer_default->implementation &&
            outer_default->implementation->type == AST_BLOCK;
    }
    if (valid) {
        stream = tmpfile();
        valid = stream && ast_serialize(parser->ast_root, stream) &&
            fseek(stream, 0L, SEEK_SET) == 0;
        if (valid) decoded = ast_deserialize(stream);
        valid = valid && decoded && ast_validate(decoded, NULL);
    }
    if (stream) fclose(stream);
    if (decoded) ast_destroy(decoded);
    parser_destroy(parser);
    if (!valid) fprintf(stderr, "Nested group/default AST parser+codec test failed.\\n");
    return valid;
}

static int test_enum_payload_rejects_malformed_fields(void) {
    static const char* const invalid_sources[] = {
        "nt:enum: Broken { Value(x) }",
        "nt:enum: Broken { Value(!i32) }",
        "nt:enum: Broken { Value(x!i32,) }",
        "nt:enum: Broken { Value(x!i32, y) }",
        "nt:enum: Broken { Value(x! ) }"
    };
    size_t i;
    for(i=0u;i<sizeof(invalid_sources)/sizeof(invalid_sources[0]);++i) {
        ParserContext* parser=parser_init_source("enum-invalid-payload.nxf",
            (const uint8_t*)invalid_sources[i],strlen(invalid_sources[i]));
        int result;
        if(!parser) return 0;
        result=parser_run(parser);
        if(result==PARSER_STATUS_OK ||
           parser_get_status(parser)!=PARSER_STATUS_SYNTAX_ERROR ||
           parser->error_count==0u) {
            fprintf(stderr,"Malformed Enum payload %lu was not rejected.\\n",
                (unsigned long)i);
            parser_destroy(parser);
            return 0;
        }
        parser_destroy(parser);
    }
    return 1;
}

static int test_no_osmodule_program_directive(void) {
    static const char source[] = "no: osmodule; f:main() {}";
    static const char* const invalid_sources[] = {
        "no: coremodule;",
        "no: osmodule; no: osmodule;",
        "f:main() { no: osmodule; }",
        "no: osmodule"
    };
    const size_t directive_length = sizeof("no: osmodule;") - 1u;
    ParserContext* parser = parser_init_source(
        "no-osmodule-directive.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTProgram* program;
    ASTNode* decoded = NULL;
    FILE* stream = NULL;
    size_t i;
    int valid = 0;

    if (!parser) return 0;
    if (parser_run(parser) != PARSER_STATUS_OK || parser->error_count != 0 ||
        !parser->ast_root || !ast_validate((ASTNode*)parser->ast_root, NULL))
        goto done;
    program = (ASTProgram*)parser->ast_root;
    if (program->base.type != AST_PROGRAM || !program->disable_osmodule ||
        !program->disable_osmodule_span.has_location ||
        program->disable_osmodule_span.start_offset != 0u ||
        program->disable_osmodule_span.end_offset != directive_length ||
        program->disable_osmodule_span.start_line != 1u ||
        program->disable_osmodule_span.start_column != 1u ||
        program->disable_osmodule_span.end_line != 1u ||
        program->disable_osmodule_span.end_column != directive_length + 1u)
        goto done;

    stream = tmpfile();
    if (!stream || !ast_serialize((ASTNode*)program, stream) ||
        fseek(stream, 0L, SEEK_SET) != 0)
        goto done;
    decoded = ast_deserialize(stream);
    if (!decoded || !ast_validate(decoded, NULL) ||
        decoded->type != AST_PROGRAM ||
        !((ASTProgram*)decoded)->disable_osmodule ||
        ((ASTProgram*)decoded)->disable_osmodule_span.start_offset != 0u ||
        ((ASTProgram*)decoded)->disable_osmodule_span.end_offset != directive_length ||
        ((ASTProgram*)decoded)->disable_osmodule_span.end_column != directive_length + 1u)
        goto done;
    valid = 1;

done:
    if (decoded) ast_destroy(decoded);
    if (stream) fclose(stream);
    parser_destroy(parser);
    if (!valid) {
        fprintf(stderr, "no: osmodule AST/codec test failed.\n");
        return 0;
    }

    for (i = 0u; i < sizeof(invalid_sources) / sizeof(invalid_sources[0]); ++i) {
        ParserContext* invalid = parser_init_source(
            "invalid-no-directive.nxf",
            (const uint8_t*)invalid_sources[i], strlen(invalid_sources[i]));
        int rejected;
        if (!invalid) return 0;
        rejected = parser_run(invalid) == PARSER_STATUS_SYNTAX_ERROR &&
            parser_get_status(invalid) == PARSER_STATUS_SYNTAX_ERROR &&
            invalid->error_count > 0;
        parser_destroy(invalid);
        if (!rejected) {
            fprintf(stderr, "Invalid no: directive %lu was accepted.\n",
                (unsigned long)i);
            return 0;
        }
    }
    return 1;
}

static int test_separated_variable_declarations_and_declaration_spans(void) {
    static const char source[] =
        "f:main() {\n"
        "  v: separated!i32 = 7;\n"
        "  c: fixed!u8 = 3;\n"
        "}\n";
    ParserContext* parser = parser_init_source("separated-declarations.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTVarDecl* variable;
    ASTVarDecl* constant;
    const char* function_start = strstr(source, "f:main()");
    const char* function_end = strrchr(source, '}');
    const char* variable_start = strstr(source, "v: separated");
    const char* variable_end = variable_start ? strchr(variable_start, ';') : NULL;
    const char* constant_start = strstr(source, "c: fixed");
    const char* constant_end = constant_start ? strchr(constant_start, ';') : NULL;
    int valid = 0;

    if (!parser) return 0;
    if (parser_run(parser) != PARSER_STATUS_OK || parser->error_count != 0 ||
        !parser->ast_root || !ast_validate(parser->ast_root, NULL))
        goto done;

    function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
    body = function && function->body && function->body->type == AST_BLOCK
        ? (ASTBlock*)function->body : NULL;
    variable = body ? (ASTVarDecl*)body->statements : NULL;
    constant = variable ? (ASTVarDecl*)variable->base.next : NULL;

    valid = function && function->name && strcmp(function->name, "main") == 0 &&
        function_start && function_end &&
        function->base.source_span.has_location &&
        function->base.source_span.start_offset ==
            (uint64_t)(function_start - source) &&
        function->base.source_span.end_offset ==
            (uint64_t)(function_end + 1 - source) &&
        body && body->base.source_span.has_location &&
        variable && variable->name && strcmp(variable->name, "separated") == 0 &&
        variable->type_name && strcmp(variable->type_name, "i32") == 0 &&
        variable->is_hard_typed && !variable->is_const &&
        variable->initializer && variable->initializer->type == AST_LITERAL &&
        variable_start && variable_end &&
        variable->base.source_span.has_location &&
        variable->base.source_span.start_offset ==
            (uint64_t)(variable_start - source) &&
        variable->base.source_span.end_offset ==
            (uint64_t)(variable_end + 1 - source) &&
        variable->base.source_span.start_line == 2u &&
        constant && constant->name && strcmp(constant->name, "fixed") == 0 &&
        constant->type_name && strcmp(constant->type_name, "u8") == 0 &&
        constant->is_hard_typed && constant->is_const &&
        constant->initializer && constant->initializer->type == AST_LITERAL &&
        constant_start && constant_end &&
        constant->base.source_span.has_location &&
        constant->base.source_span.start_offset ==
            (uint64_t)(constant_start - source) &&
        constant->base.source_span.end_offset ==
            (uint64_t)(constant_end + 1 - source) &&
        constant->base.source_span.start_line == 3u &&
        constant->base.next == NULL;

done:
    if (!valid) {
        Error* error;
        fprintf(stderr, "Separated variable declaration/source-span Parser test failed.\\n");
        for (error = parser->error_head; error; error = error->next)
            fprintf(stderr, "  %d:%d %s\\n", error->line, error->col,
                error->message ? error->message : "");
    }
    parser_destroy(parser);
    if (!valid) return 0;

    return parser_source_reports_syntax_error(
        "separated-variable-missing-name.nxf",
        "f:main() { v: !i32 = 1; }",
        sizeof("f:main() { v: !i32 = 1; }") - 1u);
}

static int test_expression_body_function_source_spans(void) {
    static const char source[] = "f:identity(value!i32) > value\n";
    ParserContext* parser = parser_init_source("expression-body-function.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTUnaryExpr* ret;
    const char* expression_start = strstr(source, "> value");
    const char* value_start = expression_start ? strchr(expression_start, ' ') + 1 : NULL;
    const char* value_end = value_start ? value_start + strlen("value") : NULL;
    int valid = 0;

    if (!parser) return 0;
    if (parser_run(parser) == PARSER_STATUS_OK && parser->error_count == 0 &&
        parser->ast_root && ast_validate(parser->ast_root, NULL)) {
        function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
        body = function && function->body && function->body->type == AST_BLOCK
            ? (ASTBlock*)function->body : NULL;
        ret = body && body->statements && body->statements->type == AST_RETURN
            ? (ASTUnaryExpr*)body->statements : NULL;
        valid = function && function->name &&
            strcmp(function->name, "identity") == 0 &&
            function->base.source_span.has_location &&
            function->base.source_span.start_offset == 0u &&
            function->base.source_span.end_offset ==
                (uint64_t)(value_end - source) &&
            body && body->base.source_span.has_location &&
            body->base.source_span.start_offset ==
                (uint64_t)(value_start - source) &&
            body->base.source_span.end_offset ==
                (uint64_t)(value_end - source) &&
            ret && ret->base.source_span.has_location &&
            ret->base.source_span.start_offset ==
                (uint64_t)(value_start - source) &&
            ret->base.source_span.end_offset ==
                (uint64_t)(value_end - source) &&
            ret->operand && ret->operand->type == AST_IDENTIFIER &&
            strcmp(((ASTIdentifier*)ret->operand)->name, "value") == 0;
    }
    if (!valid && parser) {
        Error* error;
        fprintf(stderr, "Expression-body function source-span Parser test failed.\\n");
        for (error = parser->error_head; error; error = error->next)
            fprintf(stderr, "  %d:%d %s\\n", error->line, error->col,
                error->message ? error->message : "");
    }
    if (parser) parser_destroy(parser);
    return valid;
}

static int test_indexed_capture_assignment_ast(void) {
    static const char source[] =
        "f:main() { v:score = notlar[\"Ali\"]; notlar[\"Ali\"]<-score; }";
    ParserContext* parser = parser_init_source("indexed-capture.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTVarDecl* variable;
    ASTBinaryExpr* assignment;
    ASTIndexAccess* target;
    const char* capture_start = strstr(source, "notlar[\"Ali\"]<-");
    const char* capture_end = capture_start
        ? capture_start + strlen("notlar[\"Ali\"]<-score") : NULL;
    int valid = 0;

    if (!parser) return 0;
    if (parser_run(parser) == PARSER_STATUS_OK && parser->error_count == 0 &&
        parser->ast_root && ast_validate(parser->ast_root, NULL)) {
        function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
        body = function && function->body && function->body->type == AST_BLOCK
            ? (ASTBlock*)function->body : NULL;
        variable = body ? (ASTVarDecl*)body->statements : NULL;
        assignment = variable ? (ASTBinaryExpr*)variable->base.next : NULL;
        target = assignment && assignment->left &&
            assignment->left->type == AST_INDEX_ACCESS
                ? (ASTIndexAccess*)assignment->left : NULL;
        valid = variable && variable->initializer &&
            variable->initializer->type == AST_INDEX_ACCESS &&
            assignment && assignment->base.type == AST_ASSIGNMENT &&
            assignment->op == TOKEN_CAPTURE && target && target->target &&
            target->target->type == AST_IDENTIFIER &&
            strcmp(((ASTIdentifier*)target->target)->name, "notlar") == 0 &&
            target->index && target->index->type == AST_LITERAL &&
            assignment->right && assignment->right->type == AST_IDENTIFIER &&
            strcmp(((ASTIdentifier*)assignment->right)->name, "score") == 0 &&
            capture_start && capture_end &&
            assignment->base.source_span.has_location &&
            assignment->base.source_span.start_offset ==
                (uint64_t)(capture_start - source) &&
            assignment->base.source_span.end_offset ==
                (uint64_t)(capture_end - source);
    }
    if (parser) parser_destroy(parser);
    if (!valid) {
        fprintf(stderr, "Indexed capture assignment Parser AST/span test failed.\\n");
        return 0;
    }
    return parser_source_reports_syntax_error(
        "invalid-capture-target.nxf",
        "f:main() { 1 <- score; }",
        sizeof("f:main() { 1 <- score; }") - 1u) == 0;
}

static int test_vector_and_bit_byte_type_parser_coverage(void) {
    static const char source[] =
        "f:main() {\n"
        "  v:vec2!Vec2;\n"
        "  v:vec3!Vec3;\n"
        "  v:vec4!Vec4;\n"
        "  v:enabled!bit;\n"
        "  v:raw!byte;\n"
        "  v:flags8!8bit;\n"
        "  v:flags16!16bit;\n"
        "  v:flags32!32bit;\n"
        "  v:flags64!64bit;\n"
        "  v:flags128!128bit;\n"
        "  v:one!1byte;\n"
        "  v:two!2byte;\n"
        "  v:four!4byte;\n"
        "  v:eight!8byte;\n"
        "  v:sixteen!16byte;\n"
        "}\n";
    static const char* const names[] = {
        "vec2", "vec3", "vec4", "enabled", "raw", "flags8", "flags16",
        "flags32", "flags64", "flags128", "one", "two", "four", "eight",
        "sixteen"
    };
    static const char* const types[] = {
        "Vec2", "Vec3", "Vec4", "bit", "byte", "8bit", "16bit", "32bit",
        "64bit", "128bit", "1byte", "2byte", "4byte", "8byte", "16byte"
    };
    ParserContext* parser = parser_init_source("vector-bit-byte-types.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTNode* node;
    size_t i;
    int valid = 0;

    if (!parser) return 0;
    if (parser_run(parser) != PARSER_STATUS_OK || parser->error_count != 0 ||
        !parser->ast_root || !ast_validate(parser->ast_root, NULL))
        goto done;
    function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
    body = function && function->body && function->body->type == AST_BLOCK
        ? (ASTBlock*)function->body : NULL;
    node = body ? body->statements : NULL;
    valid = body != NULL;
    for (i = 0u; valid && i < sizeof(names) / sizeof(names[0]); ++i) {
        ASTVarDecl* variable = node && node->type == AST_VAR_DECL
            ? (ASTVarDecl*)node : NULL;
        char spelling[64];
        const char* start;
        const char* end;
        int length = snprintf(spelling, sizeof(spelling), "v:%s", names[i]);
        start = length > 0 && (size_t)length < sizeof(spelling)
            ? strstr(source, spelling) : NULL;
        end = start ? strchr(start, ';') : NULL;
        valid = variable && variable->name && strcmp(variable->name, names[i]) == 0 &&
            variable->type_name && strcmp(variable->type_name, types[i]) == 0 &&
            variable->is_hard_typed && !variable->initializer &&
            variable->base.source_span.has_location && start && end &&
            variable->base.source_span.start_offset == (uint64_t)(start - source) &&
            variable->base.source_span.end_offset == (uint64_t)(end + 1 - source);
        node = node ? node->next : NULL;
    }
    valid = valid && node == NULL;
done:
    if (!valid)
        fprintf(stderr, "Vec2/3/4 and bit/byte width type Parser coverage failed.\\n");
    parser_destroy(parser);
    return valid;
}

static int test_copy_intrinsic_uses_generic_call_ast(void) {
    static const char source[] = "f:main() { v:copied = copy(value); }";
    ParserContext* parser = parser_init_source("copy-generic-call.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTFuncDecl* function;
    ASTVarDecl* variable;
    ASTCall* call;
    const char* call_start = strstr(source, "copy(value)");
    const char* call_end = call_start ? call_start + strlen("copy(value)") : NULL;
    int valid = 0;

    if (!parser) return 0;
    if (parser_run(parser) == PARSER_STATUS_OK && parser->error_count == 0 &&
        parser->ast_root && ast_validate(parser->ast_root, NULL)) {
        function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
        variable = function && function->body && function->body->type == AST_BLOCK
            ? (ASTVarDecl*)((ASTBlock*)function->body)->statements : NULL;
        call = variable && variable->initializer &&
            variable->initializer->type == AST_FUNC_CALL
                ? (ASTCall*)variable->initializer : NULL;
        valid = variable && variable->name && strcmp(variable->name, "copied") == 0 &&
            call && call->callee && call->callee->type == AST_IDENTIFIER &&
            strcmp(((ASTIdentifier*)call->callee)->name, "copy") == 0 &&
            call->args && call->args->type == AST_IDENTIFIER &&
            strcmp(((ASTIdentifier*)call->args)->name, "value") == 0 &&
            call_start && call_end && call->base.source_span.has_location &&
            call->base.source_span.start_offset == (uint64_t)(call_start - source) &&
            call->base.source_span.end_offset == (uint64_t)(call_end - source);
    }
    if (parser) parser_destroy(parser);
    if (!valid) fprintf(stderr, "copy() generic-call Parser AST/span test failed.\\n");
    return valid;
}

static int test_string_append_assignment_ast(void) {
    static const char source[] = "f:main() { text .= \"x\"; }";
    const char* append_start = strstr(source, "text .= \"x\"");
    const char* append_end = append_start
        ? append_start + strlen("text .= \"x\"") : NULL;
    ParserContext* ctx = parser_init_source(
        "string-append-assignment.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTBinaryExpr* assignment;
    int ok = 0;

    if (!ctx) return 0;
    if (parser_run(ctx) != PARSER_STATUS_OK ||
        ctx->error_count != 0 || !ctx->ast_root ||
        !ast_validate(ctx->ast_root)) goto done;

    function = (ASTFuncDecl*)((ASTProgram*)ctx->ast_root)->declarations;
    if (!function || function->base.type != AST_FUNC_DECL ||
        !function->body || function->body->type != AST_BLOCK) goto done;
    body = (ASTBlock*)function->body;
    if (!body->statements || body->statements->type != AST_ASSIGNMENT ||
        body->statements->next) goto done;

    assignment = (ASTBinaryExpr*)body->statements;
    if (assignment->op != TOKEN_STRING_APPEND ||
        !assignment->left || assignment->left->type != AST_IDENTIFIER ||
        strcmp(((ASTIdentifier*)assignment->left)->name, "text") != 0 ||
        !assignment->right || assignment->right->type != AST_LITERAL ||
        ((ASTLiteral*)assignment->right)->type != TOKEN_STRING ||
        !append_start || !append_end ||
        !assignment->base.source_span.has_location ||
        assignment->base.source_span.start_offset != (uint64_t)(append_start - source) ||
        assignment->base.source_span.end_offset != (uint64_t)(append_end - source) ||
        assignment->base.source_span.start_column != (uint32_t)(append_start - source) + 1u ||
        assignment->base.source_span.end_column != (uint32_t)(append_end - source) + 1u)
        goto done;
    ok = 1;

done:
    parser_destroy(ctx);
    if (!ok) return 0;

    {
        static const char invalid_source[] = "f:main() { text .= ; }";
        ParserContext* invalid = parser_init_source(
            "string-append-assignment-invalid.nxf",
            (const uint8_t*)invalid_source, sizeof(invalid_source) - 1u);
        int rejected = invalid &&
            (parser_run(invalid) != PARSER_STATUS_OK || invalid->error_count != 0);
        if (invalid) parser_destroy(invalid);
        if (!rejected) return 0;
    }
    return 1;
}

static int test_numeric_and_flow_shift_operators(void) {
    static const char source[] =
        "f:main() { "
        "v:left = a <<< b; "
        "v:right = a >>> b; "
        "v:chain_left = a <<< b <<< c; "
        "v:chain_right = a >>> b >>> c; "
        "v:stream_in = a << b; "
        "v:stream_out = a >> b; "
        "}";
    ParserContext* ctx = parser_init_source(
        "numeric-flow-shift-operators.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTFuncDecl* function;
    ASTNode* statement;
    int seen = 0;
    int ok = 0;

    if (!ctx) return 0;
    if (parser_run(ctx) != PARSER_STATUS_OK ||
        ctx->error_count != 0 || !ctx->ast_root ||
        !ast_validate(ctx->ast_root)) goto done;

    function = (ASTFuncDecl*)((ASTProgram*)ctx->ast_root)->declarations;
    if (!function || !function->body || function->body->type != AST_BLOCK)
        goto done;
    statement = ((ASTBlock*)function->body)->statements;
    while (statement) {
        ASTVarDecl* variable;
        ASTNode* value;
        if (statement->type != AST_VAR_DECL) goto done;
        variable = (ASTVarDecl*)statement;
        value = variable->initializer;
        if (!variable->name || !value) goto done;

        if (strcmp(variable->name, "left") == 0) {
            if (value->type != AST_BINARY_EXPR ||
                ((ASTBinaryExpr*)value)->op != TOKEN_BIT_SHL) goto done;
            seen |= 1;
        } else if (strcmp(variable->name, "right") == 0) {
            if (value->type != AST_BINARY_EXPR ||
                ((ASTBinaryExpr*)value)->op != TOKEN_BIT_SHR) goto done;
            seen |= 2;
        } else if (strcmp(variable->name, "chain_left") == 0) {
            ASTBinaryExpr* root;
            if (value->type != AST_BINARY_EXPR) goto done;
            root = (ASTBinaryExpr*)value;
            if (root->op != TOKEN_BIT_SHL || !root->left ||
                root->left->type != AST_BINARY_EXPR ||
                ((ASTBinaryExpr*)root->left)->op != TOKEN_BIT_SHL) goto done;
            seen |= 4;
        } else if (strcmp(variable->name, "chain_right") == 0) {
            ASTBinaryExpr* root;
            if (value->type != AST_BINARY_EXPR) goto done;
            root = (ASTBinaryExpr*)value;
            if (root->op != TOKEN_BIT_SHR || !root->left ||
                root->left->type != AST_BINARY_EXPR ||
                ((ASTBinaryExpr*)root->left)->op != TOKEN_BIT_SHR) goto done;
            seen |= 8;
        } else if (strcmp(variable->name, "stream_in") == 0) {
            if (value->type != AST_NEXUS_PIPE ||
                ((ASTNexusFlow*)value)->op != TOKEN_LSHIFT) goto done;
            seen |= 16;
        } else if (strcmp(variable->name, "stream_out") == 0) {
            if (value->type != AST_NEXUS_PIPE ||
                ((ASTNexusFlow*)value)->op != TOKEN_RSHIFT) goto done;
            seen |= 32;
        } else {
            goto done;
        }
        statement = statement->next;
    }
    if (seen != 63) goto done;
    ok = 1;

done:
    parser_destroy(ctx);
    if (!ok) return 0;

    {
        enum { OPERATORS = 33 };
        char long_source[7u + OPERATORS * 4u + 2u];
        size_t offset = 0u;
        ParserContext* excessive;
        int rejected;

        memcpy(long_source + offset, "v:x = a", 7u);
        offset += 7u;
        for (int i = 0; i < OPERATORS; ++i) {
            memcpy(long_source + offset, "<<<a", 4u);
            offset += 4u;
        }
        long_source[offset++] = ';';
        excessive = parser_init_source(
            "excessive-shift-chain.nxf",
            (const uint8_t*)long_source, offset);
        if (!excessive) return 0;
        rejected = parser_run(excessive) == PARSER_STATUS_SYNTAX_ERROR &&
            excessive->error_count > 0;
        parser_destroy(excessive);
        if (!rejected) return 0;
    }
    return 1;
}

static int test_increment_decrement_parser_forms(void) {
    static const char source[] = "f:main() { a++; b--; }";
    const char* starts[] = { "a++", "b--" };
    const OmniTokenType operators[] = { TOKEN_INC, TOKEN_DEC };
    ParserContext* ctx = parser_init_source(
        "increment-decrement-parser.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTFuncDecl* function;
    ASTNode* statement;
    size_t i;
    int ok = 0;

    if (!ctx) return 0;
    if (parser_run(ctx) != PARSER_STATUS_OK ||
        ctx->error_count != 0 || !ctx->ast_root ||
        !ast_validate(ctx->ast_root)) goto done;
    function = (ASTFuncDecl*)((ASTProgram*)ctx->ast_root)->declarations;
    if (!function || !function->body || function->body->type != AST_BLOCK)
        goto done;
    statement = ((ASTBlock*)function->body)->statements;
    for (i = 0u; i < sizeof(operators) / sizeof(operators[0]); ++i) {
        const char* end = starts[i] + strlen(starts[i]);
        ASTUnaryExpr* unary;
        if (!statement || statement->type != AST_POSTFIX_EXPR) goto done;
        unary = (ASTUnaryExpr*)statement;
        if (unary->op != operators[i] || !unary->operand ||
            unary->operand->type != AST_IDENTIFIER ||
            !unary->base.source_span.has_location ||
            unary->base.source_span.start_offset != (uint64_t)(starts[i] - source) ||
            unary->base.source_span.end_offset != (uint64_t)(end - source) ||
            unary->base.source_span.start_column != (uint32_t)(starts[i] - source) + 1u ||
            unary->base.source_span.end_column != (uint32_t)(end - source) + 1u)
            goto done;
        statement = statement->next;
    }
    if (statement) goto done;
    ok = 1;

done:
    parser_destroy(ctx);
    if (!ok) return 0;
    if (!parser_source_reports_syntax_error(
        "prefix-increment-invalid.nxf", "f:main() { ++a; }",
        sizeof("f:main() { ++a; }") - 1u)) return 0;
    if (!parser_source_reports_syntax_error(
        "prefix-decrement-invalid.nxf", "f:main() { --b; }",
        sizeof("f:main() { --b; }") - 1u)) return 0;
    return 1;
}

static int test_equality_operator_parser_forms(void) {
    static const char source[] =
        "f:main() { "
        "v:eq = a == b; "
        "v:neq = a != b; "
        "v:strict = a === b; "
        "v:forced = a ==! b; "
        "}";
    const char* expressions[] = {
        "a == b", "a != b", "a === b", "a ==! b"
    };
    const OmniTokenType operators[] = {
        TOKEN_EQ, TOKEN_NEQ, TOKEN_STRICT_EQ, TOKEN_FORCE_EQ
    };
    const char* names[] = { "eq", "neq", "strict", "forced" };
    ParserContext* ctx = parser_init_source(
        "equality-operator-forms.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTFuncDecl* function;
    ASTNode* statement;
    size_t i;
    int ok = 0;

    if (!ctx) return 0;
    if (parser_run(ctx) != PARSER_STATUS_OK ||
        ctx->error_count != 0 || !ctx->ast_root ||
        !ast_validate(ctx->ast_root)) goto done;
    function = (ASTFuncDecl*)((ASTProgram*)ctx->ast_root)->declarations;
    if (!function || !function->body || function->body->type != AST_BLOCK)
        goto done;
    statement = ((ASTBlock*)function->body)->statements;
    for (i = 0u; i < sizeof(operators) / sizeof(operators[0]); ++i) {
        ASTVarDecl* variable;
        ASTBinaryExpr* binary;
        const char* start = strstr(source, expressions[i]);
        const char* end = start ? start + strlen(expressions[i]) : NULL;
        if (!statement || statement->type != AST_VAR_DECL) goto done;
        variable = (ASTVarDecl*)statement;
        if (!variable->name || strcmp(variable->name, names[i]) != 0 ||
            !variable->initializer || variable->initializer->type != AST_BINARY_EXPR)
            goto done;
        binary = (ASTBinaryExpr*)variable->initializer;
        if (binary->op != operators[i] || !binary->left ||
            binary->left->type != AST_IDENTIFIER || !binary->right ||
            binary->right->type != AST_IDENTIFIER || !start || !end ||
            !binary->base.source_span.has_location ||
            binary->base.source_span.start_offset != (uint64_t)(start - source) ||
            binary->base.source_span.end_offset != (uint64_t)(end - source) ||
            binary->base.source_span.start_column != (uint32_t)(start - source) + 1u ||
            binary->base.source_span.end_column != (uint32_t)(end - source) + 1u)
            goto done;
        statement = statement->next;
    }
    if (statement) goto done;
    ok = 1;

done:
    parser_destroy(ctx);
    if (!ok) return 0;
    return parser_source_reports_syntax_error(
        "equality-missing-rhs.nxf", "f:main() { v:x = a === ; }",
        sizeof("f:main() { v:x = a === ; }") - 1u);
}

static int test_relational_and_unary_operator_parser_forms(void) {
    static const char source[] =
        "f:main() { "
        "v:gte = a >= b; "
        "v:lte = a <= b; "
        "v:logical_not = !flag; "
        "v:bitwise_not = ~bits; "
        "}";
    const char* expressions[] = { "a >= b", "a <= b", "!flag", "~bits" };
    const char* names[] = { "gte", "lte", "logical_not", "bitwise_not" };
    const OmniTokenType operators[] = {
        TOKEN_GTE, TOKEN_LTE, TOKEN_NOT, TOKEN_BIT_NOT
    };
    const ASTNodeType node_types[] = {
        AST_BINARY_EXPR, AST_BINARY_EXPR, AST_UNARY_EXPR, AST_UNARY_EXPR
    };
    ParserContext* ctx = parser_init_source(
        "relational-unary-operators.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTFuncDecl* function;
    ASTNode* statement;
    size_t i;
    int ok = 0;

    if (!ctx) return 0;
    if (parser_run(ctx) != PARSER_STATUS_OK ||
        ctx->error_count != 0 || !ctx->ast_root ||
        !ast_validate(ctx->ast_root)) goto done;
    function = (ASTFuncDecl*)((ASTProgram*)ctx->ast_root)->declarations;
    if (!function || !function->body || function->body->type != AST_BLOCK)
        goto done;
    statement = ((ASTBlock*)function->body)->statements;
    for (i = 0u; i < sizeof(operators) / sizeof(operators[0]); ++i) {
        ASTVarDecl* variable;
        ASTNode* expression;
        const char* start = strstr(source, expressions[i]);
        const char* end = start ? start + strlen(expressions[i]) : NULL;
        if (!statement || statement->type != AST_VAR_DECL) goto done;
        variable = (ASTVarDecl*)statement;
        expression = variable->initializer;
        if (!variable->name || strcmp(variable->name, names[i]) != 0 ||
            !expression || expression->type != node_types[i] ||
            !start || !end || !expression->source_span.has_location ||
            expression->source_span.start_offset != (uint64_t)(start - source) ||
            expression->source_span.end_offset != (uint64_t)(end - source) ||
            expression->source_span.start_column != (uint32_t)(start - source) + 1u ||
            expression->source_span.end_column != (uint32_t)(end - source) + 1u)
            goto done;
        if (node_types[i] == AST_BINARY_EXPR) {
            ASTBinaryExpr* binary = (ASTBinaryExpr*)expression;
            if (binary->op != operators[i] || !binary->left || !binary->right ||
                binary->left->type != AST_IDENTIFIER ||
                binary->right->type != AST_IDENTIFIER) goto done;
        } else {
            ASTUnaryExpr* unary = (ASTUnaryExpr*)expression;
            if (unary->op != operators[i] || !unary->operand ||
                unary->operand->type != AST_IDENTIFIER) goto done;
        }
        statement = statement->next;
    }
    if (statement) goto done;
    ok = 1;

done:
    parser_destroy(ctx);
    if (!ok) return 0;
    return parser_source_reports_syntax_error(
        "unary-missing-operand.nxf", "f:main() { v:x = !; }",
        sizeof("f:main() { v:x = !; }") - 1u);
}

static int test_nexus_flow_catch_fallback_ignore_forms(void) {
    static const char source[] =
        "f:main() { "
        "v:caught = operation() ?-> (e) { echo(e); }; "
        "v:fallback = operation() ?=> (data); "
        "v:ignored = operation() !-> continue_flow(); "
        "}";
    const char* spans[] = {
        "operation() ?-> (e) { echo(e); }",
        "operation() ?=> (data",
        "operation() !-> continue_flow()"
    };
    const char* names[] = { "caught", "fallback", "ignored" };
    const OmniTokenType operators[] = {
        TOKEN_ELSE_FLOW, TOKEN_FALLBACK, TOKEN_IGNORE
    };
    ParserContext* ctx = parser_init_source(
        "nexus-flow-error-operators.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTFuncDecl* function;
    ASTNode* statement;
    size_t i;
    int ok = 0;

    if (!ctx) return 0;
    if (parser_run(ctx) != PARSER_STATUS_OK ||
        ctx->error_count != 0 || !ctx->ast_root ||
        !ast_validate(ctx->ast_root)) goto done;
    function = (ASTFuncDecl*)((ASTProgram*)ctx->ast_root)->declarations;
    if (!function || !function->body || function->body->type != AST_BLOCK)
        goto done;
    statement = ((ASTBlock*)function->body)->statements;
    for (i = 0u; i < sizeof(operators) / sizeof(operators[0]); ++i) {
        ASTVarDecl* variable;
        ASTNexusFlow* flow;
        const char* start = strstr(source, spans[i]);
        const char* end = start ? start + strlen(spans[i]) : NULL;
        if (!statement || statement->type != AST_VAR_DECL) goto done;
        variable = (ASTVarDecl*)statement;
        if (!variable->name || strcmp(variable->name, names[i]) != 0 ||
            !variable->initializer || variable->initializer->type != AST_NEXUS_FLOW)
            goto done;
        flow = (ASTNexusFlow*)variable->initializer;
        if (flow->op != operators[i] || !flow->left || !flow->right ||
            !start || !end || !flow->base.source_span.has_location ||
            flow->base.source_span.start_offset != (uint64_t)(start - source) ||
            flow->base.source_span.end_offset != (uint64_t)(end - source))
            goto done;
        if (i == 0u) {
            if (flow->right->type != AST_IDENTIFIER ||
                strcmp(((ASTIdentifier*)flow->right)->name, "e") != 0 ||
                !flow->catch_block || flow->catch_block->type != AST_BLOCK)
                goto done;
        } else if (i == 1u) {
            if (flow->right->type != AST_IDENTIFIER || flow->catch_block)
                goto done;
        } else if (flow->right->type != AST_FUNC_CALL || flow->catch_block) {
            goto done;
        }
        statement = statement->next;
    }
    if (statement) goto done;
    ok = 1;

done:
    parser_destroy(ctx);
    if (!ok) return 0;
    return parser_source_reports_syntax_error(
        "nexus-flow-catch-missing-block.nxf",
        "f:main() { v:x = operation() ?-> (e); }",
        sizeof("f:main() { v:x = operation() ?-> (e); }") - 1u);
}

static int test_legacy_flow_spellings_are_rejected(void) {
    static const char old_if_flow[] = "f:main() { v:x = a =?> b; }";
    static const char old_iterate[] =
        "f:main() { loop(item <-- items) { echo(item); } }";
    static const char current_foreach[] =
        "f:main() { loop(item in items) { echo(item); } }";
    ParserContext* current = parser_init_source(
        "canonical-foreach-in.nxf", (const uint8_t*)current_foreach,
        sizeof(current_foreach) - 1u);
    int accepted = 0;

    if (current) {
        if (parser_run(current) == PARSER_STATUS_OK &&
            current->error_count == 0 && current->ast_root) {
            ASTFuncDecl* function =
                (ASTFuncDecl*)((ASTProgram*)current->ast_root)->declarations;
            ASTLoop* loop = function && function->body &&
                function->body->type == AST_BLOCK
                    ? (ASTLoop*)((ASTBlock*)function->body)->statements : NULL;
            accepted = loop && loop->base.type == AST_LOOP &&
                loop->loop_type == 3 && loop->variable && loop->collection;
        }
        parser_destroy(current);
    }
    if (!accepted) return 0;
    if (!parser_source_reports_syntax_error(
        "legacy-if-flow-operator.nxf", old_if_flow, sizeof(old_if_flow) - 1u))
        return 0;
    return parser_source_reports_syntax_error(
        "legacy-iterate-operator.nxf", old_iterate, sizeof(old_iterate) - 1u);
}

static int test_deferred_intent_ai_syntax_is_rejected(void) {
    static const char intent_source[] = "f:main() { @parallel; }";
    static const char ai_source[] = "f:main() { v:q = ?? \"query\"; }";
    if (!parser_source_reports_syntax_error(
        "deferred-intent.nxf", intent_source, sizeof(intent_source) - 1u))
        return 0;
    return parser_source_reports_syntax_error(
        "deferred-ai-logic.nxf", ai_source, sizeof(ai_source) - 1u);
}

static int test_parenthesized_rolling_retry_disambiguation(void) {
    static const char source[] =
        "f:main() { (source)?(2, 100ms)->target; }";
    ParserContext* ctx = parser_init_source(
        "parenthesized-rolling-retry.nxf",
        (const uint8_t*)source, sizeof(source) - 1u);
    ASTFuncDecl* function;
    ASTBlock* body;
    ASTNexusFlow* flow;
    int ok = 0;

    if (!ctx) return 0;
    if (parser_run(ctx) == PARSER_STATUS_OK && ctx->error_count == 0 &&
        ctx->ast_root) {
        function = (ASTFuncDecl*)((ASTProgram*)ctx->ast_root)->declarations;
        body = function && function->body && function->body->type == AST_BLOCK
            ? (ASTBlock*)function->body : NULL;
        flow = body && body->statements &&
            body->statements->type == AST_NEXUS_FLOW
                ? (ASTNexusFlow*)body->statements : NULL;
        ok = flow && flow->retry_count == 2 && flow->retry_delay == 100 &&
            flow->op == TOKEN_PIPE && flow->left && flow->right &&
            flow->right->type == AST_IDENTIFIER &&
            strcmp(((ASTIdentifier*)flow->right)->name, "target") == 0;
    }
    parser_destroy(ctx);
    return ok;
}

int main(int argc, char** argv) {
    if (!test_fast_exec_raw_asm_ast() || !test_fast_exec_asm_token_stream()) return 1;
    if (!test_main_cli_parameter_ast()) return 1;
    if (!test_m_immutable_value_ast()) return 1;
    if (!test_no_osmodule_program_directive()) return 1;

    if (!test_canonical_loop_and_jump_call_forms()) return 1;
    if (!test_dotted_module_use_path()) return 1;
    if (!test_use_path_malformed_and_exact_spans()) return 1;
    if (!test_use_comma_list_spans()) return 1;
    if (!test_use_alias_and_namespace_ast_codec()) { fprintf(stderr, "USE alias/namespace AST codec test failed.\\n"); return 1; }
    if (!test_pointer_reference_type_declarators()) return 1;
    if (!test_function_type_alias_and_callback_parameter()) return 1;
    if (!test_callback_return_array_and_bad_dimensions()) { fprintf(stderr, "Callback return-array and malformed dimension test failed.\\n"); return 1; }
    if (!test_callback_parameter_exact_source_spans()) { fprintf(stderr, "Callback parameter exact source-span test failed.\\n"); return 1; }
    if (!test_null_and_numeric_literal_ast()) return 1;
    if (!test_implicit_type_variable_spelling()) return 1;
    if (!test_spawn_is_outside_core_parser()) return 1;
    if (!test_intrinsic_ast_family()) return 1;
    if (!test_group_method_ast()) return 1;
    if (!test_nested_group_and_default_ast()) return 1;
    if (!test_thread_block_ast()) return 1;
    if (!test_ast_create_rejects_invalid_node_kinds()) {
        fprintf(stderr, "AST factory accepted an invalid node kind or rejected a boundary kind.\n");
        return 1;
    }
    if (!test_array_type_declarators()) { fprintf(stderr, "Array type declarator parser/codec test failed.\n"); return 1; }
    if (!test_function_array_return_ast()) { fprintf(stderr, "Function array-return parser/codec test failed.\n"); return 1; }
    if (!test_array_index_access_ast()) { fprintf(stderr, "Array index access parser/codec test failed.\n"); return 1; }
    if (!test_power_bitwise_and_compound_assignment()) { fprintf(stderr, "Power/bitwise/compound assignment parser test failed.\n"); return 1; }
    if (!test_string_append_assignment_ast()) {
        fprintf(stderr, "String append assignment AST/source-span parser test failed.\\n");
        return 1;
    }
    if (!test_numeric_and_flow_shift_operators()) {
        fprintf(stderr, "Numeric/Flow shift distinction or shift-chain bound test failed.\\n");
        return 1;
    }
    if (!test_increment_decrement_parser_forms()) {
        fprintf(stderr, "Prefix/postfix increment/decrement AST/source-span test failed.\\n");
        return 1;
    }
    if (!test_equality_operator_parser_forms()) {
        fprintf(stderr, "Equality operator AST/source-span Parser test failed.\\n");
        return 1;
    }
    if (!test_relational_and_unary_operator_parser_forms()) {
        fprintf(stderr, "Relational/unary operator AST/source-span Parser test failed.\\n");
        return 1;
    }
    if (!test_nexus_flow_catch_fallback_ignore_forms()) {
        fprintf(stderr, "Nexus Flow catch/fallback/ignore Parser test failed.\\n");
        return 1;
    }
    if (!test_legacy_flow_spellings_are_rejected()) {
        fprintf(stderr, "Legacy Flow token spellings were not rejected or canonical foreach failed.\\n");
        return 1;
    }
    if (!test_deferred_intent_ai_syntax_is_rejected()) {
        fprintf(stderr, "Deferred Intent/AI syntax was accepted by the Core Parser.\\n");
        return 1;
    }
    if (!test_extern_semicolon_declaration()) { fprintf(stderr, "EXF semicolon declaration parser test failed.\n"); return 1; }
    if (!test_extern_linkname_association_and_codec()) { fprintf(stderr, "EXF linkname association/codec test failed.\n"); return 1; }
    if (!test_extern_echo_and_listen_keywords()) { fprintf(stderr, "extern/echo/listen keyword parser test failed.\n"); return 1; }
    if (!test_modulo_expression_ast()) { fprintf(stderr, "Modulo expression/array extent parser test failed.\n"); return 1; }
    if (!test_expression_precedence_and_associativity_ast()) { fprintf(stderr, "Logical operator precedence parser test failed.\n"); return 1; }

    if (!test_array_literal_ast()) { fprintf(stderr, "Array literal parser/codec test failed.\n"); return 1; }
    if (!test_retry_metadata_rejects_overflow()) {
        fprintf(stderr, "Rolling retry count overflow was not rejected.\n");
        return 1;
    }
    if (!test_rules_lambda_relocate_unroll_ast()) {
        fprintf(stderr, "Rules/lambda/relocate/unroll AST parser test failed.\n");
        return 1;
    }
    if (!test_select_scan_ast()) { fprintf(stderr, "select/scan AST parser and codec test failed.\n"); return 1; }
    if (!test_structured_if_elsif_els_ast()) {
        fprintf(stderr, "Structured if/elsif/els parser test failed.\n");
        return 1;
    }
    if (!test_conditional_control_question_disambiguation()) {
        fprintf(stderr, "Question-mark conditional control was confused with a Nexus pipe.\n");
        return 1;
    }
    if (!test_parenthesized_rolling_retry_disambiguation()) {
        fprintf(stderr, "Parenthesized rolling retry was misclassified as conditional control.\\n");
        return 1;
    }
    if (!test_legacy_unpack_operator_is_rejected()) {
        fprintf(stderr, "Legacy ...> unpack operator was unexpectedly accepted.\\n");
        return 1;
    }
    if (!test_parser_rejects_excessive_expression_nesting()) {
        fprintf(stderr, "Parser did not safely reject excessive expression nesting.\n");
        return 1;
    }
    if (!test_parser_rejects_excessive_operator_chain()) {
        fprintf(stderr, "Parser did not safely reject an excessive operator chain.\n");
        return 1;
    }
    if (!test_parser_bounds_recursive_expression_forms()) {
        fprintf(stderr, "Parser did not bound recursive unary/assignment/postfix forms.\n");
        return 1;
    }
    if (!test_parser_context_initialization()) {
        fprintf(stderr, "Parser context initialization/early destruction test failed.\n");
        return 1;
    }
    if (!test_parser_allocation_failure_injection()) {
        fprintf(stderr, "Parser allocation-failure injection test failed.\n");
        return 1;
    }
    if (!test_function_parameter_type_separator_forms()) {
        fprintf(stderr, "Function parameter type separator forms were not parsed consistently.\n");
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
    if (!test_nested_block_exact_source_spans()) {
        fprintf(stderr, "Nested block exact source-span test failed.\\n");
        return 1;
    }
    if (!test_parser_nested_block_recovery()) {
        fprintf(stderr, "Parser did not recover inside a nested block and preserve following statements.\n");
        return 1;
    }
    if (!test_parser_malformed_function_recovery()) {
        fprintf(stderr, "Parser did not recover after a malformed function declaration.\n");
        return 1;
    }
    if (!test_parser_preserves_lexer_errors_during_recovery()) {
        fprintf(stderr, "Lexer diagnostics were lost during parser panic-mode recovery.\n");
        return 1;
    }
    if (!test_parser_allocation_failure_injection()) {
        fprintf(stderr, "Parser allocation failure injection did not preserve OOM status/cleanup.\n");
        return 1;
    }
    if (!test_struct_literal_parser_source_spans()) {
        fprintf(stderr, "Struct literal parser source-span fixture failed.\n");
        return 1;
    }
    if (!test_enum_payload_rejects_malformed_fields()) {
        fprintf(stderr, "Malformed Enum payload forms were accepted.\\n");
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
            fprintf(stderr, "Lexer diagnostic allocation failure was not propagated as OOM.\n");
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
            fprintf(stderr, "Malformed token source span was accepted.\n");
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
            fprintf(stderr, "Invalid token diagnostic reference suppressed a lexer error.\n");
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
            fprintf(stderr, "Token text that differs from its source span was accepted.\n");
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
                fprintf(stderr, "Parser diagnostic budget exhaustion was not reported distinctly.\n");
                return 1;
            }
        }
    }

    /* Block-scoped }?; is preserved as a distinct final block child. */
    {
        static const char source[] = "f:main() { v:x = 1; }?;";
        ParserContext* parser = parser_init_source("block-halt.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        ASTNode* root;
        ASTNode* fn;
        ASTNode* last;
        ASTNode* body;
        const OccSourceSpan* block_span;
        size_t open_offset = (size_t)(strchr(source, '{') - source);
        const char* halt_offset = strstr(source, "?;");
        if (!parser || parser_run(parser) != PARSER_STATUS_OK) {
            if (parser) parser_destroy(parser);
            fprintf(stderr, "block-scoped Halt syntax failed to parse\n");
            return 1;
        }
        root = parser->ast_root;
        fn = root ? ((ASTProgram*)root)->declarations : NULL;
        body = fn ? ((ASTFuncDecl*)fn)->body : NULL;
        block_span = body ? &body->source_span : NULL;
        last = body ? ((ASTBlock*)body)->statements : NULL;
        while (last && last->next) last = last->next;
        if (!last || last->type != AST_BLOCK_HALT ||
            !last->source_span.has_location ||
            last->source_span.start_offset != (uint64_t)(halt_offset - source) ||
            last->source_span.end_offset != sizeof(source) - 1u ||
            !block_span || !block_span->has_location ||
            block_span->start_offset != open_offset ||
            block_span->end_offset != sizeof(source) - 1u ||
            block_span->start_line != 1u ||
            block_span->start_column != open_offset + 1u ||
            block_span->end_line != 1u ||
            block_span->end_column != sizeof(source) ||
            !ast_validate(root, NULL)) {
            fprintf(stderr, "block-scoped Halt was not preserved as final AST child\n");
            parser_destroy(parser);
            return 1;
        }
        parser_destroy(parser);
    }

    /* Variadic declarations and named call arguments share the AST/codec contract. */
    {
        static const char source[] =
            "f:many(a!i32, ...) { ret a; } "
            "f:main() { many(1, b:2); }";
        ParserContext* parser = parser_init_source("variadic-named-args.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        ASTProgram* program;
        ASTFuncDecl* many;
        ASTFuncDecl* main_fn;
        ASTNode* call_node;
        ASTCall* call;
        const char* named_start;
        const char* call_start=strstr(source,"many(1, b:2)");
        const char* variadic_start=strstr(source,"...");
        ASTNamedArg* named_arg;
        ASTNode* decoded = NULL;
        FILE* stream = NULL;
        int valid = 0;
        if (!parser || parser_run(parser) != PARSER_STATUS_OK ||
            parser_get_status(parser) != PARSER_STATUS_OK || !parser->ast_root)
            goto named_args_done;
        program = (ASTProgram*)parser->ast_root;
        many = (ASTFuncDecl*)program->declarations;
        main_fn = many ? (ASTFuncDecl*)many->base.next : NULL;
        if (!many || many->base.type != AST_FUNC_DECL ||
            !many->params || many->params->type != AST_VAR_DECL ||
            !many->params->next || many->params->next->type != AST_VARIADIC_PARAM ||
            many->params->next->next || !main_fn || main_fn->base.type != AST_FUNC_DECL)
            goto named_args_done;
        call_node = ((ASTBlock*)main_fn->body)->statements;
        call = (ASTCall*)call_node;
        named_start=strstr(source,"b:2");
        if (!call_node || call_node->type != AST_FUNC_CALL || !call->args ||
            call->args->type != AST_LITERAL || !call->args->next ||
            call->args->next->type != AST_NAMED_ARG || !named_start)
            goto named_args_done;
        named_arg=(ASTNamedArg*)call->args->next;
        if(!call_start || !call_node->source_span.has_location ||
           !many->params->next->source_span.has_location ||
           call_node->source_span.start_offset!=(uint64_t)(call_start-source) ||
           call_node->source_span.end_offset!=(uint64_t)(call_start+strlen("many(1, b:2)")-source) ||
           call_node->source_span.start_column!=(uint32_t)(call_start-source)+1u ||
           call_node->source_span.end_column!=(uint32_t)(call_start+strlen("many(1, b:2)")-source)+1u ||
           !variadic_start || many->params->next->source_span.start_offset!=(uint64_t)(variadic_start-source) ||
           many->params->next->source_span.end_offset!=(uint64_t)(variadic_start+3-source) ||
           many->params->next->source_span.start_column!=(uint32_t)(variadic_start-source)+1u ||
           many->params->next->source_span.end_column!=(uint32_t)(variadic_start+3-source)+1u ||
           strcmp(named_arg->name,"b") != 0 ||
            !named_arg->base.source_span.has_location ||
            named_arg->base.source_span.start_offset != (uint64_t)(named_start-source) ||
            named_arg->base.source_span.end_offset != (uint64_t)(named_start-source)+3u ||
            named_arg->base.source_span.start_line != 1u ||
            named_arg->base.source_span.start_column != (uint32_t)(named_start-source)+1u ||
            named_arg->base.source_span.end_line != 1u ||
            named_arg->base.source_span.end_column != (uint32_t)(named_start-source)+4u ||
            !ast_validate(parser->ast_root, NULL))
            goto named_args_done;
        stream = tmpfile();
        if (!stream || !ast_serialize(parser->ast_root, stream) ||
            fseek(stream, 0, SEEK_SET) != 0)
            goto named_args_done;
        decoded = ast_deserialize(stream);
        if (!decoded || !ast_validate(decoded, NULL))
            goto named_args_done;
        valid = 1;
named_args_done:
        if (stream) fclose(stream);
        if (decoded) ast_destroy(decoded);
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Variadic/named argument parser or AST codec test failed.\n");
            return 1;
        }
    }

    /* Parser rejects duplicate labels, positional-after-named and missing values. */
    {
        static const char* invalid_sources[] = {
            "f:main() { target(a:1, a:2); }",
            "f:main() { target(a:1, 2); }",
            "f:main() { target(a:); }"
        };
        size_t index;
        for (index = 0u; index < sizeof(invalid_sources) / sizeof(invalid_sources[0]); ++index) {
            ParserContext* parser = parser_init_source("invalid-named-args.nxf",
                (const uint8_t*)invalid_sources[index], strlen(invalid_sources[index]));
            int status;
            if (!parser) return 1;
            status = parser_run(parser);
            if (status != PARSER_STATUS_SYNTAX_ERROR ||
                parser_get_status(parser) != PARSER_STATUS_SYNTAX_ERROR) {
                fprintf(stderr, "Invalid named-argument form was accepted: %s\n",
                    invalid_sources[index]);
                parser_destroy(parser);
                return 1;
            }
            parser_destroy(parser);
        }
    }

    /* Immutable m: values remain a distinct AST declaration, not a USE import. */
    {
        static const char source[] =
            "m:CoreValue = 5; "
            "f:main() { ret 0; }";
        ParserContext* parser = parser_init_source("immutable-value.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        ASTNode* decoded = NULL;
        FILE* stream = NULL;
        int valid = 0;
        if (!parser) goto immutable_value_done;
        {
            int parse_status = parser_run(parser);
            if (parse_status != PARSER_STATUS_OK || !parser->ast_root) {
                fprintf(stderr, "m: parse status=%d errors=%d\n",
                    parse_status, parser->error_count);
                if (parser->error_head && parser->error_head->message)
                    fprintf(stderr, "m: parser diagnostic: %s\n",
                        parser->error_head->message);
                goto immutable_value_done;
            }
        }
        {
            ASTValidationError validation;
            if (!ast_validate(parser->ast_root, &validation)) {
                fprintf(stderr, "m: AST validation failed: code=%d field=%s\n",
                    (int)validation.code, validation.field ? validation.field : "<none>");
                goto immutable_value_done;
            }
        }
        {
            ASTProgram* program = (ASTProgram*)parser->ast_root;
            ASTMustDecl* must = (ASTMustDecl*)program->declarations;
            if (!must || must->base.type != AST_MUST_DECL ||
                strcmp(must->name, "CoreValue") != 0 ||
                !must->source || must->source->type != AST_LITERAL ||
                strcmp(((ASTLiteral*)must->source)->string_value, "5") != 0 ||
                !must->base.next || must->base.next->type != AST_FUNC_DECL)
                goto immutable_value_done;
        }
        stream = tmpfile();
        if (!stream || !ast_serialize(parser->ast_root, stream) ||
            fseek(stream, 0, SEEK_SET) != 0)
            goto immutable_value_done;
        decoded = ast_deserialize(stream);
        if (!decoded || !ast_validate(decoded, NULL) ||
            ((ASTProgram*)decoded)->declarations->type != AST_MUST_DECL)
            goto immutable_value_done;
        valid = 1;
immutable_value_done:
        if (stream) fclose(stream);
        if (decoded) ast_destroy(decoded);
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "m: immutable-value parser/AST codec test failed.\n");
            return 1;
        }
    }

    /* Generic type-variable spellings are preserved losslessly in signature strings. */
    {
        static const char source[] =
            "f:sum(a!t, b!t)!t { ret a; }";
        ParserContext* parser = parser_init_source("generic-type-variable.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        ASTFuncDecl* function;
        ASTNode* decoded = NULL;
        FILE* stream = NULL;
        int valid = 0;
        if (!parser || parser_run(parser) != PARSER_STATUS_OK ||
            !parser->ast_root || !ast_validate(parser->ast_root, NULL))
            goto generic_type_done;
        function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
        if (!function || function->base.type != AST_FUNC_DECL ||
            !function->params || !function->params->next ||
            strcmp(((ASTVarDecl*)function->params)->type_name, "!t") != 0 ||
            strcmp(((ASTVarDecl*)function->params->next)->type_name, "!t") != 0 ||
            !function->return_type || strcmp(function->return_type, "!t") != 0)
            goto generic_type_done;
        stream = tmpfile();
        if (!stream || !ast_serialize(parser->ast_root, stream) ||
            fseek(stream, 0, SEEK_SET) != 0)
            goto generic_type_done;
        decoded = ast_deserialize(stream);
        if (!decoded || !ast_validate(decoded, NULL))
            goto generic_type_done;
        function = (ASTFuncDecl*)((ASTProgram*)decoded)->declarations;
        if (!function || strcmp(function->return_type, "!t") != 0)
            goto generic_type_done;
        valid = 1;
generic_type_done:
        if (stream) fclose(stream);
        if (decoded) ast_destroy(decoded);
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Generic type-variable signature preservation test failed.\n");
            return 1;
        }
    }

    /* Parser diagnostics retain the registered file identity in multi-file jobs. */
    {
        static const char bad_source[] = "v:x = ;";
        ParserContext* parser = parser_init_source("lib/math.nxf",
            (const uint8_t*)bad_source, sizeof(bad_source) - 1u);
        OccSourceFileTable files;
        OccDiagnosticCollector diagnostics;
        OccDiagnosticView view;
        ASTProgram* program = NULL;
        int valid = 0;
        occ_source_files_init(&files);
        occ_diagnostics_init(&diagnostics);
        if (parser) {
            uint32_t expected_id = UINT32_MAX;
            if (occ_source_files_register(&files, "src/main.nxf", &expected_id) &&
                expected_id == 0u &&
                parser_attach_source_files(parser, &files) &&
                parser->source_id == 1u) {
                parser_attach_diagnostics(parser, &diagnostics, OCC_STAGE_PARSER);
                (void)parser_run(parser);
                program = (ASTProgram*)parser->ast_root;
                if (occ_diagnostics_count(&diagnostics) > 0u &&
                    occ_diagnostics_get(&diagnostics, 0u, &view) &&
                    view.record->span.source_id == 1u &&
                    program && program->base.source_span.has_location == 1u &&
                    program->base.source_span.source_id == 1u &&
                    program->base.source_span.start_offset == 0u &&
                    program->base.source_span.end_offset == sizeof(bad_source) - 1u)
                    valid = 1;
            }
            parser_destroy(parser);
        }
        occ_diagnostics_destroy(&diagnostics);
        occ_source_files_destroy(&files);
        if (!valid) {
            fprintf(stderr, "Parser diagnostic source identity test failed.\n");
            return 1;
        }
    }


    /* Parser leaf nodes retain exact token spans and the compilation source ID. */
    {
        static const char source[] = "f:main() { v:x = 42 + 7; ret x; }";
        ParserContext* parser = parser_init_source("leaf-spans.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        OccSourceFileTable files;
        uint32_t first_id = UINT32_MAX;
        size_t function_offset = (size_t)(strstr(source, "f:main") - source);
        size_t declaration_offset = (size_t)(strstr(source, "v:x") - source);
        size_t literal_offset = (size_t)(strstr(source, "42") - source);
        const char* return_identifier = strstr(source, "ret x");
        size_t return_offset = return_identifier
            ? (size_t)(return_identifier - source) : 0u;
        size_t identifier_offset = return_identifier
            ? (size_t)(return_identifier + 4 - source) : 0u;
        size_t right_literal_offset = (size_t)(strstr(source, "7;") - source);
        int valid = 0;
        occ_source_files_init(&files);
        if (parser &&
            occ_source_files_register(&files, "src/other.nxf", &first_id) &&
            first_id == 0u &&
            parser_attach_source_files(parser, &files) &&
            parser->source_id == 1u &&
            parser_run(parser) == PARSER_STATUS_OK) {
            ASTFuncDecl* function =
                (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            ASTNode* statements = function
                ? ((ASTBlock*)function->body)->statements : NULL;
            ASTVarDecl* variable = (ASTVarDecl*)statements;
            ASTNode* return_node = statements ? statements->next : NULL;
            ASTNode* identifier = return_node
                ? ((ASTUnaryExpr*)return_node)->operand : NULL;
            ASTBinaryExpr* binary = variable && variable->initializer &&
                variable->initializer->type == AST_BINARY_EXPR
                    ? (ASTBinaryExpr*)variable->initializer : NULL;
            const OccSourceSpan* literal_span = binary && binary->left
                ? &binary->left->source_span : NULL;
            const OccSourceSpan* identifier_span =
                identifier ? &identifier->source_span : NULL;
            const OccSourceSpan* return_span =
                return_node ? &return_node->source_span : NULL;
            const OccSourceSpan* binary_span = binary
                ? &binary->base.source_span : NULL;
            const OccSourceSpan* declaration_span = variable
                ? &variable->base.source_span : NULL;
            const OccSourceSpan* function_span = function
                ? &function->base.source_span : NULL;
            valid = variable && variable->base.type == AST_VAR_DECL &&
                binary && binary->left && binary->right &&
                binary->left->type == AST_LITERAL && binary->right->type == AST_LITERAL &&
                literal_span && literal_span->has_location == 1u &&
                literal_span->source_id == 1u &&
                literal_span->start_offset == literal_offset &&
                literal_span->end_offset == literal_offset + 2u &&
                literal_span->start_line == 1u &&
                literal_span->start_column == literal_offset + 1u &&
                literal_span->end_column == literal_offset + 3u &&
                binary_span && binary_span->has_location == 1u &&
                binary_span->source_id == 1u &&
                binary_span->start_offset == literal_offset &&
                binary_span->end_offset == right_literal_offset + 1u &&
                binary_span->start_column == literal_offset + 1u &&
                binary_span->end_column == right_literal_offset + 2u &&
                declaration_span && declaration_span->has_location == 1u &&
                declaration_span->source_id == 1u &&
                declaration_span->start_offset == declaration_offset &&
                declaration_span->end_offset == right_literal_offset + 2u &&
                declaration_span->start_column == declaration_offset + 1u &&
                function_span && function_span->has_location == 1u &&
                function_span->source_id == 1u &&
                function_span->start_offset == function_offset &&
                function_span->end_offset == sizeof(source) - 1u &&
                function_span->start_column == function_offset + 1u &&
                function_span->end_column == sizeof(source) &&
                return_node && return_node->type == AST_RETURN &&
                return_span && return_span->has_location == 1u &&
                return_span->source_id == 1u &&
                return_span->start_offset == return_offset &&
                return_span->end_offset == identifier_offset + 2u &&
                return_span->start_column == return_offset + 1u &&
                return_span->end_column == identifier_offset + 3u &&
                identifier && identifier->type == AST_IDENTIFIER &&
                identifier_span && identifier_span->has_location == 1u &&
                identifier_span->source_id == 1u &&
                identifier_span->start_offset == identifier_offset &&
                identifier_span->end_offset == identifier_offset + 1u &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        occ_source_files_destroy(&files);
        if (!valid) {
            fprintf(stderr, "Parser leaf AST source-span test failed.\n");
            return 1;
        }
    }


    /* Prefix unary expressions span the operator and their operand. */
    {
        static const char source[] = "f:main() { v:x = -42; }";
        ParserContext* parser = parser_init_source("unary-span.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        size_t operator_offset = (size_t)(strstr(source, "-42") - source);
        ASTFuncDecl* function;
        ASTVarDecl* variable;
        ASTUnaryExpr* unary;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            variable = function
                ? (ASTVarDecl*)((ASTBlock*)function->body)->statements : NULL;
            unary = variable && variable->initializer &&
                variable->initializer->type == AST_UNARY_EXPR
                    ? (ASTUnaryExpr*)variable->initializer : NULL;
            valid = unary && unary->base.source_span.has_location == 1u &&
                unary->base.source_span.start_offset == operator_offset &&
                unary->base.source_span.end_offset == operator_offset + 3u &&
                unary->base.source_span.start_column == operator_offset + 1u &&
                unary->base.source_span.end_column == operator_offset + 4u &&
                unary->operand && unary->operand->type == AST_LITERAL &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Unary expression source-span test failed.\n");
            return 1;
        }
    }


    /* Postfix index, call and named-argument nodes preserve full production spans. */
    {
        static const char source[] = "f:main() { v:x = arr[1]; foo(a:2); x++; }";
        ParserContext* parser = parser_init_source("postfix-spans.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        size_t array_offset = (size_t)(strstr(source, "arr") - source);
        size_t index_end = (size_t)(strchr(strstr(source, "arr"), ']') + 1 - source);
        size_t call_offset = (size_t)(strstr(source, "foo") - source);
        size_t call_end = (size_t)(strchr(strstr(source, "foo"), ')') + 1 - source);
        size_t named_offset = (size_t)(strstr(source, "a:2") - source);
        size_t named_end = named_offset + 3u;
        size_t postfix_offset = (size_t)(strrchr(source, 'x') - source);
        ASTFuncDecl* function;
        ASTNode* statement;
        ASTIndexAccess* index;
        ASTCall* call;
        ASTNamedArg* named;
        ASTUnaryExpr* postfix;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            statement = function
                ? ((ASTBlock*)function->body)->statements : NULL;
            index = statement && statement->type == AST_VAR_DECL &&
                ((ASTVarDecl*)statement)->initializer &&
                ((ASTVarDecl*)statement)->initializer->type == AST_INDEX_ACCESS
                    ? (ASTIndexAccess*)((ASTVarDecl*)statement)->initializer : NULL;
            statement = statement ? statement->next : NULL;
            call = statement && statement->type == AST_FUNC_CALL
                ? (ASTCall*)statement : NULL;
            named = call && call->args && call->args->type == AST_NAMED_ARG
                ? (ASTNamedArg*)call->args : NULL;
            statement = statement ? statement->next : NULL;
            postfix = statement && statement->type == AST_POSTFIX_EXPR
                ? (ASTUnaryExpr*)statement : NULL;
            valid = index && index->base.source_span.has_location == 1u &&
                index->base.source_span.start_offset == array_offset &&
                index->base.source_span.end_offset == index_end &&
                index->base.source_span.start_column == array_offset + 1u &&
                index->base.source_span.end_column == index_end + 1u &&
                call && call->base.source_span.has_location == 1u &&
                call->base.source_span.start_offset == call_offset &&
                call->base.source_span.end_offset == call_end &&
                named && named->base.source_span.has_location == 1u &&
                named->base.source_span.start_offset == named_offset &&
                named->base.source_span.end_offset == named_end &&
                postfix && postfix->base.source_span.has_location == 1u &&
                postfix->base.source_span.start_offset == postfix_offset &&
                postfix->base.source_span.end_offset == postfix_offset + 3u &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Postfix/call source-span test failed.\n");
            return 1;
        }
    }


    /* Conditional AST spans include the full then/else production. */
    {
        static const char source[] =
            "f:main() { if (true) { ret 1; } els { ret 0; } }";
        ParserContext* parser = parser_init_source("if-span.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* keyword = strstr(source, "if");
        const char* else_close = strstr(source, "ret 0; }");
        size_t start_offset = keyword ? (size_t)(keyword - source) : 0u;
        size_t end_offset = else_close ? (size_t)(else_close + 8 - source) : 0u;
        ASTFuncDecl* function;
        ASTNode* if_node;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            if_node = function ? ((ASTBlock*)function->body)->statements : NULL;
            valid = if_node && if_node->type == AST_IF &&
                if_node->source_span.has_location == 1u &&
                if_node->source_span.start_offset == start_offset &&
                if_node->source_span.end_offset == end_offset &&
                if_node->source_span.start_column == start_offset + 1u &&
                if_node->source_span.end_column == end_offset + 1u &&
                ((ASTIf*)if_node)->then_branch && ((ASTIf*)if_node)->else_branch &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Conditional AST source-span test failed.\n");
            return 1;
        }
    }


    /* Array literal and loop expressions span their complete delimiters/body. */
    {
        static const char source[] =
            "f:main() { v:a = [1,2]; (loop) -> { break; } }";
        ParserContext* parser = parser_init_source("array-loop-spans.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* array_start = strstr(source, "[1,2]");
        const char* loop_start = strstr(source, "(loop)");
        const char* loop_end = strstr(source, "break; }");
        size_t array_offset = array_start ? (size_t)(array_start - source) : 0u;
        size_t array_end = array_start ? (size_t)(array_start + 5 - source) : 0u;
        size_t loop_offset = loop_start ? (size_t)(loop_start - source) : 0u;
        size_t loop_end_offset = loop_end ? (size_t)(loop_end + 8 - source) : 0u;
        ASTFuncDecl* function;
        ASTNode* statement;
        ASTArrayLiteral* array;
        ASTLoop* loop;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            statement = function ? ((ASTBlock*)function->body)->statements : NULL;
            array = statement && statement->type == AST_VAR_DECL &&
                ((ASTVarDecl*)statement)->initializer &&
                ((ASTVarDecl*)statement)->initializer->type == AST_ARRAY_LITERAL
                    ? (ASTArrayLiteral*)((ASTVarDecl*)statement)->initializer : NULL;
            statement = statement ? statement->next : NULL;
            loop = statement && statement->type == AST_LOOP
                ? (ASTLoop*)statement : NULL;
            valid = array && array->base.source_span.has_location == 1u &&
                array->base.source_span.start_offset == array_offset &&
                array->base.source_span.end_offset == array_end &&
                loop && loop->base.source_span.has_location == 1u &&
                loop->base.source_span.start_offset == loop_offset &&
                loop->base.source_span.end_offset == loop_end_offset &&
                loop->body && ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Array/loop AST source-span test failed.\n");
            return 1;
        }
    }


    /* Nexus Flow operators span both operands, including the operator token. */
    {
        static const char source[] = "f:main() { v:x = 1 -> 2; }";
        ParserContext* parser = parser_init_source("flow-span.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        size_t left_offset = (size_t)(strstr(source, "1 ->") - source);
        size_t right_end = (size_t)(strstr(source, "2;") - source) + 1u;
        ASTFuncDecl* function;
        ASTVarDecl* variable;
        ASTNexusFlow* pipe;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            variable = function
                ? (ASTVarDecl*)((ASTBlock*)function->body)->statements : NULL;
            pipe = variable && variable->initializer &&
                variable->initializer->type == AST_NEXUS_PIPE
                    ? (ASTNexusFlow*)variable->initializer : NULL;
            valid = pipe && pipe->base.source_span.has_location == 1u &&
                pipe->base.source_span.start_offset == left_offset &&
                pipe->base.source_span.end_offset == right_end &&
                pipe->left && pipe->right &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Nexus Flow AST source-span test failed.\n");
            return 1;
        }
    }


    /* Select/scan roots, arms and constructor/binding patterns carry source spans. */
    {
        static const char source[] =
            "f:main() { select(x) { 1 -> ret 2; default -> ret 0; } }";
        ParserContext* parser = parser_init_source("select-spans.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* select_start = strstr(source, "select");
        const char* first_pattern = strstr(source, "1 ->");
        const char* first_end = strstr(source, "ret 2;");
        const char* default_start = strstr(source, "default");
        const char* default_end = strstr(source, "ret 0;");
        size_t select_offset = select_start ? (size_t)(select_start - source) : 0u;
        size_t select_end = default_end ? (size_t)(default_end + 8 - source) : 0u;
        ASTFuncDecl* function;
        ASTNode* select_node;
        ASTNode* first_arm;
        ASTNode* default_arm;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            select_node = function ? ((ASTBlock*)function->body)->statements : NULL;
            first_arm = select_node && select_node->type == AST_SELECT
                ? ((ASTSelect*)select_node)->arms : NULL;
            default_arm = first_arm ? first_arm->next : NULL;
            valid = select_node && select_node->type == AST_SELECT &&
                select_node->source_span.has_location == 1u &&
                select_node->source_span.start_offset == select_offset &&
                select_node->source_span.end_offset == select_end &&
                first_arm && first_arm->type == AST_SELECT_ARM &&
                first_arm->source_span.has_location == 1u &&
                first_arm->source_span.start_offset == (size_t)(first_pattern - source) &&
                first_arm->source_span.end_offset == (size_t)(first_end + 6 - source) &&
                default_arm && default_arm->type == AST_SELECT_ARM &&
                default_arm->source_span.has_location == 1u &&
                default_arm->source_span.start_offset == (size_t)(default_start - source) &&
                default_arm->source_span.end_offset == (size_t)(default_end + 6 - source) &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Select AST source-span test failed.\n");
            return 1;
        }
    }


    /* Array declarator dimensions preserve each bracketed source range. */
    {
        static const char source[] = "f:main() { v:a[4][8]!i32; }";
        ParserContext* parser = parser_init_source("array-type-spans.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* first = strstr(source, "[4]");
        const char* second = strstr(source, "[8]");
        ASTFuncDecl* function;
        ASTVarDecl* variable;
        ASTArrayType* array_type;
        ASTNode* dimension;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            variable = function
                ? (ASTVarDecl*)((ASTBlock*)function->body)->statements : NULL;
            array_type = variable && variable->inline_type &&
                variable->inline_type->type == AST_ARRAY_TYPE
                    ? (ASTArrayType*)variable->inline_type : NULL;
            dimension = array_type ? array_type->dimensions : NULL;
            valid = array_type && dimension &&
                dimension->type == AST_ARRAY_DIMENSION &&
                dimension->source_span.has_location == 1u &&
                dimension->source_span.start_offset == (size_t)(first - source) &&
                dimension->source_span.end_offset == (size_t)(first + 3 - source) &&
                dimension->next && dimension->next->type == AST_ARRAY_DIMENSION &&
                dimension->next->source_span.has_location == 1u &&
                dimension->next->source_span.start_offset == (size_t)(second - source) &&
                dimension->next->source_span.end_offset == (size_t)(second + 3 - source) &&
                array_type->base.source_span.has_location == 1u &&
                array_type->base.source_span.start_offset == (size_t)(first - source) &&
                array_type->base.source_span.end_offset == (size_t)(second + 3 - source) &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Array type/dimension source-span test failed.\n");
            return 1;
        }
    }


    /* Explicit Nexus jump statements span their operator through semicolon. */
    {
        static const char source[] = "f:main() { ?>(target); }";
        ParserContext* parser = parser_init_source("jump-span.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* jump = strstr(source, "?>");
        size_t start_offset = jump ? (size_t)(jump - source) : 0u;
        ASTFuncDecl* function;
        ASTNode* node = NULL;
        int parse_status = -1;
        int valid = 0;
        if (parser) parse_status = parser_run(parser);
        if (parser && parse_status == PARSER_STATUS_OK && parser->ast_root) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            node = function ? ((ASTBlock*)function->body)->statements : NULL;
            valid = node && node->type == AST_NEXUS_JUMP &&
                node->source_span.has_location == 1u &&
                node->source_span.start_offset == start_offset &&
                node->source_span.end_offset == start_offset + 11u &&
                node->source_span.start_column == start_offset + 1u &&
                node->source_span.end_column == start_offset + 12u &&
                ast_validate(parser->ast_root, NULL);
        }
        if (!valid && parser) {
            fprintf(stderr, "jump diagnostic: status=%d errors=%d node=%d has=%u start=%llu end=%llu cols=%u..%u\n",
                parse_status, parser->error_count, node ? (int)node->type : -1,
                node ? (unsigned)node->source_span.has_location : 0u,
                (unsigned long long)(node ? node->source_span.start_offset : 0u),
                (unsigned long long)(node ? node->source_span.end_offset : 0u),
                node ? node->source_span.start_column : 0u,
                node ? node->source_span.end_column : 0u);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Nexus jump source-span test failed.\n");
            return 1;
        }
    }


    /* Tuple expressions include their opening and closing parentheses. */
    {
        static const char source[] = "f:main() { v:p = (1,2); }";
        ParserContext* parser = parser_init_source("tuple-span.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* tuple_start = strstr(source, "(1,2)");
        size_t start_offset = tuple_start ? (size_t)(tuple_start - source) : 0u;
        ASTFuncDecl* function;
        ASTVarDecl* variable;
        ASTTuple* tuple;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            variable = function
                ? (ASTVarDecl*)((ASTBlock*)function->body)->statements : NULL;
            tuple = variable && variable->initializer &&
                variable->initializer->type == AST_TUPLE
                    ? (ASTTuple*)variable->initializer : NULL;
            valid = tuple && tuple->base.source_span.has_location == 1u &&
                tuple->base.source_span.start_offset == start_offset &&
                tuple->base.source_span.end_offset == start_offset + 5u &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Tuple AST source-span test failed.\n");
            return 1;
        }
    }


    /* Thread block and its nested block include the special >{ opener. */
    {
        static const char source[] = "f:main() { (worker)>{ ret 1; } }";
        ParserContext* parser = parser_init_source("thread-span.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* thread_start = strstr(source, "(worker)");
        const char* block_start = strstr(source, ">{");
        const char* block_end = strstr(source, "ret 1; }");
        ASTFuncDecl* function;
        ASTThreadBlock* thread;
        ASTBlock* body;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            thread = function
                ? (ASTThreadBlock*)((ASTBlock*)function->body)->statements : NULL;
            body = thread ? (ASTBlock*)thread->body : NULL;
            valid = thread && thread->base.source_span.has_location == 1u &&
                thread->base.source_span.start_offset == (size_t)(thread_start - source) &&
                thread->base.source_span.end_offset == (size_t)(block_end + 8 - source) &&
                body && body->base.source_span.has_location == 1u &&
                body->base.source_span.start_offset == (size_t)(block_start - source) &&
                body->base.source_span.end_offset == (size_t)(block_end + 8 - source) &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Thread block AST source-span test failed.\n");
            return 1;
        }
    }


    /* UTF-8 source columns and offsets are byte-based, as specified. */
    {
        static const char source[] = "f:main() { v:s = \"é\"; }";
        ParserContext* parser = parser_init_source("utf8-span.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* utf8 = strstr(source, "é");
        size_t string_offset = utf8 ? (size_t)(utf8 - source) - 1u : 0u;
        ASTFuncDecl* function;
        ASTVarDecl* variable;
        ASTNode* literal;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            variable = function
                ? (ASTVarDecl*)((ASTBlock*)function->body)->statements : NULL;
            literal = variable ? variable->initializer : NULL;
            valid = literal && literal->type == AST_LITERAL &&
                literal->source_span.has_location == 1u &&
                literal->source_span.start_offset == string_offset &&
                literal->source_span.end_offset == string_offset + 4u &&
                literal->source_span.start_column == string_offset + 1u &&
                literal->source_span.end_column == string_offset + 5u &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "UTF-8 byte source-span test failed.\n");
            return 1;
        }
    }

    /* Multiline productions retain exact end line and column. */
    {
        static const char source[] = "f:main() {\n  v:x = 1;\n  ret x;\n}";
        ParserContext* parser = parser_init_source("multiline-span.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* one = strstr(source, "1;");
        size_t literal_offset = one ? (size_t)(one - source) : 0u;
        ASTProgram* program;
        ASTFuncDecl* function;
        ASTBlock* body;
        ASTVarDecl* variable;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            program = (ASTProgram*)parser->ast_root;
            function = (ASTFuncDecl*)program->declarations;
            body = function ? (ASTBlock*)function->body : NULL;
            variable = body ? (ASTVarDecl*)body->statements : NULL;
            valid = function && body && variable && variable->initializer &&
                variable->initializer->source_span.start_line == 2u &&
                variable->initializer->source_span.start_column == 9u &&
                variable->initializer->source_span.end_line == 2u &&
                variable->initializer->source_span.end_column == 10u &&
                body->base.source_span.start_line == 1u &&
                body->base.source_span.start_column == 10u &&
                body->base.source_span.end_line == 4u &&
                body->base.source_span.end_column == 2u &&
                function->base.source_span.end_line == 4u &&
                function->base.source_span.end_column == 2u &&
                program->base.source_span.has_location == 1u &&
                program->base.source_span.start_offset == 0u &&
                program->base.source_span.end_offset == sizeof(source) - 1u &&
                program->base.source_span.start_line == 1u &&
                program->base.source_span.start_column == 1u &&
                program->base.source_span.end_line == 4u &&
                program->base.source_span.end_column == 2u &&
                variable->initializer->source_span.start_offset == literal_offset &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Multiline AST source-span test failed.\n");
            return 1;
        }
    }


    /* Nexus capture nodes cover the left identifier through the right expression. */
    {
        static const char source[] = "f:main() { v:x = 1; x <- handler; }";
        ParserContext* parser = parser_init_source("capture-span.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* left = strstr(source, "x <-");
        const char* right = strstr(source, "handler");
        ASTFuncDecl* function;
        ASTNode* statement;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            statement = function ? ((ASTBlock*)function->body)->statements : NULL;
            statement = statement ? statement->next : NULL;
            valid = statement && statement->type == AST_NEXUS_CAPTURE &&
                statement->source_span.has_location == 1u &&
                statement->source_span.start_offset == (size_t)(left - source) &&
                statement->source_span.end_offset == (size_t)(right + 7 - source) &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Nexus capture source-span test failed.\n");
            return 1;
        }
    }


    /* Function parameters and variadic marker preserve their own token ranges. */
    {
        static const char source[] = "f:main(a!i32, ...){ ret a; }";
        ParserContext* parser = parser_init_source("parameter-span.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* param_start = strstr(source, "a!i32");
        const char* variadic = strstr(source, "...");
        ASTFuncDecl* function;
        ASTVarDecl* parameter;
        ASTNode* marker;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            parameter = function ? (ASTVarDecl*)function->params : NULL;
            marker = parameter ? parameter->base.next : NULL;
            valid = parameter && parameter->base.source_span.has_location == 1u &&
                parameter->base.source_span.start_offset == (size_t)(param_start - source) &&
                parameter->base.source_span.end_offset == (size_t)(param_start + 5 - source) &&
                marker && marker->type == AST_VARIADIC_PARAM &&
                marker->source_span.has_location == 1u &&
                marker->source_span.start_offset == (size_t)(variadic - source) &&
                marker->source_span.end_offset == (size_t)(variadic + 3 - source) &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Function parameter source-span test failed.\n");
            return 1;
        }
    }


    /* Normative expression lambda f:(params)>expr spans the full production. */
    {
        static const char source[] = "f:main() { v:f = f:(x) > x + 1; }";
        ParserContext* parser = parser_init_source("lambda-expression-span.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* start = strstr(source, "f:(x)");
        const char* end = strstr(source, "x + 1;");
        ASTFuncDecl* function;
        ASTVarDecl* variable;
        ASTLambda* lambda;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            variable = function
                ? (ASTVarDecl*)((ASTBlock*)function->body)->statements : NULL;
            lambda = variable && variable->initializer &&
                variable->initializer->type == AST_LAMBDA
                    ? (ASTLambda*)variable->initializer : NULL;
            valid = lambda && lambda->base.source_span.has_location == 1u &&
                lambda->base.source_span.start_offset == (size_t)(start - source) &&
                lambda->base.source_span.end_offset == (size_t)(end + 5 - source) &&
                lambda->body_is_expression == 1 &&
                lambda->params && lambda->params->source_span.has_location == 1u &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Expression lambda source-span test failed.\n");
            return 1;
        }
    }

    /* Normative block lambda f:(params){...} includes its body braces. */
    {
        static const char source[] = "f:main() { v:f = f:(x) { ret x; }; }";
        ParserContext* parser = parser_init_source("lambda-block-span.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* start = strstr(source, "f:(x)");
        const char* body_open = strstr(source, "{ ret x");
        const char* body_close = strstr(source, "ret x; }");
        ASTFuncDecl* function;
        ASTVarDecl* variable;
        ASTLambda* lambda;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            variable = function
                ? (ASTVarDecl*)((ASTBlock*)function->body)->statements : NULL;
            lambda = variable && variable->initializer &&
                variable->initializer->type == AST_LAMBDA
                    ? (ASTLambda*)variable->initializer : NULL;
            valid = lambda && lambda->base.source_span.has_location == 1u &&
                lambda->base.source_span.start_offset == (size_t)(start - source) &&
                lambda->base.source_span.end_offset == (size_t)(body_close + 8 - source) &&
                lambda->body && lambda->body->source_span.start_offset == (size_t)(body_open - source) &&
                lambda->body->source_span.end_offset == (size_t)(body_close + 8 - source) &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Block lambda source-span test failed.\n");
            return 1;
        }
    }


    /* Nexus label spans its label expression and the attached statement. */
    {
        static const char source[] = "f:main() { label:> ret 1; }";
        ParserContext* parser = parser_init_source("label-span.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* label = strstr(source, "label");
        const char* statement_end = strstr(source, "ret 1;");
        ASTFuncDecl* function;
        ASTNode* node;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            node = function ? ((ASTBlock*)function->body)->statements : NULL;
            valid = node && node->type == AST_NEXUS_LABEL &&
                node->source_span.has_location == 1u &&
                node->source_span.start_offset == (size_t)(label - source) &&
                node->source_span.end_offset == (size_t)(statement_end + 6 - source) &&
                node->source_span.start_column == (size_t)(label - source) + 1u &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Nexus label source-span test failed.\n");
            return 1;
        }
    }


    /* Group declarations and their members preserve independent spans. */
    {
        static const char source[] = "f:main() { group Pair { v:x!i32; } }";
        ParserContext* parser = parser_init_source("group-span.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* group_start = strstr(source, "group Pair");
        const char* group_end = group_start ? strchr(group_start, '}') : NULL;
        const char* member_start = strstr(source, "v:x!i32");
        ASTFuncDecl* function;
        ASTGroupDecl* group;
        ASTVarDecl* member;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            group = function
                ? (ASTGroupDecl*)((ASTBlock*)function->body)->statements : NULL;
            member = group ? (ASTVarDecl*)group->members : NULL;
            valid = group && group->base.source_span.has_location == 1u &&
                group->base.source_span.start_offset == (size_t)(group_start - source) &&
                group->base.source_span.end_offset == (size_t)(group_end + 1 - source) &&
                member && member->base.source_span.has_location == 1u &&
                member->base.source_span.start_offset == (size_t)(member_start - source) &&
                member->base.source_span.end_offset == (size_t)(member_start + 8 - source) &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Group AST source-span test failed.\n");
            return 1;
        }
    }


    /* Targetless Nexus Halt is a complete ?; statement. */
    {
        static const char source[] = "f:main() { ?; }";
        ParserContext* parser = parser_init_source("halt-span.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* halt = strstr(source, "?;");
        ASTFuncDecl* function;
        ASTNode* node;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            node = function ? ((ASTBlock*)function->body)->statements : NULL;
            valid = node && node->type == AST_NEXUS_HALT &&
                node->source_span.has_location == 1u &&
                node->source_span.start_offset == (uint64_t)(halt - source) &&
                node->source_span.end_offset == (uint64_t)(halt + 2 - source) &&
                !((ASTHalt*)node)->expr && ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Targetless Halt source-span test failed.\n");
            return 1;
        }
    }


    /* Inline newtype struct aggregate preserves aggregate and field spans. */
    {
        static const char source[] =
            "f:main() { nt:struct: point { v:x!i32, v:y!i32 } }";
        ParserContext* parser = parser_init_source("newtype-aggregate-span.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* start = strstr(source, "nt:struct:");
        const char* close = start ? strchr(start, '}') : NULL;
        const char* field_x = strstr(source, "v:x!i32");
        const char* field_y = strstr(source, "v:y!i32");
        ASTFuncDecl* function;
        ASTStructDecl* aggregate;
        ASTNode* field;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            aggregate = function
                ? (ASTStructDecl*)((ASTBlock*)function->body)->statements : NULL;
            field = aggregate ? aggregate->fields : NULL;
            valid = aggregate && aggregate->name && strcmp(aggregate->name, "point") == 0 &&
                aggregate->base.source_span.has_location == 1u &&
                aggregate->base.source_span.start_offset == (uint64_t)(start - source) &&
                aggregate->base.source_span.end_offset == (uint64_t)(close + 1 - source) &&
                field && field->source_span.has_location == 1u &&
                field->source_span.start_offset == (uint64_t)(field_x - source) &&
                field->source_span.end_offset == (uint64_t)(field_x + 7 - source) &&
                field->next && field->next->source_span.has_location == 1u &&
                field->next->source_span.start_offset == (uint64_t)(field_y - source) &&
                field->next->source_span.end_offset == (uint64_t)(field_y + 7 - source) &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Inline newtype aggregate source-span test failed.\n");
            return 1;
        }
    }


    /* asmcall/asmjmp are function-call AST nodes with exact call ranges. */
    {
        static const char source[] =
            "f:main() { asmcall(\"CRITICAL_ADD\"); asmjmp(\"MY_ASM_BLOCK\"); }";
        ParserContext* parser = parser_init_source("asm-call-span.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        const char* call_start = strstr(source, "asmcall");
        const char* jump_start = strstr(source, "asmjmp");
        ASTFuncDecl* function;
        ASTCall* call;
        ASTCall* jump;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            call = function ? (ASTCall*)((ASTBlock*)function->body)->statements : NULL;
            jump = call ? (ASTCall*)call->base.next : NULL;
            valid = call && call->base.type == AST_FUNC_CALL && call->callee &&
                call->callee->type == AST_IDENTIFIER &&
                strcmp(((ASTIdentifier*)call->callee)->name, "asmcall") == 0 &&
                call->base.source_span.has_location == 1u &&
                call->base.source_span.start_offset == (uint64_t)(call_start - source) &&
                call->base.source_span.end_offset == (uint64_t)(strchr(call_start, ')') + 1 - source) &&
                jump && jump->base.type == AST_FUNC_CALL && jump->callee &&
                jump->callee->type == AST_IDENTIFIER &&
                strcmp(((ASTIdentifier*)jump->callee)->name, "asmjmp") == 0 &&
                jump->base.source_span.has_location == 1u &&
                jump->base.source_span.start_offset == (uint64_t)(jump_start - source) &&
                jump->base.source_span.end_offset == (uint64_t)(strchr(jump_start, ')') + 1 - source) &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "asmcall/asmjmp AST source-span test failed.\n");
            return 1;
        }
    }

    /* use aliases and defer bodies retain dedicated AST ownership/context. */
    {
        static const char source[] =
            "use CoreModule as Core, MathModule;\n"
            "f:main() { defer { v:cleanup!i32 = 1; } ret 0; }";
        ParserContext* parser = parser_init_source("use-defer-ast.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        ASTProgram* program;
        ASTUse* use_core;
        ASTUse* use_math;
        ASTFuncDecl* function;
        ASTDefer* defer_node;
        ASTBlock* defer_body;
        ASTVarDecl* cleanup;
        const char* core_start = strstr(source, "CoreModule");
        const char* core_alias_end = strstr(source, "Core,");
        const char* math_start = strstr(source, "MathModule");
        const char* defer_start = strstr(source, "defer");
        const char* defer_close = defer_start ? strchr(defer_start, '}') : NULL;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            program = (ASTProgram*)parser->ast_root;
            use_core = (ASTUse*)program->declarations;
            use_math = use_core ? (ASTUse*)use_core->base.next : NULL;
            function = use_math ? (ASTFuncDecl*)use_math->base.next : NULL;
            defer_node = function
                ? (ASTDefer*)((ASTBlock*)function->body)->statements : NULL;
            defer_body = defer_node && defer_node->body &&
                defer_node->body->type == AST_BLOCK ? (ASTBlock*)defer_node->body : NULL;
            cleanup = defer_body ? (ASTVarDecl*)defer_body->statements : NULL;
            valid = use_core && use_core->base.type == AST_USE &&
                strcmp(use_core->module_name, "CoreModule") == 0 &&
                use_core->alias && strcmp(use_core->alias, "Core") == 0 &&
                use_core->base.source_span.has_location && core_start &&
                core_alias_end &&
                use_core->base.source_span.start_offset == (uint64_t)(core_start-source) &&
                use_core->base.source_span.end_offset ==
                    (uint64_t)(core_alias_end + strlen("Core") - source) &&
                use_math && strcmp(use_math->module_name, "MathModule") == 0 &&
                !use_math->alias && math_start &&
                use_math->base.source_span.has_location &&
                use_math->base.source_span.start_offset == (uint64_t)(math_start-source) &&
                use_math->base.source_span.end_offset ==
                    (uint64_t)(math_start + strlen("MathModule") - source) &&
                function && function->base.type == AST_FUNC_DECL &&
                defer_node && defer_node->base.type == AST_DEFER &&
                defer_start && defer_close && defer_node->base.source_span.has_location &&
                defer_node->base.source_span.start_offset == (uint64_t)(defer_start-source) &&
                defer_node->base.source_span.end_offset == (uint64_t)(defer_close+1-source) &&
                defer_body && cleanup && cleanup->name &&
                strcmp(cleanup->name, "cleanup") == 0 &&
                ast_validate(parser->ast_root, NULL);
        }
        if (!valid) {
            if (parser) parser_destroy(parser);
            fprintf(stderr, "use alias/defer AST ownership test failed.\n");
            return 1;
        }
        if (parser) parser_destroy(parser);
    }

    /* Map is an ordinary parameter type; explicit hard typing remains compatible. */
    {
        static const char source[] =
            "f:main() { v:notlar!map; }\n"
            "f:consume(items:map, legacy!map) { ret; }";
        static const char legacy_source[] =
            "f:main() { v:notlar!map str, i32; }";
        static const char generic_source[] =
            "f:main() { v:notlar!map<str, i32>; }";
        ParserContext* parser = parser_init_source("map-type.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        ParserContext* legacy_parser = parser_init_source("map-legacy-args.nxf",
            (const uint8_t*)legacy_source, sizeof(legacy_source) - 1u);
        ParserContext* generic_parser = parser_init_source("map-generic-args.nxf",
            (const uint8_t*)generic_source, sizeof(generic_source) - 1u);
        ASTFuncDecl* main_function;
        ASTVarDecl* map_var;
        ASTFuncDecl* consume_function;
        ASTVarDecl* map_param;
        ASTVarDecl* hard_map_param;
        int valid = 0;
        int legacy_rejected = 0;
        int generic_not_yet_supported = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK &&
            parser->error_count == 0 && ast_validate(parser->ast_root, NULL)) {
            main_function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            map_var = main_function
                ? (ASTVarDecl*)((ASTBlock*)main_function->body)->statements : NULL;
            consume_function = main_function ? (ASTFuncDecl*)main_function->base.next : NULL;
            map_param = consume_function ? (ASTVarDecl*)consume_function->params : NULL;
            hard_map_param = map_param ? (ASTVarDecl*)map_param->base.next : NULL;
            valid = main_function && main_function->name &&
                strcmp(main_function->name, "main") == 0 &&
                map_var && map_var->name && strcmp(map_var->name, "notlar") == 0 &&
                map_var->is_hard_typed && map_var->type_name &&
                strcmp(map_var->type_name, "map") == 0 &&
                consume_function && consume_function->name &&
                strcmp(consume_function->name, "consume") == 0 &&
                map_param && map_param->name &&
                strcmp(map_param->name, "items") == 0 &&
                !map_param->is_hard_typed && map_param->type_name &&
                strcmp(map_param->type_name, "map") == 0 &&
                hard_map_param && hard_map_param->name &&
                strcmp(hard_map_param->name, "legacy") == 0 &&
                hard_map_param->is_hard_typed && hard_map_param->type_name &&
                strcmp(hard_map_param->type_name, "!map") == 0;
        }
        if (legacy_parser)
            legacy_rejected = parser_run(legacy_parser) != PARSER_STATUS_OK ||
                legacy_parser->error_count != 0;
        if (generic_parser)
            generic_not_yet_supported = parser_run(generic_parser) != PARSER_STATUS_OK ||
                generic_parser->error_count != 0;
        if (parser) parser_destroy(parser);
        if (legacy_parser) parser_destroy(legacy_parser);
        if (generic_parser) parser_destroy(generic_parser);
        if (!valid || !legacy_rejected || !generic_not_yet_supported) {
            fprintf(stderr, "Map fixture: valid=%d legacy=%d generic=%d\n",
                valid, legacy_rejected, generic_not_yet_supported);
            return 1;
        }
    }

    /* Default parameter expressions are retained as initializer AST children. */
    {
        static const char source[] = "f:sum(a:10, b!i32)!i32 { ret a; }";
        ParserContext* parser = parser_init_source("default-param.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        ASTFuncDecl* function;
        ASTVarDecl* first;
        ASTVarDecl* second;
        int valid = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            first = function ? (ASTVarDecl*)function->params : NULL;
            second = first ? (ASTVarDecl*)first->base.next : NULL;
            valid = function && first && first->name &&
                strcmp(first->name, "a") == 0 && first->initializer &&
                first->initializer->type == AST_LITERAL &&
                strcmp(((ASTLiteral*)first->initializer)->string_value, "10") == 0 &&
                second && second->name && strcmp(second->name, "b") == 0 &&
                second->type_name && strcmp(second->type_name, "!i32") == 0 &&
                ast_validate(parser->ast_root, NULL);
        }
        if (parser) parser_destroy(parser);
        if (!valid) {
            fprintf(stderr, "Default parameter AST test failed.\n");
            return 1;
        }
    }


    /* Expression-bodied and block-bodied lambdas have one stable AST contract. */
    {
        static const char source[] =
            "f:main() {"
            " v:mul = f:(a, b) > a * b;"
            " v:square = f:(a) { ret a * a; };"
            "}";
        static const char invalid_source[] = "f:main() { v:bad = f:(x); }";
        ParserContext* parser = parser_init_source("lambda-contract.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        ParserContext* invalid = parser_init_source("lambda-missing-body.nxf",
            (const uint8_t*)invalid_source, sizeof(invalid_source) - 1u);
        FILE* stream = NULL;
        ASTNode* decoded = NULL;
        ASTFuncDecl* function;
        ASTVarDecl* mul;
        ASTVarDecl* square;
        ASTLambda* expression_lambda;
        ASTLambda* block_lambda;
        int valid = 0;
        int invalid_rejected = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK &&
            parser->error_count == 0 && parser->ast_root &&
            ast_validate(parser->ast_root, NULL)) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            mul = function && function->body && function->body->type == AST_BLOCK
                ? (ASTVarDecl*)((ASTBlock*)function->body)->statements : NULL;
            square = mul ? (ASTVarDecl*)mul->base.next : NULL;
            expression_lambda = mul && mul->initializer &&
                mul->initializer->type == AST_LAMBDA
                    ? (ASTLambda*)mul->initializer : NULL;
            block_lambda = square && square->initializer &&
                square->initializer->type == AST_LAMBDA
                    ? (ASTLambda*)square->initializer : NULL;
            stream = tmpfile();
            if (expression_lambda && block_lambda && stream &&
                expression_lambda->body_is_expression == 1 &&
                expression_lambda->body &&
                expression_lambda->body->type == AST_BINARY_EXPR &&
                ((ASTBinaryExpr*)expression_lambda->body)->left &&
                ((ASTBinaryExpr*)expression_lambda->body)->left->type == AST_IDENTIFIER &&
                ((ASTBinaryExpr*)expression_lambda->body)->right &&
                ((ASTBinaryExpr*)expression_lambda->body)->right->type == AST_IDENTIFIER &&
                expression_lambda->params && expression_lambda->params->next &&
                !expression_lambda->params->next->next &&
                block_lambda->body_is_expression == 0 &&
                block_lambda->body && block_lambda->body->type == AST_BLOCK &&
                ((ASTBlock*)block_lambda->body)->statements &&
                ((ASTBlock*)block_lambda->body)->statements->type == AST_RETURN &&
                ((ASTUnaryExpr*)((ASTBlock*)block_lambda->body)->statements)->operand &&
                ast_serialize(parser->ast_root, stream) &&
                fseek(stream, 0L, SEEK_SET) == 0) {
                decoded = ast_deserialize(stream);
                if (decoded && ast_validate(decoded, NULL)) {
                    ASTFuncDecl* decoded_function =
                        (ASTFuncDecl*)((ASTProgram*)decoded)->declarations;
                    ASTVarDecl* decoded_mul = decoded_function && decoded_function->body
                        ? (ASTVarDecl*)((ASTBlock*)decoded_function->body)->statements : NULL;
                    ASTVarDecl* decoded_square = decoded_mul
                        ? (ASTVarDecl*)decoded_mul->base.next : NULL;
                    ASTLambda* decoded_expression = decoded_mul && decoded_mul->initializer &&
                        decoded_mul->initializer->type == AST_LAMBDA
                            ? (ASTLambda*)decoded_mul->initializer : NULL;
                    ASTLambda* decoded_block = decoded_square && decoded_square->initializer &&
                        decoded_square->initializer->type == AST_LAMBDA
                            ? (ASTLambda*)decoded_square->initializer : NULL;
                    valid = decoded_expression && decoded_expression->body_is_expression == 1 &&
                        decoded_expression->body &&
                        decoded_expression->body->type == AST_BINARY_EXPR &&
                        decoded_block && decoded_block->body_is_expression == 0 &&
                        decoded_block->body && decoded_block->body->type == AST_BLOCK;
                }
            }
        }
        if (invalid) {
            ParserStatus invalid_status = parser_run(invalid);
            invalid_rejected = invalid_status != PARSER_STATUS_OK ||
                invalid->error_count != 0;
        }
        valid = valid && invalid_rejected;
        if (stream) fclose(stream);
        if (decoded) ast_destroy(decoded);
        if (parser) parser_destroy(parser);
        if (invalid) parser_destroy(invalid);
        if (!valid) {
            fprintf(stderr, "Lambda expression/block AST contract test failed.\n");
            return 1;
        }
    }


    /* Return/break/continue are distinct control-flow AST productions. */
    {
        static const char source[] =
            "f:main() { ret; ret 7; break; continue; }";
        static const char invalid_source[] = "f:main() { break }";
        ParserContext* parser = parser_init_source("control-flow-ast.nxf",
            (const uint8_t*)source, sizeof(source) - 1u);
        ParserContext* invalid = parser_init_source("control-flow-missing-semicolon.nxf",
            (const uint8_t*)invalid_source, sizeof(invalid_source) - 1u);
        FILE* stream = NULL;
        ASTNode* decoded = NULL;
        ASTFuncDecl* function;
        ASTNode* statement;
        const char* ret_void_start=strstr(source,"ret;");
        const char* ret_value_start=strstr(source,"ret 7;");
        const char* break_start=strstr(source,"break;");
        const char* continue_start=strstr(source,"continue;");
        int valid = 0;
        int invalid_rejected = 0;
        if (parser && parser_run(parser) == PARSER_STATUS_OK &&
            parser->error_count == 0 && parser->ast_root &&
            ast_validate(parser->ast_root, NULL)) {
            function = (ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            statement = function && function->body && function->body->type == AST_BLOCK
                ? ((ASTBlock*)function->body)->statements : NULL;
            stream = tmpfile();
            if (statement && statement->type == AST_RETURN &&
                ((ASTUnaryExpr*)statement)->operand == NULL &&
                statement->next && statement->next->type == AST_RETURN &&
                ((ASTUnaryExpr*)statement->next)->operand &&
                ((ASTUnaryExpr*)statement->next)->operand->type == AST_LITERAL &&
                statement->next->next && statement->next->next->type == AST_BREAK &&
                statement->next->next->next &&
                statement->next->next->next->type == AST_CONTINUE &&
                statement->next->next->next->next == NULL &&
                statement->source_span.has_location &&
                statement->next->source_span.has_location &&
                statement->next->next->source_span.has_location &&
                statement->next->next->next->source_span.has_location &&
                ret_void_start &&
                statement->source_span.start_offset==(uint64_t)(ret_void_start-source) &&
                statement->source_span.end_offset==(uint64_t)(ret_void_start+strlen("ret;")-source) &&
                statement->source_span.start_column==(uint32_t)(ret_void_start-source)+1u &&
                statement->source_span.end_column==(uint32_t)(ret_void_start+strlen("ret;")-source)+1u &&
                ret_value_start &&
                statement->next->source_span.start_offset==(uint64_t)(ret_value_start-source) &&
                statement->next->source_span.end_offset==(uint64_t)(ret_value_start+strlen("ret 7;")-source) &&
                statement->next->source_span.start_column==(uint32_t)(ret_value_start-source)+1u &&
                statement->next->source_span.end_column==(uint32_t)(ret_value_start+strlen("ret 7;")-source)+1u &&
                break_start &&
                statement->next->next->source_span.start_offset==(uint64_t)(break_start-source) &&
                statement->next->next->source_span.end_offset==(uint64_t)(break_start+strlen("break;")-source) &&
                statement->next->next->source_span.start_column==(uint32_t)(break_start-source)+1u &&
                statement->next->next->source_span.end_column==(uint32_t)(break_start+strlen("break;")-source)+1u &&
                continue_start &&
                statement->next->next->next->source_span.start_offset==(uint64_t)(continue_start-source) &&
                statement->next->next->next->source_span.end_offset==(uint64_t)(continue_start+strlen("continue;")-source) &&
                statement->next->next->next->source_span.start_column==(uint32_t)(continue_start-source)+1u &&
                statement->next->next->next->source_span.end_column==(uint32_t)(continue_start+strlen("continue;")-source)+1u &&
                stream && ast_serialize(parser->ast_root, stream) &&
                fseek(stream, 0L, SEEK_SET) == 0) {
                decoded = ast_deserialize(stream);
                valid = decoded && ast_validate(decoded, NULL);
            }
        }
        if (invalid) {
            ParserStatus status = parser_run(invalid);
            invalid_rejected = status != PARSER_STATUS_OK || invalid->error_count != 0;
        }
        valid = valid && invalid_rejected;
        if (stream) fclose(stream);
        if (decoded) ast_destroy(decoded);
        if (parser) parser_destroy(parser);
        if (invalid) parser_destroy(invalid);
        if (!valid) {
            fprintf(stderr, "Return/break/continue AST contract test failed.\n");
            return 1;
        }
    }

    /* Default parameter values are AST_VAR_DECL initializers, not named-call args. */
    {
        static const char source[] =
            "f:defaults(a:10, b:20) { ret a; }";
        static const char* invalid_sources[] = {
            "f:bad(a:) { ret; }",
            "f:bad(a:10, b:) { ret; }"
        };
        const char* a_start=strstr(source,"a:10");
        const char* b_start=strstr(source,"b:20");
        ParserContext* parser=parser_init_source("default-parameters.nxf",
            (const uint8_t*)source,sizeof(source)-1u);
        ASTNode* decoded=NULL;
        FILE* stream=NULL;
        int valid=0;
        size_t i;
        if(parser && parser_run(parser)==PARSER_STATUS_OK &&
           parser->error_count==0 && parser->ast_root &&
           ast_validate(parser->ast_root,NULL) && a_start && b_start) {
            ASTFuncDecl* fn=(ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            ASTVarDecl* first=fn?(ASTVarDecl*)fn->params:NULL;
            ASTVarDecl* second=first?(ASTVarDecl*)first->base.next:NULL;
            valid=first && first->base.type==AST_VAR_DECL &&
                first->name && strcmp(first->name,"a")==0 &&
                first->initializer && first->initializer->type==AST_LITERAL &&
                strcmp(((ASTLiteral*)first->initializer)->string_value,"10")==0 &&
                first->base.source_span.has_location &&
                first->base.source_span.start_offset==(uint64_t)(a_start-source) &&
                first->base.source_span.end_offset==(uint64_t)(a_start-source)+4u &&
                first->base.source_span.start_column==(uint32_t)(a_start-source)+1u &&
                first->base.source_span.end_column==(uint32_t)(a_start-source)+5u &&
                second && second->base.type==AST_VAR_DECL &&
                second->name && strcmp(second->name,"b")==0 &&
                second->initializer && second->initializer->type==AST_LITERAL &&
                strcmp(((ASTLiteral*)second->initializer)->string_value,"20")==0 &&
                second->base.source_span.has_location &&
                second->base.source_span.start_offset==(uint64_t)(b_start-source) &&
                second->base.source_span.end_offset==(uint64_t)(b_start-source)+4u &&
                second->base.source_span.start_column==(uint32_t)(b_start-source)+1u &&
                second->base.source_span.end_column==(uint32_t)(b_start-source)+5u &&
                second->base.next==NULL;
            stream=tmpfile();
            if(valid && stream && ast_serialize(parser->ast_root,stream) &&
               fseek(stream,0L,SEEK_SET)==0) {
                decoded=ast_deserialize(stream);
                valid=decoded && ast_validate(decoded,NULL);
            } else valid=0;
        }
        for(i=0u;i<sizeof(invalid_sources)/sizeof(invalid_sources[0]);++i) {
            ParserContext* invalid=parser_init_source("invalid-default-parameter.nxf",
                (const uint8_t*)invalid_sources[i],strlen(invalid_sources[i]));
            if(!invalid) { valid=0; break; }
            if(parser_run(invalid)==PARSER_STATUS_OK &&
               invalid->error_count==0) valid=0;
            parser_destroy(invalid);
        }
        if(stream) fclose(stream);
        if(decoded) ast_destroy(decoded);
        if(parser) parser_destroy(parser);
        if(!valid) {
            fprintf(stderr,"Default parameter AST/span/codec checks failed.\\n");
            return 1;
        }
    }

    /* Enum variants, optional payload fields and their source ranges form a stable AST chain. */
    {
        static const char source[] =
            "nt:enum: Task {\n"
            "  IO(file!str)\n"
            "  Compute(a!i32, b!i32)\n"
            "  Cleanup\n"
            "}";
        const char* io_start=strstr(source,"IO(file!str)");
        const char* io_close=io_start?strchr(io_start,')'):NULL;
        const char* compute_start=strstr(source,"Compute(a!i32, b!i32)");
        const char* compute_close=compute_start?strchr(compute_start,')'):NULL;
        const char* cleanup_start=strstr(source,"Cleanup");
        const char* enum_close=strrchr(source,'}');
        ParserContext* parser=parser_init_source("enum-variant-ast.nxf",
            (const uint8_t*)source,sizeof(source)-1u);
        ASTNode* decoded=NULL;
        FILE* stream=NULL;
        int valid=0;
        if(parser && parser_run(parser)==PARSER_STATUS_OK &&
           parser->error_count==0 && parser->ast_root && io_start && io_close &&
           compute_start && compute_close && cleanup_start && enum_close) {
            ASTEnumDecl* declaration=(ASTEnumDecl*)((ASTProgram*)parser->ast_root)->declarations;
            ASTEnumVariant* io=declaration?(ASTEnumVariant*)declaration->variants:NULL;
            ASTEnumVariant* compute=io?(ASTEnumVariant*)io->base.next:NULL;
            ASTEnumVariant* cleanup=compute?(ASTEnumVariant*)compute->base.next:NULL;
            ASTVarDecl* file_field=io?(ASTVarDecl*)io->data:NULL;
            ASTVarDecl* a_field=compute?(ASTVarDecl*)compute->data:NULL;
            ASTVarDecl* b_field=a_field?(ASTVarDecl*)a_field->base.next:NULL;
            valid=declaration && strcmp(declaration->name,"Task")==0 &&
                declaration->base.source_span.has_location &&
                declaration->base.source_span.start_offset==0u &&
                declaration->base.source_span.end_offset==(uint64_t)(enum_close+1-source) &&
                io && strcmp(io->name,"IO")==0 && file_field &&
                strcmp(file_field->name,"file")==0 && file_field->type_name &&
                strcmp(file_field->type_name,"str")==0 &&
                io->base.source_span.start_offset==(uint64_t)(io_start-source) &&
                io->base.source_span.end_offset==(uint64_t)(io_close+1-source) &&
                file_field->base.source_span.start_offset==(uint64_t)(strstr(source,"file!str")-source) &&
                file_field->base.source_span.end_offset==(uint64_t)(strstr(source,"file!str")+strlen("file!str")-source) &&
                compute && strcmp(compute->name,"Compute")==0 && a_field && b_field &&
                strcmp(a_field->name,"a")==0 && strcmp(b_field->name,"b")==0 &&
                compute->base.source_span.start_offset==(uint64_t)(compute_start-source) &&
                compute->base.source_span.end_offset==(uint64_t)(compute_close+1-source) &&
                cleanup && strcmp(cleanup->name,"Cleanup")==0 && cleanup->data==NULL &&
                cleanup->base.source_span.start_offset==(uint64_t)(cleanup_start-source) &&
                cleanup->base.source_span.end_offset==(uint64_t)(cleanup_start+strlen("Cleanup")-source) &&
                cleanup->base.next==NULL && ast_validate(parser->ast_root,NULL);
            if(!valid) {
                ASTValidationError ve;
                int graph_ok=ast_validate(parser->ast_root,&ve);
                fprintf(stderr,
                    "Enum AST mismatch decl=%s span=%llu..%llu IO=%s span=%llu..%llu field=%s type=%s fieldspan=%llu..%llu Compute=%s span=%llu..%llu a=%s b=%s Cleanup=%s span=%llu..%llu graph=%d code=%d field=%s node=%d\n",
                    declaration&&declaration->name?declaration->name:"<null>",
                    (unsigned long long)(declaration?declaration->base.source_span.start_offset:0),
                    (unsigned long long)(declaration?declaration->base.source_span.end_offset:0),
                    io&&io->name?io->name:"<null>",
                    (unsigned long long)(io?io->base.source_span.start_offset:0),
                    (unsigned long long)(io?io->base.source_span.end_offset:0),
                    file_field&&file_field->name?file_field->name:"<null>",
                    file_field&&file_field->type_name?file_field->type_name:"<null>",
                    (unsigned long long)(file_field?file_field->base.source_span.start_offset:0),
                    (unsigned long long)(file_field?file_field->base.source_span.end_offset:0),
                    compute&&compute->name?compute->name:"<null>",
                    (unsigned long long)(compute?compute->base.source_span.start_offset:0),
                    (unsigned long long)(compute?compute->base.source_span.end_offset:0),
                    a_field&&a_field->name?a_field->name:"<null>",
                    b_field&&b_field->name?b_field->name:"<null>",
                    cleanup&&cleanup->name?cleanup->name:"<null>",
                    (unsigned long long)(cleanup?cleanup->base.source_span.start_offset:0),
                    (unsigned long long)(cleanup?cleanup->base.source_span.end_offset:0),
                    graph_ok,(int)ve.code,ve.field?ve.field:"<none>",
                    ve.node?(int)ve.node->type:-1);
            }
            stream=tmpfile();
            if(!stream) { fprintf(stderr,"Enum AST tmpfile failed\n"); valid=0; }
            else if(valid && !ast_serialize(parser->ast_root,stream)) {
                fprintf(stderr,"Enum AST serialization failed\n"); valid=0;
            } else if(valid && fseek(stream,0L,SEEK_SET)!=0) {
                fprintf(stderr,"Enum AST rewind failed\n"); valid=0;
            } else if(valid) {
                decoded=ast_deserialize(stream);
                if(!decoded) { fprintf(stderr,"Enum AST deserialization failed\n"); valid=0; }
                else if(!ast_validate(decoded,NULL)) {
                    fprintf(stderr,"Enum AST decoded graph invalid\n"); valid=0;
                }
            }
        } else if(parser) {
            Error* error;
            fprintf(stderr,"Enum AST parse failed status=%d errors=%d\n",
                (int)parser_get_status(parser),parser->error_count);
            for(error=parser->error_head;error;error=error->next)
                fprintf(stderr,"  %d:%d %s\n",error->line,error->col,
                    error->message?error->message:"");
        }
        if(parser && (!parser->ast_root || parser->error_count!=0))
            fprintf(stderr,"Enum fixture parse failed: status=%d errors=%u\\n",
                (int)parser->status,(unsigned)parser->error_count);
        if(stream) fclose(stream);
        if(decoded) ast_destroy(decoded);
        if(parser) parser_destroy(parser);
        if(!valid) {
            fprintf(stderr,"Enum variant/payload AST span and codec test failed.\n");
            return 1;
        }
    }

    /* Struct, packed Struct and Union declarations preserve distinct AST flags and field order. */
    {
        static const char source[] =
            "struct:Point {\n"
            "  v:x!i32,\n"
            "  v:y!i32\n"
            "}\n"
            "!struct:NetworkPacket {\n"
            "  v:id!u8,\n"
            "  v:data!u32\n"
            "}\n"
            "union:Register32 {\n"
            "  v:full!u32,\n"
            "  v:parts!u16\n"
            "}\n"
            "nt:Addr = u64;";
        const char* point_start=strstr(source,"struct:Point");
        const char* packet_start=strstr(source,"!struct:NetworkPacket");
        const char* union_start=strstr(source,"union:Register32");
        const char* point_end=strstr(source,"}\n!struct:NetworkPacket");
        const char* packet_end=strstr(source,"}\nunion:Register32");
        const char* union_end=strrchr(source,'}');
        const char* newtype_start=strstr(source,"nt:Addr = u64;");
        ParserContext* parser=parser_init_source("aggregate-declarations.nxf",
            (const uint8_t*)source,sizeof(source)-1u);
        ASTNode* decoded=NULL;
        FILE* stream=NULL;
        int valid=0;
        if(parser && parser_run(parser)==PARSER_STATUS_OK &&
           parser->error_count==0 && parser->ast_root &&
           point_start && packet_start && union_start && point_end &&
           packet_end && union_end && newtype_start) {
            ASTStructDecl* point=(ASTStructDecl*)((ASTProgram*)parser->ast_root)->declarations;
            ASTStructDecl* packet=point?(ASTStructDecl*)point->base.next:NULL;
            ASTStructDecl* reg=packet?(ASTStructDecl*)packet->base.next:NULL;
            ASTNewTypeDecl* newtype=reg && reg->base.next && reg->base.next->type==AST_NEWTYPE_DECL
                ? (ASTNewTypeDecl*)reg->base.next : NULL;
            ASTVarDecl* point_x=point?(ASTVarDecl*)point->fields:NULL;
            ASTVarDecl* point_y=point_x?(ASTVarDecl*)point_x->base.next:NULL;
            ASTVarDecl* packet_id=packet?(ASTVarDecl*)packet->fields:NULL;
            ASTVarDecl* packet_data=packet_id?(ASTVarDecl*)packet_id->base.next:NULL;
            ASTVarDecl* reg_full=reg?(ASTVarDecl*)reg->fields:NULL;
            ASTVarDecl* reg_parts=reg_full?(ASTVarDecl*)reg_full->base.next:NULL;
            valid=point && strcmp(point->name,"Point")==0 &&
                !point->is_packed && !point->is_union &&
                point->base.source_span.start_offset==(uint64_t)(point_start-source) &&
                point->base.source_span.end_offset==(uint64_t)(point_end+1-source) &&
                point_x && strcmp(point_x->name,"x")==0 &&
                point_y && strcmp(point_y->name,"y")==0 &&
                packet && strcmp(packet->name,"NetworkPacket")==0 &&
                packet->is_packed && !packet->is_union &&
                packet->base.source_span.start_offset==(uint64_t)(packet_start-source) &&
                packet->base.source_span.end_offset==(uint64_t)(packet_end+1-source) &&
                packet_id && strcmp(packet_id->name,"id")==0 &&
                packet_data && strcmp(packet_data->name,"data")==0 &&
                reg && strcmp(reg->name,"Register32")==0 &&
                reg->is_union && !reg->is_packed &&
                reg->base.source_span.start_offset==(uint64_t)(union_start-source) &&
                reg->base.source_span.end_offset==(uint64_t)(union_end+1-source) &&
                reg_full && strcmp(reg_full->name,"full")==0 &&
                reg_parts && strcmp(reg_parts->name,"parts")==0 &&
                reg_parts->base.next==NULL && newtype &&
                strcmp(newtype->name,"Addr")==0 && strcmp(newtype->target_type,"u64")==0 &&
                newtype->base.source_span.start_offset==(uint64_t)(newtype_start-source) &&
                newtype->base.source_span.end_offset==(uint64_t)(newtype_start+strlen("nt:Addr = u64;")-source) &&
                newtype->base.next==NULL && ast_validate(parser->ast_root,NULL);
            stream=tmpfile();
            if(valid && stream && ast_serialize(parser->ast_root,stream) &&
               fseek(stream,0L,SEEK_SET)==0) {
                decoded=ast_deserialize(stream);
                valid=decoded && ast_validate(decoded,NULL);
            } else valid=0;
        }
        if(stream) fclose(stream);
        if(decoded) ast_destroy(decoded);
        if(parser) parser_destroy(parser);
        if(!valid) {
            fprintf(stderr,"Struct/packed Struct/Union AST shape, span and codec test failed.\n");
            return 1;
        }
    }

    /* Group methods, default dispatch and nested groups have distinct AST ownership. */
    {
        static const char source[] =
            "group db {\n"
            "  open => f:(path!str) { ret; }\n"
            "  default { ret; }\n"
            "  nosql => group {\n"
            "    query => f:(sql!str) { ret; }\n"
            "  }\n"
            "}";
        const char* group_start=strstr(source,"group db");
        const char* open_start=strstr(source,"open =>");
        const char* open_end=strstr(source,"ret; }");
        const char* default_start=strstr(source,"default {");
        const char* default_end=default_start?strstr(default_start,"ret; }"):NULL;
        const char* nested_start=strstr(source,"nosql => group");
        const char* query_start=strstr(source,"query =>");
        const char* query_end=query_start?strstr(query_start,"ret; }"):NULL;
        const char* nested_close=strstr(source,"  }\n}");
        const char* nested_end=nested_close?nested_close+2:NULL;
        const char* group_end=strrchr(source,'}');
        ParserContext* parser=parser_init_source("group-method-ast.nxf",
            (const uint8_t*)source,sizeof(source)-1u);
        ASTNode* decoded=NULL;
        FILE* stream=NULL;
        int valid=0;
        if(parser && parser_run(parser)==PARSER_STATUS_OK &&
           parser->error_count==0 && parser->ast_root &&
           group_start && open_start && open_end && default_start &&
           default_end && nested_start && query_start && query_end &&
           nested_end && group_end) {
            ASTGroupDecl* group=(ASTGroupDecl*)((ASTProgram*)parser->ast_root)->declarations;
            ASTMethod* open=group?(ASTMethod*)group->members:NULL;
            ASTMethod* default_method=open && open->base.next &&
                open->base.next->type==AST_METHOD?(ASTMethod*)open->base.next:NULL;
            ASTGroupDecl* nested=default_method && default_method->base.next &&
                default_method->base.next->type==AST_GROUP_DECL
                    ? (ASTGroupDecl*)default_method->base.next : NULL;
            ASTMethod* query=nested?(ASTMethod*)nested->members:NULL;
            valid=group && strcmp(group->name,"db")==0 &&
                group->base.source_span.start_offset==(uint64_t)(group_start-source) &&
                group->base.source_span.end_offset==(uint64_t)(group_end+1-source) &&
                open && strcmp(open->public_name,"open")==0 &&
                open->implementation && open->implementation->type==AST_FUNC_DECL &&
                open->base.source_span.start_offset==(uint64_t)(open_start-source) &&
                open->base.source_span.end_offset==(uint64_t)(open_end+strlen("ret; }")-source) &&
                default_method && strcmp(default_method->public_name,"default")==0 &&
                default_method->implementation && default_method->implementation->type==AST_BLOCK &&
                default_method->base.source_span.start_offset==(uint64_t)(default_start-source) &&
                default_method->base.source_span.end_offset==(uint64_t)(default_end+strlen("ret; }")-source) &&
                nested && strcmp(nested->name,"nosql")==0 &&
                nested->base.source_span.start_offset==(uint64_t)(nested_start-source) &&
                nested->base.source_span.end_offset==(uint64_t)(nested_end+1-source) &&
                query && strcmp(query->public_name,"query")==0 &&
                query->implementation && query->implementation->type==AST_FUNC_DECL &&
                query->base.source_span.start_offset==(uint64_t)(query_start-source) &&
                query->base.source_span.end_offset==(uint64_t)(query_end+strlen("ret; }")-source) &&
                query->base.next==NULL && ast_validate(parser->ast_root,NULL);
            if(!valid) {
                ASTValidationError validation_error;
                int graph_ok=ast_validate(parser->ast_root,&validation_error);
                fprintf(stderr,
                    "Group AST shape mismatch db=%s open=%s default=%s nested=%s query=%s graph=%d code=%d field=%s node=%d spans[g=%llu..%llu o=%llu..%llu d=%llu..%llu n=%llu..%llu q=%llu..%llu]\\n",
                    group&&group->name?group->name:"<null>",
                    open&&open->public_name?open->public_name:"<null>",
                    default_method&&default_method->public_name?default_method->public_name:"<null>",
                    nested&&nested->name?nested->name:"<null>",
                    query&&query->public_name?query->public_name:"<null>",graph_ok,
                    (int)validation_error.code,validation_error.field?validation_error.field:"<none>",
                    validation_error.node?(int)validation_error.node->type:-1,
                    (unsigned long long)(group?group->base.source_span.start_offset:0),
                    (unsigned long long)(group?group->base.source_span.end_offset:0),
                    (unsigned long long)(open?open->base.source_span.start_offset:0),
                    (unsigned long long)(open?open->base.source_span.end_offset:0),
                    (unsigned long long)(default_method?default_method->base.source_span.start_offset:0),
                    (unsigned long long)(default_method?default_method->base.source_span.end_offset:0),
                    (unsigned long long)(nested?nested->base.source_span.start_offset:0),
                    (unsigned long long)(nested?nested->base.source_span.end_offset:0),
                    (unsigned long long)(query?query->base.source_span.start_offset:0),
                    (unsigned long long)(query?query->base.source_span.end_offset:0));
            }
            stream=tmpfile();
            if(!stream) { fprintf(stderr,"Group AST tmpfile failed\\n"); valid=0; }
            else if(valid && !ast_serialize(parser->ast_root,stream)) {
                fprintf(stderr,"Group AST serialization failed\\n"); valid=0;
            } else if(valid && fseek(stream,0L,SEEK_SET)!=0) {
                fprintf(stderr,"Group AST rewind failed\\n"); valid=0;
            } else if(valid) {
                decoded=ast_deserialize(stream);
                if(!decoded) { fprintf(stderr,"Group AST deserialization failed\\n"); valid=0; }
                else if(!ast_validate(decoded,NULL)) {
                    fprintf(stderr,"Group AST decoded graph invalid\\n"); valid=0;
                }
            }
        } else if(parser) {
            Error* error;
            fprintf(stderr,"Group AST parse/validation failed status=%d errors=%d\\n",
                (int)parser_get_status(parser),parser->error_count);
            for(error=parser->error_head;error;error=error->next)
                fprintf(stderr,"  %d:%d %s\\n",error->line,error->col,
                    error->message?error->message:"");
            if(parser->ast_root) ast_print((ASTNode*)parser->ast_root);
        }
        if(!valid && parser && !parser->ast_root)
            fprintf(stderr,"Group AST parser produced no root\\n");
        if(stream) fclose(stream);
        if(decoded) ast_destroy(decoded);
        if(parser) parser_destroy(parser);
        if(!valid) {
            fprintf(stderr,"Group method/default/nested-group AST span and codec test failed.\n");
            return 1;
        }
    }

    /* Nested Struct literals remain owned expression children and round-trip recursively. */
    {
        static const char source[] =
            "f:main() { v:o!Outer = Outer { inner = Inner { value = 1 }, flag = 2 }; }";
        ParserContext* parser=parser_init_source("nested-struct-literals.nxf",
            (const uint8_t*)source,sizeof(source)-1u);
        ASTNode* decoded=NULL;
        FILE* stream=NULL;
        int valid=0;
        if(parser && parser_run(parser)==PARSER_STATUS_OK &&
           parser->error_count==0 && parser->ast_root && ast_validate(parser->ast_root,NULL)) {
            ASTFuncDecl* fn=(ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            ASTVarDecl* variable=fn && fn->body && fn->body->type==AST_BLOCK
                ? (ASTVarDecl*)((ASTBlock*)fn->body)->statements : NULL;
            ASTStructLiteral* outer=variable && variable->initializer &&
                variable->initializer->type==AST_STRUCT_LITERAL
                    ? (ASTStructLiteral*)variable->initializer : NULL;
            ASTStructFieldInit* inner_field=outer?(ASTStructFieldInit*)outer->fields:NULL;
            ASTStructFieldInit* flag_field=inner_field && inner_field->base.next &&
                inner_field->base.next->type==AST_STRUCT_FIELD_INIT
                    ? (ASTStructFieldInit*)inner_field->base.next : NULL;
            ASTStructLiteral* inner=inner_field && inner_field->value &&
                inner_field->value->type==AST_STRUCT_LITERAL
                    ? (ASTStructLiteral*)inner_field->value : NULL;
            ASTStructFieldInit* value_field=inner?(ASTStructFieldInit*)inner->fields:NULL;
            valid=outer && strcmp(outer->type_name,"Outer")==0 &&
                inner_field && strcmp(inner_field->name,"inner")==0 &&
                inner && strcmp(inner->type_name,"Inner")==0 &&
                value_field && strcmp(value_field->name,"value")==0 &&
                value_field->value && value_field->value->type==AST_LITERAL &&
                flag_field && strcmp(flag_field->name,"flag")==0 &&
                flag_field->value && flag_field->value->type==AST_LITERAL &&
                flag_field->base.next==NULL;
            if(!valid) fprintf(stderr,"Nested Struct literal parsed shape assertion failed\\n");
            stream=tmpfile();
            if(!stream) {
                fprintf(stderr,"Nested Struct literal tmpfile failed\\n");
                valid=0;
            } else if(valid && !ast_serialize(parser->ast_root,stream)) {
                fprintf(stderr,"Nested Struct literal serialization failed\\n");
                valid=0;
            } else if(valid && fseek(stream,0L,SEEK_SET)!=0) {
                fprintf(stderr,"Nested Struct literal stream rewind failed\\n");
                valid=0;
            } else if(valid) {
                decoded=ast_deserialize(stream);
                if(!decoded) {
                    fprintf(stderr,"Nested Struct literal deserialization failed\\n");
                    valid=0;
                } else if(!ast_validate(decoded,NULL)) {
                    fprintf(stderr,"Nested Struct literal decoded graph invalid\\n");
                    valid=0;
                }
            }
        } else if(parser) {
            Error* error;
            fprintf(stderr,"Nested Struct literal parse failed status=%d errors=%d\\n",
                (int)parser_get_status(parser),parser->error_count);
            for(error=parser->error_head;error;error=error->next)
                fprintf(stderr,"  %d:%d %s\\n",error->line,error->col,
                    error->message?error->message:"");
        }
        if(!valid && parser && parser->ast_root) {
            fprintf(stderr,"Nested Struct literal AST shape dump follows:\\n");
            ast_print((ASTNode*)parser->ast_root);
        }
        if(stream) fclose(stream);
        if(decoded) ast_destroy(decoded);
        if(parser) parser_destroy(parser);
        if(!valid) {
            fprintf(stderr,"Nested Struct literal AST/codec test failed.\\n");
            return 1;
        }
    }

    /* Program root span covers the complete source buffer, not only the last token. */
    {
        static const char source[]="f:main() {}";
        ParserContext* parser=parser_init_source("program-root-span.nxf",
            (const uint8_t*)source,sizeof(source)-1u);
        ASTNode* decoded=NULL;
        FILE* stream=NULL;
        const char* open_brace=strchr(source,'{');
        const char* close_brace=strchr(source,'}');
        int valid=0;
        if(parser && parser_run(parser)==PARSER_STATUS_OK &&
           parser->error_count==0 && parser->ast_root && open_brace && close_brace) {
            ASTNode* root=(ASTNode*)parser->ast_root;
            ASTFuncDecl* function=(ASTFuncDecl*)((ASTProgram*)root)->declarations;
            ASTBlock* body=function && function->body && function->body->type==AST_BLOCK
                ? (ASTBlock*)function->body : NULL;
            valid=root->type==AST_PROGRAM && root->source_span.has_location &&
                root->source_span.start_offset==0u &&
                root->source_span.end_offset==(uint64_t)(sizeof(source)-1u) &&
                root->source_span.start_line==1u &&
                root->source_span.start_column==1u &&
                root->source_span.end_line==1u &&
                root->source_span.end_column==(uint32_t)sizeof(source) &&
                root->line==1 && root->col==1 &&
                function && function->base.source_span.has_location &&
                function->base.source_span.start_offset==0u &&
                function->base.source_span.end_offset==(uint64_t)(sizeof(source)-1u) &&
                body && body->base.source_span.has_location &&
                body->base.source_span.start_offset==(uint64_t)(open_brace-source) &&
                body->base.source_span.end_offset==(uint64_t)(close_brace+1-source) &&
                body->base.source_span.start_column==(uint32_t)(open_brace-source)+1u &&
                body->base.source_span.end_column==(uint32_t)(close_brace+1-source)+1u &&
                ast_validate(root,NULL);
            if(!valid) fprintf(stderr,
                "AST_PROGRAM root span actual type=%d has=%u offsets=%llu..%llu lines=%u..%u cols=%u..%u node=%d:%d expected_end=%zu\\n",
                (int)root->type,(unsigned)root->source_span.has_location,
                (unsigned long long)root->source_span.start_offset,
                (unsigned long long)root->source_span.end_offset,
                (unsigned)root->source_span.start_line,(unsigned)root->source_span.end_line,
                (unsigned)root->source_span.start_column,(unsigned)root->source_span.end_column,
                root->line,root->col,sizeof(source)-1u);
            stream=tmpfile();
            if(!stream) {
                fprintf(stderr,"AST_PROGRAM span test tmpfile failed\\n");
                valid=0;
            } else if(!valid) {
                valid=0;
            } else if(!ast_serialize(root,stream)) {
                fprintf(stderr,"AST_PROGRAM span test serialization failed\\n");
                valid=0;
            } else if(fseek(stream,0L,SEEK_SET)!=0) {
                fprintf(stderr,"AST_PROGRAM span test rewind failed\\n");
                valid=0;
            } else {
                decoded=ast_deserialize(stream);
                valid=decoded && decoded->source_span.has_location &&
                    decoded->source_span.start_offset==0u &&
                    decoded->source_span.end_offset==(uint64_t)(sizeof(source)-1u) &&
                    decoded->source_span.end_line==1u &&
                    decoded->source_span.end_column==(uint32_t)sizeof(source) &&
                    ast_validate(decoded,NULL);
                if(!valid) fprintf(stderr,"AST_PROGRAM decoded span missing or mismatched\\n");
            }
        } else if(parser) {
            Error* error;
            fprintf(stderr,"AST_PROGRAM span parse failed status=%d errors=%d\\n",
                (int)parser_get_status(parser),parser->error_count);
            for(error=parser->error_head;error;error=error->next)
                fprintf(stderr,"  %d:%d %s\\n",error->line,error->col,
                    error->message?error->message:"");
        }
        if(stream) fclose(stream);
        if(decoded) ast_destroy(decoded);
        if(parser) parser_destroy(parser);
        if(!valid) {
            fprintf(stderr,"AST_PROGRAM full-source span/codec test failed.\\n");
            return 1;
        }
    }

    /* Shift operators bind tighter than bitwise AND and remain left-associative. */
    {
        static const char source[] =
            "f:main() { v:a!u8 = 1 + 2 <<< 3 & 4; }";
        ParserContext* parser=parser_init_source("shift-precedence.nxf",
            (const uint8_t*)source,sizeof(source)-1u);
        int valid=0;
        if(parser && parser_run(parser)==PARSER_STATUS_OK &&
           parser->error_count==0 && parser->ast_root) {
            ASTFuncDecl* fn=(ASTFuncDecl*)((ASTProgram*)parser->ast_root)->declarations;
            ASTVarDecl* variable=fn && fn->body && fn->body->type==AST_BLOCK
                ? (ASTVarDecl*)((ASTBlock*)fn->body)->statements : NULL;
            ASTBinaryExpr* and_expr=variable && variable->initializer &&
                variable->initializer->type==AST_BINARY_EXPR
                    ? (ASTBinaryExpr*)variable->initializer : NULL;
            ASTBinaryExpr* shift_expr=and_expr && and_expr->left &&
                and_expr->left->type==AST_BINARY_EXPR
                    ? (ASTBinaryExpr*)and_expr->left : NULL;
            ASTBinaryExpr* add_expr=shift_expr && shift_expr->left &&
                shift_expr->left->type==AST_BINARY_EXPR
                    ? (ASTBinaryExpr*)shift_expr->left : NULL;
            valid=and_expr && and_expr->op==TOKEN_BIT_AND &&
                shift_expr && shift_expr->op==TOKEN_BIT_SHL &&
                add_expr && add_expr->op==TOKEN_PLUS;
        }
        if(parser) parser_destroy(parser);
        if(!valid) {
            fprintf(stderr,"Shift precedence/AST construction test failed.\\n");
            return 1;
        }
    }

    if (!test_separated_variable_declarations_and_declaration_spans()) {
        fprintf(stderr, "Separated declaration Parser coverage failed.\\n");
        return 1;
    }

    if (!test_expression_body_function_source_spans()) {
        fprintf(stderr, "Expression-body function Parser coverage failed.\\n");
        return 1;
    }

    if (!test_indexed_capture_assignment_ast()) {
        fprintf(stderr, "Indexed capture assignment Parser coverage failed.\\n");
        return 1;
    }

    if (!test_vector_and_bit_byte_type_parser_coverage()) {
        fprintf(stderr, "Vector/bit/byte type Parser coverage failed.\\n");
        return 1;
    }

    if (!test_copy_intrinsic_uses_generic_call_ast()) {
        fprintf(stderr, "copy() generic-call Parser coverage failed.\\n");
        return 1;
    }

    return 0;
}
