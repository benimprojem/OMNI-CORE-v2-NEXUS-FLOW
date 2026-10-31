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

static int write_invalid_lexical_source(const char* path) {
    static const char source[] = "v:x = §;\n";
    FILE* file = fopen(path, "wb");
    if (!file) return 0;
    int ok = fwrite(source, 1, sizeof(source) - 1u, file) == sizeof(source) - 1u;
    if (fclose(file) != 0) ok = 0;
    return ok;
}

int main(int argc, char** argv) {
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
    int invalid_ok = invalid_result != 0 && invalid->error_count > 0 &&
        invalid->ast_root != NULL && occ_diagnostics_count(&diagnostics) > 0 &&
        occ_diagnostics_count_severity(&diagnostics, OCC_DIAG_ERROR) > 0;
    parser_destroy(invalid);
    occ_diagnostics_destroy(&diagnostics);
    if (!invalid_ok) {
        fprintf(stderr, "Parser did not report invalid syntax or retain its partial AST for cleanup.\n");
        return 1;
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
