#include "semantic.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lexer.h"

void semantic_init(SemanticContext* ctx, const char* filename, const char* lang_file_path) {
    (void)lang_file_path; /* Reserved for the shared diagnostics localization API. */
    ctx->head = NULL;
    ctx->filename = filename;
    ctx->error_count = 0;
    ctx->error_head = NULL;
    ctx->error_tail = NULL;
    ctx->current_depth = 0;
    ctx->current_func_return_type = NULL;
    ctx->diagnostics = NULL;
    ctx->diagnostic_stage_id = OCC_STAGE_SEMANTIC;
    ctx->current_scope_id = 1u;
    ctx->node_id_resolver = NULL;
    ctx->node_id_context = NULL;
    ctx->result_enabled = 0u;
    ctx->result_status = semantic_result_init(&ctx->result);
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
    ctx->result_enabled = (uint8_t)(resolver != NULL);
    ctx->current_scope_id = 1u;
}

SemanticResultStatus semantic_finalize_result(
    SemanticContext* ctx, uint32_t ast_node_count
) {
    if (!ctx) return SEMANTIC_RESULT_INVALID_ARGUMENT;
    if (!ctx->result_enabled) return SEMANTIC_RESULT_INVALID_ARGUMENT;
    if (ctx->result_status != SEMANTIC_RESULT_OK) return ctx->result_status;
    ctx->result_status = semantic_result_finalize(&ctx->result, ast_node_count);
    return ctx->result_status;
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
        int dist = levenshtein(name, current->name);
        size_t max_len = strlen(name) > strlen(current->name) ? strlen(name) : strlen(current->name);
        double denominator = max_len ? (double)max_len : 1.0;
        int score = (int)((1.0 - (double)dist / denominator) * 100);
        if (score > best_score) {
            best_score = score;
            best_slot = current;
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
    Error* err = (Error*)malloc(sizeof(Error));
    err->code = code;
    err->filename = strdup(ctx->filename ? ctx->filename : "<unknown>");
    err->line = node->line;
    err->col = node->col;
    err->message = strdup(msg);
    err->suggestion = suggestion ? strdup(suggestion) : NULL;
    err->hint = hint ? strdup(hint) : NULL;
    err->similarity = similarity;
    err->is_warning = is_warning;
    err->next = NULL;

    if (ctx->error_tail) {
        ctx->error_tail->next = err;
        ctx->error_tail = err;
    } else {
        ctx->error_head = err;
        ctx->error_tail = err;
    }
    ctx->error_count++;
}

static void report_error(SemanticContext* ctx, ASTNode* node, ErrorCode code, const char* msg) {
    report_issue(ctx, node, code, msg, NULL, NULL, 0, 0);
}

static Slot* find_slot(SemanticContext* ctx, const char* name) {
    Slot* current = ctx->head;
    while (current) {
        if (strcmp(current->name, name) == 0) return current;
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
        default: return 0u;
    }
}

static uint32_t semantic_type_id(
    SemanticContext* ctx, const char* type_name
) {
    const char* name = type_name;
    uint16_t flags = 0u;
    uint16_t kind;
    uint32_t referenced_symbol_id = 0u;
    uint32_t base_type_id = 0u;
    uint32_t type_id = 0u;
    Slot* declared_type = NULL;
    SemanticResultStatus status;

    if (!ctx || !ctx->result_enabled || !type_name || !*type_name) return 0u;
    if (*name == '!') {
        flags |= SEMANTIC_RESULT_TYPE_FLAG_HARD;
        ++name;
    }
    if (!*name) return 0u;

    if (is_primitive_type_for_result(name)) {
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

static Slot* add_slot(
    SemanticContext* ctx,
    const char* name,
    SlotKind kind,
    int is_const,
    const char* type_name,
    ASTNode* declaration_node,
    int is_implicit,
    int is_packed
) {
    Slot* s = (Slot*)malloc(sizeof(Slot));
    if (!s) {
        semantic_set_result_error(ctx, SEMANTIC_RESULT_OUT_OF_MEMORY);
        return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->name = strdup(name ? name : "");
    s->kind = kind;
    s->is_const = is_const;
    s->depth = ctx->current_depth;
    s->type_name = type_name ? strdup(type_name) : NULL;
    s->param_count = 0;
    s->param_types = NULL;
    s->fields = NULL;
    s->is_packed = is_packed;
    s->scope_id = ctx->current_scope_id;
    s->type_id = semantic_type_id(ctx, type_name);

    if (ctx->result_enabled) {
        uint32_t declaration_node_id = semantic_node_id(ctx, declaration_node);
        uint16_t result_flags = 0u;
        if (is_const) result_flags |= SEMANTIC_RESULT_SYMBOL_FLAG_CONST;
        if (is_packed) result_flags |= SEMANTIC_RESULT_SYMBOL_FLAG_PACKED;
        if (is_implicit) result_flags |= SEMANTIC_RESULT_SYMBOL_FLAG_IMPLICIT;
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
    uint32_t base_type_id = semantic_type_id(ctx, base_type_name);
    uint32_t type_id = 0u;
    SemanticResultStatus status;

    if (!ctx || !slot || !ctx->result_enabled || slot->symbol_id == 0u) return;
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

    if (!ctx || !ctx->result_enabled || !node) return;
    node_id = semantic_node_id(ctx, node);
    type_id = slot ? slot->type_id : semantic_type_id(ctx, type_name);
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

    Slot* current = ctx->head;
    while (current && current->depth == ctx->current_depth) {
        Slot* next = current->next;
        destroy_slot(current);
        current = next;
    }
    ctx->head = current;
    if (ctx->result_enabled && ctx->current_scope_id > 1u) {
        const SemanticResultScope* scope =
            &ctx->result.scopes[ctx->current_scope_id - 1u];
        if (scope->depth == (uint32_t)ctx->current_depth) {
            ctx->current_scope_id = scope->parent_scope_id;
        }
    }
    ctx->current_depth--;
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
        case AST_FUNC_CALL: {
            ASTCall* call = (ASTCall*)node;
            if (call->callee && call->callee->type == AST_IDENTIFIER) {
                ASTIdentifier* id = (ASTIdentifier*)call->callee;
                Slot* slot = find_slot(ctx, id->name);
                semantic_record_annotation(
                    ctx, node, slot, slot ? slot->type_name : NULL,
                    SEMANTIC_RESULT_VALUE_RVALUE);
                return slot ? slot->type_name : NULL;
            }
            return NULL;
        }
        case AST_BINARY_EXPR: {
            const char* type_name =
                get_expr_type(ctx, ((ASTBinaryExpr*)node)->left);
            semantic_record_annotation(
                ctx, node, NULL, type_name, SEMANTIC_RESULT_VALUE_RVALUE);
            return type_name;
        }
        case AST_UNARY_EXPR: {
            const char* type_name =
                get_expr_type(ctx, ((ASTUnaryExpr*)node)->operand);
            semantic_record_annotation(
                ctx, node, NULL, type_name, SEMANTIC_RESULT_VALUE_RVALUE);
            return type_name;
        }
        default:
            return NULL;
    }
}

static int are_types_compatible(const char* type1, const char* type2) {
    if (!type1 || !type2) return 0;
    const char* t1 = (type1[0] == '!') ? type1 + 1 : type1;
    const char* t2 = (type2[0] == '!') ? type2 + 1 : type2;
    return strcmp(t1, t2) == 0;
}

static int is_type_defined(SemanticContext* ctx, const char* type_name) {
    if (!type_name) return 1; // Infer type or void
    if (type_name[0] == '!') type_name++; // Skip hard-type prefix

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

void semantic_check(SemanticContext* ctx, ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
            semantic_check(ctx, ((ASTProgram*)node)->declarations);
            break;

        case AST_VAR_DECL: {
            ASTVarDecl* decl = (ASTVarDecl*)node;
            Slot* existing = find_slot(ctx, decl->name);
            if (existing && existing->depth == ctx->current_depth) {
                char buf[256];
                snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_REDEFINED), decl->name);
                report_issue(ctx, node, ERR_SEMANTIC_REDEFINED, buf, NULL, "Bu ismi daha önce kullandınız.", 0, 0);
            } else {
                int is_inline = (decl->inline_type != NULL);
                int valid_type = 0;
                if (decl->type_name) {
                     if (is_type_defined(ctx, decl->type_name)) valid_type = 1;
                     else if (is_inline) {
                         const char* t = (decl->type_name[0] == '!') ? decl->type_name + 1 : decl->type_name;
                         if (strcmp(t, "struct") == 0 || strcmp(t, "union") == 0) valid_type = 1;
                     }
                }

                if (decl->type_name && !valid_type) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_UNKNOWN_TYPE), decl->type_name);
                    report_error(ctx, node, ERR_SEMANTIC_UNKNOWN_TYPE, buf);
                }

                Slot* declared_slot = add_slot(
                    ctx, decl->name, SLOT_VAR, decl->is_const,
                    decl->type_name, node, 0, 0);
                semantic_record_annotation(
                    ctx, node, declared_slot, decl->type_name,
                    SEMANTIC_RESULT_VALUE_LVALUE);
                printf("%s: %s (%s)\n", msg_get(MSG_SEMANTIC_SLOT), decl->name, decl->is_const ? "CONST" : "MUT");
            }
            
            if (decl->inline_type) {
                semantic_check(ctx, decl->inline_type);
            }

            if (decl->initializer) {
                semantic_check(ctx, decl->initializer);
                if (decl->type_name) {
                    const char* init_type = get_expr_type(ctx, decl->initializer);
                    if (init_type && !are_types_compatible(decl->type_name, init_type)) {
                        char buf[256];
                        snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_TYPE_MISMATCH), decl->type_name, init_type);
                        report_issue(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, buf, NULL, "İlgili veri tipini kontrol edin.", 0, 0);
                    }
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
            
            // Sıfıra Bölme Kontrolü (Literal 0 için)
            if (bin->op == TOKEN_SLASH || bin->op == TOKEN_PERCENT) {
                if (bin->right->type == AST_LITERAL) {
                    ASTLiteral* lit = (ASTLiteral*)bin->right;
                    if (lit->type == TOKEN_NUMBER && strcmp(lit->string_value, "0") == 0) {
                        report_error(ctx, node, ERR_SEMANTIC_DIV_ZERO, msg_get(ERR_SEMANTIC_DIV_ZERO));
                    }
                }
            }
            
            semantic_check(ctx, bin->left);
            semantic_check(ctx, bin->right);
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
                         snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_TYPE_MISMATCH), slot->type_name, right_type);
                         report_error(ctx, node, ERR_SEMANTIC_TYPE_MISMATCH, buf);
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
                add_slot(ctx, capture->handler_name, SLOT_VAR, 0, NULL, node, 1, 0); // Handler'lar genelde mutable'dır
                printf("%s: %s\n", msg_get(MSG_SEMANTIC_HANDLER), capture->handler_name);
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

        case AST_FUNC_DECL: {
            ASTFuncDecl* func = (ASTFuncDecl*)node;
            if (func->name) {
                Slot* existing = find_slot(ctx, func->name);
                if (existing && existing->depth == ctx->current_depth) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_FUNC_REDEFINED), func->name);
                    report_error(ctx, node, ERR_SEMANTIC_FUNC_REDEFINED, buf);
                } else {
                    Slot* s = add_slot(ctx, func->name, SLOT_FUNC, 1, func->return_type, node, 0, 0);
                    
                    // Parametre bilgilerini kaydet
                    int p_count = 0;
                    ASTNode* p = func->params;
                    while(p) { p_count++; p = p->next; }
                    
                    s->param_count = p_count;
                    if (p_count > 0) {
                        s->param_types = (char**)malloc(sizeof(char*) * p_count);
                        p = func->params;
                        int i = 0;
                        while(p) {
                            ASTVarDecl* v = (ASTVarDecl*)p;
                            s->param_types[i++] = v->type_name ? strdup(v->type_name) : NULL;
                            p = p->next;
                        }
                    }
                }
            }
            
            const char* prev_return_type = ctx->current_func_return_type;
            ctx->current_func_return_type = func->return_type;
            enter_scope(ctx, node);
            if (func->params) semantic_check(ctx, func->params);
            if (func->body) semantic_check(ctx, func->body);
            exit_scope(ctx);
            ctx->current_func_return_type = prev_return_type;
            break;
        }

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

        case AST_FUNC_CALL: {
            ASTCall* call = (ASTCall*)node;
            semantic_check(ctx, call->callee);
            
            if (call->callee->type == AST_IDENTIFIER) {
                ASTIdentifier* id = (ASTIdentifier*)call->callee;
                Slot* s = find_slot(ctx, id->name);
                if (s) {
                    // Argüman Sayısı Kontrolü
                    int arg_count = 0;
                    ASTNode* arg = call->args;
                    while(arg) { arg_count++; arg = arg->next; }
                    
                    if (arg_count != s->param_count) {
                        char buf[256];
                        snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_ARG_COUNT), id->name, s->param_count, arg_count);
                        report_error(ctx, node, ERR_SEMANTIC_ARG_COUNT, buf);
                    }

                    // Tip Kontrolü
                    arg = call->args;
                    int i = 0;
                    while(arg && i < s->param_count) {
                        const char* arg_type = get_expr_type(ctx, arg);
                        const char* expected = s->param_types[i];
                        if (arg_type && expected && !are_types_compatible(expected, arg_type)) {
                            char buf[256];
                            snprintf(buf, sizeof(buf), msg_get(ERR_SEMANTIC_ARG_TYPE), i+1, expected, arg_type);
                            report_error(ctx, arg, ERR_SEMANTIC_ARG_TYPE, buf);
                        }
                        arg = arg->next;
                        i++;
                    }
                }
            }
            
            if (call->args) semantic_check(ctx, call->args);
            break;
        }

        case AST_STRUCT_DECL:
        case AST_UNION_DECL: {
            ASTStructDecl* sdecl = (ASTStructDecl*)node;
            if (sdecl->name) {
                if (find_slot(ctx, sdecl->name)) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "Type '%s' already defined.", sdecl->name);
                    report_error(ctx, node, ERR_SEMANTIC_REDEFINED, buf);
                } else {
                    Slot* s = add_slot(ctx, sdecl->name, sdecl->is_union ? SLOT_UNION : SLOT_STRUCT, 1, NULL, node, 0, sdecl->is_packed);
                    s->is_packed = sdecl->is_packed;
                    semantic_define_named_type(ctx, s, sdecl->name, NULL);
                    
                    // Alanları kaydet
                    ASTNode* field_node = sdecl->fields;
                    FieldInfo* last_field = NULL;
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

                        FieldInfo* f = (FieldInfo*)malloc(sizeof(FieldInfo));
                        f->name = strdup(v->name);
                        f->type_name = v->type_name ? strdup(v->type_name) : NULL;
                        f->next = NULL;
                        
                        if (last_field) last_field->next = f;
                        else s->fields = f;
                        last_field = f;
                        
                        field_node = field_node->next;
                    }
                    printf("[SEMANTIC] Registered %s: %s\n", sdecl->is_union ? "UNION" : "STRUCT", sdecl->name);
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
            if (find_slot(ctx, edecl->name)) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Type '%s' already defined.", edecl->name);
                report_error(ctx, node, ERR_SEMANTIC_REDEFINED, buf);
            } else {
                Slot* s = add_slot(ctx, edecl->name, SLOT_ENUM, 1, NULL, node, 0, 0);
                semantic_define_named_type(ctx, s, edecl->name, NULL);
                
                ASTNode* var_node = edecl->variants;
                FieldInfo* last_var = NULL;
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
                            field_node = field_node->next;
                        }
                    }

                    FieldInfo* f = (FieldInfo*)malloc(sizeof(FieldInfo));
                    f->name = strdup(v->name);
                    f->type_name = NULL; // Variant data type could be stored here if needed
                    f->next = NULL;

                    if (last_var) last_var->next = f;
                    else s->fields = f; // Reusing fields for variants
                    last_var = f;
                    
                    var_node = var_node->next;
                }
                printf("[SEMANTIC] Registered ENUM: %s\n", edecl->name);
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
                Slot* newtype_slot = add_slot(ctx, nt->name, SLOT_NEWTYPE, 1, nt->target_type, node, 0, 0);
                semantic_define_named_type(ctx, newtype_slot, nt->name, nt->target_type);
                printf("[SEMANTIC] Registered NEWTYPE: %s -> %s\n", nt->name, nt->target_type);
            }
            break;
        }
        
        case AST_LOOP: {
            ASTLoop* loop = (ASTLoop*)node;
            enter_scope(ctx, node);

            // 1. Init / Variable Handling
            if (loop->loop_type == 3) { // Foreach: loop(var <-- collection)
                if (loop->variable) {
                    if (loop->variable->type == AST_VAR_DECL) {
                        semantic_check(ctx, loop->variable);
                    } else if (loop->variable->type == AST_IDENTIFIER) {
                        // Implicit declaration: v:name
                        ASTIdentifier* id = (ASTIdentifier*)loop->variable;
                        if (!find_slot(ctx, id->name)) {
                            add_slot(ctx, id->name, SLOT_VAR, 0, "any", (ASTNode*)id, 1, 0);
                            printf("[SEMANTIC] Implicit loop variable: %s\n", id->name);
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
                            add_slot(ctx, id->name, SLOT_VAR, 0, "i32", (ASTNode*)id, 1, 0);
                            printf("[SEMANTIC] Implicit loop counter: %s\n", id->name);
                        }
                        semantic_check(ctx, loop->init);
                    } else if (loop->init->type == AST_ASSIGNMENT) {
                        // Implicit: loop(i=0, ...) -> v:i=0
                        ASTBinaryExpr* assign = (ASTBinaryExpr*)loop->init;
                        if (assign->left->type == AST_IDENTIFIER) {
                            ASTIdentifier* id = (ASTIdentifier*)assign->left;
                            if (!find_slot(ctx, id->name)) {
                                const char* type = get_expr_type(ctx, assign->right);
                                add_slot(ctx, id->name, SLOT_VAR, 0, type ? type : "i32", (ASTNode*)id, 1, 0);
                                printf("[SEMANTIC] Implicit loop variable: %s\n", id->name);
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
                                add_slot(ctx, "it", SLOT_VAR, 0, "i32", node, 1, 0);
                                printf("[SEMANTIC] Implicit range variable: it\n");
                            }
                        }
                    }
                    semantic_check(ctx, loop->condition);
                }
                if (loop->increment) semantic_check(ctx, loop->increment);
            }

            if (loop->body) semantic_check(ctx, loop->body);
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
                add_slot(ctx, lbl->label_name, SLOT_LABEL, 1, NULL, node, 0, 0);
                printf("[SEMANTIC] Label defined: %s\n", lbl->label_name);
            }
            if (lbl->statement) semantic_check(ctx, lbl->statement);
            break;
        }

        case AST_NEXUS_JUMP: {
            ASTNexusJump* jmp = (ASTNexusJump*)node;
            // İleriye dönük atlamalar (forward jump) olabileceği için burada kesin varlık kontrolü yapmıyoruz
            printf("[SEMANTIC] Jump to label: %s\n", jmp->label_name);
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

        case AST_PANIC: {
            break;
        }

        case AST_LISTEN: {
            ASTNexusCapture* listen = (ASTNexusCapture*)node;
            // Listen bloğu içinde 'global_listener' slotu veya benzeri bir kapsam açılabilir
            enter_scope(ctx, node);
            add_slot(ctx, listen->handler_name, SLOT_VAR, 1, "any", node, 1, 0);
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
            enter_scope(ctx, node);
            if (g->members) semantic_check(ctx, g->members);
            exit_scope(ctx);
            printf("[SEMANTIC] Analyzed GROUP: %s\n", g->name);
            break;
        }

        case AST_METHOD: {
            ASTMethod* m = (ASTMethod*)node;
            if (m->implementation) semantic_check(ctx, m->implementation);
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
        case AST_CAST:
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

    if (node->next) semantic_check(ctx, node->next);
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