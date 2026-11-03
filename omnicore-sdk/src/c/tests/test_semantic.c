#include "parser.h"
#include "ast.h"
#include "semantic.h"

#include <stdio.h>
#include <string.h>

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
        Error* parser_error;
        fprintf(stderr, "Parser rejected semantic fixture: %s\\n", source);
        for (parser_error = parser->error_head; parser_error; parser_error = parser_error->next) {
            fprintf(stderr, "  parser error %d at %d:%d: %s\\n", parser_error->code,
                parser_error->line, parser_error->col,
                parser_error->message ? parser_error->message : "(null)");
        }
        parser_destroy(parser);
        return 0;
    }
    semantic_init(&semantic, "semantic-control.nxf", NULL);
    semantic_check(&semantic, (ASTNode*)parser->ast_root);
    ok = semantic.error_count == expected_errors;
    if (!ok) { Error* e; fprintf(stderr, "Semantic test mismatch: expected=%d actual=%d source=%s\\n", expected_errors, semantic.error_count, source); for(e=semantic.error_head;e;e=e->next) fprintf(stderr,"  semantic error %d: %s\\n",e->code,e->message?e->message:"(null)"); }
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
    const char* valid_loop = "f:main() { loop (true) { break; continue; } }";
    const char* nested_function_break =
        "f:main() { loop (true) { f:nested() { break; } break; } }";
    const char* valid_conditional_actions =
        "f:main() { loop (true) { (true) -> break; (false) -> continue; } }";
    const char* invalid_conditional_actions =
        "f:main() { (true) -> break; (false) -> continue; }";
    return check_source(valid_if, 0) &&
        check_source(invalid_condition, 1) &&
        check_source(invalid_break, 2) &&
        check_source(valid_loop, 0) &&
        check_source(nested_function_break, 1) &&
        check_source(valid_conditional_actions, 0) &&
        check_source(invalid_conditional_actions, 2);
}

static int rejects_semantic_pending_node(ASTNodeType type) {
    ASTNode* node = ast_create(type);
    SemanticContext semantic;
    int rejected;
    if (!node) return 0;
    semantic_init(&semantic, "semantic-pending-node.nxf", NULL);
    semantic_check(&semantic, node);
    rejected = semantic.error_count > 0;
    semantic_destroy(&semantic);
    ast_destroy(node);
    return rejected;
}

static int check_semantic_fail_closed_nodes(void) {
    return rejects_semantic_pending_node(AST_TUPLE) &&
        rejects_semantic_pending_node(AST_ASM_BLOCK);
}

static int check_postfix_semantics(void) {
    const char* valid =
        "f:main() { v:counter!i32 = 0; counter++; counter--; }";
    const char* constant_operand =
        "f:main() { c:counter!i32 = 0; counter++; }";
    const char* non_numeric_operand =
        "f:main() { v:label!str = \"x\"; label++; }";
    const char* unresolved_operand =
        "f:main() { missing++; }";
    return check_source(valid, 0) &&
        check_source(constant_operand, 1) &&
        check_source(non_numeric_operand, 1) &&
        check_source(unresolved_operand, 1);
}

static int check_aggregate_member_semantics(void) {
    const char* valid =
        "f:main() { struct: Point { v:x!i32, v:y!i32 }"
        " v:p!Point; v:a!i32 = p.x; p.y = 2; }";
    const char* nested =
        "f:main() { struct: Inner { v:x!i32 }"
        " struct: Outer { v:inner!Inner } v:o!Outer;"
        " v:a!i32 = o.inner.x; o.inner.x = 3; }";
    const char* constant_object =
        "f:main() { struct: Point { v:x!i32 } c:p!Point; p.x = 2; }";
    const char* incompatible =
        "f:main() { struct: Point { v:x!i32 } v:p!Point; p.x = \"bad\"; }";
    const char* unknown_member =
        "f:main() { struct: Point { v:x!i32 } v:p!Point; v:a = p.missing; }";
    const char* non_aggregate =
        "f:main() { v:value!i32 = 1; v:bad = value.member; }";
    return check_source(valid, 0) &&
        check_source(nested, 0) &&
        check_source(constant_object, 1) &&
        check_source(incompatible, 1) &&
        check_source(unknown_member, 1) &&
        check_source(non_aggregate, 1);
}

