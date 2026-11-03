#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#define PARSER_MAX_NESTING_DEPTH 256u

static int parser_identifier_is(const Token* token, const char* text);

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

// Dosyayı belleğe okuyan yardımcı fonksiyon.
static char* read_file_content(
    const char* filename, size_t* out_len, ParserStatus* out_status
) {
    FILE* f;
    long file_length;
    size_t length;
    char* buffer;

    if (out_len) *out_len = 0u;
    if (out_status) *out_status = PARSER_STATUS_INVALID_ARGUMENT;
    if (!filename || !*filename || !out_status) return NULL;

    f = fopen(filename, "rb");
    if (!f) {
        *out_status = PARSER_STATUS_IO_ERROR;
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        (void)fclose(f);
        *out_status = PARSER_STATUS_IO_ERROR;
        return NULL;
    }

    file_length = ftell(f);
    if (file_length < 0 || fseek(f, 0, SEEK_SET) != 0) {
        (void)fclose(f);
        *out_status = PARSER_STATUS_IO_ERROR;
        return NULL;
    }

    if ((uintmax_t)file_length >= (uintmax_t)SIZE_MAX) {
        (void)fclose(f);
        *out_status = PARSER_STATUS_INVALID_ARGUMENT;
        return NULL;
    }

    length = (size_t)file_length;
    buffer = (char*)malloc(length + 1u);
    if (!buffer) {
        (void)fclose(f);
        *out_status = PARSER_STATUS_OUT_OF_MEMORY;
        return NULL;
    }

    if ((length > 0u && fread(buffer, 1u, length, f) != length) || ferror(f)) {
        free(buffer);
        (void)fclose(f);
        *out_status = PARSER_STATUS_IO_ERROR;
        return NULL;
    }

    if (fclose(f) != 0) {
        free(buffer);
        *out_status = PARSER_STATUS_IO_ERROR;
        return NULL;
    }

    buffer[length] = '\0';
    if (out_len) *out_len = length;
    *out_status = PARSER_STATUS_OK;
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

ParserStatus parser_init_source_ex(
    const char* filename, const uint8_t* source, size_t source_len, ParserContext** out_ctx
) {
    ParserContext* ctx;
    char* copy;

    if (!out_ctx) return PARSER_STATUS_INVALID_ARGUMENT;
    *out_ctx = NULL;
    if ((!source && source_len != 0u) || source_len == SIZE_MAX ||
        (source && memchr(source, '\0', source_len) != NULL)) {
        return PARSER_STATUS_INVALID_ARGUMENT;
    }

    ctx = (ParserContext*)malloc(sizeof(*ctx));
    if (!ctx) return PARSER_STATUS_OUT_OF_MEMORY;
    copy = (char*)malloc(source_len + 1u);
    if (!copy) {
        free(ctx);
        return PARSER_STATUS_OUT_OF_MEMORY;
    }
    if (source_len) memcpy(copy, source, source_len);
    copy[source_len] = '\0';
    parser_context_init(ctx, filename, copy, source_len);
    *out_ctx = ctx;
    return PARSER_STATUS_OK;
}

ParserContext* parser_init_source(const char* filename, const uint8_t* source, size_t source_len) {
    ParserContext* ctx = NULL;
    if (parser_init_source_ex(filename, source, source_len, &ctx) != PARSER_STATUS_OK) {
        return NULL;
    }
    return ctx;
}

ParserStatus parser_init_ex(const char* filename, ParserContext** out_ctx) {
    ParserContext* ctx;
    char* source;
    size_t source_len = 0u;
    ParserStatus status = PARSER_STATUS_INVALID_ARGUMENT;

    if (!out_ctx) return PARSER_STATUS_INVALID_ARGUMENT;
    *out_ctx = NULL;
    if (!filename || !*filename) return PARSER_STATUS_INVALID_ARGUMENT;

    source = read_file_content(filename, &source_len, &status);
    if (!source) return status;

    ctx = (ParserContext*)malloc(sizeof(*ctx));
    if (!ctx) {
        free(source);
        return PARSER_STATUS_OUT_OF_MEMORY;
    }
    parser_context_init(ctx, filename, source, source_len);
    *out_ctx = ctx;
    return PARSER_STATUS_OK;
}

ParserContext* parser_init(const char* filename) {
    ParserContext* ctx = NULL;
    if (parser_init_ex(filename, &ctx) != PARSER_STATUS_OK) return NULL;
    return ctx;
}

// --- Parser State & Helper Functions ---

typedef struct {
    ParserContext* ctx;
    Token current;
    Token previous;
    int had_error;
    int panic_mode;
    unsigned int nesting_depth;
} Parser;

static ASTNode* parse_logical_or(Parser* p);

static void parser_mark_oom(Parser* p) {
    if (p && p->ctx) p->ctx->status = PARSER_STATUS_OUT_OF_MEMORY;
}

static int parser_status_is_terminal(ParserStatus status) {
    return status == PARSER_STATUS_OUT_OF_MEMORY ||
        status == PARSER_STATUS_DIAGNOSTIC_LIMIT;
}

static int parser_should_fail_allocation(ParserContext* ctx) {
    if (!ctx) return 0;
    if (ctx->test_allocation_count < SIZE_MAX) ++ctx->test_allocation_count;
    return ctx->test_fail_allocation_at != 0u &&
        ctx->test_allocation_count == ctx->test_fail_allocation_at;
}

static void* parser_context_malloc(ParserContext* ctx, size_t size, int clear) {
    void* memory;
    if (parser_should_fail_allocation(ctx)) return NULL;
    memory = clear ? calloc(1u, size ? size : 1u) : malloc(size ? size : 1u);
    return memory;
}

static char* parser_context_strdup(ParserContext* ctx, const char* value) {
    size_t length;
    char* copy;
    if (!value) return NULL;
    length = strlen(value);
    if (length == SIZE_MAX) return NULL;
    copy = (char*)parser_context_malloc(ctx, length + 1u, 0);
    if (copy) memcpy(copy, value, length + 1u);
    return copy;
}

static void* parser_malloc(Parser* p, size_t size) {
    void* memory = parser_context_malloc(p ? p->ctx : NULL, size, 0);
    if (!memory) parser_mark_oom(p);
    return memory;
}

static char* parser_strdup(Parser* p, const char* value) {
    size_t length;
    char* copy;
    if (!value) {
        parser_mark_oom(p);
        return NULL;
    }
    length = strlen(value);
    if (length == SIZE_MAX) {
        parser_mark_oom(p);
        return NULL;
    }
    copy = (char*)parser_malloc(p, length + 1u);
    if (copy) memcpy(copy, value, length + 1u);
    return copy;
}

static ASTNode* parser_ast_create(Parser* p, ASTNodeType type) {
    ASTNode* node;
    if (parser_should_fail_allocation(p ? p->ctx : NULL)) {
        parser_mark_oom(p);
        return NULL;
    }
    node = ast_create(type);
    if (!node) parser_mark_oom(p);
    return node;
}

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
    /* Lexical errors must remain visible even while syntax recovery is active. */
    if (p->panic_mode && token->type != TOKEN_ERROR) return;
    p->panic_mode = 1;
    if (token->type == TOKEN_ERROR && token->diagnostic_id != 0 &&
        p->ctx->diagnostics &&
        (size_t)token->diagnostic_id <= occ_diagnostics_count(p->ctx->diagnostics)) {
        OccDiagnosticView upstream;
        if (occ_diagnostics_get(p->ctx->diagnostics,
                (size_t)token->diagnostic_id - 1u, &upstream) &&
            upstream.record->stage_id == OCC_STAGE_LEXER &&
            upstream.record->span.has_location &&
            upstream.record->span.start_offset == (uint64_t)token->source_offset &&
            upstream.record->span.end_offset ==
                (uint64_t)(token->source_offset + token->source_length)) {
            /* The upstream lexer diagnostic owns this exact token finding. */
            p->had_error = 1;
            return;
        }
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
        if (parser_status_is_terminal(p->ctx->status)) break;
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

/*
 * Panic-mode recovery: discard the damaged statement, but preserve the next
 * statement boundary and the enclosing block delimiter for the caller.
 */
static int parser_is_statement_start(OmniTokenType type) {
    switch (type) {
        case TOKEN_PRE_VAR:
        case TOKEN_PRE_CONST:
        case TOKEN_PRE_FUNC:
        case TOKEN_KW_FN:
        case TOKEN_KW_RET:
        case TOKEN_KW_IF:
        case TOKEN_KW_USE:
        case TOKEN_KW_DEFER:
        case TOKEN_KW_STRUCT:
        case TOKEN_PRE_STRUCT:
        case TOKEN_KW_ENUM:
        case TOKEN_PRE_ENUM:
        case TOKEN_KW_UNION:
        case TOKEN_PRE_UNION:
        case TOKEN_KW_GROUP:
        case TOKEN_PRE_NEWTYPE:
        case TOKEN_LBRACE:
        case TOKEN_KW_LOOP:
        case TOKEN_KW_BREAK:
        case TOKEN_KW_CONTINUE:
        case TOKEN_HALT:
        case TOKEN_INTENT:
        case TOKEN_FORCE:
        case TOKEN_JUMP:
            return 1;
        default:
            return 0;
    }
}

/* A parenthesized condition followed by -> and a statement-only action
 * is conditional control flow, not a value/data pipeline. Expression RHS forms
 * (identifiers, calls, literals and blocks) remain ordinary Nexus pipe syntax.
 */
static int parser_is_conditional_action_start(OmniTokenType type) {
    if (type == TOKEN_LBRACE) return 0; /* -> { ... } remains a Nexus pipe body. */
    return parser_is_statement_start(type);
}

static int parser_looks_like_conditional_action(Parser* p) {
    ParserContext lookahead;
    Token token;
    unsigned int nested_parens = 0u;

    if (!p || !p->ctx || p->current.type != TOKEN_LPAREN) return 0;

    /* Scan a private cursor copy: lookahead must not move the live lexer/token stream. */
    lookahead = *p->ctx;
    for (;;) {
        token = parser_next_token(&lookahead);
        if (token.type == TOKEN_EOF || token.type == TOKEN_ERROR) return 0;

        if (token.type == TOKEN_LPAREN) {
            if (nested_parens >= PARSER_MAX_NESTING_DEPTH) return 0;
            ++nested_parens;
        } else if (token.type == TOKEN_RPAREN) {
            if (nested_parens == 0u) {
                Token arrow = parser_next_token(&lookahead);
                Token action = parser_next_token(&lookahead);
                return arrow.type == TOKEN_PIPE &&
                    parser_is_conditional_action_start(action.type);
            }
            --nested_parens;
        }
    }
}

static void parser_synchronize_statement(Parser* p) {
    if (!p || !p->ctx) return;

    /* Consume at least one token unless already positioned at a safe boundary. */
    if (p->current.type == TOKEN_SEMICOLON) {
        advance(p);
        p->panic_mode = 0;
        return;
    }
    if (p->current.type == TOKEN_RBRACE || p->current.type == TOKEN_EOF) {
        p->panic_mode = 0;
        return;
    }

    /*
     * A failed declaration may leave the cursor exactly on the next valid
     * declaration (for example, a missing parameter before the next f:name).
     * Preserve that boundary instead of consuming the next declaration.
     */
    if (parser_is_statement_start(p->current.type)) {
        p->panic_mode = 0;
        return;
    }

    advance(p);
    while (p->current.type != TOKEN_EOF &&
           p->current.type != TOKEN_RBRACE &&
           p->current.type != TOKEN_SEMICOLON) {
        if (parser_is_statement_start(p->current.type)) break;
        advance(p);
    }

    if (p->current.type == TOKEN_SEMICOLON) advance(p);
    p->panic_mode = 0;
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
static ASTNode* parse_if_statement(Parser* p);
static ASTNode* parse_block(Parser* p);
static ASTNode* parse_select_scan(Parser* p, int is_scan);
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
static ASTNode* parse_postfix(Parser* p);\nstatic ASTNode* parser_make_binary(Parser* p, OmniTokenType op, ASTNode* left, ASTNode* right);
static ASTNode* parse_range(Parser* p);
static ASTNode* parse_logical_and(Parser* p);
static ASTNode* parse_logical_or(Parser* p);
static ASTNode* parse_equality(Parser* p);
static int parse_struct_body(Parser* p, ASTStructDecl* decl);
static ASTNode* parse_comparison(Parser* p);
static ASTNode* parse_additive(Parser* p);
static ASTNode* parse_unary(Parser* p);
static ASTNode* parse_intrinsic_call(Parser* p, ASTNodeType type);
static ASTNode* parse_defer(Parser* p);
static ASTNode* parse_use(Parser* p);
static ASTNode* parse_thread_block(Parser* p, char* name);
static ASTNode* parse_lambda(Parser* p);

// Primary: Literals, Identifiers, Grouping
static ASTNode* parse_primary(Parser* p) {
    if (match(p, TOKEN_NUMBER) || match(p, TOKEN_STRING) || match(p, TOKEN_CHAR)) {
        Token token = p->previous;
        ASTLiteral* node = (ASTLiteral*)parser_ast_create(p, AST_LITERAL);
        if (!node) return NULL;
        node->base.line = token.line;
        node->base.col = token.col;
        node->type = token.type;
        node->string_value = (char*)parser_malloc(p, (size_t)token.length + 1u);
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
        ASTLiteral* node = (ASTLiteral*)parser_ast_create(p, AST_LITERAL);
        if (!node) return NULL;
        node->base.line = p->previous.line;
        node->base.col = p->previous.col;
        node->type = p->previous.type;
        node->string_value = parser_strdup(p, value);
        if (!node->string_value) {
            ast_destroy((ASTNode*)node);
            return NULL;
        }
        return (ASTNode*)node;
    }

    if (match(p, TOKEN_IDENTIFIER)) {
        ASTIdentifier* id_node = (ASTIdentifier*)parser_ast_create(p, AST_IDENTIFIER);
        if (!id_node) return NULL;
        id_node->base.line = p->previous.line;
        id_node->base.col = p->previous.col;
        id_node->name = (char*)parser_malloc(p, (size_t)p->previous.length + 1u);
        if (!id_node->name) {
            ast_destroy((ASTNode*)id_node);
            return NULL;
        }
        memcpy(id_node->name, p->previous.start, (size_t)p->previous.length);
        id_node->name[p->previous.length] = '\0';
        return (ASTNode*)id_node;
    }

    if (check(p, TOKEN_KW_FN) && peek_token(p).type == TOKEN_LPAREN) {
        advance(p);
        return parse_lambda(p);
    }

    if (match(p, TOKEN_KW_FN)) {
        return parse_func_decl(p);
    }

    if (match(p, TOKEN_KW_LOOP)) {
        ASTLoop* loop = (ASTLoop*)parser_ast_create(p, AST_LOOP);
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

        if (match(p, TOKEN_ITERATE) ||
            (p->current.type == TOKEN_IDENTIFIER && parser_identifier_is(&p->current, "in") && (advance(p), 1))) {
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

    if (match(p, TOKEN_LBRACKET)) {
        Token open=p->previous;
        ASTArrayLiteral* array=(ASTArrayLiteral*)parser_ast_create(p,AST_ARRAY_LITERAL);
        ASTNode* head=NULL; ASTNode* tail=NULL;
        if(!array)return NULL;
        array->base.line=open.line; array->base.col=open.col;
        if(!check(p,TOKEN_RBRACKET)) {
            do {
                ASTNode* element=parse_expression(p);
                if(!element){ast_destroy((ASTNode*)array);ast_destroy(head);return NULL;}
                if(!head)head=element;else tail->next=element;
                tail=element;
            } while(match(p,TOKEN_COMMA) && !check(p,TOKEN_RBRACKET));
        }
        if(!check(p,TOKEN_RBRACKET)) {
            error_at(p,&p->current,ERR_PARSER_EXPECTED_EXPRESSION,"Expected ']' after array elements.");
            ast_destroy((ASTNode*)array);ast_destroy(head);return NULL;
        }
        advance(p);
        array->elements=head;
        return (ASTNode*)array;
    }

    if (match(p, TOKEN_LPAREN)) {
        // (loop) Kontrolü
        if (match(p, TOKEN_KW_LOOP)) {
            consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);
            
            ASTLoop* loop = (ASTLoop*)parser_ast_create(p, AST_LOOP);
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
            ASTTuple* tuple = (ASTTuple*)parser_ast_create(p, AST_TUPLE);
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
             ASTTuple* tuple = (ASTTuple*)parser_ast_create(p, AST_TUPLE);
             if (!tuple) return NULL;
             tuple->base.line = p->previous.line;
             tuple->base.col = p->previous.col;
             tuple->elements = NULL;
             return (ASTNode*)tuple;
        }

        ASTNode* expr = parse_expression(p);
        if (!expr) return NULL;
        
        ASTTuple* tuple = (ASTTuple*)parser_ast_create(p, AST_TUPLE);
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
        ASTAiLogic* ai = (ASTAiLogic*)parser_ast_create(p, AST_AI_LOGIC);
        if (!ai) return NULL;
        ai->base.line = p->previous.line;
        ai->base.col = p->previous.col;
        if (p->current.type == TOKEN_STRING || p->current.type == TOKEN_IDENTIFIER) {
            Token t = p->current;
            ai->query = parser_malloc(p, (size_t)t.length + 1u);
            if (!ai->query) {
                ast_destroy((ASTNode*)ai);
                return NULL;
            }
            memcpy(ai->query, t.start, (size_t)t.length);
            ai->query[t.length] = '\0';
            advance(p);
        } else {
            ai->query = parser_strdup(p, "auto");
            if (!ai->query) {
                ast_destroy((ASTNode*)ai);
                return NULL;
            }
        }
        return (ASTNode*)ai;
    }

    error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(ERR_PARSER_EXPECTED_EXPRESSION));
    return NULL;
}

static ASTNode* parse_intrinsic_call(Parser* p, ASTNodeType type) {
    if (type == AST_CAST) {
        ASTCast* cast = (ASTCast*)parser_ast_create(p, AST_CAST);
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
        if (!cast->value) {
            ast_destroy((ASTNode*)cast);
            return NULL;
        }
        consume(p, TOKEN_COMMA, ERR_PARSER_UNEXPECTED_TOKEN, MSG_EXPECT_COMMA_ROLLING);
        if (p->current.type == TOKEN_IDENTIFIER ||
            (p->current.type >= TOKEN_TYPE_U8 && p->current.type <= TOKEN_TYPE_ANY)) {
            type_token = p->current;
            advance(p);
            target = (ASTIdentifier*)parser_ast_create(p, AST_IDENTIFIER);
            if (!target) {
                ast_destroy((ASTNode*)cast);
                return NULL;
            }
            target->base.line = type_token.line;
            target->base.col = type_token.col;
            target->name = (char*)parser_malloc(p, (size_t)type_token.length + 1u);
            if (!target->name) {
                ast_destroy((ASTNode*)target);
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
        ASTPanic* panic = (ASTPanic*)parser_ast_create(p, AST_PANIC);
        if (!panic) return NULL;
        panic->base.line = p->previous.line;
        panic->base.col = p->previous.col;
        if (match(p, TOKEN_LPAREN)) {
            if (match(p, TOKEN_STRING)) {
                Token message = p->previous;
                panic->message = (char*)parser_malloc(p, (size_t)message.length + 1u);
                if (!panic->message) {
                    ast_destroy((ASTNode*)panic);
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

    ASTCall* call = (ASTCall*)parser_ast_create(p, type);
    if (!call) return NULL;
    call->base.line = p->previous.line;
    call->base.col = p->previous.col;
    call->args = NULL;
    call->callee = NULL;

    /* reg.r(...) form: build the member identifier with checked ownership. */
    if (type == AST_REG && match(p, TOKEN_DOT)) {
        if (match(p, TOKEN_IDENTIFIER)) {
            Token member = p->previous;
            ASTIdentifier* id = (ASTIdentifier*)parser_ast_create(p, AST_IDENTIFIER);
            if (!id) {
                ast_destroy((ASTNode*)call);
                return NULL;
            }
            id->name = (char*)parser_malloc(p, (size_t)member.length + 1u);
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
    ASTDefer* defer = (ASTDefer*)parser_ast_create(p, AST_DEFER);
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
        use = (ASTUse*)parser_ast_create(p, AST_USE);
        if (!use) {
            ast_destroy((ASTNode*)head);
            return NULL;
        }
        use->base.line = module_token.line;
        use->base.col = module_token.col;
        use->module_name = (char*)parser_malloc(p, (size_t)module_token.length + 1u);
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
            use->alias = (char*)parser_malloc(p, (size_t)alias_token.length + 1u);
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
    ASTThreadBlock* tb = (ASTThreadBlock*)parser_ast_create(p, AST_THREAD_BLOCK);
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
    unsigned int postfix_depth = 0u;
    ASTNode* expr = parse_primary(p);
    if (!expr) return NULL;

    for (;;) {
        if (match(p, TOKEN_LBRACKET)) {
            Token open = p->previous;
            ASTIndexAccess* access;
            ASTNode* index_expr;
            if (postfix_depth++ >= 32u) {
                error_at(p, &open, ERR_PARSER_UNEXPECTED_TOKEN,
                    "Maximum postfix-chain depth exceeded.");
                ast_destroy(expr);
                return NULL;
            }
            index_expr = parse_expression(p);
            if (!index_expr) {
                ast_destroy(expr);
                return NULL;
            }
            if (p->current.type != TOKEN_RBRACKET) {
                error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
                    "Expected ']' after index expression.");
                ast_destroy(expr);
                ast_destroy(index_expr);
                return NULL;
            }
            advance(p);
            access = (ASTIndexAccess*)parser_ast_create(p, AST_INDEX_ACCESS);
            if (!access) {
                ast_destroy(expr);
                ast_destroy(index_expr);
                return NULL;
            }
            access->base.line = open.line;
            access->base.col = open.col;
            access->target = expr;
            access->index = index_expr;
            expr = (ASTNode*)access;
        } else if (match(p, TOKEN_DOT)) {
            if (postfix_depth++ >= 32u) {
                error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN,
                    "Maximum postfix-chain depth exceeded.");
                ast_destroy(expr);
                return NULL;
            }
            if (!match(p, TOKEN_IDENTIFIER)) {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(ERR_PARSER_EXPECTED_EXPRESSION));
                ast_destroy(expr);
                return NULL;
            }
            Token member = p->previous;
            ASTMemberAccess* node = (ASTMemberAccess*)parser_ast_create(p, AST_MEMBER_ACCESS);
            if (!node) {
                ast_destroy(expr);
                return NULL;
            }
            node->base.line = member.line;
            node->base.col = member.col;
            node->object = expr;
            node->member_name = (char*)parser_malloc(p, (size_t)member.length + 1u);
            if (!node->member_name) {
                ast_destroy((ASTNode*)node);
                return NULL;
            }
            memcpy(node->member_name, member.start, (size_t)member.length);
            node->member_name[member.length] = '\0';
            expr = (ASTNode*)node;
        } else if (match(p, TOKEN_LPAREN)) {
            if (postfix_depth++ >= 32u) {
                error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN,
                    "Maximum postfix-chain depth exceeded.");
                ast_destroy(expr);
                return NULL;
            }
            ASTCall* call = (ASTCall*)parser_ast_create(p, AST_FUNC_CALL);
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
            if (postfix_depth++ >= 32u) {
                error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN,
                    "Maximum postfix-chain depth exceeded.");
                ast_destroy(expr);
                return NULL;
            }
            ASTUnaryExpr* post = (ASTUnaryExpr*)parser_ast_create(p, AST_POSTFIX_EXPR);
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

static ASTNode* parse_power(Parser* p) {
    ASTNode* left=parse_postfix(p); if(!left)return NULL;
    if(match(p,TOKEN_POWER)){
        ASTNode*right;
        if(p->nesting_depth>=PARSER_MAX_NESTING_DEPTH){error_at(p,&p->previous,ERR_PARSER_UNEXPECTED_TOKEN,"Maximum parser nesting depth exceeded.");ast_destroy(left);return NULL;}
        ++p->nesting_depth;right=parse_unary(p);--p->nesting_depth;
        if(!right){ast_destroy(left);return NULL;}
        return parser_make_binary(p,TOKEN_POWER,left,right);
    }
    return left;
}
static ASTNode* parse_unary(Parser* p) {
    if (match(p, TOKEN_INC) || match(p, TOKEN_DEC) ||
        match(p, TOKEN_NOT) || match(p, TOKEN_MINUS) || match(p, TOKEN_BIT_NOT)) {
        OmniTokenType op = p->previous.type;
        ASTNode* operand;
        if (p->nesting_depth >= PARSER_MAX_NESTING_DEPTH) {
            error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN,
                "Maximum parser nesting depth exceeded.");
            return NULL;
        }
        ++p->nesting_depth;
        operand = parse_unary(p);
        --p->nesting_depth;
        ASTUnaryExpr* unary;
        if (!operand) return NULL;
        unary = (ASTUnaryExpr*)parser_ast_create(p, AST_UNARY_EXPR);
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
    return parse_power(p);
}

// Binary expression construction keeps ownership of both operands on every path.
static ASTNode* parser_make_binary(Parser* p, OmniTokenType op, ASTNode* left, ASTNode* right) {
    ASTBinaryExpr* binary = (ASTBinaryExpr*)parser_ast_create(p, AST_BINARY_EXPR);
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
    unsigned int chain_depth = 0u;
    ASTNode* left = parse_unary(p);
    if (!left) return NULL;
    while (match(p, TOKEN_STAR) || match(p, TOKEN_SLASH) ||
           match(p, TOKEN_PERCENT)) {
        OmniTokenType op = p->previous.type;
        if (chain_depth++ >= 32u) {
            error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN,
                "Maximum expression operator-chain depth exceeded.");
            ast_destroy(left);
            return NULL;
        }
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
    unsigned int chain_depth = 0u;
    ASTNode* left = parse_additive(p);
    if (!left) return NULL;
    while (match(p, TOKEN_GT) || match(p, TOKEN_GTE) ||
           match(p, TOKEN_LT) || match(p, TOKEN_LTE)) {
        OmniTokenType op = p->previous.type;
        if (chain_depth++ >= 32u) {
            error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN,
                "Maximum expression operator-chain depth exceeded.");
            ast_destroy(left);
            return NULL;
        }
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
    unsigned int chain_depth = 0u;
    ASTNode* left = parse_comparison(p);
    if (!left) return NULL;
    while (match(p, TOKEN_EQ) || match(p, TOKEN_NEQ) ||
           match(p, TOKEN_STRICT_EQ) || match(p, TOKEN_FORCE_EQ)) {
        OmniTokenType op = p->previous.type;
        if (chain_depth++ >= 32u) {
            error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN,
                "Maximum expression operator-chain depth exceeded.");
            ast_destroy(left);
            return NULL;
        }
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
    unsigned int chain_depth = 0u;
    ASTNode* left = parse_term(p);
    if (!left) return NULL;
    while (match(p, TOKEN_PLUS) || match(p, TOKEN_MINUS)) {
        OmniTokenType op = p->previous.type;
        if (chain_depth++ >= 32u) {
            error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN,
                "Maximum expression operator-chain depth exceeded.");
            ast_destroy(left);
            return NULL;
        }
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

static ASTNode* parse_bitwise_and(Parser* p) {
    unsigned int depth=0u;ASTNode*left=parse_equality(p);if(!left)return NULL;
    while(match(p,TOKEN_BIT_AND)){ASTNode*right;if(depth++>=32u){error_at(p,&p->previous,ERR_PARSER_UNEXPECTED_TOKEN,"Maximum bitwise chain depth exceeded.");ast_destroy(left);return NULL;}right=parse_equality(p);if(!right){ast_destroy(left);return NULL;}left=parser_make_binary(p,TOKEN_BIT_AND,left,right);if(!left)return NULL;}return left;
}
static ASTNode* parse_bitwise_xor(Parser* p) {
    unsigned int depth=0u;ASTNode*left=parse_bitwise_and(p);if(!left)return NULL;
    while(match(p,TOKEN_BIT_XOR)){ASTNode*right;if(depth++>=32u){error_at(p,&p->previous,ERR_PARSER_UNEXPECTED_TOKEN,"Maximum bitwise chain depth exceeded.");ast_destroy(left);return NULL;}right=parse_bitwise_and(p);if(!right){ast_destroy(left);return NULL;}left=parser_make_binary(p,TOKEN_BIT_XOR,left,right);if(!left)return NULL;}return left;
}
static ASTNode* parse_bitwise_or(Parser* p) {
    unsigned int depth=0u;ASTNode*left=parse_bitwise_xor(p);if(!left)return NULL;
    while(match(p,TOKEN_BIT_OR)){ASTNode*right;if(depth++>=32u){error_at(p,&p->previous,ERR_PARSER_UNEXPECTED_TOKEN,"Maximum bitwise chain depth exceeded.");ast_destroy(left);return NULL;}right=parse_bitwise_xor(p);if(!right){ast_destroy(left);return NULL;}left=parser_make_binary(p,TOKEN_BIT_OR,left,right);if(!left)return NULL;}return left;
}
static ASTNode* parse_logical_and(Parser* p) {
    unsigned int chain_depth = 0u;
    ASTNode* left = parse_range(p);
    if (!left) return NULL;
    while (match(p, TOKEN_AND)) {
        OmniTokenType op = p->previous.type;
        ASTNode* right;
        if (chain_depth++ >= 32u) {
            error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN,
                "Maximum logical operator-chain depth exceeded.");
            ast_destroy(left);
            return NULL;
        }
        right = parse_range(p);
        if (!right) {
            ast_destroy(left);
            return NULL;
        }
        left = parser_make_binary(p, op, left, right);
        if (!left) return NULL;
    }
    return left;
}

static ASTNode* parse_logical_or(Parser* p) {
    unsigned int chain_depth = 0u;
    ASTNode* left = parse_logical_and(p);
    if (!left) return NULL;
    while (match(p, TOKEN_OR)) {
        OmniTokenType op = p->previous.type;
        ASTNode* right;
        if (chain_depth++ >= 32u) {
            error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN,
                "Maximum logical operator-chain depth exceeded.");
            ast_destroy(left);
            return NULL;
        }
        right = parse_logical_and(p);
        if (!right) {
            ast_destroy(left);
            return NULL;
        }
        left = parser_make_binary(p, op, left, right);
        if (!left) return NULL;
    }
    return left;
}

static ASTNode* parse_nexus_flow(Parser* p) {
    ASTNode* expr = parse_logical_or(p);
    if (!expr) return NULL;
    
        if (match(p, TOKEN_ELSE_FLOW) || match(p, TOKEN_PIPE) || 
            match(p, TOKEN_IGNORE) || match(p, TOKEN_IF_FLOW) || 
            match(p, TOKEN_FALLBACK) || match(p, TOKEN_RELOCATE) ||
            match(p, TOKEN_LSHIFT) || match(p, TOKEN_RSHIFT) ||
            p->current.type == TOKEN_QMARK) {
            
        OmniTokenType op;
        int retry_count = 0;
        int retry_delay = 0;
        int is_rolling = 0;

        // Rolling Check: ?(n,ms)->
        if (p->current.type == TOKEN_QMARK) {
            is_rolling = 1;
            advance(p); // Consume ?
            if (match(p, TOKEN_LPAREN)) {
                {
                    Token number;
                    char buffer[32];
                    char* end=NULL;
                    long value;
                    if (!match(p, TOKEN_NUMBER)) {
                        error_at(p,&p->current,ERR_PARSER_EXPECTED_EXPRESSION,"Expected non-negative retry count.");
                        return expr;
                    }
                    number=p->previous;
                    if(number.length<=0 || (size_t)number.length>=sizeof(buffer)) {
                        error_at(p,&number,ERR_PARSER_UNEXPECTED_TOKEN,"Retry count is out of range.");
                        return expr;
                    }
                    memcpy(buffer,number.start,(size_t)number.length);buffer[number.length]='\0';
                    errno=0;value=strtol(buffer,&end,10);
                    if(errno || !end || *end!='\0' || value<0 || value>INT_MAX) {
                        error_at(p,&number,ERR_PARSER_UNEXPECTED_TOKEN,"Retry count is out of range.");
                        return expr;
                    }
                    retry_count=(int)value;
                    consume(p, TOKEN_COMMA, ERR_PARSER_EXPECTED_EXPRESSION, MSG_EXPECT_COMMA_ROLLING);
                    if (!match(p, TOKEN_NUMBER)) {
                        error_at(p,&p->current,ERR_PARSER_EXPECTED_EXPRESSION,"Expected retry delay in milliseconds.");
                        return expr;
                    }
                    number=p->previous;
                    if(number.length<=0 || (size_t)number.length>=sizeof(buffer)) {
                        error_at(p,&number,ERR_PARSER_UNEXPECTED_TOKEN,"Retry delay is out of range.");
                        return expr;
                    }
                    memcpy(buffer,number.start,(size_t)number.length);buffer[number.length]='\0';
                    errno=0;value=strtol(buffer,&end,10);
                    if(errno || !end || *end!='\0' || value<0 || value>INT_MAX) {
                        error_at(p,&number,ERR_PARSER_UNEXPECTED_TOKEN,"Retry delay is out of range.");
                        return expr;
                    }
                    retry_delay=(int)value;
                    if(p->current.type==TOKEN_IDENTIFIER && parser_identifier_is(&p->current,"ms")) advance(p);
                }
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
        
        if (op == TOKEN_RELOCATE) {
            ASTRelocate* relocate = (ASTRelocate*)parser_ast_create(p, AST_RELOCATE);
            if (!relocate) { ast_destroy(expr); ast_destroy(right); return NULL; }
            relocate->base.line = p->previous.line;
            relocate->base.col = p->previous.col;
            relocate->source = expr;
            relocate->target = right;
            expr = (ASTNode*)relocate;
        } else if (!is_rolling && (op == TOKEN_PIPE || op == TOKEN_LSHIFT || op == TOKEN_RSHIFT)) {
            ASTNexusFlow* pipe = (ASTNexusFlow*)parser_ast_create(p, AST_NEXUS_PIPE);
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
            ASTNexusFlow* flow = (ASTNexusFlow*)parser_ast_create(p, AST_NEXUS_FLOW);
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
        ASTUnpack* unpack = (ASTUnpack*)parser_ast_create(p, AST_UNPACK);
        if (!unpack) {
            ast_destroy(expr);
            return NULL;
        }
        unpack->base.line = p->previous.line;
        unpack->base.col = p->previous.col;
        unpack->expr = expr;
        expr = (ASTNode*)unpack;
        if (p->nesting_depth >= PARSER_MAX_NESTING_DEPTH) {
            error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN,
                "Maximum parser nesting depth exceeded.");
            ast_destroy(expr);
            return NULL;
        }
        ++p->nesting_depth;
        ASTNode* unpacked = parse_nexus_flow(p);
        --p->nesting_depth;
        if (!unpacked) ast_destroy(expr);
        return unpacked;
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
        
        ASTNexusCapture* capture = (ASTNexusCapture*)parser_ast_create(p, AST_NEXUS_CAPTURE);
        if (!capture) {
            ast_destroy(expr);
            return NULL;
        }
        capture->base.line = p->previous.line;
        capture->base.col = p->previous.col;
        capture->handler_name = parser_strdup(p, ((ASTIdentifier*)expr)->name);
        if (!capture->handler_name) {
            ast_destroy((ASTNode*)capture);
            ast_destroy(expr);
            return NULL;
        }
        if (p->nesting_depth >= PARSER_MAX_NESTING_DEPTH) {
            error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN,
                "Maximum parser nesting depth exceeded.");
            ast_destroy((ASTNode*)capture);
            ast_destroy(expr);
            return NULL;
        }
        ++p->nesting_depth;
        capture->expr = parse_assignment(p);
        --p->nesting_depth;
        ast_destroy(expr);
        if (!capture->expr) {
            ast_destroy((ASTNode*)capture);
            return NULL;
        }
        return (ASTNode*)capture;
    }

    if (match(p, TOKEN_ASSIGN) || match(p, TOKEN_PLUS_EQ) ||
        match(p, TOKEN_MINUS_EQ) || match(p, TOKEN_STAR_EQ) ||
        match(p, TOKEN_SLASH_EQ) || match(p, TOKEN_BIT_AND_EQ) ||
        match(p, TOKEN_BIT_OR_EQ) || match(p, TOKEN_BIT_XOR_EQ) ||
        match(p, TOKEN_STRING_APPEND)) {
        OmniTokenType assignment_op = p->previous.type;
        ASTNode* right;
        if (p->nesting_depth >= PARSER_MAX_NESTING_DEPTH) {
            error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN,
                "Maximum parser nesting depth exceeded.");
            ast_destroy(expr);
            return NULL;
        }
        ++p->nesting_depth;
        right = parse_assignment(p);
        --p->nesting_depth;
        if (!right) {
            ast_destroy(expr);
            return NULL;
        }
        ASTBinaryExpr* assign = (ASTBinaryExpr*)parser_ast_create(p, AST_ASSIGNMENT);
        if (!assign) {
            ast_destroy(expr);
            ast_destroy(right);
            return NULL;
        }
        assign->base.line = p->previous.line;
        assign->base.col = p->previous.col;
        assign->op = assignment_op;
        assign->left = expr;
        assign->right = right;
        return (ASTNode*)assign;
    }

    if (match(p, TOKEN_FORCE_ASSIGN)) {
        ASTNode* right;
        if (p->nesting_depth >= PARSER_MAX_NESTING_DEPTH) {
            error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN,
                "Maximum parser nesting depth exceeded.");
            ast_destroy(expr);
            return NULL;
        }
        ++p->nesting_depth;
        right = parse_assignment(p);
        --p->nesting_depth;
        if (!right) {
            ast_destroy(expr);
            return NULL;
        }
        ASTBinaryExpr* assign = (ASTBinaryExpr*)parser_ast_create(p, AST_ASSIGNMENT);
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
    ASTNode* expression;
    if (p->nesting_depth >= PARSER_MAX_NESTING_DEPTH) {
        error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
            "Maximum parser nesting depth exceeded.");
        return NULL;
    }
    ++p->nesting_depth;
    // İfade hiyerarşisi: Assignment/Capture -> Nexus Flow -> Additive ...
    expression = parse_assignment(p);
    --p->nesting_depth;
    return expression;
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
        } else {
            if (p->current.type != TOKEN_ERROR) {
                error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
                    msg_get(ERR_PARSER_UNEXPECTED_TOKEN));
            }
            advance(p);
        }
    }
    decl->fields = head;
    consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
    return 1;
}

/* Parse declarator dimensions such as [5][n][] into an owned type subtree.
 * The base element type remains on ASTVarDecl.type_name; an empty dimension
 * has length == NULL and therefore represents a dynamic/unspecified extent.
 */
static int parse_array_type_suffix(Parser* p, ASTVarDecl* decl) {
    ASTArrayType* array_type;
    ASTNode* tail = NULL;
    if (!p || !decl || p->current.type != TOKEN_LBRACKET) return 1;
    if (decl->inline_type) {
        error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
            "An inline type cannot also have an array declarator.");
        return 0;
    }
    array_type = (ASTArrayType*)parser_ast_create(p, AST_ARRAY_TYPE);
    if (!array_type) return 0;
    array_type->base.line = p->current.line;
    array_type->base.col = p->current.col;
    while (match(p, TOKEN_LBRACKET)) {
        Token open = p->previous;
        ASTArrayDimension* dimension =
            (ASTArrayDimension*)parser_ast_create(p, AST_ARRAY_DIMENSION);
        if (!dimension) {
            ast_destroy((ASTNode*)array_type);
            return 0;
        }
        dimension->base.line = open.line;
        dimension->base.col = open.col;
        if (p->current.type != TOKEN_RBRACKET) {
            dimension->length = parse_expression(p);
            if (!dimension->length) {
                ast_destroy((ASTNode*)dimension);
                ast_destroy((ASTNode*)array_type);
                return 0;
            }
        }
        if (p->current.type != TOKEN_RBRACKET) {
            error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
                "Expected ']' after array dimension.");
            ast_destroy((ASTNode*)dimension);
            ast_destroy((ASTNode*)array_type);
            return 0;
        }
        advance(p);
        if (!array_type->dimensions) array_type->dimensions = (ASTNode*)dimension;
        else tail->next = (ASTNode*)dimension;
        tail = (ASTNode*)dimension;
    }
    decl->inline_type = (ASTNode*)array_type;
    return 1;
}

// Variable Declaration: v:name = expr;
static ASTNode* parse_var_decl(Parser* p) {
    // v: veya c: tüketildi, p->previous içinde
    int is_const = (p->previous.type == TOKEN_PRE_CONST);
    Token t = p->previous;

    // Lexer "v:isim" şeklinde tek token döndürdüğü için ismi ayıklıyoruz
    int prefix_len = (t.type == TOKEN_PRE_VAR || t.type == TOKEN_PRE_CONST) ? 2 : 0;
    
    ASTVarDecl* decl = (ASTVarDecl*)parser_ast_create(p, AST_VAR_DECL);
    if (!decl) return NULL;
    decl->base.line = t.line;
    decl->base.col = t.col;
    decl->is_const = is_const;
    decl->inline_type = NULL;
    
    if (t.length > prefix_len) {
        decl->name = (char*)parser_malloc(p, (size_t)(t.length - prefix_len) + 1u);
        if (!decl->name) { ast_destroy((ASTNode*)decl); return NULL; }
        memcpy(decl->name, t.start + prefix_len, (size_t)(t.length - prefix_len)); 
        decl->name[t.length - prefix_len] = '\0';
    } else if (t.length == prefix_len) {
        // Separated: v: name
        if (match(p, TOKEN_IDENTIFIER)) {
            Token id = p->previous;
            decl->name = (char*)parser_malloc(p, (size_t)id.length + 1u);
            if (!decl->name) { ast_destroy((ASTNode*)decl); return NULL; }
            memcpy(decl->name, id.start, (size_t)id.length);
            decl->name[id.length] = '\0';
        } else {
            error_at(p, &p->current, ERR_PARSER_INVALID_VAR, msg_get(ERR_PARSER_INVALID_VAR));
        }
    } else {
        error_at(p, &p->previous, ERR_PARSER_INVALID_VAR, msg_get(ERR_PARSER_INVALID_VAR));
    }

    /* Array dimensions follow the declared variable name: v:a[4][8]!f32. */
    if (!parse_array_type_suffix(p, decl)) { ast_destroy((ASTNode*)decl); return NULL; }

    // Tip Tanımı: !Type
    if (match(p, TOKEN_NOT)) {
        if (decl->inline_type &&
            (p->current.type == TOKEN_KW_STRUCT || p->current.type == TOKEN_KW_UNION)) {
            error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
                "Inline struct/union definitions cannot be combined with an array declarator; declare the aggregate type separately.");
            ast_destroy((ASTNode*)decl);
            return NULL;
        }
        decl->is_hard_typed = 1;
        
        if (match(p, TOKEN_KW_STRUCT)) {
             decl->type_name = parser_strdup(p, "struct");
             if (!decl->type_name) { ast_destroy((ASTNode*)decl); return NULL; }
             if (p->current.type == TOKEN_LBRACE) {
                 ASTStructDecl* sdecl = (ASTStructDecl*)parser_ast_create(p, AST_STRUCT_DECL);
                 if (!sdecl) { ast_destroy((ASTNode*)decl); return NULL; }
                 sdecl->base.line = p->previous.line;
                 sdecl->base.col = p->previous.col;
                 if (!parse_struct_body(p, sdecl)) { ast_destroy((ASTNode*)sdecl); ast_destroy((ASTNode*)decl); return NULL; }
                 decl->inline_type = (ASTNode*)sdecl;
             }
        } else if (match(p, TOKEN_KW_UNION)) {
             decl->type_name = parser_strdup(p, "union");
             if (!decl->type_name) { ast_destroy((ASTNode*)decl); return NULL; }
             if (p->current.type == TOKEN_LBRACE) {
                 ASTStructDecl* sdecl = (ASTStructDecl*)parser_ast_create(p, AST_UNION_DECL);
                 if (!sdecl) { ast_destroy((ASTNode*)decl); return NULL; }
                 sdecl->base.line = p->previous.line;
                 sdecl->base.col = p->previous.col;
                 sdecl->is_union = 1;
                 if (!parse_struct_body(p, sdecl)) { ast_destroy((ASTNode*)sdecl); ast_destroy((ASTNode*)decl); return NULL; }
                 decl->inline_type = (ASTNode*)sdecl;
             }
        } else if ((p->current.type >= TOKEN_TYPE_U8 && p->current.type <= TOKEN_TYPE_ANY) || p->current.type == TOKEN_IDENTIFIER) {
             Token type_token = p->current;
             decl->type_name = (char*)parser_malloc(p, (size_t)type_token.length + 1u);
             if (!decl->type_name) { ast_destroy((ASTNode*)decl); return NULL; }
             memcpy(decl->type_name, type_token.start, (size_t)type_token.length);
             decl->type_name[type_token.length] = '\0';
             advance(p);
        } else {
             error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_RET_TYPE));
        }
    }

    if (match(p, TOKEN_ASSIGN) || match(p, TOKEN_PLUS_EQ) ||
        match(p, TOKEN_MINUS_EQ) || match(p, TOKEN_STAR_EQ) ||
        match(p, TOKEN_SLASH_EQ) || match(p, TOKEN_BIT_AND_EQ) ||
        match(p, TOKEN_BIT_OR_EQ) || match(p, TOKEN_BIT_XOR_EQ) ||
        match(p, TOKEN_STRING_APPEND)) {
        OmniTokenType assignment_op=p->previous.type;
        ASTNode* right;
}