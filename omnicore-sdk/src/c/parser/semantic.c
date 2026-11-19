#include "semantic.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>
#include "lexer.h"
#include "../semantic/semantic_import_environment.h"

static int semantic_slot_is_nominal_type(const Slot* slot);
static void semantic_validate_intr_intrinsic(SemanticContext* ctx, ASTNode* node);

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
    ctx->expression_type_names = NULL;
    ctx->thread_blocks = NULL;
    ctx->thread_block_tail = NULL;
    ctx->active_thread_block = NULL;
    ctx->thread_capture_copy_depth = 0u;
    ctx->thread_block_depth = 0u;
    ctx->borrow_operand_depth = 0u;
    ctx->assignment_target_depth = 0u;
    ctx->assignment_borrow_guard_owner = NULL;
    memset(&ctx->lang, 0, sizeof(ctx->lang));
    ctx->current_method_receiver_type = NULL;
    ctx->current_method_public_name = NULL;
    ctx->current_method_receiver_type_id = 0u;
    ctx->current_method_context = 0u;
    ctx->current_method_is_object = 0u;
    ctx->suppress_group_default_dispatch = 0u;
    ctx->node_id_resolver = NULL;
    ctx->import_environment = NULL;
    ctx->node_id_context = NULL;
    ctx->result_enabled = 0u;
    ctx->result_status = semantic_result_init(&ctx->result);
    ctx->test_allocation_fail_at = 0u;
    ctx->test_allocation_attempts = 0u;
}

