#include "semantic.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>
#include "lexer.h"

static int semantic_slot_is_nominal_type(const Slot* slot);

void semantic_init(SemanticContext* ctx, const char* filename, const char* lang_file_path) {
    (void)lang_file_path; /* Reserved for the shared diagnostics localization API. */
    ctx->head = NULL;
    ctx->filename = filename;
    ctx->error_count = 0;
    ctx->error_head = NULL;
    ctx->error_tail = NULL;
    ctx->current_depth = 0;
    ctx->loop_depth = 0u;
    ctx->function_depth = 0u;
    ctx->current_func_return_type = NULL;
    ctx->diagnostics = NULL;
    ctx->diagnostic_stage_id = OCC_STAGE_SEMANTIC;
    ctx->current_scope_id = 1u;
    ctx->current_group_owner = NULL;
    ctx->current_fast_exec_owner = NULL;
    memset(&ctx->lang, 0, sizeof(ctx->lang));
    ctx->current_method_receiver_type = NULL;
    ctx->current_method_public_name = NULL;
    ctx->current_method_receiver_type_id = 0u;
    ctx->current_method_context = 0u;
    ctx->current_method_is_object = 0u;
    ctx->suppress_group_default_dispatch = 0u;
    ctx->node_id_resolver = NULL;
    ctx->node_id_context = NULL;
    ctx->result_enabled = 0u;
    ctx->result_status = semantic_result_init(&ctx->result);
    ctx->test_allocation_fail_at = 0u;
    ctx->test_allocation_attempts = 0u;
}

// --- Similarity Matching (ULTRA) ---
void semantic_attach_diagnostics(SemanticContext* ctx, OccDiagnosticCollector* collector, uint32_t stage_id) {
    if (!ctx) return;
    ctx->diagnostics = collector;
    ctx->diagnostic_stage_id = stage_id ? stage_id : OCC_STAGE_SEMANTIC;
}

void semantic_attach_node_id_resolver(
    SemanticContext* ctx, SemanticNodeIdResolver resolver, void* user_context
) {
    if (!ctx) return;
    ctx->node_id_resolver = resolver;
    ctx->node_id_context = user_context;
    ctx->result_enabled = (uint8_t)(
        resolver != NULL && ctx->result_status == SEMANTIC_RESULT_OK);
    ctx->current_scope_id = 1u;
}

SemanticResultStatus semantic_finalize_result(
    SemanticContext* ctx, uint32_t ast_node_count
) {
    if (!ctx) return SEMANTIC_RESULT_INVALID_ARGUMENT;
    if (!ctx->result_enabled) return SEMANTIC_RESULT_INVALID_ARGUMENT;
    if (ctx->result_status != SEMANTIC_RESULT_OK) return ctx->result_status;
    if (ctx->error_count != 0) {
        ctx->result_status = SEMANTIC_RESULT_INVALID_DATA;
        return ctx->result_status;
    }
    ctx->result_status = semantic_result_finalize(&ctx->result, ast_node_count);
    return ctx->result_status;
}

static int semantic_should_fail_allocation(SemanticContext* ctx) {
    if (!ctx || ctx->test_allocation_fail_at == 0u) return 0;
    if (ctx->test_allocation_attempts < UINT32_MAX)
        ++ctx->test_allocation_attempts;
    return ctx->test_allocation_attempts == ctx->test_allocation_fail_at;
}

static void* semantic_allocate(SemanticContext* ctx, size_t size) {
    if (semantic_should_fail_allocation(ctx)) return NULL;
    return malloc(size);
}

static void* semantic_callocate(
    SemanticContext* ctx, size_t count, size_t size
) {
    if (semantic_should_fail_allocation(ctx)) return NULL;
    return calloc(count, size);
}

static char indexed_element_type_buffer[64];

static char* semantic_duplicate(SemanticContext* ctx, const char* value) {
    size_t length;
    char* copy;
    if (!value) value = "";
    length = strlen(value);
    if (length == SIZE_MAX) return NULL;
    copy = (char*)semantic_allocate(ctx, length + 1u);
    if (copy) memcpy(copy, value, length + 1u);
    return copy;
}

void semantic_test_fail_allocation_at(
    SemanticContext* ctx, uint32_t allocation_index
) {
    if (!ctx) return;
    ctx->test_allocation_fail_at = allocation_index;
    ctx->test_allocation_attempts = 0u;
}

uint32_t semantic_test_get_allocation_attempts(
    const SemanticContext* ctx
) {
    return ctx ? ctx->test_allocation_attempts : 0u;
}

static void semantic_set_result_error(
    SemanticContext* ctx, SemanticResultStatus status
) {
    if (ctx && ctx->result_status == SEMANTIC_RESULT_OK &&
        status != SEMANTIC_RESULT_OK) {
        ctx->result_status = status;
    }
}

static uint32_t semantic_node_id(
    const SemanticContext* ctx, const ASTNode* node
) {
    if (!ctx || !ctx->result_enabled || !ctx->node_id_resolver || !node) return 0u;
    return ctx->node_id_resolver(ctx->node_id_context, node);
}

static int levenshtein(const char* s1, const char* s2) {
    size_t input_len1 = strlen(s1), input_len2 = strlen(s2);
    int len1 = (int)(input_len1 > 127u ? 127u : input_len1);
    int len2 = (int)(input_len2 > 127u ? 127u : input_len2);
    int matrix[128][128];
    for (int i = 0; i <= len1; i++) matrix[i][0] = i;
    for (int j = 0; j <= len2; j++) matrix[0][j] = j;
    for (int i = 1; i <= len1; i++) {
        for (int j = 1; j <= len2; j++) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            int a = matrix[i - 1][j] + 1;
            int b = matrix[i][j - 1] + 1;
            int c = matrix[i - 1][j - 1] + cost;
            if (a < b) matrix[i][j] = (a < c) ? a : c;
            else matrix[i][j] = (b < c) ? b : c;
        }
    }
    return matrix[len1][len2];
}

static Slot* find_best_suggestion(SemanticContext* ctx, const char* name, int* out_score) {
    Slot* current = ctx->head;
    Slot* best_slot = NULL;
    int best_score = 0;
    while (current) {
        if (!((semantic_slot_is_nominal_type(current) ||
               current->kind == SLOT_FUNC) &&
              current->group_owner &&
              current->group_owner != ctx->current_group_owner)) {
            int dist = levenshtein(name, current->name);
            size_t max_len = strlen(name) > strlen(current->name)
                ? strlen(name) : strlen(current->name);
            double denominator = max_len ? (double)max_len : 1.0;
            int score = (int)((1.0 - (double)dist / denominator) * 100);
            if (score > best_score) {
                best_score = score;
                best_slot = current;
            }
        }
        current = current->next;
    }
    *out_score = best_score;
    return (best_score > 60) ? best_slot : NULL;
}

static void report_issue(SemanticContext* ctx, ASTNode* node, ErrorCode code, const char* msg, const char* suggestion, const char* hint, int similarity, int is_warning) {
    if (ctx->diagnostics) {
        OccDiagnosticInput input = {0};
        input.code = (uint32_t)code;
        input.message_id = (uint32_t)code;
        input.stage_id = ctx->diagnostic_stage_id ? ctx->diagnostic_stage_id : OCC_STAGE_SEMANTIC;
        input.severity = is_warning ? OCC_DIAG_WARNING : OCC_DIAG_ERROR;
        if (node) {
            /* Preserve the complete parser-provided range, not only its start. */
            input.span = node->source_span;
            if (!input.span.has_location && node->line > 0 && node->col > 0) {
                input.span.start_line = (uint32_t)node->line;
                input.span.start_column = (uint32_t)node->col;
                input.span.end_line = input.span.start_line;
                input.span.end_column = input.span.start_column;
                input.span.has_location = 1u;
            }
        }
        input.message = msg ? msg : "";
        input.message_length = strlen(input.message);
        input.hint = hint ? hint : suggestion;
        input.hint_length = input.hint ? strlen(input.hint) : 0u;
        (void)occ_diagnostics_add(ctx->diagnostics, &input);
    }
    Error* err = (Error*)semantic_callocate(ctx, 1u, sizeof(Error));
    if (!err) {
        if (ctx->result_status == SEMANTIC_RESULT_OK)
            ctx->result_status = SEMANTIC_RESULT_OUT_OF_MEMORY;
        if (ctx->error_count < INT_MAX) ++ctx->error_count;
        return;
    }

    err->code = code;
    err->filename = semantic_duplicate(ctx, ctx->filename ? ctx->filename : "<unknown>");
    err->line = node ? (node->line > 0 ? node->line :
        (node->source_span.has_location ? (int)node->source_span.start_line : 0)) : 0;
    err->col = node ? (node->col > 0 ? node->col :
        (node->source_span.has_location ? (int)node->source_span.start_column : 0)) : 0;
    err->message = semantic_duplicate(ctx, msg ? msg : "");
    err->suggestion = suggestion ? semantic_duplicate(ctx, suggestion) : NULL;
    err->hint = hint ? semantic_duplicate(ctx, hint) : NULL;
    err->similarity = similarity;
    err->is_warning = is_warning;

    if (!err->filename || !err->message ||
        (suggestion && !err->suggestion) || (hint && !err->hint)) {
        free(err->filename);
        free(err->message);
        free(err->suggestion);
        free(err->hint);
        free(err);
        if (ctx->result_status == SEMANTIC_RESULT_OK)
            ctx->result_status = SEMANTIC_RESULT_OUT_OF_MEMORY;
        if (ctx->error_count < INT_MAX) ++ctx->error_count;
        return;
    }

    if (ctx->error_tail) {
        ctx->error_tail->next = err;
        ctx->error_tail = err;
    } else {
        ctx->error_head = err;
        ctx->error_tail = err;
    }
    if (ctx->error_count < INT_MAX) ++ctx->error_count;
}

static void report_error(SemanticContext* ctx, ASTNode* node, ErrorCode code, const char* msg) {
    report_issue(ctx, node, code, msg, NULL, NULL, 0, 0);
}

static int semantic_slot_is_nominal_type(const Slot* slot) {
    return slot && (slot->kind == SLOT_STRUCT || slot->kind == SLOT_UNION ||
        slot->kind == SLOT_ENUM);
}

