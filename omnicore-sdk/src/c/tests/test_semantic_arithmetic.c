#include "parser.h"
#include "semantic.h"

#include <stdio.h>
#include <string.h>

static int check_source(const char* source, int expect_error) {
    ParserContext* parser = parser_init_source(
        "semantic-arithmetic.nxf",
        (const uint8_t*)source, strlen(source));
    SemanticContext semantic;
    int ok;
    if (!parser) return 0;
    if (parser_run(parser) != PARSER_STATUS_OK ||
        parser_get_status(parser) != PARSER_STATUS_OK ||
        !parser->ast_root) {
        parser_destroy(parser);
        return 0;
    }

    semantic_init(&semantic, "semantic-arithmetic.nxf", NULL);
    semantic_check(&semantic, (ASTNode*)parser->ast_root);
    ok = expect_error ? semantic.error_count > 0 : semantic.error_count == 0;
    if (!ok) {
        fprintf(stderr, "Unexpected semantic result (expected_error=%d, errors=%d): %s\n",
            expect_error, semantic.error_count, source);
    }
    semantic_destroy(&semantic);
    parser_destroy(parser);
    return ok;
}

int main(void) {
    static const char integer_expression[] =
        "f:main() !i32 { ret 10 + 20 * 3; }";
    static const char float_expression[] =
        "f:main() !f64 { ret 10.0 + 2.5 * 3.0; }";
    static const char mismatched_operands[] =
        "f:main() !i32 { ret 10 + 2.5; }";
    static const char non_numeric_operands[] =
        "f:main() !bool { ret true - false; }";
    static const char comparison_condition[] =
        "f:main() !bool { ret 10 < 20; }";
    static const char invalid_logical_operands[] =
        "f:main() !bool { ret 1 && true; }";

    if (!check_source(integer_expression, 0)) return 1;
    if (!check_source(float_expression, 0)) return 1;
    if (!check_source(mismatched_operands, 1)) return 1;
    if (!check_source(non_numeric_operands, 1)) return 1;
    if (!check_source(comparison_condition, 0)) return 1;
    if (!check_source(invalid_logical_operands, 1)) return 1;
    puts("Arithmetic semantic operand validation passed.");
    return 0;
}