static int check_array_literal_semantics(void) {
    const char* valid =
        "f:main() { v:values = [1, 2, 3]; v:matrix = [[1, 2], [3, 4]]; v:empty = []; }";
    const char* mixed =
        "f:main() { v:invalid = [1, \"two\"]; }";
    return check_source(valid,0) && check_source(mixed,1);
}

static int check_compound_assignment_semantics(void) {
    const char* valid_numeric = "f:main() { v:x!i32 = 1; x += 2; x *= 3; }";
    const char* invalid_numeric = "f:main() { v:x!str = \"a\"; x += \"b\"; }";
    const char* invalid_bitwise_float = "f:main() { v:x!f32 = 1.0; x &= 1.0; }";
    const char* valid_bitwise = "f:main() { v:x!u32 = 1; x |= 2; x ^= 3; }";
    return check_source(valid_numeric,0) && check_source(invalid_numeric,1) &&
        check_source(invalid_bitwise_float,1) && check_source(valid_bitwise,0);
}

static int check_array_type_semantics(void) {
    const char* valid_params = "f:consume(a![], b!i32[], third:i32[5]) {}";
    const char* valid_matrix_param = "f:consume_matrix(matrix_data:f64[2][2]) {}";
    const char* valid =
        "f:consume(a![], b!i32[], third:i32[5]) {}"
        "f:consume_matrix(matrix_data:f64[2][2]) {}"
        "f:main() { v:dynamic[]; v:fixed[5]!i32 = [1,2,3,4,5];"
        " v:matrix[2][2]!f64 = [[1.0,2.0],[3.0,4.0]];"
        " v:computed[2+3]!i32 = [1,2,3,4,5];"
        " v:modulo_extent[17%5]!i32 = [1,2];"
        " consume(dynamic,fixed,computed); consume_matrix(matrix); }";
    const char* zero_extent = "f:main() { v:invalid[0]!i32; }";
    const char* fractional_extent = "f:main() { v:invalid[1.5]!i32; }";
    const char* zero_expression = "f:main() { v:invalid[3-3]!i32; }";
    const char* division_by_zero = "f:main() { v:invalid[8/0]!i32; }";
    const char* overflow_extent = "f:main() { v:invalid[9223372036854775807+1]!i32; }";
    const char* negative_extent = "f:main() { v:invalid[-1]!i32; }";
    const char* wrong_shape = "f:main() { v:fixed[3]!i32 = [1,2]; }";
    const char* wrong_element = "f:main() { v:fixed[2]!i32 = [1.5,2.5]; }";
    const char* call_element_mismatch =
        "f:take(values:i32[4]) {} f:main() { v:wrong[4]!f32; take(wrong); }";
    const char* call_extent_mismatch =
        "f:take(values:i32[4]) {} f:main() { v:wrong[5]!i32; take(wrong); }";
    const char* call_rank_mismatch =
        "f:take(values:i32[2][2]) {} f:main() { v:wrong[4]!i32; take(wrong); }";
    const char* call_literal_valid =
        "f:take(values:i32[2]) {} f:main() { take([1,2]); }";
    const char* call_literal_element_mismatch =
        "f:take(values:i32[1]) {} f:main() { take([1.5]); }";
    const char* valid_index =
        "f:main() { v:items[4]!i32; v:x = items[1+2]; items[0] = x; }";
    const char* invalid_index_target = "f:main() { v:x!i32 = 2; v:y = x[0]; }";
    const char* invalid_index_type = "f:main() { v:x!i32 = 2; v:y = x[1.5]; }";
    const char* const_array_index_write = "f:main() { c:items[2]!i32 = [1,2]; items[0] = 3; }";
    const char* indexed_element_type_mismatch = "f:main() { v:items[2]!i32 = [1,2]; items[0] = \"x\"; }";
    const char* non_lvalue_assignment = "f:main() { v:x!i32 = 1; (x + 1) = 2; }";
    return check_source(valid_params,0) &&
        check_source(valid_matrix_param,0) &&
        check_source(valid,0) &&
        check_source(zero_extent,1) &&
        check_source(fractional_extent,1) &&
        check_source(zero_expression,1) &&
        check_source(division_by_zero,2) &&
        check_source(overflow_extent,1) &&
        check_source(negative_extent,1) &&
        check_source(wrong_shape,1) &&
        check_source(wrong_element,2) &&
        check_source(call_element_mismatch,1) &&
        check_source(call_extent_mismatch,1) &&
        check_source(call_rank_mismatch,1) &&
        check_source(call_literal_valid,0) &&
        check_source(call_literal_element_mismatch,1) &&
        check_source(valid_index,0) &&
        check_source(invalid_index_target,1) &&
        check_source(invalid_index_type,2) &&
        check_source(const_array_index_write,1) &&
        check_source(indexed_element_type_mismatch,1) &&
        check_source(non_lvalue_assignment,1);
}

