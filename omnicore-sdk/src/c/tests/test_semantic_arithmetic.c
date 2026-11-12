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


static int check_error_detail(const char* source, const char* required_a,
    const char* required_b, int expected_line) {
    ParserContext* parser = parser_init_source(
        "semantic-diagnostic.nxf", (const uint8_t*)source, strlen(source));
    SemanticContext semantic;
    Error* error;
    int ok = 0;
    if (!parser) return 0;
    {
        int parse_result = parser_run(parser);
        if (parse_result != PARSER_STATUS_OK ||
            parser_get_status(parser) != PARSER_STATUS_OK || !parser->ast_root) {
            Error* parse_error;
            fprintf(stderr, "Diagnostic fixture parse failed: result=%d status=%d source=%s\\n",
                parse_result, parser_get_status(parser), source);
            for (parse_error = parser->error_head; parse_error; parse_error = parse_error->next)
                fprintf(stderr, "  parser error at %d:%d: %s\\n",
                    parse_error->line, parse_error->col,
                    parse_error->message ? parse_error->message : "<none>");
            parser_destroy(parser);
            return 0;
        }
    }
    semantic_init(&semantic, "semantic-diagnostic.nxf", NULL);
    semantic_check(&semantic, (ASTNode*)parser->ast_root);
    error = semantic.error_head;
    if (error && error->message && error->line == expected_line &&
        error->col > 0 && strstr(error->message, required_a) &&
        (!required_b || strstr(error->message, required_b))) {
        ok = 1;
    } else {
        fprintf(stderr,
            "Diagnostic detail mismatch: line=%d col=%d message='%s' expected line=%d text='%s'/'%s'\n",
            error ? error->line : 0, error ? error->col : 0,
            error && error->message ? error->message : "<none>",
            expected_line, required_a, required_b ? required_b : "<none>");
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
    static const char power_integer[] = "f:main() !i32 { ret 2 ^^ 3; }";
    static const char power_float[] = "f:main() !f64 { ret 2.0 ^^ 3.0; }";
    static const char power_non_numeric[] = "f:main() !bool { ret true ^^ false; }";
    static const char bitwise_valid[] = "f:main() !i32 { ret 7 & 3 | 2 ^ 1; }";
    static const char bitwise_float[] = "f:main() !f32 { ret 7.0 & 3.0; }";
    static const char shift_valid[] = "f:main() !i32 { ret 7 <<< 2 >>> 1; }";
    static const char shift_mixed_integer_types[] =
        "f:main(a!u8, b!u16) { v:c!u8 = a <<< b; }";
    static const char shift_float_invalid[] =
        "f:main() !f64 { ret 7.0 <<< 2.0; }";
    static const char shift_negative_invalid[] =
        "f:main() !i32 { ret 7 <<< -1; }";
    static const char shift_count_equal_width[] =
        "f:main() !i32 { ret 1 <<< 32; }";
    static const char shift_count_greater_width[] =
        "f:main() !i32 { ret 1 >>> 33; }";
    static const char shift_u8_equal_width[] =
        "f:main(a!u8) !u8 { ret a <<< 8; }";
    static const char shift_u8_valid_boundary[] =
        "f:main(a!u8) !u8 { ret a <<< 7; }";
    static const char shift_i64_valid_boundary[] =
        "f:main(a!i64) !i64 { ret a >>> 63; }";
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
    if (!check_source(power_integer, 0)) return 1;
    if (!check_source(power_float, 0)) return 1;
    if (!check_source(power_non_numeric, 1)) return 1;
    if (!check_source(bitwise_valid, 0)) return 1;
    if (!check_source(bitwise_float, 1)) return 1;
    if (!check_source(shift_valid, 0)) return 1;
    if (!check_source(shift_mixed_integer_types, 0)) return 1;
    if (!check_source(shift_float_invalid, 1)) return 1;
    if (!check_source(shift_negative_invalid, 1)) return 1;
    if (!check_source(shift_count_equal_width, 1)) return 1;
    if (!check_source(shift_count_greater_width, 1)) return 1;
    if (!check_source(shift_u8_equal_width, 1)) return 1;
    if (!check_source(shift_u8_valid_boundary, 0)) return 1;
    if (!check_source(shift_i64_valid_boundary, 0)) return 1;
    if (!check_source(logical_valid, 0)) return 1;
    if (!check_source(logical_invalid, 1)) return 1;
    if (!check_error_detail(
        "f:main(a!u8) !u8 {\n    ret a <<< 8;\n}",
        "8-bit width", "0..7", 2)) return 1;
    if (!check_error_detail(
        "f:main() !f32 {\n    ret 7.0 & 3.0;\n}",
        "Operator '&'", "f32", 2)) return 1;
    puts("Arithmetic semantic operand validation passed.");
    return 0;
}
