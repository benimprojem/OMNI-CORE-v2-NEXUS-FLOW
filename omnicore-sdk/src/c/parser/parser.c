#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// --- Message System ---
#define MAX_ERROR_CODE 4000
static char* GLOBAL_MSG_TABLE[MAX_ERROR_CODE];
static int msg_table_initialized = 0;

void msg_init(const char* path) {
    if (msg_table_initialized) return;
    
    memset(GLOBAL_MSG_TABLE, 0, sizeof(GLOBAL_MSG_TABLE));
    
    FILE* f = fopen(path, "r");
    if (!f) {
        printf("[WARN] Language file not found: %s. Using defaults/IDs.\n", path);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char* eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            int code = atoi(line);
            char* msg = eq + 1;
            size_t len = strlen(msg);
            if (len > 0 && msg[len-1] == '\n') msg[len-1] = '\0';
            
            if (code > 0 && code < MAX_ERROR_CODE) {
                if (GLOBAL_MSG_TABLE[code]) free(GLOBAL_MSG_TABLE[code]);
                GLOBAL_MSG_TABLE[code] = strdup(msg);
            }
        }
    }
    fclose(f);
    msg_table_initialized = 1;
}

const char* msg_get(int id) {
    if (id > 0 && id < MAX_ERROR_CODE && GLOBAL_MSG_TABLE[id]) {
        return GLOBAL_MSG_TABLE[id];
    }
    return "Unknown Message";
}

void msg_destroy(void) {
    int i;
    for (i = 0; i < MAX_ERROR_CODE; ++i) {
        free(GLOBAL_MSG_TABLE[i]);
        GLOBAL_MSG_TABLE[i] = NULL;
    }
    msg_table_initialized = 0;
}

// Dosyayı belleğe okuyan yardımcı fonksiyon
static char* read_file_content(const char* filename, size_t* out_len) {
    if (out_len) *out_len = 0;
    if (!filename) return NULL;

    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    long file_length = ftell(f);
    if (file_length < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    size_t length = (size_t)file_length;
    if (length == (size_t)-1) {
        fclose(f);
        return NULL;
    }

    char* buffer = (char*)malloc(length + 1u);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    if (length > 0 && fread(buffer, 1, length, f) != length) {
        free(buffer);
        fclose(f);
        return NULL;
    }

    if (ferror(f)) {
        free(buffer);
        fclose(f);
        return NULL;
    }

    buffer[length] = '\0';
    fclose(f);

    if (out_len) *out_len = length;
    return buffer;
}

static void parser_context_init(
    ParserContext* ctx, const char* filename, char* owned_source, size_t source_len
) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->filename = filename ? filename : "<memory>";
    ctx->file_handle = NULL;
    ctx->source_buffer = owned_source;
    ctx->source_len = source_len;
    ctx->current_line = 1;
    ctx->current_col = 1;
    ctx->token_start_line = 1;
    ctx->token_start_col = 1;
    ctx->diagnostic_stage_id = OCC_STAGE_PARSER;
}

ParserContext* parser_init_source(const char* filename, const uint8_t* source, size_t source_len) {
    ParserContext* ctx;
    char* copy;
    if ((!source && source_len != 0u) || source_len == SIZE_MAX) return NULL;
    if (source && memchr(source, '\0', source_len) != NULL) return NULL;
    ctx = (ParserContext*)malloc(sizeof(*ctx));
    if (!ctx) return NULL;
    copy = (char*)malloc(source_len + 1u);
    if (!copy) { free(ctx); return NULL; }
    if (source_len) memcpy(copy, source, source_len);
    copy[source_len] = '\0';
    parser_context_init(ctx, filename, copy, source_len);
    return ctx;
}

ParserContext* parser_init(const char* filename) {
    ParserContext* ctx;
    char* source;
    size_t source_len = 0u;
    if (!filename) return NULL;
    source = read_file_content(filename, &source_len);
    if (!source) return NULL;
    ctx = (ParserContext*)malloc(sizeof(*ctx));
    if (!ctx) {
        free(source);
        return NULL;
    }
    parser_context_init(ctx, filename, source, source_len);
    return ctx;
}

// --- Parser State & Helper Functions ---

typedef struct {
    ParserContext* ctx;
    Token current;
    Token previous;
    int had_error;
    int panic_mode;
} Parser;

static Token parser_next_token(ParserContext* ctx) {
    if (ctx->token_stream) {
        const Token* tokens = ctx->token_stream;
        if (ctx->token_stream_index < ctx->token_stream_count) {
            return tokens[ctx->token_stream_index++];
        }
        return tokens[ctx->token_stream_count - 1u];
    }
    return lexer_next_token(ctx);
}

static void parser_add_issue(ParserContext* ctx, ErrorCode code, int line, int col, const char* message, const char* suggestion, const char* hint, int similarity, int is_warning, const OccSourceSpan* source_span);

/* Token spans use byte offsets and byte-based columns, matching lexer advance(). */
static void parser_make_token_span(
    const ParserContext* ctx,
    const Token* token,
    OccSourceSpan* span
) {
    size_t start;
    size_t length;
    size_t i;
    uint32_t line;
    uint32_t column;

    if (!span) return;
    memset(span, 0, sizeof(*span));
    if (!ctx || !token || token->line <= 0 || token->col <= 0) return;

    start = token->source_offset;
    if (start > ctx->source_len) return;
    length = token->source_length;
    if (length > ctx->source_len - start) {
        length = ctx->source_len - start;
    }

    line = (uint32_t)token->line;
    column = (uint32_t)token->col;
    for (i = 0; i < length; ++i) {
        if (ctx->source_buffer[start + i] == '\n') {
            if (line == UINT32_MAX) return;
            ++line;
            column = 1u;
        } else {
            if (column == UINT32_MAX) return;
            ++column;
        }
    }

    span->start_offset = (uint64_t)start;
    span->end_offset = (uint64_t)(start + length);
    span->start_line = (uint32_t)token->line;
    span->start_column = (uint32_t)token->col;
    span->end_line = line;
    span->end_column = column;
    span->has_location = 1u;
}

static void error_at(Parser* p, Token* token, ErrorCode code, const char* message) {
    if (p->panic_mode) return;
    p->panic_mode = 1;
    if (token->type == TOKEN_ERROR && token->diagnostic_id != 0 && p->ctx->diagnostics) {
        /* The upstream OIR DIAGNOSTICS payload already owns this finding. */
        p->had_error = 1;
        return;
    }
    
    char full_msg[256];
    if (token->type == TOKEN_EOF) {
        snprintf(full_msg, sizeof(full_msg), msg_get(MSG_AT_END), message);
    } else if (token->type == TOKEN_ERROR) {
        snprintf(full_msg, sizeof(full_msg), "%s", message);
    } else {
        snprintf(full_msg, sizeof(full_msg), msg_get(MSG_AT_TOKEN), token->length, token->start, message);
    }

    {
        size_t diagnostic_count_before = p->ctx->diagnostics
            ? occ_diagnostics_count(p->ctx->diagnostics) : 0u;
        {
            OccSourceSpan span;
            parser_make_token_span(p->ctx, token, &span);
            parser_add_issue(p->ctx, code, token->line, token->col,
                full_msg, NULL, NULL, 0, 0, &span);
        }
        if (token->type == TOKEN_ERROR && p->ctx->diagnostics) {
            size_t diagnostic_count_after = occ_diagnostics_count(p->ctx->diagnostics);
            if (diagnostic_count_after > diagnostic_count_before &&
                diagnostic_count_before < UINT32_MAX) {
                token->diagnostic_id = (uint32_t)(diagnostic_count_before + 1u);
            }
        }
    }
    p->had_error = 1;
}

static void advance(Parser* p) {
    p->previous = p->current;
    for (;;) {
        p->current = parser_next_token(p->ctx);
        if (p->current.type != TOKEN_ERROR) break;
        error_at(p, &p->current, ERR_LEXER_ERROR, p->current.start);
    }
}

static void consume(Parser* p, OmniTokenType type, ErrorCode code, int msg_id) {
    if (p->current.type == type) {
        advance(p);
        return;
    }
    error_at(p, &p->current, code, msg_get(msg_id));
}

static int match(Parser* p, OmniTokenType type) {
    if (p->current.type != type) return 0;
    advance(p);
    return 1;
}

static int check(Parser* p, OmniTokenType type) {
    return p->current.type == type;
}

static Token parser_peek_n(Parser* p, int n) {
    Token token;
    size_t old_pos;
    size_t old_token_index;
    int old_line, old_col, old_token_line, old_token_col;
    size_t old_token_offset;
    int i;

    memset(&token, 0, sizeof(token));
    token.type = TOKEN_EOF;
    token.start = "EOF";
    if (!p || !p->ctx || n <= 0) return token;

    old_pos = p->ctx->pos;
    old_line = p->ctx->current_line;
    old_col = p->ctx->current_col;
    old_token_offset = p->ctx->token_start_offset;
    old_token_line = p->ctx->token_start_line;
    old_token_col = p->ctx->token_start_col;
    old_token_index = p->ctx->token_stream_index;

    for (i = 0; i < n; ++i) token = parser_next_token(p->ctx);

    p->ctx->pos = old_pos;
    p->ctx->current_line = old_line;
    p->ctx->current_col = old_col;
    p->ctx->token_start_offset = old_token_offset;
    p->ctx->token_start_line = old_token_line;
    p->ctx->token_start_col = old_token_col;
    p->ctx->token_stream_index = old_token_index;
    return token;
}

static Token peek_token(Parser* p) {
    return parser_peek_n(p, 1);
}

static Token peek_token_ahead(Parser* p, int n) {
    return parser_peek_n(p, n);
}

// --- Parsing Logic ---

static ASTNode* parse_expression(Parser* p);
static ASTNode* parse_statement(Parser* p);
static ASTNode* parse_block(Parser* p);
static ASTNode* parse_assignment(Parser* p);
static ASTNode* parse_func_decl(Parser* p);
static ASTNode* parse_nexus_flow(Parser* p);
static ASTNode* parse_return_statement(Parser* p);
static ASTNode* parse_var_decl(Parser* p);
static ASTNode* parse_struct_decl(Parser* p, int is_packed, char* name);
static ASTNode* parse_enum_decl(Parser* p, char* name);
static ASTNode* parse_union_decl(Parser* p, char* name);
static ASTNode* parse_newtype_decl(Parser* p);
static ASTNode* parse_group_decl(Parser* p);
static ASTNode* parse_postfix(Parser* p);
static ASTNode* parse_range(Parser* p);
static ASTNode* parse_equality(Parser* p);
static int parse_struct_body(Parser* p, ASTStructDecl* decl);
static ASTNode* parse_comparison(Parser* p);
static ASTNode* parse_additive(Parser* p);
static ASTNode* parse_unary(Parser* p);
static ASTNode* parse_intrinsic_call(Parser* p, ASTNodeType type);
static ASTNode* parse_defer(Parser* p);
static ASTNode* parse_use(Parser* p);
static ASTNode* parse_thread_block(Parser* p, char* name);