void semantic_attach_import_environment(
    SemanticContext* ctx,
    const struct OccSemanticImportEnvironment* environment
) {
    if (!ctx) return;
    /* The caller owns the immutable environment and its backing OIR views. */
    ctx->import_environment = environment;
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

static const char* semantic_intern_expression_type(
    SemanticContext* ctx, const char* prefix, const char* target_type
) {
    size_t prefix_length;
    size_t target_length;
    size_t length;
    SemanticTypeName* entry;
    char* name;
    if (!ctx || !prefix || !target_type) return NULL;
    if (target_type[0] == '!') ++target_type;
    prefix_length = strlen(prefix);
    target_length = strlen(target_type);
    if (target_length > SIZE_MAX - prefix_length - 1u) return NULL;
    length = prefix_length + target_length;
    for (entry = ctx->expression_type_names; entry; entry = entry->next) {
        if (strlen(entry->name) == length &&
            memcmp(entry->name, prefix, prefix_length) == 0 &&
            memcmp(entry->name + prefix_length, target_type, target_length) == 0)
            return entry->name;
    }
    entry = (SemanticTypeName*)semantic_allocate(ctx, sizeof(*entry));
    if (!entry) {
        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
        return NULL;
    }
    name = (char*)semantic_allocate(ctx, length + 1u);
    if (!name) {
        free(entry);
        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
        return NULL;
    }
    memcpy(name, prefix, prefix_length);
    memcpy(name + prefix_length, target_type, target_length + 1u);
    entry->name = name;
    entry->next = ctx->expression_type_names;
    ctx->expression_type_names = entry;
    return name;
}

static const char* semantic_intern_full_expression_type(
    SemanticContext* ctx, const char* value
) {
    SemanticTypeName* entry;
    size_t length;
    if (!ctx || !value) return NULL;
    length = strlen(value);
    for (entry = ctx->expression_type_names; entry; entry = entry->next) {
        if (entry->name && strlen(entry->name) == length &&
            memcmp(entry->name, value, length) == 0) return entry->name;
    }
    entry = (SemanticTypeName*)semantic_allocate(ctx, sizeof(*entry));
    if (!entry) {
        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
        return NULL;
    }
    entry->name = (char*)semantic_allocate(ctx, length + 1u);
    if (!entry->name) {
        free(entry);
        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
        return NULL;
    }
    memcpy(entry->name, value, length + 1u);
    entry->next = ctx->expression_type_names;
    ctx->expression_type_names = entry;
    return entry->name;
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

static int semantic_borrow_paths_overlap(const char* left, const char* right);

/* Borrow diagnostics identify both the rejected operation and the binding
   that keeps the conflicting borrow alive. The node span points at the
   rejected expression; the hint explains the least surprising repair. */
static const Slot* semantic_find_active_borrower(
    const SemanticContext* ctx, const Slot* owner, const char* requested_path
) {
    const Slot* current;
    if (!ctx || !owner) return NULL;
    for (current = ctx->head; current; current = current->next) {
        if (current->borrowed_from == owner && current->borrow_kind != 0u &&
            semantic_borrow_paths_overlap(requested_path, current->borrow_path))
            return current;
    }
    return NULL;
}

static void report_borrow_conflict(
    SemanticContext* ctx, ASTNode* node, Slot* owner, int requested_mutable,
    const char* requested_path, const char* operation
) {
    const Slot* active = semantic_find_active_borrower(
        ctx, owner, requested_path);
    const char* active_name = active && active->name
        ? active->name : "<temporary call argument>";
    const char* active_kind = active && active->borrow_kind == 2u
        ? "exclusive mutable" : "shared";
    const char* active_path = active && active->borrow_path &&
        active->borrow_path[0] ? active->borrow_path : "<whole value>";
    const char* requested = requested_path && requested_path[0]
        ? requested_path : "<whole value>";
    char message[640];
    char suggestion[512];
    const char* owner_name = owner && owner->name ? owner->name : "<unknown>";
    if (requested_mutable) {
        (void)snprintf(message, sizeof(message),
            "Cannot %s '%s' at path '%s': %s borrow '%s' at path '%s' "
            "is still active; the two projections overlap.",
            operation ? operation : "borrow", owner_name, requested,
            active_kind, active_name, active_path);
        (void)snprintf(suggestion, sizeof(suggestion),
            "End the scope of reference '%s' (or reassign it to a different "
            "valid owner) before borrowing this overlapping path mutably. "
            "Disjoint struct fields and distinct constant array indexes may "
            "be borrowed independently; dynamic indexes and unknown pointer "
            "provenance are treated conservatively.",
            active_name);
    } else {
        (void)snprintf(message, sizeof(message),
            "Cannot %s '%s' at path '%s': exclusive mutable borrow '%s' "
            "at path '%s' is still active; shared access would overlap it.",
            operation ? operation : "borrow", owner_name, requested,
            active_name, active_path);
        (void)snprintf(suggestion, sizeof(suggestion),
            "Use reference '%s' for this access, or end its scope before "
            "accessing the overlapping path '%s'.",
            active_name, requested);
    }
    report_issue(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
        message, suggestion, suggestion, 0, 0);
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
        "u16", "i16", "i8", "i128", "u128", "f32", "f128", "d32", "d64", "d128", "char", "bit", "byte",
        "8bit", "16bit", "32bit", "64bit", "128bit",
        "1byte", "2byte", "4byte", "8byte", "16byte", "Vec2", "Vec3", "Vec4", "void", "any"
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

    if (strcmp(name, "array") == 0 || strcmp(name, "map") == 0 ||
        strcmp(name, "Map") == 0) {
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
        if (unary->op == TOKEN_MINUS && unary->operand &&
            unary->operand->type == AST_LITERAL) {
            uint64_t magnitude = 0u;
            int overflow = 0;
            if (semantic_parse_unsigned_integer_literal(
                    (const ASTLiteral*)unary->operand,
                    &magnitude, &overflow) && !overflow) {
                if (magnitude == (uint64_t)INT64_MAX + 1u) {
                    *value_out = INT64_MIN;
                    return 1;
                }
                if (magnitude <= (uint64_t)INT64_MAX) {
                    *value_out = -(int64_t)magnitude;
                    return 1;
                }
                return 0;
            }
        }
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


static int is_numeric_primitive_type(const char* type_name);

static int semantic_is_map_key_type_depth(
    SemanticContext* ctx, const char* type_name, unsigned int depth
) {
    Slot* type_slot;
    if (!type_name || depth > 32u) return 0;
    if (*type_name == '!') ++type_name;
    if (is_numeric_primitive_type(type_name) ||
        strcmp(type_name, "str") == 0 ||
        strcmp(type_name, "char") == 0 ||
        strcmp(type_name, "bool") == 0) return 1;
    type_slot = find_slot(ctx, type_name);
    if (type_slot && type_slot->kind == SLOT_ENUM) return 1;
    if (type_slot && type_slot->kind == SLOT_NEWTYPE &&
        type_slot->type_name) {
        return semantic_is_map_key_type_depth(
            ctx, type_slot->type_name, depth + 1u);
    }
    return 0;
}

static int semantic_is_map_key_type(
    SemanticContext* ctx, const char* type_name
) {
    return semantic_is_map_key_type_depth(ctx, type_name, 0u);
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
    {
        uint16_t annotation_flags = 0u;
        if (ctx->current_method_is_object && slot &&
            slot->name && strcmp(slot->name, "self") == 0)
            annotation_flags |= SEMANTIC_RESULT_ANNOTATION_FLAG_ADDRESS_RECEIVER;
        if (ctx->active_thread_block && slot &&
            slot->kind == SLOT_VAR &&
            slot->depth <= (int)ctx->active_thread_block->capture_base_depth &&
            ctx->thread_capture_copy_depth != 0u)
            annotation_flags |= SEMANTIC_RESULT_ANNOTATION_FLAG_THREAD_CAPTURE;
        status = semantic_result_set_annotation(
            &ctx->result,
            node_id,
            ctx->current_scope_id,
            slot ? slot->symbol_id : 0u,
            type_id,
            value_category,
            annotation_flags);
    }
    semantic_set_result_error(ctx, status);
}

static void semantic_release_borrow_binding(Slot* borrower) {
    Slot* owner;
    if (!borrower) return;
    owner = borrower->borrowed_from;
    if (!owner) {
        free(borrower->borrow_path);
        borrower->borrow_path = NULL;
        borrower->borrow_kind = 0u;
        borrower->borrow_origin = NULL;
        return;
    }
    if (borrower->borrow_kind == 1u) {
        if (owner->shared_borrow_count > 0u)
            --owner->shared_borrow_count;
    } else if (borrower->borrow_kind == 2u) {
        owner->mutable_borrow_active = 0u;
    }
    borrower->borrowed_from = NULL;
    borrower->borrow_origin = NULL;
    borrower->borrow_kind = 0u;
    free(borrower->borrow_path);
    borrower->borrow_path = NULL;
}

static void destroy_slot(Slot* slot) {
    if (!slot) return;
    semantic_release_borrow_binding(slot);

    free(slot->name);
    free(slot->type_name);
    free(slot->borrow_path);

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

    /* Release every borrow in this scope before freeing any owner Slot.
       Declaration order must not affect cleanup safety. */
    {
        Slot* current;
        for (current = ctx->head; current; current = current->next) {
            if (current->depth == ctx->current_depth)
                semantic_release_borrow_binding(current);
        }
    }

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

static int are_types_compatible(SemanticContext* ctx, const char* type1, const char* type2);

static const ASTTypeExpr* semantic_callback_type_from_slot(
    SemanticContext* ctx, const Slot* slot
);
static const char* semantic_callback_return_type(
    SemanticContext* ctx, const Slot* slot
);
static const char* get_expr_type(SemanticContext* ctx, ASTNode* node);
static int semantic_type_pattern_contains_variable(const char* pattern);
static int semantic_unify_type_pattern(
    SemanticContext* ctx, const char* pattern, const char* actual,
    const char** inferred_type
);
static const char* semantic_substitute_type_variable(
    SemanticContext* ctx, const char* pattern, const char* inferred_type
);
static ASTNode* semantic_find_type_variable_argument(
    Slot* function_slot, ASTNode* arguments, uint32_t parameter_offset,
    const char** out_pattern
);
static Slot* semantic_resolve_group_callable(
    SemanticContext* ctx, ASTMemberAccess* access,
    int* out_is_instance_call, int report_diagnostics);
static const char* semantic_resolve_imported_call(
    SemanticContext* ctx, ASTNode* call_node, int validate_arguments,
    int* out_handled
);

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

/* A named function may be stored in a callback binding only when its full
   callable contract matches. Parameter names are part of the callback API
   because call sites may use named arguments. */
static int semantic_function_matches_callback_type(
    SemanticContext* ctx, const Slot* callback_slot,
    const Slot* function_slot
) {
    const ASTTypeExpr* callback_type;
    const ASTFuncDecl* function;
    const ASTNode* callback_parameter;
    const ASTNode* function_parameter;
    const char* callback_return;
    const char* function_return;
    if (!ctx || !callback_slot || !function_slot ||
        function_slot->kind != SLOT_FUNC ||
        !function_slot->declaration_node ||
        (function_slot->declaration_node->type != AST_FUNC_DECL &&
         function_slot->declaration_node->type != AST_EXTERN_DECL))
        return 0;
    callback_type = semantic_callback_type_from_slot(ctx, callback_slot);
    if (!callback_type) return 0;
    function = (const ASTFuncDecl*)function_slot->declaration_node;
    callback_return = semantic_callback_return_type(ctx, callback_slot);
    function_return = function->return_type ? function->return_type : "void";
    if (!callback_return || !function_return ||
        !are_types_compatible(ctx, callback_return, function_return) ||
        !are_types_compatible(ctx, function_return, callback_return))
        return 0;
    callback_parameter = callback_type->params;
    function_parameter = function->params;
    while (callback_parameter && function_parameter) {
        if (callback_parameter->type == AST_VARIADIC_PARAM ||
            function_parameter->type == AST_VARIADIC_PARAM) {
            if (callback_parameter->type != AST_VARIADIC_PARAM ||
                function_parameter->type != AST_VARIADIC_PARAM ||
                callback_parameter->next || function_parameter->next)
                return 0;
            callback_parameter = callback_parameter->next;
            function_parameter = function_parameter->next;
            break;
        }
        if (callback_parameter->type != AST_VAR_DECL ||
            function_parameter->type != AST_VAR_DECL) return 0;
        {
            const ASTVarDecl* expected =
                (const ASTVarDecl*)callback_parameter;
            const ASTVarDecl* actual =
                (const ASTVarDecl*)function_parameter;
            if (!expected->name || !actual->name ||
                strcmp(expected->name, actual->name) != 0 ||
                !expected->type_name || !actual->type_name ||
                !are_types_compatible(ctx, expected->type_name, actual->type_name) ||
                !are_types_compatible(ctx, actual->type_name, expected->type_name) ||
                (!!expected->initializer != !!actual->initializer))
                return 0;
        }
        callback_parameter = callback_parameter->next;
        function_parameter = function_parameter->next;
    }
    return callback_parameter == NULL && function_parameter == NULL;
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
    if (node->type == AST_UNARY_EXPR) {
        ASTUnaryExpr* unary = (ASTUnaryExpr*)node;
        const char* operand_type;
        const char* raw_type;
        if (unary->op != TOKEN_STAR || !unary->operand) return 0;
        operand_type = get_expr_type(ctx, unary->operand);
        raw_type = operand_type && operand_type[0] == '!'
            ? operand_type + 1 : operand_type;
        return raw_type && raw_type[0] == '*';
    }
    return 0;
}

/* Build a stable projection path for a statically identifiable lvalue.
   Empty string denotes the whole root; dynamic indexes use [*]. */
static char* semantic_lvalue_path(SemanticContext* ctx, const ASTNode* node) {
    char* parent;
    char index_suffix[48];
    const char* suffix;
    size_t parent_length, suffix_length;
    char* result;
    if (!node) return NULL;
    if (node->type == AST_IDENTIFIER) {
        result = (char*)malloc(1u);
        if (result) result[0] = '\0';
        return result;
    }
    if (node->type == AST_MEMBER_ACCESS) {
        const ASTMemberAccess* member = (const ASTMemberAccess*)node;
        size_t member_length;
        char* member_suffix;
        if (!member->member_name || !member->member_name[0]) return NULL;
        parent = semantic_lvalue_path(ctx, member->object);
        if (!parent) return NULL;
        {
            const char* object_type = get_expr_type(ctx, member->object);
            const char* aggregate_name = object_type && object_type[0] == '!'
                ? object_type + 1 : object_type;
            Slot* aggregate = aggregate_name
                ? find_slot(ctx, aggregate_name) : NULL;
            int is_union_projection = aggregate &&
                aggregate->kind == SLOT_UNION;
            member_length = is_union_projection ? 1u
                : strlen(member->member_name);
            if (member_length > SIZE_MAX - 2u) {
                free(parent);
                return NULL;
            }
            member_suffix = (char*)malloc(member_length + 2u);
            if (!member_suffix) {
                free(parent);
                return NULL;
            }
            member_suffix[0] = '.';
            if (is_union_projection) {
                member_suffix[1] = '*';
                member_suffix[2] = '\0';
            } else {
                memcpy(member_suffix + 1u, member->member_name,
                    member_length + 1u);
            }
            suffix = member_suffix;
        }
    } else if (node->type == AST_INDEX_ACCESS) {
        const ASTIndexAccess* index = (const ASTIndexAccess*)node;
        int64_t constant_index;
        parent = semantic_lvalue_path(ctx, index->target);
        if (!parent) return NULL;
        if (semantic_eval_integer_constant(index->index, &constant_index))
            (void)snprintf(index_suffix, sizeof(index_suffix), "[%lld]",
                (long long)constant_index);
        else
            (void)snprintf(index_suffix, sizeof(index_suffix), "[*]");
        suffix = index_suffix;
    } else {
        /* Pointer dereferences and computed expressions have unknown provenance. */
        return NULL;
    }
    parent_length = strlen(parent);
    suffix_length = strlen(suffix);
    if (parent_length > SIZE_MAX - suffix_length - 1u) {
        free(parent);
        if (node->type == AST_MEMBER_ACCESS) free((void*)suffix);
        return NULL;
    }
    result = (char*)malloc(parent_length + suffix_length + 1u);
    if (result) {
        memcpy(result, parent, parent_length);
        memcpy(result + parent_length, suffix, suffix_length + 1u);
    }
    free(parent);
    if (node->type == AST_MEMBER_ACCESS) free((void*)suffix);
    return result;
}

/* Resolve the containing storage owner for identifier/member/index lvalues. */
static Slot* semantic_lvalue_root_slot(SemanticContext* ctx, ASTNode* node) {
    if (!ctx || !node) return NULL;
    switch (node->type) {
        case AST_IDENTIFIER:
            return find_slot(ctx, ((ASTIdentifier*)node)->name);
        case AST_MEMBER_ACCESS:
            return semantic_lvalue_root_slot(
                ctx, ((ASTMemberAccess*)node)->object);
        case AST_INDEX_ACCESS:
            return semantic_lvalue_root_slot(
                ctx, ((ASTIndexAccess*)node)->target);
        default:
            /* Computed/raw-pointer lvalues do not prove a safe owner. */
            return NULL;
    }
}

/* Projection paths are tokenized as member (.name) and index ([N]) steps.
   A wildcard index overlaps every index; a common prefix overlaps descendants. */
static int semantic_borrow_paths_overlap(const char* left, const char* right) {
    size_t li = 0u, ri = 0u;
    if (!left || !right) return 1; /* Unknown provenance is conservative. */
    while (left[li] && right[ri]) {
        size_t lend = li, rend = ri;
        int left_index, right_index;
        if (left[li] == '.') {
            ++lend;
            while (left[lend] && left[lend] != '.' && left[lend] != '[') ++lend;
        } else if (left[li] == '[') {
            ++lend;
            while (left[lend] && left[lend] != ']') ++lend;
            if (left[lend] == ']') ++lend;
        } else return 1;
        if (right[ri] == '.') {
            ++rend;
            while (right[rend] && right[rend] != '.' && right[rend] != '[') ++rend;
        } else if (right[ri] == '[') {
            ++rend;
            while (right[rend] && right[rend] != ']') ++rend;
            if (right[rend] == ']') ++rend;
        } else return 1;
        left_index = left[li] == '[';
        right_index = right[ri] == '[';
        if (left_index != right_index) return 0;
        if (left_index) {
            size_t llen = lend - li, rlen = rend - ri;
            int left_wild = llen == 3u && left[li + 1u] == '*';
            int right_wild = rlen == 3u && right[ri + 1u] == '*';
            if (!left_wild && !right_wild &&
                (llen != rlen || memcmp(left + li, right + ri, llen) != 0))
                return 0;
        } else {
            size_t llen = lend - li, rlen = rend - ri;
            if (llen != rlen || memcmp(left + li, right + ri, llen) != 0)
                return 0;
        }
        li = lend;
        ri = rend;
    }
    /* A root or ancestor path overlaps all descendants. */
    return left[li] == '\0' || right[ri] == '\0';
}

static int semantic_borrow_conflicts(
    const SemanticContext* ctx, const Slot* owner, const char* path,
    int requested_mutable, const Slot* excluded
) {
    const Slot* current;
    if (!ctx || !owner) return 0;
    for (current = ctx->head; current; current = current->next) {
        if (current == excluded || current->borrowed_from != owner ||
            current->borrow_kind == 0u) continue;
        if (semantic_borrow_paths_overlap(
                path, current->borrow_path) &&
            (requested_mutable || current->borrow_kind == 2u))
            return 1;
    }
    return 0;
}

static uint32_t semantic_shared_borrows_on_path(
    const SemanticContext* ctx, const Slot* owner, const char* path,
    const Slot* excluded
) {
    const Slot* current;
    uint32_t count = 0u;
    if (!ctx || !owner) return 0u;
    for (current = ctx->head; current; current = current->next) {
        if (current == excluded || current->borrowed_from != owner ||
            current->borrow_kind != 1u) continue;
        if (semantic_borrow_paths_overlap(path, current->borrow_path) &&
            count < UINT32_MAX) ++count;
    }
    return count;
}

static int semantic_is_reference_type_name(const char* type_name) {
    if (!type_name) return 0;
    if (*type_name == '!') ++type_name;
    return type_name[0] == '&' && type_name[1] != '\0';
}

static int semantic_is_mut_reference_type_name(const char* type_name) {
    if (!type_name) return 0;
    if (*type_name == '!') ++type_name;
    return strncmp(type_name, "&mut", 4u) == 0;
}

static int semantic_is_raw_pointer_type_name(const char* type_name) {
    if (!type_name) return 0;
    while (*type_name == '!') ++type_name;
    return type_name[0] == '*' && type_name[1] != '\0';
}

static const char* semantic_function_type_text(
    SemanticContext* ctx, const char* type_name
) {
    Slot* alias;
    while (type_name && *type_name == '!') ++type_name;
    if (!type_name || !*type_name) return NULL;
    if (strncmp(type_name, "f:(", 3u) == 0) return type_name;
    alias = find_slot(ctx, type_name);
    if (alias && alias->kind == SLOT_NEWTYPE) {
        const char* target = alias->type_name;
        while (target && *target == '!') ++target;
        if (target && strncmp(target, "f:(", 3u) == 0) return target;
    }
    return NULL;
}

static int semantic_is_function_type_name(
    SemanticContext* ctx, const char* type_name
) {
    return semantic_function_type_text(ctx, type_name) != NULL;
}

static int semantic_expression_may_be_null(
    SemanticContext* ctx, const ASTNode* expression
) {
    if (!expression) return 0;
    if (expression->type == AST_LITERAL)
        return ((const ASTLiteral*)expression)->type == TOKEN_NULL;
    if (expression->type == AST_IDENTIFIER) {
        Slot* source = find_slot(ctx, ((const ASTIdentifier*)expression)->name);
        return source && source->is_nullable;
    }
    if (expression->type == AST_FUNC_CALL) {
        const char* result_type = get_expr_type(ctx, (ASTNode*)expression);
        return semantic_is_raw_pointer_type_name(result_type);
    }
    return 0;
}

/* Aggregate-contained safe references need field-level owner/lifetime metadata.
   Until that representation exists, reject direct references and aggregates
   (including NewType aliases) that recursively contain them. */
static int semantic_type_contains_safe_reference(
    SemanticContext* ctx, const char* type_name, unsigned int depth
) {
    const char* raw_type = type_name;
    Slot* type_slot;
    FieldInfo* field;
    if (!ctx || !raw_type) return 0;
    if (depth > 64u) return 1; /* Cyclic/deep type graphs fail closed. */
    while (*raw_type == '!') ++raw_type;
    if (semantic_is_reference_type_name(raw_type)) return 1;
    if (strncmp(raw_type, "f:", 2u) == 0)
        return 0; /* A function type describes parameter/return contracts; it does not store those references. */
    if (strncmp(raw_type, "array|", 6u) == 0) {
        const char* element_start = raw_type + 6u;
        size_t element_length = strcspn(element_start, "[");
        char element_type[256];
        if (element_length == 0u) return 0;
        if (element_length >= sizeof(element_type)) return 1;
        memcpy(element_type, element_start, element_length);
        element_type[element_length] = '\0';
        return semantic_type_contains_safe_reference(
            ctx, element_type, depth + 1u);
    }
    type_slot = find_slot(ctx, raw_type);
    if (!type_slot) return 0;
    if (type_slot->kind == SLOT_NEWTYPE)
        return semantic_type_contains_safe_reference(
            ctx, type_slot->type_name, depth + 1u);
    if (type_slot->kind != SLOT_STRUCT && type_slot->kind != SLOT_UNION) return 0;
    for (field = type_slot->fields; field; field = field->next) {
        if (semantic_type_contains_safe_reference(
                ctx, field->type_name, depth + 1u)) return 1;
    }
    return 0;
}


/* A callback signature is not reference storage, but exposing that
   signature through an untyped/extern/variadic boundary erases its lifetime
   contract. Keep that ABI rule separate from aggregate-storage inspection. */
static int semantic_type_or_callback_contract_contains_safe_reference(
    SemanticContext* ctx, const char* type_name, unsigned int depth
) {
    const char* raw_type = type_name;
    Slot* type_slot;
    FieldInfo* field;
    if (!ctx || !raw_type) return 0;
    if (depth > 64u) return 1;
    while (*raw_type == '!') ++raw_type;
    if (semantic_is_reference_type_name(raw_type)) return 1;
    if (strncmp(raw_type, "f:", 2u) == 0)
        return strchr(raw_type, '&') != NULL;
    if (strncmp(raw_type, "array|", 6u) == 0) {
        const char* element_start = raw_type + 6u;
        size_t element_length = strcspn(element_start, "[");
        char element_type[256];
        if (element_length == 0u) return 0;
        if (element_length >= sizeof(element_type)) return 1;
        memcpy(element_type, element_start, element_length);
        element_type[element_length] = '\0';
        return semantic_type_or_callback_contract_contains_safe_reference(
            ctx, element_type, depth + 1u);
    }
    type_slot = find_slot(ctx, raw_type);
    if (!type_slot) return 0;
    if (type_slot->kind == SLOT_NEWTYPE)
        return semantic_type_or_callback_contract_contains_safe_reference(
            ctx, type_slot->type_name, depth + 1u);
    if (type_slot->kind != SLOT_STRUCT && type_slot->kind != SLOT_UNION)
        return 0;
    for (field = type_slot->fields; field; field = field->next) {
        if (semantic_type_or_callback_contract_contains_safe_reference(
                ctx, field->type_name, depth + 1u))
            return 1;
    }
    return 0;
}

static int semantic_type_is_any_depth(
    SemanticContext* ctx, const char* type_name, unsigned int depth
) {
    Slot* slot;
    if (!type_name || depth > 32u) return 0;
    while (*type_name == '!') ++type_name;
    if (strcmp(type_name, "any") == 0) return 1;
    slot = find_slot(ctx, type_name);
    if (slot && slot->kind == SLOT_NEWTYPE && slot->type_name)
        return semantic_type_is_any_depth(ctx, slot->type_name, depth + 1u);
    return 0;
}

static int semantic_type_is_any(
    SemanticContext* ctx, const char* type_name
) {
    return semantic_type_is_any_depth(ctx, type_name, 0u);
}

typedef struct SemanticReferenceInitEntry {
    Slot* slot;
    uint8_t is_initialized;
    uint8_t is_nullable;
    ASTNode* callable_declaration;
} SemanticReferenceInitEntry;

typedef struct SemanticReferenceInitSnapshot {
    SemanticReferenceInitEntry* entries;
    size_t count;
} SemanticReferenceInitSnapshot;

/* Safe-reference bindings are usable only after a valid initializer or
   assignment. Branch snapshots implement a conservative must-initialize
   merge: a binding is initialized after an if only when every path is. */
static int semantic_reference_init_snapshot_capture(
    SemanticContext* ctx, SemanticReferenceInitSnapshot* snapshot
) {
    Slot* slot;
    size_t count = 0u;
    size_t index = 0u;
    if (!ctx || !snapshot) return 0;
    snapshot->entries = NULL;
    snapshot->count = 0u;
    for (slot = ctx->head; slot; slot = slot->next) {
        if (slot->kind == SLOT_VAR &&
            (semantic_is_reference_type_name(slot->type_name) ||
             semantic_is_raw_pointer_type_name(slot->type_name) ||
             semantic_is_function_type_name(ctx, slot->type_name))) {
            if (count == SIZE_MAX) {
                semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
                return 0;
            }
            ++count;
        }
    }
    if (count == 0u) return 1;
    if (count > SIZE_MAX / sizeof(*snapshot->entries)) {
        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
        return 0;
    }
    snapshot->entries = (SemanticReferenceInitEntry*)semantic_callocate(
        ctx, count, sizeof(*snapshot->entries));
    if (!snapshot->entries) {
        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
        return 0;
    }
    snapshot->count = count;
    for (slot = ctx->head; slot; slot = slot->next) {
        if (slot->kind == SLOT_VAR &&
            (semantic_is_reference_type_name(slot->type_name) ||
             semantic_is_raw_pointer_type_name(slot->type_name) ||
             semantic_is_function_type_name(ctx, slot->type_name))) {
            snapshot->entries[index].slot = slot;
            snapshot->entries[index].is_initialized = slot->is_initialized;
            snapshot->entries[index].is_nullable = slot->is_nullable;
            snapshot->entries[index].callable_declaration =
                slot->callable_declaration;
            ++index;
        }
    }
    return 1;
}

static void semantic_reference_init_snapshot_restore(
    const SemanticReferenceInitSnapshot* snapshot
) {
    size_t index;
    if (!snapshot) return;
    for (index = 0u; index < snapshot->count; ++index) {
        if (snapshot->entries[index].slot)
            snapshot->entries[index].slot->is_initialized =
                snapshot->entries[index].is_initialized;
        if (snapshot->entries[index].slot)
            snapshot->entries[index].slot->is_nullable =
                snapshot->entries[index].is_nullable;
        if (snapshot->entries[index].slot)
            snapshot->entries[index].slot->callable_declaration =
                snapshot->entries[index].callable_declaration;
    }
}

static void semantic_reference_init_snapshot_merge(
    SemanticReferenceInitSnapshot* left,
    const SemanticReferenceInitSnapshot* right
) {
    size_t index;
    if (!left || !right || left->count != right->count) return;
    for (index = 0u; index < left->count; ++index) {
        Slot* slot = left->entries[index].slot;
        if (slot && slot == right->entries[index].slot) {
            left->entries[index].is_initialized = (uint8_t)(
                left->entries[index].is_initialized &&
                right->entries[index].is_initialized);
            slot->is_initialized = left->entries[index].is_initialized;
            left->entries[index].is_nullable = (uint8_t)(
                left->entries[index].is_nullable ||
                right->entries[index].is_nullable);
            slot->is_nullable = left->entries[index].is_nullable;
            if (left->entries[index].callable_declaration !=
                right->entries[index].callable_declaration) {
                left->entries[index].callable_declaration = NULL;
                slot->callable_declaration = NULL;
            } else {
                slot->callable_declaration =
                    left->entries[index].callable_declaration;
            }
        }
    }
}

static void semantic_refine_nullable_binding_from_condition(
    SemanticContext* ctx, const ASTNode* condition, int truth_value
) {
    const ASTBinaryExpr* comparison;
    const ASTIdentifier* identifier = NULL;
    const ASTLiteral* literal = NULL;
    Slot* slot;
    int nonnull_when_true;
    int should_be_nonnull;
    if (!ctx || !condition || condition->type != AST_BINARY_EXPR) return;
    comparison = (const ASTBinaryExpr*)condition;
    if (comparison->op != TOKEN_EQ && comparison->op != TOKEN_NEQ) return;
    if (comparison->left && comparison->left->type == AST_IDENTIFIER &&
        comparison->right && comparison->right->type == AST_LITERAL) {
        identifier = (const ASTIdentifier*)comparison->left;
        literal = (const ASTLiteral*)comparison->right;
    } else if (comparison->right && comparison->right->type == AST_IDENTIFIER &&
               comparison->left && comparison->left->type == AST_LITERAL) {
        identifier = (const ASTIdentifier*)comparison->right;
        literal = (const ASTLiteral*)comparison->left;
    }
    if (!identifier || !literal || literal->type != TOKEN_NULL) return;
    slot = find_slot(ctx, identifier->name);
    if (!slot || slot->kind != SLOT_VAR ||
        (!semantic_is_raw_pointer_type_name(slot->type_name) &&
         !semantic_is_function_type_name(ctx, slot->type_name))) return;
    nonnull_when_true = comparison->op == TOKEN_NEQ;
    should_be_nonnull = truth_value ? nonnull_when_true : !nonnull_when_true;
    slot->is_nullable = (uint8_t)!should_be_nonnull;
}


static int semantic_statement_contains_break(const ASTNode* node) {
    const ASTNode* current;
    if (!node) return 0;
    if (node->type == AST_BREAK) return 1;
    /* A nested loop owns its own break statements. */
    if (node->type == AST_LOOP) return 0;
    if (node->type == AST_BLOCK) {
        return semantic_statement_contains_break(
            ((const ASTBlock*)node)->statements);
    }
    if (node->type == AST_IF) {
        const ASTIf* branch = (const ASTIf*)node;
        return semantic_statement_contains_break(branch->then_branch) ||
            semantic_statement_contains_break(branch->else_branch);
    }
    if (node->type == AST_SELECT) {
        for (current = ((const ASTSelect*)node)->arms; current;
             current = current->next) {
            if (current->type == AST_SELECT_ARM &&
                semantic_statement_contains_break(
                    ((const ASTSelectArm*)current)->body)) return 1;
        }
    } else if (node->type == AST_SCAN) {
        for (current = ((const ASTScan*)node)->arms; current;
             current = current->next) {
            if (current->type == AST_SCAN_ARM &&
                semantic_statement_contains_break(
                    ((const ASTScanArm*)current)->body)) return 1;
        }
    }
    return 0;
}

/* True when every path through a statement either returns a value or
   diverges (panic/exit/infinite loop). */
static int semantic_function_statement_terminates(const ASTNode* node) {
    const ASTNode* current;
    if (!node) return 0;
    if (node->type == AST_RETURN || node->type == AST_PANIC ||
        node->type == AST_EXIT) return 1;
    if (node->type == AST_BLOCK) {
        for (current = ((const ASTBlock*)node)->statements; current;
             current = current->next) {
            if (semantic_function_statement_terminates(current)) return 1;
        }
        return 0;
    }
    if (node->type == AST_IF) {
        const ASTIf* branch = (const ASTIf*)node;
        return branch->else_branch &&
            semantic_function_statement_terminates(branch->then_branch) &&
            semantic_function_statement_terminates(branch->else_branch);
    }
    if (node->type == AST_SELECT) {
        const ASTSelect* select = (const ASTSelect*)node;
        int has_default = 0;
        if (!select->arms) return 0;
        for (current = select->arms; current; current = current->next) {
            const ASTSelectArm* arm;
            if (current->type != AST_SELECT_ARM) return 0;
            arm = (const ASTSelectArm*)current;
            if (arm->is_default) has_default = 1;
            if (!semantic_function_statement_terminates(arm->body)) return 0;
        }
        return has_default;
    }
    if (node->type == AST_SCAN) {
        const ASTScan* scan = (const ASTScan*)node;
        int has_else = 0;
        if (!scan->arms) return 0;
        for (current = scan->arms; current; current = current->next) {
            const ASTScanArm* arm;
            if (current->type != AST_SCAN_ARM) return 0;
            arm = (const ASTScanArm*)current;
            if (!arm->pattern) has_else = 1;
            if (!semantic_function_statement_terminates(arm->body)) return 0;
        }
        return has_else;
    }
    if (node->type == AST_LOOP) {
        const ASTLoop* loop = (const ASTLoop*)node;
        return loop->loop_type == 0 &&
            !semantic_statement_contains_break(loop->body);
    }
    return 0;
}

static int semantic_branch_terminates_flow(const ASTNode* branch) {
    const ASTNode* statement;
    if (!branch) return 0;
    if (branch->type == AST_BLOCK) {
        statement = ((const ASTBlock*)branch)->statements;
        if (!statement) return 0;
        while (statement->next) statement = statement->next;
        return semantic_branch_terminates_flow(statement);
    }
    if (branch->type == AST_RETURN ||
        branch->type == AST_BREAK ||
        branch->type == AST_CONTINUE) return 1;
    if (branch->type == AST_IF) {
        const ASTIf* conditional = (const ASTIf*)branch;
        return conditional->else_branch &&
            semantic_branch_terminates_flow(conditional->then_branch) &&
            semantic_branch_terminates_flow(conditional->else_branch);
    }
    return 0;
}

static void semantic_reference_init_snapshot_destroy(
    SemanticReferenceInitSnapshot* snapshot
) {
    if (!snapshot) return;
    free(snapshot->entries);
    snapshot->entries = NULL;
    snapshot->count = 0u;
}

static int semantic_reference_return_has_stable_origin(
    SemanticContext* ctx, ASTNode* operand
) {
    Slot* slot;
    if (!ctx || !operand) return 0;
    if (operand->type == AST_IDENTIFIER) {
        slot = find_slot(ctx, ((ASTIdentifier*)operand)->name);
        if (!slot) return 0;
        if (slot->is_parameter &&
            semantic_is_reference_type_name(slot->type_name)) return 1;
        if (slot->depth == 0) return 1;
        if (slot->borrow_origin &&
            (slot->borrow_origin->is_parameter ||
             slot->borrow_origin->depth == 0)) return 1;
        return slot->borrowed_from && slot->borrowed_from->depth == 0;
    }
    if (operand->type == AST_UNARY_EXPR) {
        ASTUnaryExpr* unary = (ASTUnaryExpr*)operand;
        Slot* owner;
        if (unary->op != TOKEN_BIT_AND &&
            unary->op != TOKEN_BORROW_MUT) return 0;
        owner = semantic_lvalue_root_slot(ctx, unary->operand);
        return owner && owner->depth == 0;
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
static int semantic_vector_dimension(const char* type_name, size_t* dimension_out);
static const char* semantic_dereference_target_type(
    const char* type_name, int* is_shared_reference);

static const char* get_expr_type(SemanticContext* ctx, ASTNode* node) {
    if (!node) return NULL;
    switch (node->type) {
        case AST_LITERAL: {
            ASTLiteral* lit = (ASTLiteral*)node;
            const char* type_name = NULL;
            if (!lit->string_value) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Literal AST is missing its string value.");
                return NULL;
            }
            switch (lit->type) {
                case TOKEN_NUMBER:
                    type_name = (strchr(lit->string_value, '.') ||
                        strchr(lit->string_value, 'E') ||
                        strchr(lit->string_value, 'e')) ? "f64" : "i32";
                    break;
                case TOKEN_STRING: type_name = "str"; break;
                case TOKEN_CHAR: type_name = "char"; break;
                case TOKEN_NULL: type_name = "null"; break;
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
            if (call->callee && call->callee->type == AST_MEMBER_ACCESS) {
                const char* imported_result =
                    semantic_resolve_imported_call(ctx, node, 0, NULL);
                if (imported_result) {
                    semantic_record_annotation(
                        ctx, node, NULL, imported_result,
                        SEMANTIC_RESULT_VALUE_RVALUE);
                    return imported_result;
                }
            }
            Slot* slot = NULL;
            const char* result_type = NULL;
            uint32_t parameter_offset = 0u;
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
                if (id->name && strcmp(id->name, "intr") == 0)
                    semantic_validate_intr_intrinsic(ctx, node);
            } else if (call->callee &&
                call->callee->type == AST_MEMBER_ACCESS) {
                int is_instance_call = 0;
                slot = semantic_resolve_group_callable(
                    ctx, (ASTMemberAccess*)call->callee,
                    &is_instance_call, 0);
                if (is_instance_call) parameter_offset = 1u;
            }
            result_type = slot ? slot->type_name : NULL;
            if (slot && slot->kind == SLOT_VAR &&
                semantic_callback_type_from_slot(ctx, slot)) {
                result_type = semantic_callback_return_type(ctx, slot);
            }
            if (result_type) {
                const char* normalized_result_type = result_type;
                while (*normalized_result_type == '!')
                    ++normalized_result_type;
                if (semantic_type_pattern_contains_variable(
                        normalized_result_type)) {
                    const char* parameter_pattern = NULL;
                    ASTNode* inferred_argument = semantic_find_type_variable_argument(
                        slot, call->args, parameter_offset, &parameter_pattern);
                    const char* actual_argument_type = inferred_argument
                        ? get_expr_type(ctx, inferred_argument) : NULL;
                    const char* inferred_base_type = NULL;
                    if (parameter_pattern && actual_argument_type &&
                        semantic_unify_type_pattern(
                            ctx, parameter_pattern, actual_argument_type,
                            &inferred_base_type) && inferred_base_type) {
                        result_type = semantic_substitute_type_variable(
                            ctx, result_type, inferred_base_type);
                    }
                }
            }
            semantic_record_annotation(
                ctx, node,
                slot && slot->kind == SLOT_VAR ? NULL : slot,
                result_type, SEMANTIC_RESULT_VALUE_RVALUE);
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
        case AST_UNARY_EXPR: {
            ASTUnaryExpr* unary = (ASTUnaryExpr*)node;
            const char* operand_type = get_expr_type(ctx, unary->operand);
            const char* type_name = operand_type;
            if (unary->op == TOKEN_STAR) {
                type_name = semantic_dereference_target_type(
                    operand_type, NULL);
                /* semantic_check_node owns the single diagnostic. */
            } else if (unary->op == TOKEN_BIT_AND) {
                type_name = semantic_intern_expression_type(
                    ctx, "&", operand_type);
            } else if (unary->op == TOKEN_BORROW_MUT) {
                type_name = semantic_intern_expression_type(
                    ctx, "&mut", operand_type);
            }
            semantic_record_annotation(
                ctx, node, NULL, type_name, SEMANTIC_RESULT_VALUE_RVALUE);
            return type_name;
        }
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
            size_t vector_dimension = 0u;
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
            } else if (semantic_vector_dimension(
                    target_type, &vector_dimension)) {
                /* Vec2/Vec3/Vec4 components use f64 scalar representation. */
                element_type = "f64";
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
                else if(!are_types_compatible(ctx, element_type,current_type)) {
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


static int semantic_expression_contains_safe_reference(
    SemanticContext* ctx, const ASTNode* expression, unsigned int depth
) {
    const ASTNode* current;
    const char* expression_type;
    if (!ctx || !expression) return 0;
    if (depth > 64u) return 1;
    switch (expression->type) {
        case AST_ARRAY_LITERAL:
            for (current = ((const ASTArrayLiteral*)expression)->elements;
                 current; current = current->next) {
                if (semantic_expression_contains_safe_reference(
                        ctx, current, depth + 1u)) return 1;
            }
            return 0;
        case AST_TUPLE:
            for (current = ((const ASTTuple*)expression)->elements;
                 current; current = current->next) {
                if (semantic_expression_contains_safe_reference(
                        ctx, current, depth + 1u)) return 1;
            }
            return 0;
        case AST_STRUCT_LITERAL:
            for (current = ((const ASTStructLiteral*)expression)->fields;
                 current; current = current->next) {
                if (current->type == AST_STRUCT_FIELD_INIT &&
                    semantic_expression_contains_safe_reference(
                        ctx, ((const ASTStructFieldInit*)current)->value,
                        depth + 1u)) return 1;
            }
            return 0;
        case AST_NAMED_ARG:
            return semantic_expression_contains_safe_reference(
                ctx, ((const ASTNamedArg*)expression)->value, depth + 1u);
        case AST_RELOCATE:
            return semantic_expression_contains_safe_reference(
                ctx, ((const ASTRelocate*)expression)->source, depth + 1u);
        case AST_CAST: {
            const ASTCast* cast = (const ASTCast*)expression;
            if (cast->target_type &&
                cast->target_type->type == AST_IDENTIFIER &&
                semantic_is_raw_pointer_type_name(
                    ((const ASTIdentifier*)cast->target_type)->name)) {
                /* An explicit conversion to raw pointer is the language's
                   unsafe escape hatch; it no longer carries safe provenance. */
                return 0;
            }
            return semantic_expression_contains_safe_reference(
                ctx, cast->value, depth + 1u);
        }
        default:
            break;
    }
    expression_type = get_expr_type(ctx, (ASTNode*)expression);
    return semantic_type_contains_safe_reference(
        ctx, expression_type, 0u);
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

static const char* semantic_dereference_target_type(
    const char* type_name, int* is_shared_reference
) {
    const char* raw_type = type_name;
    if (is_shared_reference) *is_shared_reference = 0;
    if (!raw_type) return NULL;
    if (raw_type[0] == '!') ++raw_type;
    if (strncmp(raw_type, "&mut", 4u) == 0) return raw_type + 4;
    if (raw_type[0] == '&') {
        if (is_shared_reference) *is_shared_reference = 1;
        return raw_type + 1;
    }
    if (raw_type[0] == '*') return raw_type + 1;
    return NULL;
}

static int are_types_compatible(SemanticContext* ctx, const char* type1, const char* type2) {
    const char* t1;
    const char* t2;
    if (!type1 || !type2) return 0;
    t1 = (type1[0] == '!') ? type1 + 1 : type1;
    t2 = (type2[0] == '!') ? type2 + 1 : type2;
    if (strcmp(t1, t2) == 0) return 1;
    /* `t` is the language's implicit type variable; call-site inference
       binds it to the concrete argument/initializer type. */
    if (strcmp(t1, "t") == 0 || strcmp(t2, "t") == 0) return 1;
    if ((semantic_is_raw_pointer_type_name(t1) ||
         semantic_is_function_type_name(ctx, t1)) &&
        strcmp(t2, "null") == 0) return 1;
    {
        const char* function_t1 = semantic_function_type_text(ctx, t1);
        const char* function_t2 = semantic_function_type_text(ctx, t2);
        if (function_t1 && function_t2)
            return strcmp(function_t1, function_t2) == 0;
    }
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

static int semantic_is_compile_time_constant_expression_depth(
    SemanticContext* ctx, const ASTNode* node, unsigned int depth
) {
    const ASTNode* current;
    if (!ctx || !node || depth > 128u) return 0;
    switch (node->type) {
        case AST_LITERAL:
            return 1;

        case AST_IDENTIFIER: {
            const Slot* slot = find_slot(ctx,
                ((const ASTIdentifier*)node)->name);
            const ASTVarDecl* declaration;
            if (!slot || !slot->is_const || !slot->declaration_node ||
                slot->declaration_node->type != AST_VAR_DECL)
                return 0;
            declaration = (const ASTVarDecl*)slot->declaration_node;
            return declaration->is_const && declaration->initializer &&
                semantic_is_compile_time_constant_expression_depth(
                    ctx, declaration->initializer, depth + 1u);
        }

        case AST_UNARY_EXPR: {
            const ASTUnaryExpr* unary = (const ASTUnaryExpr*)node;
            if (unary->op != TOKEN_PLUS && unary->op != TOKEN_MINUS &&
                unary->op != TOKEN_NOT && unary->op != TOKEN_BIT_NOT)
                return 0;
            return semantic_is_compile_time_constant_expression_depth(
                ctx, unary->operand, depth + 1u);
        }

        case AST_BINARY_EXPR: {
            const ASTBinaryExpr* binary = (const ASTBinaryExpr*)node;
            return binary->left && binary->right &&
                semantic_is_compile_time_constant_expression_depth(
                    ctx, binary->left, depth + 1u) &&
                semantic_is_compile_time_constant_expression_depth(
                    ctx, binary->right, depth + 1u);
        }

        case AST_ARRAY_LITERAL:
            for (current = ((const ASTArrayLiteral*)node)->elements;
                 current; current = current->next) {
                if (!semantic_is_compile_time_constant_expression_depth(
                        ctx, current, depth + 1u)) return 0;
            }
            return 1;

        case AST_TUPLE:
            for (current = ((const ASTTuple*)node)->elements;
                 current; current = current->next) {
                if (!semantic_is_compile_time_constant_expression_depth(
                        ctx, current, depth + 1u)) return 0;
            }
            return 1;

        case AST_STRUCT_LITERAL:
            for (current = ((const ASTStructLiteral*)node)->fields;
                 current; current = current->next) {
                const ASTStructFieldInit* field;
                if (current->type != AST_STRUCT_FIELD_INIT) return 0;
                field = (const ASTStructFieldInit*)current;
                if (!field->value ||
                    !semantic_is_compile_time_constant_expression_depth(
                        ctx, field->value, depth + 1u)) return 0;
            }
            return 1;

        case AST_CAST:
            return semantic_is_compile_time_constant_expression_depth(
                ctx, ((const ASTCast*)node)->value, depth + 1u);

        default:
            /* Calls, mutable reads, assignments, I/O and control flow are
               runtime behavior and cannot initialize a c: declaration. */
            return 0;
    }
}

static int semantic_is_compile_time_constant_expression(
    SemanticContext* ctx, const ASTNode* node
) {
    return semantic_is_compile_time_constant_expression_depth(ctx, node, 0u);
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
        "i8", "i16", "i32", "i64", "i128",
        "u8", "u16", "u32", "u64", "u128",
        "f32", "f64", "f128", "d32", "d64", "d128"
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
        "i8", "i16", "i32", "i64", "i128", "u8", "u16", "u32", "u64", "u128"
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
        { "i32", 32u, 0 }, { "i64", 64u, 0 }, { "i128", 128u, 0 },
        { "u8", 8u, 1 }, { "u16", 16u, 1 },
        { "u32", 32u, 1 }, { "u64", 64u, 1 }, { "u128", 128u, 1 }
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
        strcmp(type_name, "i8") == 0 || strcmp(type_name, "i128") == 0 ||
        strcmp(type_name, "u128") == 0 || strcmp(type_name, "f32") == 0 ||
        strcmp(type_name, "f128") == 0 || strcmp(type_name, "d32") == 0 ||
        strcmp(type_name, "d64") == 0 || strcmp(type_name, "d128") == 0 ||
        strcmp(type_name, "char") == 0 || strcmp(type_name, "bit") == 0 ||
        strcmp(type_name, "byte") == 0 ||
        strcmp(type_name, "8bit") == 0 || strcmp(type_name, "16bit") == 0 ||
        strcmp(type_name, "32bit") == 0 || strcmp(type_name, "64bit") == 0 ||
        strcmp(type_name, "128bit") == 0 || strcmp(type_name, "1byte") == 0 ||
        strcmp(type_name, "2byte") == 0 || strcmp(type_name, "4byte") == 0 ||
        strcmp(type_name, "8byte") == 0 || strcmp(type_name, "16byte") == 0 ||
        strcmp(type_name, "Vec2") == 0 || strcmp(type_name, "Vec3") == 0 ||
        strcmp(type_name, "Vec4") == 0 || strcmp(type_name, "map") == 0 ||
        strcmp(type_name, "Map") == 0 || strcmp(type_name, "void") == 0 ||
        strcmp(type_name, "any") == 0) {
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

static const char* semantic_intrinsic_expected_name(ASTNodeType type) {
    switch (type) {
        case AST_ECHO: return "echo";
        case AST_PRNT: return "prnt";
        case AST_PRMT: return "prmt";
        case AST_WRITE: return "write";
        case AST_FWRITE: return "fwrite";
        case AST_ADDR: return "addr";
        case AST_LEN: return "len";
        case AST_PEEK: return "peek";
        case AST_POKE: return "poke";
        case AST_INB: return "inb";
        case AST_OUTB: return "outb";
        /* AST_INTR is a dedicated hardware interrupt node; unlike ordinary
           named intrinsics it intentionally has no callee identifier. */
        case AST_AREA: return "area";
        case AST_ZONE: return "zone";
        case AST_FREE: return "free";
        default: return NULL;
    }
}

static void semantic_validate_intrinsic_callee(
    SemanticContext* ctx, ASTNode* node, const ASTCall* call
) {
    const char* expected = semantic_intrinsic_expected_name(node->type);
    ASTIdentifier* identifier;
    if (!expected) return;
    /*
     * Dedicated intrinsic AST node kinds already encode their canonical
     * identity. Parser-produced nodes therefore legitimately omit callee;
     * when an optional callee is present, validate it strictly.
     */
    if (!call || !call->callee) return;
    if (call->callee->type != AST_IDENTIFIER) {
        report_error(ctx, call->callee, ERR_SEMANTIC_TYPE_MISMATCH,
            "Intrinsic callee must be an identifier node.");
        return;
    }
    identifier = (ASTIdentifier*)call->callee;
    if (!identifier->name || strcmp(identifier->name, expected) != 0) {
        char message[192];
        (void)snprintf(message, sizeof(message),
            "Intrinsic AST callee must be '%s'.", expected);
        report_error(ctx, call->callee, ERR_SEMANTIC_TYPE_MISMATCH, message);
    }
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

static void semantic_validate_intr_intrinsic(
    SemanticContext* ctx, ASTNode* node
) {
    ASTCall* call = (ASTCall*)node;
    size_t count = semantic_intrinsic_argument_count(call);
    ASTNode* number = semantic_intrinsic_argument_value(call->args);
    const char* type = number ? get_expr_type(ctx, number) : NULL;
    if (count != 1u) {
        char message[144];
        (void)snprintf(message, sizeof(message),
            "intr() expects exactly one integer argument; received %llu.",
            (unsigned long long)count);
        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
        return;
    }
    if (type && strcmp(type, "any") != 0 &&
        !semantic_is_integer_value_type(ctx, type)) {
        report_error(ctx, number, ERR_SEMANTIC_TYPE_MISMATCH,
            "intr() interrupt number must be an integer.");
    }
}

static void semantic_validate_irq_intrinsic(
    SemanticContext* ctx, ASTNode* node
) {
    ASTCall* call = (ASTCall*)node;
    size_t count = semantic_intrinsic_argument_count(call);
    ASTNode* number = semantic_intrinsic_argument_value(call->args);
    ASTNode* handler = call->args && call->args->next
        ? semantic_intrinsic_argument_value(call->args->next) : NULL;
    const char* number_type = number ? get_expr_type(ctx, number) : NULL;
    if (count != 2u) {
        char message[160];
        (void)snprintf(message, sizeof(message),
            "irq() expects exactly 2 arguments (interrupt number, handler); received %llu.",
            (unsigned long long)count);
        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
        return;
    }
    if (number_type && strcmp(number_type, "any") != 0 &&
        !semantic_is_integer_value_type(ctx, number_type)) {
        report_error(ctx, number, ERR_SEMANTIC_TYPE_MISMATCH,
            "irq() interrupt number must be an integer.");
    }
    if (!handler || (handler->type != AST_IDENTIFIER &&
                     handler->type != AST_FUNC_CALL)) {
        report_error(ctx, handler ? handler : node, ERR_SEMANTIC_TYPE_MISMATCH,
            "irq() handler must be a function identifier or callable expression.");
    } else if (handler->type == AST_IDENTIFIER) {
        ASTIdentifier* id = (ASTIdentifier*)handler;
        Slot* slot = id->name ? find_slot(ctx, id->name) : NULL;
        if (slot && slot->kind != SLOT_FUNC) {
            report_error(ctx, handler, ERR_SEMANTIC_TYPE_MISMATCH,
                "irq() handler identifier must resolve to a declared function.");
        }
    }
}

static void semantic_validate_reg_intrinsic(
    SemanticContext* ctx, ASTNode* node
) {
    ASTCall* call = (ASTCall*)node;
    size_t count = semantic_intrinsic_argument_count(call);
    ASTNode* operation = semantic_intrinsic_argument_value(call->args);
    ASTNode* name = call->args && call->args->next
        ? semantic_intrinsic_argument_value(call->args->next) : NULL;
    ASTNode* value = call->args && call->args->next &&
        call->args->next->next
        ? semantic_intrinsic_argument_value(call->args->next->next) : NULL;
    if (count < 2u || count > 3u) {
        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
            "reg() expects 2 or 3 arguments: operation, register name, optional value.");
        return;
    }
    if (!operation || operation->type != AST_IDENTIFIER ||
        !((ASTIdentifier*)operation)->name ||
        (strcmp(((ASTIdentifier*)operation)->name, "r") != 0 &&
         strcmp(((ASTIdentifier*)operation)->name, "w") != 0)) {
        report_error(ctx, operation ? operation : node,
            ERR_SEMANTIC_TYPE_MISMATCH,
            "reg() operation must be identifier 'r' (read) or 'w' (write).");
        return;
    }
    {
        int write = strcmp(((ASTIdentifier*)operation)->name, "w") == 0;
        if ((write && count != 3u) || (!write && count != 2u)) {
            report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                "reg.r() requires operation/name; reg.w() requires operation/name/value.");
            return;
        }
    }
    if (!name || name->type != AST_LITERAL ||
        ((ASTLiteral*)name)->type != TOKEN_STRING ||
        !((ASTLiteral*)name)->string_value ||
        !((ASTLiteral*)name)->string_value[0]) {
        report_error(ctx, name ? name : node, ERR_SEMANTIC_TYPE_MISMATCH,
            "reg() register name must be a non-empty string literal.");
    }
    if (value) {
        const char* type = get_expr_type(ctx, value);
        if (type && strcmp(type, "any") != 0 &&
            !semantic_is_integer_value_type(ctx, type) &&
            !semantic_is_integer_or_address_type(ctx, type)) {
            report_error(ctx, value, ERR_SEMANTIC_TYPE_MISMATCH,
                "reg.w() value must be an integer or address value.");
        }
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

static Slot* semantic_pattern_enum_slot(
    SemanticContext* ctx, const char* subject_type
) {
    const char* type_name = subject_type;
    Slot* slot;
    if (!ctx || !type_name || !*type_name) return NULL;
    if (*type_name == '!') ++type_name;
    slot = find_slot(ctx, type_name);
    return slot && slot->kind == SLOT_ENUM ? slot : NULL;
}

static ASTEnumVariant* semantic_find_enum_variant(
    Slot* enum_slot, const char* variant_name
) {
    ASTNode* current;
    const ASTEnumDecl* declaration;
    if (!enum_slot || !variant_name || !*variant_name ||
        !enum_slot->declaration_node ||
        enum_slot->declaration_node->type != AST_ENUM_DECL) return NULL;
    declaration = (const ASTEnumDecl*)enum_slot->declaration_node;
    for (current = declaration->variants; current; current = current->next) {
        ASTEnumVariant* variant;
        if (current->type != AST_ENUM_VARIANT) continue;
        variant = (ASTEnumVariant*)current;
        if (variant->name && strcmp(variant->name, variant_name) == 0)
            return variant;
    }
    return NULL;
}


/* Enum constructors use the type-qualified call form: Type.Variant(...). */
static int semantic_check_enum_constructor_call(
    SemanticContext* ctx, ASTNode* call_node, ASTCall* call
) {
    ASTMemberAccess* access;
    ASTIdentifier* type_identifier;
    Slot* enum_slot;
    ASTEnumVariant* variant;
    ASTNode* field_node;
    ASTNode* argument;
    unsigned int expected_count=0u, actual_count=0u;
    int named_arguments=0;
    if(!ctx || !call || !call->callee ||
       call->callee->type!=AST_MEMBER_ACCESS) return 0;
    access=(ASTMemberAccess*)call->callee;
    if(!access->object || access->object->type!=AST_IDENTIFIER ||
       !access->member_name) return 0;
    type_identifier=(ASTIdentifier*)access->object;
    if(!type_identifier->name) return 0;
    enum_slot=find_slot(ctx,type_identifier->name);
    if(!enum_slot || enum_slot->kind!=SLOT_ENUM) return 0;

    variant=semantic_find_enum_variant(enum_slot,access->member_name);
    if(!variant) {
        char message[256];
        (void)snprintf(message,sizeof(message),
            "Enum '%s' has no variant named '%s'.",
            enum_slot->name,access->member_name);
        report_error(ctx,call_node,ERR_SEMANTIC_TYPE_MISMATCH,message);
        return 1;
    }

    for(field_node=variant->data;field_node;field_node=field_node->next) {
        if(field_node->type==AST_VAR_DECL) ++expected_count;
    }
    for(argument=call->args;argument;argument=argument->next) {
        ++actual_count;
        if(argument->type==AST_NAMED_ARG) named_arguments=1;
    }
    if(actual_count!=expected_count) {
        char message[256];
        (void)snprintf(message,sizeof(message),
            "Enum constructor '%s.%s' expects %u payload argument(s), but got %u.",
            enum_slot->name,variant->name,expected_count,actual_count);
        report_error(ctx,call_node,ERR_SEMANTIC_ARG_COUNT,message);
    }

    if(named_arguments) {
        for(argument=call->args;argument;argument=argument->next) {
            ASTNamedArg* named;
            ASTNode* candidate;
            ASTVarDecl* matched_field=NULL;
            const char* actual_type;
            if(argument->type!=AST_NAMED_ARG) {
                report_error(ctx,argument,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Enum constructor arguments must be either all positional or all named.");
                continue;
            }
            named=(ASTNamedArg*)argument;
            if(!named->name || !named->value) {
                report_error(ctx,argument,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Enum constructor named argument requires a field name and value.");
                continue;
            }
            for(candidate=variant->data;candidate;candidate=candidate->next) {
                ASTVarDecl* field;
                if(candidate->type!=AST_VAR_DECL) continue;
                field=(ASTVarDecl*)candidate;
                if(field->name && strcmp(field->name,named->name)==0) {
                    matched_field=field;
                    break;
                }
            }
            if(!matched_field) {
                char message[256];
                (void)snprintf(message,sizeof(message),
                    "Enum variant '%s' has no payload field named '%s'.",
                    variant->name,named->name);
                report_error(ctx,argument,ERR_SEMANTIC_TYPE_MISMATCH,message);
                continue;
            }
            {
                ASTNode* previous_argument;
                for(previous_argument=call->args;previous_argument!=argument;
                    previous_argument=previous_argument->next) {
                    if(previous_argument->type==AST_NAMED_ARG &&
                       ((ASTNamedArg*)previous_argument)->name &&
                       strcmp(((ASTNamedArg*)previous_argument)->name,named->name)==0) {
                        report_error(ctx,argument,ERR_SEMANTIC_REDEFINED,
                            "Enum constructor payload field is supplied more than once.");
                        break;
                    }
                }
            }
            actual_type=get_expr_type(ctx,named->value);
            if(matched_field->type_name && actual_type &&
               !are_types_compatible(ctx, matched_field->type_name,actual_type)) {
                char message[256];
                (void)snprintf(message,sizeof(message),
                    "Enum payload '%s' expects '%s', but got '%s'.",
                    matched_field->name,matched_field->type_name,actual_type);
                report_error(ctx,named->value,ERR_SEMANTIC_ARG_TYPE,message);
            }
        }
        for(field_node=variant->data;field_node;field_node=field_node->next) {
            ASTVarDecl* required_field;
            ASTNode* supplied;
            int found=0;
            if(field_node->type!=AST_VAR_DECL) continue;
            required_field=(ASTVarDecl*)field_node;
            for(supplied=call->args;supplied;supplied=supplied->next) {
                if(supplied->type==AST_NAMED_ARG &&
                   ((ASTNamedArg*)supplied)->name && required_field->name &&
                   strcmp(((ASTNamedArg*)supplied)->name,required_field->name)==0) {
                    found=1;break;
                }
            }
            if(!found) {
                char message[256];
                (void)snprintf(message,sizeof(message),
                    "Enum constructor '%s.%s' is missing payload field '%s'.",
                    enum_slot->name,variant->name,
                    required_field->name ? required_field->name : "<unnamed>");
                report_error(ctx,call_node,ERR_SEMANTIC_ARG_COUNT,message);
            }
        }
    } else {
        ASTNode* field=variant->data;
        for(argument=call->args;argument;argument=argument->next) {
            ASTVarDecl* payload_field=NULL;
            const char* actual_type;
            while(field && field->type!=AST_VAR_DECL) field=field->next;
            if(!field) break;
            payload_field=(ASTVarDecl*)field;
            actual_type=get_expr_type(ctx,argument);
            if(payload_field->type_name && actual_type &&
               !are_types_compatible(ctx, payload_field->type_name,actual_type)) {
                char message[256];
                (void)snprintf(message,sizeof(message),
                    "Enum payload '%s' expects '%s', but got '%s'.",
                    payload_field->name ? payload_field->name : "<unnamed>",
                    payload_field->type_name,actual_type);
                report_error(ctx,argument,ERR_SEMANTIC_ARG_TYPE,message);
            }
            field=field->next;
        }
    }
    semantic_record_annotation(ctx,call->callee,enum_slot,enum_slot->name,
        SEMANTIC_RESULT_VALUE_FUNCTION_DESIGNATOR);
    semantic_record_annotation(ctx,call_node,enum_slot,enum_slot->name,
        SEMANTIC_RESULT_VALUE_RVALUE);
    return 1;
}

static void semantic_add_pattern_binding(
    SemanticContext* ctx, ASTNode* node, const char* name,
    const char* binding_type
) {
    Slot* existing;
    Slot* slot;
    if (!name || !name[0]) {
        report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
            "Pattern binding is missing its identifier.");
        return;
    }
    existing=find_slot(ctx,name);
    if(existing && existing->depth==ctx->current_depth) {
        report_error(ctx,node,ERR_SEMANTIC_REDEFINED,
            "Pattern binding is duplicated in the same arm.");
        return;
    }
    slot=add_slot(ctx,name,SLOT_VAR,0,
        binding_type && *binding_type ? binding_type : "any",
        node,1,0,0,0u);
    if (slot && slot->type_name) {
        slot->type_id = semantic_type_id(ctx,slot->type_name,0);
        if (ctx->result_enabled && slot->symbol_id != 0u &&
            slot->type_id != 0u) {
            SemanticResultStatus type_status =
                semantic_result_set_symbol_type(
                    &ctx->result,slot->symbol_id,slot->type_id);
            semantic_set_result_error(ctx,type_status);
        }
    }
    semantic_record_annotation(ctx,node,slot,
        slot ? slot->type_name : binding_type,
        SEMANTIC_RESULT_VALUE_LVALUE);
}

static const char* semantic_pattern_enum_variant_name(
    SemanticContext* ctx, ASTNode* pattern, const char* subject_type
) {
    Slot* enum_slot = semantic_pattern_enum_slot(ctx,subject_type);
    ASTNode* current;
    if(!enum_slot) return NULL;
    for(current=pattern;current;current=current->next) {
        if(current->type==AST_PATTERN_CONSTRUCTOR) {
            ASTPatternConstructor* constructor=(ASTPatternConstructor*)current;
            if(constructor->name &&
               semantic_find_enum_variant(enum_slot,constructor->name))
                return constructor->name;
        } else if(current->type==AST_PATTERN_BINDING) {
            ASTPatternBinding* binding=(ASTPatternBinding*)current;
            ASTEnumVariant* variant=binding->name
                ? semantic_find_enum_variant(enum_slot,binding->name) : NULL;
            if(variant && !variant->data) return binding->name;
        }
    }
    return NULL;
}

static int semantic_scan_arm_is_guarded(const ASTScanArm* arm) {
    return arm && arm->has_guard != 0;
}

static void semantic_check_enum_scan_coverage(
    SemanticContext* ctx, ASTScan* scan, const char* subject_type
) {
    Slot* enum_slot=semantic_pattern_enum_slot(ctx,subject_type);
    ASTNode* arm_node;
    ASTNode* prior;
    ASTNode* variant_node;
    int has_else=0;
    if(!enum_slot || !scan ||
       !enum_slot->declaration_node ||
       enum_slot->declaration_node->type!=AST_ENUM_DECL) return;

    for(arm_node=scan->arms;arm_node;arm_node=arm_node->next) {
        ASTScanArm* arm;
        const char* current_name;
        if(arm_node->type!=AST_SCAN_ARM) continue;
        arm=(ASTScanArm*)arm_node;
        if(!arm->pattern) { has_else=1; continue; }
        if(semantic_scan_arm_is_guarded(arm)) continue;
        current_name=semantic_pattern_enum_variant_name(
            ctx,arm->pattern,subject_type);
        if(!current_name) continue;
        for(prior=scan->arms;prior && prior!=arm_node;prior=prior->next) {
            ASTScanArm* prior_arm;
            const char* prior_name;
            if(prior->type!=AST_SCAN_ARM) continue;
            prior_arm=(ASTScanArm*)prior;
            if(!prior_arm->pattern || semantic_scan_arm_is_guarded(prior_arm))
                continue;
            prior_name=semantic_pattern_enum_variant_name(
                ctx,prior_arm->pattern,subject_type);
            if(prior_name && strcmp(prior_name,current_name)==0) {
                char message[256];
                (void)snprintf(message,sizeof(message),
                    "Enum variant '%s' is matched more than once in this scan.",
                    current_name);
                report_error(ctx,arm_node,ERR_SEMANTIC_REDEFINED,message);
                break;
            }
        }
    }

    if(has_else) return;
    for(variant_node=((ASTEnumDecl*)enum_slot->declaration_node)->variants;
        variant_node;variant_node=variant_node->next) {
        ASTEnumVariant* variant;
        int covered=0;
        if(variant_node->type!=AST_ENUM_VARIANT) continue;
        variant=(ASTEnumVariant*)variant_node;
        if(!variant->name || !variant->name[0]) continue;
        for(arm_node=scan->arms;arm_node;arm_node=arm_node->next) {
            ASTScanArm* arm;
            const char* matched;
            if(arm_node->type!=AST_SCAN_ARM) continue;
            arm=(ASTScanArm*)arm_node;
            if(!arm->pattern || semantic_scan_arm_is_guarded(arm)) continue;
            matched=semantic_pattern_enum_variant_name(
                ctx,arm->pattern,subject_type);
            if(matched && strcmp(matched,variant->name)==0) {
                covered=1;
                break;
            }
        }
        if(!covered) {
            char message[256];
            (void)snprintf(message,sizeof(message),
                "Non-exhaustive scan for enum '%s': missing variant '%s' or an else arm.",
                enum_slot->name,variant->name);
            report_error(ctx,(ASTNode*)variant,ERR_SEMANTIC_TYPE_MISMATCH,message);
        }
    }
}

static void semantic_check_pattern_bindings(
    SemanticContext* ctx, ASTNode* pattern, const char* subject_type
) {
    ASTNode* current;
    const char* binding_type = subject_type && *subject_type
        ? subject_type : "any";
    Slot* enum_slot = semantic_pattern_enum_slot(ctx,subject_type);
    for (current=pattern; current; current=current->next) {
        if (current->type==AST_PATTERN_BINDING) {
            ASTPatternBinding* binding=(ASTPatternBinding*)current;
            /* A bare enum variant is a tag match, not a new local variable. */
            if (enum_slot && binding->name) {
                ASTEnumVariant* bare_variant =
                    semantic_find_enum_variant(enum_slot,binding->name);
                if (bare_variant && !bare_variant->data) continue;
                if (bare_variant && bare_variant->data) {
                    char message[256];
                    (void)snprintf(message,sizeof(message),
                        "Enum variant '%s' carries payload and requires a constructor pattern.",
                        binding->name);
                    report_error(ctx,current,ERR_SEMANTIC_ARG_TYPE,message);
                    continue;
                }
            }
            semantic_add_pattern_binding(
                ctx,current,binding->name,binding_type);
        } else if (current->type==AST_PATTERN_CONSTRUCTOR) {
            ASTPatternConstructor* constructor=(ASTPatternConstructor*)current;
            ASTEnumVariant* variant;
            ASTNode* field_node;
            ASTNode* binding_node;
            if(!constructor->name || !constructor->name[0]) {
                report_error(ctx,current,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Constructor pattern is missing its name.");
                continue;
            }
            if(!enum_slot) {
                char message[256];
                (void)snprintf(message,sizeof(message),
                    "Constructor pattern requires an enum subject; got '%s'.",
                    binding_type);
                report_error(ctx,current,ERR_SEMANTIC_TYPE_MISMATCH,message);
                continue;
            }
            variant=semantic_find_enum_variant(enum_slot,constructor->name);
            if(!variant) {
                char message[256];
                (void)snprintf(message,sizeof(message),
                    "Enum '%s' has no variant named '%s'.",
                    enum_slot->name,constructor->name);
                report_error(ctx,current,ERR_SEMANTIC_UNKNOWN_TYPE,message);
                continue;
            }
            field_node=variant->data;
            binding_node=constructor->bindings;
            while(field_node && binding_node) {
                ASTVarDecl* field;
                ASTPatternBinding* binding;
                if(field_node->type!=AST_VAR_DECL) {
                    report_error(ctx,field_node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "Enum payload contains an invalid field declaration.");
                    field_node=field_node->next;
                    binding_node=binding_node->next;
                    continue;
                }
                if(binding_node->type!=AST_PATTERN_BINDING) {
                    report_error(ctx,binding_node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "Enum constructor payload patterns must be simple bindings.");
                    field_node=field_node->next;
                    binding_node=binding_node->next;
                    continue;
                }
                field=(ASTVarDecl*)field_node;
                binding=(ASTPatternBinding*)binding_node;
                semantic_add_pattern_binding(
                    ctx,binding_node,binding->name,
                    field->type_name && field->type_name[0]
                        ? field->type_name : "any");
                field_node=field_node->next;
                binding_node=binding_node->next;
            }
            if(field_node || binding_node) {
                char message[256];
                size_t expected=0u, actual=0u;
                ASTNode* item;
                for(item=variant->data;item;item=item->next) ++expected;
                for(item=constructor->bindings;item;item=item->next) ++actual;
                (void)snprintf(message,sizeof(message),
                    "Enum variant '%s' expects %lu payload binding(s), but pattern provides %lu.",
                    constructor->name,(unsigned long)expected,(unsigned long)actual);
                report_error(ctx,current,ERR_SEMANTIC_ARG_TYPE,message);
            }
        } else if(current->type==AST_LITERAL) {
            const char* literal_type=get_expr_type(ctx,current);
            if(subject_type && literal_type &&
               !are_types_compatible(ctx, subject_type,literal_type)) {
                char message[256];
                (void)snprintf(message,sizeof(message),
                    "Pattern literal type '%s' is incompatible with subject type '%s'.",
                    literal_type,subject_type);
                report_error(ctx,current,ERR_SEMANTIC_TYPE_MISMATCH,message);
            }
        } else {
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
        if(actual && expected && !are_types_compatible(ctx, expected,actual)) {
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
            if (actual_type && !are_types_compatible(ctx, element_type, actual_type)) {
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
    int required_parameter_count = 0;
    int saw_default_parameter = 0;
    ASTNode* parameter;
    if (!ctx || !function_slot || !function) return 0;
    if (function_slot->param_count != 0 || function_slot->param_types ||
        function_slot->is_variadic) return 1;

    for (parameter = function->params; parameter; parameter = parameter->next) {
        if (parameter->type == AST_VARIADIC_PARAM) {
            if (parameter->next) {
                report_error(ctx, parameter, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Variadic parameter must be the final function parameter.");
            }
            function_slot->is_variadic = 1;
        } else if (parameter->type == AST_VAR_DECL) {
            ASTVarDecl* variable = (ASTVarDecl*)parameter;
            ++parameter_count;
            if (variable->initializer) {
                saw_default_parameter = 1;
            } else if (saw_default_parameter) {
                report_error(ctx, parameter, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Required parameters cannot follow parameters with default values.");
            } else {
                ++required_parameter_count;
            }
        } else {
            semantic_set_result_error(ctx,SEMANTIC_RESULT_INVALID_DATA);
            return 0;
        }
    }
    function_slot->param_count = parameter_count;
    function_slot->required_param_count = required_parameter_count;
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
                } else if (variable->initializer) {
                    const char* inferred_parameter_type =
                        get_expr_type(ctx, variable->initializer);
                    if (inferred_parameter_type && *inferred_parameter_type) {
                        function_slot->param_types[index] =
                            semantic_duplicate(ctx, inferred_parameter_type);
                        if (!function_slot->param_types[index]) {
                            semantic_set_result_error(
                                ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
                            return 0;
                        }
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
            /* The implementation declaration emits the single duplicate-name diagnostic. */
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

static const ASTTypeExpr* semantic_unwrap_type_qualifiers(
    const ASTTypeExpr* type
) {
    while (type && type->kind == AST_TYPE_QUALIFIED)
        type = type->child && type->child->type == AST_TYPE_EXPR
            ? (const ASTTypeExpr*)type->child : NULL;
    return type;
}

static const ASTTypeExpr* semantic_callback_type_from_expr(
    SemanticContext* ctx, const ASTTypeExpr* type, unsigned int depth
) {
    Slot* alias;
    const ASTNewTypeDecl* declaration;
    type = semantic_unwrap_type_qualifiers(type);
    if (!ctx || !type || depth > 32u) return NULL;
    if (type->kind == AST_TYPE_FUNCTION) return type;
    if (type->kind != AST_TYPE_NAMED || !type->name) return NULL;
    alias = find_slot(ctx, type->name);
    if (!alias || alias->kind != SLOT_NEWTYPE ||
        !alias->declaration_node ||
        alias->declaration_node->type != AST_NEWTYPE_DECL) return NULL;
    declaration = (const ASTNewTypeDecl*)alias->declaration_node;
    if (declaration->target_type_expr &&
        declaration->target_type_expr->type == AST_TYPE_EXPR) {
        return semantic_callback_type_from_expr(
            ctx, (const ASTTypeExpr*)declaration->target_type_expr,
            depth + 1u);
    }
    return NULL;
}

static const ASTTypeExpr* semantic_callback_type_from_slot(
    SemanticContext* ctx, const Slot* slot
) {
    const ASTVarDecl* variable;
    const ASTNewTypeDecl* newtype;
    const ASTTypeExpr* type = NULL;
    const char* raw_type;
    Slot* alias;
    if (!ctx || !slot) return NULL;
    if (slot->declaration_node &&
        slot->declaration_node->type == AST_VAR_DECL) {
        variable = (const ASTVarDecl*)slot->declaration_node;
        if (variable->type_expr &&
            variable->type_expr->type == AST_TYPE_EXPR) {
            type = semantic_callback_type_from_expr(
                ctx, (const ASTTypeExpr*)variable->type_expr, 0u);
            if (type) return type;
        }
    }
    raw_type = slot->type_name;
    while (raw_type && *raw_type == '!') ++raw_type;
    if (!raw_type || !*raw_type) return NULL;
    if (strncmp(raw_type, "f:(", 3u) == 0) {
        if (slot->declaration_node &&
            slot->declaration_node->type == AST_NEWTYPE_DECL) {
            newtype = (const ASTNewTypeDecl*)slot->declaration_node;
            if (newtype->target_type_expr &&
                newtype->target_type_expr->type == AST_TYPE_EXPR)
                return semantic_callback_type_from_expr(
                    ctx, (const ASTTypeExpr*)newtype->target_type_expr, 0u);
        }
        return NULL;
    }
    alias = find_slot(ctx, raw_type);
    if (alias && alias->kind == SLOT_NEWTYPE &&
        alias->declaration_node &&
        alias->declaration_node->type == AST_NEWTYPE_DECL) {
        newtype = (const ASTNewTypeDecl*)alias->declaration_node;
        if (newtype->target_type_expr &&
            newtype->target_type_expr->type == AST_TYPE_EXPR)
            return semantic_callback_type_from_expr(
                ctx, (const ASTTypeExpr*)newtype->target_type_expr, 0u);
    }
    return NULL;
}

static const char* semantic_callback_signature_text(
    SemanticContext* ctx, const Slot* slot
) {
    const char* raw_type;
    Slot* alias;
    if (!ctx || !slot) return NULL;
    raw_type = slot->type_name;
    while (raw_type && *raw_type == '!') ++raw_type;
    if (!raw_type) return NULL;
    if (strncmp(raw_type, "f:(", 3u) == 0) return raw_type;
    alias = find_slot(ctx, raw_type);
    if (alias && alias->kind == SLOT_NEWTYPE) {
        raw_type = alias->type_name;
        while (raw_type && *raw_type == '!') ++raw_type;
        if (raw_type && strncmp(raw_type, "f:(", 3u) == 0)
            return raw_type;
    }
    return NULL;
}

static const char* semantic_callback_return_type(
    SemanticContext* ctx, const Slot* slot
) {
    const char* signature = semantic_callback_signature_text(ctx, slot);
    const char* cursor;
    size_t depth = 0u;
    if (!signature || strncmp(signature, "f:(", 3u) != 0) return NULL;
    cursor = signature + 2u;
    for (; *cursor; ++cursor) {
        if (*cursor == '(') ++depth;
        else if (*cursor == ')') {
            if (depth == 1u) break;
            if (depth > 1u) --depth;
        }
    }
    if (*cursor != ')') return NULL;
    ++cursor;
    if (*cursor == '!' || *cursor == '>') ++cursor;
    return *cursor ? cursor : NULL;
}

/* Build a non-owning AST signature view for a function-valued binding.
   The parameter type strings are owned temporarily by the caller. */
static int semantic_prepare_callback_signature(
    SemanticContext* ctx, Slot* source, Slot* callback_view,
    ASTFuncDecl* callback_declaration, char*** owned_parameter_types
) {
    const ASTTypeExpr* function_type;
    ASTNode* parameter;
    uint32_t count = 0u;
    uint32_t required = 0u;
    uint32_t index = 0u;
    char** parameter_types = NULL;
    if (owned_parameter_types) *owned_parameter_types = NULL;
    if (!ctx || !source || !callback_view || !callback_declaration ||
        !owned_parameter_types) return 0;
    function_type = semantic_callback_type_from_slot(ctx, source);
    if (!function_type) return 0;
    for (parameter = function_type->params; parameter; parameter = parameter->next) {
        if (parameter->type == AST_VARIADIC_PARAM) {
            if (parameter->next) {
                report_error(ctx, parameter, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Variadic callback parameter must be the final parameter.");
            }
        } else if (parameter->type == AST_VAR_DECL) {
            const ASTVarDecl* variable = (const ASTVarDecl*)parameter;
            if (count == UINT32_MAX) return 0;
            ++count;
            if (!variable->initializer) ++required;
        } else {
            report_error(ctx, parameter, ERR_SEMANTIC_TYPE_MISMATCH,
                "Callback signature contains an unsupported parameter node.");
            return 0;
        }
    }
    if (count != 0u) {
        parameter_types = (char**)semantic_callocate(
            ctx, (size_t)count, sizeof(*parameter_types));
        if (!parameter_types) {
            semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
            return 0;
        }
    }
    for (parameter = function_type->params; parameter; parameter = parameter->next) {
        if (parameter->type == AST_VARIADIC_PARAM) continue;
        {
            const ASTVarDecl* variable = (const ASTVarDecl*)parameter;
            if (!variable->type_name || !variable->type_name[0]) {
                uint32_t i;
                for (i = 0u; i < index; ++i) free(parameter_types[i]);
                free(parameter_types);
                report_error(ctx, parameter, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Callback parameter is missing its resolved type name.");
                return 0;
            }
            parameter_types[index] = semantic_duplicate(ctx, variable->type_name);
            if (!parameter_types[index]) {
                uint32_t i;
                for (i = 0u; i < index; ++i) free(parameter_types[i]);
                free(parameter_types);
                semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
                return 0;
            }
            ++index;
        }
    }
    memset(callback_declaration, 0, sizeof(*callback_declaration));
    callback_declaration->base.type = AST_FUNC_DECL;
    callback_declaration->params = function_type->params;
    *callback_view = *source;
    callback_view->kind = SLOT_FUNC;
    callback_view->declaration_node = (ASTNode*)callback_declaration;
    callback_view->param_count = (int)count;
    callback_view->required_param_count = (int)required;
    callback_view->is_variadic = 0;
    for (parameter = function_type->params; parameter; parameter = parameter->next) {
        if (parameter->type == AST_VARIADIC_PARAM)
            callback_view->is_variadic = 1;
    }
    callback_view->param_types = parameter_types;
    *owned_parameter_types = parameter_types;
    return 1;
}

static void semantic_destroy_callback_parameter_types(
    char** parameter_types, uint32_t count
) {
    uint32_t index;
    if (!parameter_types) return;
    for (index = 0u; index < count; ++index) free(parameter_types[index]);
    free(parameter_types);
}

static int semantic_is_type_identifier_char(char value) {
    return (value >= 'a' && value <= 'z') ||
        (value >= 'A' && value <= 'Z') ||
        (value >= '0' && value <= '9') || value == '_';
}

static int semantic_type_pattern_contains_variable(const char* pattern) {
    const char* current;
    if (!pattern) return 0;
    while (*pattern == '!') ++pattern;
    for (current = pattern; *current; ++current) {
        if (*current == 't' &&
            (current == pattern ||
             !semantic_is_type_identifier_char(current[-1])) &&
            !semantic_is_type_identifier_char(current[1])) return 1;
    }
    return 0;
}


static int semantic_unify_function_type_pattern(
    SemanticContext* ctx, const char* pattern, const char* actual,
    const char** inferred_type
) {
    const char* p_signature = semantic_function_type_text(ctx, pattern);
    const char* a_signature = semantic_function_type_text(ctx, actual);
    const char* p_cursor;
    const char* a_cursor;
    const char* p_close;
    const char* a_close;
    size_t p_depth = 1u;
    size_t a_depth = 1u;
    if (!p_signature || !a_signature) return 0;
    p_cursor = p_signature + 3u;
    a_cursor = a_signature + 3u;
    p_close = p_cursor;
    a_close = a_cursor;
    while (*p_close && p_depth) {
        if (*p_close == '(') ++p_depth;
        else if (*p_close == ')') --p_depth;
        if (p_depth) ++p_close;
    }
    while (*a_close && a_depth) {
        if (*a_close == '(') ++a_depth;
        else if (*a_close == ')') --a_depth;
        if (a_depth) ++a_close;
    }
    if (p_depth || a_depth || *p_close != ')' || *a_close != ')')
        return 0;
    while (p_cursor < p_close || a_cursor < a_close) {
        const char* p_end = p_cursor;
        const char* a_end = a_cursor;
        const char* p_marker = NULL;
        const char* a_marker = NULL;
        size_t p_nested = 0u;
        size_t a_nested = 0u;
        size_t p_name_length;
        size_t a_name_length;
        char p_type[512];
        char a_type[512];
        size_t p_type_length;
        size_t a_type_length;
        if (p_cursor == p_close || a_cursor == a_close) return 0;
        while (p_end < p_close) {
            if (*p_end == '(') ++p_nested;
            else if (*p_end == ')' && p_nested) --p_nested;
            else if (*p_end == ',' && p_nested == 0u) break;
            ++p_end;
        }
        while (a_end < a_close) {
            if (*a_end == '(') ++a_nested;
            else if (*a_end == ')' && a_nested) --a_nested;
            else if (*a_end == ',' && a_nested == 0u) break;
            ++a_end;
        }
        if ((size_t)(p_end - p_cursor) == 3u &&
            memcmp(p_cursor, "...", 3u) == 0) {
            if ((size_t)(a_end - a_cursor) != 3u ||
                memcmp(a_cursor, "...", 3u) != 0) return 0;
        } else {
            const char* scan;
            for (scan = p_cursor; scan < p_end; ++scan)
                if (*scan == '!' || *scan == ':') { p_marker = scan; break; }
            for (scan = a_cursor; scan < a_end; ++scan)
                if (*scan == '!' || *scan == ':') { a_marker = scan; break; }
            if (!p_marker || !a_marker) return 0;
            p_name_length = (size_t)(p_marker - p_cursor);
            a_name_length = (size_t)(a_marker - a_cursor);
            if (p_name_length != a_name_length ||
                memcmp(p_cursor, a_cursor, p_name_length) != 0) return 0;
            p_type_length = (size_t)(p_end - (p_marker + 1));
            a_type_length = (size_t)(a_end - (a_marker + 1));
            if (p_type_length == 0u || a_type_length == 0u ||
                p_type_length >= sizeof(p_type) ||
                a_type_length >= sizeof(a_type)) return 0;
            memcpy(p_type, p_marker + 1, p_type_length);
            p_type[p_type_length] = '\0';
            memcpy(a_type, a_marker + 1, a_type_length);
            a_type[a_type_length] = '\0';
            if (!semantic_unify_type_pattern(
                    ctx, p_type, a_type, inferred_type)) return 0;
        }
        p_cursor = p_end < p_close ? p_end + 1 : p_close;
        a_cursor = a_end < a_close ? a_end + 1 : a_close;
    }
    if (p_cursor != p_close || a_cursor != a_close) return 0;
    {
        const char* p_return = p_close + 1;
        const char* a_return = a_close + 1;
        if (*p_return == '!' || *p_return == '>') ++p_return;
        if (*a_return == '!' || *a_return == '>') ++a_return;
        if (!*p_return || !*a_return) return 0;
        return semantic_unify_type_pattern(
            ctx, p_return, a_return, inferred_type);
    }
}

static int semantic_unify_type_pattern(
    SemanticContext* ctx, const char* pattern, const char* actual,
    const char** inferred_type
) {
    const char* pattern_dimensions;
    const char* actual_dimensions;
    char pattern_element[256];
    char actual_element[256];
    size_t pattern_length;
    size_t actual_length;
    if (!pattern || !actual || !inferred_type) return 0;
    while (*pattern == '!') ++pattern;
    while (*actual == '!') ++actual;
    if (strncmp(pattern, "f:(", 3u) == 0 ||
        semantic_function_type_text(ctx, pattern)) {
        if (!(strncmp(actual, "f:(", 3u) == 0 ||
              semantic_function_type_text(ctx, actual))) return 0;
        return semantic_unify_function_type_pattern(
            ctx, pattern, actual, inferred_type);
    }
    if (strcmp(pattern, "t") == 0) {
        /* A generic function may forward an unresolved type variable from one
           parameter into another callback parameter without binding it yet. */
        if (strcmp(actual, "t") == 0) return 1;
        if (strcmp(actual, "null") == 0 ||
            semantic_type_contains_safe_reference(ctx, actual, 0u))
            return 0;
        if (!*inferred_type) {
            *inferred_type = semantic_intern_full_expression_type(ctx, actual);
            return *inferred_type != NULL;
        }
        return are_types_compatible(ctx, *inferred_type, actual);
    }
    if (pattern[0] == '*') {
        return actual[0] == '*' &&
            semantic_unify_type_pattern(
                ctx, pattern + 1, actual + 1, inferred_type);
    }
    if (strncmp(pattern, "&mut", 4u) == 0) {
        return strncmp(actual, "&mut", 4u) == 0 &&
            semantic_unify_type_pattern(
                ctx, pattern + 4, actual + 4, inferred_type);
    }
    if (pattern[0] == '&') {
        return actual[0] == '&' && strncmp(actual, "&mut", 4u) != 0 &&
            semantic_unify_type_pattern(
                ctx, pattern + 1, actual + 1, inferred_type);
    }
    if (strncmp(pattern, "array|", 6u) == 0) {
        if (strncmp(actual, "array|", 6u) != 0) return 0;
        pattern_dimensions = strchr(pattern + 6u, '[');
        actual_dimensions = strchr(actual + 6u, '[');
        if (!pattern_dimensions || !actual_dimensions ||
            (strcmp(pattern_dimensions, "[]") != 0 &&
             strcmp(pattern_dimensions, actual_dimensions) != 0)) return 0;
        pattern_length = (size_t)(pattern_dimensions - (pattern + 6u));
        actual_length = (size_t)(actual_dimensions - (actual + 6u));
        if (pattern_length == 0u || actual_length == 0u ||
            pattern_length >= sizeof(pattern_element) ||
            actual_length >= sizeof(actual_element)) return 0;
        memcpy(pattern_element, pattern + 6u, pattern_length);
        pattern_element[pattern_length] = '\0';
        memcpy(actual_element, actual + 6u, actual_length);
        actual_element[actual_length] = '\0';
        return semantic_unify_type_pattern(
            ctx, pattern_element, actual_element, inferred_type);
    }
    if (semantic_type_pattern_contains_variable(pattern)) return 0;
    return are_types_compatible(ctx, pattern, actual);
}

static const char* semantic_substitute_type_variable(
    SemanticContext* ctx, const char* pattern, const char* inferred_type
) {
    size_t pattern_length;
    size_t inferred_length;
    size_t occurrence_count = 0u;
    size_t output_length;
    size_t read_index;
    size_t write_index = 0u;
    char* output;
    const char* result;
    if (!ctx || !pattern || !inferred_type) return NULL;
    while (*inferred_type == '!') ++inferred_type;
    pattern_length = strlen(pattern);
    inferred_length = strlen(inferred_type);
    if (!inferred_length) return NULL;
    for (read_index = 0u; read_index < pattern_length; ++read_index) {
        if (pattern[read_index] == 't' &&
            (read_index == 0u ||
             !semantic_is_type_identifier_char(pattern[read_index - 1u])) &&
            !semantic_is_type_identifier_char(pattern[read_index + 1u]))
            ++occurrence_count;
    }
    if (!occurrence_count) return pattern;
    if (inferred_length > SIZE_MAX / occurrence_count ||
        pattern_length > SIZE_MAX - occurrence_count * inferred_length)
        return NULL;
    output_length = pattern_length +
        occurrence_count * (inferred_length - 1u);
    output = (char*)semantic_allocate(ctx, output_length + 1u);
    if (!output) {
        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
        return NULL;
    }
    for (read_index = 0u; read_index < pattern_length;) {
        if (pattern[read_index] == 't' &&
            (read_index == 0u ||
             !semantic_is_type_identifier_char(pattern[read_index - 1u])) &&
            !semantic_is_type_identifier_char(pattern[read_index + 1u])) {
            memcpy(output + write_index, inferred_type, inferred_length);
            write_index += inferred_length;
            ++read_index;
        } else {
            output[write_index++] = pattern[read_index++];
        }
    }
    output[write_index] = '\0';
    result = semantic_intern_full_expression_type(ctx, output);
    free(output);
    return result;
}

static void semantic_validate_call_arguments(
    SemanticContext* ctx, ASTNode* call_node, Slot* function_slot,
    ASTNode* arguments, uint32_t parameter_offset
) {
    uint32_t argument_count = 0u;
    uint32_t positional_count = 0u;
    uint32_t expected_count;
    uint32_t minimum_count;
    ASTNode* argument;
    uint32_t argument_index = 0u;
    int call_error_baseline;
    ASTFuncDecl* declaration = NULL;
    const char* inferred_type_variable = NULL;
    typedef struct {
        Slot* owner;
        char* path;
        uint8_t is_mutable;
    } SemanticCallBorrow;
    SemanticCallBorrow* temporary_borrows = NULL;
    uint32_t temporary_borrow_count = 0u;

    if (!ctx || !function_slot) return;
    call_error_baseline = ctx->error_count;
    for (argument = arguments; argument; argument = argument->next) {
        ++argument_count;
        if (argument->type != AST_NAMED_ARG) ++positional_count;
    }

    if (function_slot->param_count < (int)parameter_offset) {
        report_error(ctx, call_node, ERR_SEMANTIC_TYPE_MISMATCH,
            "Resolved method signature is inconsistent with its receiver mode.");
        return;
    }
    expected_count = (uint32_t)function_slot->param_count - parameter_offset;
    minimum_count = (uint32_t)function_slot->required_param_count > parameter_offset
        ? (uint32_t)function_slot->required_param_count - parameter_offset : 0u;
    if (function_slot->declaration_node &&
        (function_slot->declaration_node->type == AST_FUNC_DECL ||
         function_slot->declaration_node->type == AST_EXTERN_DECL)) {
        declaration = (ASTFuncDecl*)function_slot->declaration_node;
    }
    if (!function_slot->is_variadic && positional_count > expected_count) {
        char message[256];
        snprintf(message, sizeof(message), msg_get(ERR_SEMANTIC_ARG_COUNT),
            function_slot->name, (int)expected_count, (int)argument_count);
        report_error(ctx, call_node, ERR_SEMANTIC_ARG_COUNT, message);
    } else if (!declaration && argument_count < minimum_count) {
        char message[256];
        snprintf(message, sizeof(message), msg_get(ERR_SEMANTIC_ARG_COUNT),
            function_slot->name, (int)expected_count, (int)argument_count);
        report_error(ctx, call_node, ERR_SEMANTIC_ARG_COUNT, message);
    }

    if (argument_count) {
        temporary_borrows = (SemanticCallBorrow*)calloc(
            argument_count, sizeof(*temporary_borrows));
        if (!temporary_borrows) {
            report_error(ctx, call_node, ERR_SEMANTIC_TYPE_MISMATCH,
                "Unable to allocate temporary call-borrow tracking state.");
            return;
        }
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
                    uint32_t declaration_index = 0u;
                    for(parameter_node=declaration->params;parameter_node;
                        parameter_node=parameter_node->next) {
                        if(parameter_node->type==AST_VARIADIC_PARAM) break;
                        if(declaration_index++ < parameter_offset) continue;
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

            if (expected_type && semantic_type_is_any(ctx, expected_type) &&
                (semantic_expression_contains_safe_reference(
                    ctx, argument_value, 0u) ||
                 semantic_type_or_callback_contract_contains_safe_reference(
                    ctx, argument_type, 0u))) {
                report_issue(ctx, argument, ERR_SEMANTIC_ARG_TYPE,
                    "A safe reference cannot be passed through an any parameter because that would erase its owner and lifetime provenance.",
                    "Keep the argument typed as &T/&mut T, or pass an ordinary value/raw pointer when dynamic typing is required.",
                    "Keep the argument typed as &T/&mut T, or pass an ordinary value/raw pointer when dynamic typing is required.",
                    0, 0);
            }
            if (function_slot->declaration_node &&
                function_slot->declaration_node->type == AST_EXTERN_DECL &&
                semantic_type_or_callback_contract_contains_safe_reference(ctx, argument_type, 0u)) {
                report_issue(ctx, argument, ERR_SEMANTIC_ARG_TYPE,
                    "A safe reference or callback signature containing one cannot cross an extern call boundary without an explicit lifetime/ABI contract.",
                    "Pass a value or explicitly managed raw pointer, or wrap the external API with a typed Nexus Flow function that enforces the reference lifetime locally.",
                    "Pass a value or explicitly managed raw pointer, or wrap the external API with a typed Nexus Flow function that enforces the reference lifetime locally.",
                    0, 0);
            }
            if (function_slot->is_variadic &&
                parameter_index >= (uint32_t)function_slot->param_count &&
                semantic_type_or_callback_contract_contains_safe_reference(ctx, argument_type, 0u)) {
                report_issue(ctx, argument, ERR_SEMANTIC_ARG_TYPE,
                    "A variadic argument cannot carry a safe reference or a value that contains one; the variadic call contract has no element-level lifetime tracking.",
                    "Pass a value type or an explicitly managed raw pointer. Use a typed reference parameter when the callee needs a safe reference.",
                    "Pass a value type or an explicitly managed raw pointer. Use a typed reference parameter when the callee needs a safe reference.",
                    0, 0);
            }

            /* A direct &owner / &mut owner argument borrows for the duration
               of this call. Reference identifiers already carry a binding. */
            if (expected_type && argument_value &&
                argument_value->type == AST_UNARY_EXPR &&
                semantic_is_reference_type_name(expected_type)) {
                ASTUnaryExpr* borrow = (ASTUnaryExpr*)argument_value;
                if (borrow->op == TOKEN_BIT_AND ||
                    borrow->op == TOKEN_BORROW_MUT) {
                    Slot* owner = semantic_lvalue_root_slot(ctx, borrow->operand);
                    char* borrow_path = semantic_lvalue_path(ctx, borrow->operand);
                    int is_mutable = borrow->op == TOKEN_BORROW_MUT;
                    int expected_mutable =
                        semantic_is_mut_reference_type_name(expected_type);
                    int conflict = 0;
                    uint32_t k;
                    if (!owner || !semantic_is_lvalue(ctx, borrow->operand)) {
                        report_error(ctx, argument, ERR_SEMANTIC_TYPE_MISMATCH,
                            "Temporary call borrow requires a safe storage lvalue with known provenance.");
                        conflict = 1;
                    } else if (is_mutable != expected_mutable) {
                        report_error(ctx, argument, ERR_SEMANTIC_ARG_TYPE,
                            "Borrow mutability does not match the reference parameter.");
                        conflict = 1;
                    } else if (!borrow_path ||
                               semantic_borrow_conflicts(
                                   ctx, owner, borrow_path, is_mutable, NULL)) {
                        report_error(ctx, argument, ERR_SEMANTIC_TYPE_MISMATCH,
                            "Temporary call borrow has unknown provenance or overlaps an active reference.");
                        conflict = 1;
                    }
                    for (k = 0u; !conflict && k < temporary_borrow_count; ++k) {
                        if (temporary_borrows[k].owner == owner &&
                            semantic_borrow_paths_overlap(
                                borrow_path, temporary_borrows[k].path) &&
                            (is_mutable || temporary_borrows[k].is_mutable)) {
                            report_error(ctx, argument, ERR_SEMANTIC_TYPE_MISMATCH,
                                "Arguments in this call contain overlapping shared/exclusive borrows.");
                            conflict = 1;
                        }
                    }
                    if (!conflict) {
                        temporary_borrows[temporary_borrow_count].owner = owner;
                        temporary_borrows[temporary_borrow_count].path = borrow_path;
                        temporary_borrows[temporary_borrow_count].is_mutable =
                            (uint8_t)is_mutable;
                        borrow_path = NULL;
                        ++temporary_borrow_count;
                    }
                    free(borrow_path);
                }
            }

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

            if (argument_type && expected_type) {
                if (semantic_type_pattern_contains_variable(expected_type)) {
                    const char* inferred_base_type = NULL;
                    if (!semantic_unify_type_pattern(
                            ctx, expected_type, argument_type,
                            &inferred_base_type)) {
                        char message[384];
                        (void)snprintf(message, sizeof(message),
                            "Type variable pattern '%s' cannot match argument %u type '%s'.",
                            expected_type, (unsigned)(parameter_index + 1u),
                            argument_type);
                        report_error(ctx, argument, ERR_SEMANTIC_ARG_TYPE, message);
                    } else if (inferred_base_type) {
                        if (!inferred_type_variable)
                            inferred_type_variable = inferred_base_type;
                        else if (!are_types_compatible(
                                     ctx, inferred_type_variable,
                                     inferred_base_type)) {
                            char message[384];
                            (void)snprintf(message, sizeof(message),
                                "Type variable 't' was inferred as '%s', but argument %u requires incompatible base type '%s'.",
                                inferred_type_variable,
                                (unsigned)(parameter_index + 1u),
                                inferred_base_type);
                            report_error(ctx, argument,
                                ERR_SEMANTIC_ARG_TYPE, message);
                        }
                    }
                } else {
                    if (semantic_is_function_type_name(ctx, expected_type) &&
                        semantic_expression_may_be_null(ctx, argument_value)) {
                        report_issue(ctx, argument, ERR_SEMANTIC_ARG_TYPE,
                            "A nullable callback value cannot be passed to a non-null callback parameter.",
                            "Check the callback against null before passing it, or initialize it from a non-null callback value.",
                            "Check the callback against null before passing it, or initialize it from a non-null callback value.",
                            0, 0);
                    }
                    if (!are_types_compatible(ctx, expected_type, argument_type)) {
                        char message[256];
                        snprintf(message, sizeof(message), msg_get(ERR_SEMANTIC_ARG_TYPE),
                            (int)(parameter_index+1u),expected_type,argument_type);
                        report_error(ctx,argument,ERR_SEMANTIC_ARG_TYPE,message);
                    }
                }
            }
            if (parameter && parameter->inline_type &&
                parameter->inline_type->type == AST_ARRAY_TYPE &&
                argument_value && argument_value->type == AST_ARRAY_LITERAL) {
                semantic_validate_array_initializer(
                    ctx, argument_value,
                    ((ASTArrayType*)parameter->inline_type)->dimensions,
                    parameter->type_name);
            }
        }
    }

    /* Argument count alone cannot prove that named arguments supplied
       every required parameter (e.g. f(required, optional=...) called with
       only optional: value). Verify each required visible parameter by name. */
    if (declaration && ctx->error_count == call_error_baseline) {
        uint32_t visible_index;
        for (visible_index = positional_count;
             visible_index < minimum_count; ++visible_index) {
            ASTNode* parameter_node = declaration->params;
            uint32_t declaration_index = 0u;
            const char* required_name = NULL;
            int supplied = 0;
            while (parameter_node) {
                if (parameter_node->type == AST_VARIADIC_PARAM) break;
                if (declaration_index++ >= parameter_offset) {
                    if (declaration_index - 1u - parameter_offset ==
                        visible_index) {
                        if (parameter_node->type == AST_VAR_DECL)
                            required_name = ((ASTVarDecl*)parameter_node)->name;
                        break;
                    }
                }
                parameter_node = parameter_node->next;
            }
            if (required_name) {
                ASTNode* supplied_argument;
                for (supplied_argument = arguments; supplied_argument;
                     supplied_argument = supplied_argument->next) {
                    if (supplied_argument->type == AST_NAMED_ARG) {
                        ASTNamedArg* named = (ASTNamedArg*)supplied_argument;
                        if (named->name &&
                            strcmp(named->name, required_name) == 0) {
                            supplied = 1;
                            break;
                        }
                    }
                }
                if (!supplied) {
                    char message[320];
                    (void)snprintf(message, sizeof(message),
                        "Required parameter '%s' is missing from call to '%s'.",
                        required_name, function_slot->name
                            ? function_slot->name : "<callable>");
                    report_issue(ctx, call_node, ERR_SEMANTIC_ARG_COUNT,
                        message,
                        "Pass this required parameter positionally or by its declared name; only parameters with defaults may be omitted.",
                        "Pass this required parameter positionally or by its declared name; only parameters with defaults may be omitted.",
                        0, 0);
                    break;
                }
            }
        }
    }

    /* Release in reverse acquisition order, including after argument errors. */
    while (temporary_borrow_count > 0u) {
        SemanticCallBorrow* borrow =
            &temporary_borrows[--temporary_borrow_count];
        free(borrow->path);
    }
    free(temporary_borrows);
}
 
/* Resolve a qualified module function against the verified import catalog.
 * Temporary AST/Slot records borrow immutable interface metadata only during
 * validation; no imported memory is copied into the local symbol table. */
static const char* semantic_resolve_imported_call(
    SemanticContext* ctx, ASTNode* call_node, int validate_arguments,
    int* out_handled
) {
    ASTCall* call;
    ASTMemberAccess* access;
    ASTIdentifier* selector_node;
    size_t alias_index = 0u, namespace_index = 0u, binding_index = 0u;
    OccSemanticImportEnvironmentStatus alias_status, namespace_status;
    OccSemanticImportedSymbol symbol;
    OccSemanticImportedType return_type;
    OccSemanticImportEnvironmentBinding binding;
    size_t relation_count = 0u, i;
    OccSemanticImportedRelation relations[256];
    OccSemanticImportedType parameter_types[256];
    ASTVarDecl parameters[256];
    Slot imported_slot;
    ASTFuncDecl imported_declaration;
    const char* result_type = NULL;
    uint8_t ordinal_seen[256];
    size_t parameter_count = 0u;
    int selector_recognized = 0;
    char* param_type_names[256];

    if (out_handled) *out_handled = 0;
    if (!ctx || !call_node || call_node->type != AST_FUNC_CALL ||
        !ctx->import_environment) return NULL;
    call = (ASTCall*)call_node;
    if (!call->callee || call->callee->type != AST_MEMBER_ACCESS) return NULL;
    access = (ASTMemberAccess*)call->callee;
    if (!access->object || access->object->type != AST_IDENTIFIER ||
        !access->member_name || !access->member_name[0]) return NULL;
    selector_node = (ASTIdentifier*)access->object;
    if (!selector_node->name || !selector_node->name[0]) return NULL;

    alias_status = occ_semantic_import_environment_find_alias(
        ctx->import_environment, selector_node->name, &alias_index);
    namespace_status = occ_semantic_import_environment_find_namespace(
        ctx->import_environment, selector_node->name, &namespace_index);
    if (alias_status == OCC_SEMANTIC_IMPORT_ENV_OK) {
        selector_recognized = 1;
        binding_index = alias_index;
    }
    if (namespace_status == OCC_SEMANTIC_IMPORT_ENV_OK) {
        if (selector_recognized && binding_index != namespace_index) {
            if (validate_arguments)
                report_issue(ctx, call_node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "The selector matches both an import alias and a different namespace binding.",
                    "Use a selector that identifies exactly one imported module.",
                    "Use a selector that identifies exactly one imported module.",
                    0, 0);
            return NULL;
        }
        selector_recognized = 1;
        binding_index = namespace_index;
    }
    if (!selector_recognized) return NULL;
    if (out_handled) *out_handled = 1;

#define IMPORT_CALL_ERROR(message_text, suggestion_text) do { \
    if (validate_arguments) report_issue(ctx, call_node, \
        ERR_SEMANTIC_TYPE_MISMATCH, (message_text), \
        (suggestion_text), (suggestion_text), 0, 0); \
    return NULL; \
} while (0)

    if (alias_status == OCC_SEMANTIC_IMPORT_ENV_AMBIGUOUS ||
        namespace_status == OCC_SEMANTIC_IMPORT_ENV_AMBIGUOUS)
        IMPORT_CALL_ERROR(
            "The module selector is ambiguous across imported bindings.",
            "Give each imported module a unique alias and namespace selector.");
    if (occ_semantic_import_environment_get_binding(
            ctx->import_environment, binding_index, &binding) !=
        OCC_SEMANTIC_IMPORT_ENV_OK)
        IMPORT_CALL_ERROR(
            "The selected import binding is unavailable.",
            "Check the module import record and verified interface.");
    if (occ_semantic_import_environment_find_symbol(
            ctx->import_environment, binding_index, access->member_name,
            &symbol) != OCC_SEMANTIC_IMPORT_ENV_OK)
        IMPORT_CALL_ERROR(
            "The selected module does not export a unique symbol with this name.",
            "Check the spelling and ensure the declaration is PUBLIC or EXPORTED.");
    if ((symbol.flags & (SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC |
                         SEMANTIC_RESULT_SYMBOL_FLAG_EXPORTED)) == 0u)
        IMPORT_CALL_ERROR(
            "The selected module symbol is not marked PUBLIC or EXPORTED.",
            "Expose the declaration through the module's public interface.");
    if (symbol.kind != SEMANTIC_RESULT_SYMBOL_FUNCTION)
        IMPORT_CALL_ERROR(
            "The selected imported symbol is not a function.",
            "Call an exported function or use a supported exported value/type.");
    if (symbol.source_type_id == 0u ||
        occ_semantic_import_environment_find_type(
            ctx->import_environment, binding_index, symbol.source_type_id,
            &return_type) != OCC_SEMANTIC_IMPORT_ENV_OK ||
        !return_type.name || !return_type.name[0])
        IMPORT_CALL_ERROR(
            "The imported function has no valid return-type metadata.",
            "Rebuild the module interface from a finalized SemanticResult.");
    result_type = return_type.name;
    if (occ_semantic_import_environment_get_relation_count(
            ctx->import_environment, binding_index, symbol.source_symbol_id,
            SEMANTIC_RESULT_RELATION_FUNCTION_PARAMETER,
            &relation_count) != OCC_SEMANTIC_IMPORT_ENV_OK ||
        relation_count > 256u)
        IMPORT_CALL_ERROR(
            "The imported function signature is incomplete or exceeds the supported 256-parameter limit.",
            "Reduce the exported parameter count or rebuild the module interface.");
    memset(&imported_slot, 0, sizeof(imported_slot));
    memset(&imported_declaration, 0, sizeof(imported_declaration));
    memset(parameters, 0, sizeof(parameters));
    memset(parameter_types, 0, sizeof(parameter_types));
    memset(ordinal_seen, 0, sizeof(ordinal_seen));
    memset(param_type_names, 0, sizeof(param_type_names));
    for (i = 0u; i < relation_count; ++i) {
        if (occ_semantic_import_environment_get_relation_at(
                ctx->import_environment, binding_index,
                symbol.source_symbol_id,
                SEMANTIC_RESULT_RELATION_FUNCTION_PARAMETER,
                i, &relations[i]) != OCC_SEMANTIC_IMPORT_ENV_OK)
            IMPORT_CALL_ERROR(
                "The imported function parameter metadata could not be read.",
                "Rebuild the module interface and verify its relation table.");
        if (relations[i].ordinal >= relation_count ||
            ordinal_seen[relations[i].ordinal] ||
            !relations[i].name || !relations[i].name[0] ||
            relations[i].type_id == 0u ||
            occ_semantic_import_environment_find_type(
                ctx->import_environment, binding_index,
                relations[i].type_id, &parameter_types[i]) !=
                OCC_SEMANTIC_IMPORT_ENV_OK ||
            !parameter_types[i].name || !parameter_types[i].name[0])
            IMPORT_CALL_ERROR(
                "The imported function has duplicate, missing, or invalid parameter metadata.",
                "Rebuild the module interface with a complete ordered function signature.");
        ordinal_seen[relations[i].ordinal] = 1u;
        parameters[relations[i].ordinal].base.type = AST_VAR_DECL;
        parameters[relations[i].ordinal].name = (char*)relations[i].name;
        parameters[relations[i].ordinal].type_name =
            (char*)parameter_types[i].name;
        param_type_names[relations[i].ordinal] =
            (char*)parameter_types[i].name;
    }
    for (i = 0u; i < relation_count; ++i) {
        if (!ordinal_seen[i])
            IMPORT_CALL_ERROR(
                "The imported function parameter ordinals are not contiguous.",
                "Rebuild the module interface with zero-based contiguous parameter ordinals.");
        if (i + 1u < relation_count)
            parameters[i].base.next = (ASTNode*)&parameters[i + 1u];
    }
    parameter_count = relation_count;
    imported_declaration.base.type = AST_FUNC_DECL;
    imported_declaration.name = (char*)symbol.name;
    imported_declaration.params = relation_count
        ? (ASTNode*)&parameters[0] : NULL;
    imported_declaration.return_type = (char*)result_type;
    imported_slot.name = (char*)symbol.name;
    imported_slot.kind = SLOT_FUNC;
    imported_slot.type_name = (char*)result_type;
    imported_slot.declaration_node = (ASTNode*)&imported_declaration;
    imported_slot.param_count = (int)parameter_count;
    imported_slot.required_param_count = (int)parameter_count;
    imported_slot.param_types = param_type_names;
    if (validate_arguments) {
        semantic_validate_call_arguments(
            ctx, call_node, &imported_slot, call->args, 0u);
        semantic_record_annotation(
            ctx, call->callee, NULL, result_type,
            SEMANTIC_RESULT_VALUE_FUNCTION_DESIGNATOR);
        semantic_record_annotation(
            ctx, call_node, NULL, result_type,
            SEMANTIC_RESULT_VALUE_RVALUE);
    }
#undef IMPORT_CALL_ERROR
    return result_type;
}


static ASTNode* semantic_find_type_variable_argument(
    Slot* function_slot, ASTNode* arguments, uint32_t parameter_offset,
    const char** out_pattern
) {
    uint32_t parameter_index;
    ASTFuncDecl* declaration = NULL;
    ASTNode* parameter_node = NULL;
    const char* parameter_name = NULL;
    uint32_t positional_index = 0u;
    ASTNode* argument;

    if (out_pattern) *out_pattern = NULL;
    if (!function_slot || !function_slot->param_types || !arguments ||
        function_slot->param_count < (int)parameter_offset) return NULL;

    for (parameter_index = parameter_offset;
         parameter_index < (uint32_t)function_slot->param_count;
         ++parameter_index) {
        const char* expected_type = function_slot->param_types[parameter_index];
        while (expected_type && *expected_type == '!') ++expected_type;
        if (expected_type &&
            semantic_type_pattern_contains_variable(expected_type)) {
            if (out_pattern)
                *out_pattern = function_slot->param_types[parameter_index];
            break;
        }
    }
    if (parameter_index >= (uint32_t)function_slot->param_count) return NULL;

    if (function_slot->declaration_node &&
        (function_slot->declaration_node->type == AST_FUNC_DECL ||
         function_slot->declaration_node->type == AST_EXTERN_DECL)) {
        declaration = (ASTFuncDecl*)function_slot->declaration_node;
    }
    if (declaration) {
        uint32_t current_index = 0u;
        for (parameter_node = declaration->params; parameter_node;
             parameter_node = parameter_node->next) {
            if (parameter_node->type == AST_VARIADIC_PARAM) continue;
            if (current_index++ == parameter_index) break;
        }
        if (parameter_node && parameter_node->type == AST_VAR_DECL)
            parameter_name = ((ASTVarDecl*)parameter_node)->name;
    }

    for (argument = arguments; argument; argument = argument->next) {
        if (argument->type == AST_NAMED_ARG) {
            ASTNamedArg* named = (ASTNamedArg*)argument;
            if (parameter_name && named->name &&
                strcmp(parameter_name, named->name) == 0)
                return named->value;
        } else {
            if (positional_index == parameter_index - parameter_offset)
                return argument;
            ++positional_index;
        }
    }
    return NULL;
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

static ThreadBlockEntry* semantic_find_thread_block(
    const SemanticContext* ctx, const char* name
) {
    ThreadBlockEntry* entry = ctx ? ctx->thread_blocks : NULL;
    if (!name) return NULL;
    while (entry) {
        if (entry->name && strcmp(entry->name, name) == 0) return entry;
        entry = entry->next;
    }
    return NULL;
}

static int semantic_register_thread_block(
    SemanticContext* ctx, ASTThreadBlock* block
) {
    ThreadBlockEntry* entry;
    if (!ctx || !block || !block->name || !block->name[0]) return 0;
    if (semantic_find_thread_block(ctx, block->name)) {
        char message[256];
        snprintf(message, sizeof(message),
            "Thread block '%s' is already declared; thread block names must be unique.",
            block->name);
        report_error(ctx, (ASTNode*)block, ERR_SEMANTIC_REDEFINED, message);
        return 0;
    }
    entry = (ThreadBlockEntry*)semantic_callocate(ctx, 1u, sizeof(*entry));
    if (!entry) {
        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
        report_error(ctx, (ASTNode*)block, ERR_SEMANTIC_TYPE_MISMATCH,
            "Unable to register thread_block: out of memory.");
        return 0;
    }
    entry->name = semantic_duplicate(ctx, block->name);
    if (!entry->name) {
        free(entry);
        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
        report_error(ctx, (ASTNode*)block, ERR_SEMANTIC_TYPE_MISMATCH,
            "Unable to copy thread_block name: out of memory.");
        return 0;
    }
    entry->declaration = (ASTNode*)block;
    if (ctx->thread_block_tail) ctx->thread_block_tail->next = entry;
    else ctx->thread_blocks = entry;
    ctx->thread_block_tail = entry;
    return 1;
}

static int semantic_register_thread_capture(
    SemanticContext* ctx, Slot* source, ASTNode* use_node
) {
    ThreadBlockEntry* block;
    ThreadBlockCapture* capture;
    if (!ctx || !source || !use_node ||
        !(block = ctx->active_thread_block)) return 1;
    if (source->kind != SLOT_VAR ||
        (uint32_t)source->depth > block->capture_base_depth) return 1;
    if (ctx->thread_capture_copy_depth == 0u) {
        char message[256];
        (void)snprintf(message, sizeof(message),
            "Thread block cannot directly access outer variable '%s'; capture its value explicitly with copy(%s).",
            source->name ? source->name : "<unnamed>",
            source->name ? source->name : "<unnamed>");
        report_error(ctx, use_node, ERR_SEMANTIC_TYPE_MISMATCH, message);
        return 0;
    }

    for (capture = block->captures; capture; capture = capture->next) {
        if ((source->symbol_id != 0u &&
             capture->symbol_id == source->symbol_id) ||
            (source->symbol_id == 0u &&
             capture->source_declaration == source->declaration_node))
            return 1;
    }

    capture = (ThreadBlockCapture*)semantic_callocate(ctx, 1u, sizeof(*capture));
    if (!capture) {
        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
        report_error(ctx, use_node, ERR_SEMANTIC_TYPE_MISMATCH,
            "Unable to record thread_block value capture: out of memory.");
        return 0;
    }
    capture->name = semantic_duplicate(ctx, source->name ? source->name : "");
    capture->type_name = semantic_duplicate(
        ctx, source->type_name ? source->type_name : "");
    if (!capture->name || !capture->type_name) {
        free(capture->name);
        free(capture->type_name);
        free(capture);
        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
        report_error(ctx, use_node, ERR_SEMANTIC_TYPE_MISMATCH,
            "Unable to copy thread_block capture metadata: out of memory.");
        return 0;
    }
    capture->symbol_id = source->symbol_id;
    capture->source_scope_id = source->scope_id;
    capture->source_depth = (uint32_t)source->depth;
    capture->first_use = use_node;
    capture->source_declaration = source->declaration_node;
    if (block->capture_tail) block->capture_tail->next = capture;
    else block->captures = capture;
    block->capture_tail = capture;
    ++block->capture_count;
    return 1;
}

static void semantic_validate_spawn_call(
    SemanticContext* ctx, ASTNode* node, ASTCall* call
) {
    ASTNode* argument;
    const char* name;
    if (!ctx || !call || !call->callee ||
        call->callee->type != AST_IDENTIFIER) return;
    name = ((ASTIdentifier*)call->callee)->name;
    if (!name || strcmp(name, "spawn") != 0) return;
    argument = call->args;
    if (!argument || argument->next || argument->type != AST_IDENTIFIER) {
        report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
            "spawn requires exactly one identifier naming a declared thread_block.");
        return;
    }
    name = ((ASTIdentifier*)argument)->name;
    if (!name) {
        report_error(ctx, argument, ERR_SEMANTIC_UNDEFINED,
            "spawn requires a named thread_block or a declared zero-argument function.");
        return;
    }
    /* spawn accepts either a named AST_THREAD_BLOCK payload or a function
       entry point. The parser intentionally treats both as module API calls. */
    if (semantic_find_thread_block(ctx, name)) return;
    {
        Slot* target = find_slot(ctx, name);
        if (target && target->kind == SLOT_FUNC) {
            ASTNode* declaration = target->declaration_node;
            ASTFuncDecl* function = declaration && declaration->type == AST_FUNC_DECL
                ? (ASTFuncDecl*)declaration : NULL;
            if (function && !function->params) return;
            report_error(ctx, argument, ERR_SEMANTIC_TYPE_MISMATCH,
                "spawn function entry must have zero parameters; pass arguments through an explicit Thread payload or API.");
            return;
        }
    }
    {
        char message[256];
        snprintf(message, sizeof(message),
            "spawn target '%s' is neither a declared thread_block nor a zero-argument function.",
            name);
        report_error(ctx, argument, ERR_SEMANTIC_UNDEFINED, message);
    }
}

static void semantic_copy_shared_reference_binding(
    SemanticContext* ctx, Slot* target, Slot* source, ASTNode* assignment
);

/* Resolve a conservative returned-reference contract: every reachable
   return in the supported statement forms must return the same ref parameter. */
typedef struct SemanticReturnedReferenceAlias {
    const char* name;
    int parameter_index;
    struct SemanticReturnedReferenceAlias* next;
} SemanticReturnedReferenceAlias;

static int semantic_returned_reference_parameter_index(
    const char* name, const ASTNode* params,
    const SemanticReturnedReferenceAlias* aliases
) {
    const ASTNode* parameter;
    int index = 0;
    if (!name) return -1;
    for (parameter = params; parameter; parameter = parameter->next) {
        if (parameter->type == AST_VAR_DECL) {
            const ASTVarDecl* declaration = (const ASTVarDecl*)parameter;
            if (declaration->name &&
                strcmp(declaration->name, name) == 0 &&
                semantic_is_reference_type_name(declaration->type_name))
                return index;
            ++index;
        } else if (parameter->type != AST_VARIADIC_PARAM) {
            return -1;
        }
    }
    for (; aliases; aliases = aliases->next) {
        if (aliases->name && strcmp(aliases->name, name) == 0)
            return aliases->parameter_index;
    }
    return -1;
}

static void semantic_returned_reference_alias_restore(
    SemanticReturnedReferenceAlias** aliases,
    SemanticReturnedReferenceAlias* marker
) {
    while (aliases && *aliases != marker) {
        SemanticReturnedReferenceAlias* removed = *aliases;
        *aliases = removed->next;
        free(removed);
    }
}

static int semantic_scan_returned_reference_parameter(
    const ASTNode* node, const ASTNode* params, int* selected, int* saw_return,
    SemanticReturnedReferenceAlias** aliases
) {
    const ASTNode* current;
    if (!node) return 1;
    for (current = node; current; current = current->next) {
        if (current->type == AST_RETURN) {
            const ASTUnaryExpr* ret = (const ASTUnaryExpr*)current;
            const char* returned_name;
            int matched;
            if (!ret->operand || ret->operand->type != AST_IDENTIFIER)
                return 0;
            returned_name = ((const ASTIdentifier*)ret->operand)->name;
            matched = semantic_returned_reference_parameter_index(
                returned_name, params, aliases ? *aliases : NULL);
            if (matched < 0 || (*saw_return && *selected != matched))
                return 0;
            *selected = matched;
            *saw_return = 1;
        } else if (current->type == AST_VAR_DECL) {
            const ASTVarDecl* declaration = (const ASTVarDecl*)current;
            if (declaration->name &&
                semantic_is_reference_type_name(declaration->type_name) &&
                declaration->initializer &&
                declaration->initializer->type == AST_IDENTIFIER) {
                int origin = semantic_returned_reference_parameter_index(
                    ((const ASTIdentifier*)declaration->initializer)->name,
                    params, aliases ? *aliases : NULL);
                if (origin >= 0) {
                    SemanticReturnedReferenceAlias* alias =
                        (SemanticReturnedReferenceAlias*)malloc(sizeof(*alias));
                    if (!alias) return 0;
                    alias->name = declaration->name;
                    alias->parameter_index = origin;
                    alias->next = aliases ? *aliases : NULL;
                    if (aliases) *aliases = alias;
                    else free(alias);
                }
            }
        } else if (current->type == AST_ASSIGNMENT) {
            const ASTBinaryExpr* assignment = (const ASTBinaryExpr*)current;
            if (aliases && assignment->left &&
                assignment->left->type == AST_IDENTIFIER) {
                const char* assigned_name =
                    ((const ASTIdentifier*)assignment->left)->name;
                const SemanticReturnedReferenceAlias* alias;
                for (alias = *aliases; alias; alias = alias->next) {
                    if (alias->name && assigned_name &&
                        strcmp(alias->name, assigned_name) == 0) {
                        /* Rebinding changes the origin; without a full CFG
                           merge this function's returned-origin inference fails closed. */
                        return 0;
                    }
                }
            }
        } else if (current->type == AST_BLOCK) {
            SemanticReturnedReferenceAlias* marker = aliases ? *aliases : NULL;
            int ok = semantic_scan_returned_reference_parameter(
                ((const ASTBlock*)current)->statements,
                params, selected, saw_return, aliases);
            if (aliases)
                semantic_returned_reference_alias_restore(aliases, marker);
            if (!ok) return 0;
        } else if (current->type == AST_IF) {
            const ASTIf* branch = (const ASTIf*)current;
            SemanticReturnedReferenceAlias* marker = aliases ? *aliases : NULL;
            int then_ok = semantic_scan_returned_reference_parameter(
                branch->then_branch, params, selected, saw_return, aliases);
            if (aliases)
                semantic_returned_reference_alias_restore(aliases, marker);
            if (!then_ok) return 0;
            if (!semantic_scan_returned_reference_parameter(
                    branch->else_branch, params, selected, saw_return, aliases))
                return 0;
            if (aliases)
                semantic_returned_reference_alias_restore(aliases, marker);
        } else if (current->type == AST_LOOP) {
            const ASTLoop* loop = (const ASTLoop*)current;
            SemanticReturnedReferenceAlias* marker = aliases ? *aliases : NULL;
            int ok = semantic_scan_returned_reference_parameter(
                loop->body, params, selected, saw_return, aliases);
            if (aliases)
                semantic_returned_reference_alias_restore(aliases, marker);
            if (!ok) return 0;
        } else if (current->type == AST_SELECT) {
            const ASTNode* arm;
            for (arm = ((const ASTSelect*)current)->arms; arm; arm = arm->next) {
                SemanticReturnedReferenceAlias* marker = aliases ? *aliases : NULL;
                int ok = arm->type == AST_SELECT_ARM &&
                    semantic_scan_returned_reference_parameter(
                        ((const ASTSelectArm*)arm)->body,
                        params, selected, saw_return, aliases);
                if (aliases)
                    semantic_returned_reference_alias_restore(aliases, marker);
                if (!ok) return 0;
            }
        } else if (current->type == AST_SCAN) {
            const ASTNode* arm;
            for (arm = ((const ASTScan*)current)->arms; arm; arm = arm->next) {
                SemanticReturnedReferenceAlias* marker = aliases ? *aliases : NULL;
                int ok = arm->type == AST_SCAN_ARM &&
                    semantic_scan_returned_reference_parameter(
                        ((const ASTScanArm*)arm)->body,
                        params, selected, saw_return, aliases);
                if (aliases)
                    semantic_returned_reference_alias_restore(aliases, marker);
                if (!ok) return 0;
            }
        } else if (current->type == AST_NEXUS_FLOW) {
            const ASTNexusFlow* flow = (const ASTNexusFlow*)current;
            if (!semantic_scan_returned_reference_parameter(
                    flow->right, params, selected, saw_return, aliases)) return 0;
        } else if (current->type == AST_DEFER) {
            SemanticReturnedReferenceAlias* marker = aliases ? *aliases : NULL;
            int ok = semantic_scan_returned_reference_parameter(
                ((const ASTDefer*)current)->body,
                params, selected, saw_return, aliases);
            if (aliases)
                semantic_returned_reference_alias_restore(aliases, marker);
            if (!ok) return 0;
        } else if (current->type == AST_ON_BLOCK) {
            SemanticReturnedReferenceAlias* marker = aliases ? *aliases : NULL;
            int ok = semantic_scan_returned_reference_parameter(
                ((const ASTOnBlock*)current)->body,
                params, selected, saw_return, aliases);
            if (aliases)
                semantic_returned_reference_alias_restore(aliases, marker);
            if (!ok) return 0;
        } else if (current->type == AST_THREAD_BLOCK) {
            SemanticReturnedReferenceAlias* marker = aliases ? *aliases : NULL;
            int ok = semantic_scan_returned_reference_parameter(
                ((const ASTThreadBlock*)current)->body,
                params, selected, saw_return, aliases);
            if (aliases)
                semantic_returned_reference_alias_restore(aliases, marker);
            if (!ok) return 0;
        } else if (current->type == AST_UNROLL) {
            SemanticReturnedReferenceAlias* marker = aliases ? *aliases : NULL;
            int ok = semantic_scan_returned_reference_parameter(
                ((const ASTUnroll*)current)->loop,
                params, selected, saw_return, aliases);
            if (aliases)
                semantic_returned_reference_alias_restore(aliases, marker);
            if (!ok) return 0;
        } else if (current->type == AST_FUNC_DECL ||
                   current->type == AST_LAMBDA) {
            continue;
        } else if (current->type == AST_RULESET_DECL ||
                   current->type == AST_RULE_TRIGGER ||
                   current->type == AST_RULE_ALWAYS ||
                   current->type == AST_RULE_DEFAULT ||
                   current->type == AST_RULE_BARRIER) {
            continue;
        }
    }
    return 1;
}

static int semantic_returned_reference_parameter(
    const ASTFuncDecl* declaration
) {
    int selected = -1;
    int saw_return = 0;
    SemanticReturnedReferenceAlias* aliases = NULL;
    int ok;
    if (!declaration || !declaration->body) return -1;
    ok = semantic_scan_returned_reference_parameter(
        declaration->body, declaration->params, &selected, &saw_return, &aliases);
    semantic_returned_reference_alias_restore(&aliases, NULL);
    if (!ok || !saw_return) return -1;
    return selected;
}

static ASTNode* semantic_call_argument_for_parameter(
    const ASTCall* call, const ASTFuncDecl* declaration,
    uint32_t parameter_index, uint32_t parameter_offset
) {
    const ASTNode* parameter;
    ASTNode* argument;
    uint32_t index = 0u;
    uint32_t visible_index;
    const char* parameter_name = NULL;
    if (!call || !declaration || parameter_index < parameter_offset)
        return NULL;
    visible_index = parameter_index - parameter_offset;
    for (parameter = declaration->params; parameter; parameter = parameter->next) {
        if (parameter->type == AST_VARIADIC_PARAM) continue;
        if (index++ == parameter_index && parameter->type == AST_VAR_DECL) {
            parameter_name = ((const ASTVarDecl*)parameter)->name;
            break;
        }
    }
    if (!parameter_name) return NULL;
    {
        uint32_t positional = 0u;
        for (argument = call->args; argument; argument = argument->next) {
            if (argument->type == AST_NAMED_ARG) {
                ASTNamedArg* named = (ASTNamedArg*)argument;
                if (named->name &&
                    strcmp(named->name, parameter_name) == 0) return named->value;
            } else if (positional++ == visible_index) {
                return argument;
            }
        }
    }
    return NULL;
}

static int semantic_bind_returned_call_reference(
    SemanticContext* ctx, Slot* borrower, ASTNode* initializer
) {
    ASTCall* call;
    Slot* function;
    ASTFuncDecl* declaration;
    ASTNode* source_argument;
    ASTNode* argument_value;
    Slot* owner = NULL;
    char* borrow_path = NULL;
    int parameter_index;
    int is_mutable;
    uint32_t parameter_offset = 0u;
    if (!ctx || !borrower || !initializer ||
        initializer->type != AST_FUNC_CALL) return 0;
    call = (ASTCall*)initializer;
    if (!call->callee) return 0;
    if (call->callee->type == AST_IDENTIFIER) {
        function = find_slot(ctx, ((ASTIdentifier*)call->callee)->name);
    } else if (call->callee->type == AST_MEMBER_ACCESS) {
        int is_instance_call = 0;
        function = semantic_resolve_group_callable(
            ctx, (ASTMemberAccess*)call->callee, &is_instance_call, 0);
        if (is_instance_call) parameter_offset = 1u;
    } else {
        return 0;
    }
    if (function && function->kind == SLOT_VAR &&
        semantic_is_reference_type_name(
            semantic_callback_return_type(ctx, function))) {
        if (!function->callable_declaration ||
            function->callable_declaration->type != AST_FUNC_DECL)
            return 1;
        declaration = (ASTFuncDecl*)function->callable_declaration;
    } else {
        if (!function || function->kind != SLOT_FUNC ||
            !function->declaration_node ||
            function->declaration_node->type != AST_FUNC_DECL) return 0;
        declaration = (ASTFuncDecl*)function->declaration_node;
    }
    parameter_index = semantic_returned_reference_parameter(declaration);
    if (parameter_index < 0 || (uint32_t)parameter_index < parameter_offset)
        return 0;
    source_argument = semantic_call_argument_for_parameter(
        call, declaration, (uint32_t)parameter_index, parameter_offset);
    argument_value = source_argument;
    if (argument_value && argument_value->type == AST_NAMED_ARG)
        argument_value = ((ASTNamedArg*)argument_value)->value;
    if (!argument_value) return 0;

    if (argument_value->type == AST_IDENTIFIER) {
        Slot* source = find_slot(ctx, ((ASTIdentifier*)argument_value)->name);
        Slot* origin;
        if (!source ||
            !semantic_is_reference_type_name(source->type_name)) return 0;
        if (!source->borrowed_from) {
            origin = source->borrow_origin;
            if (!origin && (source->is_parameter || source->depth == 0))
                origin = source;
            /* Returning a shared reference identifier preserves its caller
               lifetime anchor. Duplicating an untracked exclusive reference
               is not safe and remains fail-closed. */
            if (!origin ||
                semantic_is_mut_reference_type_name(source->type_name) ||
                semantic_is_mut_reference_type_name(borrower->type_name) ||
                !semantic_is_reference_type_name(borrower->type_name))
                return 0;
            if (borrower->depth < origin->depth) {
                report_issue(ctx, initializer, ERR_SEMANTIC_RETURN_MISMATCH,
                    "Returned reference would outlive the parameter or global reference from which it originates.",
                    "Keep the returned reference in a scope no longer-lived than its argument origin; do not store it in global or outer-scope state.",
                    "Keep the returned reference in a scope no longer-lived than its argument origin; do not store it in global or outer-scope state.",
                    0, 0);
                return 1;
            }
            semantic_release_borrow_binding(borrower);
            borrower->borrow_origin = origin;
            return 1;
        }
        owner = source->borrowed_from;
        if (source->borrow_path) {
            size_t source_path_length = strlen(source->borrow_path);
            borrow_path = (char*)malloc(source_path_length + 1u);
            if (borrow_path)
                memcpy(borrow_path, source->borrow_path, source_path_length + 1u);
        } else {
            borrow_path = (char*)malloc(1u);
            if (borrow_path) borrow_path[0] = '\0';
        }
        is_mutable = source->borrow_kind == 2u;
    } else if (argument_value->type == AST_UNARY_EXPR) {
        ASTUnaryExpr* borrow = (ASTUnaryExpr*)argument_value;
        if (borrow->op != TOKEN_BIT_AND && borrow->op != TOKEN_BORROW_MUT)
            return 0;
        owner = semantic_lvalue_root_slot(ctx, borrow->operand);
        borrow_path = semantic_lvalue_path(ctx, borrow->operand);
        is_mutable = borrow->op == TOKEN_BORROW_MUT;
    } else {
        return 0;
    }
    if (!owner || !borrow_path ||
        !semantic_is_reference_type_name(borrower->type_name) ||
        is_mutable != semantic_is_mut_reference_type_name(borrower->type_name) ||
        borrower->depth < owner->depth) {
        free(borrow_path);
        return 0;
    }
    if (semantic_borrow_conflicts(
            ctx, owner, borrow_path, is_mutable, borrower) ||
        (!is_mutable && semantic_shared_borrows_on_path(
            ctx, owner, borrow_path, borrower) == UINT32_MAX)) {
        free(borrow_path);
        return 0;
    }

    semantic_release_borrow_binding(borrower);
    if (is_mutable) {
        owner->mutable_borrow_active = 1u;
        borrower->borrow_kind = 2u;
    } else {
        ++owner->shared_borrow_count;
        borrower->borrow_kind = 1u;
    }
    borrower->borrowed_from = owner;
    borrower->borrow_path = borrow_path;
    return 1;
}

static void semantic_register_borrow_binding(
    SemanticContext* ctx, Slot* borrower, ASTNode* initializer
) {
    ASTUnaryExpr* borrow;
    Slot* owner;
    int is_mutable;
    if (!ctx || !borrower || !initializer ||
        !semantic_is_reference_type_name(borrower->type_name)) return;

    if (initializer->type == AST_IDENTIFIER) {
        ASTIdentifier* source_id = (ASTIdentifier*)initializer;
        Slot* source = find_slot(ctx, source_id->name);
        if (source && source != borrower &&
            semantic_is_reference_type_name(source->type_name) &&
            are_types_compatible(ctx, borrower->type_name, source->type_name)) {
            semantic_copy_shared_reference_binding(
                ctx, borrower, source, initializer);
        } else if (source == borrower) {
            report_error(ctx, initializer, ERR_SEMANTIC_TYPE_MISMATCH,
                "A reference binding cannot be initialized from itself.");
        }
        return;
    }
    if (initializer->type == AST_FUNC_CALL) {
        if (semantic_bind_returned_call_reference(ctx, borrower, initializer))
            return;
        report_error(ctx, initializer, ERR_SEMANTIC_TYPE_MISMATCH,
            "Reference-returning call has no supported, consistent reference-parameter origin; computed and unknown origins are rejected.");
        return;
    }
    if (initializer->type != AST_UNARY_EXPR) {
        const char* initializer_type = get_expr_type(ctx, initializer);
        if (semantic_is_reference_type_name(initializer_type)) {
            report_error(ctx, initializer, ERR_SEMANTIC_TYPE_MISMATCH,
                "Reference initialization requires a direct borrow, a tracked shared-reference binding, or a call with a verified reference-parameter return origin.");
        }
        return;
    }
    borrow = (ASTUnaryExpr*)initializer;
    if (borrow->op != TOKEN_BIT_AND && borrow->op != TOKEN_BORROW_MUT)
        return;
    /* The unary-borrow validator owns the diagnostic for non-lvalue operands.
       Only lvalues proceed to the separate provenance check below. */
    if (!semantic_is_lvalue(ctx, borrow->operand)) return;
    owner = semantic_lvalue_root_slot(ctx, borrow->operand);
    {
        char* projection_path = semantic_lvalue_path(ctx, borrow->operand);
        if (!projection_path) {
            report_error(ctx, (ASTNode*)borrow, ERR_SEMANTIC_TYPE_MISMATCH,
                "Safe borrow requires a statically identifiable storage path; pointer dereferences and computed lvalues are rejected.");
            return;
        }
        free(projection_path);
    }
    if (!owner) {
        report_error(ctx, (ASTNode*)borrow, ERR_SEMANTIC_TYPE_MISMATCH,
            "Safe borrow requires a local storage owner with statically known provenance; raw-pointer and computed lvalues are not supported.");
        return;
    }
    is_mutable = borrow->op == TOKEN_BORROW_MUT;
    if (borrower->depth < owner->depth) {
        report_error(ctx, (ASTNode*)borrow, ERR_SEMANTIC_TYPE_MISMATCH,
            "Reference binding cannot outlive its borrowed owner; the owner must be in the same or an outer scope.");
        return;
    }
    {
        char* projection_path = semantic_lvalue_path(ctx, borrow->operand);
        if (!projection_path) {
            report_error(ctx, (ASTNode*)borrow, ERR_SEMANTIC_TYPE_MISMATCH,
                "Unable to allocate borrow projection metadata.");
            return;
        }
        if (semantic_borrow_conflicts(
                ctx, owner, projection_path, is_mutable, borrower) ||
            (!is_mutable && semantic_shared_borrows_on_path(
                ctx, owner, projection_path, borrower) == UINT32_MAX)) {
            report_borrow_conflict(
                ctx, (ASTNode*)borrow, owner, is_mutable, projection_path,
                is_mutable ? "create an exclusive mutable borrow of"
                           : "create a shared borrow of");
            free(projection_path);
            return;
        }
        semantic_release_borrow_binding(borrower);
        if (is_mutable) {
            owner->mutable_borrow_active = 1u;
            borrower->borrow_kind = 2u;
        } else {
            ++owner->shared_borrow_count;
            borrower->borrow_kind = 1u;
        }
        borrower->borrowed_from = owner;
        borrower->borrow_path = projection_path;
    }
}

static void semantic_copy_shared_reference_binding(
    SemanticContext* ctx, Slot* target, Slot* source, ASTNode* assignment
) {
    Slot* owner;
    uint32_t effective_shared_count;
    char* copied_path = NULL;
    if (!ctx || !target || !source || !assignment || target == source) return;
    if (semantic_is_mut_reference_type_name(target->type_name)) {
        report_error(ctx, assignment, ERR_SEMANTIC_TYPE_MISMATCH,
            "An exclusive mutable reference cannot be copied; transfer or reborrow semantics are required.");
        return;
    }
    if (source->borrow_kind == 2u && source->borrowed_from) {
        report_error(ctx, assignment, ERR_SEMANTIC_TYPE_MISMATCH,
            "An exclusive mutable reference cannot be copied into another binding.");
        return;
    }
    owner = source->borrow_kind == 1u ? source->borrowed_from : NULL;
    if (owner) {
        const char* source_path = source->borrow_path ? source->borrow_path : "";
        size_t path_length = strlen(source_path);
        copied_path = (char*)malloc(path_length + 1u);
        if (copied_path) memcpy(copied_path, source_path, path_length + 1u);
        if (!copied_path) {
            report_error(ctx, assignment, ERR_SEMANTIC_TYPE_MISMATCH,
                "Unable to copy shared-reference projection metadata.");
            return;
        }
    }
    if (!owner) {
        Slot* origin = source->borrow_origin;
        if (!origin && (source->is_parameter || source->depth == 0))
            origin = source;
        if (origin && target->depth < origin->depth) {
            char message[320];
            (void)snprintf(message, sizeof(message),
                "Cannot store reference '%s' into longer-lived binding '%s': its origin '%s' is scoped at depth %d.",
                source->name ? source->name : "<reference>",
                target->name ? target->name : "<reference>",
                origin->name ? origin->name : "<origin>",
                origin->depth);
            report_issue(ctx, assignment, ERR_SEMANTIC_TYPE_MISMATCH, message,
                "Keep this reference in a binding whose lifetime does not exceed its parameter or global origin. Do not store a parameter-derived reference in global or outer-scope state.",
                "Keep this reference in a binding whose lifetime does not exceed its parameter or global origin. Do not store a parameter-derived reference in global or outer-scope state.",
                0, 0);
            return;
        }
        semantic_release_borrow_binding(target);
        target->borrow_origin = origin;
        return;
    }
    effective_shared_count = semantic_shared_borrows_on_path(
        ctx, owner, copied_path, target);
    if (semantic_borrow_conflicts(
            ctx, owner, copied_path, 0, target) ||
        effective_shared_count == UINT32_MAX) {
        report_error(ctx, assignment, ERR_SEMANTIC_TYPE_MISMATCH,
            "Cannot copy this shared reference because an overlapping exclusive borrow is active or the path borrow limit was reached.");
        free(copied_path);
        return;
    }
    semantic_release_borrow_binding(target);
    ++owner->shared_borrow_count;
    target->borrowed_from = owner;
    target->borrow_kind = 1u;
    target->borrow_path = copied_path;
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
            Slot* existing;
            if (!immutable->name || !immutable->name[0]) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "An immutable m: declaration requires a non-empty name.");
                break;
            }
            existing = find_slot(ctx, immutable->name);
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
            Slot* existing;
            if (!decl->name || !decl->name[0]) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Variable declaration requires a non-empty name.");
                break;
            }
            existing = find_slot(ctx, decl->name);
            int is_array = (decl->inline_type &&
                decl->inline_type->type == AST_ARRAY_TYPE);
            int is_inline = (decl->inline_type != NULL && !is_array);
            int valid_type = 0;
            int initializer_error_baseline = ctx->error_count;
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
            if (declared_slot && is_array &&
                semantic_type_contains_safe_reference(
                    ctx, decl->type_name, 0u)) {
                report_issue(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Array element type contains a safe reference or an aggregate that contains one; array-held safe references have no element-level owner/lifetime metadata.",
                    "Keep safe references in local bindings. Use a value element type, or use a raw pointer only when the program explicitly accepts its unsafe lifetime contract.",
                    "Keep safe references in local bindings. Use a value element type, or use a raw pointer only when the program explicitly accepts its unsafe lifetime contract.",
                    0, 0);
            } else if (declared_slot &&
                !semantic_is_reference_type_name(declared_slot->type_name) &&
                semantic_type_contains_safe_reference(
                    ctx, declared_slot->type_name, 0u)) {
                report_issue(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "This binding's NewType or aggregate contains safe-reference storage, but standalone alias/aggregate reference ownership is not implemented.",
                    "Use a direct local safe-reference binding, or redesign the stored value so the reference remains outside the aggregate/container.",
                    "Use a direct local safe-reference binding, or redesign the stored value so the reference remains outside the aggregate/container.",
                    0, 0);
            }
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

            if (decl->is_const && !decl->initializer) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "A c: compile-time constant requires an initializer expression.");
            }

            initializer_error_baseline = ctx->error_count;
            if (decl->initializer) {
                const char* init_type;
                semantic_check(ctx, decl->initializer);
                if (decl->is_const &&
                    !semantic_is_compile_time_constant_expression(
                        ctx, decl->initializer)) {
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Initializer for c: must be a compile-time constant expression; runtime calls and non-constant values are not allowed.");
                }
                init_type = get_expr_type(ctx, decl->initializer);
                if (((declared_slot && semantic_type_is_any(
                        ctx, declared_slot->type_name)) ||
                     (declared_slot && semantic_is_array_type_name(
                        declared_slot->type_name)) ||
                     (declared_slot && declared_slot->type_name &&
                      strcmp(declared_slot->type_name, "tuple") == 0) ||
                     (decl->initializer->type == AST_STRUCT_LITERAL) ||
                     (decl->initializer->type == AST_TUPLE)) &&
                    semantic_expression_contains_safe_reference(
                        ctx, decl->initializer, 0u)) {
                    report_issue(ctx, decl->initializer,
                        ERR_SEMANTIC_TYPE_MISMATCH,
                        "A safe reference cannot be stored in any, an array, a tuple, or an aggregate initializer because element/field-level borrow provenance is unavailable.",
                        "Keep the safe reference in a direct local binding; store only values or explicitly managed raw pointers in dynamic/aggregate containers.",
                        "Keep the safe reference in a direct local binding; store only values or explicitly managed raw pointers in dynamic/aggregate containers.",
                        0, 0);
                }
                if (!is_array && semantic_is_array_type_name(init_type) &&
                    semantic_type_contains_safe_reference(ctx, init_type, 0u)) {
                    report_issue(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Inferred array initializer contains safe references; array-held reference provenance is not supported.",
                        "Keep each safe reference in a local binding instead of storing references inside an inferred array.",
                        "Keep each safe reference in a local binding instead of storing references inside an inferred array.",
                        0, 0);
                }
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
                    int named_function_initializer = 0;
                    if (declared_slot &&
                        semantic_is_function_type_name(ctx, declared_slot->type_name) &&
                        decl->initializer->type == AST_IDENTIFIER) {
                        Slot* function_slot = find_slot(ctx,
                            ((ASTIdentifier*)decl->initializer)->name);
                        if (function_slot && function_slot->kind == SLOT_FUNC) {
                            named_function_initializer = 1;
                            if (!semantic_function_matches_callback_type(
                                    ctx, declared_slot, function_slot)) {
                                report_issue(ctx, decl->initializer,
                                    ERR_SEMANTIC_TYPE_MISMATCH,
                                    "Named function does not match the callback parameter names, parameter types, optionality, variadic contract, or return type.",
                                    "Use a function whose complete signature matches the callback type, including parameter names used by named calls.",
                                    "Use a function whose complete signature matches the callback type, including parameter names used by named calls.",
                                    0, 0);
                            } else if (function_slot->depth > declared_slot->depth) {
                                report_issue(ctx, decl->initializer,
                                    ERR_SEMANTIC_TYPE_MISMATCH,
                                    "Callback target is declared in a shorter-lived lexical scope than the callback binding.",
                                    "Keep the callback binding within the function declaration's scope, or move the function declaration to an outer scope.",
                                    "Keep the callback binding within the function declaration's scope, or move the function declaration to an outer scope.",
                                    0, 0);
                            }
                        }
                    }
                    size_t vector_dimension = 0u;
                    if (semantic_vector_dimension(decl->type_name,
                            &vector_dimension) &&
                        decl->initializer &&
                        decl->initializer->type == AST_ARRAY_LITERAL) {
                        semantic_validate_vector_initializer(
                            ctx, node, decl->type_name, decl->initializer);
                    } else if (!named_function_initializer && init_type &&
                        !are_types_compatible(ctx, decl->type_name, init_type)) {
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

            if (declared_slot && decl->initializer &&
                ctx->error_count == initializer_error_baseline) {
                semantic_register_borrow_binding(
                    ctx, declared_slot, decl->initializer);
            }

            if (declared_slot &&
                semantic_is_reference_type_name(declared_slot->type_name)) {
                if (decl->initializer &&
                    ctx->error_count == initializer_error_baseline) {
                    declared_slot->is_initialized = 1u;
                } else if (!decl->initializer && declared_slot->depth == 0) {
                    report_issue(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Global safe reference declaration requires an initializer; global reference storage cannot begin with an unbound value.",
                        "Initialize the global reference from a valid global/static owner. Local safe references may be declared first and assigned before their first read.",
                        "Initialize the global reference from a valid global/static owner. Local safe references may be declared first and assigned before their first read.",
                        0, 0);
                }
            } else if (declared_slot &&
                semantic_is_raw_pointer_type_name(declared_slot->type_name)) {
                if (decl->initializer &&
                    ctx->error_count == initializer_error_baseline) {
                    declared_slot->is_initialized = 1u;
                    declared_slot->is_nullable =
                        (uint8_t)semantic_expression_may_be_null(
                            ctx, decl->initializer);
                } else if (!decl->initializer && declared_slot->depth == 0) {
                    declared_slot->is_initialized = 1u;
                    declared_slot->is_nullable = 1u;
                }
            } else if (declared_slot &&
                semantic_is_function_type_name(ctx, declared_slot->type_name)) {
                if (decl->initializer &&
                    ctx->error_count == initializer_error_baseline) {
                    declared_slot->is_initialized = 1u;
                    declared_slot->is_nullable =
                        (uint8_t)semantic_expression_may_be_null(
                            ctx, decl->initializer);
                    declared_slot->callable_declaration = NULL;
                    if (decl->initializer->type == AST_IDENTIFIER) {
                        Slot* source_callback = find_slot(ctx,
                            ((ASTIdentifier*)decl->initializer)->name);
                        if (source_callback && source_callback->kind == SLOT_FUNC &&
                            semantic_function_matches_callback_type(
                                ctx, declared_slot, source_callback) &&
                            source_callback->depth <= declared_slot->depth) {
                            declared_slot->callable_declaration =
                                source_callback->declaration_node;
                        } else if (source_callback &&
                            source_callback->kind == SLOT_VAR &&
                            semantic_is_function_type_name(
                                ctx, source_callback->type_name)) {
                            declared_slot->callable_declaration =
                                source_callback->callable_declaration;
                        }
                    }
                } else if (!decl->initializer && declared_slot->depth == 0) {
                    declared_slot->is_initialized = 1u;
                    declared_slot->is_nullable = 1u;
                    declared_slot->callable_declaration = NULL;
                }
            }

            if (declared_slot) {
                semantic_record_annotation(
                    ctx, node, declared_slot, declared_slot->type_name,
                    SEMANTIC_RESULT_VALUE_LVALUE);
            }
            break;
        }

        case AST_ARRAY_LITERAL: {
            ASTNode* element;
            for (element = ((ASTArrayLiteral*)node)->elements;
                 element; element = element->next) {
                const char* element_type = get_expr_type(ctx, element);
                if (semantic_type_contains_safe_reference(
                        ctx, element_type, 0u)) {
                    report_issue(ctx, element, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Array literal element contains a safe reference or an aggregate with safe-reference storage; container-held reference provenance is unsupported.",
                        "Keep safe references in local bindings rather than storing them in array elements.",
                        "Keep safe references in local bindings rather than storing them in array elements.",
                        0, 0);
                }
            }
            semantic_check(ctx,((ASTArrayLiteral*)node)->elements);
            break;
        }
        case AST_NAMED_ARG: {
            ASTNamedArg* named=(ASTNamedArg*)node;
            if (!named->name || !named->name[0] || !named->value) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Named argument requires a non-empty name and value expression.");
                break;
            }
            semantic_check(ctx,named->value);
            (void)get_expr_type(ctx,node);
            break;
        }
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
            ++ctx->borrow_operand_depth;
            semantic_check(ctx, access->target);
            --ctx->borrow_operand_depth;
            semantic_check(ctx, access->index);
            {
                Slot* owner = semantic_lvalue_root_slot(ctx, node);
                char* path = semantic_lvalue_path(ctx, node);
                if (owner && ctx->borrow_operand_depth == 0u &&
                    ctx->assignment_target_depth == 0u &&
                    (!path ||
                     semantic_borrow_conflicts(ctx, owner, path, 0, NULL))) {
                    report_error(ctx, node, ERR_SEMANTIC_CONST_ASSIGN,
                        "Cannot read this indexed element while an overlapping exclusive mutable borrow is active.");
                }
                free(path);
            }
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
            size_t vector_dimension = 0u;
            int target_is_vector =
                semantic_vector_dimension(target_type, &vector_dimension);
            if (target_type && strcmp(target_type,"tuple") != 0 &&
                !semantic_is_array_type_name(target_type) &&
                !semantic_representation_element_type(target_type) &&
                !semantic_is_map_type_name(target_type) &&
                !target_is_vector) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Index target must be an array, map, tuple, bit vector, or byte block.");
            }
            index_type = get_expr_type(ctx, access->index);
            if (semantic_is_map_type_name(target_type) &&
                index_type && !semantic_is_map_key_type(ctx, index_type)) {
                report_issue(ctx, access->index, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Map key type is not hashable under the Nexus Flow Map contract.",
                    "Use a numeric scalar, string, char, bool, enum, or a NewType whose underlying type is one of those.",
                    "Use a numeric scalar, string, char, bool, enum, or a NewType whose underlying type is one of those.",
                    0, 0);
            } else if (!semantic_is_map_type_name(target_type) &&
                index_type && strcmp(index_type, "any") != 0 &&
                !is_integer_primitive_type(index_type)) {
                report_error(ctx, access->index, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Index expression must have an integer type.");
            } else if (target_is_vector &&
                semantic_eval_integer_constant(access->index, &constant_index) &&
                (constant_index < 0 ||
                 (uint64_t)constant_index >= (uint64_t)vector_dimension)) {
                report_error(ctx, access->index, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Vector component index is outside the Vec2/Vec3/Vec4 dimension.");
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
            if (resolved && resolved->is_moved) {
                char moved_message[256];
                (void)snprintf(moved_message, sizeof(moved_message),
                    "Use of '%s' after ownership was transferred is invalid.",
                    id->name ? id->name : "<unnamed>");
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, moved_message);
            } else if (resolved && resolved->kind == SLOT_VAR &&
                (semantic_is_reference_type_name(resolved->type_name) ||
                 semantic_is_raw_pointer_type_name(resolved->type_name) ||
                 semantic_is_function_type_name(ctx, resolved->type_name)) &&
                !resolved->is_initialized &&
                ctx->assignment_target_depth == 0u) {
                char message[320];
                (void)snprintf(message, sizeof(message),
                    "Pointer/reference binding '%s' is read before it has been initialized.",
                    id->name ? id->name : "<unnamed>");
                report_issue(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message,
                    "Assign a valid pointer/reference value before reading, dereferencing, passing, or returning this binding.",
                    "Assign a valid pointer/reference value before reading, dereferencing, passing, or returning this binding.",
                    0, 0);
            } else if (resolved &&
                ctx->borrow_operand_depth == 0u &&
                ctx->assignment_target_depth == 0u &&
                ctx->assignment_borrow_guard_owner != resolved &&
                semantic_borrow_conflicts(ctx, resolved, "", 0, NULL)) {
                report_error(ctx, node, ERR_SEMANTIC_CONST_ASSIGN,
                    "Cannot read this owner while an overlapping exclusive mutable borrow is active.");
            } else if (!resolved) {
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
            } else if (!semantic_register_thread_capture(
                    ctx, resolved, node)) {
                break;
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
            if (!bin->left || !bin->right) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Binary expression requires both left and right operands.");
                break;
            }

            semantic_check(ctx, bin->left);
            semantic_check(ctx, bin->right);

            if (is_arithmetic_binary_operator(bin->op)) {
                const char* left_type = get_expr_type(ctx, bin->left);
                const char* right_type = get_expr_type(ctx, bin->right);

                if (left_type && right_type) {
                    const char* operator_name =
                        semantic_binary_operator_name(bin->op);
                    char message[320];
                    if (!are_types_compatible(ctx, left_type, right_type)) {
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

            if (bin->op == TOKEN_RELOCATE) {
                Slot* source_slot = NULL;
                Slot* target_slot = NULL;
                if (bin->left->type == AST_IDENTIFIER)
                    source_slot = find_slot(ctx, ((ASTIdentifier*)bin->left)->name);
                if (bin->right->type == AST_IDENTIFIER)
                    target_slot = find_slot(ctx, ((ASTIdentifier*)bin->right)->name);
                if (!source_slot || !target_slot) {
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Relocation requires declared source and destination identifiers.");
                } else if (source_slot == target_slot) {
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Relocation source and destination must be different variables.");
                } else if (source_slot->is_moved) {
                    report_error(ctx, bin->left, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Relocation source has already been moved.");
                } else if (source_slot->type_name && target_slot->type_name &&
                    !are_types_compatible(ctx, source_slot->type_name,
                        target_slot->type_name)) {
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Relocation source and destination types are incompatible.");
                } else {
                    source_slot->is_moved = 1;
                    semantic_record_annotation(ctx, node, target_slot,
                        target_slot->type_name, SEMANTIC_RESULT_VALUE_RVALUE);
                }
            }

            if (bin->op == TOKEN_RANGE) {
                const char* begin_type = get_expr_type(ctx, bin->left);
                const char* end_type = get_expr_type(ctx, bin->right);
                if (begin_type && end_type &&
                    (!are_types_compatible(ctx, begin_type, end_type) ||
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
                    (!are_types_compatible(ctx, left_type, right_type) ||
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
                    !are_types_compatible(ctx, left_type, right_type)) {
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

            int assignment_error_baseline = ctx->error_count;
            if (assign->op != TOKEN_ASSIGN && assign->op != TOKEN_FORCE_ASSIGN &&
                semantic_lvalue_is_const(ctx, assign->left)) {
                report_error(ctx, node, ERR_SEMANTIC_CONST_ASSIGN,
                    "Cannot modify a constant or immutable value with compound assignment.");
            }
            {
                Slot* owner_slot = semantic_lvalue_root_slot(ctx, assign->left);
                char* target_path = semantic_lvalue_path(ctx, assign->left);
                if (owner_slot && (!target_path ||
                    semantic_borrow_conflicts(
                        ctx, owner_slot, target_path, 1, NULL))) {
                    report_error(ctx, node, ERR_SEMANTIC_CONST_ASSIGN,
                        "Cannot modify this storage path while an overlapping shared or exclusive borrow is active.");
                }
                free(target_path);
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
                    int named_function_assignment = 0;
                    if (semantic_type_is_any(ctx, slot->type_name) &&
                        semantic_expression_contains_safe_reference(
                            ctx, assign->right, 0u)) {
                        report_issue(ctx, assign->right,
                            ERR_SEMANTIC_TYPE_MISMATCH,
                            "A safe reference cannot be assigned to any because its owner and lifetime provenance would be erased.",
                            "Keep the destination typed as &T/&mut T, or store a value/raw pointer instead.",
                            "Keep the destination typed as &T/&mut T, or store a value/raw pointer instead.",
                            0, 0);
                    }
                    if (semantic_is_function_type_name(ctx, slot->type_name) &&
                        assign->right->type == AST_IDENTIFIER) {
                        Slot* function_slot = find_slot(ctx,
                            ((ASTIdentifier*)assign->right)->name);
                        if (function_slot && function_slot->kind == SLOT_FUNC) {
                            named_function_assignment = 1;
                            if (!semantic_function_matches_callback_type(
                                    ctx, slot, function_slot)) {
                                report_issue(ctx, assign->right,
                                    ERR_SEMANTIC_TYPE_MISMATCH,
                                    "Named function does not match the callback parameter names, parameter types, optionality, variadic contract, or return type.",
                                    "Assign a function whose complete signature matches the callback type.",
                                    "Assign a function whose complete signature matches the callback type.",
                                    0, 0);
                            }
                        }
                    }
                    if (!named_function_assignment && right_type &&
                        !are_types_compatible(ctx, slot->type_name, right_type)) {
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
                    !are_types_compatible(ctx, element_type, right_type)) {
                    char message[320];
                    (void)snprintf(message, sizeof(message),
                        "Operator '%s' cannot assign value of type '%s' to an indexed element of type '%s'.",
                        semantic_binary_operator_name(assign->op), right_type,
                        element_type);
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
                }
            } else if (assign->left->type == AST_UNARY_EXPR &&
                       ((ASTUnaryExpr*)assign->left)->op == TOKEN_STAR) {
                const char* target_type = get_expr_type(ctx, assign->left);
                const char* right_type = get_expr_type(ctx, assign->right);
                int is_shared_reference = 0;
                (void)semantic_dereference_target_type(
                    get_expr_type(ctx,
                        ((ASTUnaryExpr*)assign->left)->operand),
                    &is_shared_reference);
                if (is_shared_reference) {
                    report_error(ctx, node, ERR_SEMANTIC_CONST_ASSIGN,
                        "Cannot assign through an immutable shared reference.");
                }
                if (target_type && right_type &&
                    !are_types_compatible(ctx, target_type, right_type)) {
                    char message[320];
                    (void)snprintf(message, sizeof(message),
                        "Cannot assign value of type '%s' through pointer to '%s'.",
                        right_type, target_type);
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
                            !are_types_compatible(ctx, field->type_name, right_type)) {
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

            if ((assign->op == TOKEN_ASSIGN ||
                 assign->op == TOKEN_FORCE_ASSIGN) &&
                (assign->left->type == AST_MEMBER_ACCESS ||
                 assign->left->type == AST_INDEX_ACCESS ||
                 (assign->left->type == AST_UNARY_EXPR &&
                  ((ASTUnaryExpr*)assign->left)->op == TOKEN_STAR))) {
                const char* stored_type = get_expr_type(ctx, assign->right);
                const char* target_type = get_expr_type(ctx, assign->left);
                if (semantic_expression_contains_safe_reference(
                        ctx, assign->right, 0u) &&
                    !semantic_type_contains_safe_reference(ctx, target_type, 0u)) {
                    report_issue(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Safe references cannot be stored through this member/index/pointer target because its storage does not track reference owner and lifetime provenance.",
                        "Keep the reference in a local binding. Do not store it in Map/Vec/any fields or through a raw pointer.",
                        "Keep the reference in a local binding. Do not store it in Map/Vec/any fields or through a raw pointer.",
                        0, 0);
                }
            }
            if (assign->left &&
                assign->left->type != AST_IDENTIFIER &&
                assign->left->type != AST_MEMBER_ACCESS &&
                assign->left->type != AST_INDEX_ACCESS &&
                !(assign->left->type == AST_UNARY_EXPR &&
                  ((ASTUnaryExpr*)assign->left)->op == TOKEN_STAR)) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Assignment target must be a mutable identifier, member, index, or pointer dereference.");
            }
            if (assign->op != TOKEN_ASSIGN && assign->op != TOKEN_FORCE_ASSIGN) {
                /*
                 * Compound assignment writes through its target just like
                 * ordinary assignment. Preserve immutable shared-reference
                 * semantics for every compound operator, not only '='.
                 */
                if (assign->left->type == AST_UNARY_EXPR &&
                    ((ASTUnaryExpr*)assign->left)->op == TOKEN_STAR) {
                    int is_shared_reference = 0;
                    (void)semantic_dereference_target_type(
                        get_expr_type(ctx,
                            ((ASTUnaryExpr*)assign->left)->operand),
                        &is_shared_reference);
                    if (is_shared_reference) {
                        report_error(ctx, node, ERR_SEMANTIC_CONST_ASSIGN,
                            "Cannot modify a value through an immutable shared reference.");
                    }
                }
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
                        (!are_types_compatible(ctx, left_type, right_type) ||
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
            {
                Slot* previous_guard_owner = ctx->assignment_borrow_guard_owner;
                Slot* target_owner = semantic_lvalue_root_slot(ctx, assign->left);
                ++ctx->assignment_target_depth;
                semantic_check(ctx, assign->left);
                --ctx->assignment_target_depth;
                {
                    char* target_path = semantic_lvalue_path(ctx, assign->left);
                    if (target_owner && (!target_path ||
                        semantic_borrow_conflicts(
                            ctx, target_owner, target_path, 1, NULL))) {
                        ctx->assignment_borrow_guard_owner = target_owner;
                    }
                    free(target_path);
                }
                semantic_check(ctx, assign->right);
                ctx->assignment_borrow_guard_owner = previous_guard_owner;
            }
            if (assign->left->type == AST_IDENTIFIER) {
                ASTIdentifier* target_id = (ASTIdentifier*)assign->left;
                Slot* target_slot = find_slot(ctx, target_id->name);
                if (target_slot &&
                    semantic_is_reference_type_name(target_slot->type_name)) {
                    const char* source_type = get_expr_type(ctx, assign->right);
                    if (source_type && target_slot->type_name &&
                        are_types_compatible(ctx, target_slot->type_name, source_type)) {
                        if (assign->right->type == AST_UNARY_EXPR &&
                            (((ASTUnaryExpr*)assign->right)->op == TOKEN_BIT_AND ||
                             ((ASTUnaryExpr*)assign->right)->op == TOKEN_BORROW_MUT)) {
                            if (ctx->error_count == assignment_error_baseline) {
                                semantic_register_borrow_binding(
                                    ctx, target_slot, assign->right);
                            }
                        } else if (assign->right->type == AST_IDENTIFIER) {
                            ASTIdentifier* source_id = (ASTIdentifier*)assign->right;
                            Slot* source_slot = find_slot(ctx, source_id->name);
                            if (source_slot &&
                                semantic_is_reference_type_name(source_slot->type_name) &&
                                !semantic_is_mut_reference_type_name(source_slot->type_name) &&
                                !semantic_is_mut_reference_type_name(target_slot->type_name)) {
                                semantic_copy_shared_reference_binding(
                                    ctx, target_slot, source_slot, node);
                            } else {
                                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                                    "Only shared references may be copied; exclusive mutable references cannot be duplicated.");
                            }
                        } else {
                            report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                                "Safe reference reassignment requires a borrow expression or a copyable shared-reference binding.");
                        }
                        if (ctx->error_count == assignment_error_baseline)
                            target_slot->is_initialized = 1u;
                    }
                } else if (target_slot &&
                    semantic_is_raw_pointer_type_name(target_slot->type_name)) {
                    const char* source_type = get_expr_type(ctx, assign->right);
                    if (source_type &&
                        are_types_compatible(ctx, target_slot->type_name, source_type) &&
                        ctx->error_count == assignment_error_baseline)
                        target_slot->is_initialized = 1u;
                    if (ctx->error_count == assignment_error_baseline)
                        target_slot->is_nullable =
                            (uint8_t)semantic_expression_may_be_null(
                                ctx, assign->right);
                } else if (target_slot &&
                    semantic_is_function_type_name(ctx, target_slot->type_name)) {
                    const char* source_type = get_expr_type(ctx, assign->right);
                    int compatible_function_value = 0;
                    if (assign->right->type == AST_IDENTIFIER) {
                        Slot* source_slot = find_slot(ctx,
                            ((ASTIdentifier*)assign->right)->name);
                        if (source_slot && source_slot->kind == SLOT_FUNC) {
                            compatible_function_value =
                                semantic_function_matches_callback_type(
                                    ctx, target_slot, source_slot);
                        } else if (source_slot &&
                            semantic_is_function_type_name(ctx, source_slot->type_name)) {
                            compatible_function_value =
                                are_types_compatible(ctx, target_slot->type_name,
                                    source_slot->type_name);
                        }
                    }
                    if ((compatible_function_value ||
                         (source_type && are_types_compatible(
                            ctx, target_slot->type_name, source_type))) &&
                        ctx->error_count == assignment_error_baseline) {
                        target_slot->is_initialized = 1u;
                        target_slot->is_nullable =
                            (uint8_t)semantic_expression_may_be_null(
                                ctx, assign->right);
                        target_slot->callable_declaration = NULL;
                        if (assign->right->type == AST_IDENTIFIER) {
                            Slot* source_callback = find_slot(ctx,
                                ((ASTIdentifier*)assign->right)->name);
                            if (source_callback && source_callback->kind == SLOT_FUNC &&
                                semantic_function_matches_callback_type(
                                    ctx, target_slot, source_callback) &&
                                source_callback->depth <= target_slot->depth) {
                                target_slot->callable_declaration =
                                    source_callback->declaration_node;
                            } else if (source_callback &&
                                source_callback->kind == SLOT_VAR &&
                                semantic_is_function_type_name(
                                    ctx, source_callback->type_name)) {
                                target_slot->callable_declaration =
                                    source_callback->callable_declaration;
                            }
                        }
                    }
                }
            } else {
                const char* target_type = get_expr_type(ctx, assign->left);
                if (semantic_is_reference_type_name(target_type)) {
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Reassigning a stored reference through a member, index, or pointer dereference is not supported; use a local reference binding.");
                }
            }
            break;
        }

        case AST_NEXUS_CAPTURE: {
            ASTNexusCapture* capture = (ASTNexusCapture*)node;
            Slot* handler_slot;
            const char* captured_type;

            /* Evaluate source before introducing destination name.
               This prevents self-reference to an uninitialized Handler. */
            if (!capture->handler_name || !capture->handler_name[0] ||
                !capture->expr) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Nexus capture requires a non-empty handler name and source expression.");
                break;
            }
            semantic_check(ctx, capture->expr);
            captured_type = get_expr_type(ctx, capture->expr);

            if (find_slot(ctx, capture->handler_name)) {
                char buf[256];
                snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_REDEFINED),
                    capture->handler_name);
                report_error(ctx, node, ERR_SEMANTIC_REDEFINED, buf);
                break;
            }

            handler_slot = add_slot(
                ctx, capture->handler_name, SLOT_VAR, 0, captured_type,
                node, 1, 0, 0, 0u);
            if (handler_slot) {
                semantic_record_annotation(
                    ctx, node, handler_slot, handler_slot->type_name,
                    SEMANTIC_RESULT_VALUE_LVALUE);
            }
            break;
        }

        case AST_NEXUS_PIPE: {
            ASTNexusFlow* pipe = (ASTNexusFlow*)node;
            if (!pipe->left || !pipe->right) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Nexus pipe requires both left and right operands.");
                break;
            }
            semantic_check(ctx, pipe->left);
            semantic_check(ctx, pipe->right);
            if (pipe->catch_block) semantic_check(ctx, pipe->catch_block);
            break;
        }

        case AST_NEXUS_FLOW: {
            ASTNexusFlow* flow = (ASTNexusFlow*)node;
            if (!flow->left || !flow->right) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Nexus Flow operation requires both left and right operands.");
                break;
            }
            semantic_check(ctx, flow->left);
            semantic_check(ctx, flow->right);
            if (flow->catch_block) semantic_check(ctx, flow->catch_block);
            break;
        }
        case AST_SELECT: {
            ASTSelect* select=(ASTSelect*)node;
            SemanticReferenceInitSnapshot before_arms = {0};
            SemanticReferenceInitSnapshot merged_arms = {0};
            SemanticReferenceInitSnapshot arm_state = {0};
            int before_captured = 0;
            int merged_captured = 0;
            int all_arms_captured = 1;
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
            before_captured = semantic_reference_init_snapshot_capture(
                ctx, &before_arms);
            for(arm_node=select->arms;arm_node;arm_node=arm_node->next) {
                ASTSelectArm* arm;
                if (before_captured)
                    semantic_reference_init_snapshot_restore(&before_arms);
                if(arm_node->type!=AST_SELECT_ARM) {
                    report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "select contains an invalid arm node.");
                    all_arms_captured = 0;
                    continue;
                }
                arm=(ASTSelectArm*)arm_node;
                if(!arm->body) {
                    report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "select arm is missing its body.");
                    all_arms_captured = 0;
                    continue;
                }
                if(arm->is_default) {
                    if(arm->pattern) {
                        report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                            "select default arm must not contain a match pattern.");
                        all_arms_captured = 0;
                    }
                    if(seen_default) {
                        report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                            "select may contain only one default arm.");
                        all_arms_captured = 0;
                    }
                    seen_default=1;
                    if(arm_node->next) {
                        report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                            "select default arm must be the final arm.");
                        all_arms_captured = 0;
                    }
                } else if(!arm->pattern) {
                    report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "select match arm is missing its pattern.");
                    all_arms_captured = 0;
                    continue;
                }
                enter_scope(ctx,arm_node);
                semantic_check_pattern_bindings(ctx,arm->pattern,subject_type);
                semantic_check(ctx,arm->body);
                exit_scope(ctx);
                if (before_captured) {
                    memset(&arm_state, 0, sizeof(arm_state));
                    if (semantic_reference_init_snapshot_capture(ctx, &arm_state)) {
                        if (!merged_captured) {
                            merged_arms = arm_state;
                            memset(&arm_state, 0, sizeof(arm_state));
                            merged_captured = 1;
                        } else {
                            semantic_reference_init_snapshot_merge(
                                &merged_arms, &arm_state);
                        }
                    } else {
                        all_arms_captured = 0;
                    }
                    semantic_reference_init_snapshot_destroy(&arm_state);
                }
            }
            if (before_captured) {
                if (merged_captured && all_arms_captured) {
                    /* Without a default, no arm may match; include the
                       unchanged incoming state as an additional control path. */
                    if (!seen_default)
                        semantic_reference_init_snapshot_merge(
                            &merged_arms, &before_arms);
                    semantic_reference_init_snapshot_restore(&merged_arms);
                } else {
                    semantic_reference_init_snapshot_restore(&before_arms);
                }
            }
            semantic_reference_init_snapshot_destroy(&before_arms);
            semantic_reference_init_snapshot_destroy(&merged_arms);
            semantic_reference_init_snapshot_destroy(&arm_state);
            break;
        }

        case AST_SCAN: {
            ASTScan* scan=(ASTScan*)node;
            SemanticReferenceInitSnapshot before_arms = {0};
            SemanticReferenceInitSnapshot merged_arms = {0};
            SemanticReferenceInitSnapshot arm_state = {0};
            int before_captured = 0;
            int merged_captured = 0;
            int all_arms_captured = 1;
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
            before_captured = semantic_reference_init_snapshot_capture(
                ctx, &before_arms);
            for(arm_node=scan->arms;arm_node;arm_node=arm_node->next) {
                ASTScanArm* arm;
                if (before_captured)
                    semantic_reference_init_snapshot_restore(&before_arms);
                if(arm_node->type!=AST_SCAN_ARM) {
                    report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "scan contains an invalid arm node.");
                    all_arms_captured = 0;
                    continue;
                }
                arm=(ASTScanArm*)arm_node;
                if(!arm->body) {
                    report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                        "scan arm is missing its body.");
                    all_arms_captured = 0;
                    continue;
                }
                if(!arm->pattern) {
                    if(seen_else) {
                        report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                            "scan may contain only one else arm.");
                        all_arms_captured = 0;
                    }
                    seen_else=1;
                    if(arm_node->next) {
                        report_error(ctx,arm_node,ERR_SEMANTIC_TYPE_MISMATCH,
                            "scan else arm must be the final arm.");
                        all_arms_captured = 0;
                    }
                }
                enter_scope(ctx,arm_node);
                semantic_check_pattern_bindings(ctx,arm->pattern,subject_type);
                semantic_check(ctx,arm->body);
                exit_scope(ctx);
                if (before_captured) {
                    memset(&arm_state, 0, sizeof(arm_state));
                    if (semantic_reference_init_snapshot_capture(ctx, &arm_state)) {
                        if (!merged_captured) {
                            merged_arms = arm_state;
                            memset(&arm_state, 0, sizeof(arm_state));
                            merged_captured = 1;
                        } else {
                            semantic_reference_init_snapshot_merge(
                                &merged_arms, &arm_state);
                        }
                    } else {
                        all_arms_captured = 0;
                    }
                    semantic_reference_init_snapshot_destroy(&arm_state);
                }
            }
            if (before_captured) {
                if (merged_captured && all_arms_captured) {
                    /* Without else, a value may match no arm; include the
                       unchanged incoming state as an additional path. */
                    if (!seen_else)
                        semantic_reference_init_snapshot_merge(
                            &merged_arms, &before_arms);
                    semantic_reference_init_snapshot_restore(&merged_arms);
                } else {
                    semantic_reference_init_snapshot_restore(&before_arms);
                }
            }
            semantic_reference_init_snapshot_destroy(&before_arms);
            semantic_reference_init_snapshot_destroy(&merged_arms);
            semantic_reference_init_snapshot_destroy(&arm_state);
            semantic_check_enum_scan_coverage(ctx,scan,subject_type);
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
            int params_valid=1;
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
                    params_valid=0;
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
                if(params_valid)
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
            Slot* source_slot=NULL;
            Slot* target_slot=NULL;
            const char* source_type;
            const char* target_type;
            semantic_check(ctx,relocate->source);
            semantic_check(ctx,relocate->target);
            source_type=get_expr_type(ctx,relocate->source);
            target_type=get_expr_type(ctx,relocate->target);
            if(relocate->source && relocate->source->type==AST_IDENTIFIER)
                source_slot=find_slot(ctx,((ASTIdentifier*)relocate->source)->name);
            if(relocate->target && relocate->target->type==AST_IDENTIFIER)
                target_slot=find_slot(ctx,((ASTIdentifier*)relocate->target)->name);
            if(!relocate->source || relocate->source->type!=AST_IDENTIFIER ||
               !relocate->target || relocate->target->type!=AST_IDENTIFIER) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Relocation requires identifier source and destination operands.");
            } else if(!source_slot || !target_slot) {
                /* Identifier resolution already reports the undefined name. */
            } else if(source_slot==target_slot) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Relocation source and destination must be different variables.");
            } else if(source_type && target_type &&
                      !are_types_compatible(ctx, source_type,target_type)) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Relocate source and destination types must match.");
            } else {
                source_slot->is_moved=1;
                semantic_record_annotation(ctx,node,target_slot,
                    target_type?target_type:"any",SEMANTIC_RESULT_VALUE_RVALUE);
            }
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
            if (!function_name || !*function_name) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Function declaration requires a non-empty name.");
                break;
            }
            if (node->type == AST_FUNC_DECL && !func->body) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Function declaration is missing its body.");
                break;
            }
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
            if (func->return_type &&
                semantic_type_pattern_contains_variable(func->return_type)) {
                ASTNode* generic_parameter;
                int return_variable_is_bound = 0;
                for (generic_parameter = func->params; generic_parameter;
                     generic_parameter = generic_parameter->next) {
                    if (generic_parameter->type == AST_VAR_DECL &&
                        semantic_type_pattern_contains_variable(
                            ((ASTVarDecl*)generic_parameter)->type_name)) {
                        return_variable_is_bound = 1;
                        break;
                    }
                }
                if (!return_variable_is_bound) {
                    report_error(ctx, node, ERR_SEMANTIC_UNKNOWN_TYPE,
                        "Function return type contains unbound type variable 't'; no parameter supplies an inference source.");
                    return_type_valid = 0;
                }
            }
            if (func->return_type &&
                !semantic_is_reference_type_name(func->return_type) &&
                semantic_type_contains_safe_reference(
                    ctx, func->return_type, 0u)) {
                report_issue(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Function return type hides safe-reference storage inside a NewType, aggregate, or array; the return-origin contract cannot represent it.",
                    "Return a direct safe-reference type with a verifiable parameter/global origin, or return a value type.",
                    "Return a direct safe-reference type with a verifiable parameter/global origin, or return a value type.",
                    0, 0);
                return_type_valid = 0;
            }
            if (node->type == AST_EXTERN_DECL && func->return_type &&
                semantic_type_or_callback_contract_contains_safe_reference(
                    ctx, func->return_type, 0u)) {
                report_issue(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "An extern function cannot return a safe reference or callback type that contains one; the external implementation has no verifiable borrow-origin contract.",
                    "Return a value or raw pointer from the extern declaration. Convert it to a safe reference only inside a Nexus Flow wrapper with a verified owner.",
                    "Return a value or raw pointer from the extern declaration. Convert it to a safe reference only inside a Nexus Flow wrapper with a verified owner.",
                    0, 0);
                return_type_valid = 0;
            }
            if (func->return_array_type && func->return_type &&
                semantic_type_contains_safe_reference(
                    ctx, func->return_type, 0u)) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Array return elements cannot contain safe references because element-level owner/lifetime metadata is unavailable.");
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
            if (func->params) {
                ASTNode* parameter_node;
                uint32_t parameter_index = 0u;
                semantic_check(ctx, func->params);
                if (node->type == AST_EXTERN_DECL) {
                    ASTNode* extern_parameter;
                    for (extern_parameter = func->params; extern_parameter;
                         extern_parameter = extern_parameter->next) {
                        if (extern_parameter->type == AST_VAR_DECL) {
                            ASTVarDecl* parameter = (ASTVarDecl*)extern_parameter;
                            if (semantic_type_or_callback_contract_contains_safe_reference(
                                    ctx, parameter->type_name, 0u)) {
                                report_issue(ctx, extern_parameter,
                                    ERR_SEMANTIC_TYPE_MISMATCH,
                                    "An extern parameter cannot expose a safe reference or a callback signature containing safe references.",
                                    "Use a value/raw-pointer parameter, or keep the safe-reference operation inside a typed Nexus Flow wrapper.",
                                    "Use a value/raw-pointer parameter, or keep the safe-reference operation inside a typed Nexus Flow wrapper.",
                                    0, 0);
                            }
                        }
                    }
                }
                for (parameter_node = func->params; parameter_node;
                     parameter_node = parameter_node->next) {
                    if (parameter_node->type == AST_VARIADIC_PARAM) continue;
                    if (parameter_node->type == AST_VAR_DECL) {
                        Slot* parameter_slot = semantic_find_slot_by_declaration(
                            ctx, parameter_node);
                        if (function_slot && function_slot->param_types &&
                            parameter_index < (uint32_t)function_slot->param_count &&
                            !function_slot->param_types[parameter_index] &&
                            parameter_slot && parameter_slot->type_name) {
                            function_slot->param_types[parameter_index] =
                                semantic_duplicate(ctx, parameter_slot->type_name);
                            if (!function_slot->param_types[parameter_index])
                                semantic_set_result_error(
                                    ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
                        }
                        ++parameter_index;
                        if (parameter_slot) {
                            parameter_slot->is_parameter = 1u;
                            parameter_slot->is_initialized = 1u;
                            parameter_slot->is_nullable =
                                (uint8_t)semantic_is_raw_pointer_type_name(
                                    parameter_slot->type_name);
                            if (semantic_is_reference_type_name(
                                    parameter_slot->type_name))
                                parameter_slot->borrow_origin = parameter_slot;
                        }
                    }
                }
            }

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
                    if (return_type_valid && func->return_type) {
                        const char* normalized_return = func->return_type;
                        while (normalized_return &&
                               (*normalized_return == '!' ||
                                *normalized_return == '>'))
                            ++normalized_return;
                        if (normalized_return && *normalized_return &&
                            strcmp(normalized_return, "void") != 0 &&
                            !semantic_function_statement_terminates(func->body)) {
                            char message[320];
                            (void)snprintf(message, sizeof(message),
                                "Function '%s' can reach its end without returning a value of type '%s'.",
                                function_name, func->return_type);
                            report_issue(ctx, node,
                                ERR_SEMANTIC_RETURN_MISMATCH, message,
                                "Return a value on every control-flow path, add a complete else/default arm, or make the function return type void.",
                                "Return a value on every control-flow path, add a complete else/default arm, or make the function return type void.",
                                0, 0);
                        }
                    }
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
            SemanticReferenceInitSnapshot before_state = {0};
            SemanticReferenceInitSnapshot then_state = {0};
            SemanticReferenceInitSnapshot else_state = {0};
            int before_captured = 0;
            int then_captured = 0;
            int else_captured = 0;
            int then_terminates = 0;
            int else_terminates = 0;
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
            before_captured = semantic_reference_init_snapshot_capture(
                ctx, &before_state);
            semantic_refine_nullable_binding_from_condition(
                ctx, conditional->condition, 1);
            if (conditional->then_branch) {
                if (conditional->then_branch->type == AST_BLOCK) {
                    semantic_check(ctx, conditional->then_branch);
                } else {
                    enter_scope(ctx, conditional->then_branch);
                    semantic_check(ctx, conditional->then_branch);
                    exit_scope(ctx);
                }
            }
            if (before_captured)
                then_captured = semantic_reference_init_snapshot_capture(
                    ctx, &then_state);
            then_terminates = semantic_branch_terminates_flow(
                conditional->then_branch);
            if (before_captured)
                semantic_reference_init_snapshot_restore(&before_state);
            semantic_refine_nullable_binding_from_condition(
                ctx, conditional->condition, 0);
            if (conditional->else_branch) {
                if (conditional->else_branch->type == AST_BLOCK) {
                    semantic_check(ctx, conditional->else_branch);
                } else {
                    enter_scope(ctx, conditional->else_branch);
                    semantic_check(ctx, conditional->else_branch);
                    exit_scope(ctx);
                }
                if (before_captured)
                    else_captured = semantic_reference_init_snapshot_capture(
                        ctx, &else_state);
                else_terminates = semantic_branch_terminates_flow(
                    conditional->else_branch);
                if (before_captured && then_captured && else_captured) {
                    if (then_terminates && !else_terminates)
                        semantic_reference_init_snapshot_restore(&else_state);
                    else if (!then_terminates && else_terminates)
                        semantic_reference_init_snapshot_restore(&then_state);
                    else if (!then_terminates && !else_terminates)
                        semantic_reference_init_snapshot_merge(
                            &then_state, &else_state);
                    else
                        semantic_reference_init_snapshot_restore(&before_state);
                } else if (before_captured) {
                    semantic_reference_init_snapshot_restore(&before_state);
                }
            } else if (before_captured) {
                /* With no else, the false path always continues. If the true
                   branch terminates, only the refined false path survives. */
                if (then_captured && then_terminates) {
                    /* The false refinement is already active. */
                } else if (then_captured) {
                    SemanticReferenceInitSnapshot false_state = {0};
                    if (semantic_reference_init_snapshot_capture(
                            ctx, &false_state)) {
                        semantic_reference_init_snapshot_merge(
                            &then_state, &false_state);
                        semantic_reference_init_snapshot_restore(&then_state);
                    } else {
                        semantic_reference_init_snapshot_restore(&before_state);
                    }
                    semantic_reference_init_snapshot_destroy(&false_state);
                } else {
                    semantic_reference_init_snapshot_restore(&before_state);
                }
            }
            semantic_reference_init_snapshot_destroy(&before_state);
            semantic_reference_init_snapshot_destroy(&then_state);
            semantic_reference_init_snapshot_destroy(&else_state);
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

            if (ret->operand) {
                const char* returned_type = get_expr_type(ctx, ret->operand);
                if (semantic_type_is_any(ctx, ctx->current_func_return_type) &&
                    semantic_expression_contains_safe_reference(
                        ctx, ret->operand, 0u)) {
                    report_issue(ctx, node, ERR_SEMANTIC_RETURN_MISMATCH,
                        "A safe reference cannot be returned through any because its borrow origin would be erased.",
                        "Return the direct safe-reference type and preserve its parameter/global origin, or return a value/raw pointer.",
                        "Return the direct safe-reference type and preserve its parameter/global origin, or return a value/raw pointer.",
                        0, 0);
                }
                if (semantic_is_reference_type_name(returned_type) &&
                    !semantic_reference_return_has_stable_origin(
                        ctx, ret->operand)) {
                    report_error(ctx, node, ERR_SEMANTIC_RETURN_MISMATCH,
                        "Returning a safe reference to local storage is not supported; return a reference parameter or static/global storage.");
                }
            }

            if (ctx->current_func_return_type) {
                const char* actual_type = ret->operand ? get_expr_type(ctx, ret->operand) : "void";
                if (semantic_is_function_type_name(
                        ctx, ctx->current_func_return_type) &&
                    semantic_expression_may_be_null(ctx, ret->operand)) {
                    report_issue(ctx, node, ERR_SEMANTIC_RETURN_MISMATCH,
                        "A function returning a callback value must return a non-null function value.",
                        "Return a valid callback binding or change the return contract to an explicitly nullable callback type.",
                        "Return a valid callback binding or change the return contract to an explicitly nullable callback type.",
                        0, 0);
                }
                if (!are_types_compatible(ctx, ctx->current_func_return_type, actual_type)) {
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
            if (unary->op == TOKEN_BIT_AND ||
                unary->op == TOKEN_BORROW_MUT) {
                ++ctx->borrow_operand_depth;
                semantic_check(ctx, unary->operand);
                --ctx->borrow_operand_depth;
            } else {
                semantic_check(ctx, unary->operand);
            }
            if (unary->op == TOKEN_STAR && ctx->assignment_target_depth > 0u &&
                unary->operand && unary->operand->type == AST_IDENTIFIER) {
                Slot* pointer_slot = find_slot(
                    ctx, ((ASTIdentifier*)unary->operand)->name);
                if (pointer_slot && pointer_slot->kind == SLOT_VAR &&
                    (semantic_is_reference_type_name(pointer_slot->type_name) ||
                     semantic_is_raw_pointer_type_name(pointer_slot->type_name)) &&
                    !pointer_slot->is_initialized) {
                    report_issue(ctx, unary->operand,
                        ERR_SEMANTIC_TYPE_MISMATCH,
                        "Dereference assignment reads a pointer/reference binding before it has been initialized.",
                        "Initialize the pointer/reference before writing through the dereference.",
                        "Initialize the pointer/reference before writing through the dereference.",
                        0, 0);
                }
            }
            operand_type = get_expr_type(ctx, unary->operand);
            if (operand_type && unary->op == TOKEN_NOT &&
                !semantic_is_boolean_condition(ctx, unary->operand)) {
                char message[256];
                (void)snprintf(message, sizeof(message),
                    "Logical negation '!' requires a bool operand; got '%s'.",
                    operand_type);
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, message);
            } else if (unary->op == TOKEN_STAR) {
                if (!semantic_dereference_target_type(
                        operand_type, NULL)) {
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Dereference requires a pointer or reference operand.");
                }
            } else if (unary->op == TOKEN_BIT_AND ||
                       unary->op == TOKEN_BORROW_MUT) {
                if (!unary->operand || !semantic_is_lvalue(ctx, unary->operand)) {
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "Borrow operator requires an addressable lvalue.");
                } else if (unary->op == TOKEN_BORROW_MUT &&
                           semantic_lvalue_is_const(ctx, unary->operand)) {
                    report_error(ctx, node, ERR_SEMANTIC_CONST_ASSIGN,
                        "Cannot create an exclusive mutable borrow from an immutable value.");
                }
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
                    !are_types_compatible(ctx, field->type_name, actual_type)) {
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
            ASTMemberAccess* access = (ASTMemberAccess*)node;
            if (access->object) {
                uint8_t previous_suppression =
                    ctx->suppress_group_default_dispatch;
                ++ctx->borrow_operand_depth;
                ctx->suppress_group_default_dispatch = 1u;
                semantic_check_node(ctx, access->object, 0);
                ctx->suppress_group_default_dispatch = previous_suppression;
                --ctx->borrow_operand_depth;
            }
            {
                Slot* owner = semantic_lvalue_root_slot(ctx, node);
                char* path = semantic_lvalue_path(ctx, node);
                if (owner && ctx->borrow_operand_depth == 0u &&
                    ctx->assignment_target_depth == 0u &&
                    (!path ||
                     semantic_borrow_conflicts(ctx, owner, path, 0, NULL))) {
                    report_error(ctx, node, ERR_SEMANTIC_CONST_ASSIGN,
                        "Cannot read this member while an overlapping exclusive mutable borrow is active.");
                }
                free(path);
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
            int is_thread_copy_call = ctx->active_thread_block &&
                call->callee && call->callee->type == AST_IDENTIFIER &&
                ((ASTIdentifier*)call->callee)->name &&
                strcmp(((ASTIdentifier*)call->callee)->name, "copy") == 0;
            if (semantic_check_asm_intrinsic(ctx, node, call)) {
                if (call->args) semantic_check(ctx, call->args);
                break;
            }
            if (semantic_check_enum_constructor_call(ctx,node,call)) {
                if (call->args) semantic_check(ctx,call->args);
                break;
            }
            int imported_call_handled = 0;
            if (call->callee && call->callee->type == AST_MEMBER_ACCESS) {
                (void)semantic_resolve_imported_call(
                    ctx, node, 1, &imported_call_handled);
                if (!imported_call_handled) {
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
                }
            } else if (call->callee) {
                const char* callee_name = call->callee->type == AST_IDENTIFIER
                    ? ((ASTIdentifier*)call->callee)->name : NULL;
                int is_spawn_api = callee_name && strcmp(callee_name, "spawn") == 0;
                int is_thread_module_api = callee_name &&
                    (strcmp(callee_name, "listen") == 0 ||
                     strcmp(callee_name, "send") == 0 ||
                     strcmp(callee_name, "receive") == 0 ||
                     strcmp(callee_name, "done") == 0);
                if (!is_spawn_api && !is_thread_copy_call &&
                    !is_thread_module_api) {
                    semantic_check(ctx, call->callee);
                    if (call->callee->type == AST_IDENTIFIER) {
                        ASTIdentifier* id = (ASTIdentifier*)call->callee;
                        Slot* function_slot = find_slot(ctx, id->name);
                        if (function_slot && function_slot->kind == SLOT_FUNC) {
                            semantic_validate_call_arguments(
                                ctx, node, function_slot, call->args, 0u);
                        } else if (function_slot && function_slot->kind == SLOT_VAR) {
                            Slot callback_view;
                            ASTFuncDecl callback_declaration;
                            char** callback_parameter_types = NULL;
                            uint32_t callback_parameter_count = 0u;
                            if (semantic_prepare_callback_signature(
                                    ctx, function_slot, &callback_view,
                                    &callback_declaration,
                                    &callback_parameter_types)) {
                                callback_parameter_count =
                                    (uint32_t)callback_view.param_count;
                                if (function_slot->is_nullable) {
                                    report_issue(ctx, node,
                                        ERR_SEMANTIC_TYPE_MISMATCH,
                                        "Callback value may be null and cannot be invoked without a non-null check.",
                                        "Guard the callback with a comparison against null, or initialize it from a non-null callback value before calling it.",
                                        "Guard the callback with a comparison against null, or initialize it from a non-null callback value before calling it.",
                                        0, 0);
                                }
                                semantic_validate_call_arguments(
                                    ctx, node, &callback_view, call->args, 0u);
                                if (semantic_is_reference_type_name(
                                        semantic_callback_return_type(ctx, function_slot)) &&
                                    (!function_slot->callable_declaration ||
                                     function_slot->callable_declaration->type != AST_FUNC_DECL ||
                                     semantic_returned_reference_parameter(
                                        (ASTFuncDecl*)function_slot->callable_declaration) < 0)) {
                                    report_issue(ctx, node,
                                        ERR_SEMANTIC_TYPE_MISMATCH,
                                        "Function-value callback returns a safe reference but its target has no verifiable, consistent reference-parameter return origin.",
                                        "Assign a named function with a consistent reference-parameter return origin, or change the callback result to a value type.",
                                        "Assign a named function with a consistent reference-parameter return origin, or change the callback result to a value type.",
                                        0, 0);
                                }
                                semantic_destroy_callback_parameter_types(
                                    callback_parameter_types,
                                    callback_parameter_count);
                            } else {
                                char message[256];
                                (void)snprintf(message, sizeof(message),
                                    "Value '%s' is not callable; expected a function type or a function declaration.",
                                    id->name ? id->name : "<unnamed>");
                                report_error(ctx, node,
                                    ERR_SEMANTIC_TYPE_MISMATCH, message);
                            }
                        } else if (function_slot) {
                            /* Preserve existing Group/default-dispatch call handling. */
                            semantic_validate_call_arguments(
                                ctx, node, function_slot, call->args, 0u);
                        }
                    }
                }
            }
            semantic_validate_spawn_call(ctx, node, call);
            if (is_thread_copy_call) {
                if (!call->args || call->args->next) {
                    report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "copy() inside a thread_block requires exactly one source expression.");
                } else {
                    ++ctx->thread_capture_copy_depth;
                    semantic_check(ctx, call->args);
                    --ctx->thread_capture_copy_depth;
                }
            } else if (call->args) {
                int is_spawn_api = call->callee &&
                    call->callee->type == AST_IDENTIFIER &&
                    ((ASTIdentifier*)call->callee)->name &&
                    strcmp(((ASTIdentifier*)call->callee)->name, "spawn") == 0;
                /* The payload registry validator owns spawn argument shape;
                   don't also resolve the payload key as a normal variable. */
                if (!is_spawn_api) semantic_check(ctx, call->args);
            }
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
                        ASTVarDecl* v;
                        int is_inline_field;
                        int valid_type = 0;
                        if (field_node->type != AST_VAR_DECL) {
                            /* The aggregate-name validator already reported this node. */
                            field_node = field_node->next;
                            continue;
                        }
                        v = (ASTVarDecl*)field_node;
                        if (!v->name || !v->name[0]) {
                            field_node = field_node->next;
                            continue;
                        }

                        is_inline_field = v->inline_type != NULL;
                        if (v->type_name && v->type_name[0]) {
                            if (is_type_defined(ctx, v->type_name)) valid_type = 1;
                            else if (is_inline_field) {
                                const char* t = v->type_name[0] == '!'
                                    ? v->type_name + 1 : v->type_name;
                                if (strcmp(t, "struct") == 0 ||
                                    strcmp(t, "union") == 0) valid_type = 1;
                            }
                        }

                        if ((!v->type_name || !v->type_name[0]) && !v->type_expr) {
                            char message[256];
                            (void)snprintf(message, sizeof(message),
                                "Aggregate field '%s' requires an explicit type.",
                                v->name);
                            report_error(ctx, field_node,
                                ERR_SEMANTIC_TYPE_MISMATCH, message);
                        } else if (v->type_name && !valid_type && !v->type_expr) {
                            char buf[256];
                            (void)snprintf(buf, sizeof(buf),
                                "Field '%s' uses unknown type '%s'.",
                                v->name, v->type_name);
                            report_error(ctx, field_node,
                                ERR_SEMANTIC_UNKNOWN_TYPE, buf);
                        }

                        if (v->type_name &&
                            semantic_type_contains_safe_reference(
                                ctx, v->type_name, 0u)) {
                            char message[384];
                            (void)snprintf(message, sizeof(message),
                                "Aggregate '%s' field '%s' contains safe reference type '%s'; aggregate-contained reference ownership and lifetime tracking are not implemented.",
                                sdecl->name ? sdecl->name : "<anonymous>",
                                v->name, v->type_name);
                            report_issue(ctx, field_node,
                                ERR_SEMANTIC_TYPE_MISMATCH, message,
                                "Keep the safe reference in a local reference binding until aggregate field ownership and lifetime tracking are implemented. Raw pointers are a separate unsafe storage contract.",
                                "Keep the safe reference in a local reference binding until aggregate field ownership and lifetime tracking are implemented. Raw pointers are a separate unsafe storage contract.",
                                0, 0);
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
                    ASTVarDecl* v;
                    int is_inline_field;
                    int valid_type = 0;
                    if (field_node->type != AST_VAR_DECL) {
                        /* The aggregate-name validator already reported this node. */
                        field_node = field_node->next;
                        continue;
                    }
                    v = (ASTVarDecl*)field_node;
                    if (!v->name || !v->name[0]) {
                        field_node = field_node->next;
                        continue;
                    }
                    is_inline_field = v->inline_type != NULL;
                    if (v->type_name && v->type_name[0]) {
                        if (is_type_defined(ctx, v->type_name)) valid_type = 1;
                        else if (is_inline_field) {
                            const char* t = v->type_name[0] == '!'
                                ? v->type_name + 1 : v->type_name;
                            if (strcmp(t, "struct") == 0 ||
                                strcmp(t, "union") == 0) valid_type = 1;
                        }
                    }

                    if ((!v->type_name || !v->type_name[0]) && !v->type_expr) {
                        char message[256];
                        (void)snprintf(message, sizeof(message),
                            "Aggregate field '%s' requires an explicit type.",
                            v->name);
                        report_error(ctx, field_node,
                            ERR_SEMANTIC_TYPE_MISMATCH, message);
                    } else if (v->type_name && !valid_type && !v->type_expr) {
                        char buf[256];
                        (void)snprintf(buf, sizeof(buf),
                            "Field '%s' uses unknown type '%s'.",
                            v->name, v->type_name);
                        report_error(ctx, field_node, ERR_SEMANTIC_UNKNOWN_TYPE, buf);
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
            Slot* existing_type;
            int is_group_binding_name;
            if (!edecl->name || !edecl->name[0]) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Enum declaration requires a non-empty type name.");
                break;
            }
            existing_type = find_slot(ctx, edecl->name);
            is_group_binding_name =
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
                Slot* s = add_slot(ctx, edecl->name, SLOT_ENUM, 1, NULL,
                    node, 0, 0, 0, 0u);
                ASTNode* var_node = edecl->variants;
                FieldInfo* last_var = NULL;
                uint32_t variant_ordinal = 0u;
                uint32_t payload_field_ordinal = 0u;
                int64_t next_discriminant = 0;
                int next_discriminant_valid = 1;
                if (!s) break;
                semantic_define_named_type(ctx, s, edecl->name, NULL);
                semantic_record_annotation(
                    ctx, node, s, edecl->name, SEMANTIC_RESULT_VALUE_UNSPECIFIED);

                while (var_node) {
                    ASTEnumVariant* variant;
                    ASTNode* previous_variant;
                    int variant_name_valid = 1;

                    if (var_node->type != AST_ENUM_VARIANT) {
                        report_error(ctx, var_node, ERR_SEMANTIC_TYPE_MISMATCH,
                            "Enum variant list contains a non-variant AST node.");
                        var_node = var_node->next;
                        continue;
                    }
                    variant = (ASTEnumVariant*)var_node;
                    if (!variant->name || !variant->name[0]) {
                        report_error(ctx, var_node, ERR_SEMANTIC_TYPE_MISMATCH,
                            "Enum variant requires a non-empty name.");
                        variant_name_valid = 0;
                    } else {
                        for (previous_variant = edecl->variants;
                             previous_variant && previous_variant != var_node;
                             previous_variant = previous_variant->next) {
                            if (previous_variant->type == AST_ENUM_VARIANT) {
                                const ASTEnumVariant* earlier =
                                    (const ASTEnumVariant*)previous_variant;
                                if (earlier->name &&
                                    strcmp(earlier->name, variant->name) == 0) {
                                    char message[256];
                                    (void)snprintf(message, sizeof(message),
                                        "Enum '%s' declares variant '%s' more than once.",
                                        edecl->name, variant->name);
                                    report_error(ctx, var_node,
                                        ERR_SEMANTIC_REDEFINED, message);
                                    variant_name_valid = 0;
                                    break;
                                }
                            }
                        }
                    }

                    /*
                     * Resolve each enum tag before payload processing. Explicit
                     * values must be integer constant expressions in the signed
                     * 64-bit semantic domain. Implicit values start at zero and
                     * continue from the previously resolved tag.
                     */
                    {
                        int64_t resolved_value = 0;
                        int value_valid = 1;
                        if (variant->discriminant_expr) {
                            if (!semantic_eval_integer_constant(
                                    variant->discriminant_expr, &resolved_value)) {
                                report_error(ctx, variant->discriminant_expr,
                                    ERR_SEMANTIC_TYPE_MISMATCH,
                                    "Enum discriminant must be a compile-time integer constant expression representable as signed 64-bit.");
                                value_valid = 0;
                            }
                        } else if (next_discriminant_valid) {
                            resolved_value = next_discriminant;
                        } else {
                            report_error(ctx, var_node,
                                ERR_SEMANTIC_TYPE_MISMATCH,
                                "Implicit enum discriminant overflows signed 64-bit; provide an explicit smaller value.");
                            value_valid = 0;
                        }

                        if (value_valid) {
                            for (previous_variant = edecl->variants;
                                 previous_variant && previous_variant != var_node;
                                 previous_variant = previous_variant->next) {
                                if (previous_variant->type == AST_ENUM_VARIANT) {
                                    const ASTEnumVariant* earlier =
                                        (const ASTEnumVariant*)previous_variant;
                                    if (earlier->has_resolved_discriminant &&
                                        earlier->discriminant_value == resolved_value) {
                                        char message[320];
                                        (void)snprintf(message, sizeof(message),
                                            "Enum '%s' assigns discriminant value %lld more than once (variant '%s').",
                                            edecl->name, (long long)resolved_value,
                                            variant->name ? variant->name : "<unnamed>");
                                        report_error(ctx, var_node,
                                            ERR_SEMANTIC_REDEFINED, message);
                                        break;
                                    }
                                }
                            }
                            variant->discriminant_value = resolved_value;
                            variant->has_resolved_discriminant = 1u;
                            if (resolved_value == INT64_MAX) {
                                next_discriminant_valid = 0;
                            } else {
                                next_discriminant = resolved_value + 1;
                                next_discriminant_valid = 1;
                            }
                        } else {
                            variant->has_resolved_discriminant = 0u;
                        }
                    }

                    if (variant->data) {
                        ASTNode* field_node = variant->data;
                        while (field_node) {
                            ASTVarDecl* field;
                            ASTNode* previous_field;
                            int field_name_valid = 1;
                            int field_type_valid = 0;
                            int inline_field;

                            if (field_node->type != AST_VAR_DECL) {
                                report_error(ctx, field_node,
                                    ERR_SEMANTIC_TYPE_MISMATCH,
                                    "Enum variant payload contains a non-field AST node.");
                                field_node = field_node->next;
                                continue;
                            }
                            field = (ASTVarDecl*)field_node;
                            if (!field->name || !field->name[0]) {
                                report_error(ctx, field_node,
                                    ERR_SEMANTIC_TYPE_MISMATCH,
                                    "Enum variant payload fields require non-empty names.");
                                field_name_valid = 0;
                            } else {
                                for (previous_field = variant->data;
                                     previous_field && previous_field != field_node;
                                     previous_field = previous_field->next) {
                                    if (previous_field->type == AST_VAR_DECL) {
                                        const ASTVarDecl* earlier =
                                            (const ASTVarDecl*)previous_field;
                                        if (earlier->name &&
                                            strcmp(earlier->name, field->name) == 0) {
                                            char message[320];
                                            (void)snprintf(message, sizeof(message),
                                                "Enum variant '%s' declares payload field '%s' more than once.",
                                                variant->name ? variant->name : "<unnamed>",
                                                field->name);
                                            report_error(ctx, field_node,
                                                ERR_SEMANTIC_REDEFINED, message);
                                            field_name_valid = 0;
                                            break;
                                        }
                                    }
                                }
                            }

                            inline_field = field->inline_type != NULL;
                            if (field->type_name && field->type_name[0]) {
                                if (is_type_defined(ctx, field->type_name))
                                    field_type_valid = 1;
                                else if (inline_field) {
                                    const char* normalized = field->type_name[0] == '!'
                                        ? field->type_name + 1 : field->type_name;
                                    if (strcmp(normalized, "struct") == 0 ||
                                        strcmp(normalized, "union") == 0)
                                        field_type_valid = 1;
                                }
                            }
                            if ((!field->type_name || !field->type_name[0]) &&
                                !field->type_expr) {
                                report_error(ctx, field_node,
                                    ERR_SEMANTIC_TYPE_MISMATCH,
                                    "Enum variant payload field requires an explicit type.");
                            } else if (field->type_name && !field_type_valid &&
                                       !field->type_expr) {
                                char message[256];
                                (void)snprintf(message, sizeof(message),
                                    "Enum payload field '%s' uses unknown type '%s'.",
                                    field->name ? field->name : "<unnamed>",
                                    field->type_name);
                                report_error(ctx, field_node,
                                    ERR_SEMANTIC_UNKNOWN_TYPE, message);
                            }

                            if (field->inline_type && !field->type_expr)
                                semantic_check(ctx, field->inline_type);
                            if (field->type_expr)
                                semantic_check(ctx, field->type_expr);

                            if (ctx->result_enabled && s->symbol_id != 0u &&
                                field_name_valid) {
                                SemanticResultStatus relation_status =
                                    semantic_result_add_relation(
                                        &ctx->result,
                                        s->symbol_id,
                                        SEMANTIC_RESULT_RELATION_ENUM_PAYLOAD_FIELD,
                                        payload_field_ordinal++,
                                        0u,
                                        semantic_node_id(ctx, field_node),
                                        field->name,
                                        semantic_type_id(ctx, field->type_name,
                                            field->is_hard_typed));
                                semantic_set_result_error(ctx, relation_status);
                            }
                            field_node = field_node->next;
                        }
                    }

                    if (variant_name_valid) {
                        if (ctx->result_enabled && s->symbol_id != 0u) {
                            SemanticResultStatus relation_status =
                                semantic_result_add_relation(
                                    &ctx->result,
                                    s->symbol_id,
                                    SEMANTIC_RESULT_RELATION_ENUM_VARIANT,
                                    variant_ordinal++,
                                    0u,
                                    semantic_node_id(ctx, var_node),
                                    variant->name,
                                    s->type_id);
                            semantic_set_result_error(ctx, relation_status);
                        }

                        {
                            FieldInfo* entry = (FieldInfo*)semantic_callocate(
                                ctx, 1u, sizeof(FieldInfo));
                            if (!entry) {
                                semantic_set_result_error(
                                    ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
                                break;
                            }
                            entry->name = semantic_duplicate(ctx, variant->name);
                            if (!entry->name) {
                                free(entry);
                                semantic_set_result_error(
                                    ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
                                break;
                            }
                            if (last_var) last_var->next = entry;
                            else s->fields = entry;
                            last_var = entry;
                        }
                    }
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
            if (loop->loop_type < 0 || loop->loop_type > 3) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Loop contains an unsupported loop kind.");
                break;
            }
            if (!loop->body) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Loop is missing its body.");
                break;
            }
            if (loop->loop_type == 3 && (!loop->variable || !loop->collection)) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Foreach loop requires both an iterator and a collection.");
                break;
            }
            if (loop->loop_type == 1 && !loop->condition) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Conditional loop is missing its condition expression.");
                break;
            }
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

            {
                SemanticReferenceInitSnapshot before_body = {0};
                SemanticReferenceInitSnapshot after_body = {0};
                int captured = semantic_reference_init_snapshot_capture(
                    ctx, &before_body);
                int after_captured = 0;
                if (loop->body) semantic_check(ctx, loop->body);
                /* A loop may execute zero or more times. Merge the body
                   state with the incoming state so callback target provenance
                   cannot be mistaken for a value that survived every path. */
                if (captured)
                    after_captured = semantic_reference_init_snapshot_capture(
                        ctx, &after_body);
                if (captured && after_captured) {
                    semantic_reference_init_snapshot_merge(
                        &after_body, &before_body);
                    semantic_reference_init_snapshot_restore(&after_body);
                } else if (captured) {
                    semantic_reference_init_snapshot_restore(&before_body);
                }
                semantic_reference_init_snapshot_destroy(&before_body);
                semantic_reference_init_snapshot_destroy(&after_body);
            }
            if (ctx->loop_depth > 0u) --ctx->loop_depth;
            exit_scope(ctx);
            break;
        }

        case AST_NEXUS_LABEL: {
            ASTNexusLabel* lbl = (ASTNexusLabel*)node;
            if (!lbl->label_name || !lbl->label_name[0]) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Nexus label declaration requires a non-empty label name.");
                break;
            }
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
            if (!jmp->label_name || !jmp->label_name[0]) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Nexus jump requires a non-empty target label name.");
                break;
            }
            /* Forward targets are resolved by the later control-flow validation pass. */
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
            report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                "AST_AI_LOGIC is reserved and is not supported by the Semantic pipeline.");
            break;
        }

        case AST_UNPACK: {
            ASTUnpack* u = (ASTUnpack*)node;
            if (!u->expr) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Reserved unpack AST node is missing its source expression.");
                break;
            }
            semantic_check(ctx, u->expr);
            break;
        }

        case AST_CAST: {
            ASTCast* cast = (ASTCast*)node;
            if (!cast->value) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "cast() is missing its source value expression.");
                break;
            }
            if (!cast->target_type) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "cast() is missing its target type.");
                break;
            }
            semantic_check(ctx, cast->value);
            if (cast->target_type && cast->target_type->type == AST_IDENTIFIER) {
                const char* target_name = ((ASTIdentifier*)cast->target_type)->name;
                const char* source_type = get_expr_type(ctx, cast->value);
                if (semantic_is_reference_type_name(target_name) &&
                    (semantic_type_is_any(ctx, source_type) ||
                     semantic_is_raw_pointer_type_name(source_type))) {
                    report_issue(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                        "cast() cannot create a safe reference from any or a raw pointer because no owner/lifetime provenance is available.",
                        "Create a safe reference with &owner/&mut owner, or use a raw pointer and keep its unsafe contract explicit.",
                        "Create a safe reference with &owner/&mut owner, or use a raw pointer and keep its unsafe contract explicit.",
                        0, 0);
                }
            }
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
            report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                "Legacy AST_LISTEN is not emitted by the core Parser; use the Thread module listen(...) API.");
            break;
        }

        case AST_ECHO: {
            ASTCall* call = (ASTCall*)node;
            if (call->args) semantic_check(ctx, call->args);
            break;
        }

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
            /* Dedicated intrinsic callee is identity metadata, not a variable use. */
            if (node->type == AST_REG) {
                ASTNode* write_value = call->args && call->args->next &&
                    call->args->next->next
                    ? semantic_intrinsic_argument_value(call->args->next->next)
                    : NULL;
                if (write_value) semantic_check(ctx, write_value);
            } else if (call->args) {
                semantic_check(ctx, call->args);
            }
            semantic_validate_intrinsic_callee(ctx, node, call);
            if (node->type == AST_ADDR || node->type == AST_LEN ||
                node->type == AST_PEEK || node->type == AST_POKE)
                semantic_validate_memory_intrinsic(ctx, node);
            else if (node->type == AST_INB || node->type == AST_OUTB)
                semantic_validate_port_intrinsic(ctx, node);
            else if (node->type == AST_AREA || node->type == AST_ZONE ||
                     node->type == AST_FREE)
                semantic_validate_allocation_intrinsic(ctx, node);
            else if (node->type == AST_IRQ)
                semantic_validate_irq_intrinsic(ctx, node);
            else if (node->type == AST_REG)
                semantic_validate_reg_intrinsic(ctx, node);
            break;
        }

        case AST_GROUP_DECL: {
            ASTGroupDecl* g = (ASTGroupDecl*)node;
            const ASTNode* previous_group_owner = ctx->current_group_owner;
            Slot* group_slot = NULL;
            Slot* existing_group;
            if (!g->name || !g->name[0]) {
                report_error(ctx,node,ERR_SEMANTIC_TYPE_MISMATCH,
                    "Group declaration requires a non-empty name.");
                break;
            }
            existing_group = find_slot(ctx, g->name);
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
            int is_default_handler = m->public_name &&
                strcmp(m->public_name, "default") == 0;
            if (!m->public_name || !m->public_name[0]) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Group method requires a non-empty public name.");
            }
            if (!m->implementation) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Group method is missing its implementation.");
                break;
            }
            if (m->implementation->type != AST_FUNC_DECL &&
                m->implementation->type != AST_EXTERN_DECL &&
                !(is_default_handler && m->implementation->type == AST_BLOCK)) {
                report_error(ctx, m->implementation,
                    ERR_SEMANTIC_TYPE_MISMATCH,
                    is_default_handler
                        ? "The default handler implementation must be a function, extern declaration, or block."
                        : "Group method implementation must be a function or extern declaration.");
                break;
            }
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

        /* Legacy serialized Thread call tags: recurse only; new Parser emits AST_FUNC_CALL. */
        case AST_SPAWN:
        case AST_SEND:
        case AST_RECEIVE:
        case AST_DONE:
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
            semantic_validate_intrinsic_callee(ctx, node, call);
            if (node->type == AST_INTR) semantic_validate_intr_intrinsic(ctx, node);
            break;
        }

        case AST_USE: {
            // Modül yükleme mantığı
            break;
        }

        case AST_DEFER: {
            ASTDefer* defer = (ASTDefer*)node;
            if (!defer->body) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "defer requires a statement or block body.");
                break;
            }
            semantic_check(ctx, defer->body);
            break;
        }

        case AST_THREAD_BLOCK: {
            ASTThreadBlock* tb = (ASTThreadBlock*)node;
            if (ctx->thread_block_depth != 0u) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "Nested thread_block declarations are not allowed; each thread_block must be a complete, independent thread payload.");
                break;
            }
            if (!tb->name || !tb->name[0]) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "thread_block requires a non-empty block name.");
                break;
            }
            if (!tb->body) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "thread_block is missing its body.");
                break;
            }
            if (tb->body->type != AST_BLOCK) {
                report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH,
                    "thread_block body must be an AST_BLOCK node.");
                break;
            }
            /* Register before checking the payload so later spawn() calls
               can resolve this complete named payload. */
            if (!semantic_register_thread_block(ctx, tb)) break;
            /* The named block is one complete thread payload. Nested
               payloads are rejected; the child AST_BLOCK owns its scope. */
            {
                ThreadBlockEntry* active_block =
                    semantic_find_thread_block(ctx, tb->name);
                ThreadBlockEntry* previous_block = ctx->active_thread_block;
                if (active_block) {
                    active_block->capture_base_depth =
                        (uint32_t)ctx->current_depth;
                    ctx->active_thread_block = active_block;
                }
                ++ctx->thread_block_depth;
                semantic_check(ctx, tb->body);
                --ctx->thread_block_depth;
                ctx->active_thread_block = previous_block;
            }
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

    Slot* current;
    /* Release all cross-Slot links before destroying any Slot. */
    for (current = ctx->head; current; current = current->next)
        semantic_release_borrow_binding(current);
    current = ctx->head;
    while (current) {
        Slot* next = current->next;
        destroy_slot(current);
        current = next;
    }
    ctx->head = NULL;

    SemanticTypeName* expression_type = ctx->expression_type_names;
    while (expression_type) {
        SemanticTypeName* next = expression_type->next;
        free(expression_type->name);
        free(expression_type);
        expression_type = next;
    }
    ctx->expression_type_names = NULL;

    ThreadBlockEntry* thread_block = ctx->thread_blocks;
    while (thread_block) {
        ThreadBlockEntry* next = thread_block->next;
        ThreadBlockCapture* capture = thread_block->captures;
        while (capture) {
            ThreadBlockCapture* capture_next = capture->next;
            free(capture->name);
            free(capture->type_name);
            free(capture);
            capture = capture_next;
        }
        free(thread_block->name);
        free(thread_block);
        thread_block = next;
    }
    ctx->thread_blocks = NULL;
    ctx->thread_block_tail = NULL;

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
    ctx->import_environment = NULL;
    ctx->current_scope_id = 0u;
}