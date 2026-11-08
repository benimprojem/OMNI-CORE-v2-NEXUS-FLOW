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
    ctx->current_func_return_type = NULL;
    ctx->diagnostics = NULL;
    ctx->diagnostic_stage_id = OCC_STAGE_SEMANTIC;
    ctx->current_scope_id = 1u;
    ctx->current_group_owner = NULL;
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
            input.span.start_line = node->line > 0 ? (uint32_t)node->line : 0u;
            input.span.start_column = node->col > 0 ? (uint32_t)node->col : 0u;
            input.span.end_line = input.span.start_line;
            input.span.end_column = input.span.start_column;
            input.span.has_location = (uint8_t)(node->line > 0 && node->col > 0);
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
    err->line = node ? node->line : 0;
    err->col = node ? node->col : 0;
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
        "u16", "i16", "i8", "f32", "char", "byte", "void", "any"
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

    if (strcmp(name, "array") == 0) {
        kind = SEMANTIC_RESULT_TYPE_AGGREGATE;
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

static int semantic_eval_integer_constant(
    const ASTNode* expression, int64_t* value_out
) {
    if (!expression || !value_out) return 0;
    if (expression->type == AST_LITERAL) {
        const ASTLiteral* literal = (const ASTLiteral*)expression;
        const char* cursor;
        char* end = NULL;
        long long value;
        if (literal->type != TOKEN_NUMBER || !literal->string_value ||
            !literal->string_value[0]) return 0;
        for (cursor = literal->string_value; *cursor; ++cursor) {
            if (*cursor < '0' || *cursor > '9') return 0;
        }
        errno = 0;
        value = strtoll(literal->string_value, &end, 10);
        if (errno == ERANGE || !end || *end != '\0') return 0;
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
    s->type_id = semantic_type_id(ctx, type_name, is_hard);

    if (ctx->result_enabled &&
        ctx->result_status == SEMANTIC_RESULT_OK) {
        uint32_t declaration_node_id = semantic_node_id(ctx, declaration_node);
        uint16_t result_flags = 0u;
        if (is_const && kind == SLOT_VAR) {
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
        0u);
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
            if (call->callee && call->callee->type == AST_IDENTIFIER) {
                ASTIdentifier* id = (ASTIdentifier*)call->callee;
                slot = find_slot(ctx, id->name);
            } else if (call->callee &&
                call->callee->type == AST_MEMBER_ACCESS) {
                int is_instance_call = 0;
                slot = semantic_resolve_group_callable(
                    ctx, (ASTMemberAccess*)call->callee,
                    &is_instance_call, 0);
            }
            semantic_record_annotation(
                ctx, node, slot, slot ? slot->type_name : NULL,
                SEMANTIC_RESULT_VALUE_RVALUE);
            return slot ? slot->type_name : NULL;
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
            if (target_type && strncmp(target_type, "array|", 6u) == 0) {
                const char* begin = target_type + 6u;
                const char* end = strchr(begin, '[');
                size_t length = end ? (size_t)(end - begin) : strlen(begin);
                if (length > 0u && length < 64u) {
                    if (length < sizeof(indexed_element_type_buffer)) {
                        memcpy(indexed_element_type_buffer, begin, length);
                        indexed_element_type_buffer[length] = '\0';
                        element_type = indexed_element_type_buffer;
                    }
                }
            }
            return element_type;
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
    if (strcmp(t1, "array") == 0 &&
        semantic_is_array_type_name(t2)) return 1;
    if (strcmp(t2, "array") == 0 &&
        semantic_is_array_type_name(t1)) return 1;
    return semantic_array_signatures_compatible(t1, t2);
}

static int is_numeric_primitive_type(const char* type_name) {
    static const char* const names[] = {
        "i8", "i16", "i32", "i64",
        "u8", "u16", "u32", "u64",
        "f32", "f64"
    };
    size_t i;
    if (!type_name) return 0;
    if (type_name[0] == '!') ++type_name;
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

static int is_type_defined(SemanticContext* ctx, const char* type_name) {
    if (!type_name) return 1; // Infer type or void
    if (type_name[0] == '!') type_name++; // Skip hard-type prefix

    // Generic and structurally typed array aggregates.
    if (semantic_is_array_type_name(type_name)) return 1;

    // Primitives
    if (strcmp(type_name, "i32") == 0 || strcmp(type_name, "u8") == 0 ||
        strcmp(type_name, "str") == 0 || strcmp(type_name, "bool") == 0 ||
        strcmp(type_name, "f64") == 0 || strcmp(type_name, "i64") == 0 ||
        strcmp(type_name, "u64") == 0 || strcmp(type_name, "u32") == 0 ||
        strcmp(type_name, "u16") == 0 || strcmp(type_name, "i16") == 0 ||
        strcmp(type_name, "i8") == 0 || strcmp(type_name, "f32") == 0 ||
        strcmp(type_name, "char") == 0 || strcmp(type_name, "byte") == 0 ||
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

static void semantic_check_pattern_bindings(SemanticContext* ctx, ASTNode* pattern) {
    ASTNode* current;
    for (current=pattern; current; current=current->next) {
        if (current->type==AST_PATTERN_BINDING) {
            ASTPatternBinding* binding=(ASTPatternBinding*)current;
            Slot* existing=find_slot(ctx,binding->name ? binding->name : "");
            if(existing && existing->depth==ctx->current_depth) {
                report_error(ctx,current,ERR_SEMANTIC_REDEFINED,
                    "Pattern binding is duplicated in the same arm.");
            } else {
                Slot* slot=add_slot(ctx,binding->name,SLOT_VAR,0,"any",
                    current,1,0,0,0u);
                semantic_record_annotation(ctx,current,slot,"any",
                    SEMANTIC_RESULT_VALUE_LVALUE);
            }
        } else if (current->type==AST_PATTERN_CONSTRUCTOR) {
            semantic_check_pattern_bindings(ctx,
                ((ASTPatternConstructor*)current)->bindings);
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
            if(duplicate)continue;
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
    if (function_slot->param_count != 0 || function_slot->param_types)
        return 1;

    for (parameter = function->params; parameter; parameter = parameter->next)
        ++parameter_count;
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
    if (argument_count != expected_count) {
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

    for (argument = arguments; argument; argument = argument->next,
         ++argument_index) {
        uint32_t parameter_index = argument_index + parameter_offset;
        const char* argument_type = get_expr_type(ctx, argument);
        const char* expected_type =
            function_slot->param_types &&
            parameter_index < (uint32_t)function_slot->param_count
                ? function_slot->param_types[parameter_index] : NULL;
        ASTVarDecl* parameter = NULL;

        if (declaration) {
            ASTNode* parameter_node = declaration->params;
            uint32_t current_index = 0u;
            while (parameter_node && current_index < parameter_index) {
                parameter_node = parameter_node->next;
                ++current_index;
            }
            if (parameter_node && parameter_node->type == AST_VAR_DECL)
                parameter = (ASTVarDecl*)parameter_node;
        }

        if (argument_type && expected_type &&
            !are_types_compatible(expected_type, argument_type)) {
            char message[256];
            snprintf(message, sizeof(message), msg_get(ERR_SEMANTIC_ARG_TYPE),
                (int)(parameter_index + 1u), expected_type, argument_type);
            report_error(ctx, argument, ERR_SEMANTIC_ARG_TYPE, message);
        } else if (parameter && parameter->inline_type &&
            parameter->inline_type->type == AST_ARRAY_TYPE &&
            argument->type == AST_ARRAY_LITERAL) {
            semantic_validate_array_initializer(
                ctx, argument,
                ((ASTArrayType*)parameter->inline_type)->dimensions,
                parameter->type_name);
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
static void semantic_check_node(SemanticContext* ctx, ASTNode* node, int follow_siblings);

static void semantic_check_node(
    SemanticContext* ctx, ASTNode* node, int follow_siblings
) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
            semantic_check(ctx, ((ASTProgram*)node)->declarations);
            break;

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

                if (decl->type_name && !valid_type) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_UNKNOWN_TYPE),
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

            if (decl->inline_type) semantic_check(ctx, decl->inline_type);
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
                    if (init_type && !are_types_compatible(decl->type_name, init_type)) {
                        char buf[256];
                        snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_TYPE_MISMATCH),
                            decl->type_name, init_type);
                        report_issue(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, buf,
                            NULL, "İlgili veri tipini kontrol edin.", 0, 0);
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
            semantic_check(ctx,((ASTArrayLiteral*)node)->elements);
            break;
        case AST_INDEX_ACCESS: {
            ASTIndexAccess* access=(ASTIndexAccess*)node;
            const char* target_type;
            const char* index_type;
            if (!access->target || !access->index) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Array indexing requires both target and index.");
                break;
            }
            semantic_check(ctx,access->target);
            semantic_check(ctx,access->index);
            target_type=get_expr_type(ctx,access->target);
            if (!target_type || !semantic_is_array_type_name(target_type)) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Index target must be an array value.");
            }
            index_type=get_expr_type(ctx,access->index);
            if (index_type && strcmp(index_type,"any")!=0 &&
                strcmp(index_type,"i32")!=0 && strcmp(index_type,"i64")!=0 &&
                strcmp(index_type,"u32")!=0 && strcmp(index_type,"u64")!=0 &&
                strcmp(index_type,"int")!=0 && strcmp(index_type,"uint")!=0) {
                report_error(ctx,access->index,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Array index must have an integer type.");
            }
            semantic_record_annotation(ctx,node,NULL,
                get_expr_type(ctx,node),SEMANTIC_RESULT_VALUE_LVALUE);
            break;
        }
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
                    if (!are_types_compatible(left_type, right_type)) {
                        char message[256];
                        snprintf(message, sizeof(message),
                            msg_get(ERR_SEMANTIC_TYPE_MISMATCH),
                            left_type, right_type);
                        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                    } else if (!is_numeric_primitive_type(left_type) ||
                               !is_numeric_primitive_type(right_type) ||
                               (bin->op == TOKEN_PERCENT &&
                                !is_integer_primitive_type(left_type))) {
                        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                            "Arithmetic operators require matching numeric operands; remainder requires integer operands.");
                    }
                }
            }

            if (bin->op == TOKEN_BIT_AND || bin->op == TOKEN_BIT_OR ||
                bin->op == TOKEN_BIT_XOR) {
                const char* left_type=get_expr_type(ctx,bin->left);
                const char* right_type=get_expr_type(ctx,bin->right);
                if(left_type&&right_type&&
                   (!are_types_compatible(left_type,right_type)||
                    !is_integer_primitive_type(left_type)||
                    !is_integer_primitive_type(right_type))) {
                    report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "Bitwise operators require matching integer operands.");
                }
            }

            if (bin->op == TOKEN_EQ || bin->op == TOKEN_NEQ ||
                bin->op == TOKEN_GT || bin->op == TOKEN_LT ||
                bin->op == TOKEN_GTE || bin->op == TOKEN_LTE) {
                const char* left_type = get_expr_type(ctx, bin->left);
                const char* right_type = get_expr_type(ctx, bin->right);
                if (left_type && right_type &&
                    !are_types_compatible(left_type, right_type)) {
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Comparison operands must have compatible types.");
                } else if ((bin->op == TOKEN_GT || bin->op == TOKEN_LT ||
                            bin->op == TOKEN_GTE || bin->op == TOKEN_LTE) &&
                           left_type && !is_numeric_primitive_type(left_type)) {
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Ordered comparisons require numeric operands.");
                }
            } else if (bin->op == TOKEN_AND || bin->op == TOKEN_OR) {
                if (!semantic_is_boolean_condition(ctx, bin->left) ||
                    !semantic_is_boolean_condition(ctx, bin->right)) {
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Logical operators require bool operands.");
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
                        char buf[256];
                        snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_TYPE_MISMATCH),
                            slot->type_name, right_type);
                        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, buf);
                    }
                }
            } else if (assign->left->type == AST_INDEX_ACCESS) {
                const char* element_type = get_expr_type(ctx, assign->left);
                const char* right_type = get_expr_type(ctx, assign->right);
                if (semantic_lvalue_is_const(ctx, assign->left)) {
                    report_error(ctx, node, ERR_SEMANTIC_CONST_ASSIGN,
                        "Cannot assign through an index of a constant array.");
                }
                if (element_type && right_type &&
                    !are_types_compatible(element_type, right_type)) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_TYPE_MISMATCH),
                        element_type, right_type);
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, buf);
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
                            char buf[256];
                            snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_TYPE_MISMATCH),
                                field->type_name, right_type);
                            report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, buf);
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
                if(assign->op==TOKEN_STRING_APPEND) {
                    if((left_type&&strcmp(left_type,"str")!=0)||
                       (right_type&&strcmp(right_type,"str")!=0))
                        report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                            "String append (.=) requires str operands.");
                } else if(numeric_compound||bitwise_compound) {
                    if(left_type&&right_type&&
                       (!are_types_compatible(left_type,right_type)||
                        !is_numeric_primitive_type(left_type)||
                        !is_numeric_primitive_type(right_type)||
                        (bitwise_compound&&
                         (!is_integer_primitive_type(left_type)||
                          !is_integer_primitive_type(right_type)))))
                        report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                            bitwise_compound?
                            "Bitwise compound assignment requires matching integer operands.":
                            "Numeric compound assignment requires matching numeric operands.");
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
        case AST_SELECT:
            semantic_check(ctx,((ASTSelect*)node)->selector);
            semantic_check(ctx,((ASTSelect*)node)->arms);
            break;

        case AST_SCAN:
            semantic_check(ctx,((ASTScan*)node)->subject);
            semantic_check(ctx,((ASTScan*)node)->arms);
            break;

        case AST_SELECT_ARM: {
            ASTSelectArm* arm=(ASTSelectArm*)node;
            enter_scope(ctx,node);
            semantic_check_pattern_bindings(ctx,arm->pattern);
            semantic_check(ctx,arm->body);
            exit_scope(ctx);
            break;
        }

        case AST_SCAN_ARM: {
            ASTScanArm* arm=(ASTScanArm*)node;
            enter_scope(ctx,node);
            semantic_check_pattern_bindings(ctx,arm->pattern);
            semantic_check(ctx,arm->body);
            exit_scope(ctx);
            break;
        }

        case AST_RULESET_DECL: {
            ASTRulesetDecl* ruleset=(ASTRulesetDecl*)node;
            Slot* existing=find_slot(ctx,ruleset->name?ruleset->name:"");
            Slot* slot=NULL;
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
            if(rule->condition) {
                semantic_check(ctx,rule->condition);
                if(!semantic_is_boolean_condition(ctx,rule->condition))
                    report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "Rule barrier condition must have bool type.");
            }
            if(rule->message) {
                const char* message_type;
                semantic_check(ctx,rule->message);
                message_type=get_expr_type(ctx,rule->message);
                if(message_type && strcmp(message_type,"str")!=0)
                    report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "Rule barrier message must have str type.");
            }
            break;
        }

        case AST_RULE_TRIGGER: {
            ASTRuleTrigger* rule=(ASTRuleTrigger*)node;
            if(rule->condition) {
                semantic_check(ctx,rule->condition);
                if(!semantic_is_boolean_condition(ctx,rule->condition))
                    report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "Rule trigger condition must have bool type.");
            }
            /* A bare identifier is an event label, not a variable reference. */
            if(rule->action && rule->action->type!=AST_IDENTIFIER)
                semantic_check(ctx,rule->action);
            break;
        }

        case AST_RULE_ALWAYS:
            semantic_check(ctx,((ASTRuleAlways*)node)->action);
            break;

        case AST_RULE_DEFAULT:
            semantic_check(ctx,((ASTRuleDefault*)node)->action);
            break;

        case AST_APPLY_RULES: {
            ASTApplyRules* apply=(ASTApplyRules*)node;
            Slot* slot=find_slot(ctx,apply->ruleset_name?apply->ruleset_name:"");
            semantic_check_rule_arguments(ctx,node,apply->ruleset_name,
                apply->args,slot);
            break;
        }

        case AST_ON_BLOCK: {
            ASTOnBlock* handler=(ASTOnBlock*)node;
            Slot* ruleset=find_slot(ctx,handler->ruleset_name?handler->ruleset_name:"");
            if(!ruleset || ruleset->kind!=SLOT_FUNC) {
                char message[256];
                snprintf(message,sizeof(message),"Unknown ruleset '%s' in on-block.",
                    handler->ruleset_name?handler->ruleset_name:"");
                report_error(ctx,node,ERR_SEMANTIC_UNDEFINED,message);
            } else if(!semantic_rule_has_event(ruleset,handler->event_name)) {
                char message[256];
                snprintf(message,sizeof(message),"Unknown event '%s' for ruleset '%s'.",
                    handler->event_name?handler->event_name:"",
                    handler->ruleset_name?handler->ruleset_name:"");
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
            if ((!function_name || !*function_name) &&
                ctx->current_method_context)
                function_name = ctx->current_method_public_name;
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
            ctx->current_func_return_type = func->return_type;
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
                    ASTVarDecl* parameter = (ASTVarDecl*)parameter_node;
                    Slot* parameter_slot = find_slot(ctx, parameter->name);
                    uint32_t parameter_node_id =
                        semantic_node_id(ctx, parameter_node);
                    SemanticResultStatus status = semantic_result_add_relation(
                        &ctx->result,
                        function_slot->symbol_id,
                        SEMANTIC_RESULT_RELATION_FUNCTION_PARAMETER,
                        ordinal++,
                        parameter_slot ? parameter_slot->symbol_id : 0u,
                        parameter_node_id,
                        parameter->name,
                        parameter_slot ? parameter_slot->type_id :
                            semantic_declared_type_id(ctx, parameter));
                    semantic_set_result_error(ctx, status);
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
                if (func->body) semantic_check(ctx, func->body);
                ctx->current_method_context = method_context_for_function;
            }
            exit_scope(ctx);
            ctx->current_func_return_type = prev_return_type;
            ctx->loop_depth = prev_loop_depth;
            break;
        }

        case AST_IF: {
            ASTIf* conditional = (ASTIf*)node;
            if (conditional->condition) {
                semantic_check(ctx, conditional->condition);
                if (!semantic_is_boolean_condition(ctx, conditional->condition)) {
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "if/elsif condition must have bool type.");
                }
            }
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

        case AST_RETURN: {
            ASTUnaryExpr* ret = (ASTUnaryExpr*)node;
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
            semantic_check(ctx, unary->operand);
            break;
        }

        case AST_POSTFIX_EXPR: {
            ASTUnaryExpr* postfix = (ASTUnaryExpr*)node;
            const char* operand_type = NULL;
            if (postfix->operand) {
                semantic_check(ctx, postfix->operand);
                operand_type = get_expr_type(ctx, postfix->operand);
            }
            if (!postfix->operand || postfix->operand->type != AST_IDENTIFIER) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Postfix increment/decrement currently requires a variable identifier.");
            } else {
                Slot* operand_slot = find_slot(ctx,
                    ((ASTIdentifier*)postfix->operand)->name);
                if (!operand_slot) {
                    /* Identifier resolution already emitted the undefined-name diagnostic. */
                } else if (operand_slot->is_const) {
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Postfix increment/decrement cannot modify a constant.");
                } else if (!operand_type || !is_numeric_primitive_type(operand_type)) {
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Postfix increment/decrement requires a numeric variable.");
                }
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

            if (!struct_slot || struct_slot->kind != SLOT_STRUCT) {
                char message[256];
                snprintf(message, sizeof(message),
                    "Struct literal type '%s' is not a visible Struct type.",
                    literal->type_name ? literal->type_name : "");
                report_error(ctx, node, ERR_SEMANTIC_UNKNOWN_TYPE, message);
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
            report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                "Tuple values are not enabled until tuple typing is implemented.");
            break;

        case AST_ASM_BLOCK:
            report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                "Inline assembly blocks are not enabled in the semantic pipeline.");
            break;

        case AST_FUNC_CALL: {
            ASTCall* call = (ASTCall*)node;
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

                        if (v->type_name && !valid_type) {
                            char buf[256];
                            snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_UNKNOWN_TYPE), v->type_name);
                            report_error(ctx, (ASTNode*)v, ERR_SEMANTIC_UNKNOWN_TYPE, buf);
                        }
                        
                        if (v->inline_type) {
                            semantic_check(ctx, v->inline_type);
                        }

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

                    if (v->type_name && !valid_type) {
                        char buf[256];
                        snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_UNKNOWN_TYPE), v->type_name);
                        report_error(ctx, (ASTNode*)v, ERR_SEMANTIC_UNKNOWN_TYPE, buf);
                    }
                    
                    if (v->inline_type) {
                        semantic_check(ctx, v->inline_type);
                    }

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

                            if (fdecl->type_name && !valid_type) {
                                char buf[256];
                                snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_UNKNOWN_TYPE), fdecl->type_name);
                                report_error(ctx, (ASTNode*)fdecl, ERR_SEMANTIC_UNKNOWN_TYPE, buf);
                            }
                            if (fdecl->inline_type) {
                                semantic_check(ctx, fdecl->inline_type);
                            }
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
            if (find_slot(ctx, nt->name)) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Type '%s' already defined.", nt->name);
                report_error(ctx, node, ERR_SEMANTIC_REDEFINED, buf);
            } else {
                Slot* newtype_slot = add_slot(
                    ctx, nt->name, SLOT_NEWTYPE, 1, nt->target_type, node, 0, 0, 0, 0u);
                semantic_define_named_type(
                    ctx, newtype_slot, nt->name, nt->target_type);
                semantic_record_annotation(
                    ctx, node, newtype_slot, nt->name,
                    SEMANTIC_RESULT_VALUE_UNSPECIFIED);
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
                    // Range Loop Check for 'it'
                    if (loop->condition->type == AST_BINARY_EXPR) {
                        ASTBinaryExpr* bin = (ASTBinaryExpr*)loop->condition;
                        if (bin->op == TOKEN_RANGE) {
                             if (!find_slot(ctx, "it")) {
                                add_slot(ctx, "it", SLOT_VAR, 0, "i32", node, 1, 0, 0, 0u);
                            }
                        }
                    }
                    semantic_check(ctx, loop->condition);
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
                    snprintf(message, sizeof(message),
                        msg_get(ERR_SEMANTIC_UNKNOWN_TYPE), target_name);
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
        case AST_FREE:
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