// Primary: Literals, Identifiers, Grouping
static ASTNode* parse_primary(Parser* p) {
    if (match(p, TOKEN_NUMBER) || match(p, TOKEN_STRING) || match(p, TOKEN_CHAR)) {
        Token token = p->previous;
        ASTLiteral* node = (ASTLiteral*)ast_create(AST_LITERAL);
        if (!node) return NULL;
        node->base.line = token.line;
        node->base.col = token.col;
        node->type = token.type;
        node->string_value = (char*)malloc((size_t)token.length + 1u);
        if (!node->string_value) {
            ast_destroy((ASTNode*)node);
            return NULL;
        }
        memcpy(node->string_value, token.start, (size_t)token.length);
        node->string_value[token.length] = '\0';
        return (ASTNode*)node;
    }

    if (match(p, TOKEN_TRUE) || match(p, TOKEN_FALSE)) {
        const char* value = p->previous.type == TOKEN_TRUE ? "true" : "false";
        ASTLiteral* node = (ASTLiteral*)ast_create(AST_LITERAL);
        if (!node) return NULL;
        node->base.line = p->previous.line;
        node->base.col = p->previous.col;
        node->type = p->previous.type;
        node->string_value = strdup(value);
        if (!node->string_value) {
            ast_destroy((ASTNode*)node);
            return NULL;
        }
        return (ASTNode*)node;
    }

    if (match(p, TOKEN_IDENTIFIER)) {
        ASTIdentifier* id_node = (ASTIdentifier*)ast_create(AST_IDENTIFIER);
        if (!id_node) return NULL;
        id_node->base.line = p->previous.line;
        id_node->base.col = p->previous.col;
        id_node->name = (char*)malloc((size_t)p->previous.length + 1u);
        if (!id_node->name) {
            ast_destroy((ASTNode*)id_node);
            return NULL;
        }
        memcpy(id_node->name, p->previous.start, (size_t)p->previous.length);
        id_node->name[p->previous.length] = '\0';
        return (ASTNode*)id_node;
    }

    if (match(p, TOKEN_KW_FN)) {
        return parse_func_decl(p);
    }

    if (match(p, TOKEN_KW_LOOP)) {
        ASTLoop* loop = (ASTLoop*)ast_create(AST_LOOP);
        if (!loop) return NULL;
        loop->base.line = p->previous.line;
        loop->base.col = p->previous.col;
        loop->loop_type = 0;

        consume(p, TOKEN_LPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_LPAREN_FUNC);

        // Argümanları parse et
        ASTNode* first = NULL;
        if (match(p, TOKEN_PRE_VAR)) {
            first = parse_var_decl(p);
        } else if (match(p, TOKEN_PRE_CONST)) {
            first = parse_var_decl(p);
        } else {
            first = parse_expression(p);
        }
        if (!first) {
            ast_destroy((ASTNode*)loop);
            return NULL;
        }

        if (match(p, TOKEN_ITERATE)) {
            loop->loop_type = 3;
            loop->variable = first;
            loop->collection = parse_expression(p);
            if (!loop->collection) {
                ast_destroy((ASTNode*)loop);
                return NULL;
            }
        } else if (match(p, TOKEN_COMMA)) {
            loop->loop_type = 2;
            loop->init = first;
            loop->condition = parse_expression(p);
            if (!loop->condition) {
                ast_destroy((ASTNode*)loop);
                return NULL;
            }
            consume(p, TOKEN_COMMA, ERR_PARSER_EXPECTED_EXPRESSION, MSG_EXPECT_COMMA_ROLLING);
            loop->increment = parse_expression(p);
            if (!loop->increment) {
                ast_destroy((ASTNode*)loop);
                return NULL;
            }
        } else {
            loop->loop_type = 1;
            loop->condition = first;
        }
        consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);

        if (match(p, TOKEN_PIPE)) { } // Opsiyonel ->
        
        if (match(p, TOKEN_LBRACE)) loop->body = parse_block(p);
        else error_at(p, &p->current, ERR_PARSER_EXPECTED_BRACE, msg_get(MSG_EXPECT_LBRACE_FUNC));
        return (ASTNode*)loop;
    }

    if (match(p, TOKEN_LPAREN)) {
        // (loop) Kontrolü
        if (match(p, TOKEN_KW_LOOP)) {
            consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);
            
            ASTLoop* loop = (ASTLoop*)ast_create(AST_LOOP);
            if (!loop) return NULL;
            loop->base.line = p->previous.line;
            loop->base.col = p->previous.col;
            loop->loop_type = 0; // Infinite default
            // Artık argüman beklemiyoruz, sadece (loop) sonsuz döngüdür.

            // -> { ... }
            if (match(p, TOKEN_PIPE)) {
                // Opsiyonel pipe
            }
            
            if (match(p, TOKEN_LBRACE)) {
                loop->body = parse_block(p);
            } else {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_BRACE, msg_get(MSG_EXPECT_LBRACE_FUNC));
            }
            return (ASTNode*)loop;
        }

        ASTNode* expr = parse_expression(p);
        if (!expr) return NULL;

        /* Tuple Check: (expr, expr, ...) */
        if (p->current.type == TOKEN_COMMA) {
            ASTTuple* tuple = (ASTTuple*)ast_create(AST_TUPLE);
            ASTNode* tail = expr;
            if (!tuple) {
                ast_destroy(expr);
                return NULL;
            }
            tuple->base.line = expr->line;
            tuple->base.col = expr->col;
            tuple->elements = expr;

            while (match(p, TOKEN_COMMA)) {
                ASTNode* next_elem = parse_expression(p);
                if (!next_elem) {
                    ast_destroy((ASTNode*)tuple);
                    return NULL;
                }
                tail->next = next_elem;
                tail = next_elem;
            }

            consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);
            return (ASTNode*)tuple;
        }

        consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);
        return expr;
    }

    if (match(p, TOKEN_KW_PRNT)) return parse_intrinsic_call(p, AST_PRNT);
    if (match(p, TOKEN_KW_PRMT)) return parse_intrinsic_call(p, AST_PRMT);
    if (match(p, TOKEN_KW_WRITE)) return parse_intrinsic_call(p, AST_WRITE);
    if (match(p, TOKEN_KW_FWRITE)) return parse_intrinsic_call(p, AST_FWRITE);
    if (match(p, TOKEN_KW_TYPEOF)) return parse_intrinsic_call(p, AST_TYPEOF);
    if (match(p, TOKEN_KW_SIZEOF)) return parse_intrinsic_call(p, AST_SIZEOF);
    if (match(p, TOKEN_KW_LEN)) return parse_intrinsic_call(p, AST_LEN);
    if (match(p, TOKEN_KW_IS_OK)) return parse_intrinsic_call(p, AST_IS_OK);
    if (match(p, TOKEN_KW_IS_ERR)) return parse_intrinsic_call(p, AST_IS_ERR);
    if (match(p, TOKEN_KW_IS_SOME)) return parse_intrinsic_call(p, AST_IS_SOME);
    if (match(p, TOKEN_KW_IS_NONE)) return parse_intrinsic_call(p, AST_IS_NONE);
    if (match(p, TOKEN_KW_IS_NULL)) return parse_intrinsic_call(p, AST_IS_NULL);
    if (match(p, TOKEN_KW_PANIC)) return parse_intrinsic_call(p, AST_PANIC);
    if (match(p, TOKEN_KW_EXIT)) return parse_intrinsic_call(p, AST_EXIT);
    if (match(p, TOKEN_KW_CAST)) return parse_intrinsic_call(p, AST_CAST);
    if (match(p, TOKEN_KW_SWAP)) return parse_intrinsic_call(p, AST_SWAP);
    if (match(p, TOKEN_KW_PEEK)) return parse_intrinsic_call(p, AST_PEEK);
    if (match(p, TOKEN_KW_POKE)) return parse_intrinsic_call(p, AST_POKE);
    if (match(p, TOKEN_KW_INB)) return parse_intrinsic_call(p, AST_INB);
    if (match(p, TOKEN_KW_OUTB)) return parse_intrinsic_call(p, AST_OUTB);
    if (match(p, TOKEN_KW_IRQ)) return parse_intrinsic_call(p, AST_IRQ);
    if (match(p, TOKEN_KW_INTR)) return parse_intrinsic_call(p, AST_INTR);
    if (match(p, TOKEN_KW_REG)) return parse_intrinsic_call(p, AST_REG);
    if (match(p, TOKEN_KW_AREA)) return parse_intrinsic_call(p, AST_AREA);
    if (match(p, TOKEN_KW_ZONE)) return parse_intrinsic_call(p, AST_ZONE);
    if (match(p, TOKEN_KW_ADDR)) return parse_intrinsic_call(p, AST_ADDR);
    if (match(p, TOKEN_KW_DATE)) return parse_intrinsic_call(p, AST_DATE);
    if (match(p, TOKEN_KW_DATENOW)) return parse_intrinsic_call(p, AST_DATENOW);
    if (match(p, TOKEN_KW_TIME)) return parse_intrinsic_call(p, AST_TIME);
    if (match(p, TOKEN_KW_CLOCK)) return parse_intrinsic_call(p, AST_CLOCK);
    if (match(p, TOKEN_KW_FREE)) return parse_intrinsic_call(p, AST_FREE);
    if (match(p, TOKEN_KW_SPAWN)) return parse_intrinsic_call(p, AST_SPAWN);
    if (match(p, TOKEN_KW_SEND)) return parse_intrinsic_call(p, AST_SEND);
    if (match(p, TOKEN_KW_RECEIVE)) return parse_intrinsic_call(p, AST_RECEIVE);
    if (match(p, TOKEN_KW_DONE)) return parse_intrinsic_call(p, AST_DONE);
    if (match(p, TOKEN_KW_LEN)) return parse_intrinsic_call(p, AST_LEN);

    // Support for { expr, expr } (Struct Init / Tuple)
    if (match(p, TOKEN_LBRACE)) {
        // { } Empty tuple check
        if (match(p, TOKEN_RBRACE)) {
             ASTTuple* tuple = (ASTTuple*)ast_create(AST_TUPLE);
             if (!tuple) return NULL;
             tuple->base.line = p->previous.line;
             tuple->base.col = p->previous.col;
             tuple->elements = NULL;
             return (ASTNode*)tuple;
        }

        ASTNode* expr = parse_expression(p);
        if (!expr) return NULL;
        
        ASTTuple* tuple = (ASTTuple*)ast_create(AST_TUPLE);
        ASTNode* tail = expr;
        if (!tuple) {
            ast_destroy(expr);
            return NULL;
        }
        tuple->base.line = expr->line;
        tuple->base.col = expr->col;
        tuple->elements = expr;

        while (match(p, TOKEN_COMMA)) {
            ASTNode* next_elem = parse_expression(p);
            if (!next_elem) {
                ast_destroy((ASTNode*)tuple);
                return NULL;
            }
            tail->next = next_elem;
            tail = next_elem;
        }
        
        consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
        return (ASTNode*)tuple;
    }

    if (match(p, TOKEN_AI)) {
        ASTAiLogic* ai = (ASTAiLogic*)ast_create(AST_AI_LOGIC);
        ai->base.line = p->previous.line;
        ai->base.col = p->previous.col;
        if (p->current.type == TOKEN_STRING || p->current.type == TOKEN_IDENTIFIER) {
            Token t = p->current;
            ai->query = malloc(t.length + 1);
            memcpy(ai->query, t.start, t.length);
            ai->query[t.length] = '\0';
            advance(p);
        } else {
            ai->query = strdup("auto");
        }
        return (ASTNode*)ai;
    }

    error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(ERR_PARSER_EXPECTED_EXPRESSION));
    return NULL;
}