static int check_extended_ast_semantics(void) {
    const char* valid =
        "f:main() {"
        " v:choice = 1;"
        " select(choice) { Range(s,e) when s == e -> { v:seen = s; } default -> { } }"
        " scan(choice) { Range(a,b) when a == b && b == a -> { v:seen2 = b; } else -> { } }"
        " rules PrimeBorders(n) {"
        "  n < 2 :=> \"out of range\";"
        "  n == 2 -> Success;"
        "  always -> true;"
        "  default -> false;"
        " }"
        " apply rules(PrimeBorders(2));"
        " on PrimeBorders.Success { v:handled = true; }"
        " v:callback = f:(x) > x;"
        " v:callback_block = f:(y) { ret y; };"
        " v:data = 1;"
        " unroll(3) -> loop(item in data) { v:copy = item; }"
        " v:src!i32 = 1; v:dst!i32 = 2; v:moved = src _> dst;"
        "}";
    const char* leaked_pattern =
        "f:main() { v:choice = 1; select(choice) { Range(s,e) -> { } } v:leak = s; }";
    const char* unknown_apply =
        "f:main() { apply rules(Missing(1)); }";
    const char* unknown_handler =
        "f:main() { on Missing.Success { } }";
    const char* unknown_event =
        "f:main() { rules R() { true -> Success; } on R.Missing { } }";
    const char* duplicate_pattern =
        "f:main() { v:choice = 1; select(choice) { Range(s,s) -> { } } }";
    const char* invalid_guard =
        "f:main() { v:choice = 1; scan(choice) { Range(a,b) when 1 -> { } } }";
    const char* relocate_type_mismatch =
        "f:main() { v:src!i32 = 1; v:dst!str = \"x\"; v:moved = src _> dst; }";
    return check_source(valid,0) &&
        check_source(leaked_pattern,1) &&
        check_source(unknown_apply,1) &&
        check_source(unknown_handler,1) &&
        check_source(unknown_event,1) &&
        check_source(duplicate_pattern,1) &&
        check_source(invalid_guard,1) &&
        check_source(relocate_type_mismatch,1);
}

int main(int argc, char** argv) {
    if (!check_semantic_fail_closed_nodes()) {
        fprintf(stderr, "Semantic pending AST nodes were not rejected.\\n");
        return 1;
    }
    if (!check_postfix_semantics()) {
        fprintf(stderr, "Postfix semantic checks failed.\n");
        return 1;
    }
    if (!check_compound_assignment_semantics()) {
        fprintf(stderr, "Compound assignment semantic checks failed.\\n");
        return 1;
    }
    if (!check_array_type_semantics()) {
        fprintf(stderr, "Array type semantic checks failed.\n");
        return 1;
    }
    if (!check_aggregate_member_semantics()) {
        fprintf(stderr, "Aggregate member semantic checks failed.\n");
        return 1;
    }
    if (!check_array_literal_semantics()) {
        fprintf(stderr, "Array literal semantic checks failed.\n");
        return 1;
    }
    if (!check_extended_ast_semantics()) {
        fprintf(stderr, "Extended AST semantic scope/effect checks failed.\n");
        return 1;
    }
    if (!check_control_flow_semantics()) {
        fprintf(stderr, "Control-flow semantic checks failed.\n");
        return 1;
    }
    if (argc != 3) return 2;
    if (!check_file(argv[1], 0, ERR_SEMANTIC_UNDEFINED)) return 1;
    if (!check_file(argv[2], 1, ERR_SEMANTIC_UNDEFINED)) return 1;
    return 0;
}
