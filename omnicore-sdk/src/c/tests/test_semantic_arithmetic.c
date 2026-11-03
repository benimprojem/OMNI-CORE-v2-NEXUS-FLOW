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
    static const char modulo_integer[] =
        "f:main() !i32 { ret 17 % 5; }";
    static const char modulo_float[] =
        "f:main() !f64 { ret 17.0 % 5.0; }";
    static const char modulo_zero[] =
        "f:main() !i32 { ret 17 % 0; }";
    static const char compound_valid[] = "f:main() { v:x = 1; x += 2; v:s = \"a\"; s .= \"b\"; }";
    static const char compound_float[] = "f:main() { v:x = 1; x += 2.0; }";
    static const char compound_bitwise_float[] = "f:main() { v:x = 1; x &= 2.0; }";
    static const char append_non_string[] = "f:main() { v:x = 1; x .= \"b\"; }";
    static const char invalid_lvalue[] = "f:main() { 1 = 2; }";
    static const char bitwise_valid[] = "f:main() !i32 { ret 7 & 3 | 2 ^ 1; }";
    static const char bitwise_float[] = "f:main() !f32 { ret 7.0 & 3.0; }";
    static const char logical_valid[] =
        "f:main() !bool { ret (1 < 2) && (3 < 4) || false; }";
    static const char logical_invalid[] =
        "f:main() !bool { ret 1 && true; }";
    if (!check_source(integer_expression, 0)) return 1;
    if (!check_source(float_expression, 0)) return 1;
    if (!check_source(mismatched_operands, 1)) return 1;
    if (!check_source(non_numeric_operands, 1)) return 1;
    if (!check_source(comparison_condition, 0)) return 1;
    if (!check_source(modulo_integer, 0)) return 1;
    if (!check_source(modulo_float, 1)) return 1;
    if (!check_source(modulo_zero, 1)) return 1;
    if (!check_source(compound_valid, 0)) return 1;
    if (!check_source(compound_float, 1)) return 1;
    if (!check_source(compound_bitwise_float, 1)) return 1;
    if (!check_source(append_non_string, 1)) return 1;
    if (!check_source(invalid_lvalue, 1)) return 1;
    if (!check_source(bitwise_valid, 0)) return 1;
    if (!check_source(bitwise_float, 1)) return 1;
    if (!check_source(logical_valid, 0)) return 1;
    if (!check_source(logical_invalid, 1)) return 1;
    puts("Arithmetic semantic operand validation passed.");
    return 0;
}