static ASTNode* parse_intrinsic_call(Parser* p, ASTNodeType type) {
    if (type == AST_CAST) {
        ASTCast* cast = (ASTCast*)ast_create(AST_CAST);
        Token type_token;
        ASTIdentifier* target;
        if (!cast) return NULL;
        cast->base.line = p->previous.line;
        cast->base.col = p->previous.col;
        if (!match(p, TOKEN_LPAREN)) {
            error_at(p, &p->current, ERR_PARSER_EXPECTED_PAREN, msg_get(MSG_EXPECT_LPAREN_FUNC));
            return (ASTNode*)cast;
        }
        cast->value = parse_expression(p);
        consume(p, TOKEN_COMMA, ERR_PARSER_UNEXPECTED_TOKEN, MSG_EXPECT_COMMA_ROLLING);
        if (p->current.type == TOKEN_IDENTIFIER ||
            (p->current.type >= TOKEN_TYPE_U8 && p->current.type <= TOKEN_TYPE_ANY)) {
            type_token = p->current;
            advance(p);
            target = (ASTIdentifier*)ast_create(AST_IDENTIFIER);
            if (!target) {
                ast_destroy((ASTNode*)cast);
                return NULL;
            }
            target->base.line = type_token.line;
            target->base.col = type_token.col;
            target->name = (char*)malloc((size_t)type_token.length + 1u);
            if (!target->name) {
                free(target);
                ast_destroy((ASTNode*)cast);
                return NULL;
            }
            memcpy(target->name, type_token.start, (size_t)type_token.length);
            target->name[type_token.length] = '\0';
            cast->target_type = (ASTNode*)target;
        } else {
            error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_RET_TYPE));
        }
        consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_ARGS);
        return (ASTNode*)cast;
    }

    if (type == AST_PANIC) {
        ASTPanic* panic = (ASTPanic*)ast_create(AST_PANIC);
        if (!panic) return NULL;
        panic->base.line = p->previous.line;
        panic->base.col = p->previous.col;
        if (match(p, TOKEN_LPAREN)) {
            if (match(p, TOKEN_STRING)) {
                Token message = p->previous;
                panic->message = (char*)malloc((size_t)message.length + 1u);
                if (!panic->message) {
                    free(panic);
                    return NULL;
                }
                memcpy(panic->message, message.start, (size_t)message.length);
                panic->message[message.length] = '\0';
            } else {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(ERR_PARSER_EXPECTED_EXPRESSION));
            }
            consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_ARGS);
        }
        return (ASTNode*)panic;
    }

    ASTCall* call = (ASTCall*)ast_create(type);
    if (!call) return NULL;
    call->base.line = p->previous.line;
    call->base.col = p->previous.col;
    call->args = NULL;
    call->callee = NULL;

    /* reg.r(...) form: build the member identifier with checked ownership. */
    if (type == AST_REG && match(p, TOKEN_DOT)) {
        if (match(p, TOKEN_IDENTIFIER)) {
            Token member = p->previous;
            ASTIdentifier* id = (ASTIdentifier*)ast_create(AST_IDENTIFIER);
            if (!id) {
                ast_destroy((ASTNode*)call);
                return NULL;
            }
            id->name = (char*)malloc((size_t)member.length + 1u);
            if (!id->name) {
                ast_destroy((ASTNode*)id);
                ast_destroy((ASTNode*)call);
                return NULL;
            }
            memcpy(id->name, member.start, (size_t)member.length);
            id->name[member.length] = '\0';
            id->base.line = member.line;
            id->base.col = member.col;
            call->callee = (ASTNode*)id;
        } else {
            error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                msg_get(ERR_PARSER_EXPECTED_EXPRESSION));
            ast_destroy((ASTNode*)call);
            return NULL;
        }
    }

    if (match(p, TOKEN_LPAREN)) {
        if (p->current.type != TOKEN_RPAREN) {
            ASTNode* tail = NULL;
            do {
                ASTNode* arg = parse_expression(p);
                if (!arg) {
                    ast_destroy((ASTNode*)call);
                    return NULL;
                }
                if (!call->args) call->args = arg;
                else tail->next = arg;
                tail = arg;
            } while (match(p, TOKEN_COMMA));
        }
        consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_ARGS);
    }
    return (ASTNode*)call;
}

static ASTNode* parse_defer(Parser* p) {
    ASTDefer* defer = (ASTDefer*)ast_create(AST_DEFER);
    if (!defer) return NULL;
    defer->base.line = p->previous.line;
    defer->base.col = p->previous.col;

    if (match(p, TOKEN_LBRACE)) defer->body = parse_block(p);
    else defer->body = parse_statement(p);

    if (!defer->body) {
        ast_destroy((ASTNode*)defer);
        return NULL;
    }
    return (ASTNode*)defer;
}

static ASTNode* parse_use(Parser* p) {
    ASTUse* head = NULL;
    ASTUse* tail = NULL;

    do {
        Token module_token;
        ASTUse* use;
        if (!match(p, TOKEN_IDENTIFIER)) {
            error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                msg_get(MSG_EXPECT_MODULE_NAME));
            ast_destroy((ASTNode*)head);
            return NULL;
        }
        module_token = p->previous;
        use = (ASTUse*)ast_create(AST_USE);
        if (!use) {
            ast_destroy((ASTNode*)head);
            return NULL;
        }
        use->base.line = module_token.line;
        use->base.col = module_token.col;
        use->module_name = (char*)malloc((size_t)module_token.length + 1u);
        if (!use->module_name) {
            ast_destroy((ASTNode*)use);
            ast_destroy((ASTNode*)head);
            return NULL;
        }
        memcpy(use->module_name, module_token.start, (size_t)module_token.length);
        use->module_name[module_token.length] = '\0';

        if (match(p, TOKEN_KW_AS)) {
            Token alias_token;
            if (!match(p, TOKEN_IDENTIFIER)) {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                    msg_get(MSG_EXPECT_ALIAS_NAME));
                ast_destroy((ASTNode*)use);
                ast_destroy((ASTNode*)head);
                return NULL;
            }
            alias_token = p->previous;
            use->alias = (char*)malloc((size_t)alias_token.length + 1u);
            if (!use->alias) {
                ast_destroy((ASTNode*)use);
                ast_destroy((ASTNode*)head);
                return NULL;
            }
            memcpy(use->alias, alias_token.start, (size_t)alias_token.length);
            use->alias[alias_token.length] = '\0';
        }

        if (!head) head = use;
        else tail->base.next = (ASTNode*)use;
        tail = use;
    } while (match(p, TOKEN_COMMA));

    consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
    return (ASTNode*)head;
}

static ASTNode* parse_thread_block(Parser* p, char* name) {
    ASTThreadBlock* tb = (ASTThreadBlock*)ast_create(AST_THREAD_BLOCK);
    if (!tb) {
        free(name);
        return NULL;
    }
    tb->base.line = p->previous.line;
    tb->base.col = p->previous.col;
    tb->name = name;
    tb->mode = NULL;
    tb->timeout = NULL;
    tb->body = parse_block(p);
    if (!tb->body) {
        ast_destroy((ASTNode*)tb);
        return NULL;
    }
    return (ASTNode*)tb;
}

static ASTNode* parse_postfix(Parser* p) {
    ASTNode* expr = parse_primary(p);
    if (!expr) return NULL;

    for (;;) {
        if (match(p, TOKEN_DOT)) {
            if (!match(p, TOKEN_IDENTIFIER)) {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(ERR_PARSER_EXPECTED_EXPRESSION));
                ast_destroy(expr);
                return NULL;
            }
            Token member = p->previous;
            ASTMemberAccess* node = (ASTMemberAccess*)ast_create(AST_MEMBER_ACCESS);
            if (!node) {
                ast_destroy(expr);
                return NULL;
            }
            node->base.line = member.line;
            node->base.col = member.col;
            node->object = expr;
            node->member_name = (char*)malloc((size_t)member.length + 1u);
            if (!node->member_name) {
                ast_destroy((ASTNode*)node);
                return NULL;
            }
            memcpy(node->member_name, member.start, (size_t)member.length);
            node->member_name[member.length] = '\0';
            expr = (ASTNode*)node;
        } else if (match(p, TOKEN_LPAREN)) {
            ASTCall* call = (ASTCall*)ast_create(AST_FUNC_CALL);
            if (!call) {
                ast_destroy(expr);
                return NULL;
            }
            call->base.line = p->previous.line;
            call->base.col = p->previous.col;
            call->callee = expr;
            call->args = NULL;

            if (p->current.type != TOKEN_RPAREN) {
                ASTNode* tail = NULL;
                do {
                    ASTNode* arg = parse_expression(p);
                    if (!arg) {
                        ast_destroy((ASTNode*)call);
                        return NULL;
                    }
                    if (!call->args) call->args = arg;
                    else tail->next = arg;
                    tail = arg;
                } while (match(p, TOKEN_COMMA));
            }
            consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_ARGS);
            expr = (ASTNode*)call;
        } else if (match(p, TOKEN_INC) || match(p, TOKEN_DEC)) {
            ASTUnaryExpr* post = (ASTUnaryExpr*)ast_create(AST_POSTFIX_EXPR);
            if (!post) {
                ast_destroy(expr);
                return NULL;
            }
            post->base.line = p->previous.line;
            post->base.col = p->previous.col;
            post->op = p->previous.type;
            post->operand = expr;
            expr = (ASTNode*)post;
        } else {
            break;
        }
    }
    return expr;
}

static ASTNode* parse_unary(Parser* p) {
    if (match(p, TOKEN_INC) || match(p, TOKEN_DEC) ||
        match(p, TOKEN_NOT) || match(p, TOKEN_MINUS) || match(p, TOKEN_BIT_NOT)) {
        OmniTokenType op = p->previous.type;
        ASTNode* operand = parse_unary(p);
        ASTUnaryExpr* unary;
        if (!operand) return NULL;
        unary = (ASTUnaryExpr*)ast_create(AST_UNARY_EXPR);
        if (!unary) {
            ast_destroy(operand);
            return NULL;
        }
        unary->base.line = p->previous.line;
        unary->base.col = p->previous.col;
        unary->op = op;
        unary->operand = operand;
        return (ASTNode*)unary;
    }
    return parse_postfix(p);
}

// Binary expression construction keeps ownership of both operands on every path.
static ASTNode* parser_make_binary(Parser* p, OmniTokenType op, ASTNode* left, ASTNode* right) {
    ASTBinaryExpr* binary = (ASTBinaryExpr*)ast_create(AST_BINARY_EXPR);
    if (!binary) {
        ast_destroy(left);
        ast_destroy(right);
        return NULL;
    }
    binary->base.line = p->previous.line;
    binary->base.col = p->previous.col;
    binary->left = left;
    binary->right = right;
    binary->op = op;
    return (ASTNode*)binary;
}

static ASTNode* parse_term(Parser* p) {
    ASTNode* left = parse_unary(p);
    if (!left) return NULL;
    while (match(p, TOKEN_STAR) || match(p, TOKEN_SLASH)) {
        OmniTokenType op = p->previous.type;
        ASTNode* right = parse_unary(p);
        if (!right) {
            ast_destroy(left);
            return NULL;
        }
        left = parser_make_binary(p, op, left, right);
        if (!left) return NULL;
    }
    return left;
}