static Slot* find_slot(SemanticContext* ctx, const char* name) {
    Slot* current = ctx ? ctx->head : NULL;
    if (!name) return NULL;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            /*
             * A nominal type declared by a Group is visible only while
             * checking members of that exact Group. A nested/sibling Group
             * must not inherit the enclosing Group's private type namespace.
             * Types without a Group owner retain ordinary lexical visibility.
             */
            if (((semantic_slot_is_nominal_type(current) ||
                  current->kind == SLOT_FUNC) &&
                 current->group_owner &&
                 current->group_owner != ctx->current_group_owner)) {
                current = current->next;
                continue;
            }
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static int is_primitive_type_for_result(const char* name) {
    static const char* const names[] = {
        "i32", "u8", "str", "bool", "f64", "i64", "u64", "u32",
        "u16", "i16", "i8", "f32", "d32", "d64", "char", "bit", "byte",
        "8bit", "16bit", "32bit", "64bit", "128bit",
        "1byte", "2byte", "4byte", "8byte", "16byte", "void", "any"
    };
    size_t i;
    for (i = 0u; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (strcmp(name, names[i]) == 0) return 1;
    }
    return 0;
}

static uint16_t semantic_result_symbol_kind(SlotKind kind) {
    switch (kind) {
        case SLOT_VAR: return SEMANTIC_RESULT_SYMBOL_VARIABLE;
        case SLOT_FUNC: return SEMANTIC_RESULT_SYMBOL_FUNCTION;
        case SLOT_STRUCT: return SEMANTIC_RESULT_SYMBOL_STRUCT;
        case SLOT_UNION: return SEMANTIC_RESULT_SYMBOL_UNION;
        case SLOT_ENUM: return SEMANTIC_RESULT_SYMBOL_ENUM;
        case SLOT_NEWTYPE: return SEMANTIC_RESULT_SYMBOL_NEWTYPE;
        case SLOT_LABEL: return SEMANTIC_RESULT_SYMBOL_LABEL;
        case SLOT_GROUP: return SEMANTIC_RESULT_SYMBOL_GROUP;
        default: return 0u;
    }
}

static uint32_t semantic_type_id(
    SemanticContext* ctx, const char* type_name, int force_hard
) {
    const char* name = type_name;
    uint16_t flags = 0u;
    uint16_t kind;
    uint32_t referenced_symbol_id = 0u;
    uint32_t base_type_id = 0u;
    uint32_t type_id = 0u;
    Slot* declared_type = NULL;
    SemanticResultStatus status;

    if (!ctx || !ctx->result_enabled ||
        ctx->result_status != SEMANTIC_RESULT_OK ||
        !type_name || !*type_name) return 0u;
    if (*name == '!') {
        flags |= SEMANTIC_RESULT_TYPE_FLAG_HARD;
        ++name;
    }
    if (force_hard) flags |= SEMANTIC_RESULT_TYPE_FLAG_HARD;
    if (!*name) return 0u;

    /* Pointer/reference layers are structural SemanticResult types.
       Each layer points to the immediately underlying type ID. */
    if (*name == '*' || *name == '&') {
        const char* layer = name;
        uint32_t underlying;
        if (*name == '&') {
            if (name[1] == '\0') return 0u;
            underlying = semantic_type_id(ctx, name + 1, 0);
            kind = SEMANTIC_RESULT_TYPE_REFERENCE;
            layer = "&";
        } else {
            size_t depth = 0u;
            while (name[depth] == '*') ++depth;
            if (name[depth] == '\0') return 0u;
            underlying = semantic_type_id(ctx, name + depth, 0);
            if (!underlying) return 0u;
            kind = SEMANTIC_RESULT_TYPE_POINTER;
            layer = "*";
            while (depth > 1u) {
                status = semantic_result_intern_type(
                    &ctx->result, kind, 0u, layer, 0u, underlying, &underlying);
                semantic_set_result_error(ctx, status);
                if (status != SEMANTIC_RESULT_OK) return 0u;
                --depth;
            }
        }
        if (!underlying) return 0u;
        status = semantic_result_intern_type(
            &ctx->result, kind, flags, layer, 0u, underlying, &type_id);
        semantic_set_result_error(ctx, status);
        return status == SEMANTIC_RESULT_OK ? type_id : 0u;
    }

    if (strcmp(name, "array") == 0) {
        kind = SEMANTIC_RESULT_TYPE_AGGREGATE;
    } else if (strncmp(name, "f:(", 3u) == 0) {
        kind = SEMANTIC_RESULT_TYPE_FUNCTION;
    } else if (strcmp(name, "tuple") == 0) {
        kind = SEMANTIC_RESULT_TYPE_TUPLE;
    } else if (strcmp(name, "t") == 0) {
        kind = SEMANTIC_RESULT_TYPE_INFERRED;
    } else if (is_primitive_type_for_result(name)) {
        kind = SEMANTIC_RESULT_TYPE_PRIMITIVE;
    } else {
        declared_type = find_slot(ctx, name);
        if (declared_type && declared_type->symbol_id != 0u &&
            (declared_type->kind == SLOT_STRUCT ||
                declared_type->kind == SLOT_UNION ||
                declared_type->kind == SLOT_ENUM ||
                declared_type->kind == SLOT_NEWTYPE)) {
            kind = SEMANTIC_RESULT_TYPE_USER_DEFINED;
            referenced_symbol_id = declared_type->symbol_id;
            if (declared_type->type_id != 0u &&
                declared_type->type_id <= ctx->result.type_count) {
                base_type_id = ctx->result.types[
                    declared_type->type_id - 1u].base_type_id;
            }
        } else {
            kind = SEMANTIC_RESULT_TYPE_UNKNOWN;
        }
    }

    status = semantic_result_intern_type(
        &ctx->result, kind, flags, name, referenced_symbol_id, base_type_id, &type_id);
    semantic_set_result_error(ctx, status);
    return status == SEMANTIC_RESULT_OK ? type_id : 0u;
}

static int semantic_parse_unsigned_integer_literal(
    const ASTLiteral* literal, uint64_t* value_out, int* overflow_out
) {
    const char* cursor;
    unsigned base = 10u;
    uint64_t value = 0u;
    int have_digit = 0;
    int overflow = 0;

    if (!literal || literal->type != TOKEN_NUMBER ||
        !literal->string_value || !literal->string_value[0] ||
        !value_out || !overflow_out) return 0;

    cursor = literal->string_value;
    if (cursor[0] == '0' && cursor[1] != '\0') {
        if (cursor[1] == 'x' || cursor[1] == 'X') {
            base = 16u;
            cursor += 2;
        } else if (cursor[1] == 'b' || cursor[1] == 'B') {
            base = 2u;
            cursor += 2;
        } else if (cursor[1] == 'o' || cursor[1] == 'O') {
            base = 8u;
            cursor += 2;
        }
    }

    for (; *cursor; ++cursor) {
        unsigned digit;
        unsigned char ch = (unsigned char)*cursor;
        if (ch == '_') continue;
        if (ch >= '0' && ch <= '9') digit = (unsigned)(ch - '0');
        else if (ch >= 'a' && ch <= 'f') digit = 10u + (unsigned)(ch - 'a');
        else if (ch >= 'A' && ch <= 'F') digit = 10u + (unsigned)(ch - 'A');
        else return 0;
        if (digit >= base) return 0;
        have_digit = 1;
        if (!overflow) {
            if (value > (UINT64_MAX - (uint64_t)digit) / (uint64_t)base)
                overflow = 1;
            else
                value = value * (uint64_t)base + (uint64_t)digit;
        }
    }

    if (!have_digit) return 0;
    *value_out = value;
    *overflow_out = overflow;
    return 1;
}

static int semantic_eval_integer_constant(
    const ASTNode* expression, int64_t* value_out
) {
    if (!expression || !value_out) return 0;
    if (expression->type == AST_LITERAL) {
        uint64_t value = 0u;
        int overflow = 0;
        if (!semantic_parse_unsigned_integer_literal(
                (const ASTLiteral*)expression, &value, &overflow) ||
            overflow || value > (uint64_t)INT64_MAX) return 0;
        *value_out = (int64_t)value;
        return 1;
    }
    if (expression->type == AST_UNARY_EXPR) {
        const ASTUnaryExpr* unary = (const ASTUnaryExpr*)expression;
        int64_t operand;
        if (!semantic_eval_integer_constant(unary->operand, &operand))
            return 0;
        if (unary->op == TOKEN_PLUS) {
            *value_out = operand;
            return 1;
        }
        if (unary->op == TOKEN_MINUS) {
            if (operand == INT64_MIN) return 0;
            *value_out = -operand;
            return 1;
        }
        return 0;
    }
    if (expression->type == AST_BINARY_EXPR) {
        const ASTBinaryExpr* binary = (const ASTBinaryExpr*)expression;
        int64_t left;
        int64_t right;
        if (!semantic_eval_integer_constant(binary->left, &left) ||
            !semantic_eval_integer_constant(binary->right, &right))
            return 0;
        switch (binary->op) {
            case TOKEN_PLUS:
                if ((right > 0 && left > INT64_MAX - right) ||
                    (right < 0 && left < INT64_MIN - right)) return 0;
                *value_out = left + right;
                return 1;
            case TOKEN_MINUS:
                if ((right < 0 && left > INT64_MAX + right) ||
                    (right > 0 && left < INT64_MIN + right)) return 0;
                *value_out = left - right;
                return 1;
            case TOKEN_STAR:
                if (left == 0 || right == 0) {
                    *value_out = 0;
                    return 1;
                }
                if ((left == -1 && right == INT64_MIN) ||
                    (right == -1 && left == INT64_MIN)) return 0;
                if (left > 0) {
                    if ((right > 0 && left > INT64_MAX / right) ||
                        (right < 0 && right < INT64_MIN / left)) return 0;
                } else {
                    if ((right > 0 && left < INT64_MIN / right) ||
                        (right < 0 && left < INT64_MAX / right)) return 0;
                }
                *value_out = left * right;
                return 1;
            case TOKEN_SLASH:
                if (right == 0 || (left == INT64_MIN && right == -1))
                    return 0;
                *value_out = left / right;
                return 1;
            case TOKEN_PERCENT:
                if (right == 0 || (left == INT64_MIN && right == -1))
                    return 0;
                *value_out = left % right;
                return 1;
            default:
                return 0;
        }
    }
    return 0;
}

/*
 * Shift counts accept the complete unsigned 64-bit literal range.
 * Return values: 1 = non-negative constant, -1 = negative constant,
 * -2 = literal exceeds uint64_t, 0 = not statically known.
 */
static int semantic_eval_shift_count(
    const ASTNode* expression, uint64_t* value_out
) {
    int64_t signed_value;
    if (!expression || !value_out) return 0;

    if (expression->type == AST_LITERAL) {
        int overflow = 0;
        if (!semantic_parse_unsigned_integer_literal(
                (const ASTLiteral*)expression, value_out, &overflow))
            return 0;
        return overflow ? -2 : 1;
    }

    if (expression->type == AST_UNARY_EXPR) {
        const ASTUnaryExpr* unary = (const ASTUnaryExpr*)expression;
        if (unary->op == TOKEN_PLUS)
            return semantic_eval_shift_count(unary->operand, value_out);
        if (unary->op == TOKEN_MINUS) {
            if (unary->operand && unary->operand->type == AST_LITERAL) {
                int overflow = 0;
                if (semantic_parse_unsigned_integer_literal(
                        (const ASTLiteral*)unary->operand,
                        value_out, &overflow)) {
                    if (!overflow && *value_out == 0u) return 1;
                    return -1;
                }
            }
            if (semantic_eval_integer_constant(expression, &signed_value)) {
                if (signed_value < 0) return -1;
                *value_out = (uint64_t)signed_value;
                return 1;
            }
            return 0;
        }
    }

    if (semantic_eval_integer_constant(expression, &signed_value)) {
        if (signed_value < 0) return -1;
        *value_out = (uint64_t)signed_value;
        return 1;
    }
    return 0;
}

static int semantic_array_dimension_value(
    const ASTNode* expression, uint64_t* value_out
) {
    int64_t value;
    if (!value_out ||
        !semantic_eval_integer_constant(expression, &value) ||
        value <= 0) return 0;
    *value_out = (uint64_t)value;
    return 1;
}

static int semantic_format_array_signature(
    const ASTVarDecl* declaration, char* output, size_t output_size
) {
    const ASTArrayType* array_type;
    const ASTNode* dimension;
    size_t used = 0u;
    int written;
    const char* element_type;
    if (!declaration || !output || output_size == 0u ||
        !declaration->inline_type ||
        declaration->inline_type->type != AST_ARRAY_TYPE) return 0;
    element_type = declaration->type_name ? declaration->type_name : "*";
    if (element_type[0] == '!') ++element_type;
    written = snprintf(output, output_size, "array|%s", element_type);
    if (written < 0 || (size_t)written >= output_size) return 0;
    used = (size_t)written;
    array_type = (const ASTArrayType*)declaration->inline_type;
    for (dimension = array_type->dimensions; dimension;
         dimension = dimension->next) {
        const ASTArrayDimension* dim = (const ASTArrayDimension*)dimension;
        uint64_t extent = 0u;
        if (dim->length &&
            semantic_array_dimension_value(dim->length, &extent)) {
            written = snprintf(output + used, output_size - used,
                "[%llu]", (unsigned long long)extent);
        } else if (!dim->length) {
            written = snprintf(output + used, output_size - used, "[]");
        } else {
            written = snprintf(output + used, output_size - used, "[?]");
        }
        if (written < 0 || (size_t)written >= output_size - used)
            return 0;
        used += (size_t)written;
    }
    return 1;
}

static int semantic_is_map_type_name(const char* type_name) {
    if (!type_name) return 0;
    if (type_name[0] == '!') ++type_name;
    return strcmp(type_name, "map") == 0 ||
        strcmp(type_name, "Map") == 0;
}

static int semantic_is_array_type_name(const char* type_name) {
    return type_name &&
        (strcmp(type_name, "array") == 0 ||
         strncmp(type_name, "array|", 6u) == 0);
}

static uint32_t semantic_declared_type_id(
    SemanticContext* ctx, const ASTVarDecl* declaration
) {
    const ASTArrayType* array_type;
    const ASTNode* dimension;
    char shape[256];
    size_t used;
    uint32_t base_type_id = 0u;
    uint32_t type_id = 0u;
    uint16_t flags = 0u;
    SemanticResultStatus status;
    if (!ctx || !declaration) return 0u;
    if (!declaration->inline_type ||
        declaration->inline_type->type != AST_ARRAY_TYPE) {
        return semantic_type_id(ctx, declaration->type_name,
            declaration->is_hard_typed);
    }
    if (!ctx->result_enabled || ctx->result_status != SEMANTIC_RESULT_OK)
        return 0u;
    array_type = (const ASTArrayType*)declaration->inline_type;
    if (declaration->type_name) {
        base_type_id = semantic_type_id(ctx, declaration->type_name,
            declaration->is_hard_typed);
    }
    if (declaration->is_hard_typed)
        flags |= SEMANTIC_RESULT_TYPE_FLAG_HARD;
    memcpy(shape, "array", 6u);
    used = 5u;
    for (dimension = array_type->dimensions; dimension; dimension = dimension->next) {
        const ASTArrayDimension* dim = (const ASTArrayDimension*)dimension;
        int written;
        if (!dim->length) {
            written = snprintf(shape + used, sizeof(shape) - used, "[]");
        } else {
            uint64_t dimension_value = 0u;
            if (semantic_array_dimension_value(dim->length, &dimension_value)) {
                written = snprintf(shape + used, sizeof(shape) - used, "[%llu]",
                    (unsigned long long)dimension_value);
            } else {
                written = snprintf(shape + used, sizeof(shape) - used, "[?]");
            }
        }
        if (written < 0 || (size_t)written >= sizeof(shape) - used)
            return 0u;
        used += (size_t)written;
    }
    status = semantic_result_intern_type(
        &ctx->result, SEMANTIC_RESULT_TYPE_AGGREGATE, flags,
        shape, 0u, base_type_id, &type_id);
    semantic_set_result_error(ctx, status);
    return status == SEMANTIC_RESULT_OK ? type_id : 0u;
}

static Slot* add_slot(
    SemanticContext* ctx,
    const char* name,
    SlotKind kind,
    int is_const,
    const char* type_name,
    ASTNode* declaration_node,
    int is_implicit,
    int is_packed,
    int is_hard,
    uint16_t extra_symbol_flags
) {
    Slot* s = (Slot*)semantic_allocate(ctx, sizeof(Slot));
    if (!s) {
        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
        return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->name = semantic_duplicate(ctx, name ? name : "");
    s->type_name = type_name ? semantic_duplicate(ctx, type_name) : NULL;
    if (!s->name || (type_name && !s->type_name)) {
        free(s->name);
        free(s->type_name);
        free(s);
        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
        return NULL;
    }
    s->kind = kind;
    s->declaration_node = declaration_node;
    s->is_const = is_const;
    s->depth = ctx->current_depth;
    s->param_count = 0;
    s->param_types = NULL;
    s->fields = NULL;
    s->is_packed = is_packed;
    s->scope_id = ctx->current_scope_id;
    s->group_owner = ctx->current_group_owner;
    s->fast_exec_owner = ctx->current_fast_exec_owner;
    s->type_id = semantic_type_id(ctx, type_name, is_hard);

    if (ctx->result_enabled &&
        ctx->result_status == SEMANTIC_RESULT_OK) {
        uint32_t declaration_node_id = semantic_node_id(ctx, declaration_node);
        uint16_t result_flags = 0u;
        if (is_const && kind == SLOT_VAR &&
            (extra_symbol_flags & SEMANTIC_RESULT_SYMBOL_FLAG_IMMUTABLE) == 0u) {
            result_flags |= SEMANTIC_RESULT_SYMBOL_FLAG_CONST;
        }
        if (is_packed) result_flags |= SEMANTIC_RESULT_SYMBOL_FLAG_PACKED;
        if (is_implicit) result_flags |= SEMANTIC_RESULT_SYMBOL_FLAG_IMPLICIT;
        if (is_hard) result_flags |= SEMANTIC_RESULT_SYMBOL_FLAG_HARD;
        result_flags |= extra_symbol_flags;
        if (type_name && type_name[0] == '!') {
            result_flags |= SEMANTIC_RESULT_SYMBOL_FLAG_HARD;
        }
        if (declaration_node_id == 0u || ctx->current_scope_id == 0u) {
            semantic_set_result_error(ctx, SEMANTIC_RESULT_INVALID_DATA);
        } else {
            SemanticResultStatus status = semantic_result_add_symbol(
                &ctx->result,
                ctx->current_scope_id,
                declaration_node_id,
                semantic_result_symbol_kind(kind),
                result_flags,
                name,
                s->type_id,
                &s->symbol_id);
            semantic_set_result_error(ctx, status);
        }
    }

    s->next = ctx->head;
    ctx->head = s;
    return s;
}

static void semantic_define_named_type(
    SemanticContext* ctx, Slot* slot, const char* type_name,
    const char* base_type_name
) {
    uint32_t base_type_id = semantic_type_id(ctx, base_type_name, 0);
    uint32_t type_id = 0u;
    SemanticResultStatus status;

    if (!ctx || !slot || !ctx->result_enabled ||
        ctx->result_status != SEMANTIC_RESULT_OK || slot->symbol_id == 0u) return;
    status = semantic_result_intern_type(
        &ctx->result,
        SEMANTIC_RESULT_TYPE_USER_DEFINED,
        0u,
        type_name,
        slot->symbol_id,
        base_type_id,
        &type_id);
    semantic_set_result_error(ctx, status);
    if (status == SEMANTIC_RESULT_OK) {
        status = semantic_result_set_symbol_type(
            &ctx->result, slot->symbol_id, type_id);
        semantic_set_result_error(ctx, status);
        if (status == SEMANTIC_RESULT_OK) slot->type_id = type_id;
    }
}

static void semantic_record_annotation(
    SemanticContext* ctx,
    ASTNode* node,
    Slot* slot,
    const char* type_name,
    uint16_t value_category
) {
    uint32_t node_id;
    uint32_t type_id;
    SemanticResultStatus status;

    if (!ctx || !ctx->result_enabled ||
        ctx->result_status != SEMANTIC_RESULT_OK || !node) return;
    node_id = semantic_node_id(ctx, node);
    type_id = slot ? slot->type_id : semantic_type_id(ctx, type_name, 0);
    if (node_id == 0u || ctx->current_scope_id == 0u) {
        semantic_set_result_error(ctx, SEMANTIC_RESULT_INVALID_DATA);
        return;
    }
    status = semantic_result_set_annotation(
        &ctx->result,
        node_id,
        ctx->current_scope_id,
        slot ? slot->symbol_id : 0u,
        type_id,
        value_category,
        (uint16_t)((ctx->current_method_is_object && slot &&
            slot->name && strcmp(slot->name, "self") == 0)
            ? SEMANTIC_RESULT_ANNOTATION_FLAG_ADDRESS_RECEIVER : 0u));
    semantic_set_result_error(ctx, status);
}

static void destroy_slot(Slot* slot) {
    if (!slot) return;

    free(slot->name);
    free(slot->type_name);

    if (slot->param_types) {
        for (int i = 0; i < slot->param_count; ++i) {
            free(slot->param_types[i]);
        }
        free(slot->param_types);
    }

    FieldInfo* field = slot->fields;
    while (field) {
        FieldInfo* next = field->next;
        free(field->name);
        free(field->type_name);
        free(field);
        field = next;
    }

    free(slot);
}

static void enter_scope(SemanticContext* ctx, ASTNode* owner_node) {
    if (ctx->result_enabled && ctx->result_status == SEMANTIC_RESULT_OK) {
        uint32_t owner_node_id = semantic_node_id(ctx, owner_node);
        uint32_t new_scope_id = 0u;
        SemanticResultStatus status;
        if (owner_node_id == 0u || ctx->current_scope_id == 0u) {
            semantic_set_result_error(ctx, SEMANTIC_RESULT_INVALID_DATA);
        } else {
            status = semantic_result_add_scope(
                &ctx->result,
                ctx->current_scope_id,
                (uint32_t)(ctx->current_depth + 1),
                0u,
                owner_node_id,
                &new_scope_id);
            semantic_set_result_error(ctx, status);
            if (status == SEMANTIC_RESULT_OK) ctx->current_scope_id = new_scope_id;
        }
    }
    ctx->current_depth++;
}

static void exit_scope(SemanticContext* ctx) {
    if (!ctx) return;

    Slot** link = &ctx->head;
    while (*link) {
        Slot* current = *link;
        if (current->depth == ctx->current_depth) {
            *link = current->next;
            destroy_slot(current);
        } else {
            link = &current->next;
        }
    }
    if (ctx->result_enabled && ctx->current_scope_id > 1u) {
        const SemanticResultScope* scope =
            &ctx->result.scopes[ctx->current_scope_id - 1u];
        if (scope->depth == (uint32_t)ctx->current_depth) {
            ctx->current_scope_id = scope->parent_scope_id;
        }
    }
    ctx->current_depth--;
}

static int are_types_compatible(const char* type1, const char* type2);

static const char* get_expr_type(SemanticContext* ctx, ASTNode* node);
static Slot* semantic_resolve_group_callable(
    SemanticContext* ctx, ASTMemberAccess* access,
    int* out_is_instance_call, int report_diagnostics);

static FieldInfo* semantic_resolve_member(
    SemanticContext* ctx, ASTNode* node, int report_diagnostics
) {
    ASTMemberAccess* access;
    const char* object_type;
    const char* aggregate_name;
    Slot* aggregate;
    FieldInfo* field;
    if (!node || node->type != AST_MEMBER_ACCESS) return NULL;
    access = (ASTMemberAccess*)node;
    if (!access->object || !access->member_name) return NULL;
    object_type = get_expr_type(ctx, access->object);
    if (!object_type) return NULL;
    aggregate_name = object_type[0] == '!' ? object_type + 1 : object_type;
    aggregate = find_slot(ctx, aggregate_name);
    if (!aggregate ||
        (aggregate->kind != SLOT_STRUCT && aggregate->kind != SLOT_UNION)) {
        if (report_diagnostics) {
            report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                "Member access requires a struct or union value.");
        }
        return NULL;
    }
    for (field = aggregate->fields; field; field = field->next) {
        if (strcmp(field->name, access->member_name) == 0) return field;
    }
    if (report_diagnostics) {
        char message[256];
        snprintf(message, sizeof(message),
            "Aggregate type '%s' has no member named '%s'.",
            aggregate_name, access->member_name);
        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
    }
    return NULL;
}

static int semantic_is_lvalue(SemanticContext* ctx, ASTNode* node) {
    if (!ctx || !node) return 0;
    if (node->type == AST_IDENTIFIER) {
        Slot* slot = find_slot(ctx, ((ASTIdentifier*)node)->name);
        return slot && slot->kind == SLOT_VAR;
    }
    if (node->type == AST_MEMBER_ACCESS) {
        ASTMemberAccess* access = (ASTMemberAccess*)node;
        return semantic_resolve_member(ctx, node, 0) != NULL &&
            semantic_is_lvalue(ctx, access->object);
    }
    if (node->type == AST_INDEX_ACCESS) {
        ASTIndexAccess* access = (ASTIndexAccess*)node;
        return semantic_is_lvalue(ctx, access->target);
    }
    return 0;
}

static const char* semantic_lvalue_description(ASTNode* node) {
    if (!node) return "<missing>";
    if (node->type == AST_IDENTIFIER)
        return ((ASTIdentifier*)node)->name
            ? ((ASTIdentifier*)node)->name : "<unnamed>";
    if (node->type == AST_MEMBER_ACCESS)
        return ((ASTMemberAccess*)node)->member_name
            ? ((ASTMemberAccess*)node)->member_name : "<member>";
    if (node->type == AST_INDEX_ACCESS) return "indexed element";
    return "<expression>";
}

static int semantic_array_known_length(
    SemanticContext* ctx, ASTNode* target, uint64_t* out_length
) {
    const ASTNode* array_expression = NULL;
    const ASTArrayType* array_type = NULL;
    if (!ctx || !target || !out_length) return 0;
    if (target->type == AST_ARRAY_LITERAL) {
        array_expression = target;
    } else if (target->type == AST_IDENTIFIER) {
        Slot* slot = find_slot(ctx, ((ASTIdentifier*)target)->name);
        if (slot && slot->declaration_node &&
            slot->declaration_node->type == AST_VAR_DECL) {
            const ASTVarDecl* declaration =
                (const ASTVarDecl*)slot->declaration_node;
            if (declaration->initializer &&
                declaration->initializer->type == AST_ARRAY_LITERAL) {
                array_expression = declaration->initializer;
            } else if (declaration->inline_type &&
                       declaration->inline_type->type == AST_ARRAY_TYPE) {
                array_type = (const ASTArrayType*)declaration->inline_type;
            } else if (declaration->type_expr &&
                       declaration->type_expr->type == AST_TYPE_EXPR) {
                const ASTTypeExpr* type_expr =
                    (const ASTTypeExpr*)declaration->type_expr;
                while (type_expr && type_expr->kind == AST_TYPE_QUALIFIED)
                    type_expr = type_expr->child
                        ? (const ASTTypeExpr*)type_expr->child : NULL;
                if (type_expr && type_expr->kind == AST_TYPE_ARRAY &&
                    type_expr->dimensions &&
                    type_expr->dimensions->type == AST_ARRAY_TYPE) {
                    array_type = (const ASTArrayType*)type_expr->dimensions;
                }
            }
        }
    }
    if (array_expression) {
        const ASTNode* element =
            ((const ASTArrayLiteral*)array_expression)->elements;
        uint64_t count = 0u;
        for (; element; element = element->next) {
            if (count == UINT64_MAX) return 0;
            ++count;
        }
        *out_length = count;
        return 1;
    }
    if (array_type && array_type->dimensions &&
        array_type->dimensions->type == AST_ARRAY_DIMENSION) {
        const ASTArrayDimension* dimension =
            (const ASTArrayDimension*)array_type->dimensions;
        if (dimension->length)
            return semantic_array_dimension_value(dimension->length, out_length);
    }
    return 0;
}

static int semantic_lvalue_is_const(SemanticContext* ctx, ASTNode* node) {
    if (!node) return 0;
    if (node->type == AST_IDENTIFIER) {
        Slot* slot = find_slot(ctx, ((ASTIdentifier*)node)->name);
        return slot ? slot->is_const : 0;
    }
    if (node->type == AST_MEMBER_ACCESS) {
        ASTMemberAccess* access = (ASTMemberAccess*)node;
        FieldInfo* field = semantic_resolve_member(ctx, node, 0);
        return field && (field->is_const ||
            semantic_lvalue_is_const(ctx, access->object));
    }
    if (node->type == AST_INDEX_ACCESS) {
        ASTIndexAccess* access = (ASTIndexAccess*)node;
        return semantic_lvalue_is_const(ctx, access->target);
    }
    return 0;
}

static int is_integer_primitive_type(const char* type_name);
static const char* semantic_representation_element_type(const char* type_name);

static const char* get_expr_type(SemanticContext* ctx, ASTNode* node) {
    if (!node) return NULL;
    switch (node->type) {
        case AST_LITERAL: {
            ASTLiteral* lit = (ASTLiteral*)node;
            const char* type_name = NULL;
            switch (lit->type) {
                case TOKEN_NUMBER:
                    type_name = (strchr(lit->string_value, '.') ||
                        strchr(lit->string_value, 'E') ||
                        strchr(lit->string_value, 'e')) ? "f64" : "i32";
                    break;
                case TOKEN_STRING: type_name = "str"; break;
                case TOKEN_CHAR: type_name = "char"; break;
                case TOKEN_TRUE:
                case TOKEN_FALSE: type_name = "bool"; break;
                default: break;
            }
            semantic_record_annotation(
                ctx, node, NULL, type_name, SEMANTIC_RESULT_VALUE_RVALUE);
            return type_name;
        }
        case AST_IDENTIFIER: {
            ASTIdentifier* id = (ASTIdentifier*)node;
            Slot* slot = find_slot(ctx, id->name);
            semantic_record_annotation(
                ctx, node, slot, slot ? slot->type_name : NULL,
                slot && slot->kind == SLOT_FUNC
                    ? SEMANTIC_RESULT_VALUE_FUNCTION_DESIGNATOR
                    : SEMANTIC_RESULT_VALUE_LVALUE);
            return slot ? slot->type_name : NULL;
        }
        case AST_AREA:
            semantic_record_annotation(
                ctx, node, NULL, "area", SEMANTIC_RESULT_VALUE_RVALUE);
            return "area";
        case AST_ZONE:
            semantic_record_annotation(
                ctx, node, NULL, "zone", SEMANTIC_RESULT_VALUE_RVALUE);
            return "zone";
        case AST_FREE:
            semantic_record_annotation(
                ctx, node, NULL, "void", SEMANTIC_RESULT_VALUE_RVALUE);
            return "void";
        case AST_ADDR:
        case AST_LEN:
            semantic_record_annotation(
                ctx, node, NULL, "u64", SEMANTIC_RESULT_VALUE_RVALUE);
            return "u64";
        case AST_PEEK:
            semantic_record_annotation(
                ctx, node, NULL, "u8", SEMANTIC_RESULT_VALUE_RVALUE);
            return "u8";
        case AST_POKE:
            semantic_record_annotation(
                ctx, node, NULL, "void", SEMANTIC_RESULT_VALUE_RVALUE);
            return "void";
        case AST_INB:
            semantic_record_annotation(
                ctx, node, NULL, "u8", SEMANTIC_RESULT_VALUE_RVALUE);
            return "u8";
        case AST_OUTB:
            semantic_record_annotation(
                ctx, node, NULL, "void", SEMANTIC_RESULT_VALUE_RVALUE);
            return "void";
        case AST_STRUCT_LITERAL: {
            ASTStructLiteral* literal = (ASTStructLiteral*)node;
            Slot* type_slot = literal->type_name
                ? find_slot(ctx, literal->type_name) : NULL;
            if (type_slot && type_slot->kind == SLOT_STRUCT) {
                semantic_record_annotation(
                    ctx, node, type_slot, type_slot->name,
                    SEMANTIC_RESULT_VALUE_RVALUE);
                return type_slot->name;
            }
            return NULL;
        }
        case AST_MEMBER_ACCESS: {
            FieldInfo* field = semantic_resolve_member(ctx, node, 0);
            const char* type_name = field ? field->type_name : NULL;
            if (field) semantic_record_annotation(
                ctx, node, NULL, type_name, SEMANTIC_RESULT_VALUE_LVALUE);
            return type_name;
        }
        case AST_FUNC_CALL: {
            ASTCall* call = (ASTCall*)node;
            Slot* slot = NULL;
            const char* result_type = NULL;
            if (call->callee && call->callee->type == AST_IDENTIFIER) {
                ASTIdentifier* id = (ASTIdentifier*)call->callee;
                if (id->name &&
                    (strcmp(id->name, "asmcall") == 0 ||
                     strcmp(id->name, "asmjmp") == 0)) {
                    semantic_record_annotation(
                        ctx, node, NULL, "void", SEMANTIC_RESULT_VALUE_RVALUE);
                    return "void";
                }
                slot = find_slot(ctx, id->name);
            } else if (call->callee &&
                call->callee->type == AST_MEMBER_ACCESS) {
                int is_instance_call = 0;
                slot = semantic_resolve_group_callable(
                    ctx, (ASTMemberAccess*)call->callee,
                    &is_instance_call, 0);
            }
            result_type = slot ? slot->type_name : NULL;
            if (result_type &&
                (strcmp(result_type, "t") == 0 ||
                 (result_type[0] == '!' && strcmp(result_type + 1, "t") == 0)) &&
                call->args) {
                const char* inferred = get_expr_type(ctx, call->args);
                if (inferred) result_type = inferred;
            }
            semantic_record_annotation(
                ctx, node, slot, result_type,
                SEMANTIC_RESULT_VALUE_RVALUE);
            return result_type;
        }
        case AST_BINARY_EXPR:
        case AST_ASSIGNMENT: {
            ASTBinaryExpr* binary = (ASTBinaryExpr*)node;
            const char* type_name;
            if (node->type == AST_BINARY_EXPR &&
                (binary->op == TOKEN_EQ || binary->op == TOKEN_NEQ ||
                 binary->op == TOKEN_GT || binary->op == TOKEN_LT ||
                 binary->op == TOKEN_GTE || binary->op == TOKEN_LTE ||
                 binary->op == TOKEN_AND || binary->op == TOKEN_OR)) {
                type_name = "bool";
            } else {
                type_name = get_expr_type(ctx, binary->left);
            }
            semantic_record_annotation(
                ctx, node, NULL, type_name, SEMANTIC_RESULT_VALUE_RVALUE);
            return type_name;
        }
        case AST_UNARY_EXPR:
        case AST_POSTFIX_EXPR: {
            const char* type_name =
                get_expr_type(ctx, ((ASTUnaryExpr*)node)->operand);
            semantic_record_annotation(
                ctx, node, NULL, type_name, SEMANTIC_RESULT_VALUE_RVALUE);
            return type_name;
        }
        case AST_INDEX_ACCESS: {
            ASTIndexAccess* access = (ASTIndexAccess*)node;
            const char* target_type = get_expr_type(ctx, access->target);
            const char* element_type = "any";
            const char* representation_element =
                semantic_representation_element_type(target_type);
            if (target_type && strcmp(target_type, "tuple") == 0) {
                const ASTTuple* tuple_expr = NULL;
                int64_t tuple_index;
                if (access->target && access->target->type == AST_TUPLE) {
                    tuple_expr=(const ASTTuple*)access->target;
                } else if (access->target &&
                    access->target->type == AST_IDENTIFIER) {
                    const ASTIdentifier* id=(const ASTIdentifier*)access->target;
                    Slot* tuple_slot=find_slot(ctx,id->name);
                    if (tuple_slot && tuple_slot->declaration_node &&
                        tuple_slot->declaration_node->type == AST_VAR_DECL) {
                        const ASTVarDecl* tuple_decl=
                            (const ASTVarDecl*)tuple_slot->declaration_node;
                        if (tuple_decl->initializer &&
                            tuple_decl->initializer->type == AST_TUPLE)
                            tuple_expr=(const ASTTuple*)tuple_decl->initializer;
                    }
                }
                if (tuple_expr &&
                    semantic_eval_integer_constant(access->index,&tuple_index)) {
                    const ASTNode* item = tuple_expr->elements;
                    if (tuple_index >= 0) {
                        while (item && tuple_index > 0) {
                            item=item->next;
                            --tuple_index;
                        }
                        if (item) element_type=get_expr_type(ctx,(ASTNode*)item);
                    }
                }
            } else if (representation_element) {
                element_type = representation_element;
            } else if (target_type && strncmp(target_type, "array|", 6u) == 0) {
                const char* begin = target_type + 6u;
                const char* end = strchr(begin, '[');
                size_t length = end ? (size_t)(end - begin) : strlen(begin);
                if (length > 0u && length < sizeof(indexed_element_type_buffer)) {
                    memcpy(indexed_element_type_buffer, begin, length);
                    indexed_element_type_buffer[length] = '\0';
                    element_type = indexed_element_type_buffer;
                }
            } else if (target_type && strcmp(target_type, "array") == 0) {
                const ASTNode* array_expression = NULL;
                if (access->target->type == AST_ARRAY_LITERAL) {
                    array_expression = access->target;
                } else if (access->target->type == AST_IDENTIFIER) {
                    Slot* array_slot = find_slot(ctx,
                        ((ASTIdentifier*)access->target)->name);
                    if (array_slot && array_slot->declaration_node &&
                        array_slot->declaration_node->type == AST_VAR_DECL) {
                        const ASTVarDecl* array_decl =
                            (const ASTVarDecl*)array_slot->declaration_node;
                        if (array_decl->initializer &&
                            array_decl->initializer->type == AST_ARRAY_LITERAL)
                            array_expression = array_decl->initializer;
                    }
                }
                if (array_expression) {
                    const ASTNode* first =
                        ((const ASTArrayLiteral*)array_expression)->elements;
                    if (first) element_type = get_expr_type(ctx, (ASTNode*)first);
                }
            }
            semantic_record_annotation(
                ctx, node, NULL, element_type, SEMANTIC_RESULT_VALUE_LVALUE);
            return element_type;
        }
        case AST_NAMED_ARG: {
            ASTNamedArg* named=(ASTNamedArg*)node;
            const char* value_type=get_expr_type(ctx,named->value);
            semantic_record_annotation(ctx,node,NULL,value_type,
                SEMANTIC_RESULT_VALUE_RVALUE);
            return value_type;
        }
        case AST_TUPLE: {
            ASTNode* element=((ASTTuple*)node)->elements;
            for(;element;element=element->next) {
                if(!get_expr_type(ctx,element)) {
                    /* Unknown element expressions keep their own diagnostics. */
                }
            }
            semantic_record_annotation(
                ctx,node,NULL,"tuple",SEMANTIC_RESULT_VALUE_RVALUE);
            return "tuple";
        }
        case AST_ARRAY_LITERAL: {
            ASTNode* element=((ASTArrayLiteral*)node)->elements;
            const char* element_type=NULL;
            for(;element;element=element->next) {
                const char* current_type=get_expr_type(ctx,element);
                if(!current_type) continue;
                if(!element_type) element_type=current_type;
                else if(!are_types_compatible(element_type,current_type)) {
                    report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "Array literal elements must have compatible types.");
                    return NULL;
                }
            }
            semantic_record_annotation(
                ctx,node,NULL,"array",SEMANTIC_RESULT_VALUE_RVALUE);
            return "array";
        }
        case AST_RELOCATE: {
            const char* type_name = get_expr_type(ctx, ((ASTRelocate*)node)->source);
            semantic_record_annotation(
                ctx, node, NULL, type_name ? type_name : "any",
                SEMANTIC_RESULT_VALUE_RVALUE);
            return type_name ? type_name : "any";
        }
        case AST_LAMBDA:
            semantic_record_annotation(
                ctx, node, NULL, "any", SEMANTIC_RESULT_VALUE_RVALUE);
            return "any";
        default:
            return NULL;
    }
}

static int semantic_array_signatures_compatible(
    const char* expected, const char* actual
) {
    const char* expected_dim;
    const char* actual_dim;
    const char* expected_element;
    const char* actual_element;
    size_t expected_element_length;
    size_t actual_element_length;
    if (!expected || !actual ||
        strncmp(expected, "array|", 6u) != 0 ||
        strncmp(actual, "array|", 6u) != 0) return 0;
    expected_element = expected + 6u;
    actual_element = actual + 6u;
    expected_dim = strchr(expected_element, '[');
    actual_dim = strchr(actual_element, '[');
    expected_element_length = expected_dim
        ? (size_t)(expected_dim - expected_element) : strlen(expected_element);
    actual_element_length = actual_dim
        ? (size_t)(actual_dim - actual_element) : strlen(actual_element);
    if (expected_element_length != 1u || expected_element[0] != '*') {
        if (expected_element_length != actual_element_length ||
            memcmp(expected_element, actual_element,
                expected_element_length) != 0) return 0;
    }
    while (expected_dim && actual_dim) {
        const char* expected_end = strchr(expected_dim, ']');
        const char* actual_end = strchr(actual_dim, ']');
        size_t expected_length;
        size_t actual_length;
        if (!expected_end || !actual_end) return 0;
        expected_length = (size_t)(expected_end - expected_dim + 1);
        actual_length = (size_t)(actual_end - actual_dim + 1);
        /* An unspecified expected extent accepts any actual extent. */
        if (expected_length != 2u || expected_dim[1] != ']') {
            if (expected_length != actual_length ||
                memcmp(expected_dim, actual_dim, expected_length) != 0)
                return 0;
        }
        expected_dim = expected_end[1] ? expected_end + 1 : NULL;
        actual_dim = actual_end[1] ? actual_end + 1 : NULL;
        if (expected_dim && *expected_dim != '[') return 0;
        if (actual_dim && *actual_dim != '[') return 0;
    }
    return expected_dim == NULL && actual_dim == NULL;
}

