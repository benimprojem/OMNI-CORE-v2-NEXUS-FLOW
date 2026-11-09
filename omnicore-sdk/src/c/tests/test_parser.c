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
        "f:main(a!i32, b!i64) {"
        " v:first = a + b;"
        " { v:inner = combine(first, b:7); }"
        " v:pair = (first, b);"
        " struct: Point { v:x!i32, v:y!i32 }"
        " v:p!Point = Point { y = 2, x = 1 };"
        " nt:Callback = f:(buffer!u8[16], matrix!i32[4][4], dynamic!u8[])!u8[8];"
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
        "f:main(a!i32, b!i64) {"
        " v:first = a + b;"
        " { v:inner = combine(first, b:7); }"
        " v:pair = (first, b);"
        " struct: Point { v:x!i32, v:y!i32 }"
        " v:p!Point = Point { y = 2, x = 1 };"