static ASTNode* parse_comparison(Parser* p) {
    ASTNode* left = parse_additive(p);
    if (!left) return NULL;
    while (match(p, TOKEN_GT) || match(p, TOKEN_GTE) ||
           match(p, TOKEN_LT) || match(p, TOKEN_LTE)) {
        OmniTokenType op = p->previous.type;
        ASTNode* right = parse_additive(p);
        if (!right) {
            ast_destroy(left);
            return NULL;
        }
        left = parser_make_binary(p, op, left, right);
        if (!left) return NULL;
    }
    return left;
}

static ASTNode* parse_equality(Parser* p) {
    ASTNode* left = parse_comparison(p);
    if (!left) return NULL;
    while (match(p, TOKEN_EQ) || match(p, TOKEN_NEQ) ||
           match(p, TOKEN_STRICT_EQ) || match(p, TOKEN_FORCE_EQ)) {
        OmniTokenType op = p->previous.type;
        ASTNode* right = parse_comparison(p);
        if (!right) {
            ast_destroy(left);
            return NULL;
        }
        left = parser_make_binary(p, op, left, right);
        if (!left) return NULL;
    }
    return left;
}

static ASTNode* parse_additive(Parser* p) {
    ASTNode* left = parse_term(p);
    if (!left) return NULL;
    while (match(p, TOKEN_PLUS) || match(p, TOKEN_MINUS)) {
        OmniTokenType op = p->previous.type;
        ASTNode* right = parse_term(p);
        if (!right) {
            ast_destroy(left);
            return NULL;
        }
        left = parser_make_binary(p, op, left, right);
        if (!left) return NULL;
    }
    return left;
}

static ASTNode* parse_range(Parser* p) {
    ASTNode* expr = parse_equality(p);
    if (!expr) return NULL;
    if (match(p, TOKEN_RANGE)) {
        ASTNode* right = parse_equality(p);
        if (!right) {
            ast_destroy(expr);
            return NULL;
        }
        return parser_make_binary(p, TOKEN_RANGE, expr, right);
    }
    return expr;
}

static ASTNode* parse_nexus_flow(Parser* p) {
    ASTNode* expr = parse_range(p);
    if (!expr) return NULL;
    
        if (match(p, TOKEN_ELSE_FLOW) || match(p, TOKEN_PIPE) || 
            match(p, TOKEN_IGNORE) || match(p, TOKEN_IF_FLOW) || 
            match(p, TOKEN_FALLBACK) || match(p, TOKEN_RELOCATE) ||
            match(p, TOKEN_LSHIFT) || match(p, TOKEN_RSHIFT) ||
            p->current.type == TOKEN_QMARK) {
            
        OmniTokenType op;
        int retry_count = 0;
        int retry_delay = 0;

        // Rolling Check: ?(n,ms)->
        if (p->current.type == TOKEN_QMARK) {
            advance(p); // Consume ?
            if (match(p, TOKEN_LPAREN)) {
                if (match(p, TOKEN_NUMBER)) retry_count = atoi(p->previous.start);
                consume(p, TOKEN_COMMA, ERR_PARSER_EXPECTED_EXPRESSION, MSG_EXPECT_COMMA_ROLLING);
                if (match(p, TOKEN_NUMBER)) retry_delay = atoi(p->previous.start);
                consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_ROLLING);
                consume(p, TOKEN_PIPE, ERR_PARSER_UNEXPECTED_TOKEN, MSG_EXPECT_PIPE_ROLLING);
                op = TOKEN_PIPE;
            } else {
                error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN, msg_get(MSG_UNEXPECTED_QMARK));
                return expr;
            }
        } else {
            op = p->previous.type;
        }

        ASTNode* right = parse_range(p);
        if (!right) {
            ast_destroy(expr);
            return NULL;
        }
        
        if (op == TOKEN_PIPE || op == TOKEN_LSHIFT || op == TOKEN_RSHIFT || op == TOKEN_RELOCATE) {
            ASTNexusFlow* pipe = (ASTNexusFlow*)ast_create(AST_NEXUS_PIPE);
            if (!pipe) {
                ast_destroy(expr);
                ast_destroy(right);
                return NULL;
            }
            pipe->base.line = p->previous.line;
            pipe->base.col = p->previous.col;
            pipe->op = op;
            pipe->left = expr;
            pipe->right = right;
            expr = (ASTNode*)pipe;
        } else {
            ASTNexusFlow* flow = (ASTNexusFlow*)ast_create(AST_NEXUS_FLOW);
            if (!flow) {
                ast_destroy(expr);
                ast_destroy(right);
                return NULL;
            }
            flow->base.line = p->previous.line;
            flow->base.col = p->previous.col;
            flow->op = op;
            flow->retry_count = retry_count;
            flow->retry_delay = retry_delay;
            flow->left = expr;
            flow->right = right;
            expr = (ASTNode*)flow;
        }
    }

    if (match(p, TOKEN_UNPACK)) {
        ASTUnpack* unpack = (ASTUnpack*)ast_create(AST_UNPACK);
        if (!unpack) {
            ast_destroy(expr);
            return NULL;
        }
        unpack->base.line = p->previous.line;
        unpack->base.col = p->previous.col;
        unpack->expr = expr;
        expr = (ASTNode*)unpack;
        return parse_nexus_flow(p);
    }

    if (match(p, TOKEN_ERR_HANDLER)) {
        if (expr->type == AST_NEXUS_FLOW || expr->type == AST_NEXUS_PIPE) {
            ASTNexusFlow* flow = (ASTNexusFlow*)expr;
            flow->catch_block = parse_block(p);
        } else {
             error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN, "Hata yakalayıcı (e) sadece akış (flow) operatörlerinden sonra gelebilir.");
        }
    }

    return expr;
}

static ASTNode* parse_assignment(Parser* p) {
    ASTNode* expr = parse_nexus_flow(p);
    if (!expr) return NULL;

    if (match(p, TOKEN_CAPTURE)) {
        // Nexus Capture: left <- right
        if (expr->type != AST_IDENTIFIER) {
             error_at(p, &p->previous, ERR_PARSER_INVALID_VAR, msg_get(MSG_INVALID_CAPTURE));
             return expr;
        }
        
        ASTNexusCapture* capture = (ASTNexusCapture*)ast_create(AST_NEXUS_CAPTURE);
        if (!capture) {
            ast_destroy(expr);
            return NULL;
        }
        capture->base.line = p->previous.line;
        capture->base.col = p->previous.col;
        capture->handler_name = strdup(((ASTIdentifier*)expr)->name);
        if (!capture->handler_name) {
            ast_destroy((ASTNode*)capture);
            ast_destroy(expr);
            return NULL;
        }
        capture->expr = parse_assignment(p);
        ast_destroy(expr);
        if (!capture->expr) {
            ast_destroy((ASTNode*)capture);
            return NULL;
        }
        return (ASTNode*)capture;
    }

    if (match(p, TOKEN_ASSIGN)) {
        ASTNode* right = parse_assignment(p);
        if (!right) {
            ast_destroy(expr);
            return NULL;
        }
        ASTBinaryExpr* assign = (ASTBinaryExpr*)ast_create(AST_ASSIGNMENT);
        if (!assign) {
            ast_destroy(expr);
            ast_destroy(right);
            return NULL;
        }
        assign->base.line = p->previous.line;
        assign->base.col = p->previous.col;
        assign->op = TOKEN_ASSIGN;
        assign->left = expr;
        assign->right = right;
        return (ASTNode*)assign;
    }

    if (match(p, TOKEN_FORCE_ASSIGN)) {
        ASTNode* right = parse_assignment(p);
        if (!right) {
            ast_destroy(expr);
            return NULL;
        }
        ASTBinaryExpr* assign = (ASTBinaryExpr*)ast_create(AST_ASSIGNMENT);
        if (!assign) {
            ast_destroy(expr);
            ast_destroy(right);
            return NULL;
        }
        assign->base.line = p->previous.line;
        assign->base.col = p->previous.col;
        assign->op = TOKEN_FORCE_ASSIGN;
        assign->left = expr;
        assign->right = right;
        return (ASTNode*)assign;
    }
    
    return expr;
}

static ASTNode* parse_expression(Parser* p) {
    // İfade hiyerarşisi: Assignment/Capture -> Nexus Flow -> Additive ...
    return parse_assignment(p);
}

static int parse_struct_body(Parser* p, ASTStructDecl* decl) {
    consume(p, TOKEN_LBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_LBRACE_FUNC);
    ASTNode* head = NULL;
    ASTNode* tail = NULL;
    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        if (match(p, TOKEN_PRE_VAR)) {
            ASTNode* field = parse_var_decl(p);
            if (!field) { ast_destroy(head); return 0; }
            if (!head) head = field; else tail->next = field;
            tail = field;
            if (match(p, TOKEN_COMMA)) {}
        } else advance(p);
    }
    decl->fields = head;
    consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
    return 1;
}

// Variable Declaration: v:name = expr;
static ASTNode* parse_var_decl(Parser* p) {
    // v: veya c: tüketildi, p->previous içinde
    int is_const = (p->previous.type == TOKEN_PRE_CONST);
    Token t = p->previous;

    // Lexer "v:isim" şeklinde tek token döndürdüğü için ismi ayıklıyoruz
    int prefix_len = (t.type == TOKEN_PRE_VAR || t.type == TOKEN_PRE_CONST) ? 2 : 0;
    
    ASTVarDecl* decl = (ASTVarDecl*)ast_create(AST_VAR_DECL);
    if (!decl) return NULL;
    decl->base.line = t.line;
    decl->base.col = t.col;
    decl->is_const = is_const;
    decl->inline_type = NULL;
    
    if (t.length > prefix_len) {
        decl->name = (char*)malloc((size_t)(t.length - prefix_len) + 1u);
        if (!decl->name) { ast_destroy((ASTNode*)decl); return NULL; }
        memcpy(decl->name, t.start + prefix_len, (size_t)(t.length - prefix_len)); 
        decl->name[t.length - prefix_len] = '\0';
    } else if (t.length == prefix_len) {
        // Separated: v: name
        if (match(p, TOKEN_IDENTIFIER)) {
            Token id = p->previous;
            decl->name = (char*)malloc((size_t)id.length + 1u);
            if (!decl->name) { ast_destroy((ASTNode*)decl); return NULL; }
            memcpy(decl->name, id.start, (size_t)id.length);
            decl->name[id.length] = '\0';
        } else {
            error_at(p, &p->current, ERR_PARSER_INVALID_VAR, msg_get(ERR_PARSER_INVALID_VAR));
        }
    } else {
        error_at(p, &p->previous, ERR_PARSER_INVALID_VAR, msg_get(ERR_PARSER_INVALID_VAR));
    }

    // Tip Tanımı: !Type
    if (match(p, TOKEN_NOT)) {
        decl->is_hard_typed = 1;
        
        if (match(p, TOKEN_KW_STRUCT)) {
             decl->type_name = strdup("struct");
             if (!decl->type_name) { ast_destroy((ASTNode*)decl); return NULL; }
             if (p->current.type == TOKEN_LBRACE) {
                 ASTStructDecl* sdecl = (ASTStructDecl*)ast_create(AST_STRUCT_DECL);
                 if (!sdecl) { ast_destroy((ASTNode*)decl); return NULL; }
                 sdecl->base.line = p->previous.line;
                 sdecl->base.col = p->previous.col;
                 parse_struct_body(p, sdecl);
                 decl->inline_type = (ASTNode*)sdecl;
             }
        } else if (match(p, TOKEN_KW_UNION)) {
             decl->type_name = strdup("union");
             if (!decl->type_name) { ast_destroy((ASTNode*)decl); return NULL; }
             if (p->current.type == TOKEN_LBRACE) {
                 ASTStructDecl* sdecl = (ASTStructDecl*)ast_create(AST_UNION_DECL);
                 if (!sdecl) { ast_destroy((ASTNode*)decl); return NULL; }
                 sdecl->base.line = p->previous.line;
                 sdecl->base.col = p->previous.col;
                 sdecl->is_union = 1;
                 parse_struct_body(p, sdecl);
                 decl->inline_type = (ASTNode*)sdecl;
             }
        } else if ((p->current.type >= TOKEN_TYPE_U8 && p->current.type <= TOKEN_TYPE_ANY) || p->current.type == TOKEN_IDENTIFIER) {
             Token type_token = p->current;
             decl->type_name = (char*)malloc((size_t)type_token.length + 1u);
             if (!decl->type_name) { ast_destroy((ASTNode*)decl); return NULL; }
             memcpy(decl->type_name, type_token.start, (size_t)type_token.length);
             decl->type_name[type_token.length] = '\0';
             advance(p);
        } else {
             error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_RET_TYPE));
        }
    }

    if (match(p, TOKEN_ASSIGN)) {
        decl->initializer = parse_expression(p);
        if (!decl->initializer) {
            ast_destroy((ASTNode*)decl);
            return NULL;
        }
    }

    if (!decl->name) {
        ast_destroy((ASTNode*)decl);
        return NULL;
    }
    return (ASTNode*)decl;
}