static int are_types_compatible(const char* type1, const char* type2) {
    const char* t1;
    const char* t2;
    if (!type1 || !type2) return 0;
    t1 = (type1[0] == '!') ? type1 + 1 : type1;
    t2 = (type2[0] == '!') ? type2 + 1 : type2;
    if (strcmp(t1, t2) == 0) return 1;
    /* `t` is the language's implicit type variable; call-site inference
       binds it to the concrete argument/initializer type. */
    if (strcmp(t1, "t") == 0 || strcmp(t2, "t") == 0) return 1;
    if (strcmp(t1, "array") == 0 &&
        semantic_is_array_type_name(t2)) return 1;
    if (strcmp(t2, "array") == 0 &&
        semantic_is_array_type_name(t1)) return 1;
    return semantic_array_signatures_compatible(t1, t2);
}

static int is_numeric_primitive_type(const char* type_name);

static int semantic_vector_dimension(
    const char* type_name, size_t* dimension_out
) {
    if (!type_name || !dimension_out) return 0;
    if (type_name[0] == '!') ++type_name;
    if (strcmp(type_name, "Vec2") == 0) *dimension_out = 2u;
    else if (strcmp(type_name, "Vec3") == 0) *dimension_out = 3u;
    else if (strcmp(type_name, "Vec4") == 0) *dimension_out = 4u;
    else return 0;
    return 1;
}

static void semantic_validate_vector_initializer(
    SemanticContext* ctx, ASTNode* declaration_node,
    const char* vector_type, ASTNode* initializer
) {
    size_t expected = 0u, count = 0u;
    ASTNode* element;
    if (!semantic_vector_dimension(vector_type, &expected) ||
        !initializer || initializer->type != AST_ARRAY_LITERAL) return;
    for (element = ((ASTArrayLiteral*)initializer)->elements;
         element; element = element->next) {
        const char* element_type = get_expr_type(ctx, element);
        ++count;
        if (element_type && !is_numeric_primitive_type(element_type)) {
            char message[256];
            (void)snprintf(message, sizeof(message),
                "%s components must be numeric; element %llu has type '%s'.",
                vector_type, (unsigned long long)count, element_type);
            report_error(ctx, element, ERR_SEMANTIC_TYPE_MISMATCH, message);
        }
    }
    if (count != expected) {
        char message[256];
        (void)snprintf(message, sizeof(message),
            "%s initializer requires exactly %llu numeric components; received %llu.",
            vector_type, (unsigned long long)expected,
            (unsigned long long)count);
        report_error(ctx, declaration_node, ERR_SEMANTIC_TYPE_MISMATCH, message);
    }
}

static int is_numeric_primitive_type(const char* type_name) {
    static const char* const names[] = {
        "i8", "i16", "i32", "i64",
        "u8", "u16", "u32", "u64",
        "f32", "f64", "d32", "d64"
    };
    size_t i;
    if (!type_name) return 0;
    if (type_name[0] == '!') ++type_name;
    if (strcmp(type_name, "t") == 0) return 1;
    for (i = 0u; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (strcmp(type_name, names[i]) == 0) return 1;
    }
    return 0;
}

static int is_arithmetic_binary_operator(OmniTokenType op) {
    return op == TOKEN_PLUS || op == TOKEN_MINUS ||
        op == TOKEN_STAR || op == TOKEN_SLASH ||
        op == TOKEN_PERCENT || op == TOKEN_POWER;
}

static const char* semantic_binary_operator_name(OmniTokenType op) {
    switch (op) {
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_POWER: return "^^";
        case TOKEN_ASSIGN: return "=";
        case TOKEN_FORCE_ASSIGN: return "!!=";
        case TOKEN_PLUS_EQ: return "+=";
        case TOKEN_MINUS_EQ: return "-=";
        case TOKEN_STAR_EQ: return "*=";
        case TOKEN_SLASH_EQ: return "/=";
        case TOKEN_BIT_AND_EQ: return "&=";
        case TOKEN_BIT_OR_EQ: return "|=";
        case TOKEN_BIT_XOR_EQ: return "^=";
        case TOKEN_STRING_APPEND: return ".=";
        case TOKEN_BIT_AND: return "&";
        case TOKEN_BIT_OR: return "|";
        case TOKEN_BIT_XOR: return "^";
        case TOKEN_BIT_SHL: return "<<<";
        case TOKEN_BIT_SHR: return ">>>";
        case TOKEN_EQ: return "==";
        case TOKEN_NEQ: return "!=";
        case TOKEN_GT: return ">";
        case TOKEN_LT: return "<";
        case TOKEN_GTE: return ">=";
        case TOKEN_LTE: return "<=";
        case TOKEN_AND: return "&&";
        case TOKEN_OR: return "||";
        default: return "<unknown>";
    }
}

static int is_integer_primitive_type(const char* type_name) {
    static const char* const names[] = {
        "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64"
    };
    size_t i;
    if (!type_name) return 0;
    if (type_name[0] == '!') ++type_name;
    for (i = 0u; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (strcmp(type_name, names[i]) == 0) return 1;
    }
    return 0;
}

static int is_bitwise_primitive_type(const char* type_name) {
    if (!type_name) return 0;
    if (type_name[0] == '!') ++type_name;
    static const char* const representation_types[] = {
        "8bit", "16bit", "32bit", "64bit", "128bit",
        "1byte", "2byte", "4byte", "8byte", "16byte"
    };
    size_t i;
    if (is_integer_primitive_type(type_name) ||
        strcmp(type_name, "bit") == 0 || strcmp(type_name, "byte") == 0)
        return 1;
    for (i = 0u; i < sizeof(representation_types) / sizeof(representation_types[0]); ++i) {
        if (strcmp(type_name, representation_types[i]) == 0) return 1;
    }
    return 0;
}

/*
 * Fixed-width representation indexing:
 * - Nbit is indexed by individual bit and yields "bit".
 * - Nbyte is indexed by byte and yields "byte".
 * The source spelling is retained in the AST; this helper only determines
 * the semantic element type and does not prescribe host integer endianness.
 */
static const char* semantic_representation_element_type(const char* type_name) {
    static const char* const bit_vectors[] = {
        "8bit", "16bit", "32bit", "64bit", "128bit"
    };
    static const char* const byte_blocks[] = {
        "1byte", "2byte", "4byte", "8byte", "16byte"
    };
    size_t i;
    if (!type_name) return NULL;
    if (type_name[0] == '!') ++type_name;
    for (i = 0u; i < sizeof(bit_vectors) / sizeof(bit_vectors[0]); ++i)
        if (strcmp(type_name, bit_vectors[i]) == 0) return "bit";
    for (i = 0u; i < sizeof(byte_blocks) / sizeof(byte_blocks[0]); ++i)
        if (strcmp(type_name, byte_blocks[i]) == 0) return "byte";
    return NULL;
}

static int semantic_representation_index_limit(
    const char* type_name, uint64_t* limit_out
) {
    static const struct {
        const char* name;
        uint64_t count;
    } indexed_types[] = {
        { "8bit", 8u }, { "16bit", 16u }, { "32bit", 32u },
        { "64bit", 64u }, { "128bit", 128u },
        { "1byte", 1u }, { "2byte", 2u }, { "4byte", 4u },
        { "8byte", 8u }, { "16byte", 16u }
    };
    size_t i;
    if (!type_name || !limit_out) return 0;
    if (type_name[0] == '!') ++type_name;
    for (i = 0u; i < sizeof(indexed_types) / sizeof(indexed_types[0]); ++i) {
        if (strcmp(type_name, indexed_types[i].name) == 0) {
            *limit_out = indexed_types[i].count;
            return 1;
        }
    }
    return 0;
}

static int semantic_representation_bit_width(
    const char* type_name, uint64_t* width_out
) {
    static const struct {
        const char* name;
        uint64_t bits;
    } types[] = {
        { "bit", 1u }, { "byte", 8u },
        { "8bit", 8u }, { "16bit", 16u }, { "32bit", 32u },
        { "64bit", 64u }, { "128bit", 128u },
        { "1byte", 8u }, { "2byte", 16u }, { "4byte", 32u },
        { "8byte", 64u }, { "16byte", 128u }
    };
    size_t i;
    if (!type_name || !width_out) return 0;
    if (type_name[0] == '!') ++type_name;
    for (i = 0u; i < sizeof(types) / sizeof(types[0]); ++i) {
        if (strcmp(type_name, types[i].name) == 0) {
            *width_out = types[i].bits;
            return 1;
        }
    }
    return 0;
}

static int semantic_integer_type_width(
    const char* type_name, uint64_t* width_out, int* is_unsigned_out
) {
    static const struct {
        const char* name;
        uint64_t bits;
        int is_unsigned;
    } types[] = {
        { "i8", 8u, 0 }, { "i16", 16u, 0 },
        { "i32", 32u, 0 }, { "i64", 64u, 0 },
        { "u8", 8u, 1 }, { "u16", 16u, 1 },
        { "u32", 32u, 1 }, { "u64", 64u, 1 }
    };
    size_t i;
    if (!type_name || !width_out || !is_unsigned_out) return 0;
    if (type_name[0] == '!') ++type_name;
    for (i = 0u; i < sizeof(types) / sizeof(types[0]); ++i) {
        if (strcmp(type_name, types[i].name) == 0) {
            *width_out = types[i].bits;
            *is_unsigned_out = types[i].is_unsigned;
            return 1;
        }
    }
    return 0;
}

/*
 * Shift operations accept ordinary integer values and the Nexus fixed-width
 * bit/byte representations. Representation types retain their exact width:
 * bit=1, byte=8, Nbit=N, Nbyte=8*N.
 */
static int semantic_shift_value_width(
    const char* type_name, uint64_t* width_out
) {
    int is_unsigned = 0;
    if (!type_name || !width_out) return 0;
    if (semantic_representation_bit_width(type_name, width_out)) return 1;
    return semantic_integer_type_width(type_name, width_out, &is_unsigned);
}

/*
 * Return 1 for a proven-safe explicit representation conversion, 0 when
 * the source/target categories are not convertible, -1 for a constant value
 * outside the target range, and -2 when a runtime range check is required.
 */
static int semantic_hard_representation_coercion_status(
    const ASTVarDecl* declaration, const char* source_type
) {
    uint64_t target_bits = 0u;
    uint64_t source_bits = 0u;
    uint64_t integer_bits = 0u;
    int source_unsigned = 0;
    int64_t constant_value = 0;
    if (!declaration || !declaration->is_hard_typed ||
        !declaration->type_name || !source_type ||
        !semantic_representation_bit_width(
            declaration->type_name, &target_bits)) return 0;

    if (semantic_representation_bit_width(source_type, &source_bits))
        return source_bits <= target_bits ? 1 : -2;

    if (!semantic_integer_type_width(
            source_type, &integer_bits, &source_unsigned)) return 0;

    if (semantic_eval_integer_constant(
            declaration->initializer, &constant_value)) {
        uint64_t maximum;
        if (constant_value < 0) return -1;
        if (target_bits >= 64u) return 1;
        maximum = (UINT64_C(1) << target_bits) - UINT64_C(1);
        return (uint64_t)constant_value <= maximum ? 1 : -1;
    }

    if (source_unsigned && integer_bits <= target_bits) return 1;
    return -2;
}

static void semantic_record_representation_conversion(
    SemanticContext* ctx,
    ASTNode* declaration_node,
    const Slot* destination_slot,
    const char* source_type,
    const char* destination_type
) {
    uint32_t node_id, source_type_id, destination_type_id;
    uint64_t source_bits = 0u, destination_bits = 0u;
    uint64_t integer_bits = 0u;
    int source_unsigned = 0;
    uint16_t conversion_kind;
    SemanticResultStatus status;
    if (!ctx || !ctx->result_enabled ||
        ctx->result_status != SEMANTIC_RESULT_OK ||
        !declaration_node || !destination_slot ||
        !source_type || !destination_type) return;

    node_id = semantic_node_id(ctx, declaration_node);
    source_type_id = semantic_type_id(ctx, source_type, 0);
    destination_type_id = destination_slot->type_id
        ? destination_slot->type_id
        : semantic_type_id(ctx, destination_type, 1);
    if (!node_id || !source_type_id || !destination_type_id) {
        semantic_set_result_error(ctx, SEMANTIC_RESULT_INVALID_DATA);
        return;
    }

    if (semantic_representation_bit_width(source_type, &source_bits)) {
        /* source is another bit/byte representation type */
    } else if (semantic_integer_type_width(
            source_type, &integer_bits, &source_unsigned)) {
        source_bits = integer_bits;
    } else {
        return;
    }
    (void)source_unsigned;
    if (!semantic_representation_bit_width(
            destination_type, &destination_bits)) return;

    if (source_bits < destination_bits)
        conversion_kind = SEMANTIC_RESULT_CONVERSION_WIDENING;
    else if (source_bits > destination_bits)
        conversion_kind = SEMANTIC_RESULT_CONVERSION_NARROWING;
    else
        conversion_kind = SEMANTIC_RESULT_CONVERSION_EXPLICIT_CAST;

    status = semantic_result_add_conversion(
        &ctx->result, node_id, 0u, source_type_id, destination_type_id,
        conversion_kind, SEMANTIC_RESULT_CONVERSION_FLAG_EXPLICIT);
    semantic_set_result_error(ctx, status);
}
static int is_type_defined(SemanticContext* ctx, const char* type_name) {
    if (!type_name) return 1; // Infer type or void
    if (type_name[0] == '!') type_name++; // Skip hard-type prefix

    /* Pointer/reference wrappers are valid only when their base type resolves. */
    if (type_name[0] == '&')
        return type_name[1] != '\0' && is_type_defined(ctx, type_name + 1);
    if (type_name[0] == '*') {
        while (*type_name == '*') ++type_name;
        return *type_name != '\0' && is_type_defined(ctx, type_name);
    }

    // Generic, recursive array and function types are represented in AST.
    if (semantic_is_array_type_name(type_name) ||
        strcmp(type_name, "tuple") == 0 || strcmp(type_name, "t") == 0 ||
        strncmp(type_name, "f:(", 3u) == 0)
        return 1;

    // Primitives
    if (strcmp(type_name, "i32") == 0 || strcmp(type_name, "u8") == 0 ||
        strcmp(type_name, "str") == 0 || strcmp(type_name, "bool") == 0 ||
        strcmp(type_name, "f64") == 0 || strcmp(type_name, "i64") == 0 ||
        strcmp(type_name, "u64") == 0 || strcmp(type_name, "u32") == 0 ||
        strcmp(type_name, "u16") == 0 || strcmp(type_name, "i16") == 0 ||
        strcmp(type_name, "i8") == 0 || strcmp(type_name, "f32") == 0 ||
        strcmp(type_name, "d32") == 0 || strcmp(type_name, "d64") == 0 ||
        strcmp(type_name, "char") == 0 || strcmp(type_name, "bit") == 0 ||
        strcmp(type_name, "byte") == 0 ||
        strcmp(type_name, "8bit") == 0 || strcmp(type_name, "16bit") == 0 ||
        strcmp(type_name, "32bit") == 0 || strcmp(type_name, "64bit") == 0 ||
        strcmp(type_name, "128bit") == 0 || strcmp(type_name, "1byte") == 0 ||
        strcmp(type_name, "2byte") == 0 || strcmp(type_name, "4byte") == 0 ||
        strcmp(type_name, "8byte") == 0 || strcmp(type_name, "16byte") == 0 ||
        strcmp(type_name, "void") == 0 || strcmp(type_name, "any") == 0) {
        return 1;
    }

    // Check symbol table
    Slot* s = find_slot(ctx, type_name);
    if (s && (s->kind == SLOT_STRUCT || s->kind == SLOT_UNION || 
              s->kind == SLOT_ENUM || s->kind == SLOT_NEWTYPE)) {
        return 1;
    }
    return 0;
}

static void semantic_validate_type_expr(SemanticContext* ctx, ASTNode* node) {
    ASTTypeExpr* type;
    ASTNode* parameter;
    if (!ctx || !node) return;
    if (node->type != AST_TYPE_EXPR) {
        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
            "Recursive type expression contains a node that is not a type expression.");
        return;
    }
    type = (ASTTypeExpr*)node;
    switch (type->kind) {
        case AST_TYPE_NAMED:
            if (!type->name || !type->name[0]) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Named type expression requires a non-empty type name.");
            } else if (!is_type_defined(ctx, type->name)) {
                char message[256];
                (void)snprintf(message, sizeof(message),
                    "Unknown type '%s' in recursive type expression.",
                    type->name);
                report_error(ctx, node, ERR_SEMANTIC_UNKNOWN_TYPE, message);
            }
            break;

        case AST_TYPE_POINTER:
        case AST_TYPE_REFERENCE:
            if (!type->child) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    type->kind == AST_TYPE_POINTER
                        ? "Pointer type expression requires a target type."
                        : "Reference type expression requires a target type.");
            } else {
                semantic_validate_type_expr(ctx, type->child);
            }
            break;

        case AST_TYPE_ARRAY:
            if (!type->child) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Array type expression requires an element type.");
            } else {
                semantic_validate_type_expr(ctx, type->child);
            }
            if (!type->dimensions ||
                type->dimensions->type != AST_ARRAY_TYPE) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Array type expression requires an array dimension descriptor.");
            } else {
                semantic_check(ctx, type->dimensions);
            }
            break;

        case AST_TYPE_FUNCTION:
            if (!type->child) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Function type expression requires a return type.");
            } else {
                semantic_validate_type_expr(ctx, type->child);
            }
            for (parameter = type->params; parameter; parameter = parameter->next) {
                if (parameter->type == AST_VARIADIC_PARAM) {
                    if (parameter->next) {
                        report_error(ctx, parameter, ERR_SEMANTIC_TYPE_MISMATCH,
                            "Variadic function type marker must be the final parameter.");
                    }
                } else if (parameter->type == AST_VAR_DECL) {
                    ASTVarDecl* declaration = (ASTVarDecl*)parameter;
                    ASTNode* previous;
                    if (!declaration->name || !declaration->name[0]) {
                        report_error(ctx, parameter, ERR_SEMANTIC_TYPE_MISMATCH,
                            "Function type parameter requires a non-empty name.");
                    } else {
                        for (previous = type->params; previous != parameter;
                             previous = previous->next) {
                            if (previous->type == AST_VAR_DECL) {
                                ASTVarDecl* earlier = (ASTVarDecl*)previous;
                                if (earlier->name &&
                                    strcmp(earlier->name, declaration->name) == 0) {
                                    char message[256];
                                    (void)snprintf(message, sizeof(message),
                                        "Function type parameter '%s' is declared more than once.",
                                        declaration->name);
                                    report_error(ctx, parameter,
                                        ERR_SEMANTIC_REDEFINED, message);
                                    break;
                                }
                            }
                        }
                    }
                    if (declaration->type_expr) {
                        semantic_validate_type_expr(ctx, declaration->type_expr);
                    } else if (!declaration->type_name ||
                               !is_type_defined(ctx, declaration->type_name)) {
                        char message[256];
                        (void)snprintf(message, sizeof(message),
                            "Function type parameter '%s' references an unknown type.",
                            declaration->type_name ? declaration->type_name : "<missing>");
                        report_error(ctx, parameter, ERR_SEMANTIC_UNKNOWN_TYPE, message);
                    }
                } else {
                    report_error(ctx, parameter, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Function type parameters must be typed declarations or a final variadic marker.");
                }
            }
            break;

        case AST_TYPE_QUALIFIED:
            if ((type->flags & ~AST_TYPE_FLAG_HARD_TYPED) != 0u) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Type expression contains unsupported qualifier flags.");
            }
            if (!type->child) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Qualified type expression requires an underlying type.");
            } else {
                semantic_validate_type_expr(ctx, type->child);
            }
            break;

        default:
            report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                "Unknown recursive type expression kind.");
            break;
    }
}

static int semantic_is_boolean_condition(SemanticContext* ctx, ASTNode* node) {
    const char* type_name;
    if (!node) return 0;
    if (node->type == AST_LITERAL) {
        ASTLiteral* literal = (ASTLiteral*)node;
        return literal->type == TOKEN_TRUE || literal->type == TOKEN_FALSE;
    }
    if (node->type == AST_UNARY_EXPR) {
        ASTUnaryExpr* unary = (ASTUnaryExpr*)node;
        return unary->op == TOKEN_NOT &&
            semantic_is_boolean_condition(ctx, unary->operand);
    }
    if (node->type == AST_BINARY_EXPR) {
        ASTBinaryExpr* binary = (ASTBinaryExpr*)node;
        if (binary->op == TOKEN_AND || binary->op == TOKEN_OR) {
            return semantic_is_boolean_condition(ctx, binary->left) &&
                semantic_is_boolean_condition(ctx, binary->right);
        }
        switch (binary->op) {
            case TOKEN_EQ:
            case TOKEN_NEQ:
            case TOKEN_GT:
            case TOKEN_LT:
            case TOKEN_GTE:
            case TOKEN_LTE:
                return 1;
            default:
                return 0;
        }
    }
    type_name = get_expr_type(ctx, node);
    return type_name && strcmp(type_name[0] == '!' ? type_name + 1 : type_name, "bool") == 0;
}

static void semantic_check_boolean_condition(
    SemanticContext* ctx, ASTNode* node, const char* construct_name
) {
    const char* type_name;
    if (!ctx || !node) return;
    /* These operators already report their own operand-level diagnostics. */
    if ((node->type == AST_UNARY_EXPR &&
         ((ASTUnaryExpr*)node)->op == TOKEN_NOT) ||
        (node->type == AST_BINARY_EXPR &&
         (((ASTBinaryExpr*)node)->op == TOKEN_AND ||
          ((ASTBinaryExpr*)node)->op == TOKEN_OR))) {
        return;
    }
    type_name = get_expr_type(ctx, node);
    if (!type_name) return;
    if (type_name[0] == '!') ++type_name;
    if (strcmp(type_name, "bool") == 0) return;
    {
        char message[320];
        (void)snprintf(message, sizeof(message),
            "%s condition requires bool; got '%s'.",
            construct_name ? construct_name : "Control-flow",
            type_name);
        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
    }
}

static ASTNode* semantic_intrinsic_argument_value(ASTNode* argument) {
    if (!argument) return NULL;
    if (argument->type == AST_NAMED_ARG)
        return ((ASTNamedArg*)argument)->value;
    return argument;
}

static size_t semantic_intrinsic_argument_count(const ASTCall* call) {
    const ASTNode* argument;
    size_t count = 0u;
    if (!call) return 0u;
    for (argument = call->args; argument; argument = argument->next) {
        if (count == SIZE_MAX) return SIZE_MAX;
        ++count;
    }
    return count;
}

static int semantic_is_integer_value_type(
    SemanticContext* ctx, const char* type_name
) {
    const char* current = type_name;
    unsigned int depth;
    for (depth = 0u; current && depth < 16u; ++depth) {
        Slot* slot;
        if (*current == '!') ++current;
        if (is_integer_primitive_type(current)) return 1;
        slot = find_slot(ctx, current);
        if (!slot || slot->kind != SLOT_NEWTYPE || !slot->type_name)
            return 0;
        current = slot->type_name;
    }
    return 0;
}

static int semantic_is_integer_or_address_type(
    SemanticContext* ctx, const char* type_name
) {
    const char* current = type_name;
    unsigned int depth;
    for (depth = 0u; current && depth < 16u; ++depth) {
        Slot* slot;
        if (*current == '!') ++current;
        if (*current == '*' || *current == '&') return 1;
        if (is_integer_primitive_type(current)) return 1;
        slot = find_slot(ctx, current);
        if (!slot || slot->kind != SLOT_NEWTYPE || !slot->type_name)
            return 0;
        current = slot->type_name;
    }
    return 0;
}

static int semantic_is_lengthable_type(const char* type_name) {
    const char* type = type_name;
    if (!type) return 0;
    if (*type == '!') ++type;
    if (semantic_is_array_type_name(type)) return 1;
    if (strcmp(type, "str") == 0 || strcmp(type, "string") == 0 ||
        strcmp(type, "buffer") == 0 || strcmp(type, "Buffer") == 0 ||
        strcmp(type, "map") == 0 || strcmp(type, "Map") == 0 ||
        strcmp(type, "vec") == 0 || strcmp(type, "Vec") == 0)
        return 1;
    return 0;
}

static void semantic_validate_memory_intrinsic(
    SemanticContext* ctx, ASTNode* node
) {
    ASTCall* call = (ASTCall*)node;
    size_t count = semantic_intrinsic_argument_count(call);
    ASTNode* first = semantic_intrinsic_argument_value(call->args);
    ASTNode* second = call->args && call->args->next
        ? semantic_intrinsic_argument_value(call->args->next) : NULL;
    const char* first_type = first ? get_expr_type(ctx, first) : NULL;
    const char* second_type = second ? get_expr_type(ctx, second) : NULL;
    size_t expected = (node->type == AST_POKE) ? 2u : 1u;
    const char* name = node->type == AST_ADDR ? "addr" :
        node->type == AST_LEN ? "len" :
        node->type == AST_PEEK ? "peek" : "poke";
    if (count != expected) {
        char message[192];
        (void)snprintf(message, sizeof(message),
            "%s() expects exactly %u argument%s; received %llu.",
            name, (unsigned int)expected, expected == 1u ? "" : "s",
            (unsigned long long)count);
        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
        return;
    }
    if (node->type == AST_ADDR) {
        if (first_type && !semantic_is_lvalue(ctx, first)) {
            char message[256];
            (void)snprintf(message, sizeof(message),
                "addr() requires an addressable variable, field, or indexed element; got '%s'.",
                first_type);
            report_error(ctx, first, ERR_SEMANTIC_TYPE_MISMATCH, message);
        }
    } else if (node->type == AST_LEN) {
        if (first_type && !semantic_is_lengthable_type(first_type)) {
            char message[256];
            (void)snprintf(message, sizeof(message),
                "len() requires a collection, string, or buffer; got '%s'.",
                first_type);
            report_error(ctx, first, ERR_SEMANTIC_TYPE_MISMATCH, message);
        }
    } else if (node->type == AST_PEEK) {
        if (first_type && strcmp(first_type, "any") != 0 &&
            !semantic_is_integer_or_address_type(ctx, first_type)) {
            char message[256];
            (void)snprintf(message, sizeof(message),
                "peek() address must be an integer or pointer value; got '%s'.",
                first_type);
            report_error(ctx, first, ERR_SEMANTIC_TYPE_MISMATCH, message);
        }
    } else if (node->type == AST_POKE) {
        if (first_type && strcmp(first_type, "any") != 0 &&
            !semantic_is_integer_or_address_type(ctx, first_type)) {
            char message[256];
            (void)snprintf(message, sizeof(message),
                "poke() address must be an integer or pointer value; got '%s'.",
                first_type);
            report_error(ctx, first, ERR_SEMANTIC_TYPE_MISMATCH, message);
        }
        if (second_type && strcmp(second_type, "any") != 0 &&
            !semantic_is_integer_value_type(ctx, second_type)) {
            char message[256];
            (void)snprintf(message, sizeof(message),
                "poke() value must be an integer or byte value; got '%s'.",
                second_type);
            report_error(ctx, second, ERR_SEMANTIC_TYPE_MISMATCH, message);
        }
    }
}

