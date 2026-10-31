#ifdef NDEBUG
#undef NDEBUG
#endif
#include "semantic_result.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    SemanticResult result;
    uint32_t scope_inner = 0u, scope_nested = 0u;
    uint32_t type_i32 = 0u, type_i32_hard = 0u, type_mode = 0u, type_point = 0u;
    uint32_t symbol_function = 0u, symbol_outer_value = 0u;
    uint32_t symbol_inner_value = 0u, symbol_mode = 0u, symbol_point = 0u;

    assert(semantic_result_init(&result) == SEMANTIC_RESULT_OK);
    assert(result.scope_count == 1u && result.scopes[0].scope_id == 1u);

    assert(semantic_result_add_scope(&result, 1u, 1u, 0u, 2u,
        &scope_inner) == SEMANTIC_RESULT_OK);
    assert(semantic_result_add_scope(&result, scope_inner, 2u, 0u, 3u,
        &scope_nested) == SEMANTIC_RESULT_OK);

    /* Canonical type interning is stable; hard type remains a distinct identity. */
    assert(semantic_result_intern_type(&result, SEMANTIC_RESULT_TYPE_PRIMITIVE,
        0u, "i32", 0u, 0u, &type_i32) == SEMANTIC_RESULT_OK);
    assert(semantic_result_intern_type(&result, SEMANTIC_RESULT_TYPE_PRIMITIVE,
        0u, "i32", 0u, 0u, &type_i32_hard) == SEMANTIC_RESULT_OK);
    assert(type_i32 == type_i32_hard && type_i32 == 1u);
    assert(semantic_result_intern_type(&result, SEMANTIC_RESULT_TYPE_PRIMITIVE,
        SEMANTIC_RESULT_TYPE_FLAG_HARD, "i32", 0u, 0u,
        &type_i32_hard) == SEMANTIC_RESULT_OK);
    assert(type_i32_hard == 2u && type_i32_hard != type_i32);

    /* Forward declaration: symbol is recorded before its final type is known. */
    assert(semantic_result_add_symbol(&result, 1u, 4u,
        SEMANTIC_RESULT_SYMBOL_FUNCTION, SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC,
        "forward_fn", 0u, &symbol_function) == SEMANTIC_RESULT_OK);
    assert(symbol_function == 1u);

    /* Same name is allowed in nested scopes and resolves to the nearest declaration. */
    assert(semantic_result_add_symbol(&result, scope_inner, 5u,
        SEMANTIC_RESULT_SYMBOL_VARIABLE, 0u, "value", type_i32,
        &symbol_outer_value) == SEMANTIC_RESULT_OK);
    assert(semantic_result_add_symbol(&result, scope_nested, 6u,
        SEMANTIC_RESULT_SYMBOL_VARIABLE, SEMANTIC_RESULT_SYMBOL_FLAG_HARD,
        "value", type_i32_hard, &symbol_inner_value) == SEMANTIC_RESULT_OK);
    assert(symbol_outer_value == 2u && symbol_inner_value == 3u);
    assert(semantic_result_find_visible_symbol(&result, scope_nested, "value") ==
        symbol_inner_value);
    assert(semantic_result_find_visible_symbol(&result, scope_inner, "value") ==
        symbol_outer_value);
    assert(semantic_result_find_visible_symbol(&result, 1u, "value") == 0u);
    assert(semantic_result_find_visible_symbol(&result, scope_nested, "forward_fn") ==
        symbol_function);

    /* Type declarations may be referenced and completed after their symbols exist. */
    assert(semantic_result_add_symbol(&result, 1u, 7u,
        SEMANTIC_RESULT_SYMBOL_ENUM, SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC,
        "Mode", 0u, &symbol_mode) == SEMANTIC_RESULT_OK);
    assert(semantic_result_intern_type(&result, SEMANTIC_RESULT_TYPE_USER_DEFINED,
        0u, "Mode", symbol_mode, type_i32, &type_mode) == SEMANTIC_RESULT_OK);
    assert(semantic_result_set_symbol_type(&result, symbol_mode, type_mode) ==
        SEMANTIC_RESULT_OK);
    assert(semantic_result_set_symbol_type(&result, symbol_function, type_i32) ==
        SEMANTIC_RESULT_OK);

    assert(semantic_result_add_symbol(&result, 1u, 12u,
        SEMANTIC_RESULT_SYMBOL_STRUCT, SEMANTIC_RESULT_SYMBOL_FLAG_PACKED,
        "Point", 0u, &symbol_point) == SEMANTIC_RESULT_OK);
    assert(semantic_result_intern_type(&result, SEMANTIC_RESULT_TYPE_USER_DEFINED,
        0u, "Point", symbol_point, 0u, &type_point) == SEMANTIC_RESULT_OK);
    assert(semantic_result_set_symbol_type(&result, symbol_point, type_point) ==
        SEMANTIC_RESULT_OK);

    /* Insert relations out of order; the model canonicalizes owner/kind/ordinal. */
    assert(semantic_result_add_relation(&result, symbol_function,
        SEMANTIC_RESULT_RELATION_FUNCTION_PARAMETER, 1u, 0u, 9u,
        "y", type_i32_hard) == SEMANTIC_RESULT_OK);
    assert(semantic_result_add_relation(&result, symbol_function,
        SEMANTIC_RESULT_RELATION_FUNCTION_PARAMETER, 0u, 0u, 8u,
        "x", type_i32) == SEMANTIC_RESULT_OK);
    assert(semantic_result_add_relation(&result, symbol_mode,
        SEMANTIC_RESULT_RELATION_ENUM_VARIANT, 0u, 0u, 10u,
        "Some", type_mode) == SEMANTIC_RESULT_OK);
    assert(semantic_result_add_relation(&result, symbol_mode,
        SEMANTIC_RESULT_RELATION_ENUM_PAYLOAD_FIELD, 0u, 0u, 11u,
        "code", type_i32) == SEMANTIC_RESULT_OK);
    assert(semantic_result_add_relation(&result, symbol_point,
        SEMANTIC_RESULT_RELATION_AGGREGATE_FIELD, 0u, 0u, 13u,
        "x", type_i32) == SEMANTIC_RESULT_OK);

    /* Annotations are maintained in ascending AST node order. */
    assert(semantic_result_set_annotation(&result, 8u, scope_nested,
        symbol_function, type_i32, SEMANTIC_RESULT_VALUE_FUNCTION_DESIGNATOR,
        0u) == SEMANTIC_RESULT_OK);
    assert(semantic_result_set_annotation(&result, 5u, scope_inner,
        symbol_outer_value, type_i32, SEMANTIC_RESULT_VALUE_LVALUE,
        0u) == SEMANTIC_RESULT_OK);
    assert(semantic_result_set_annotation(&result, 6u, scope_nested,
        symbol_inner_value, type_i32_hard, SEMANTIC_RESULT_VALUE_LVALUE,
        0u) == SEMANTIC_RESULT_OK);
    assert(result.annotations[0].node_id == 5u &&
        result.annotations[1].node_id == 6u &&
        result.annotations[2].node_id == 8u);

    assert(semantic_result_add_conversion(&result, 8u, 0u,
        type_i32, type_i32_hard, SEMANTIC_RESULT_CONVERSION_EXPLICIT_CAST,
        SEMANTIC_RESULT_CONVERSION_FLAG_EXPLICIT) == SEMANTIC_RESULT_OK);

    assert(semantic_result_finalize(&result, 13u) == SEMANTIC_RESULT_OK);
    assert(result.finalized == 1u);

    /* Relation ranges are now stable 1-based slices in the relation table. */
    assert(result.symbols[symbol_function - 1u].first_relation == 1u &&
        result.symbols[symbol_function - 1u].relation_count == 2u);
    assert(result.symbols[symbol_mode - 1u].first_relation == 3u &&
        result.symbols[symbol_mode - 1u].relation_count == 2u);
    assert(result.symbols[symbol_point - 1u].first_relation == 5u &&
        result.symbols[symbol_point - 1u].relation_count == 1u);
    assert(result.relations[0].ordinal == 0u &&
        result.relations[1].ordinal == 1u);

    /* Finalized results are immutable; callers must start a new invocation to edit. */
    {
        uint32_t ignored = 0u;
        assert(semantic_result_add_scope(&result, 1u, 1u, 0u, 14u,
            &ignored) == SEMANTIC_RESULT_INVALID_DATA);
    }
    semantic_result_destroy(&result);

    /* Sibling-scope symbols are not visible from an unrelated lexical branch. */
    {
        SemanticResult bad;
        uint32_t left = 0u, right = 0u, symbol = 0u;
        assert(semantic_result_init(&bad) == SEMANTIC_RESULT_OK);
        assert(semantic_result_add_scope(&bad, 1u, 1u, 0u, 2u, &left) ==
            SEMANTIC_RESULT_OK);
        assert(semantic_result_add_scope(&bad, 1u, 1u, 0u, 3u, &right) ==
            SEMANTIC_RESULT_OK);
        assert(semantic_result_add_symbol(&bad, right, 4u,
            SEMANTIC_RESULT_SYMBOL_VARIABLE, 0u, "sibling", 0u,
            &symbol) == SEMANTIC_RESULT_OK);
        assert(semantic_result_set_annotation(&bad, 5u, left, symbol, 0u,
            SEMANTIC_RESULT_VALUE_LVALUE, 0u) == SEMANTIC_RESULT_OK);
        assert(semantic_result_finalize(&bad, 5u) ==
            SEMANTIC_RESULT_INVALID_DATA);
        semantic_result_destroy(&bad);
    }

    /* Scope owner nodes must be unique. */
    {
        SemanticResult bad;
        uint32_t first = 0u, second = 0u;
        assert(semantic_result_init(&bad) == SEMANTIC_RESULT_OK);
        assert(semantic_result_add_scope(&bad, 1u, 1u, 0u, 2u, &first) ==
            SEMANTIC_RESULT_OK);
        assert(semantic_result_add_scope(&bad, 1u, 1u, 0u, 2u, &second) ==
            SEMANTIC_RESULT_OK);
        assert(semantic_result_finalize(&bad, 2u) ==
            SEMANTIC_RESULT_INVALID_DATA);
        semantic_result_destroy(&bad);
    }

    /* A relation ordinal gap cannot be finalized. */
    {
        SemanticResult bad;
        uint32_t function = 0u;
        assert(semantic_result_init(&bad) == SEMANTIC_RESULT_OK);
        assert(semantic_result_add_symbol(&bad, 1u, 2u,
            SEMANTIC_RESULT_SYMBOL_FUNCTION, 0u, "f", 0u,
            &function) == SEMANTIC_RESULT_OK);
        assert(semantic_result_add_relation(&bad, function,
            SEMANTIC_RESULT_RELATION_FUNCTION_PARAMETER, 0u, 0u, 3u,
            "a", 0u) == SEMANTIC_RESULT_OK);
        assert(semantic_result_add_relation(&bad, function,
            SEMANTIC_RESULT_RELATION_FUNCTION_PARAMETER, 2u, 0u, 4u,
            "b", 0u) == SEMANTIC_RESULT_OK);
        assert(semantic_result_finalize(&bad, 4u) ==
            SEMANTIC_RESULT_INVALID_DATA);
        semantic_result_destroy(&bad);
    }

    /* A result with only the root scope is a valid empty translation unit. */
    {
        SemanticResult empty;
        assert(semantic_result_init(&empty) == SEMANTIC_RESULT_OK);
        assert(semantic_result_finalize(&empty, 1u) == SEMANTIC_RESULT_OK);
        semantic_result_destroy(&empty);
    }

    puts("Persistent semantic result model tests passed.");
    return 0;
}