static ASTNode* parse_return_statement(Parser* p) {
    ASTUnaryExpr* ret = (ASTUnaryExpr*)ast_create(AST_RETURN);
    if (!ret) return NULL;
    ret->base.line = p->previous.line;
    ret->base.col = p->previous.col;
    ret->op = TOKEN_KW_RET;
    
    if (!match(p, TOKEN_SEMICOLON)) {
        ret->operand = parse_expression(p);
        if (!ret->operand) {
            ast_destroy((ASTNode*)ret);
            return NULL;
        }
        consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_RET);
    } else {
        ret->operand = NULL;
    }
    return (ASTNode*)ret;
}

static ASTNode* parse_func_decl(Parser* p) {
    ASTFuncDecl* func = (ASTFuncDecl*)ast_create(AST_FUNC_DECL);
    ASTNode* param_tail = NULL;
    if (!func) return NULL;
    func->base.line = p->previous.line;
    func->base.col = p->previous.col;

    if (p->previous.type == TOKEN_PRE_FUNC) {
        Token t = p->previous;
        if (t.length < 2) {
            ast_destroy((ASTNode*)func);
            return NULL;
        }
        func->name = (char*)malloc((size_t)t.length - 1u);
        if (!func->name) {
            ast_destroy((ASTNode*)func);
            return NULL;
        }
        memcpy(func->name, t.start + 2, (size_t)t.length - 2u);
        func->name[t.length - 2] = '\0';
    } else if (match(p, TOKEN_IDENTIFIER)) {
        Token t = p->previous;
        func->name = (char*)malloc((size_t)t.length + 1u);
        if (!func->name) {
            ast_destroy((ASTNode*)func);
            return NULL;
        }
        memcpy(func->name, t.start, (size_t)t.length);
        func->name[t.length] = '\0';
    }

    consume(p, TOKEN_LPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_LPAREN_FUNC);

    if (p->current.type != TOKEN_RPAREN) {
        do {
            ASTVarDecl* param;
            Token param_token;
            int has_bang = 0;
            if (!match(p, TOKEN_IDENTIFIER)) {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                    msg_get(MSG_EXPECT_PARAM_NAME));
                ast_destroy((ASTNode*)func);
                return NULL;
            }
            param_token = p->previous;
            param = (ASTVarDecl*)ast_create(AST_VAR_DECL);
            if (!param) {
                ast_destroy((ASTNode*)func);
                return NULL;
            }
            param->base.line = param_token.line;
            param->base.col = param_token.col;
            param->is_const = 0;
            param->name = (char*)malloc((size_t)param_token.length + 1u);
            if (!param->name) {
                ast_destroy((ASTNode*)param);
                ast_destroy((ASTNode*)func);
                return NULL;
            }
            memcpy(param->name, param_token.start, (size_t)param_token.length);
            param->name[param_token.length] = '\0';

            if (!func->params) func->params = (ASTNode*)param;
            else param_tail->next = (ASTNode*)param;
            param_tail = (ASTNode*)param;

            if (match(p, TOKEN_NOT)) has_bang = 1;
            if ((p->current.type >= TOKEN_TYPE_U8 && p->current.type <= TOKEN_TYPE_ANY) ||
                p->current.type == TOKEN_IDENTIFIER) {
                Token type_token = p->current;
                size_t type_length = (size_t)type_token.length;
                size_t total_length = type_length + (has_bang ? 1u : 0u);
                param->type_name = (char*)malloc(total_length + 1u);
                if (!param->type_name) {
                    ast_destroy((ASTNode*)func);
                    return NULL;
                }
                if (has_bang) param->type_name[0] = '!';
                memcpy(param->type_name + (has_bang ? 1u : 0u),
                    type_token.start, type_length);
                param->type_name[total_length] = '\0';
                param->is_hard_typed = has_bang;
                advance(p);
            }
        } while (match(p, TOKEN_COMMA));
    }

    consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_PARAMS);

    /* Lambda body: > expression */
    if (match(p, TOKEN_GT)) {
        ASTNode* expr = parse_expression(p);
        ASTBlock* block;
        ASTUnaryExpr* ret;
        if (!expr) {
            ast_destroy((ASTNode*)func);
            return NULL;
        }
        block = (ASTBlock*)ast_create(AST_BLOCK);
        if (!block) {
            ast_destroy(expr);
            ast_destroy((ASTNode*)func);
            return NULL;
        }
        ret = (ASTUnaryExpr*)ast_create(AST_RETURN);
        if (!ret) {
            ast_destroy(expr);
            ast_destroy((ASTNode*)block);
            ast_destroy((ASTNode*)func);
            return NULL;
        }
        block->base.line = func->base.line;
        block->base.col = func->base.col;
        ret->base.line = func->base.line;
        ret->base.col = func->base.col;
        ret->op = TOKEN_KW_RET;
        ret->operand = expr;
        block->statements = (ASTNode*)ret;
        func->body = (ASTNode*)block;
        return (ASTNode*)func;
    }

    {
        int has_bang = 0;
        if (match(p, TOKEN_NOT)) has_bang = 1;
        if ((p->current.type >= TOKEN_TYPE_U8 && p->current.type <= TOKEN_TYPE_ANY) ||
            p->current.type == TOKEN_IDENTIFIER) {
            Token type_token = p->current;
            size_t type_length = (size_t)type_token.length;
            size_t total_length = type_length + (has_bang ? 1u : 0u);
            func->return_type = (char*)malloc(total_length + 1u);
            if (!func->return_type) {
                ast_destroy((ASTNode*)func);
                return NULL;
            }
            if (has_bang) func->return_type[0] = '!';
            memcpy(func->return_type + (has_bang ? 1u : 0u),
                type_token.start, type_length);
            func->return_type[total_length] = '\0';
            advance(p);
        } else if (has_bang) {
            error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                msg_get(MSG_EXPECT_TYPE_BANG));
            ast_destroy((ASTNode*)func);
            return NULL;
        }
    }

    consume(p, TOKEN_LBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_LBRACE_FUNC);
    func->body = parse_block(p);
    if (!func->body) {
        ast_destroy((ASTNode*)func);
        return NULL;
    }
    return (ASTNode*)func;
}

static ASTNode* parse_block(Parser* p) {
    ASTBlock* block = (ASTBlock*)ast_create(AST_BLOCK);
    if (!block) return NULL;
    block->base.line = p->current.line; // { karakterinin konumu
    block->base.col = p->current.col;
    ASTNode* head = NULL;
    ASTNode* tail = NULL;

    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        ASTNode* stmt = parse_statement(p);
        if (stmt) {
            if (!head) head = stmt;
            else tail->next = stmt;
            tail = stmt;
        } else {
            advance(p);
        }
    }
    consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
    block->statements = head;
    return (ASTNode*)block;
}

static ASTNode* parse_struct_decl(Parser* p, int is_packed, char* name) {
    ASTStructDecl* decl = (ASTStructDecl*)ast_create(AST_STRUCT_DECL);
    if (!decl) { free(name); return NULL; }
    decl->base.line = p->previous.line;
    decl->base.col = p->previous.col;
    decl->is_packed = is_packed;
    decl->is_union = 0;
    if (name && name[0] != '\0') decl->name = name;
    else {
        free(name);
        if (p->previous.type == TOKEN_PRE_STRUCT) {
            Token t = p->previous;
            if (t.length > 7) {
                size_t n = (size_t)t.length - 7u;
                decl->name = (char*)malloc(n + 1u);
                if (decl->name) { memcpy(decl->name, t.start + 7, n); decl->name[n] = '\0'; }
            } else if (match(p, TOKEN_IDENTIFIER)) {
                Token id = p->previous;
                decl->name = (char*)malloc((size_t)id.length + 1u);
                if (decl->name) { memcpy(decl->name, id.start, (size_t)id.length); decl->name[id.length] = '\0'; }
            }
        } else {
            if (p->current.type == TOKEN_COLON) advance(p);
            if (match(p, TOKEN_IDENTIFIER)) {
                Token t = p->previous;
                decl->name = (char*)malloc((size_t)t.length + 1u);
                if (decl->name) { memcpy(decl->name, t.start, (size_t)t.length); decl->name[t.length] = '\0'; }
            }
        }
    }
    if (!decl->name || !parse_struct_body(p, decl)) {
        if (!decl->name) error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_FUNC_NAME));
        ast_destroy((ASTNode*)decl);
        return NULL;
    }
    return (ASTNode*)decl;
}

static ASTNode* parse_union_decl(Parser* p, char* name) {
    ASTStructDecl* decl = (ASTStructDecl*)ast_create(AST_UNION_DECL);
    if (!decl) { free(name); return NULL; }
    decl->base.line = p->previous.line;
    decl->base.col = p->previous.col;
    decl->is_packed = 0;
    decl->is_union = 1;
    if (name && name[0] != '\0') decl->name = name;
    else {
        free(name);
        if (p->previous.type == TOKEN_PRE_UNION) {
            Token t = p->previous;
            if (t.length > 6) {
                size_t n = (size_t)t.length - 6u;
                decl->name = (char*)malloc(n + 1u);
                if (decl->name) { memcpy(decl->name, t.start + 6, n); decl->name[n] = '\0'; }
            } else if (match(p, TOKEN_IDENTIFIER)) {
                Token id = p->previous;
                decl->name = (char*)malloc((size_t)id.length + 1u);
                if (decl->name) { memcpy(decl->name, id.start, (size_t)id.length); decl->name[id.length] = '\0'; }
            }
        } else {
            if (p->current.type == TOKEN_COLON) advance(p);
            if (match(p, TOKEN_IDENTIFIER)) {
                Token t = p->previous;
                decl->name = (char*)malloc((size_t)t.length + 1u);
                if (decl->name) { memcpy(decl->name, t.start, (size_t)t.length); decl->name[t.length] = '\0'; }
            }
        }
    }
    if (!decl->name || !parse_struct_body(p, decl)) {
        if (!decl->name) error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_FUNC_NAME));
        ast_destroy((ASTNode*)decl);
        return NULL;
    }
    return (ASTNode*)decl;
}

