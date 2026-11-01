#include "parser.h"
#include "ast.h"
#include "semantic.h"

#include <stdio.h>

static int check_file(const char* path, int expect_error, ErrorCode expected_code) {
    ParserContext* parser = parser_init(path);
    if (!parser) {
        fprintf(stderr, "Could not initialize parser for %s\n", path);
        return 0;
    }
    if (parser_run(parser) != 0 || !parser->ast_root) {
        fprintf(stderr, "Parser failed for semantic fixture %s\n", path);
        parser_destroy(parser);
        return 0;
    }

    OccDiagnosticCollector diagnostics;
    occ_diagnostics_init(&diagnostics);

    SemanticContext semantic;
    semantic_init(&semantic, path, NULL);
    semantic_attach_diagnostics(&semantic, &diagnostics, OCC_STAGE_SEMANTIC);
    semantic_check(&semantic, (ASTNode*)parser->ast_root);

    int ok = expect_error ? semantic.error_count > 0 : semantic.error_count == 0;
    if (expect_error) {
        ok = ok && occ_diagnostics_count(&diagnostics) > 0 &&
            occ_diagnostics_count_severity(&diagnostics, OCC_DIAG_ERROR) > 0;
    } else {
        ok = ok && occ_diagnostics_count(&diagnostics) == 0;
    }
    if (ok && expect_error) {
        int found = 0;
        for (Error* error = semantic.error_head; error; error = error->next) {
            if (error->code == expected_code) found = 1;
        }
        ok = found;
    }
    if (!ok) {
        fprintf(stderr, "Unexpected semantic result for %s (errors=%d).\n",
            path, semantic.error_count);
    }

    semantic_destroy(&semantic);
    occ_diagnostics_destroy(&diagnostics);
    parser_destroy(parser);
    return ok;
}

static int check_source(const char* source, int expected_errors) {
    ParserContext* parser = parser_init_source(
        "semantic-control.nxf", (const uint8_t*)source, strlen(source));
    SemanticContext semantic;
    int ok;
    if (!parser) return 0;
    if (parser_run(parser) != 0 || !parser->ast_root) {
        parser_destroy(parser);
        return 0;
    }
    semantic_init(&semantic, "semantic-control.nxf", NULL);
    semantic_check(&semantic, (ASTNode*)parser->ast_root);
    ok = semantic.error_count == expected_errors;
    semantic_destroy(&semantic);
    parser_destroy(parser);
    return ok;
}

static int check_control_flow_semantics(void) {
    const char* valid_if =
        "f:main() { if (true) { v:x!i32 = 1; } elsif (1 == 1) "
        "v:y!i32 = 2; els v:z!i32 = 3; }";
    const char* invalid_condition = "f:main() { if (1) v:x!i32 = 1; }";
    const char* invalid_break = "f:main() { break; continue; }";
    return check_source(valid_if, 0) &&
        check_source(invalid_condition, 1) &&
        check_source(invalid_break, 2);
}

int main(int argc, char** argv) {
    if (!check_control_flow_semantics()) {
        fprintf(stderr, "Control-flow semantic checks failed.\\n");
        return 1;
    }
    if (argc != 3) return 2;
    if (!check_file(argv[1], 0, ERR_SEMANTIC_UNDEFINED)) return 1;
    if (!check_file(argv[2], 1, ERR_SEMANTIC_UNDEFINED)) return 1;
    return 0;
}