static void semantic_validate_port_intrinsic(
    SemanticContext* ctx, ASTNode* node
) {
    ASTCall* call = (ASTCall*)node;
    size_t count = semantic_intrinsic_argument_count(call);
    size_t expected = node->type == AST_OUTB ? 2u : 1u;
    ASTNode* argument = call->args;
    size_t index = 0u;
    if (count != expected) {
        char message[160];
        (void)snprintf(message, sizeof(message),
            "%s() expects exactly %u integer argument%s; received %llu.",
            node->type == AST_INB ? "inb" : "outb",
            (unsigned int)expected, expected == 1u ? "" : "s",
            (unsigned long long)count);
        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
        return;
    }
    while (argument && index < expected) {
        ASTNode* value = semantic_intrinsic_argument_value(argument);
        const char* value_type = value ? get_expr_type(ctx, value) : NULL;
        if (value_type && strcmp(value_type, "any") != 0 &&
            !semantic_is_integer_value_type(ctx, value_type)) {
            char message[256];
            (void)snprintf(message, sizeof(message),
                "%s() argument %llu must have an integer type; got '%s'.",
                node->type == AST_INB ? "inb" : "outb",
                (unsigned long long)(index + 1u), value_type);
            report_error(ctx, value, ERR_SEMANTIC_TYPE_MISMATCH, message);
        }
        argument = argument->next;
        ++index;
    }
}

static void semantic_validate_allocation_intrinsic(
    SemanticContext* ctx, ASTNode* node
) {
    ASTCall* call = (ASTCall*)node;
    size_t count = semantic_intrinsic_argument_count(call);
    ASTNode* argument = semantic_intrinsic_argument_value(call->args);
    const char* argument_type = argument ? get_expr_type(ctx, argument) : NULL;
    if (count != 1u) {
        char message[160];
        const char* name = node->type == AST_AREA ? "area" :
            node->type == AST_ZONE ? "zone" : "free";
        (void)snprintf(message, sizeof(message),
            "%s() expects exactly one argument; received %llu.",
            name, (unsigned long long)count);
        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
        return;
    }
    if (node->type == AST_AREA || node->type == AST_ZONE) {
        const char* name = node->type == AST_AREA ? "area" : "zone";
        int64_t constant_size;
        if (argument_type && strcmp(argument_type, "any") != 0 &&
            !semantic_is_integer_value_type(ctx, argument_type)) {
            char message[256];
            (void)snprintf(message, sizeof(message),
                "%s() size must have an integer type; got '%s'.",
                name, argument_type);
            report_error(ctx, argument, ERR_SEMANTIC_TYPE_MISMATCH, message);
        } else if (semantic_eval_integer_constant(argument, &constant_size) &&
                   constant_size <= 0) {
            char message[192];
            (void)snprintf(message, sizeof(message),
                "%s() size must be greater than zero; got %lld.",
                name, (long long)constant_size);
            report_error(ctx, argument, ERR_SEMANTIC_TYPE_MISMATCH, message);
        }
    } else if (argument_type && strcmp(argument_type, "any") != 0 &&
               strcmp(argument_type, "area") != 0 &&
               strcmp(argument_type, "zone") != 0 &&
               !semantic_is_integer_or_address_type(ctx, argument_type)) {
        char message[256];
        (void)snprintf(message, sizeof(message),
            "free() requires an area/zone handle or address value; got '%s'.",
            argument_type);
        report_error(ctx, argument, ERR_SEMANTIC_TYPE_MISMATCH, message);
    }
}