static ASTNode* parse_enum_decl(Parser* p, char* name) {
    ASTEnumDecl* decl = (ASTEnumDecl*)ast_create(AST_ENUM_DECL);
    ASTNode* head = NULL;
    ASTNode* tail = NULL;
    if (!decl) { free(name); return NULL; }
    decl->base.line = p->previous.line;
    if (name && name[0] != '\0') decl->name = name;
    else {
        free(name);
        if (p->previous.type == TOKEN_PRE_ENUM) {
            Token t = p->previous;
            if (t.length > 5) {
                size_t n = (size_t)t.length - 5u;
                decl->name = (char*)malloc(n + 1u);
                if (decl->name) { memcpy(decl->name, t.start + 5, n); decl->name[n] = '\0'; }
            } else if (match(p, TOKEN_IDENTIFIER)) {
                Token id = p->previous;
                decl->name = (char*)malloc((size_t)id.length + 1u);
                if (decl->name) { memcpy(decl->name, id.start, (size_t)id.length); decl->name[id.length] = '\0'; }
            }
        } else {
            if (p->current.type == TOKEN_COLON) advance(p);
            if (match(p, TOKEN_IDENTIFIER)) {
                Token t = p->previous;
                decl->name = (char*)malloc((size_t)t.length + 1u);
                if (decl->name) { memcpy(decl->name, t.start, (size_t)t.length); decl->name[t.length] = '\0'; }
            }
        }
    }
    if (!decl->name) { error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_FUNC_NAME)); ast_destroy((ASTNode*)decl); return NULL; }
    consume(p, TOKEN_LBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_LBRACE_FUNC);
    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        ASTEnumVariant* var;
        Token vt;
        if (!match(p, TOKEN_IDENTIFIER)) { advance(p); continue; }
        vt = p->previous;
        var = (ASTEnumVariant*)ast_create(AST_ENUM_VARIANT);
        if (!var) { ast_destroy(head); ast_destroy((ASTNode*)decl); return NULL; }
        var->name = (char*)malloc((size_t)vt.length + 1u);
        if (!var->name) { ast_destroy((ASTNode*)var); ast_destroy(head); ast_destroy((ASTNode*)decl); return NULL; }
        memcpy(var->name, vt.start, (size_t)vt.length);
        var->name[vt.length] = '\0';
        if (!head) head = (ASTNode*)var; else tail->next = (ASTNode*)var;
        tail = (ASTNode*)var;
        if (match(p, TOKEN_LPAREN)) {
            ASTNode* data_head = NULL;
            ASTNode* data_tail = NULL;
            var->data = NULL;
            if (p->current.type != TOKEN_RPAREN) do {
                ASTVarDecl* field;
                Token ft;
                if (!match(p, TOKEN_IDENTIFIER)) { error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, "Expect field name in enum variant."); ast_destroy(head); ast_destroy((ASTNode*)decl); return NULL; }
                ft = p->previous;
                field = (ASTVarDecl*)ast_create(AST_VAR_DECL);
                if (!field) { ast_destroy(data_head); ast_destroy(head); ast_destroy((ASTNode*)decl); return NULL; }
                field->base.line = ft.line; field->base.col = ft.col; field->is_const = 1;
                field->name = (char*)malloc((size_t)ft.length + 1u);
                if (!field->name) { ast_destroy((ASTNode*)field); ast_destroy(data_head); ast_destroy(head); ast_destroy((ASTNode*)decl); return NULL; }
                memcpy(field->name, ft.start, (size_t)ft.length); field->name[ft.length] = '\0';
                if (!match(p, TOKEN_NOT)) { error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN, msg_get(MSG_EXPECT_TYPE_BANG)); ast_destroy((ASTNode*)field); ast_destroy(data_head); ast_destroy(head); ast_destroy((ASTNode*)decl); return NULL; }
                if ((p->current.type >= TOKEN_TYPE_U8 && p->current.type <= TOKEN_TYPE_ANY) || p->current.type == TOKEN_IDENTIFIER) {
                    Token tt = p->current;
                    field->type_name = (char*)malloc((size_t)tt.length + 1u);
                    if (!field->type_name) { ast_destroy((ASTNode*)field); ast_destroy(data_head); ast_destroy(head); ast_destroy((ASTNode*)decl); return NULL; }
                    memcpy(field->type_name, tt.start, (size_t)tt.length); field->type_name[tt.length] = '\0';
                    field->is_hard_typed = 1; advance(p);
                } else { error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_RET_TYPE)); ast_destroy((ASTNode*)field); ast_destroy(data_head); ast_destroy(head); ast_destroy((ASTNode*)decl); return NULL; }
                if (!data_head) data_head = (ASTNode*)field; else data_tail->next = (ASTNode*)field;
                data_tail = (ASTNode*)field;
                var->data = data_head;
            } while (match(p, TOKEN_COMMA));
            consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_PARAMS);
        }
        if (match(p, TOKEN_COMMA)) {}
    }
    decl->variants = head;
    consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
    return (ASTNode*)decl;
}

static ASTNode* parse_newtype_decl(Parser* p) {
    // nt:Name = Type; veya nt:struct:Name
    Token t = p->previous;
    // nt: prefix'ini atla
    char* raw = malloc(t.length + 1);
    memcpy(raw, t.start, t.length);
    raw[t.length] = '\0';
    char* name_part = raw + 3; // "nt:"
    char* decl_name = NULL;

    // 1. Contiguous cases: nt:struct:Name, nt:enum:Name, nt:union:Name
    if (strncmp(name_part, "struct:", 7) == 0) {
        decl_name = malloc(strlen(name_part + 7) + 1);
        strcpy(decl_name, name_part + 7);
        free(raw);
        return parse_struct_decl(p, 0, decl_name);
    }
    if (strncmp(name_part, "enum:", 5) == 0) {
        decl_name = malloc(strlen(name_part + 5) + 1);
        strcpy(decl_name, name_part + 5);
        free(raw);
        return parse_enum_decl(p, decl_name);
    }
    if (strncmp(name_part, "union:", 6) == 0) {
        decl_name = malloc(strlen(name_part + 6) + 1);
        strcpy(decl_name, name_part + 6);
        free(raw);
        return parse_union_decl(p, decl_name);
    }

    // 2. Check for separated keywords or contiguous keyword tokens (nt:struct)
    int is_struct = (strcmp(name_part, "struct") == 0);
    int is_enum = (strcmp(name_part, "enum") == 0);
    int is_union = (strcmp(name_part, "union") == 0);
    int is_separated = (*name_part == '\0');

    if (is_struct || is_enum || is_union || is_separated) {
        free(raw);
        
        OmniTokenType current_type = p->current.type;
        int type_kind = 0; // 1: struct, 2: enum, 3: union

        if (is_struct) type_kind = 1;
        else if (is_enum) type_kind = 2;
        else if (is_union) type_kind = 3;
        else if (is_separated) {
            if (current_type == TOKEN_KW_STRUCT || current_type == TOKEN_PRE_STRUCT) type_kind = 1;
            else if (current_type == TOKEN_KW_ENUM || current_type == TOKEN_PRE_ENUM) type_kind = 2;
            else if (current_type == TOKEN_KW_UNION || current_type == TOKEN_PRE_UNION) type_kind = 3;
        }

        if (type_kind > 0) {
            // If separated, consume the keyword
            if (is_separated) {
                Token t_cur = p->current;
                int prefix_len = 0;
                if (type_kind == 1 && t_cur.type == TOKEN_PRE_STRUCT) prefix_len = 7;
                if (type_kind == 2 && t_cur.type == TOKEN_PRE_ENUM) prefix_len = 5;
                if (type_kind == 3 && t_cur.type == TOKEN_PRE_UNION) prefix_len = 6;

                advance(p); // Consume keyword/prefix

                if (prefix_len > 0 && t_cur.length > prefix_len) {
                    // Name is attached (e.g. struct:Name)
                    decl_name = malloc(t_cur.length - prefix_len + 1);
                    memcpy(decl_name, t_cur.start + prefix_len, t_cur.length - prefix_len);
                    decl_name[t_cur.length - prefix_len] = '\0';
                    
                    if (type_kind == 1) return parse_struct_decl(p, 0, decl_name);
                    if (type_kind == 2) return parse_enum_decl(p, decl_name);
                    if (type_kind == 3) return parse_union_decl(p, decl_name);
                }
            }

            // Check for C-style Definition: { ... } Name;
            if (p->current.type == TOKEN_LBRACE) {
                if (type_kind == 1 || type_kind == 3) {
                     ASTStructDecl* decl = (ASTStructDecl*)ast_create(type_kind == 1 ? AST_STRUCT_DECL : AST_UNION_DECL);
                     decl->base.line = p->previous.line;
                     decl->base.col = p->previous.col;
                     decl->is_packed = 0;
                     decl->is_union = (type_kind == 3);
                     
                     consume(p, TOKEN_LBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_LBRACE_FUNC);
                     
                     ASTNode* head = NULL;
                     ASTNode* tail = NULL;
                     while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
                        if (match(p, TOKEN_PRE_VAR)) {
                            ASTNode* field = parse_var_decl(p);
                            if (!head) head = field; else tail->next = field;
                            tail = field;
                            if (match(p, TOKEN_COMMA)) {} 
                        } else {
                            advance(p);
                        }
                     }
                     decl->fields = head;
                     consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
                     
                     if (match(p, TOKEN_IDENTIFIER)) {
                         Token t_id = p->previous;
                         decl->name = malloc(t_id.length + 1);
                         memcpy(decl->name, t_id.start, t_id.length);
                         decl->name[t_id.length] = '\0';
                     } else {
                         error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_FUNC_NAME));
                     }
                     consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
                     return (ASTNode*)decl;
                } else {
                    // Enum definition: nt: enum { ... } Name;
                    ASTEnumDecl* decl = (ASTEnumDecl*)ast_create(AST_ENUM_DECL);
                    decl->base.line = p->previous.line;
                    
                    consume(p, TOKEN_LBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_LBRACE_FUNC);
                    ASTNode* head = NULL;
                    ASTNode* tail = NULL;
                    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
                        if (match(p, TOKEN_IDENTIFIER)) {
                            ASTEnumVariant* var = (ASTEnumVariant*)ast_create(AST_ENUM_VARIANT);
                            Token t_var = p->previous;
                            var->name = malloc(t_var.length + 1);
                            memcpy(var->name, t_var.start, t_var.length);
                            var->name[t_var.length] = '\0';
                            var->data = NULL;
                            
                            if (match(p, TOKEN_LPAREN)) {
                                while(p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) advance(p);
                                consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_PARAMS);
                            }

                            if (!head) head = (ASTNode*)var; else tail->next = (ASTNode*)var;
                            tail = (ASTNode*)var;
                            if (match(p, TOKEN_COMMA)) {}
                        } else {
                            advance(p);
                        }
                    }
                    decl->variants = head;
                    consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
                    
                    if (match(p, TOKEN_IDENTIFIER)) {
                         Token t_id = p->previous;
                         decl->name = malloc(t_id.length + 1);
                         memcpy(decl->name, t_id.start, t_id.length);
                         decl->name[t_id.length] = '\0';
                    } else {
                         error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_FUNC_NAME));
                    }
                    consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
                    return (ASTNode*)decl;
                }
            } else {
                // Declaration: nt: struct : Name or nt: struct Name
                if (p->current.type == TOKEN_COLON) {
                    advance(p);
                }
                if (match(p, TOKEN_IDENTIFIER)) {
                    Token id = p->previous;
                    decl_name = malloc(id.length + 1);
                    memcpy(decl_name, id.start, id.length);
                    decl_name[id.length] = '\0';
                }
                
                if (type_kind == 1) return parse_struct_decl(p, 0, decl_name);
                if (type_kind == 2) return parse_enum_decl(p, decl_name);
                if (type_kind == 3) return parse_union_decl(p, decl_name);
            }
        }
    }

    ASTNewTypeDecl* decl = (ASTNewTypeDecl*)ast_create(AST_NEWTYPE_DECL);
    decl->name = malloc(strlen(name_part) + 1);
    strcpy(decl->name, name_part);
    free(raw);

    consume(p, TOKEN_ASSIGN, ERR_PARSER_UNEXPECTED_TOKEN, MSG_UNEXPECTED_CHAR);
    // Tip ismini al (Identifier veya primitive type)
    if (p->current.type >= TOKEN_TYPE_U8 && p->current.type <= TOKEN_TYPE_ANY) advance(p);
    else if (p->current.type == TOKEN_IDENTIFIER) advance(p);
    
    Token type_tok = p->previous;
    decl->target_type = malloc(type_tok.length + 1);
    memcpy(decl->target_type, type_tok.start, type_tok.length);
    decl->target_type[type_tok.length] = '\0';
    
    consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
    return (ASTNode*)decl;
}

