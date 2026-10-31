#include "parser.h"
#include "ast.h"

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
    if (parser_run(ctx) != 0 || ctx->error_count != 0 || !ctx->ast_root) goto done;
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

int main(int argc, char** argv) {
    if (!test_cast_and_panic_ast()) {
        fprintf(stderr, "CAST/PANIC AST payload test failed.\n");
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

    return 0;
}