static void semantic_check_pattern_bindings(
    SemanticContext* ctx, ASTNode* pattern, const char* subject_type
) {
    ASTNode* current;
    const char* binding_type = subject_type && *subject_type
        ? subject_type : "any";
    for (current=pattern; current; current=current->next) {
        if (current->type==AST_PATTERN_BINDING) {
            ASTPatternBinding* binding=(ASTPatternBinding*)current;
            Slot* existing;
            if(!binding->name || !binding->name[0]) {
                report_error(ctx,current,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Pattern binding is missing its identifier.");
                continue;
            }
            existing=find_slot(ctx,binding->name);
            if(existing && existing->depth==ctx->current_depth) {
                report_error(ctx,current,ERR_SEMANTIC_REDEFINED,
                    "Pattern binding is duplicated in the same arm.");
            } else {
                Slot* slot=add_slot(ctx,binding->name,SLOT_VAR,0,binding_type,
                    current,1,0,0,0u);
                if (slot && binding_type != NULL) {
                    slot->type_id = semantic_type_id(ctx,binding_type,0);
                    if (ctx->result_enabled && slot->symbol_id != 0u &&
                        slot->type_id != 0u) {
                        SemanticResultStatus type_status =
                            semantic_result_set_symbol_type(
                                &ctx->result,slot->symbol_id,slot->type_id);
                        semantic_set_result_error(ctx,type_status);
                    }
                }
                semantic_record_annotation(ctx,current,slot,binding_type,
                    SEMANTIC_RESULT_VALUE_LVALUE);
            }
        } else if (current->type==AST_PATTERN_CONSTRUCTOR) {
            ASTPatternConstructor* constructor=(ASTPatternConstructor*)current;
            if(!constructor->name || !constructor->name[0]) {
                report_error(ctx,current,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Constructor pattern is missing its name.");
                continue;
            }
            if(!constructor->bindings) {
                report_error(ctx,current,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Constructor pattern requires at least one binding.");
                continue;
            }
            semantic_check_pattern_bindings(ctx,constructor->bindings,binding_type);
        } else if(current->type!=AST_LITERAL) {
            report_error(ctx,current,ERR_SEMANTIC_TYPE_MISMATCH,
                "Pattern contains an unsupported AST node.");
        }
    }
}

static void semantic_register_rule_signature(
    SemanticContext* ctx, Slot* slot, ASTNode* params
) {
    ASTNode* current;
    int count=0,index=0;
    for(current=params;current;current=current->next) {
        if(count==INT_MAX) {
            semantic_set_result_error(ctx,SEMANTIC_RESULT_INVALID_DATA);
            return;
        }
        ++count;
    }
    if(!slot)return;
    slot->param_count=count;
    if(count==0)return;
    slot->param_types=(char**)semantic_callocate(ctx,(size_t)count,sizeof(char*));
    if(!slot->param_types) {
        semantic_set_result_error(ctx,SEMANTIC_RESULT_OUT_OF_MEMORY);
        return;
    }
    for(current=params;current;current=current->next,++index) {
        ASTVarDecl* parameter=(ASTVarDecl*)current;
        if(parameter->type_name) {
            slot->param_types[index]=semantic_duplicate(ctx,parameter->type_name);
            if(!slot->param_types[index]) {
                semantic_set_result_error(ctx,SEMANTIC_RESULT_OUT_OF_MEMORY);
                return;
            }
        }
    }
}

static void semantic_register_rule_events(
    SemanticContext* ctx, Slot* slot, ASTNode* rules
) {
    ASTNode* rule;
    FieldInfo* tail;
    if(!slot)return;
    tail=slot->fields;
    while(tail && tail->next)tail=tail->next;
    for(rule=rules;rule;rule=rule->next) {
        ASTNode* action=NULL;
        FieldInfo* field;
        if(rule->type==AST_RULE_TRIGGER)
            action=((ASTRuleTrigger*)rule)->action;
        if(!action || action->type!=AST_IDENTIFIER)continue;
        {
            const char* name=((ASTIdentifier*)action)->name;
            FieldInfo* existing;
            int duplicate=0;
            for(existing=slot->fields;existing;existing=existing->next)
                if(existing->name && name && strcmp(existing->name,name)==0)
                    duplicate=1;
            if(duplicate) {
                report_error(ctx,rule,ERR_SEMANTIC_REDEFINED,
                    "Ruleset contains a duplicate trigger event name.");
                continue;
            }
            field=(FieldInfo*)semantic_callocate(ctx,1u,sizeof(*field));
            if(!field) {
                semantic_set_result_error(ctx,SEMANTIC_RESULT_OUT_OF_MEMORY);
                return;
            }
            field->name=semantic_duplicate(ctx,name);
            if(!field->name) {
                free(field);
                semantic_set_result_error(ctx,SEMANTIC_RESULT_OUT_OF_MEMORY);
                return;
            }
            if(tail)tail->next=field;else slot->fields=field;
            tail=field;
        }
    }
}

static int semantic_rule_has_event(const Slot* slot, const char* event_name) {
    const FieldInfo* field;
    if(!slot || !event_name)return 0;
    for(field=slot->fields;field;field=field->next)
        if(field->name && strcmp(field->name,event_name)==0)return 1;
    return 0;
}

static void semantic_check_rule_arguments(
    SemanticContext* ctx, ASTNode* node, const char* name,
    ASTNode* args, Slot* slot
) {
    ASTNode* arg;
    int count=0,index=0;
    if(args) semantic_check(ctx,args);
    for(arg=args;arg;arg=arg->next) {
        if(count<INT_MAX)++count;
    }
    if(!slot) {
        char message[256];
        snprintf(message,sizeof(message),"Unknown ruleset '%s'.",name?name:"");
        report_error(ctx,node,ERR_SEMANTIC_UNDEFINED,message);
        return;
    }
    if(slot->kind!=SLOT_FUNC) {
        report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
            "Applied ruleset name does not refer to a callable ruleset.");
        return;
    }
    if(count!=slot->param_count) {
        char message[256];
        snprintf(message,sizeof(message),
            "Ruleset '%s' expects %d argument(s), got %d.",
            name?name:"",slot->param_count,count);
        report_error(ctx,node,ERR_SEMANTIC_ARG_COUNT,message);
    }
    for(arg=args;arg && slot->param_types && index<slot->param_count;
        arg=arg->next,++index) {
        const char* actual=get_expr_type(ctx,arg);
        const char* expected=slot->param_types[index];
        if(actual && expected && !are_types_compatible(expected,actual)) {
            report_error(ctx,arg,ERR_SEMANTIC_ARG_TYPE,
                "Ruleset argument type does not match its parameter type.");
        }
    }
}

static void semantic_validate_array_initializer(
    SemanticContext* ctx,
    ASTNode* initializer,
    const ASTNode* dimension,
    const char* element_type
) {
    ASTNode* item;
    size_t count = 0u;
    if (!ctx || !dimension) return;
    if (!initializer || initializer->type != AST_ARRAY_LITERAL) {
        report_error(ctx, initializer ? initializer : (ASTNode*)dimension,
            ERR_SEMANTIC_TYPE_MISMATCH,
            "Array initializer nesting does not match the declared dimensions.");
        return;
    }
    for (item = ((ASTArrayLiteral*)initializer)->elements; item; item = item->next)
        ++count;
    if (dimension->type == AST_ARRAY_DIMENSION &&
        ((const ASTArrayDimension*)dimension)->length) {
        uint64_t expected_length = 0u;
        const ASTNode* length_node = ((const ASTArrayDimension*)dimension)->length;
        if (semantic_array_dimension_value(length_node, &expected_length) &&
            (uint64_t)count != expected_length) {
            report_error(ctx, initializer, ERR_SEMANTIC_TYPE_MISMATCH,
                "Array initializer length does not match its fixed dimension.");
        }
    }
    for (item = ((ASTArrayLiteral*)initializer)->elements; item; item = item->next) {
        if (dimension->next) {
            semantic_validate_array_initializer(ctx, item, dimension->next,
                element_type);
        } else if (item->type == AST_ARRAY_LITERAL) {
            report_error(ctx, item, ERR_SEMANTIC_TYPE_MISMATCH,
                "Array initializer has more dimensions than declared.");
        } else if (element_type) {
            const char* actual_type = get_expr_type(ctx, item);
            if (actual_type && !are_types_compatible(element_type, actual_type)) {
                report_error(ctx, item, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Array initializer element does not match the declared element type.");
            }
        }
    }
}

static Slot* semantic_find_slot_by_declaration(
    SemanticContext* ctx, const ASTNode* declaration
) {
    Slot* slot = ctx ? ctx->head : NULL;
    while (slot) {
        if (slot->declaration_node == declaration) return slot;
        slot = slot->next;
    }
    return NULL;
}
static int semantic_populate_function_signature(
    SemanticContext* ctx, Slot* function_slot, ASTFuncDecl* function
) {
    int parameter_count = 0;
    ASTNode* parameter;
    if (!ctx || !function_slot || !function) return 0;
    if (function_slot->param_count != 0 || function_slot->param_types ||
        function_slot->is_variadic) return 1;

    for (parameter = function->params; parameter; parameter = parameter->next) {
        if (parameter->type == AST_VARIADIC_PARAM) {
            function_slot->is_variadic = 1;
        } else if (parameter->type == AST_VAR_DECL) {
            ++parameter_count;
        } else {
            semantic_set_result_error(ctx,SEMANTIC_RESULT_INVALID_DATA);
            return 0;
        }
    }
    function_slot->param_count = parameter_count;
    if (parameter_count == 0) return 1;

    function_slot->param_types = (char**)semantic_callocate(
        ctx, (size_t)parameter_count, sizeof(char*));
    if (!function_slot->param_types) {
        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
        return 0;
    }

    parameter = function->params;
    {
        int index = 0;
        while (parameter) {
            if (parameter->type == AST_VARIADIC_PARAM) {
                parameter = parameter->next;
                continue;
            }
            {
                ASTVarDecl* variable = (ASTVarDecl*)parameter;
                if (variable->type_name || (variable->inline_type &&
                    variable->inline_type->type == AST_ARRAY_TYPE)) {
                    if (variable->inline_type &&
                        variable->inline_type->type == AST_ARRAY_TYPE) {
                        char array_signature[256];
                        if (!semantic_format_array_signature(
                            variable, array_signature, sizeof(array_signature))) {
                            semantic_set_result_error(
                                ctx, SEMANTIC_RESULT_INVALID_DATA);
                            return 0;
                        }
                        function_slot->param_types[index] =
                            semantic_duplicate(ctx, array_signature);
                    } else {
                        function_slot->param_types[index] =
                            semantic_duplicate(ctx, variable->type_name);
                    }
                    if (!function_slot->param_types[index]) {
                        semantic_set_result_error(
                            ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
                        return 0;
                    }
                }
                ++index;
            }
            parameter = parameter->next;
        }
    }
    return 1;
}

static void semantic_predeclare_group_callables(
    SemanticContext* ctx, ASTGroupDecl* group, Slot* receiver_slot
) {
    ASTNode* member;
    if (!ctx || !group) return;

    for (member = group->members; member; member = member->next) {
        ASTNode* implementation_node = NULL;
        ASTFuncDecl* implementation;
        const char* implementation_name = NULL;
        Slot* function_slot;
        Slot* existing;
        int is_method_alias = 0;

        if (member->type == AST_METHOD) {
            ASTMethod* method = (ASTMethod*)member;
            implementation_node = method->implementation;
            implementation_name = method->public_name;
            is_method_alias = 1;
        } else if (member->type == AST_FUNC_DECL ||
                   member->type == AST_EXTERN_DECL) {
            /* Direct Group functions are static API members under their own names. */
            implementation_node = member;
        } else {
            continue;
        }
        if (!implementation_node ||
            (implementation_node->type != AST_FUNC_DECL &&
             implementation_node->type != AST_EXTERN_DECL)) continue;
        implementation = (ASTFuncDecl*)implementation_node;
        if (implementation->name && *implementation->name)
            implementation_name = implementation->name;
        if (!implementation_name || !*implementation_name) continue;

        existing = find_slot(ctx, implementation_name);
        if (existing && existing->declaration_node == implementation_node &&
            existing->kind == SLOT_FUNC) {
            function_slot = existing;
        } else if (existing &&
            (existing->depth == ctx->current_depth ||
             (existing->kind == SLOT_FUNC &&
              existing->group_owner == ctx->current_group_owner &&
              existing->scope_id == ctx->current_scope_id))) {
            char message[256];
            snprintf(message, sizeof(message),
                msg_get(ERR_SEMANTIC_FUNC_REDEFINED), implementation_name);
            report_error(ctx, implementation_node,
                ERR_SEMANTIC_FUNC_REDEFINED, message);
            continue;
        } else {
            function_slot = add_slot(
                ctx, implementation_name, SLOT_FUNC, 1,
                implementation->return_type, implementation_node,
                0, 0, 0,
                (implementation->is_public
                    ? SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC : 0u) |
                (implementation->is_exported
                    ? SEMANTIC_RESULT_SYMBOL_FLAG_EXPORTED : 0u));
        }
        if (!function_slot) continue;
        /*
         * Keep the implementation symbol alive at the containing lexical
         * depth so Group-qualified calls after the Group declaration can
         * resolve it. find_slot hides this symbol outside its owning Group;
         * its SemanticResult scope_id remains the actual Group scope.
         */
        if (ctx->current_depth > 0)
            function_slot->depth = ctx->current_depth - 1;
        (void)semantic_populate_function_signature(
            ctx, function_slot, implementation);

        /*
         * Only AST_METHOD aliases receive an implicit Object Group receiver.
         * Direct Group functions remain ordinary static functions.
         */
        if (is_method_alias && receiver_slot &&
            receiver_slot->kind == SLOT_STRUCT &&
            implementation->params &&
            implementation->params->type == AST_VAR_DECL) {
            ASTVarDecl* first = (ASTVarDecl*)implementation->params;
            if (first->name && strcmp(first->name, "self") == 0 &&
                (!first->type_name || !*first->type_name) &&
                function_slot->param_types &&
                function_slot->param_count > 0 &&
                !function_slot->param_types[0]) {
                function_slot->param_types[0] =
                    semantic_duplicate(ctx, receiver_slot->name);
                if (!function_slot->param_types[0])
                    semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
            }
        }
    }
}

static Slot* semantic_resolve_group_callable(
    SemanticContext* ctx, ASTMemberAccess* access,
    int* out_is_instance_call, int report_diagnostics
) {
    Slot* object_slot = NULL;
    Slot* nominal_slot = NULL;
    const ASTGroupDecl* group = NULL;
    const char* object_type = NULL;
    const char* nominal_name = NULL;
    const ASTNode* member;
    int is_instance_call = 0;

    if (out_is_instance_call) *out_is_instance_call = 0;
    if (!ctx || !access) return NULL;
    if (!access->object || !access->member_name || !*access->member_name) {
        if (report_diagnostics) {
            report_error(ctx, (ASTNode*)access, ERR_SEMANTIC_TYPE_MISMATCH,
                "A Group callable access requires both a receiver and a member name.");
        }
        return NULL;
    }

    if (access->object->type == AST_IDENTIFIER) {
        object_slot = find_slot(ctx, ((ASTIdentifier*)access->object)->name);
    }

    if (object_slot && object_slot->kind == SLOT_GROUP &&
        object_slot->declaration_node &&
        object_slot->declaration_node->type == AST_GROUP_DECL) {
        group = (const ASTGroupDecl*)object_slot->declaration_node;
    } else if (object_slot &&
        (object_slot->kind == SLOT_STRUCT ||
         object_slot->kind == SLOT_UNION ||
         object_slot->kind == SLOT_ENUM) &&
        object_slot->group_owner &&
        object_slot->group_owner->type == AST_GROUP_DECL &&
        ((const ASTGroupDecl*)object_slot->group_owner)->name &&
        strcmp(object_slot->name,
            ((const ASTGroupDecl*)object_slot->group_owner)->name) == 0) {
        group = (const ASTGroupDecl*)object_slot->group_owner;
    } else {
        object_type = get_expr_type(ctx, access->object);
        if (object_type && object_type[0] == '!') ++object_type;
        if (object_type && *object_type) {
            nominal_name = object_type;
            nominal_slot = find_slot(ctx, nominal_name);
            if (nominal_slot && nominal_slot->kind == SLOT_STRUCT &&
                nominal_slot->group_owner &&
                nominal_slot->group_owner->type == AST_GROUP_DECL &&
                ((const ASTGroupDecl*)nominal_slot->group_owner)->name &&
                strcmp(nominal_slot->name,
                    ((const ASTGroupDecl*)nominal_slot->group_owner)->name) == 0) {
                group = (const ASTGroupDecl*)nominal_slot->group_owner;
                is_instance_call = 1;
            }
        }
    }

    if (!group) {
        if (report_diagnostics) {
            report_error(ctx, (ASTNode*)access, ERR_SEMANTIC_TYPE_MISMATCH,
                "Method calls require a Group namespace/type or a Group-bound Struct instance.");
        }
        return NULL;
    }

    for (member = group->members; member; member = member->next) {
        const char* public_name = NULL;
        const ASTNode* implementation_node = NULL;
        Slot* implementation_slot;

        if (member->type == AST_METHOD) {
            const ASTMethod* method = (const ASTMethod*)member;
            public_name = method->public_name;
            implementation_node = method->implementation;
        } else if (member->type == AST_FUNC_DECL ||
                   member->type == AST_EXTERN_DECL) {
            const ASTFuncDecl* function = (const ASTFuncDecl*)member;
            public_name = function->name;
            implementation_node = member;
        } else {
            continue;
        }
        if (!public_name || strcmp(public_name, access->member_name) != 0)
            continue;
        if (!implementation_node ||
            (implementation_node->type != AST_FUNC_DECL &&
             implementation_node->type != AST_EXTERN_DECL)) {
            if (report_diagnostics) {
                report_error(ctx, (ASTNode*)access, ERR_SEMANTIC_TYPE_MISMATCH,
                    "The selected Group member is not a callable function or method.");
            }
            return NULL;
        }
        implementation_slot = semantic_find_slot_by_declaration(
            ctx, implementation_node);
        if (!implementation_slot || implementation_slot->kind != SLOT_FUNC) {
            if (report_diagnostics) {
                report_error(ctx, (ASTNode*)access, ERR_SEMANTIC_TYPE_MISMATCH,
                    "The Group function/method has no resolved semantic symbol.");
            }
            return NULL;
        }
        if (out_is_instance_call) *out_is_instance_call = is_instance_call;
        return implementation_slot;
    }

    if (report_diagnostics) {
        char message[256];
        snprintf(message, sizeof(message),
            "Group '%s' has no method named '%s'.",
            group->name ? group->name : "<anonymous>",
            access->member_name);
        report_error(ctx, (ASTNode*)access, ERR_SEMANTIC_TYPE_MISMATCH, message);
    }
    return NULL;
}

static int semantic_group_method_has_implicit_receiver(
    const Slot* function_slot
) {
    const ASTNode* member;
    const ASTGroupDecl* group;
    if (!function_slot || !function_slot->group_owner ||
        function_slot->group_owner->type != AST_GROUP_DECL ||
        !function_slot->declaration_node) return 0;
    group = (const ASTGroupDecl*)function_slot->group_owner;
    for (member = group->members; member; member = member->next) {
        if (member->type == AST_METHOD) {
            const ASTMethod* method = (const ASTMethod*)member;
            const ASTFuncDecl* implementation;
            if (method->implementation != function_slot->declaration_node ||
                (method->implementation->type != AST_FUNC_DECL &&
                 method->implementation->type != AST_EXTERN_DECL)) continue;
            implementation = (const ASTFuncDecl*)method->implementation;
            return implementation->params &&
                implementation->params->type == AST_VAR_DECL &&
                ((const ASTVarDecl*)implementation->params)->name &&
                strcmp(((const ASTVarDecl*)implementation->params)->name,
                    "self") == 0;
        }
    }
    return 0;
}

static void semantic_validate_call_arguments(
    SemanticContext* ctx, ASTNode* call_node, Slot* function_slot,
    ASTNode* arguments, uint32_t parameter_offset
) {
    uint32_t argument_count = 0u;
    uint32_t expected_count;
    ASTNode* argument;
    uint32_t argument_index = 0u;
    ASTFuncDecl* declaration = NULL;

    if (!ctx || !function_slot) return;
    for (argument = arguments; argument; argument = argument->next)
        ++argument_count;

    if (function_slot->param_count < (int)parameter_offset) {
        report_error(ctx, call_node, ERR_SEMANTIC_TYPE_MISMATCH,
            "Resolved method signature is inconsistent with its receiver mode.");
        return;
    }
    expected_count = (uint32_t)function_slot->param_count - parameter_offset;
    if ((function_slot->is_variadic && argument_count < expected_count) ||
        (!function_slot->is_variadic && argument_count != expected_count)) {
        char message[256];
        snprintf(message, sizeof(message), msg_get(ERR_SEMANTIC_ARG_COUNT),
            function_slot->name, (int)expected_count, (int)argument_count);
        report_error(ctx, call_node, ERR_SEMANTIC_ARG_COUNT, message);
    }

    if (function_slot->declaration_node &&
        (function_slot->declaration_node->type == AST_FUNC_DECL ||
         function_slot->declaration_node->type == AST_EXTERN_DECL)) {
        declaration = (ASTFuncDecl*)function_slot->declaration_node;
    }

    {
        uint32_t positional_index = 0u;
        int named_arguments_started = 0;
        for (argument = arguments; argument; argument = argument->next,
             ++argument_index) {
            ASTNode* argument_value=argument;
            uint32_t parameter_index;
            const char* argument_type;
            const char* expected_type;
            ASTVarDecl* parameter = NULL;
            ASTNode* parameter_node = NULL;

            if (argument->type == AST_NAMED_ARG) {
                ASTNamedArg* named=(ASTNamedArg*)argument;
                uint32_t named_index=0u;
                int found=0;
                argument_value=named->value;
                if (declaration) {
                    for(parameter_node=declaration->params;parameter_node;
                        parameter_node=parameter_node->next) {
                        if(parameter_node->type==AST_VARIADIC_PARAM) break;
                        if(parameter_node->type==AST_VAR_DECL &&
                           ((ASTVarDecl*)parameter_node)->name && named->name &&
                           strcmp(((ASTVarDecl*)parameter_node)->name,named->name)==0) {
                            found=1;
                            break;
                        }
                        ++named_index;
                    }
                }
                if(!found) {
                    char message[256];
                    snprintf(message,sizeof(message),
                        "Named argument '%s' does not match a parameter of '%s'.",
                        named->name?named->name:"",function_slot->name);
                    report_error(ctx,argument,ERR_SEMANTIC_ARG_TYPE,message);
                    continue;
                }
                {
                    ASTNode* previous;
                    int duplicate=0;
                    for(previous=arguments;previous!=argument;previous=previous->next) {
                        if(previous->type==AST_NAMED_ARG) {
                            ASTNamedArg* prior=(ASTNamedArg*)previous;
                            if(prior->name && named->name &&
                               strcmp(prior->name,named->name)==0) duplicate=1;
                        }
                    }
                    if(named_index<positional_index) duplicate=1;
                    if(duplicate) {
                        report_error(ctx,argument,ERR_SEMANTIC_REDEFINED,
                            "A function parameter cannot receive more than one argument.");
                        continue;
                    }
                }
                named_arguments_started=1;
                parameter_index=named_index+parameter_offset;
            } else {
                if(named_arguments_started) {
                    report_error(ctx,argument,ERR_SEMANTIC_ARG_TYPE,
                        "Positional arguments cannot follow named arguments.");
                }
                parameter_index=positional_index++ + parameter_offset;
            }
            argument_type=get_expr_type(ctx,argument_value);
            expected_type=function_slot->param_types &&
                parameter_index<(uint32_t)function_slot->param_count
                    ? function_slot->param_types[parameter_index] : NULL;

            if(declaration) {
                uint32_t current_index=0u;
                for(parameter_node=declaration->params;parameter_node;
                    parameter_node=parameter_node->next) {
                    if(parameter_node->type==AST_VARIADIC_PARAM) continue;
                    if(current_index==parameter_index) break;
                    ++current_index;
                }
                if(parameter_node && parameter_node->type==AST_VAR_DECL)
                    parameter=(ASTVarDecl*)parameter_node;
            }

            if(argument_type && expected_type &&
               !are_types_compatible(expected_type,argument_type)) {
                char message[256];
                snprintf(message,sizeof(message),msg_get(ERR_SEMANTIC_ARG_TYPE),
                    (int)(parameter_index+1u),expected_type,argument_type);
                report_error(ctx,argument,ERR_SEMANTIC_ARG_TYPE,message);
            } else if(parameter && parameter->inline_type &&
                parameter->inline_type->type==AST_ARRAY_TYPE &&
                argument_value && argument_value->type==AST_ARRAY_LITERAL) {
                semantic_validate_array_initializer(
                    ctx,argument_value,
                    ((ASTArrayType*)parameter->inline_type)->dimensions,
                    parameter->type_name);
            }
        }
    }
}

static const char* semantic_group_member_name(const ASTNode* member) {
    if (!member) return NULL;
    switch (member->type) {
        case AST_METHOD:
            return ((const ASTMethod*)member)->public_name;
        case AST_GROUP_DECL:
            return ((const ASTGroupDecl*)member)->name;
        case AST_STRUCT_DECL:
        case AST_UNION_DECL:
            return ((const ASTStructDecl*)member)->name;
        case AST_ENUM_DECL:
            return ((const ASTEnumDecl*)member)->name;
        case AST_FUNC_DECL:
        case AST_EXTERN_DECL:
            return ((const ASTFuncDecl*)member)->name;
        case AST_VAR_DECL:
            return ((const ASTVarDecl*)member)->name;
        case AST_NEWTYPE_DECL:
            return ((const ASTNewTypeDecl*)member)->name;
        default:
            return NULL;
    }
}

static ASTMethod* semantic_group_default_handler(
    const ASTGroupDecl* group
) {
    const ASTNode* member;
    if (!group) return NULL;
    for (member = group->members; member; member = member->next) {
        if (member->type == AST_METHOD) {
            ASTMethod* method = (ASTMethod*)member;
            if (method->public_name &&
                strcmp(method->public_name, "default") == 0 &&
                method->implementation &&
                method->implementation->type == AST_BLOCK) return method;
        }
    }
    return NULL;
}

static void semantic_validate_aggregate_field_names(
    SemanticContext* ctx,
    const char* aggregate_name,
    ASTNode* fields
) {
    ASTNode* current;
    for (current = fields; current; current = current->next) {
        ASTVarDecl* field;
        ASTNode* previous;
        if (current->type != AST_VAR_DECL) {
            report_error(ctx, current, ERR_SEMANTIC_TYPE_MISMATCH,
                "Aggregate field list contains a non-variable declaration node.");
            continue;
        }
        field = (ASTVarDecl*)current;
        if (!field->name || !field->name[0]) {
            char message[256];
            if (aggregate_name) {
                (void)snprintf(message, sizeof(message),
                    "Aggregate '%s' requires every field to have a non-empty name.",
                    aggregate_name);
            } else {
                (void)snprintf(message, sizeof(message),
                    "Anonymous aggregate requires every field to have a non-empty name.");
            }
            report_error(ctx, current, ERR_SEMANTIC_TYPE_MISMATCH, message);
            continue;
        }
        for (previous = fields; previous && previous != current;
             previous = previous->next) {
            ASTVarDecl* earlier;
            if (previous->type != AST_VAR_DECL) continue;
            earlier = (ASTVarDecl*)previous;
            if (earlier->name && strcmp(earlier->name, field->name) == 0) {
                char message[320];
                if (aggregate_name) {
                    (void)snprintf(message, sizeof(message),
                        "Aggregate '%s' declares field '%s' more than once.",
                        aggregate_name, field->name);
                } else {
                    (void)snprintf(message, sizeof(message),
                        "Anonymous aggregate declares field '%s' more than once.",
                        field->name);
                }
                report_error(ctx, current, ERR_SEMANTIC_REDEFINED, message);
                break;
            }
        }
    }
}

static void semantic_validate_group_member_names(
    SemanticContext* ctx, ASTGroupDecl* group
) {
    const ASTNode* member;
    if (!ctx || !group) return;
    for (member = group->members; member; member = member->next) {
        const char* name = semantic_group_member_name(member);
        const ASTNode* previous;
        if (!name || !*name) continue;
        for (previous = group->members; previous && previous != member;
             previous = previous->next) {
            const char* previous_name = semantic_group_member_name(previous);
            if (previous_name && strcmp(previous_name, name) == 0) {
                char message[256];
                snprintf(message, sizeof(message),
                    "Group member '%s' is declared more than once.", name);
                report_error(ctx, (ASTNode*)member, ERR_SEMANTIC_REDEFINED, message);
                break;
            }
        }
    }
}
static void semantic_register_fast_exec_labels(
    SemanticContext* ctx,
    ASTNode* statements
) {
    ASTNode* current;
    for (current = statements; current; current = current->next) {
        ASTAsmLabel* label;
        Slot* existing;
        Slot* slot;
        if (current->type != AST_ASM_LABEL) continue;
        label = (ASTAsmLabel*)current;
        if (!label->label || !label->label[0]) {
            report_error(ctx, current, ERR_SEMANTIC_TYPE_MISMATCH,
                "ASM label name cannot be empty.");
            continue;
        }
        existing = find_slot(ctx, label->label);
        if (existing && existing->kind == SLOT_LABEL &&
            existing->fast_exec_owner == ctx->current_fast_exec_owner) {
            char message[256];
            snprintf(message, sizeof(message),
                "ASM label '%s' is declared more than once in this FASTEXEC block.",
                label->label);
            report_error(ctx, current, ERR_SEMANTIC_REDEFINED, message);
            continue;
        }
        slot = add_slot(ctx, label->label, SLOT_LABEL, 0, NULL,
            current, 0, 0, 0, 0u);
        if (slot) {
            semantic_record_annotation(
                ctx, current, slot, NULL, SEMANTIC_RESULT_VALUE_UNSPECIFIED);
        }
    }
}

static Slot* semantic_find_fast_exec_label_literal(
    SemanticContext* ctx,
    const char* quoted_name
) {
    size_t length;
    Slot* current;
    if (!ctx || !ctx->current_fast_exec_owner || !quoted_name) return NULL;
    length = strlen(quoted_name);
    if (length < 3u || quoted_name[0] != '"' ||
        quoted_name[length - 1u] != '"' ||
        memchr(quoted_name + 1u, '\\', length - 2u) != NULL ||
        memchr(quoted_name + 1u, '"', length - 2u) != NULL)
        return NULL;
    for (current = ctx->head; current; current = current->next) {
        if (current->kind == SLOT_LABEL &&
            current->fast_exec_owner == ctx->current_fast_exec_owner &&
            current->name && strlen(current->name) == length - 2u &&
            memcmp(current->name, quoted_name + 1u, length - 2u) == 0)
            return current;
    }
    return NULL;
}

static int semantic_check_asm_intrinsic(
    SemanticContext* ctx,
    ASTNode* node,
    ASTCall* call
) {
    const char* intrinsic_name;
    ASTLiteral* argument;
    Slot* label_slot;
    if (!call || !call->callee || call->callee->type != AST_IDENTIFIER)
        return 0;
    intrinsic_name = ((ASTIdentifier*)call->callee)->name;
    if (!intrinsic_name ||
        (strcmp(intrinsic_name, "asmcall") != 0 &&
         strcmp(intrinsic_name, "asmjmp") != 0))
        return 0;

    if (!ctx->current_fast_exec_owner) {
        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
            "asmcall/asmjmp may only be used inside a FASTEXEC block.");
    } else if (!call->args || call->args->type != AST_LITERAL ||
        ((ASTLiteral*)call->args)->type != TOKEN_STRING ||
        !((ASTLiteral*)call->args)->string_value ||
        call->args->next != NULL) {
        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
            "asmcall/asmjmp requires exactly one string-literal ASM label name.");
    } else {
        argument = (ASTLiteral*)call->args;
        label_slot = semantic_find_fast_exec_label_literal(
            ctx, argument->string_value);
        if (!label_slot) {
            char message[256];
            snprintf(message, sizeof(message),
                "ASM label %s is not declared in the current FASTEXEC block.",
                argument->string_value);
            report_error(ctx, node, ERR_SEMANTIC_UNDEFINED, message);
        }
    }
    semantic_record_annotation(
        ctx, node, NULL, "void", SEMANTIC_RESULT_VALUE_RVALUE);
    return 1;
}

static void semantic_check_node(SemanticContext* ctx, ASTNode* node, int follow_siblings);

static void semantic_check_node(
    SemanticContext* ctx, ASTNode* node, int follow_siblings
) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
            semantic_check(ctx, ((ASTProgram*)node)->declarations);
            break;

        case AST_MUST_DECL: {
            ASTMustDecl* immutable = (ASTMustDecl*)node;
            Slot* existing = find_slot(ctx, immutable->name);
            Slot* declared_slot = NULL;
            const char* value_type = NULL;
            if (immutable->source) {
                /* Resolve before introducing the name: m:x = x must not
                   accidentally resolve to the declaration itself. */
                semantic_check(ctx, immutable->source);
                value_type = get_expr_type(ctx, immutable->source);
            }
            if (existing && existing->depth == ctx->current_depth) {
                char message[256];
                snprintf(message, sizeof(message), msg_get(ERR_SEMANTIC_REDEFINED),
                    immutable->name ? immutable->name : "<unnamed>");
                report_error(ctx, node, ERR_SEMANTIC_REDEFINED, message);
            } else if (!immutable->source) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "An immutable m: value requires an initializer expression.");
            } else {
                declared_slot = add_slot(
                    ctx, immutable->name, SLOT_VAR, 1,
                    value_type, node, 0, 0, 0,
                    SEMANTIC_RESULT_SYMBOL_FLAG_IMMUTABLE);
                if (declared_slot && value_type && *value_type) {
                    uint32_t inferred_type_id = semantic_type_id(ctx, value_type, 0);
                    declared_slot->type_id = inferred_type_id;
                    if (ctx->result_enabled &&
                        ctx->result_status == SEMANTIC_RESULT_OK &&
                        declared_slot->symbol_id != 0u &&
                        inferred_type_id != 0u) {
                        SemanticResultStatus infer_status =
                            semantic_result_set_symbol_type(
                                &ctx->result, declared_slot->symbol_id,
                                inferred_type_id);
                        semantic_set_result_error(ctx, infer_status);
                    }
                } else if (declared_slot) {
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "The initializer of an immutable m: value must have a determinable type.");
                }
            }
            if (declared_slot) {
                semantic_record_annotation(
                    ctx, node, declared_slot, declared_slot->type_name,
                    SEMANTIC_RESULT_VALUE_LVALUE);
            }
            break;
        }

        case AST_VAR_DECL: {
            ASTVarDecl* decl = (ASTVarDecl*)node;
            Slot* declared_slot = NULL;
            Slot* existing = find_slot(ctx, decl->name);
            int is_array = (decl->inline_type &&
                decl->inline_type->type == AST_ARRAY_TYPE);
            int is_inline = (decl->inline_type != NULL && !is_array);
            int valid_type = 0;
            const char* element_type = decl->type_name;
            if (element_type && element_type[0] == '!') ++element_type;
            if (existing && existing->depth == ctx->current_depth) {
                char buf[256];
                snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_REDEFINED), decl->name);
                report_issue(ctx, node, ERR_SEMANTIC_REDEFINED, buf, NULL,
                    "Bu ismi daha önce kullandınız.", 0, 0);
            } else {
                if (element_type && *element_type) {
                    if (is_type_defined(ctx, element_type)) valid_type = 1;
                    else if (is_inline &&
                        (strcmp(element_type, "struct") == 0 ||
                         strcmp(element_type, "union") == 0)) valid_type = 1;
                }

                if (decl->type_name && !valid_type && !decl->type_expr) {
                    char buf[256];
                    (void)snprintf(buf, sizeof(buf),
                        "Declaration '%s' uses unknown type '%s'.",
                        decl->name ? decl->name : "<unnamed>",
                        element_type ? element_type : decl->type_name);
                    report_error(ctx, node, ERR_SEMANTIC_UNKNOWN_TYPE, buf);
                }

                {
                    char array_signature[256];
                    const char* declared_type = decl->type_name;
                    if (is_array && semantic_format_array_signature(
                        decl, array_signature, sizeof(array_signature))) {
                        declared_type = array_signature;
                    } else if (is_array) {
                        declared_type = "array";
                    }
                    declared_slot = add_slot(
                        ctx, decl->name, SLOT_VAR, decl->is_const,
                        declared_type, node, 0, 0, decl->is_hard_typed,
                        (decl->is_public ? SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC : 0u) |
                        (decl->is_exported ? SEMANTIC_RESULT_SYMBOL_FLAG_EXPORTED : 0u));
                }
            }

            if (decl->inline_type && !decl->type_expr)
                semantic_check(ctx, decl->inline_type);
            if (decl->type_expr) semantic_check(ctx, decl->type_expr);
            if (declared_slot &&
                decl->inline_type && decl->inline_type->type == AST_ARRAY_TYPE) {
                uint32_t array_type_id = semantic_declared_type_id(ctx, decl);
                if (array_type_id != 0u) {
                    declared_slot->type_id = array_type_id;
                    if (declared_slot->symbol_id != 0u) {
                        SemanticResultStatus array_status =
                            semantic_result_set_symbol_type(&ctx->result,
                                declared_slot->symbol_id, array_type_id);
                        semantic_set_result_error(ctx, array_status);
                    }
                }
            }

            if (decl->initializer) {
                const char* init_type;
                semantic_check(ctx, decl->initializer);
                init_type = get_expr_type(ctx, decl->initializer);
                if (is_array && decl->initializer &&
                    decl->initializer->type == AST_ARRAY_LITERAL) {
                    const ASTNode* dimensions =
                        ((ASTArrayType*)decl->inline_type)->dimensions;
                    semantic_validate_array_initializer(ctx, decl->initializer,
                        dimensions, element_type && *element_type ? element_type : NULL);
                }
                if (decl->inline_type &&
                    decl->inline_type->type == AST_ARRAY_TYPE) {
                    if (init_type && !semantic_is_array_type_name(init_type)) {
                        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                            "Array declarators require an array initializer.");
                    }
                } else if (decl->type_name) {
                    size_t vector_dimension = 0u;
                    if (semantic_vector_dimension(decl->type_name,
                            &vector_dimension) &&
                        decl->initializer &&
                        decl->initializer->type == AST_ARRAY_LITERAL) {
                        semantic_validate_vector_initializer(
                            ctx, node, decl->type_name, decl->initializer);
                    } else if (init_type &&
                        !are_types_compatible(decl->type_name, init_type)) {
                        int coercion_status =
                            semantic_hard_representation_coercion_status(
                                decl, init_type);
                        if (coercion_status == -1) {
                            report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                                "Explicit bit/byte coercion value is outside the target type range.");
                        } else if (coercion_status == -2) {
                            report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                                "Explicit bit/byte coercion requires a runtime range check that is not yet available for this source type.");
                        } else if (coercion_status == 1) {
                            semantic_record_representation_conversion(
                                ctx, node, declared_slot, init_type, decl->type_name);
                        } else {
                            char buf[256];
                            snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_TYPE_MISMATCH),
                                decl->type_name, init_type);
                            report_issue(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, buf,
                                NULL, "İlgili veri tipini kontrol edin.", 0, 0);
                        }
                    }
                } else if (declared_slot && init_type && *init_type) {
                    char* inferred_name = semantic_duplicate(ctx, init_type);
                    uint32_t inferred_type_id = semantic_type_id(ctx, init_type, 0);
                    if (!inferred_name) {
                        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
                    } else if (ctx->result_enabled &&
                        ctx->result_status == SEMANTIC_RESULT_OK &&
                        declared_slot->symbol_id != 0u && inferred_type_id != 0u) {
                        SemanticResultStatus infer_status =
                            semantic_result_set_symbol_type(
                                &ctx->result, declared_slot->symbol_id,
                                inferred_type_id);
                        semantic_set_result_error(ctx, infer_status);
                        if (infer_status != SEMANTIC_RESULT_OK) {
                            free(inferred_name);
                            inferred_name = NULL;
                        }
                    }
                    if (inferred_name) {
                        free(declared_slot->type_name);
                        declared_slot->type_name = inferred_name;
                        declared_slot->type_id = inferred_type_id;
                    }
                }
            }

            if (declared_slot) {
                semantic_record_annotation(
                    ctx, node, declared_slot, declared_slot->type_name,
                    SEMANTIC_RESULT_VALUE_LVALUE);
            }
            break;
        }

        case AST_ARRAY_LITERAL:
            /* Element homogeneity and the array annotation are centralized in
               get_expr_type(); do not run that diagnostic path twice here. */
            semantic_check(ctx,((ASTArrayLiteral*)node)->elements);
            break;
        case AST_NAMED_ARG:
            semantic_check(ctx,((ASTNamedArg*)node)->value);
            (void)get_expr_type(ctx,node);
            break;
        case AST_INDEX_ACCESS: {
            ASTIndexAccess* access = (ASTIndexAccess*)node;
            const char* target_type;
            const char* index_type;
            int64_t constant_index=0;
            if (!access->target || !access->index) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Array indexing requires both target and index.");
                break;
            }
            semantic_check(ctx, access->target);
            semantic_check(ctx, access->index);
            target_type = get_expr_type(ctx, access->target);
            if (target_type && strcmp(target_type,"tuple") == 0 &&
                semantic_eval_integer_constant(access->index,&constant_index)) {
                const ASTTuple* tuple_expr=NULL;
                const ASTNode* item;
                if(access->target->type==AST_TUPLE) {
                    tuple_expr=(const ASTTuple*)access->target;
                } else if(access->target->type==AST_IDENTIFIER) {
                    Slot* tuple_slot=find_slot(ctx,
                        ((ASTIdentifier*)access->target)->name);
                    if(tuple_slot && tuple_slot->declaration_node &&
                       tuple_slot->declaration_node->type==AST_VAR_DECL) {
                        ASTVarDecl* tuple_decl=(ASTVarDecl*)tuple_slot->declaration_node;
                        if(tuple_decl->initializer &&
                           tuple_decl->initializer->type==AST_TUPLE)
                            tuple_expr=(const ASTTuple*)tuple_decl->initializer;
                    }
                }
                item=tuple_expr?tuple_expr->elements:NULL;
                if(constant_index<0) {
                    report_error(ctx,access->index,ERR_SEMANTIC_TYPE_MISMATCH,
                        "Tuple index cannot be negative.");
                } else {
                    while(item && constant_index>0) {
                        item=item->next;
                        --constant_index;
                    }
                    if(!item) report_error(ctx,access->index,
                        ERR_SEMANTIC_TYPE_MISMATCH,
                        "Tuple index is outside the tuple element range.");
                }
            }
            if (target_type && strcmp(target_type,"tuple") != 0 &&
                !semantic_is_array_type_name(target_type) &&
                !semantic_representation_element_type(target_type) &&
                !semantic_is_map_type_name(target_type)) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Index target must be an array, map, tuple, bit vector, or byte block.");
            }
            index_type = get_expr_type(ctx, access->index);
            if (!semantic_is_map_type_name(target_type) &&
                index_type && strcmp(index_type, "any") != 0 &&
                !is_integer_primitive_type(index_type)) {
                report_error(ctx, access->index, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Index expression must have an integer type.");
            } else {
                uint64_t index_limit = 0u;
                int64_t constant_index = 0;
                if (semantic_representation_index_limit(
                        target_type, &index_limit) &&
                    semantic_eval_integer_constant(
                        access->index, &constant_index) &&
                    (constant_index < 0 ||
                     (uint64_t)constant_index >= index_limit)) {
                    report_error(ctx, access->index,
                        ERR_SEMANTIC_TYPE_MISMATCH,
                        "Bit-vector or byte-block index is out of bounds.");
                } else if (target_type && semantic_is_array_type_name(target_type) &&
                    semantic_eval_integer_constant(access->index, &constant_index)) {
                    uint64_t array_length = 0u;
                    if (semantic_array_known_length(
                            ctx, access->target, &array_length) &&
                        (constant_index < 0 ||
                         (uint64_t)constant_index >= array_length)) {
                        char message[256];
                        if (constant_index < 0) {
                            (void)snprintf(message, sizeof(message),
                                "Array index %lld cannot be negative.",
                                (long long)constant_index);
                        } else if (array_length == 0u) {
                            (void)snprintf(message, sizeof(message),
                                "Array index %llu is invalid because the target array is empty.",
                                (unsigned long long)constant_index);
                        } else {
                            (void)snprintf(message, sizeof(message),
                                "Array index %llu is out of bounds for an array of length %llu; valid range is 0..%llu.",
                                (unsigned long long)constant_index,
                                (unsigned long long)array_length,
                                (unsigned long long)(array_length - 1u));
                        }
                        report_error(ctx, access->index,
                            ERR_SEMANTIC_TYPE_MISMATCH, message);
                    }
                }
            }
            semantic_record_annotation(ctx, node, NULL,
                get_expr_type(ctx, node), SEMANTIC_RESULT_VALUE_LVALUE);
            break;
        }
        case AST_TYPE_EXPR:
            semantic_validate_type_expr(ctx, node);
            break;

        case AST_ARRAY_TYPE:
            semantic_check(ctx,((ASTArrayType*)node)->dimensions);
            break;

        case AST_ARRAY_DIMENSION: {
            ASTArrayDimension* dimension=(ASTArrayDimension*)node;
            if (dimension->length) {
                uint64_t dimension_value = 0u;
                semantic_check(ctx,dimension->length);
                if (!semantic_array_dimension_value(
                    dimension->length, &dimension_value)) {
                    report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "Fixed array dimensions must be positive integer literals; use [] for dynamic dimensions.");
                }
            }
            break;
        }

        case AST_IDENTIFIER: {
            ASTIdentifier* id = (ASTIdentifier*)node;
            Slot* resolved = find_slot(ctx, id->name);
            if (!resolved) {
                int score = 0;
                Slot* best = find_best_suggestion(ctx, id->name, &score);
                char buf[256], suggest_buf[256];
                snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_UNDEFINED), id->name);
                if (best) {
                    snprintf(suggest_buf, sizeof(suggest_buf), "Belki '%s' yazmak istediniz?", best->name);
                    report_issue(ctx, node, ERR_SEMANTIC_UNDEFINED, buf, suggest_buf, NULL, score, 0);
                } else {
                    report_issue(ctx, node, ERR_SEMANTIC_UNDEFINED, buf, NULL, "Değişken tanımlanmamış. 'v:' ile tanımlamayı deneyin.", 0, 0);
                }
            } else if (resolved->kind == SLOT_GROUP &&
                !ctx->suppress_group_default_dispatch) {
                const ASTGroupDecl* group =
                    resolved->declaration_node &&
                    resolved->declaration_node->type == AST_GROUP_DECL
                        ? (const ASTGroupDecl*)resolved->declaration_node : NULL;
                if (!semantic_group_default_handler(group)) {
                    char message[256];
                    snprintf(message, sizeof(message),
                        "Group '%s' has no default handler.",
                        group && group->name ? group->name : resolved->name);
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                } else {
                    /*
                     * The Group symbol plus GROUP_DEFAULT_HANDLER relation
                     * identifies this bare Group expression as default dispatch.
                     */
                    semantic_record_annotation(
                        ctx, node, resolved, NULL,
                        SEMANTIC_RESULT_VALUE_RVALUE);
                }
            } else {
                semantic_record_annotation(
                    ctx, node, resolved, resolved->type_name,
                    resolved->kind == SLOT_FUNC
                        ? SEMANTIC_RESULT_VALUE_FUNCTION_DESIGNATOR
                        : SEMANTIC_RESULT_VALUE_LVALUE);
            }
            break;
        }

        case AST_BINARY_EXPR: {
            ASTBinaryExpr* bin = (ASTBinaryExpr*)node;

            semantic_check(ctx, bin->left);
            semantic_check(ctx, bin->right);

            if (is_arithmetic_binary_operator(bin->op)) {
                const char* left_type = get_expr_type(ctx, bin->left);
                const char* right_type = get_expr_type(ctx, bin->right);

                if (left_type && right_type) {
                    const char* operator_name =
                        semantic_binary_operator_name(bin->op);
                    char message[320];
                    if (!are_types_compatible(left_type, right_type)) {
                        (void)snprintf(message, sizeof(message),
                            "Operator '%s' requires matching operand types; got '%s' and '%s'.",
                            operator_name, left_type, right_type);
                        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                    } else if (semantic_vector_dimension(left_type,
                                   &(size_t){0}) &&
                               semantic_vector_dimension(right_type,
                                   &(size_t){0})) {
                        /*
                         * Vec2/Vec3/Vec4 use component-wise arithmetic.
                         * are_types_compatible() above enforces equal
                         * dimensions; every arithmetic binary operator is
                         * admitted here, including power and modulo.
                         */
                    } else if (!is_numeric_primitive_type(left_type) ||
                               !is_numeric_primitive_type(right_type)) {
                        (void)snprintf(message, sizeof(message),
                            "Operator '%s' requires matching numeric operands or compatible vector operands; got '%s' and '%s'.",
                            operator_name, left_type, right_type);
                        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                    } else if (bin->op == TOKEN_PERCENT &&
                               !is_integer_primitive_type(left_type)) {
                        (void)snprintf(message, sizeof(message),
                            "Operator '%%' requires integer operands; got '%s' and '%s'.",
                            left_type, right_type);
                        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                    }
                }
            }

            if (bin->op == TOKEN_RANGE) {
                const char* begin_type = get_expr_type(ctx, bin->left);
                const char* end_type = get_expr_type(ctx, bin->right);
                if (begin_type && end_type &&
                    (!are_types_compatible(begin_type, end_type) ||
                     !is_numeric_primitive_type(begin_type) ||
                     !is_numeric_primitive_type(end_type))) {
                    char message[320];
                    (void)snprintf(message, sizeof(message),
                        "Range operator '..' requires compatible numeric endpoints; got '%s' and '%s'.",
                        begin_type, end_type);
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                }
            }

            if (bin->op == TOKEN_BIT_SHL || bin->op == TOKEN_BIT_SHR) {
                const char* left_type = get_expr_type(ctx, bin->left);
                const char* right_type = get_expr_type(ctx, bin->right);
                uint64_t shift_amount = 0u;
                uint64_t value_width = 0u;
                int shift_status = semantic_eval_shift_count(
                    bin->right, &shift_amount);
                int operands_valid =
                    (!left_type || is_bitwise_primitive_type(left_type)) &&
                    (!right_type || is_integer_primitive_type(right_type));
                if (!operands_valid) {
                    char message[320];
                    (void)snprintf(message, sizeof(message),
                        "Operator '%s' requires a bitwise-capable left operand and an integer shift count; got '%s' and '%s'.",
                        semantic_binary_operator_name(bin->op),
                        left_type ? left_type : "<unknown>",
                        right_type ? right_type : "<unknown>");
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                } else if (shift_status == -1) {
                    int64_t signed_shift = 0;
                    char message[192];
                    if (semantic_eval_integer_constant(bin->right, &signed_shift)) {
                        (void)snprintf(message, sizeof(message),
                            "Bit shift amount (%lld) cannot be negative.",
                            (long long)signed_shift);
                    } else {
                        (void)snprintf(message, sizeof(message),
                            "Bit shift amount expression cannot be negative.");
                    }
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                } else if (shift_status == -2) {
                    char message[320];
                    const ASTLiteral* literal =
                        bin->right && bin->right->type == AST_LITERAL
                            ? (const ASTLiteral*)bin->right : NULL;
                    (void)snprintf(message, sizeof(message),
                        "Bit shift amount '%s' exceeds the supported 64-bit unsigned count range.",
                        literal && literal->string_value
                            ? literal->string_value : "<constant>");
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                } else if (shift_status == 1 && left_type &&
                           semantic_shift_value_width(left_type, &value_width)) {
                    if (shift_amount >= value_width) {
                        char shift_message[192];
                        (void)snprintf(shift_message, sizeof(shift_message),
                            "Bit shift amount (%llu) is out of range for %s (%llu-bit width); valid range is 0..%llu.",
                            (unsigned long long)shift_amount,
                            left_type,
                            (unsigned long long)value_width,
                            (unsigned long long)(value_width - 1u));
                        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                            shift_message);
                    }
                }
                /*
                 * A non-constant shift count is accepted here. The OIR/runtime
                 * contract must trap or otherwise define counts >= value width;
                 * Semantic cannot prove a dynamic value's range.
                 */
            }

            if (bin->op == TOKEN_BIT_AND || bin->op == TOKEN_BIT_OR ||
                bin->op == TOKEN_BIT_XOR) {
                const char* left_type=get_expr_type(ctx,bin->left);
                const char* right_type=get_expr_type(ctx,bin->right);
                if (left_type && right_type &&
                    (!are_types_compatible(left_type, right_type) ||
                     !is_bitwise_primitive_type(left_type) ||
                     !is_bitwise_primitive_type(right_type))) {
                    const char* operator_name = bin->op == TOKEN_BIT_AND ? "&" :
                        (bin->op == TOKEN_BIT_OR ? "|" : "^");
                    char message[320];
                    if (!is_bitwise_primitive_type(left_type) ||
                        !is_bitwise_primitive_type(right_type)) {
                        (void)snprintf(message, sizeof(message),
                            "Operator '%s' does not support operand types '%s' and '%s'; use matching integer or bit-representation types.",
                            operator_name, left_type, right_type);
                    } else {
                        (void)snprintf(message, sizeof(message),
                            "Operator '%s' requires matching operand types; got '%s' and '%s'.",
                            operator_name, left_type, right_type);
                    }
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                }
            }

            if (bin->op == TOKEN_EQ || bin->op == TOKEN_NEQ ||
                bin->op == TOKEN_GT || bin->op == TOKEN_LT ||
                bin->op == TOKEN_GTE || bin->op == TOKEN_LTE) {
                const char* left_type = get_expr_type(ctx, bin->left);
                const char* right_type = get_expr_type(ctx, bin->right);
                if (left_type && right_type &&
                    !are_types_compatible(left_type, right_type)) {
                    char message[320];
                    (void)snprintf(message, sizeof(message),
                        "Operator '%s' requires compatible operand types; got '%s' and '%s'.",
                        semantic_binary_operator_name(bin->op), left_type, right_type);
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                } else if ((bin->op == TOKEN_GT || bin->op == TOKEN_LT ||
                            bin->op == TOKEN_GTE || bin->op == TOKEN_LTE) &&
                           left_type && !is_numeric_primitive_type(left_type)) {
                    char message[320];
                    (void)snprintf(message, sizeof(message),
                        "Operator '%s' requires numeric operands; got '%s'.",
                        semantic_binary_operator_name(bin->op), left_type);
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                }
            } else if (bin->op == TOKEN_AND || bin->op == TOKEN_OR) {
                if (!semantic_is_boolean_condition(ctx, bin->left) ||
                    !semantic_is_boolean_condition(ctx, bin->right)) {
                    const char* left_type = get_expr_type(ctx, bin->left);
                    const char* right_type = get_expr_type(ctx, bin->right);
                    char message[320];
                    (void)snprintf(message, sizeof(message),
                        "Operator '%s' requires bool operands; got '%s' and '%s'.",
                        semantic_binary_operator_name(bin->op),
                        left_type ? left_type : "<unknown>",
                        right_type ? right_type : "<unknown>");
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                }
            }

            /* Literal-zero check is intentionally guarded against malformed ASTs. */
            if ((bin->op == TOKEN_SLASH || bin->op == TOKEN_PERCENT) &&
                bin->right && bin->right->type == AST_LITERAL) {
                ASTLiteral* lit = (ASTLiteral*)bin->right;
                if (lit->type == TOKEN_NUMBER && lit->string_value &&
                    strcmp(lit->string_value, "0") == 0) {
                    report_error(ctx, node, ERR_SEMANTIC_DIV_ZERO,
                        msg_get(ERR_SEMANTIC_DIV_ZERO));
                }
            }
            break;
        }

        case AST_ASSIGNMENT: {
            ASTBinaryExpr* assign = (ASTBinaryExpr*)node;
            if (!assign->left || !assign->right) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Assignment requires both a target expression and a value expression.");
                break;
            }

            if (assign->op != TOKEN_ASSIGN && assign->op != TOKEN_FORCE_ASSIGN &&
                semantic_lvalue_is_const(ctx, assign->left)) {
                report_error(ctx, node, ERR_SEMANTIC_CONST_ASSIGN,
                    "Cannot modify a constant or immutable value with compound assignment.");
            }
            if (assign->op == TOKEN_ASSIGN || assign->op == TOKEN_FORCE_ASSIGN) {
            if (assign->left->type == AST_IDENTIFIER) {
                ASTIdentifier* id = (ASTIdentifier*)assign->left;
                Slot* slot = find_slot(ctx, id->name);
                if (slot && slot->is_const) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_CONST_ASSIGN), id->name);
                    report_error(ctx, node, ERR_SEMANTIC_CONST_ASSIGN, buf);
                }
                if (slot && slot->type_name) {
                    const char* right_type = get_expr_type(ctx, assign->right);
                    if (right_type && !are_types_compatible(slot->type_name, right_type)) {
                        char message[320];
                        (void)snprintf(message, sizeof(message),
                            "Operator '%s' cannot assign value of type '%s' to '%s' of type '%s'.",
                            semantic_binary_operator_name(assign->op), right_type,
                            id->name, slot->type_name);
                        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                    }
                }
            } else if (assign->left->type == AST_INDEX_ACCESS) {
                const char* element_type = get_expr_type(ctx, assign->left);
                const char* right_type = get_expr_type(ctx, assign->right);
                if (semantic_lvalue_is_const(ctx, assign->left)) {
                    report_error(ctx, node, ERR_SEMANTIC_CONST_ASSIGN,
                        "Cannot assign through an index of a constant container.");
                }
                if (element_type && right_type &&
                    strcmp(element_type, "any") != 0 &&
                    !are_types_compatible(element_type, right_type)) {
                    char message[320];
                    (void)snprintf(message, sizeof(message),
                        "Operator '%s' cannot assign value of type '%s' to an indexed element of type '%s'.",
                        semantic_binary_operator_name(assign->op), right_type,
                        element_type);
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                }
            } else if (assign->left->type == AST_MEMBER_ACCESS) {
                FieldInfo* field = semantic_resolve_member(ctx, assign->left, 0);
                if (field) {
                    if (semantic_lvalue_is_const(ctx, assign->left)) {
                        report_error(ctx, node, ERR_SEMANTIC_CONST_ASSIGN,
                            "Cannot assign to a constant aggregate member.");
                    }
                    if (field->type_name) {
                        const char* right_type = get_expr_type(ctx, assign->right);
                        if (right_type &&
                            !are_types_compatible(field->type_name, right_type)) {
                            char message[320];
                            const char* object_type =
                                get_expr_type(ctx, ((ASTMemberAccess*)assign->left)->object);
                            (void)snprintf(message, sizeof(message),
                                "Operator '%s' cannot assign value of type '%s' to field '%s.%s' of type '%s'.",
                                semantic_binary_operator_name(assign->op), right_type,
                                object_type ? object_type : "<aggregate>",
                                ((ASTMemberAccess*)assign->left)->member_name
                                    ? ((ASTMemberAccess*)assign->left)->member_name : "<unknown>",
                                field->type_name);
                            report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                        }
                    }
                }
            }
            }

            if (assign->left &&
                assign->left->type != AST_IDENTIFIER &&
                assign->left->type != AST_MEMBER_ACCESS &&
                assign->left->type != AST_INDEX_ACCESS) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Assignment target must be a mutable identifier, member, or index.");
            }
            if (assign->op != TOKEN_ASSIGN && assign->op != TOKEN_FORCE_ASSIGN) {
                const char* left_type=get_expr_type(ctx,assign->left);
                const char* right_type=get_expr_type(ctx,assign->right);
                int numeric_compound=assign->op==TOKEN_PLUS_EQ||
                    assign->op==TOKEN_MINUS_EQ||assign->op==TOKEN_STAR_EQ||
                    assign->op==TOKEN_SLASH_EQ;
                int bitwise_compound=assign->op==TOKEN_BIT_AND_EQ||
                    assign->op==TOKEN_BIT_OR_EQ||assign->op==TOKEN_BIT_XOR_EQ;
                if (assign->op == TOKEN_STRING_APPEND) {
                    if ((left_type && strcmp(left_type, "str") != 0) ||
                        (right_type && strcmp(right_type, "str") != 0)) {
                        char message[320];
                        (void)snprintf(message, sizeof(message),
                            "Operator '.=' requires str operands; got '%s' and '%s'.",
                            left_type ? left_type : "<unknown>",
                            right_type ? right_type : "<unknown>");
                        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                    }
                } else if (numeric_compound || bitwise_compound) {
                    int left_is_vector = left_type &&
                        semantic_vector_dimension(left_type, &(size_t){0});
                    int right_is_vector = right_type &&
                        semantic_vector_dimension(right_type, &(size_t){0});
                    int invalid_types = left_type && right_type &&
                        (!are_types_compatible(left_type, right_type) ||
                         (bitwise_compound
                            ? (!is_bitwise_primitive_type(left_type) ||
                               !is_bitwise_primitive_type(right_type))
                            : (!(is_numeric_primitive_type(left_type) ||
                                 left_is_vector) ||
                               !(is_numeric_primitive_type(right_type) ||
                                 right_is_vector) ||
                               (left_is_vector != right_is_vector))));
                    if (invalid_types) {
                        char message[320];
                        if (bitwise_compound) {
                            (void)snprintf(message, sizeof(message),
                                "Operator '%s' requires matching bitwise-capable types; got '%s' and '%s'.",
                                semantic_binary_operator_name(assign->op),
                                left_type, right_type);
                        } else {
                            (void)snprintf(message, sizeof(message),
                                "Operator '%s' requires matching numeric types or same-dimension vector operands; got '%s' and '%s'.",
                                semantic_binary_operator_name(assign->op),
                                left_type, right_type);
                        }
                        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                    }
                }
            }
            semantic_check(ctx, assign->left);
            semantic_check(ctx, assign->right);
            break;
        }

        case AST_NEXUS_CAPTURE: {
            ASTNexusCapture* capture = (ASTNexusCapture*)node;
            // (h) <- expr : Handler tanımlanıyor
            if (find_slot(ctx, capture->handler_name)) {
                char buf[256];
                snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_REDEFINED), capture->handler_name); // Handler için de REDEFINED kullanabiliriz veya yeni kod
                report_error(ctx, node, ERR_SEMANTIC_REDEFINED, buf);
            } else {
                Slot* handler_slot = add_slot(
                    ctx, capture->handler_name, SLOT_VAR, 0, NULL, node, 1, 0, 0, 0u);
                semantic_record_annotation(
                    ctx, node, handler_slot, NULL, SEMANTIC_RESULT_VALUE_LVALUE);
            }
            if (capture->expr) semantic_check(ctx, capture->expr);
            break;
        }

        case AST_NEXUS_PIPE: {
            ASTNexusFlow* pipe = (ASTNexusFlow*)node;
            semantic_check(ctx, pipe->left);
            semantic_check(ctx, pipe->right);
            if (pipe->catch_block) semantic_check(ctx, pipe->catch_block);
            break;
        }

        case AST_NEXUS_FLOW: {
            ASTNexusFlow* flow = (ASTNexusFlow*)node;
            semantic_check(ctx, flow->left);
            semantic_check(ctx, flow->right);
            if (flow->catch_block) semantic_check(ctx, flow->catch_block);
            break;
        }
        case AST_SELECT: {
            ASTSelect* select=(ASTSelect*)node;
            ASTNode* arm_node;
            const char* subject_type;
            int seen_default=0;
            if(!select->selector) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "select statement is missing its selector expression.");
                break;
            }
            semantic_check(ctx,select->selector);
            subject_type=get_expr_type(ctx,select->selector);
            for(arm_node=select->arms;arm_node;arm_node=arm_node->next) {
                ASTSelectArm* arm;
                if(arm_node->type!=AST_SELECT_ARM) {
                    report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "select contains an invalid arm node.");
                    continue;
                }
                arm=(ASTSelectArm*)arm_node;
                if(!arm->body) {
                    report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "select arm is missing its body.");
                    continue;
                }
                if(arm->is_default) {
                    if(arm->pattern) {
                        report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                            "select default arm must not contain a match pattern.");
                    }
                    if(seen_default) {
                        report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                            "select may contain only one default arm.");
                    }
                    seen_default=1;
                    if(arm_node->next) {
                        report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                            "select default arm must be the final arm.");
                    }
                } else if(!arm->pattern) {
                    report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "select match arm is missing its pattern.");
                    continue;
                }
                enter_scope(ctx,arm_node);
                semantic_check_pattern_bindings(ctx,arm->pattern,subject_type);
                semantic_check(ctx,arm->body);
                exit_scope(ctx);
            }
            break;
        }

        case AST_SCAN: {
            ASTScan* scan=(ASTScan*)node;
            ASTNode* arm_node;
            const char* subject_type;
            int seen_else=0;
            if(!scan->subject) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "scan statement is missing its subject expression.");
                break;
            }
            semantic_check(ctx,scan->subject);
            subject_type=get_expr_type(ctx,scan->subject);
            for(arm_node=scan->arms;arm_node;arm_node=arm_node->next) {
                ASTScanArm* arm;
                if(arm_node->type!=AST_SCAN_ARM) {
                    report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "scan contains an invalid arm node.");
                    continue;
                }
                arm=(ASTScanArm*)arm_node;
                if(!arm->body) {
                    report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "scan arm is missing its body.");
                    continue;
                }
                if(!arm->pattern) {
                    if(seen_else) {
                        report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                            "scan may contain only one else arm.");
                    }
                    seen_else=1;
                    if(arm_node->next) {
                        report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                            "scan else arm must be the final arm.");
                    }
                }
                enter_scope(ctx,arm_node);
                semantic_check_pattern_bindings(ctx,arm->pattern,subject_type);
                semantic_check(ctx,arm->body);
                exit_scope(ctx);
            }
            break;
        }

        case AST_SELECT_ARM: {
            ASTSelectArm* arm=(ASTSelectArm*)node;
            if(!arm->body) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "select arm is missing its body.");
                break;
            }
            if(arm->is_default && arm->pattern) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "select default arm must not contain a match pattern.");
                break;
            }
            if(!arm->is_default && !arm->pattern) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "select match arm is missing its pattern.");
                break;
            }
            enter_scope(ctx,node);
            semantic_check_pattern_bindings(ctx,arm->pattern,NULL);
            semantic_check(ctx,arm->body);
            exit_scope(ctx);
            break;
        }

        case AST_SCAN_ARM: {
            ASTScanArm* arm=(ASTScanArm*)node;
            if(!arm->body) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "scan arm is missing its body.");
                break;
            }
            enter_scope(ctx,node);
            semantic_check_pattern_bindings(ctx,arm->pattern,NULL);
            semantic_check(ctx,arm->body);
            exit_scope(ctx);
            break;
        }

        case AST_RULESET_DECL: {
            ASTRulesetDecl* ruleset=(ASTRulesetDecl*)node;
            ASTNode* current;
            Slot* existing;
            Slot* slot=NULL;
            if(!ruleset->name || !ruleset->name[0]) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Ruleset declaration is missing its name.");
                break;
            }
            for(current=ruleset->params;current;current=current->next) {
                if(current->type!=AST_VAR_DECL) {
                    report_error(ctx,current,ERR_SEMANTIC_TYPE_MISMATCH,
                        "Ruleset parameter list contains an unsupported AST node.");
                    break;
                }
            }
            for(current=ruleset->rules;current;current=current->next) {
                if(current->type!=AST_RULE_BARRIER &&
                   current->type!=AST_RULE_TRIGGER &&
                   current->type!=AST_RULE_ALWAYS &&
                   current->type!=AST_RULE_DEFAULT) {
                    report_error(ctx,current,ERR_SEMANTIC_TYPE_MISMATCH,
                        "Ruleset contains an unsupported rule AST node.");
                }
            }
            existing=find_slot(ctx,ruleset->name);
            if(existing && existing->depth==ctx->current_depth) {
                report_error(ctx,node,ERR_SEMANTIC_FUNC_REDEFINED,
                    "Ruleset is already declared in this scope.");
            } else {
                slot=add_slot(ctx,ruleset->name,SLOT_FUNC,1,NULL,node,0,0,0,0u);
                semantic_register_rule_signature(ctx,slot,ruleset->params);
                semantic_register_rule_events(ctx,slot,ruleset->rules);
            }
            enter_scope(ctx,node);
            semantic_check(ctx,ruleset->params);
            semantic_check(ctx,ruleset->rules);
            exit_scope(ctx);
            break;
        }

        case AST_RULE_BARRIER: {
            ASTRuleBarrier* rule=(ASTRuleBarrier*)node;
            if(!rule->condition) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Rule barrier is missing its condition expression.");
                break;
            }
            if(!rule->message) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Rule barrier is missing its diagnostic message.");
                break;
            }
            semantic_check(ctx,rule->condition);
            if(!semantic_is_boolean_condition(ctx,rule->condition))
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Rule barrier condition must have bool type.");
            semantic_check(ctx,rule->message);
            {
                const char* message_type=get_expr_type(ctx,rule->message);
                if(message_type && strcmp(message_type,"str")!=0)
                    report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "Rule barrier message must have str type.");
            }
            break;
        }

        case AST_RULE_TRIGGER: {
            ASTRuleTrigger* rule=(ASTRuleTrigger*)node;
            if(!rule->condition) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Rule trigger is missing its condition expression.");
                break;
            }
            if(!rule->action) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Rule trigger is missing its action or event name.");
                break;
            }
            semantic_check(ctx,rule->condition);
            if(!semantic_is_boolean_condition(ctx,rule->condition))
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Rule trigger condition must have bool type.");
            /* A bare identifier is an event label, not a variable reference. */
            if(rule->action->type!=AST_IDENTIFIER)
                semantic_check(ctx,rule->action);
            break;
        }

        case AST_RULE_ALWAYS: {
            ASTRuleAlways* rule=(ASTRuleAlways*)node;
            if(!rule->action) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Always rule is missing its action or event name.");
                break;
            }
            semantic_check(ctx,rule->action);
            break;
        }

        case AST_RULE_DEFAULT: {
            ASTRuleDefault* rule=(ASTRuleDefault*)node;
            if(!rule->action) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Default rule is missing its action or event name.");
                break;
            }
            semantic_check(ctx,rule->action);
            break;
        }

        case AST_APPLY_RULES: {
            ASTApplyRules* apply=(ASTApplyRules*)node;
            Slot* slot;
            if(!apply->ruleset_name || !apply->ruleset_name[0]) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "apply rules is missing its ruleset name.");
                break;
            }
            slot=find_slot(ctx,apply->ruleset_name);
            semantic_check_rule_arguments(ctx,node,apply->ruleset_name,
                apply->args,slot);
            break;
        }

        case AST_ON_BLOCK: {
            ASTOnBlock* handler=(ASTOnBlock*)node;
            Slot* ruleset;
            if(!handler->ruleset_name || !handler->ruleset_name[0]) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "on-block is missing its ruleset name.");
                break;
            }
            if(!handler->event_name || !handler->event_name[0]) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "on-block is missing its event name.");
                break;
            }
            if(!handler->body) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "on-block is missing its body.");
                break;
            }
            ruleset=find_slot(ctx,handler->ruleset_name);
            if(!ruleset || ruleset->kind!=SLOT_FUNC) {
                char message[256];
                snprintf(message,sizeof(message),"Unknown ruleset '%s' in on-block.",
                    handler->ruleset_name);
                report_error(ctx,node,ERR_SEMANTIC_UNDEFINED,message);
            } else if(!semantic_rule_has_event(ruleset,handler->event_name)) {
                char message[256];
                snprintf(message,sizeof(message),"Unknown event '%s' for ruleset '%s'.",
                    handler->event_name,handler->ruleset_name);
                report_error(ctx,node,ERR_SEMANTIC_UNDEFINED,message);
            }
            enter_scope(ctx,node);
            semantic_check(ctx,handler->body);
            exit_scope(ctx);
            break;
        }

        case AST_LAMBDA: {
            ASTLambda* lambda=(ASTLambda*)node;
            const char* enclosing_return_type=ctx->current_func_return_type;
            ctx->current_func_return_type=NULL;
            enter_scope(ctx,node);
            semantic_check(ctx,lambda->params);
            semantic_check(ctx,lambda->body);
            exit_scope(ctx);
            ctx->current_func_return_type=enclosing_return_type;
            semantic_record_annotation(ctx,node,NULL,"any",
                SEMANTIC_RESULT_VALUE_RVALUE);
            break;
        }

        case AST_RELOCATE: {
            ASTRelocate* relocate=(ASTRelocate*)node;
            const char* source_type;
            const char* target_type;
            semantic_check(ctx,relocate->source);
            semantic_check(ctx,relocate->target);
            source_type=get_expr_type(ctx,relocate->source);
            target_type=get_expr_type(ctx,relocate->target);
            if(source_type && target_type &&
               !are_types_compatible(source_type,target_type))
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Relocate source and destination types must match.");
            semantic_record_annotation(ctx,node,NULL,
                source_type?source_type:"any",SEMANTIC_RESULT_VALUE_RVALUE);
            break;
        }

        case AST_UNROLL: {
            ASTUnroll* unroll=(ASTUnroll*)node;
            semantic_check(ctx,unroll->count);
            if(!unroll->count || unroll->count->type!=AST_LITERAL ||
               ((ASTLiteral*)unroll->count)->type!=TOKEN_NUMBER ||
               ((ASTLiteral*)unroll->count)->int_value<0 ||
               !((ASTLiteral*)unroll->count)->string_value ||
               strchr(((ASTLiteral*)unroll->count)->string_value,'.')!=NULL) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Unroll count must be a non-negative integer literal.");
            }
            semantic_check(ctx,unroll->loop);
            break;
        }

        case AST_FUNC_DECL:
        case AST_EXTERN_DECL: {
            ASTFuncDecl* func = (ASTFuncDecl*)node;
            Slot* function_slot = NULL;
            const char* function_name = func->name;
            int return_type_valid = 1;
            int errors_before_return_type = ctx->error_count;
            if ((!function_name || !*function_name) &&
                ctx->current_method_context)
                function_name = ctx->current_method_public_name;
            if (func->return_type_expr) {
                semantic_check(ctx, func->return_type_expr);
                if (ctx->error_count != errors_before_return_type)
                    return_type_valid = 0;
            } else if (func->return_type &&
                       !is_type_defined(ctx, func->return_type)) {
                char message[320];
                (void)snprintf(message, sizeof(message),
                    "Function '%s' declares undefined return type '%s'.",
                    function_name ? function_name : "<anonymous>",
                    func->return_type);
                report_error(ctx, node, ERR_SEMANTIC_UNKNOWN_TYPE, message);
                return_type_valid = 0;
            }
            if (function_name && *function_name) {
                Slot* existing = find_slot(ctx, function_name);
                if (existing && existing->declaration_node == node &&
                    existing->kind == SLOT_FUNC) {
                    function_slot = existing;
                } else if (existing &&
                    (existing->depth == ctx->current_depth ||
                     (existing->kind == SLOT_FUNC &&
                      existing->group_owner == ctx->current_group_owner &&
                      existing->scope_id == ctx->current_scope_id))) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_FUNC_REDEFINED), function_name);
                    report_error(ctx, node, ERR_SEMANTIC_FUNC_REDEFINED, buf);
                } else {
                    function_slot = add_slot(
                        ctx, function_name, SLOT_FUNC, 1, func->return_type,
                        node, 0, 0, 0,
                        (func->is_public ? SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC : 0u) |
                        (func->is_exported ? SEMANTIC_RESULT_SYMBOL_FLAG_EXPORTED : 0u));
                }
                if (function_slot)
                    (void)semantic_populate_function_signature(
                        ctx, function_slot, func);
            }
            semantic_record_annotation(
                ctx, node, function_slot, func->return_type,
                SEMANTIC_RESULT_VALUE_FUNCTION_DESIGNATOR);

            const char* prev_return_type = ctx->current_func_return_type;
            unsigned int prev_loop_depth = ctx->loop_depth;
            ctx->loop_depth = 0u;
            /* Suppress dependent return-value mismatches when the declared
             * return type already failed validation. */
            ctx->current_func_return_type =
                return_type_valid ? func->return_type : NULL;
            enter_scope(ctx, node);
            if (func->params) semantic_check(ctx, func->params);

            if (ctx->current_method_context) {
                ASTNode* parameter_node = func->params;
                ASTVarDecl* first_parameter =
                    parameter_node && parameter_node->type == AST_VAR_DECL
                        ? (ASTVarDecl*)parameter_node : NULL;
                ASTVarDecl* self_parameter = NULL;
                uint32_t parameter_index = 0u;

                for (; parameter_node; parameter_node = parameter_node->next,
                     ++parameter_index) {
                    if (parameter_node->type == AST_VAR_DECL &&
                        ((ASTVarDecl*)parameter_node)->name &&
                        strcmp(((ASTVarDecl*)parameter_node)->name, "self") == 0) {
                        self_parameter = (ASTVarDecl*)parameter_node;
                        if (parameter_index != 0u) {
                            report_error(ctx, parameter_node,
                                ERR_SEMANTIC_TYPE_MISMATCH,
                                "The self receiver must be the first method parameter.");
                        }
                        break;
                    }
                }

                if (self_parameter && first_parameter == self_parameter) {
                    if (!ctx->current_method_is_object ||
                        !ctx->current_method_receiver_type ||
                        (ctx->result_enabled &&
                         ctx->current_method_receiver_type_id == 0u)) {
                        report_error(ctx, (ASTNode*)self_parameter,
                            ERR_SEMANTIC_TYPE_MISMATCH,
                            "self is only valid on an instance method of a Struct-bound Object Group.");
                    } else {
                        const char* explicit_type = self_parameter->type_name;
                        const char* normalized_type = explicit_type;
                        Slot* self_slot = semantic_find_slot_by_declaration(
                            ctx, (ASTNode*)self_parameter);
                        if (normalized_type && normalized_type[0] == '!')
                            ++normalized_type;

                        if (normalized_type && *normalized_type &&
                            strcmp(normalized_type,
                                ctx->current_method_receiver_type) != 0) {
                            report_error(ctx, (ASTNode*)self_parameter,
                                ERR_SEMANTIC_TYPE_MISMATCH,
                                "Explicit self type must exactly match the Group's bound Struct type.");
                        } else if (self_slot) {
                            if (!normalized_type || !*normalized_type) {
                                char* inferred_name = semantic_duplicate(
                                    ctx, ctx->current_method_receiver_type);
                                if (!inferred_name) {
                                    semantic_set_result_error(
                                        ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
                                } else {
                                    free(self_slot->type_name);
                                    self_slot->type_name = inferred_name;
                                    self_slot->type_id =
                                        ctx->current_method_receiver_type_id;
                                    if (self_slot->symbol_id != 0u) {
                                        SemanticResultStatus self_status =
                                            semantic_result_set_symbol_type(
                                                &ctx->result, self_slot->symbol_id,
                                                self_slot->type_id);
                                        semantic_set_result_error(ctx, self_status);
                                    }
                                    semantic_record_annotation(
                                        ctx, (ASTNode*)self_parameter, self_slot,
                                        self_slot->type_name,
                                        SEMANTIC_RESULT_VALUE_LVALUE);
                                }
                            } else if (self_slot->type_id == 0u) {
                                self_slot->type_id = semantic_type_id(
                                    ctx, self_parameter->type_name,
                                    self_parameter->is_hard_typed);
                                if (self_slot->symbol_id != 0u) {
                                    SemanticResultStatus self_status =
                                        semantic_result_set_symbol_type(
                                            &ctx->result, self_slot->symbol_id,
                                            self_slot->type_id);
                                    semantic_set_result_error(ctx, self_status);
                                }
                                semantic_record_annotation(
                                    ctx, (ASTNode*)self_parameter, self_slot,
                                    self_slot->type_name,
                                    SEMANTIC_RESULT_VALUE_LVALUE);
                            }

                            if (function_slot && function_slot->param_types &&
                                function_slot->param_count > 0 &&
                                !function_slot->param_types[0]) {
                                function_slot->param_types[0] =
                                    semantic_duplicate(ctx,
                                        ctx->current_method_receiver_type);
                                if (!function_slot->param_types[0]) {
                                    semantic_set_result_error(
                                        ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
                                }
                            }
                        }
                    }
                }
            }

            if (ctx->result_enabled && function_slot &&
                function_slot->symbol_id != 0u) {
                ASTNode* parameter_node = func->params;
                uint32_t ordinal = 0u;
                while (parameter_node) {
                    const char* parameter_name = "...";
                    Slot* parameter_slot = NULL;
                    uint32_t parameter_node_id =
                        semantic_node_id(ctx, parameter_node);
                    uint32_t parameter_type_id = 0u;
                    if (parameter_node->type == AST_VAR_DECL) {
                        ASTVarDecl* parameter=(ASTVarDecl*)parameter_node;
                        parameter_name=parameter->name;
                        parameter_slot=find_slot(ctx,parameter->name);
                        parameter_type_id=parameter_slot ? parameter_slot->type_id :
                            semantic_declared_type_id(ctx,parameter);
                    } else if (parameter_node->type != AST_VARIADIC_PARAM) {
                        semantic_set_result_error(ctx,SEMANTIC_RESULT_INVALID_DATA);
                        break;
                    }
                    {
                        SemanticResultStatus status = semantic_result_add_relation(
                            &ctx->result,
                            function_slot->symbol_id,
                            SEMANTIC_RESULT_RELATION_FUNCTION_PARAMETER,
                            ordinal++,
                            parameter_slot ? parameter_slot->symbol_id : 0u,
                            parameter_node_id,
                            parameter_name,
                            parameter_type_id);
                        semantic_set_result_error(ctx,status);
                    }
                    parameter_node = parameter_node->next;
                }
            }

            /*
             * The method receiver context applies to this implementation's
             * signature only. Nested/local function declarations in the body
             * must not accidentally inherit the outer method's implicit self.
             */
            {
                uint8_t method_context_for_function = ctx->current_method_context;
                ctx->current_method_context = 0u;
                if (func->body) {
                    ++ctx->function_depth;
                    semantic_check(ctx, func->body);
                    --ctx->function_depth;
                }
                ctx->current_method_context = method_context_for_function;
            }
            exit_scope(ctx);
            ctx->current_func_return_type = prev_return_type;
            ctx->loop_depth = prev_loop_depth;
            break;
        }

        case AST_IF: {
            ASTIf* conditional = (ASTIf*)node;
            if (!conditional->condition) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "if statement is missing its condition expression.");
                break;
            }
            if (!conditional->then_branch) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "if statement is missing its primary branch.");
                break;
            }
            semantic_check(ctx, conditional->condition);
            semantic_check_boolean_condition(
                ctx, conditional->condition, "if/elsif");
            if (conditional->then_branch) {
                if (conditional->then_branch->type == AST_BLOCK) {
                    semantic_check(ctx, conditional->then_branch);
                } else {
                    enter_scope(ctx, conditional->then_branch);
                    semantic_check(ctx, conditional->then_branch);
                    exit_scope(ctx);
                }
            }
            if (conditional->else_branch) {
                if (conditional->else_branch->type == AST_BLOCK) {
                    semantic_check(ctx, conditional->else_branch);
                } else {
                    enter_scope(ctx, conditional->else_branch);
                    semantic_check(ctx, conditional->else_branch);
                    exit_scope(ctx);
                }
            }
            break;
        }

        case AST_BREAK:
        case AST_CONTINUE:
            if (ctx->loop_depth == 0u) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    node->type == AST_BREAK
                        ? "break may only be used inside a loop."
                        : "continue may only be used inside a loop.");
            }
            break;

        case AST_BLOCK: {
            ASTBlock* block = (ASTBlock*)node;
            enter_scope(ctx, node);
            semantic_check(ctx, block->statements);
            exit_scope(ctx);
            break;
        }

        case AST_BLOCK_HALT:
            /* Cleanup marker is a typed, zero-value semantic instruction.
               Its ordering/lowering is owned by the later OIR cleanup pass. */
            semantic_record_annotation(
                ctx, node, NULL, "void", SEMANTIC_RESULT_VALUE_RVALUE);
            break;

        case AST_RETURN: {
            ASTUnaryExpr* ret = (ASTUnaryExpr*)node;
            if (ctx->function_depth == 0u) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "ret may only be used inside a function body.");
                break;
            }
            if (ret->operand) semantic_check(ctx, ret->operand);
            
            if (ctx->current_func_return_type) {
                const char* actual_type = ret->operand ? get_expr_type(ctx, ret->operand) : "void";
                if (!are_types_compatible(ctx->current_func_return_type, actual_type)) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_RETURN_MISMATCH), ctx->current_func_return_type, actual_type);
                    report_error(ctx, node, ERR_SEMANTIC_RETURN_MISMATCH, buf);
                }
            }
            break;
        }

        case AST_UNARY_EXPR: {
            ASTUnaryExpr* unary = (ASTUnaryExpr*)node;
            const char* operand_type;
            semantic_check(ctx, unary->operand);
            operand_type = get_expr_type(ctx, unary->operand);
            if (operand_type && unary->op == TOKEN_NOT &&
                !semantic_is_boolean_condition(ctx, unary->operand)) {
                char message[256];
                (void)snprintf(message, sizeof(message),
                    "Logical negation '!' requires a bool operand; got '%s'.",
                    operand_type);
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
            } else if (operand_type && unary->op == TOKEN_MINUS &&
                       !is_numeric_primitive_type(operand_type) &&
                       !semantic_vector_dimension(operand_type, &(size_t){0})) {
                char message[256];
                (void)snprintf(message, sizeof(message),
                    "Unary minus requires a numeric or Vec2/Vec3/Vec4 operand; got '%s'.",
                    operand_type);
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
            } else if (operand_type && unary->op == TOKEN_BIT_NOT &&
                       !is_bitwise_primitive_type(operand_type)) {
                char message[256];
                (void)snprintf(message, sizeof(message),
                    "Bitwise complement '~' does not support operand type '%s'; use an integer or bit-representation type.",
                    operand_type);
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
            }
            break;
        }

        case AST_POSTFIX_EXPR: {
            ASTUnaryExpr* postfix = (ASTUnaryExpr*)node;
            const char* operand_type = NULL;
            if (postfix->operand) {
                semantic_check(ctx, postfix->operand);
                operand_type = get_expr_type(ctx, postfix->operand);
            }
            if (!postfix->operand) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Postfix increment/decrement requires an operand.");
            } else if (!semantic_is_lvalue(ctx, postfix->operand)) {
                if (operand_type) {
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Postfix increment/decrement requires a mutable variable, field, or indexed element.");
                }
            } else if (semantic_lvalue_is_const(ctx, postfix->operand)) {
                char message[256];
                (void)snprintf(message, sizeof(message),
                    "Postfix increment/decrement cannot modify immutable lvalue '%s'.",
                    semantic_lvalue_description(postfix->operand));
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
            } else if (!operand_type || !is_numeric_primitive_type(operand_type)) {
                char message[256];
                (void)snprintf(message, sizeof(message),
                    "Postfix increment/decrement requires a numeric lvalue; got '%s'.",
                    operand_type ? operand_type : "<unknown>");
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
            }
            if (operand_type) semantic_record_annotation(
                ctx, node, NULL, operand_type, SEMANTIC_RESULT_VALUE_RVALUE);
            break;
        }

        case AST_STRUCT_LITERAL: {
            ASTStructLiteral* literal = (ASTStructLiteral*)node;
            Slot* struct_slot = literal->type_name
                ? find_slot(ctx, literal->type_name) : NULL;
            ASTNode* initializer;
            FieldInfo* field;

            /* Always visit initializer expressions so independent diagnostics
             * are retained even when the nominal type is invalid. */
            for (initializer = literal->fields; initializer;
                 initializer = initializer->next) {
                ASTStructFieldInit* init = (ASTStructFieldInit*)initializer;
                if (init->value) semantic_check(ctx, init->value);
            }

            if (!struct_slot) {
                char message[256];
                snprintf(message, sizeof(message),
                    "Struct literal type '%s' is unknown or not visible here.",
                    literal->type_name ? literal->type_name : "");
                report_error(ctx, node, ERR_SEMANTIC_UNKNOWN_TYPE, message);
                break;
            }
            if (struct_slot->kind != SLOT_STRUCT) {
                char message[256];
                snprintf(message, sizeof(message),
                    "Type '%s' is not a Struct and cannot use Struct literal syntax.",
                    literal->type_name ? literal->type_name : "");
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                break;
            }

            /* Resolve each named initializer against the declared field set. */
            for (initializer = literal->fields; initializer;
                 initializer = initializer->next) {
                ASTStructFieldInit* init = (ASTStructFieldInit*)initializer;
                ASTNode* previous;
                const char* actual_type = init->value
                    ? get_expr_type(ctx, init->value) : NULL;
                int duplicate = 0;

                for (previous = literal->fields; previous != initializer;
                     previous = previous->next) {
                    const ASTStructFieldInit* prior =
                        (const ASTStructFieldInit*)previous;
                    if (prior->name && init->name &&
                        strcmp(prior->name, init->name) == 0) {
                        duplicate = 1;
                        break;
                    }
                }
                if (duplicate) {
                    char message[256];
                    snprintf(message, sizeof(message),
                        "Struct literal initializes field '%s' more than once.",
                        init->name ? init->name : "");
                    report_error(ctx, (ASTNode*)init,
                        ERR_SEMANTIC_REDEFINED, message);
                    continue;
                }

                for (field = struct_slot->fields; field; field = field->next) {
                    if (init->name && strcmp(field->name, init->name) == 0)
                        break;
                }
                if (!field) {
                    char message[256];
                    snprintf(message, sizeof(message),
                        "Struct '%s' has no field named '%s'.",
                        struct_slot->name, init->name ? init->name : "");
                    report_error(ctx, (ASTNode*)init,
                        ERR_SEMANTIC_TYPE_MISMATCH, message);
                    continue;
                }

                if (actual_type && field->type_name &&
                    !are_types_compatible(field->type_name, actual_type)) {
                    char message[256];
                    snprintf(message, sizeof(message),
                        "Initializer for field '%s' has type '%s'; expected '%s'.",
                        field->name, actual_type, field->type_name);
                    report_error(ctx, (ASTNode*)init,
                        ERR_SEMANTIC_TYPE_MISMATCH, message);
                }

                semantic_record_annotation(
                    ctx, (ASTNode*)init, NULL, actual_type,
                    SEMANTIC_RESULT_VALUE_RVALUE);
            }

            /* Completeness is a Semantic rule, not an AST graph rule. */
            for (field = struct_slot->fields; field; field = field->next) {
                int initialized = 0;
                for (initializer = literal->fields; initializer;
                     initializer = initializer->next) {
                    const ASTStructFieldInit* init =
                        (const ASTStructFieldInit*)initializer;
                    if (init->name && strcmp(field->name, init->name) == 0) {
                        initialized = 1;
                        break;
                    }
                }
                if (!initialized) {
                    char message[256];
                    snprintf(message, sizeof(message),
                        "Struct literal is missing required field '%s'.",
                        field->name);
                    report_error(ctx, node,
                        ERR_SEMANTIC_TYPE_MISMATCH, message);
                }
            }

            semantic_record_annotation(
                ctx, node, struct_slot, struct_slot->name,
                SEMANTIC_RESULT_VALUE_RVALUE);
            break;
        }

        case AST_MEMBER_ACCESS: {
            FieldInfo* field;
            const char* type_name;
            if (((ASTMemberAccess*)node)->object) {
                uint8_t previous_suppression =
                    ctx->suppress_group_default_dispatch;
                ctx->suppress_group_default_dispatch = 1u;
                semantic_check_node(
                    ctx, ((ASTMemberAccess*)node)->object, 0);
                ctx->suppress_group_default_dispatch = previous_suppression;
            }
            field = semantic_resolve_member(ctx, node, 1);
            type_name = field ? field->type_name : NULL;
            if (field) semantic_record_annotation(
                ctx, node, NULL, type_name, SEMANTIC_RESULT_VALUE_LVALUE);
            break;
        }

        case AST_TUPLE:
            semantic_check(ctx, ((ASTTuple*)node)->elements);
            (void)get_expr_type(ctx,node);
            break;

        case AST_ASM_BLOCK:
            report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                "Inline assembly blocks are not enabled in the semantic pipeline.");
            break;

        case AST_FASTEXEC_BLOCK: {
            ASTFastExecBlock* fast = (ASTFastExecBlock*)node;
            const ASTNode* previous_fast_exec_owner =
                ctx->current_fast_exec_owner;
            enter_scope(ctx, node);
            ctx->current_fast_exec_owner = node;
            semantic_register_fast_exec_labels(ctx, fast->statements);
            semantic_check(ctx, fast->statements);
            ctx->current_fast_exec_owner = previous_fast_exec_owner;
            exit_scope(ctx);
            break;
        }

        case AST_ASM_LABEL:
            if (!ctx->current_fast_exec_owner) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "ASM labels may only be declared inside a FASTEXEC block.");
            }
            break;

        case AST_FUNC_CALL: {
            ASTCall* call = (ASTCall*)node;
            if (semantic_check_asm_intrinsic(ctx, node, call)) {
                if (call->args) semantic_check(ctx, call->args);
                break;
            }
            if (call->callee && call->callee->type == AST_MEMBER_ACCESS) {
                ASTMemberAccess* access = (ASTMemberAccess*)call->callee;
                Slot* method_slot;
                int is_instance_call = 0;
                int method_has_self = 0;
                uint32_t parameter_offset = 0u;

                if (access->object) {
                    uint8_t previous_suppression =
                        ctx->suppress_group_default_dispatch;
                    ctx->suppress_group_default_dispatch = 1u;
                    semantic_check_node(ctx, access->object, 0);
                    ctx->suppress_group_default_dispatch = previous_suppression;
                }
                method_slot = semantic_resolve_group_callable(
                    ctx, access, &is_instance_call, 1);
                if (method_slot && method_slot->declaration_node &&
                    (method_slot->declaration_node->type == AST_FUNC_DECL ||
                     method_slot->declaration_node->type == AST_EXTERN_DECL)) {
                    ASTFuncDecl* implementation =
                        (ASTFuncDecl*)method_slot->declaration_node;
                    method_has_self =
                        semantic_group_method_has_implicit_receiver(method_slot);

                    if (is_instance_call && !method_has_self) {
                        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                            "A static Group method must be called through its Group/type name, not an instance.");
                    } else if (!is_instance_call && method_has_self) {
                        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                            "An instance Group method requires an instance receiver.");
                    } else {
                        parameter_offset = method_has_self ? 1u : 0u;
                        semantic_validate_call_arguments(
                            ctx, node, method_slot, call->args, parameter_offset);
                        semantic_record_annotation(
                            ctx, call->callee, method_slot, method_slot->type_name,
                            SEMANTIC_RESULT_VALUE_FUNCTION_DESIGNATOR);
                        semantic_record_annotation(
                            ctx, node, method_slot, method_slot->type_name,
                            SEMANTIC_RESULT_VALUE_RVALUE);
                    }
                }
            } else if (call->callee) {
                semantic_check(ctx, call->callee);
                if (call->callee->type == AST_IDENTIFIER) {
                    ASTIdentifier* id = (ASTIdentifier*)call->callee;
                    Slot* function_slot = find_slot(ctx, id->name);
                    if (function_slot)
                        semantic_validate_call_arguments(
                            ctx, node, function_slot, call->args, 0u);
                }
            }
            if (call->args) semantic_check(ctx, call->args);
            break;
        }

        case AST_STRUCT_DECL:
        case AST_UNION_DECL: {
            ASTStructDecl* sdecl = (ASTStructDecl*)node;
            if (sdecl->name) {
                Slot* existing_type = find_slot(ctx, sdecl->name);
                int is_group_binding_name =
                    existing_type && existing_type->kind == SLOT_GROUP &&
                    ctx->current_group_owner &&
                    existing_type->declaration_node == ctx->current_group_owner &&
                    strcmp(((const ASTGroupDecl*)ctx->current_group_owner)->name,
                        sdecl->name) == 0;
                if (existing_type && !is_group_binding_name) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "Type '%s' already defined.", sdecl->name);
                    report_error(ctx, node, ERR_SEMANTIC_REDEFINED, buf);
                } else {
                    Slot* s = add_slot(
                        ctx, sdecl->name,
                        sdecl->is_union ? SLOT_UNION : SLOT_STRUCT,
                        1, NULL, node, 0, sdecl->is_packed, 0,
                        (sdecl->is_public ? SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC : 0u) |
                        (sdecl->is_exported ? SEMANTIC_RESULT_SYMBOL_FLAG_EXPORTED : 0u));
                    if (!s) break;
                    semantic_define_named_type(ctx, s, sdecl->name, NULL);
                    semantic_record_annotation(
                        ctx, node, s, sdecl->name,
                        SEMANTIC_RESULT_VALUE_UNSPECIFIED);

                    semantic_validate_aggregate_field_names(
                        ctx, sdecl->name, sdecl->fields);
                    // Alanları kaydet
                    ASTNode* field_node = sdecl->fields;
                    FieldInfo* last_field = NULL;
                    uint32_t field_ordinal = 0u;
                    while (field_node) {
                        ASTVarDecl* v = (ASTVarDecl*)field_node;
                        
                        // Alan tipini kontrol et
                        int is_inline_field = (v->inline_type != NULL);
                        int valid_type = 0;
                        if (v->type_name) {
                            if (is_type_defined(ctx, v->type_name)) valid_type = 1;
                            else if (is_inline_field) {
                                const char* t = (v->type_name[0] == '!') ? v->type_name + 1 : v->type_name;
                                if (strcmp(t, "struct") == 0 || strcmp(t, "union") == 0) valid_type = 1;
                            }
                        }

                        if (v->type_name && !valid_type && !v->type_expr) {
                            char buf[256];
                            (void)snprintf(buf, sizeof(buf),
                                "Field '%s' uses unknown type '%s'.",
                                v->name ? v->name : "<unnamed>", v->type_name);
                            report_error(ctx, (ASTNode*)v, ERR_SEMANTIC_UNKNOWN_TYPE, buf);
                        }
                        
                        if (v->inline_type && !v->type_expr)
                            semantic_check(ctx, v->inline_type);
                        if (v->type_expr) semantic_check(ctx, v->type_expr);

                        if (ctx->result_enabled && s && s->symbol_id != 0u) {
                            SemanticResultStatus relation_status =
                                semantic_result_add_relation(
                                    &ctx->result,
                                    s->symbol_id,
                                    SEMANTIC_RESULT_RELATION_AGGREGATE_FIELD,
                                    field_ordinal++,
                                    0u,
                                    semantic_node_id(ctx, field_node),
                                    v->name,
                                    semantic_type_id(ctx, v->type_name, v->is_hard_typed));
                            semantic_set_result_error(ctx, relation_status);
                        }

                        FieldInfo* f = (FieldInfo*)semantic_callocate(ctx, 1u, sizeof(FieldInfo));
                        if (!f) {
                            semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
                            break;
                        }
                        f->name = semantic_duplicate(ctx, v->name ? v->name : "");
                        f->type_name = v->type_name ? semantic_duplicate(ctx, v->type_name) : NULL;
                        f->is_const = v->is_const;
                        if (!f->name || (v->type_name && !f->type_name)) {
                            free(f->name);
                            free(f->type_name);
                            free(f);
                            semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
                            break;
                        }
                        
                        if (last_field) last_field->next = f;
                        else s->fields = f;
                        last_field = f;
                        
                        field_node = field_node->next;
                    }
                }
            } else {
                // Anonymous struct (inline) - Just check fields
                semantic_validate_aggregate_field_names(
                    ctx, NULL, sdecl->fields);
                ASTNode* field_node = sdecl->fields;
                while (field_node) {
                    ASTVarDecl* v = (ASTVarDecl*)field_node;
                    
                    int is_inline_field = (v->inline_type != NULL);
                    int valid_type = 0;
                    if (v->type_name) {
                        if (is_type_defined(ctx, v->type_name)) valid_type = 1;
                        else if (is_inline_field) {
                            const char* t = (v->type_name[0] == '!') ? v->type_name + 1 : v->type_name;
                            if (strcmp(t, "struct") == 0 || strcmp(t, "union") == 0) valid_type = 1;
                        }
                    }

                    if (v->type_name && !valid_type && !v->type_expr) {
                        char buf[256];
                        (void)snprintf(buf, sizeof(buf),
                            "Field '%s' uses unknown type '%s'.",
                            v->name ? v->name : "<unnamed>", v->type_name);
                        report_error(ctx, (ASTNode*)v, ERR_SEMANTIC_UNKNOWN_TYPE, buf);
                    }
                    
                    if (v->inline_type && !v->type_expr)
                        semantic_check(ctx, v->inline_type);
                    if (v->type_expr) semantic_check(ctx, v->type_expr);

                    field_node = field_node->next;
                }
            }
            break;
        }

        case AST_ENUM_DECL: {
            ASTEnumDecl* edecl = (ASTEnumDecl*)node;
            Slot* existing_type = find_slot(ctx, edecl->name);
            int is_group_binding_name =
                existing_type && existing_type->kind == SLOT_GROUP &&
                ctx->current_group_owner &&
                existing_type->declaration_node == ctx->current_group_owner &&
                strcmp(((const ASTGroupDecl*)ctx->current_group_owner)->name,
                    edecl->name) == 0;
            if (existing_type && !is_group_binding_name) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Type '%s' already defined.", edecl->name);
                report_error(ctx, node, ERR_SEMANTIC_REDEFINED, buf);
            } else {
                Slot* s = add_slot(ctx, edecl->name, SLOT_ENUM, 1, NULL, node, 0, 0, 0, 0u);
                if (!s) break;
                semantic_define_named_type(ctx, s, edecl->name, NULL);
                semantic_record_annotation(
                    ctx, node, s, edecl->name, SEMANTIC_RESULT_VALUE_UNSPECIFIED);

                ASTNode* var_node = edecl->variants;
                FieldInfo* last_var = NULL;
                uint32_t variant_ordinal = 0u;
                uint32_t payload_field_ordinal = 0u;
                while (var_node) {
                    ASTEnumVariant* v = (ASTEnumVariant*)var_node;

                    // Varyant içindeki tipleri kontrol et
                    if (v->data) {
                        ASTNode* field_node = v->data;
                        while(field_node) {
                            ASTVarDecl* fdecl = (ASTVarDecl*)field_node;
                            
                            int is_inline_field = (fdecl->inline_type != NULL);
                            int valid_type = 0;
                            if (fdecl->type_name) {
                                if (is_type_defined(ctx, fdecl->type_name)) valid_type = 1;
                                else if (is_inline_field) {
                                    const char* t = (fdecl->type_name[0] == '!') ? fdecl->type_name + 1 : fdecl->type_name;
                                    if (strcmp(t, "struct") == 0 || strcmp(t, "union") == 0) valid_type = 1;
                                }
                            }

                            if (fdecl->type_name && !valid_type && !fdecl->type_expr) {
                                char buf[256];
                                (void)snprintf(buf, sizeof(buf),
                                    "Field '%s' uses unknown type '%s'.",
                                    fdecl->name ? fdecl->name : "<unnamed>",
                                    fdecl->type_name);
                                report_error(ctx, (ASTNode*)fdecl, ERR_SEMANTIC_UNKNOWN_TYPE, buf);
                            }
                            if (fdecl->inline_type && !fdecl->type_expr)
                                semantic_check(ctx, fdecl->inline_type);
                            if (fdecl->type_expr)
                                semantic_check(ctx, fdecl->type_expr);
                            if (ctx->result_enabled && s && s->symbol_id != 0u) {
                                SemanticResultStatus relation_status =
                                    semantic_result_add_relation(
                                        &ctx->result,
                                        s->symbol_id,
                                        SEMANTIC_RESULT_RELATION_ENUM_PAYLOAD_FIELD,
                                        payload_field_ordinal++,
                                        0u,
                                        semantic_node_id(ctx, field_node),
                                        fdecl->name,
                                        semantic_type_id(ctx, fdecl->type_name, fdecl->is_hard_typed));
                                semantic_set_result_error(ctx, relation_status);
                            }
                            field_node = field_node->next;
                        }
                    }

                    if (ctx->result_enabled && s && s->symbol_id != 0u) {
                        SemanticResultStatus relation_status =
                            semantic_result_add_relation(
                                &ctx->result,
                                s->symbol_id,
                                SEMANTIC_RESULT_RELATION_ENUM_VARIANT,
                                variant_ordinal++,
                                0u,
                                semantic_node_id(ctx, var_node),
                                v->name,
                                s->type_id);
                        semantic_set_result_error(ctx, relation_status);
                    }

                    FieldInfo* f = (FieldInfo*)semantic_callocate(ctx, 1u, sizeof(FieldInfo));
                    if (!f) {
                        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
                        break;
                    }
                    f->name = semantic_duplicate(ctx, v->name ? v->name : "");
                    if (!f->name) {
                        free(f);
                        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
                        break;
                    }

                    if (last_var) last_var->next = f;
                    else s->fields = f; // Reusing fields for variants
                    last_var = f;
                    
                    var_node = var_node->next;
                }
            }
            break;
        }

        case AST_NEWTYPE_DECL: {
            ASTNewTypeDecl* nt = (ASTNewTypeDecl*)node;
            int target_valid = nt->target_type && nt->target_type[0] != '\0';
            int errors_before_target = ctx->error_count;
            if (!nt->name || nt->name[0] == '\0') {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "NewType declaration requires a non-empty type name.");
                break;
            }
            if (nt->target_type_expr) {
                semantic_check(ctx, nt->target_type_expr);
                if (ctx->error_count != errors_before_target) target_valid = 0;
            } else if (!target_valid || !is_type_defined(ctx, nt->target_type)) {
                char message[320];
                (void)snprintf(message, sizeof(message),
                    nt->target_type && nt->target_type[0]
                        ? "NewType '%s' references undefined target type '%s'."
                        : "NewType '%s' requires a target type (got '%s').",
                    nt->name, nt->target_type ? nt->target_type : "<missing>");
                report_error(ctx, node, ERR_SEMANTIC_UNKNOWN_TYPE, message);
                target_valid = 0;
            }
            if (find_slot(ctx, nt->name)) {
                char message[256];
                (void)snprintf(message, sizeof(message),
                    "Type '%s' is already defined in the visible scope.", nt->name);
                report_error(ctx, node, ERR_SEMANTIC_REDEFINED, message);
            } else if (target_valid) {
                Slot* newtype_slot = add_slot(
                    ctx, nt->name, SLOT_NEWTYPE, 1, nt->target_type,
                    node, 0, 0, 0, 0u);
                if (newtype_slot) {
                    semantic_define_named_type(
                        ctx, newtype_slot, nt->name, nt->target_type);
                    semantic_record_annotation(
                        ctx, node, newtype_slot, nt->name,
                        SEMANTIC_RESULT_VALUE_UNSPECIFIED);
                }
            }
            break;
        }
        
        case AST_LOOP: {
            ASTLoop* loop = (ASTLoop*)node;
            enter_scope(ctx, node);
            ++ctx->loop_depth;

            // 1. Init / Variable Handling
            if (loop->loop_type == 3) { // Foreach: loop(var <-- collection)
                if (loop->variable) {
                    if (loop->variable->type == AST_VAR_DECL) {
                        semantic_check(ctx, loop->variable);
                    } else if (loop->variable->type == AST_IDENTIFIER) {
                        // Implicit declaration: v:name
                        ASTIdentifier* id = (ASTIdentifier*)loop->variable;
                        if (!find_slot(ctx, id->name)) {
                            add_slot(ctx, id->name, SLOT_VAR, 0, "any", (ASTNode*)id, 1, 0, 0, 0u);
                        }
                        semantic_check(ctx, loop->variable);
                    }
                }
                if (loop->collection) semantic_check(ctx, loop->collection);
            } else {
                // For/While/Infinite
                if (loop->init) {
                    if (loop->init->type == AST_VAR_DECL) {
                        semantic_check(ctx, loop->init);
                    } else if (loop->init->type == AST_IDENTIFIER) {
                        // Implicit: loop(i, ...) -> v:i = 0
                        ASTIdentifier* id = (ASTIdentifier*)loop->init;
                        if (!find_slot(ctx, id->name)) {
                            add_slot(ctx, id->name, SLOT_VAR, 0, "i32", (ASTNode*)id, 1, 0, 0, 0u);
                        }
                        semantic_check(ctx, loop->init);
                    } else if (loop->init->type == AST_ASSIGNMENT) {
                        // Implicit: loop(i=0, ...) -> v:i=0
                        ASTBinaryExpr* assign = (ASTBinaryExpr*)loop->init;
                        if (assign->left->type == AST_IDENTIFIER) {
                            ASTIdentifier* id = (ASTIdentifier*)assign->left;
                            if (!find_slot(ctx, id->name)) {
                                const char* type = get_expr_type(ctx, assign->right);
                                add_slot(ctx, id->name, SLOT_VAR, 0, type ? type : "i32", (ASTNode*)id, 1, 0, 0, 0u);
                            }
                        }
                        semantic_check(ctx, loop->init);
                    } else {
                        semantic_check(ctx, loop->init);
                    }
                }
                
                if (loop->condition) {
                    semantic_check(ctx, loop->condition);
                    if (loop->condition->type == AST_BINARY_EXPR &&
                        ((ASTBinaryExpr*)loop->condition)->op == TOKEN_RANGE) {
                        ASTBinaryExpr* range = (ASTBinaryExpr*)loop->condition;
                        const char* begin_type = get_expr_type(ctx, range->left);
                        if (!find_slot(ctx, "it")) {
                            add_slot(ctx, "it", SLOT_VAR, 0,
                                begin_type ? begin_type : "any",
                                node, 1, 0, 0, 0u);
                        }
                    } else {
                        semantic_check_boolean_condition(
                            ctx, loop->condition, "Loop");
                    }
                }
                if (loop->increment) semantic_check(ctx, loop->increment);
            }

            if (loop->body) semantic_check(ctx, loop->body);
            if (ctx->loop_depth > 0u) --ctx->loop_depth;
            exit_scope(ctx);
            break;
        }

        case AST_NEXUS_LABEL: {
            ASTNexusLabel* lbl = (ASTNexusLabel*)node;
            if (find_slot(ctx, lbl->label_name)) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Label '%s' already defined.", lbl->label_name);
                report_error(ctx, node, ERR_SEMANTIC_REDEFINED, buf);
            } else {
                Slot* label_slot = add_slot(
                    ctx, lbl->label_name, SLOT_LABEL, 1, NULL, node, 0, 0, 0, 0u);
                semantic_record_annotation(
                    ctx, node, label_slot, NULL, SEMANTIC_RESULT_VALUE_UNSPECIFIED);
            }
            if (lbl->statement) semantic_check(ctx, lbl->statement);
            break;
        }

        case AST_NEXUS_JUMP: {
            ASTNexusJump* jmp = (ASTNexusJump*)node;
            // İleriye dönük atlamalar (forward jump) olabileceği için burada kesin varlık kontrolü yapmıyoruz
            break;
        }

        case AST_NEXUS_HALT: {
            ASTHalt* halt = (ASTHalt*)node;
            if (halt->expr) semantic_check(ctx, halt->expr);
            break;
        }

        case AST_NEXUS_INTENT: {
            ASTIntent* in = (ASTIntent*)node;
            if (in->target) semantic_check(ctx, in->target);
            break;
        }

        case AST_AI_LOGIC: {
            // AI query validation could go here
            break;
        }

        case AST_UNPACK: {
            ASTUnpack* u = (ASTUnpack*)node;
            semantic_check(ctx, u->expr);
            break;
        }

        case AST_CAST: {
            ASTCast* cast = (ASTCast*)node;
            if (cast->value) semantic_check(ctx, cast->value);
            /* The target is a type designator, not a runtime value expression. */
            if (cast->target_type && cast->target_type->type == AST_IDENTIFIER) {
                const char* target_name = ((ASTIdentifier*)cast->target_type)->name;
                if (target_name && !is_type_defined(ctx, target_name)) {
                    char message[256];
                    (void)snprintf(message, sizeof(message),
                        "cast() target type '%s' is not declared.", target_name);
                    report_error(ctx, node, ERR_SEMANTIC_UNKNOWN_TYPE, message);
                }
            } else {
                report_error(ctx, node, ERR_SEMANTIC_UNKNOWN_TYPE,
                    "cast() requires a type name as its second argument.");
            }
            break;
        }

        case AST_PANIC: {
            break;
        }

        case AST_LISTEN: {
            ASTNexusCapture* listen = (ASTNexusCapture*)node;
            // Listen bloğu içinde 'global_listener' slotu veya benzeri bir kapsam açılabilir
            enter_scope(ctx, node);
            Slot* listen_slot = add_slot(
                ctx, listen->handler_name, SLOT_VAR, 1, "any", node, 1, 0, 0, 0u);
            semantic_record_annotation(
                ctx, node, listen_slot, "any", SEMANTIC_RESULT_VALUE_LVALUE);
            if (listen->expr) semantic_check(ctx, listen->expr);
            exit_scope(ctx);
            break;
        }

        case AST_ECHO:
        case AST_SPAWN:
        case AST_SEND:
        case AST_RECEIVE:
        case AST_DONE:
        case AST_AREA:
        case AST_ZONE:
        case AST_FREE:
        case AST_ADDR:
        case AST_LEN:
        case AST_PEEK:
        case AST_POKE:
        case AST_INB:
        case AST_OUTB:
        case AST_IRQ:
        case AST_REG: {
            ASTCall* call = (ASTCall*)node;
            if (call->callee) semantic_check(ctx, call->callee);
            if (call->args) semantic_check(ctx, call->args);
            if (node->type == AST_ADDR || node->type == AST_LEN ||
                node->type == AST_PEEK || node->type == AST_POKE)
                semantic_validate_memory_intrinsic(ctx, node);
            else if (node->type == AST_INB || node->type == AST_OUTB)
                semantic_validate_port_intrinsic(ctx, node);
            else if (node->type == AST_AREA || node->type == AST_ZONE ||
                     node->type == AST_FREE)
                semantic_validate_allocation_intrinsic(ctx, node);
            break;
        }

        case AST_GROUP_DECL: {
            ASTGroupDecl* g = (ASTGroupDecl*)node;
            const ASTNode* previous_group_owner = ctx->current_group_owner;
            Slot* group_slot = NULL;
            Slot* existing_group = find_slot(ctx, g->name);
            uint32_t member_ordinal = 0u;
            uint32_t method_ordinal = 0u;
            const ASTNode* member;

            semantic_validate_group_member_names(ctx, g);
            if (existing_group && existing_group->depth == ctx->current_depth) {
                char message[256];
                snprintf(message, sizeof(message),
                    "Group '%s' is already declared in this scope.", g->name);
                report_error(ctx, node, ERR_SEMANTIC_REDEFINED, message);
            } else {
                group_slot = add_slot(ctx, g->name, SLOT_GROUP, 0, NULL,
                    node, 0, 0, 0,
                    (g->is_public ? SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC : 0u) |
                    (g->is_exported ? SEMANTIC_RESULT_SYMBOL_FLAG_EXPORTED : 0u));
                semantic_record_annotation(ctx, node, group_slot, NULL,
                    SEMANTIC_RESULT_VALUE_UNSPECIFIED);
            }

            enter_scope(ctx, node);
            ctx->current_group_owner = node;
            {
                ASTNode* binding = NULL;
                ASTNode* current;
                for (current = g->members; current; current = current->next) {
                    const char* member_name = semantic_group_member_name(current);
                    if (member_name && strcmp(member_name, g->name) == 0 &&
                        (current->type == AST_STRUCT_DECL ||
                         current->type == AST_ENUM_DECL ||
                         current->type == AST_UNION_DECL)) {
                        binding = current;
                        break;
                    }
                }
                if (binding) semantic_check_node(ctx, binding, 0);
                {
                    Slot* receiver_slot = binding
                        ? semantic_find_slot_by_declaration(ctx, binding) : NULL;
                    semantic_predeclare_group_callables(ctx, g, receiver_slot);
                }
                for (current = g->members; current; current = current->next) {
                    if (current != binding) semantic_check_node(ctx, current, 0);
                }
            }

            if (ctx->result_enabled && ctx->result_status == SEMANTIC_RESULT_OK &&
                group_slot && group_slot->symbol_id != 0u) {
                for (member = g->members; member; member = member->next) {
                    const char* member_name = semantic_group_member_name(member);
                    Slot* member_slot = semantic_find_slot_by_declaration(
                        ctx, member);
                    uint32_t member_node_id = semantic_node_id(ctx, member);
                    SemanticResultStatus relation_status;
                    if (!member_name || !*member_name || member_node_id == 0u) {
                        semantic_set_result_error(ctx, SEMANTIC_RESULT_INVALID_DATA);
                        break;
                    }
                    relation_status = semantic_result_add_relation(
                        &ctx->result, group_slot->symbol_id,
                        SEMANTIC_RESULT_RELATION_GROUP_MEMBER,
                        member_ordinal++,
                        member_slot ? member_slot->symbol_id : 0u,
                        member_node_id, member_name,
                        member_slot ? member_slot->type_id : 0u);
                    semantic_set_result_error(ctx, relation_status);
                    if (ctx->result_status != SEMANTIC_RESULT_OK) break;

                    if (member->type == AST_STRUCT_DECL ||
                        member->type == AST_UNION_DECL ||
                        member->type == AST_ENUM_DECL) {
                        const char* nominal_name = semantic_group_member_name(member);
                        if (nominal_name && strcmp(nominal_name, g->name) == 0) {
                            if (!member_slot || member_slot->symbol_id == 0u) {
                                semantic_set_result_error(
                                    ctx, SEMANTIC_RESULT_INVALID_DATA);
                            } else {
                                relation_status = semantic_result_add_relation(
                                    &ctx->result, group_slot->symbol_id,
                                    SEMANTIC_RESULT_RELATION_GROUP_TYPE_BINDING,
                                    0u, member_slot->symbol_id, member_node_id,
                                    nominal_name, member_slot->type_id);
                                semantic_set_result_error(ctx, relation_status);
                            }
                        }
                    } else if (member->type == AST_METHOD) {
                        const ASTMethod* method = (const ASTMethod*)member;
                        if (method->public_name &&
                            strcmp(method->public_name, "default") == 0) {
                            relation_status = semantic_result_add_relation(
                                &ctx->result, group_slot->symbol_id,
                                SEMANTIC_RESULT_RELATION_GROUP_DEFAULT_HANDLER,
                                0u, 0u, member_node_id, "default", 0u);
                            semantic_set_result_error(ctx, relation_status);
                        } else if (method->implementation &&
                            (method->implementation->type == AST_FUNC_DECL ||
                             method->implementation->type == AST_EXTERN_DECL)) {
                            Slot* implementation_slot =
                                semantic_find_slot_by_declaration(
                                    ctx, method->implementation);
                            uint32_t implementation_node_id = semantic_node_id(
                                ctx, method->implementation);
                            if (!implementation_slot ||
                                implementation_slot->symbol_id == 0u ||
                                implementation_node_id == 0u) {
                                semantic_set_result_error(
                                    ctx, SEMANTIC_RESULT_INVALID_DATA);
                            } else {
                                relation_status = semantic_result_add_relation(
                                    &ctx->result, group_slot->symbol_id,
                                    SEMANTIC_RESULT_RELATION_GROUP_METHOD_IMPLEMENTATION,
                                    method_ordinal++,
                                    implementation_slot->symbol_id,
                                    implementation_node_id,
                                    method->public_name,
                                    implementation_slot->type_id);
                                semantic_set_result_error(ctx, relation_status);
                            }
                        } else {
                            semantic_set_result_error(ctx, SEMANTIC_RESULT_INVALID_DATA);
                        }
                    }
                    if (ctx->result_status != SEMANTIC_RESULT_OK) break;
                }
            }

            exit_scope(ctx);
            ctx->current_group_owner = previous_group_owner;
            break;
        }

        case AST_METHOD: {
            ASTMethod* m = (ASTMethod*)node;
            const char* previous_receiver_type = ctx->current_method_receiver_type;
            const char* previous_method_public_name = ctx->current_method_public_name;
            uint32_t previous_receiver_type_id = ctx->current_method_receiver_type_id;
            uint8_t previous_method_context = ctx->current_method_context;
            uint8_t previous_method_is_object = ctx->current_method_is_object;
            Slot* receiver_slot = NULL;

            ctx->current_method_context = 1u;
            ctx->current_method_public_name = m->public_name;
            ctx->current_method_is_object = 0u;
            ctx->current_method_receiver_type = NULL;
            ctx->current_method_receiver_type_id = 0u;

            if (ctx->current_group_owner) {
                const ASTGroupDecl* owner =
                    (const ASTGroupDecl*)ctx->current_group_owner;
                const ASTNode* member;
                for (member = owner->members; member; member = member->next) {
                    const char* member_name = semantic_group_member_name(member);
                    if (member->type == AST_STRUCT_DECL && member_name &&
                        strcmp(member_name, owner->name) == 0) {
                        receiver_slot = semantic_find_slot_by_declaration(
                            ctx, member);
                        break;
                    }
                }
            }
            if (receiver_slot && receiver_slot->kind == SLOT_STRUCT) {
                ctx->current_method_is_object = 1u;
                ctx->current_method_receiver_type = receiver_slot->name;
                ctx->current_method_receiver_type_id = receiver_slot->type_id;
            }

            if (m->implementation) semantic_check_node(ctx, m->implementation, 0);

            ctx->current_method_receiver_type = previous_receiver_type;
            ctx->current_method_receiver_type_id = previous_receiver_type_id;
            ctx->current_method_context = previous_method_context;
            ctx->current_method_public_name = previous_method_public_name;
            ctx->current_method_is_object = previous_method_is_object;
            break;
        }

        case AST_PRNT:
        case AST_PRMT:
        case AST_WRITE:
        case AST_FWRITE:
        case AST_TYPEOF:
        case AST_SIZEOF:
        case AST_IS_OK:
        case AST_IS_ERR:
        case AST_IS_SOME:
        case AST_IS_NONE:
        case AST_IS_NULL:
        case AST_EXIT:
        case AST_SWAP:
        case AST_INTR:
        case AST_DATE:
        case AST_DATENOW:
        case AST_TIME:
        case AST_CLOCK:
        case AST_ZONE_SYNC:
        case AST_ZONE_VIEW:
        case AST_ZONE_LOCK: {
            ASTCall* call = (ASTCall*)node;
            if (call->callee) semantic_check(ctx, call->callee);
            if (call->args) semantic_check(ctx, call->args);
            break;
        }

        case AST_USE: {
            // Modül yükleme mantığı
            break;
        }

        case AST_DEFER: {
            semantic_check(ctx, ((ASTDefer*)node)->body);
            break;
        }

        case AST_THREAD_BLOCK: {
            ASTThreadBlock* tb = (ASTThreadBlock*)node;
            enter_scope(ctx, node);
            semantic_check(ctx, tb->body);
            exit_scope(ctx);
            break;
        }

        default:
            break;
    }

    if (ctx->result_enabled) {
        switch (node->type) {
            case AST_LITERAL:
            case AST_BINARY_EXPR:
            case AST_ASSIGNMENT:
            case AST_UNARY_EXPR:
            case AST_FUNC_CALL:
                (void)get_expr_type(ctx, node);
                break;
            default:
                break;
        }
    }

    if (follow_siblings && node->next) semantic_check(ctx, node->next);
}