static ASTNode* parse_group_decl(Parser* p) {
    ASTGroupDecl* group = (ASTGroupDecl*)ast_create(AST_GROUP_DECL);
    ASTNode* head = NULL;
    ASTNode* tail = NULL;
    if (!group) return NULL;
    group->base.line = p->previous.line; group->base.col = p->previous.col;
    if (match(p, TOKEN_IDENTIFIER)) {
        Token t = p->previous;
        group->name = (char*)malloc((size_t)t.length + 1u);
        if (!group->name) { ast_destroy((ASTNode*)group); return NULL; }
        memcpy(group->name, t.start, (size_t)t.length); group->name[t.length] = '\0';
    }
    consume(p, TOKEN_LBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_LBRACE_FUNC);
    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        ASTNode* member = NULL;
        if (p->current.type == TOKEN_IDENTIFIER && peek_token(p).type == TOKEN_FAT_ARROW) {
            ASTMethod* method = (ASTMethod*)ast_create(AST_METHOD);
            Token mt = p->current;
            if (!method) { ast_destroy(head); ast_destroy((ASTNode*)group); return NULL; }
            method->base.line = mt.line; method->base.col = mt.col;
            method->public_name = (char*)malloc((size_t)mt.length + 1u);
            if (!method->public_name) { ast_destroy((ASTNode*)method); ast_destroy(head); ast_destroy((ASTNode*)group); return NULL; }
            memcpy(method->public_name, mt.start, (size_t)mt.length); method->public_name[mt.length] = '\0';
            advance(p); advance(p);
            if (match(p, TOKEN_KW_FN) || match(p, TOKEN_PRE_FUNC)) {
                method->implementation = parse_func_decl(p);
                if (!method->implementation) { ast_destroy((ASTNode*)method); ast_destroy(head); ast_destroy((ASTNode*)group); return NULL; }
            } else { error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, "Expected function after '=>'"); ast_destroy((ASTNode*)method); ast_destroy(head); ast_destroy((ASTNode*)group); return NULL; }
            member = (ASTNode*)method;
            if (match(p, TOKEN_SEMICOLON)) {}
        }
        if (!member) {
            if (match(p, TOKEN_KW_STRUCT) || match(p, TOKEN_PRE_STRUCT)) member = parse_struct_decl(p, 0, NULL);
            else if (match(p, TOKEN_NOT)) {
                if (match(p, TOKEN_KW_STRUCT) || match(p, TOKEN_PRE_STRUCT)) member = parse_struct_decl(p, 1, NULL);
                else { error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN, MSG_EXPECT_TYPE_BANG); ast_destroy(head); ast_destroy((ASTNode*)group); return NULL; }
            } else if (match(p, TOKEN_KW_ENUM) || match(p, TOKEN_PRE_ENUM)) member = parse_enum_decl(p, NULL);
            else if (match(p, TOKEN_KW_UNION) || match(p, TOKEN_PRE_UNION)) member = parse_union_decl(p, NULL);
            else if (match(p, TOKEN_PRE_VAR) || match(p, TOKEN_PRE_CONST)) {
                member = parse_var_decl(p);
                if (member) consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
            } else if (match(p, TOKEN_KW_FN) || match(p, TOKEN_PRE_FUNC)) member = parse_func_decl(p);
            else { advance(p); continue; }
        }
        if (!member) { ast_destroy(head); ast_destroy((ASTNode*)group); return NULL; }
        if (!head) head = member; else tail->next = member;
        tail = member;
    }
    consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
    group->members = head;
    return (ASTNode*)group;
}

static ASTNode* parse_statement(Parser* p) {
    if (p->panic_mode) p->panic_mode = 0; // Her yeni ifadede hata modunu sıfırla (Basit senkronizasyon)

    if (match(p, TOKEN_SEMICOLON)) {
        ASTBlock* nop = (ASTBlock*)ast_create(AST_BLOCK);
        if (!nop) return NULL;
        nop->base.line = p->previous.line;
        nop->base.col = p->previous.col;
        nop->statements = NULL;
        return (ASTNode*)nop;
    }

    int is_pub = 0;
    int is_exp = 0;

    if (match(p, TOKEN_KW_EXP)) is_exp = 1;
    if (match(p, TOKEN_KW_PUP)) is_pub = 1;

    if (match(p, TOKEN_KW_USE)) return parse_use(p);
    if (match(p, TOKEN_KW_DEFER)) return parse_defer(p);
    if (match(p, TOKEN_KW_RET)) return parse_return_statement(p);

    if (match(p, TOKEN_PRE_VAR) || match(p, TOKEN_PRE_CONST)) {
        ASTVarDecl* decl = (ASTVarDecl*)parse_var_decl(p);
        if (!decl) return NULL;
        decl->is_public = is_pub;
        decl->is_exported = is_exp;
        consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
        return (ASTNode*)decl;
    }
    
    if (match(p, TOKEN_KW_FN) || match(p, TOKEN_PRE_FUNC)) {
        ASTFuncDecl* decl = (ASTFuncDecl*)parse_func_decl(p);
        if (!decl) return NULL;
        decl->is_public = is_pub;
        decl->is_exported = is_exp;
        return (ASTNode*)decl;
    }

    if (match(p, TOKEN_KW_STRUCT) || match(p, TOKEN_PRE_STRUCT)) {
        ASTStructDecl* decl = (ASTStructDecl*)parse_struct_decl(p, 0, NULL);
        if (!decl) return NULL;
        decl->is_public = is_pub;
        decl->is_exported = is_exp;
        return (ASTNode*)decl;
    }

    if (match(p, TOKEN_KW_GROUP)) {
        ASTGroupDecl* decl = (ASTGroupDecl*)parse_group_decl(p);
        if (!decl) return NULL;
        decl->is_public = is_pub;
        decl->is_exported = is_exp;
        return (ASTNode*)decl;
    }

    // Special Thread Block: (name)>{...}
    if (p->current.type == TOKEN_LPAREN) {
        Token lookahead = peek_token(p);
        if (lookahead.type == TOKEN_IDENTIFIER) {
            Token next2 = peek_token_ahead(p, 2);
            if (next2.type == TOKEN_RPAREN) {
                Token next3 = peek_token_ahead(p, 3);
                if (next3.type == TOKEN_THREAD_BLOCK) {
                    advance(p); // (
                    Token name_tok = p->current;
                    advance(p); // name
                    advance(p); // )
                    advance(p); // >{
                    char* name = (char*)malloc((size_t)name_tok.length + 1u);
                    if (!name) return NULL;
                    memcpy(name, name_tok.start, (size_t)name_tok.length);
                    name[name_tok.length] = '\0';
                    return parse_thread_block(p, name);
                }
            }
        }
    }
    if (match(p, TOKEN_NOT)) {
        if (match(p, TOKEN_KW_STRUCT) || match(p, TOKEN_PRE_STRUCT)) {
            return parse_struct_decl(p, 1, NULL); // !struct (Packed)
        }
        
        // Check for !(listen)
        if (p->current.type == TOKEN_LPAREN) {
            Token lookahead = peek_token(p);
            if (lookahead.type == TOKEN_IDENTIFIER && strncmp(lookahead.start, "listen", 6) == 0) {
                advance(p); // (
                advance(p); // listen
                consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);
                consume(p, TOKEN_COLON, ERR_PARSER_UNEXPECTED_TOKEN, MSG_EXPECT_COLON_VAR);
                
                ASTNexusCapture* listen = (ASTNexusCapture*)ast_create(AST_LISTEN);
                if (!listen) return NULL;
                listen->base.line = p->previous.line;
                listen->base.col = p->previous.col;
                listen->handler_name = strdup("global_listener");
                if (!listen->handler_name) {
                    ast_destroy((ASTNode*)listen);
                    return NULL;
                }
                if (match(p, TOKEN_LBRACE)) listen->expr = parse_block(p);
                else listen->expr = parse_statement(p);
                if (!listen->expr) {
                    ast_destroy((ASTNode*)listen);
                    return NULL;
                }
                return (ASTNode*)listen;
            }
        }
    }
    if (match(p, TOKEN_KW_ENUM) || match(p, TOKEN_PRE_ENUM)) return parse_enum_decl(p, NULL);
    if (match(p, TOKEN_KW_UNION) || match(p, TOKEN_PRE_UNION)) return parse_union_decl(p, NULL);
    if (match(p, TOKEN_PRE_NEWTYPE)) return parse_newtype_decl(p);
    if (match(p, TOKEN_KW_GROUP)) return parse_group_decl(p);

    if (match(p, TOKEN_LBRACE)) {
        return parse_block(p);
    }
    if (match(p, TOKEN_JUMP)) {
        // ?>(label);
        consume(p, TOKEN_LPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_LPAREN_FUNC);
        if (match(p, TOKEN_IDENTIFIER)) {
            ASTNexusJump* jmp = (ASTNexusJump*)ast_create(AST_NEXUS_JUMP);
            Token t = p->previous;
            if (!jmp) return NULL;
            jmp->base.line = t.line;
            jmp->base.col = t.col;
            jmp->label_name = (char*)malloc((size_t)t.length + 1u);
            if (!jmp->label_name) {
                ast_destroy((ASTNode*)jmp);
                return NULL;
            }
            memcpy(jmp->label_name, t.start, (size_t)t.length);
            jmp->label_name[t.length] = '\0';
            consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);
            consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_EXPR);
            return (ASTNode*)jmp;
        }
        error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_FUNC_NAME));
    }

    if (match(p, TOKEN_HALT)) {
        ASTHalt* halt = (ASTHalt*)ast_create(AST_NEXUS_HALT);
        if (!halt) return NULL;
        halt->base.line = p->previous.line;
        halt->base.col = p->previous.col;
        if (!match(p, TOKEN_SEMICOLON)) {
            halt->expr = parse_expression(p);
            if (!halt->expr) {
                ast_destroy((ASTNode*)halt);
                return NULL;
            }
            consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_EXPR);
        }
        return (ASTNode*)halt;
    }

    if (match(p, TOKEN_INTENT)) {
        ASTIntent* in = (ASTIntent*)ast_create(AST_NEXUS_INTENT);
        if (!in) return NULL;
        in->base.line = p->previous.line;
        in->base.col = p->previous.col;
        if (match(p, TOKEN_IDENTIFIER)) {
            Token t = p->previous;
            in->intent_name = (char*)malloc((size_t)t.length + 1u);
            if (!in->intent_name) {
                ast_destroy((ASTNode*)in);
                return NULL;
            }
            memcpy(in->intent_name, t.start, (size_t)t.length);
            in->intent_name[t.length] = '\0';
        }
        if (match(p, TOKEN_LPAREN)) {
            in->target = parse_expression(p);
            if (!in->target) {
                ast_destroy((ASTNode*)in);
                return NULL;
            }
            consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);
        }
        if (match(p, TOKEN_SEMICOLON)) {} // Opsiyonel ;
        return (ASTNode*)in;
    }

    if (match(p, TOKEN_FORCE)) {
        ASTPanic* panic = (ASTPanic*)ast_create(AST_PANIC);
        if (!panic) return NULL;
        panic->base.line = p->previous.line;
        panic->base.col = p->previous.col;
        if (match(p, TOKEN_STRING)) {
            Token t = p->previous;
            panic->message = (char*)malloc((size_t)t.length + 1u);
            if (!panic->message) {
                ast_destroy((ASTNode*)panic);
                return NULL;
            }
            memcpy(panic->message, t.start, (size_t)t.length);
            panic->message[t.length] = '\0';
        } else {
            panic->message = strdup("Critical Panic");
            if (!panic->message) {
                ast_destroy((ASTNode*)panic);
                return NULL;
            }
        }
        consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_EXPR);
        return (ASTNode*)panic;
    }
    
    // Expression Statement
    ASTNode* expr = parse_expression(p);
    if (!expr) return NULL;
    
    // Check for Label Syntax: expr :> statement
    if (match(p, TOKEN_NEXUS_LABEL)) {
        char* label_name = NULL;
        
        /* Unwrap at most one single-element tuple; multi-element tuples stay invalid labels. */
        ASTNode* target = expr;
        if (target->type == AST_TUPLE) {
            ASTTuple* tuple = (ASTTuple*)target;
            if (tuple->elements && !tuple->elements->next) target = tuple->elements;
        }

        if (target->type == AST_IDENTIFIER) {
            ASTIdentifier* id = (ASTIdentifier*)target;
            size_t length = strlen(id->name);
            label_name = (char*)malloc(length + 1u);
            if (label_name) memcpy(label_name, id->name, length + 1u);
        } else if (target->type == AST_LITERAL) {
            ASTLiteral* lit = (ASTLiteral*)target;
            size_t length = lit->string_value ? strlen(lit->string_value) : 0u;
            if (lit->string_value) {
                label_name = (char*)malloc(length + 1u);
                if (label_name) memcpy(label_name, lit->string_value, length + 1u);
            }
        } else {
            error_at(p, &p->previous, ERR_PARSER_INVALID_VAR, msg_get(MSG_INVALID_CAPTURE));
            return expr;
        }
        if (!label_name) {
            ast_destroy(expr);
            return NULL;
        }

        ASTNexusLabel* lbl = (ASTNexusLabel*)ast_create(AST_NEXUS_LABEL);
        if (!lbl) {
            free(label_name);
            ast_destroy(expr);
            return NULL;
        }
        lbl->base.line = expr->line;
        lbl->base.col = expr->col;
        lbl->label_name = label_name;
        ast_destroy(expr); /* The expression only supplies the label name. */

        if (match(p, TOKEN_LBRACE)) lbl->statement = parse_block(p);
        else if (p->current.type != TOKEN_EOF && p->current.type != TOKEN_RBRACE)
            lbl->statement = parse_statement(p);
        if (p->current.type != TOKEN_EOF && p->current.type != TOKEN_RBRACE &&
            !lbl->statement) {
            ast_destroy((ASTNode*)lbl);
            return NULL;
        }
        return (ASTNode*)lbl;
    }

    // Loop, If (Nexus Flow), Label gibi yapılar noktalı virgül gerektirmez
    if (expr->type != AST_LOOP && expr->type != AST_NEXUS_FLOW && expr->type != AST_NEXUS_LABEL) {
        consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_EXPR);
    }
    return expr;
}