void semantic_check(SemanticContext* ctx, ASTNode* node) {
    semantic_check_node(ctx, node, 1);
}

void semantic_print_errors(SemanticContext* ctx) {
    if (ctx->error_count == 0) return;

    printf("\n%s (%d) ---\n", msg_get(MSG_SEMANTIC_HEADER), ctx->error_count);
    Error* current = ctx->error_head;
    while (current) {
        const char* type_label = current->is_warning ? "Uyarı" : "Hata";
        char type_char = current->is_warning ? 'W' : 'E';
        
        printf("[%c%03d] %s: %s\n", type_char, current->code, type_label, current->message);
        printf("       Dosya: %s Satır: %d:%d\n", current->filename, current->line, current->col);
        
        if (current->suggestion) {
            if (current->similarity > 0) printf("       Öneri: %s (Benzerlik: %%%d)\n", current->suggestion, current->similarity);
            else printf("       Öneri: %s\n", current->suggestion);
        }
        if (current->hint) printf("       İpucu: %s\n", current->hint);
        printf("\n");
        current = current->next;
    }
    printf("%s\n", msg_get(MSG_SEMANTIC_SEPARATOR));
}

void semantic_destroy(SemanticContext* ctx) {
    if (!ctx) return;

    Slot* current = ctx->head;
    while (current) {
        Slot* next = current->next;
        destroy_slot(current);
        current = next;
    }
    ctx->head = NULL;

    Error* err = ctx->error_head;
    while (err) {
        Error* next = err->next;
        free(err->filename);
        free(err->message);
        if (err->suggestion) free(err->suggestion);
        if (err->hint) free(err->hint);
        free(err);
        err = next;
    }
    ctx->error_head = NULL;
    ctx->error_tail = NULL;
    semantic_result_destroy(&ctx->result);
    ctx->result_enabled = 0u;
    ctx->current_scope_id = 0u;
}