int parser_run(ParserContext* ctx) {
    if (!ctx || !ctx->source_buffer || ctx->ast_root) return 1;
    printf("%s\n", msg_get(MSG_PARSER_START));

    Parser parser;
    memset(&parser, 0, sizeof(parser));
    parser.ctx = ctx;
    parser.current.type = TOKEN_EOF;
    parser.previous.type = TOKEN_EOF;
    parser.had_error = 0;
    parser.panic_mode = 0;

    advance(&parser); // İlk tokeni yükle

    ASTProgram* program = (ASTProgram*)ast_create(AST_PROGRAM);
    if (!program) return 1;
    program->base.line = 1;
    program->base.col = 1;
    ctx->ast_root = (void*)program; /* Keep partial trees owned by the context on parse failure. */
    ASTNode* current_stmt = NULL;

    while (!match(&parser, TOKEN_EOF)) {
        ASTNode* stmt = parse_statement(&parser);
        if (stmt) {
            // AST'ye ekle (Bağlı liste mantığı)
            if (program->declarations == NULL) {
                program->declarations = stmt;
                current_stmt = stmt;
            } else {
                current_stmt->next = stmt;
                current_stmt = stmt;
            }
        } else {
            // Hata durumunda sonsuz döngüyü önlemek için ilerle
            advance(&parser);
        }
    }

    if (parser.had_error) {
        return 1;
    }

    printf("%s\n", msg_get(MSG_PARSER_SUCCESS));
    
    // AST Görselleştirme
    if (program) {
        ctx->ast_root = (void*)program; // Semantic analiz için kaydet
        ast_print((ASTNode*)program);
    }
    
    return 0;
}

// --- Levenshtein Similarity Helper (ULTRA) ---
static void parser_add_issue(ParserContext* ctx, ErrorCode code, int line, int col, const char* message, const char* suggestion, const char* hint, int similarity, int is_warning, const OccSourceSpan* source_span) {
    if (ctx->diagnostics) {
        OccDiagnosticInput input = {0};
        input.code = (uint32_t)code;
        input.message_id = (uint32_t)code;
        input.stage_id = code == ERR_LEXER_ERROR ? OCC_STAGE_LEXER :
            (ctx->diagnostic_stage_id ? ctx->diagnostic_stage_id : OCC_STAGE_PARSER);
        input.severity = is_warning ? OCC_DIAG_WARNING : OCC_DIAG_ERROR;
        if (source_span) input.span = *source_span;
        input.span.start_line = source_span ? source_span->start_line : (line > 0 ? (uint32_t)line : 0u);
        input.span.start_column = source_span ? source_span->start_column : (col > 0 ? (uint32_t)col : 0u);
        if (!source_span) {
            input.span.end_line = input.span.start_line;
            input.span.end_column = input.span.start_column;
            input.span.has_location = (uint8_t)(line > 0 && col > 0);
        }
        input.message = message ? message : "";
        input.message_length = strlen(input.message);
        input.hint = hint ? hint : suggestion;
        input.hint_length = input.hint ? strlen(input.hint) : 0u;
        (void)occ_diagnostics_add(ctx->diagnostics, &input);
    }
    Error* err = (Error*)calloc(1u, sizeof(*err));
    if (!err) return;
    err->code = code;
    err->filename = strdup(ctx->filename ? ctx->filename : "<unknown>");
    err->message = strdup(message ? message : "");
    err->suggestion = suggestion ? strdup(suggestion) : NULL;
    err->hint = hint ? strdup(hint) : NULL;
    if (!err->filename || !err->message ||
        (suggestion && !err->suggestion) || (hint && !err->hint)) {
        free(err->filename);
        free(err->message);
        free(err->suggestion);
        free(err->hint);
        free(err);
        return;
    }
    err->line = line;
    err->col = col;
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

// Eski fonksiyonu yönlendir
void parser_attach_diagnostics(ParserContext* ctx, OccDiagnosticCollector* collector, uint32_t stage_id) {
    if (!ctx) return;
    ctx->diagnostics = collector;
    ctx->diagnostic_stage_id = stage_id ? stage_id : OCC_STAGE_PARSER;
}

int parser_attach_token_stream(
    ParserContext* ctx, const struct Token* tokens, size_t token_count
) {
    size_t i;
    if (!ctx || !tokens || token_count == 0 ||
        tokens[token_count - 1u].type != TOKEN_EOF) {
        return 0;
    }
    for (i = 0; i + 1u < token_count; ++i) {
        if (tokens[i].type == TOKEN_EOF) return 0;
    }
    ctx->token_stream = tokens;
    ctx->token_stream_count = token_count;
    ctx->token_stream_index = 0;
    return 1;
}

void parser_print_errors(ParserContext* ctx) {
    if (ctx->error_count == 0) return;

    printf("\n%s (%d) ---\n", msg_get(MSG_ERRORS_HEADER), ctx->error_count);
    Error* current = ctx->error_head;
    while (current) {
        // [E042] Hata: ... veya [W012] Uyarı: ...
        const char* type_label = current->is_warning ? "Uyarı" : "Hata";
        char type_char = current->is_warning ? 'W' : 'E';
        
        printf("[%c%03d] %s: %s\n", type_char, current->code, type_label, current->message);
        printf("       Dosya: %s Satır: %d:%d\n", current->filename, current->line, current->col);
        
        if (current->suggestion) {
            if (current->similarity > 0) {
                printf("       Öneri: %s (Benzerlik: %%%d)\n", current->suggestion, current->similarity);
            } else {
                printf("       Öneri: %s\n", current->suggestion);
            }
        }
        
        if (current->hint) {
            printf("       İpucu: %s\n", current->hint);
        }

        printf("\n");
        current = current->next;
    }
    printf("%s\n", msg_get(MSG_SEPARATOR));
}

void parser_destroy(ParserContext* ctx) {
    if (ctx) {
        if (ctx->ast_root) ast_destroy((ASTNode*)ctx->ast_root);
        
        Error* current = ctx->error_head;
        while (current) {
            Error* next = current->next;
            if (current->filename) free(current->filename);
            if (current->message) free(current->message);
            if (current->suggestion) free(current->suggestion);
            if (current->hint) free(current->hint);
            free(current);
            current = next;
        }
        if (ctx->source_buffer) free(ctx->source_buffer);
        free(ctx);
    }
}

int parser_save_ir(ParserContext* ctx, const char* path) {
    if (!ctx || !ctx->ast_root || !path || !*path) return 0;

    FILE* file = fopen(path, "wb");
    if (!file) {
        fprintf(stderr, "Error: Could not open IR file for writing: %s\n", path);
        return 0;
    }

    int ok = ast_serialize((ASTNode*)ctx->ast_root, file);
    if (fclose(file) != 0) ok = 0;

    if (!ok) {
        (void)remove(path);
        fprintf(stderr, "Error: IR serialization failed: %s\n", path);
        return 0;
    }

    return 1;
}