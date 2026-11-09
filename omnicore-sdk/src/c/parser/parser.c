#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

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
    unsigned int fast_exec_depth;
    int consumed_metadata;
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

    span->source_id = ctx->source_id;
    span->start_offset = (uint64_t)start;
    span->end_offset = (uint64_t)(start + length);
    span->start_line = (uint32_t)token->line;
    span->start_column = (uint32_t)token->col;
    span->end_line = line;
    span->end_column = column;
    span->has_location = 1u;
}

/* Stamp a leaf node from the exact token that produced it. */
static void parser_stamp_node_token(
    Parser* p, ASTNode* node, const Token* token
) {
    OccSourceSpan span;
    if (!p || !p->ctx || !node || !token) return;
    parser_make_token_span(p->ctx, token, &span);
    if (span.has_location) (void)ast_set_source_span(node, &span);
}

static void parser_stamp_node_source_file(Parser* p, ASTNode* node) {
    OccSourceSpan span;
    size_t i;
    uint32_t line = 1u;
    uint32_t column = 1u;
    if (!p || !p->ctx || !node || !p->ctx->source_buffer) return;
    memset(&span, 0, sizeof(span));
    span.source_id = p->ctx->source_id;
    span.start_offset = 0u;
    span.end_offset = (uint64_t)p->ctx->source_len;
    span.start_line = 1u;
    span.start_column = 1u;
    for (i = 0u; i < p->ctx->source_len; ++i) {
        if (p->ctx->source_buffer[i] == '\n') {
            ++line;
            column = 1u;
        } else {
            ++column;
        }
    }
    span.end_line = line;
    span.end_column = column;
    span.has_location = 1u;
    (void)ast_set_source_span(node, &span);
}

static void parser_stamp_node_tokens(
    Parser* p, ASTNode* node, const Token* first, const Token* last
) {
    OccSourceSpan span;
    OccSourceSpan end_span;
    if (!p || !p->ctx || !node || !first || !last) return;
    parser_make_token_span(p->ctx, first, &span);
    parser_make_token_span(p->ctx, last, &end_span);
    if (!span.has_location || !end_span.has_location ||
        span.source_id != end_span.source_id ||
        end_span.end_offset < span.start_offset) return;
    span.end_offset = end_span.end_offset;
    span.end_line = end_span.end_line;
    span.end_column = end_span.end_column;
    (void)ast_set_source_span(node, &span);
}

/* Compose a production span from its first and last owned AST children. */
static void parser_stamp_node_child_to_token(
    Parser* p, ASTNode* node, const ASTNode* first, const Token* last
) {
    OccSourceSpan span;
    OccSourceSpan end_span;
    if (!p || !p->ctx || !node || !first || !last ||
        !first->source_span.has_location) return;
    span = first->source_span;
    parser_make_token_span(p->ctx, last, &end_span);
    if (!end_span.has_location || span.source_id != end_span.source_id ||
        end_span.end_offset < span.start_offset) return;
    span.end_offset = end_span.end_offset;
    span.end_line = end_span.end_line;
    span.end_column = end_span.end_column;
    (void)ast_set_source_span(node, &span);
}

static void parser_stamp_node_child_range(
    ASTNode* node, const ASTNode* first, const ASTNode* last
) {
    OccSourceSpan span;
    if (!node || !first || !last ||
        !first->source_span.has_location || !last->source_span.has_location ||
        first->source_span.source_id != last->source_span.source_id ||
        last->source_span.end_offset < first->source_span.start_offset) return;
    span = first->source_span;
    span.end_offset = last->source_span.end_offset;
    span.end_line = last->source_span.end_line;
    span.end_column = last->source_span.end_column;
    (void)ast_set_source_span(node, &span);
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
        case TOKEN_PRE_MUST:
        case TOKEN_PRE_FUNC:
        case TOKEN_KW_FN:
        case TOKEN_KW_RET:
        case TOKEN_KW_IF:
        case TOKEN_KW_FASTEXEC:
        case TOKEN_KW_ASM:
        case TOKEN_KW_ASMCALL:
        case TOKEN_KW_ASMJMP:
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
    uint8_t old_asm_state, old_asm_pending;
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
    old_asm_state = p->ctx->lexer_asm_state;
    old_asm_pending = p->ctx->lexer_asm_body_pending;

    for (i = 0; i < n; ++i) token = parser_next_token(p->ctx);

    p->ctx->pos = old_pos;
    p->ctx->current_line = old_line;
    p->ctx->current_col = old_col;
    p->ctx->token_start_offset = old_token_offset;
    p->ctx->token_start_line = old_token_line;
    p->ctx->token_start_col = old_token_col;
    p->ctx->token_stream_index = old_token_index;
    p->ctx->lexer_asm_state = old_asm_state;
    p->ctx->lexer_asm_body_pending = old_asm_pending;
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
static ASTNode* parse_fast_exec_block(Parser* p);
static ASTNode* parse_asm_label(Parser* p);
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
static ASTNode* parse_postfix(Parser* p);
static ASTNode* parser_make_binary(Parser* p, OmniTokenType op, ASTNode* left, ASTNode* right);
static ASTNode* parse_range(Parser* p);
static ASTNode* parse_logical_and(Parser* p);
static ASTNode* parse_logical_or(Parser* p);
static ASTNode* parse_equality(Parser* p);
static int parse_struct_body(Parser* p, ASTStructDecl* decl);
static ASTNode* parse_comparison(Parser* p);
static ASTNode* parse_bitwise_or(Parser* p);
static ASTNode* parse_additive(Parser* p);
static ASTNode* parse_unary(Parser* p);
static ASTNode* parse_intrinsic_call(Parser* p, ASTNodeType type);
static ASTNode* parse_defer(Parser* p);
static ASTNode* parse_use(Parser* p);
static ASTNode* parse_thread_block(Parser* p, char* name);
static ASTNode* parse_lambda(Parser* p);

// Struct literal: Type { field = expression, ... }.
static ASTNode* parse_struct_literal(Parser* p, const Token* type_token) {
    ASTStructLiteral* literal;
    ASTNode* head = NULL;
    ASTNode* tail = NULL;
    if (!p || !type_token || !check(p, TOKEN_LBRACE)) return NULL;
    advance(p);
    literal = (ASTStructLiteral*)parser_ast_create(p, AST_STRUCT_LITERAL);
    if (!literal) return NULL;
    literal->base.line = type_token->line;
    literal->base.col = type_token->col;
    literal->type_name = (char*)parser_malloc(p, (size_t)type_token->length + 1u);
    if (!literal->type_name) { ast_destroy((ASTNode*)literal); return NULL; }
    memcpy(literal->type_name, type_token->start, (size_t)type_token->length);
    literal->type_name[type_token->length] = '\0';
    if (!check(p, TOKEN_RBRACE)) {
        do {
            Token field_token;
            ASTStructFieldInit* field;
            ASTNode* value;
            if (!match(p, TOKEN_IDENTIFIER)) {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                    "Expected Struct field name in initializer.");
                ast_destroy((ASTNode*)literal); ast_destroy(head); return NULL;
            }
            field_token = p->previous;
            field = (ASTStructFieldInit*)parser_ast_create(p, AST_STRUCT_FIELD_INIT);
            if (!field) { ast_destroy((ASTNode*)literal); ast_destroy(head); return NULL; }
            field->base.line = field_token.line; field->base.col = field_token.col;
            field->name = (char*)parser_malloc(p, (size_t)field_token.length + 1u);
            if (!field->name) { ast_destroy((ASTNode*)field); ast_destroy((ASTNode*)literal); ast_destroy(head); return NULL; }
            memcpy(field->name, field_token.start, (size_t)field_token.length);
            field->name[field_token.length] = '\0';
            if (!match(p, TOKEN_ASSIGN)) {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                    "Expected '=' after Struct field name.");
                ast_destroy((ASTNode*)field); ast_destroy((ASTNode*)literal); ast_destroy(head); return NULL;
            }
            value = parse_expression(p);
            if (!value) { ast_destroy((ASTNode*)field); ast_destroy((ASTNode*)literal); ast_destroy(head); return NULL; }
            field->value = value;
            parser_stamp_node_tokens(p, (ASTNode*)field, &field_token, &p->previous);
            if (!head) head = (ASTNode*)field; else tail->next = (ASTNode*)field;
            tail = (ASTNode*)field;
        } while (match(p, TOKEN_COMMA) && !check(p, TOKEN_RBRACE));
    }
    if (!match(p, TOKEN_RBRACE)) {
        error_at(p, &p->current, ERR_PARSER_EXPECTED_BRACE,
            "Expected '}' after Struct initializer.");
        ast_destroy((ASTNode*)literal); ast_destroy(head); return NULL;
    }
    literal->fields = head;
    parser_stamp_node_tokens(p, (ASTNode*)literal, type_token, &p->previous);
    return (ASTNode*)literal;
}

// Primary: Literals, Identifiers, Grouping
static ASTNode* parse_primary(Parser* p) {
    if (match(p, TOKEN_NUMBER) || match(p, TOKEN_STRING) || match(p, TOKEN_CHAR)) {
        Token token = p->previous;
        ASTLiteral* node = (ASTLiteral*)parser_ast_create(p, AST_LITERAL);
        if (!node) return NULL;
        node->base.line = token.line;
        node->base.col = token.col;
        parser_stamp_node_token(p, (ASTNode*)node, &token);
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

    if (match(p, TOKEN_TRUE) || match(p, TOKEN_FALSE) || match(p, TOKEN_NULL)) {
        Token token = p->previous;
        const char* value = token.type == TOKEN_TRUE ? "true" :
            (token.type == TOKEN_FALSE ? "false" : "null");
        ASTLiteral* node = (ASTLiteral*)parser_ast_create(p, AST_LITERAL);
        if (!node) return NULL;
        node->base.line = token.line;
        node->base.col = token.col;
        parser_stamp_node_token(p, (ASTNode*)node, &token);
        node->type = token.type;
        node->string_value = parser_strdup(p, value);
        if (!node->string_value) {
            ast_destroy((ASTNode*)node);
            return NULL;
        }
        return (ASTNode*)node;
    }

    if (match(p, TOKEN_IDENTIFIER)) {
        Token token = p->previous;
        ASTIdentifier* id_node = (ASTIdentifier*)parser_ast_create(p, AST_IDENTIFIER);
        if (!id_node) return NULL;
        id_node->base.line = token.line;
        id_node->base.col = token.col;
        parser_stamp_node_token(p, (ASTNode*)id_node, &token);
        id_node->name = (char*)parser_malloc(p, (size_t)token.length + 1u);
        if (!id_node->name) {
            ast_destroy((ASTNode*)id_node);
            return NULL;
        }
        memcpy(id_node->name, token.start, (size_t)token.length);
        id_node->name[token.length] = '\0';
        if (check(p, TOKEN_LBRACE)) {
            ASTNode* literal = parse_struct_literal(p, &token);
            ast_destroy((ASTNode*)id_node);
            return literal;
        }
        return (ASTNode*)id_node;
    }

    if (check(p, TOKEN_PRE_FUNC) && peek_token(p).type == TOKEN_LPAREN) {
        advance(p); /* f: followed by a parameter list is a lambda expression. */
        return parse_lambda(p);
    }
    if (check(p, TOKEN_KW_FN) && peek_token(p).type == TOKEN_LPAREN) {
        advance(p);
        return parse_lambda(p);
    }

    if (match(p, TOKEN_KW_FN)) {
        Token function_start = p->previous;
        ASTNode* function = parse_func_decl(p);
        if (function)
            parser_stamp_node_tokens(p, function, &function_start, &p->previous);
        return function;
    }

    if (match(p, TOKEN_KW_LOOP)) {
        Token loop_start = p->previous;
        ASTLoop* loop = (ASTLoop*)parser_ast_create(p, AST_LOOP);
        if (!loop) return NULL;
        loop->base.line = p->previous.line;
        loop->base.col = p->previous.col;
        loop->loop_type = 0;

        /* Canonical infinite-loop form: loop { ... }. */
        if (match(p, TOKEN_LBRACE)) {
            loop->body = parse_block(p);
            if (!loop->body) {
                ast_destroy((ASTNode*)loop);
                return NULL;
            }
            parser_stamp_node_tokens(p, (ASTNode*)loop, &loop_start, &p->previous);
            return (ASTNode*)loop;
        }

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
        if (loop->body) parser_stamp_node_tokens(p, (ASTNode*)loop, &loop_start, &p->previous);
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
        parser_stamp_node_tokens(p, (ASTNode*)array, &open, &p->previous);
        return (ASTNode*)array;
    }

    if (match(p, TOKEN_LPAREN)) {
        Token open = p->previous;
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
            if (loop->body) parser_stamp_node_tokens(p, (ASTNode*)loop, &open, &p->previous);
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
            parser_stamp_node_tokens(p, (ASTNode*)tuple, &open, &p->previous);
            return (ASTNode*)tuple;
        }

        consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);
        return expr;
    }

    if (match(p, TOKEN_KW_ASMCALL) || match(p, TOKEN_KW_ASMJMP)) {
        Token intrinsic = p->previous;
        const char* name = intrinsic.type == TOKEN_KW_ASMCALL ? "asmcall" : "asmjmp";
        ASTCall* call = (ASTCall*)parse_intrinsic_call(p, AST_FUNC_CALL);
        ASTIdentifier* callee;
        if (!call) return NULL;
        callee = (ASTIdentifier*)parser_ast_create(p, AST_IDENTIFIER);
        if (!callee) { ast_destroy((ASTNode*)call); return NULL; }
        callee->base.line = intrinsic.line;
        callee->base.col = intrinsic.col;
        parser_stamp_node_token(p, (ASTNode*)callee, &intrinsic);
        callee->name = (char*)parser_malloc(p, strlen(name) + 1u);
        if (!callee->name) {
            ast_destroy((ASTNode*)callee);
            ast_destroy((ASTNode*)call);
            return NULL;
        }
        memcpy(callee->name, name, strlen(name) + 1u);
        call->callee = (ASTNode*)callee;
        return (ASTNode*)call;
    }
    if (match(p, TOKEN_KW_ECHO)) return parse_intrinsic_call(p, AST_ECHO);
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
    if (check(p, TOKEN_KW_SPAWN)) {
        Token spawn_token = p->current;
        OccSourceSpan span;
        parser_make_token_span(p->ctx, &spawn_token, &span);
        p->panic_mode = 1;
        parser_add_issue(p->ctx, ERR_PARSER_UNEXPECTED_TOKEN,
            spawn_token.line, spawn_token.col,
            "'spawn' belongs to the separate Thread module and is not a core AST intrinsic.",
            NULL, NULL, 0, 0, &span);
        p->had_error = 1;
        return NULL;
    }
    if (match(p, TOKEN_KW_SEND)) return parse_intrinsic_call(p, AST_SEND);
    if (match(p, TOKEN_KW_RECEIVE)) return parse_intrinsic_call(p, AST_RECEIVE);
    if (match(p, TOKEN_KW_DONE)) return parse_intrinsic_call(p, AST_DONE);
    if (match(p, TOKEN_KW_LEN)) return parse_intrinsic_call(p, AST_LEN);

    // Support for { expr, expr } (Struct Init / Tuple)
    if (match(p, TOKEN_LBRACE)) {
        Token open = p->previous;
        // { } Empty tuple check
        if (match(p, TOKEN_RBRACE)) {
             ASTTuple* tuple = (ASTTuple*)parser_ast_create(p, AST_TUPLE);
             if (!tuple) return NULL;
             tuple->base.line = p->previous.line;
             tuple->base.col = open.col;
             tuple->elements = NULL;
             parser_stamp_node_tokens(p, (ASTNode*)tuple, &open, &p->previous);
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
        parser_stamp_node_tokens(p, (ASTNode*)tuple, &open, &p->previous);
        return (ASTNode*)tuple;
    }

    if (match(p, TOKEN_AI)) {
        Token ai_start = p->previous;
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
        parser_stamp_node_tokens(p, (ASTNode*)ai, &ai_start, &p->previous);
        return (ASTNode*)ai;
    }

    error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(ERR_PARSER_EXPECTED_EXPRESSION));
    return NULL;
}

static ASTNode* parse_intrinsic_call(Parser* p, ASTNodeType type) {
    Token intrinsic_start = p->previous;
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
            parser_stamp_node_token(p, (ASTNode*)target, &type_token);
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
        parser_stamp_node_tokens(p, (ASTNode*)cast, &intrinsic_start, &p->previous);
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
        parser_stamp_node_tokens(p, (ASTNode*)panic, &intrinsic_start, &p->previous);
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
            parser_stamp_node_token(p, (ASTNode*)id, &member);
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
            int saw_named = 0;
            do {
                ASTNode* arg = NULL;
                if (p->current.type == TOKEN_IDENTIFIER && peek_token(p).type == TOKEN_COLON) {
                    Token label = p->current;
                    ASTNamedArg* named = (ASTNamedArg*)parser_ast_create(p, AST_NAMED_ARG);
                    if (!named) { ast_destroy((ASTNode*)call); return NULL; }
                    named->base.line = label.line;
                    named->base.col = label.col;
                    named->name = (char*)parser_malloc(p, (size_t)label.length + 1u);
                    if (!named->name) { ast_destroy((ASTNode*)named); ast_destroy((ASTNode*)call); return NULL; }
                    memcpy(named->name, label.start, (size_t)label.length);
                    named->name[label.length] = '\0';
                    advance(p);
                    consume(p, TOKEN_COLON, ERR_PARSER_EXPECTED_EXPRESSION, MSG_EXPECT_COLON_VAR);
                    named->value = parse_expression(p);
                    if (!named->value) { ast_destroy((ASTNode*)named); ast_destroy((ASTNode*)call); return NULL; }
                    parser_stamp_node_tokens(p, (ASTNode*)named, &label, &p->previous);
                    arg = (ASTNode*)named;
                    saw_named = 1;
                } else {
                    if (saw_named) {
                        error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
                            "Positional arguments cannot follow named arguments.");
                        ast_destroy((ASTNode*)call);
                        return NULL;
                    }
                    arg = parse_expression(p);
                }
                if (!arg) { ast_destroy((ASTNode*)call); return NULL; }
                if (arg->type == AST_NAMED_ARG) {
                    ASTNode* prior;
                    const char* label = ((ASTNamedArg*)arg)->name;
                    for (prior = call->args; prior; prior = prior->next) {
                        if (prior->type == AST_NAMED_ARG &&
                            strcmp(((ASTNamedArg*)prior)->name, label) == 0) {
                            error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN,
                                "Duplicate named argument label.");
                            ast_destroy(arg);
                            ast_destroy((ASTNode*)call);
                            return NULL;
                        }
                    }
                }
                if (!call->args) call->args = arg;
                else tail->next = arg;
                tail = arg;
            } while (match(p, TOKEN_COMMA));
        }
        consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_ARGS);
    }
    parser_stamp_node_tokens(p, (ASTNode*)call, &intrinsic_start, &p->previous);
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

static ASTNode* parse_must_decl(Parser* p) {
    ASTMustDecl* dependency = (ASTMustDecl*)parser_ast_create(p, AST_MUST_DECL);
    Token prefix_token = p->previous;
    const char* name_start = NULL;
    size_t name_length = 0u;
    if (!dependency) return NULL;
    dependency->base.line = prefix_token.line;
    dependency->base.col = prefix_token.col;
    /* Like f:name, the lexer may return the whole m:Name prefix as one token. */
    if (prefix_token.length > 2) {
        name_start = prefix_token.start + 2;
        name_length = (size_t)prefix_token.length - 2u;
    } else if (match(p, TOKEN_IDENTIFIER)) {
        name_start = p->previous.start;
        name_length = (size_t)p->previous.length;
    } else {
        error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
            "Expected dependency name after m:.");
        ast_destroy((ASTNode*)dependency);
        return NULL;
    }
    dependency->name = (char*)parser_malloc(p, name_length + 1u);
    if (!dependency->name) {
        ast_destroy((ASTNode*)dependency);
        return NULL;
    }
    memcpy(dependency->name, name_start, name_length);
    dependency->name[name_length] = '\0';
    if (!match(p, TOKEN_ASSIGN)) {
        error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
            "Expected '=' after mandatory dependency name.");
        ast_destroy((ASTNode*)dependency);
        return NULL;
    }
    dependency->source = parse_expression(p);
    if (!dependency->source) {
        ast_destroy((ASTNode*)dependency);
        return NULL;
    }
    consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
    return (ASTNode*)dependency;
}

static ASTNode* parse_use(Parser* p) {
    ASTUse* head = NULL;
    ASTUse* tail = NULL;

    do {
        Token module_token;
        Token use_end;
        ASTUse* use;
        if (!match(p, TOKEN_IDENTIFIER)) {
            error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                "Expected an identifier after '.' in module path.");
            ast_destroy((ASTNode*)head);
            return NULL;
        }
        module_token = p->previous;
        use_end = module_token;
        use = (ASTUse*)parser_ast_create(p, AST_USE);
        if (!use) {
            ast_destroy((ASTNode*)head);
            return NULL;
        }
        use->base.line = module_token.line;
        use->base.col = module_token.col;
        {
            size_t module_length = (size_t)module_token.length;
            char* module_name = (char*)parser_malloc(p, module_length + 1u);
            if (!module_name) {
                ast_destroy((ASTNode*)use);
                ast_destroy((ASTNode*)head);
                return NULL;
            }
            memcpy(module_name, module_token.start, module_length);
            module_name[module_length] = '\0';

            /* Module paths may contain dotted segments: use platform.io as io; */
            while (match(p, TOKEN_DOT)) {
                Token segment;
                char* expanded;
                size_t segment_length;
                if (!match(p, TOKEN_IDENTIFIER)) {
                    error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                        msg_get(MSG_EXPECT_MODULE_NAME));
                    free(module_name);
                    ast_destroy((ASTNode*)use);
                    ast_destroy((ASTNode*)head);
                    return NULL;
                }
                segment = p->previous;
                segment_length = (size_t)segment.length;
                if (module_length > SIZE_MAX - segment_length - 2u) {
                    free(module_name);
                    ast_destroy((ASTNode*)use);
                    ast_destroy((ASTNode*)head);
                    parser_mark_oom(p);
                    return NULL;
                }
                expanded = (char*)parser_malloc(p, module_length + 1u + segment_length + 1u);
                if (!expanded) {
                    free(module_name);
                    ast_destroy((ASTNode*)use);
                    ast_destroy((ASTNode*)head);
                    return NULL;
                }
                memcpy(expanded, module_name, module_length);
                expanded[module_length] = '.';
                memcpy(expanded + module_length + 1u, segment.start, segment_length);
                module_length += 1u + segment_length;
                expanded[module_length] = '\0';
                free(module_name);
                module_name = expanded;
                use_end = segment;
            }
            use->module_name = module_name;
        }

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
            use_end = alias_token;
            use->alias = (char*)parser_malloc(p, (size_t)alias_token.length + 1u);
            if (!use->alias) {
                ast_destroy((ASTNode*)use);
                ast_destroy((ASTNode*)head);
                return NULL;
            }
            memcpy(use->alias, alias_token.start, (size_t)alias_token.length);
            use->alias[alias_token.length] = '\0';
        }

        parser_stamp_node_tokens(p, (ASTNode*)use, &module_token, &use_end);
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
            parser_stamp_node_child_to_token(p, (ASTNode*)access, expr, &p->previous);
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
            parser_stamp_node_child_to_token(p, (ASTNode*)node, expr, &member);
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
                int saw_named = 0;
                do {
                    ASTNode* arg = NULL;
                    if (p->current.type == TOKEN_IDENTIFIER && peek_token(p).type == TOKEN_COLON) {
                        Token label = p->current;
                        ASTNamedArg* named = (ASTNamedArg*)parser_ast_create(p, AST_NAMED_ARG);
                        if (!named) { ast_destroy((ASTNode*)call); return NULL; }
                        named->base.line = label.line;
                        named->base.col = label.col;
                        named->name = (char*)parser_malloc(p, (size_t)label.length + 1u);
                        if (!named->name) { ast_destroy((ASTNode*)named); ast_destroy((ASTNode*)call); return NULL; }
                        memcpy(named->name, label.start, (size_t)label.length);
                        named->name[label.length] = '\0';
                        advance(p);
                        consume(p, TOKEN_COLON, ERR_PARSER_EXPECTED_EXPRESSION, MSG_EXPECT_COLON_VAR);
                        named->value = parse_expression(p);
                        if (!named->value) { ast_destroy((ASTNode*)named); ast_destroy((ASTNode*)call); return NULL; }
                        parser_stamp_node_tokens(p, (ASTNode*)named, &label, &p->previous);
                        arg = (ASTNode*)named;
                        saw_named = 1;
                    } else {
                        if (saw_named) {
                            error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
                                "Positional arguments cannot follow named arguments.");
                            ast_destroy((ASTNode*)call);
                            return NULL;
                        }
                        arg = parse_expression(p);
                    }
                    if (!arg) { ast_destroy((ASTNode*)call); return NULL; }
                    if (arg->type == AST_NAMED_ARG) {
                        ASTNode* prior;
                        const char* label = ((ASTNamedArg*)arg)->name;
                        for (prior = call->args; prior; prior = prior->next) {
                            if (prior->type == AST_NAMED_ARG &&
                                strcmp(((ASTNamedArg*)prior)->name, label) == 0) {
                                error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN,
                                    "Duplicate named argument label.");
                                ast_destroy(arg);
                                ast_destroy((ASTNode*)call);
                                return NULL;
                            }
                        }
                    }
                    if (!call->args) call->args = arg;
                    else tail->next = arg;
                    tail = arg;
                } while (match(p, TOKEN_COMMA));
            }
            consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_ARGS);
            if (p->previous.type == TOKEN_RPAREN)
                parser_stamp_node_child_to_token(p, (ASTNode*)call, call->callee, &p->previous);
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
            parser_stamp_node_child_to_token(p, (ASTNode*)post, expr, &p->previous);
            expr = (ASTNode*)post;
        } else {
            break;
        }
    }
    return expr;
}

static ASTNode* parse_power(Parser* p) {
    ASTNode* left=parse_postfix(p);
    if(!left)return NULL;
    if(match(p,TOKEN_POWER)){
        ASTNode*right;
        if(p->nesting_depth>=PARSER_MAX_NESTING_DEPTH){
            error_at(p,&p->previous,ERR_PARSER_UNEXPECTED_TOKEN,"Maximum parser nesting depth exceeded.");
            ast_destroy(left);return NULL;
        }
        ++p->nesting_depth;right=parse_unary(p);--p->nesting_depth;
        if(!right){ast_destroy(left);return NULL;}
        return parser_make_binary(p,TOKEN_POWER,left,right);
    }
    return left;
}

static ASTNode* parse_unary(Parser* p) {
    if (match(p, TOKEN_INC) || match(p, TOKEN_DEC) ||
        match(p, TOKEN_NOT) || match(p, TOKEN_MINUS) || match(p, TOKEN_BIT_NOT)) {
        Token operator_token = p->previous;
        OmniTokenType op = operator_token.type;
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
        parser_stamp_node_tokens(p, (ASTNode*)unary, &operator_token, &p->previous);
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
    parser_stamp_node_child_range((ASTNode*)binary, left, right);
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
    ASTNode* left = parse_bitwise_or(p);
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
        ASTNode* right = parse_bitwise_or(p);
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
        ASTNode* range;
        if (!right) {
            ast_destroy(expr);
            return NULL;
        }
        range = parser_make_binary(p, TOKEN_RANGE, expr, right);
        if (!range) return NULL;
        /* Inclusive interval: a second unparenthesized '..' is invalid. */
        if (check(p, TOKEN_RANGE)) {
            error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
                "A range expression accepts only one '..' operator; use parentheses to compose ranges.");
            ast_destroy(range);
            return NULL;
        }
        return range;
    }
    return expr;
}

static ASTNode* parse_bitwise_and(Parser* p) {
    unsigned int depth=0u;ASTNode*left=parse_additive(p);if(!left)return NULL;
    while(match(p,TOKEN_BIT_AND)){
        ASTNode*right;
        if(depth++>=32u){error_at(p,&p->previous,ERR_PARSER_UNEXPECTED_TOKEN,"Maximum bitwise chain depth exceeded.");ast_destroy(left);return NULL;}
        right=parse_additive(p);if(!right){ast_destroy(left);return NULL;}
        left=parser_make_binary(p,TOKEN_BIT_AND,left,right);if(!left)return NULL;
    }return left;
}
static ASTNode* parse_bitwise_xor(Parser* p) {
    unsigned int depth=0u;ASTNode*left=parse_bitwise_and(p);if(!left)return NULL;
    while(match(p,TOKEN_BIT_XOR)){
        ASTNode*right;
        if(depth++>=32u){error_at(p,&p->previous,ERR_PARSER_UNEXPECTED_TOKEN,"Maximum bitwise chain depth exceeded.");ast_destroy(left);return NULL;}
        right=parse_bitwise_and(p);if(!right){ast_destroy(left);return NULL;}
        left=parser_make_binary(p,TOKEN_BIT_XOR,left,right);if(!left)return NULL;
    }return left;
}
static ASTNode* parse_bitwise_or(Parser* p) {
    unsigned int depth=0u;ASTNode*left=parse_bitwise_xor(p);if(!left)return NULL;
    while(match(p,TOKEN_BIT_OR)){
        ASTNode*right;
        if(depth++>=32u){error_at(p,&p->previous,ERR_PARSER_UNEXPECTED_TOKEN,"Maximum bitwise chain depth exceeded.");ast_destroy(left);return NULL;}
        right=parse_bitwise_xor(p);if(!right){ast_destroy(left);return NULL;}
        left=parser_make_binary(p,TOKEN_BIT_OR,left,right);if(!left)return NULL;
    }return left;
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

        ASTNode* right = parse_bitwise_or(p);
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
            parser_stamp_node_child_range((ASTNode*)relocate, expr, right);
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
            parser_stamp_node_child_range((ASTNode*)pipe, expr, right);
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
            parser_stamp_node_child_range((ASTNode*)flow, expr, right);
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
        parser_stamp_node_child_to_token(p, (ASTNode*)unpack, expr, &p->previous);
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
        if (!capture->expr) {
            ast_destroy((ASTNode*)capture);
            ast_destroy(expr);
            return NULL;
        }
        parser_stamp_node_child_range((ASTNode*)capture, expr, capture->expr);
        ast_destroy(expr);
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
        parser_stamp_node_child_range((ASTNode*)assign, expr, right);
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
        parser_stamp_node_child_range((ASTNode*)assign, expr, right);
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
            Token field_start = p->previous;
            ASTNode* field = parse_var_decl(p);
            if (!field) { ast_destroy(head); return 0; }
            parser_stamp_node_tokens(p, field, &field_start, &p->previous);
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
    Token array_start = p->current;
    Token array_end;
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
        array_end = p->previous;
        parser_stamp_node_tokens(p, (ASTNode*)dimension, &open, &array_end);
        if (!array_type->dimensions) array_type->dimensions = (ASTNode*)dimension;
        else tail->next = (ASTNode*)dimension;
        tail = (ASTNode*)dimension;
    }
    parser_stamp_node_tokens(p, (ASTNode*)array_type, &array_start, &array_end);
    decl->inline_type = (ASTNode*)array_type;
    return 1;
}

// Variable Declaration: v:name = expr;
static int parser_parse_type_name(Parser* p, char** out,
    int include_hard_marker, int is_hard_typed);
static int parser_parse_function_type_signature(Parser* p, char** out);

static int parser_is_type_start(Parser* p) {
    return p && (
        (p->current.type >= TOKEN_TYPE_U8 && p->current.type <= TOKEN_TYPE_ANY) ||
        p->current.type == TOKEN_IDENTIFIER ||
        p->current.type == TOKEN_STAR ||
        p->current.type == TOKEN_BIT_AND ||
        (p->current.type == TOKEN_KW_FN && p->current.length == 2 && p->current.start[1] == ':' &&
            peek_token(p).type == TOKEN_LPAREN));
}

/* Preserve pointer/reference prefixes and callback signatures in AST type-name fields. */
static int parser_parse_type_name(Parser* p, char** out,
    int include_hard_marker, int is_hard_typed) {
    char prefix[2];
    size_t prefix_length = 0u;
    size_t type_length;
    size_t total_length;
    size_t offset = 0u;
    Token type_token;
    char* type_name;

    if (!p || !out) return 0;
    *out = NULL;

    if (p->current.type == TOKEN_KW_FN && p->current.length == 2 && p->current.start[1] == ':' &&
        peek_token(p).type == TOKEN_LPAREN) {
        char* signature = NULL;
        if (!parser_parse_function_type_signature(p, &signature)) return 0;
        if (include_hard_marker && is_hard_typed) {
            size_t signature_length = strlen(signature);
            char* hard_signature = (char*)parser_malloc(p, signature_length + 2u);
            if (!hard_signature) {
                free(signature);
                return 0;
            }
            hard_signature[0] = '!';
            memcpy(hard_signature + 1u, signature, signature_length + 1u);
            free(signature);
            *out = hard_signature;
        } else {
            *out = signature;
        }
        return 1;
    }

    if (match(p, TOKEN_STAR)) {
        prefix[prefix_length++] = '*';
        if (match(p, TOKEN_STAR)) prefix[prefix_length++] = '*';
    } else if (match(p, TOKEN_BIT_AND)) {
        prefix[prefix_length++] = '&';
    }

    if (!((p->current.type >= TOKEN_TYPE_U8 &&
            p->current.type <= TOKEN_TYPE_ANY) ||
          p->current.type == TOKEN_IDENTIFIER)) {
        error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
            "Expected a type name after pointer/reference modifier.");
        return 0;
    }

    type_token = p->current;
    type_length = (size_t)type_token.length;
    if (type_length > SIZE_MAX - prefix_length - 2u) {
        parser_mark_oom(p);
        return 0;
    }
    total_length = prefix_length + type_length +
        ((include_hard_marker && is_hard_typed) ? 1u : 0u);
    type_name = (char*)parser_malloc(p, total_length + 1u);
    if (!type_name) return 0;

    if (include_hard_marker && is_hard_typed) type_name[offset++] = '!';
    if (prefix_length) {
        memcpy(type_name + offset, prefix, prefix_length);
        offset += prefix_length;
    }
    memcpy(type_name + offset, type_token.start, type_length);
    type_name[total_length] = '\0';
    advance(p);
    *out = type_name;
    return 1;
}

static ASTNode* parser_build_type_expr(
    Parser* p, const char* spelling, const Token* first, const Token* last,
    int hard_typed
);
static ASTNode* parser_build_function_type_expr(
    Parser* p, const char* spelling, const Token* first, const Token* last
);

/* Build recursive named, pointer, reference, qualified and function types. */
static ASTNode* parser_build_type_expr(
    Parser* p, const char* spelling, const Token* first, const Token* last,
    int hard_typed
) {
    size_t offset=0u;
    size_t pointer_depth=0u;
    int is_reference=0;
    ASTNode* root=NULL;
    ASTTypeExpr* named=NULL;

    if(!p || !spelling || !*spelling || !first || !last) return NULL;
    if(spelling[offset]=='!') { hard_typed=1; ++offset; }
    if(strncmp(spelling+offset,"f:(",3u)==0) {
        root=parser_build_function_type_expr(p,spelling+offset,first,last);
        if(!root) return NULL;
        goto apply_qualifier;
    }
    while(spelling[offset]=='*') { ++pointer_depth; ++offset; }
    if(spelling[offset]=='&') { is_reference=1; ++offset; }
    if(!spelling[offset]) return NULL;

    /* Parse a compact fixed/dynamic array suffix in recursive type spelling.
     * Dimensions are numeric literals or empty []; arbitrary expressions remain
     * handled by the normal declarator parser, not this signature codec. */
    {
        const char* bracket=strchr(spelling+offset,'[');
        size_t base_length=bracket?(size_t)(bracket-(spelling+offset)):strlen(spelling+offset);
        char* base_name=(char*)parser_malloc(p,base_length+1u);
        if(!base_name) return NULL;
        memcpy(base_name,spelling+offset,base_length);
        base_name[base_length]='\0';
        if(!base_length) { free(base_name); return NULL; }

        named=(ASTTypeExpr*)parser_ast_create(p,AST_TYPE_EXPR);
        if(!named) { free(base_name); return NULL; }
        named->kind=AST_TYPE_NAMED;
        named->name=base_name;
        parser_stamp_node_tokens(p,(ASTNode*)named,last,last);
        root=(ASTNode*)named;

        if(bracket) {
            ASTArrayType* array=(ASTArrayType*)parser_ast_create(p,AST_ARRAY_TYPE);
            ASTNode* dim_tail=NULL;
            const char* q=bracket;
            if(!array) { ast_destroy(root); return NULL; }
            while(*q) {
                const char* close;
                const char* d;
                ASTArrayDimension* dimension;
                ASTNode* length_node=NULL;
                long long value=0;
                if(*q!='[') { ast_destroy((ASTNode*)array); ast_destroy(root); return NULL; }
                close=strchr(q+1,']');
                if(!close) { ast_destroy((ASTNode*)array); ast_destroy(root); return NULL; }
                dimension=(ASTArrayDimension*)parser_ast_create(p,AST_ARRAY_DIMENSION);
                if(!dimension) { ast_destroy((ASTNode*)array); ast_destroy(root); return NULL; }
                d=q+1;
                if(d<close) {
                    const char* n;
                    for(n=d;n<close;++n) if(*n<'0'||*n>'9') break;
                    if(n!=close) { ast_destroy((ASTNode*)dimension); ast_destroy((ASTNode*)array); ast_destroy(root); return NULL; }
                    for(n=d;n<close;++n) {
                        if(value>(LLONG_MAX-(*n-'0'))/10) { ast_destroy((ASTNode*)dimension); ast_destroy((ASTNode*)array); ast_destroy(root); return NULL; }
                        value=value*10+(*n-'0');
                    }
                    length_node=parser_ast_create(p,AST_LITERAL);
                    if(!length_node) { ast_destroy((ASTNode*)dimension); ast_destroy((ASTNode*)array); ast_destroy(root); return NULL; }
                    ((ASTLiteral*)length_node)->lit_type=0;
                    ((ASTLiteral*)length_node)->int_value=value;
                    ((ASTLiteral*)length_node)->string_value=
                        (char*)parser_malloc(p,(size_t)(close-d)+1u);
                    if(!((ASTLiteral*)length_node)->string_value) {
                        ast_destroy(length_node);
                        ast_destroy((ASTNode*)dimension);
                        ast_destroy((ASTNode*)array);
                        ast_destroy(root);
                        return NULL;
                    }
                    memcpy(((ASTLiteral*)length_node)->string_value,d,
                        (size_t)(close-d));
                    ((ASTLiteral*)length_node)->string_value[close-d]='\0';
                    parser_stamp_node_tokens(p,length_node,first,last);
                    dimension->length=length_node;
                }
                parser_stamp_node_tokens(p,(ASTNode*)dimension,first,last);
                if(!array->dimensions) array->dimensions=(ASTNode*)dimension;
                else dim_tail->next=(ASTNode*)dimension;
                dim_tail=(ASTNode*)dimension;
                q=close+1;
            }
            parser_stamp_node_tokens(p,(ASTNode*)array,first,last);
            {
                ASTTypeExpr* wrapper=(ASTTypeExpr*)parser_ast_create(p,AST_TYPE_EXPR);
                if(!wrapper) { ast_destroy((ASTNode*)array); ast_destroy(root); return NULL; }
                wrapper->kind=AST_TYPE_ARRAY;
                wrapper->child=root;
                wrapper->dimensions=(ASTNode*)array;
                wrapper->base.line=first->line;
                wrapper->base.col=first->col;
                parser_stamp_node_tokens(p,(ASTNode*)wrapper,first,last);
                root=(ASTNode*)wrapper;
            }
        }
    }

    while(pointer_depth>0u) {
        ASTTypeExpr* wrapper=(ASTTypeExpr*)parser_ast_create(p,AST_TYPE_EXPR);
        if(!wrapper) { ast_destroy(root); return NULL; }
        wrapper->kind=AST_TYPE_POINTER;
        wrapper->child=root;
        wrapper->base.line=first->line;
        wrapper->base.col=first->col;
        parser_stamp_node_tokens(p,(ASTNode*)wrapper,first,last);
        root=(ASTNode*)wrapper;
        --pointer_depth;
    }
    if(is_reference) {
        ASTTypeExpr* wrapper=(ASTTypeExpr*)parser_ast_create(p,AST_TYPE_EXPR);
        if(!wrapper) { ast_destroy(root); return NULL; }
        wrapper->kind=AST_TYPE_REFERENCE;
        wrapper->child=root;
        wrapper->base.line=first->line;
        wrapper->base.col=first->col;
        parser_stamp_node_tokens(p,(ASTNode*)wrapper,first,last);
        root=(ASTNode*)wrapper;
    }
apply_qualifier:
    if(hard_typed) {
        ASTTypeExpr* qualifier=(ASTTypeExpr*)parser_ast_create(p,AST_TYPE_EXPR);
        if(!qualifier) { ast_destroy(root); return NULL; }
        qualifier->kind=AST_TYPE_QUALIFIED;
        qualifier->flags=AST_TYPE_FLAG_HARD_TYPED;
        qualifier->child=root;
        qualifier->base.line=first->line;
        qualifier->base.col=first->col;
        parser_stamp_node_tokens(p,(ASTNode*)qualifier,first,last);
        root=(ASTNode*)qualifier;
    }
    return root;
}

/* Wrap a parsed legacy array declarator in the recursive type tree.
 * The AST_TYPE_EXPR owns the AST_ARRAY_TYPE; declaration legacy fields are
 * borrowed aliases and are handled as such by AST lifecycle/codec/OIR code.
 */
static int parser_attach_array_type_expr(
    Parser* p, ASTNode** type_expr, ASTNode* array_type
) {
    ASTTypeExpr* wrapper;
    ASTNode* inner;
    ASTTypeExpr* qualifier = NULL;
    if (!p || !type_expr || !*type_expr || !array_type ||
        array_type->type != AST_ARRAY_TYPE) return 0;

    inner = *type_expr;
    if (inner->type == AST_TYPE_EXPR &&
        ((ASTTypeExpr*)inner)->kind == AST_TYPE_QUALIFIED) {
        qualifier = (ASTTypeExpr*)inner;
        inner = qualifier->child;
        if (!inner) return 0;
    }

    wrapper = (ASTTypeExpr*)parser_ast_create(p, AST_TYPE_EXPR);
    if (!wrapper) return 0;
    wrapper->kind = AST_TYPE_ARRAY;
    wrapper->child = inner;
    wrapper->dimensions = array_type;
    wrapper->base.line = qualifier ? qualifier->base.line : inner->line;
    wrapper->base.col = qualifier ? qualifier->base.col : inner->col;
    wrapper->base.source_span = qualifier
        ? qualifier->base.source_span : inner->source_span;
    if (array_type->source_span.has_location) {
        if (!wrapper->base.source_span.has_location) {
            wrapper->base.source_span = array_type->source_span;
        } else {
            wrapper->base.source_span.end_offset = array_type->source_span.end_offset;
            wrapper->base.source_span.end_line = array_type->source_span.end_line;
            wrapper->base.source_span.end_column = array_type->source_span.end_column;
        }
    }
    if (qualifier) {
        qualifier->child = (ASTNode*)wrapper;
        qualifier->base.source_span = wrapper->base.source_span;
    }
    else *type_expr = (ASTNode*)wrapper;
    return 1;
}

/*
 * Function type grammar:
 *   f:(name!Type, other:Type)!ReturnType
 *   Legacy compatibility: f:(name!Type, other:Type)>!ReturnType
 * The canonical signature is retained in the existing type-name string field.
 * It is distinct from an AST_LAMBDA expression/body.
 */
static ASTNode* parser_build_function_type_expr(
    Parser* p, const char* spelling, const Token* first, const Token* last
) {
    const char* open;
    const char* close;
    const char* cursor;
    const char* return_spelling;
    size_t depth=0u;
    ASTTypeExpr* function;
    ASTNode* param_head=NULL;
    ASTNode* param_tail=NULL;

    if(!p||!spelling||!first||!last||strncmp(spelling,"f:(",3u)!=0)
        return NULL;
    open=strchr(spelling,'(');
    if(!open) return NULL;
    close=open+1;
    depth=1u;
    while(*close && depth) {
        if(*close=='(') ++depth;
        else if(*close==')') --depth;
        if(depth) ++close;
    }
    if(depth!=0u || *close!=')') return NULL;
    return_spelling=close+1;
    if(!*return_spelling) return NULL;

    function=(ASTTypeExpr*)parser_ast_create(p,AST_TYPE_EXPR);
    if(!function) return NULL;
    function->kind=AST_TYPE_FUNCTION;
    function->base.line=first->line;
    function->base.col=first->col;
    parser_stamp_node_tokens(p,(ASTNode*)function,first,last);

    cursor=open+1;
    while(cursor<close) {
        const char* segment_end=cursor;
        const char* marker=NULL;
        const char* scan;
        size_t nested=0u;
        ASTVarDecl* param;
        size_t name_length;
        size_t type_length;
        const char* type_start;
        int hard=0;

        while(segment_end<close) {
            if(*segment_end=='(') ++nested;
            else if(*segment_end==')' && nested) --nested;
            else if(*segment_end==',' && nested==0u) break;
            ++segment_end;
        }
        if(segment_end==cursor) { ast_destroy((ASTNode*)function); return NULL; }
        if((size_t)(segment_end-cursor)==3u &&
           cursor[0]=='.'&&cursor[1]=='.'&&cursor[2]=='.') {
            ASTNode* variadic=parser_ast_create(p,AST_VARIADIC_PARAM);
            if(!variadic) { ast_destroy((ASTNode*)function); return NULL; }
            parser_stamp_node_tokens(p,variadic,first,last);
            if(!param_head)param_head=variadic;
            else param_tail->next=variadic;
            param_tail=variadic;
            cursor=segment_end<close?segment_end+1:close;
            continue;
        }
        for(scan=cursor;scan<segment_end;++scan) {
            if(*scan=='!'||*scan==':') { marker=scan; break; }
        }
        if(!marker || marker==cursor || marker+1>=segment_end) {
            ast_destroy(param_head);
            ast_destroy((ASTNode*)function);
            return NULL;
        }
        name_length=(size_t)(marker-cursor);
        hard=(*marker=='!');
        type_start=hard?marker:marker+1;
        type_length=(size_t)(segment_end-type_start);

        param=(ASTVarDecl*)parser_ast_create(p,AST_VAR_DECL);
        if(!param) {
            ast_destroy(param_head);
            ast_destroy((ASTNode*)function);
            return NULL;
        }
        param->name=(char*)parser_malloc(p,name_length+1u);
        param->type_name=(char*)parser_malloc(p,type_length+1u);
        if(!param->name||!param->type_name) {
            ast_destroy((ASTNode*)param);
            ast_destroy(param_head);
            ast_destroy((ASTNode*)function);
            return NULL;
        }
        memcpy(param->name,cursor,name_length);
        param->name[name_length]='\0';
        memcpy(param->type_name,type_start,type_length);
        param->type_name[type_length]='\0';
        param->is_hard_typed=hard;
        param->type_expr=parser_build_type_expr(
            p,param->type_name,first,last,hard);
        if(!param->type_expr) {
            ast_destroy((ASTNode*)param);
            ast_destroy(param_head);
            ast_destroy((ASTNode*)function);
            return NULL;
        }
        parser_stamp_node_tokens(p,(ASTNode*)param,first,last);
        if(!param_head)param_head=(ASTNode*)param;
        else param_tail->next=(ASTNode*)param;
        param_tail=(ASTNode*)param;
        cursor=segment_end<close?segment_end+1:close;
    }

    function->params=param_head;
    function->child=parser_build_type_expr(p,return_spelling,first,last,0);
    if(!function->child) {
        ast_destroy((ASTNode*)function);
        return NULL;
    }
    return (ASTNode*)function;
}

static int parser_parse_function_type_signature(Parser* p, char** out) {
    Token start;
    const char* begin;
    const char* end;
    char* parameter_type = NULL;
    char* return_type = NULL;
    size_t raw_length;
    size_t normalized_length = 0u;
    size_t i;
    char* signature;

    if (!p || !out || p->current.type != TOKEN_KW_FN ||
        p->current.length != 2 || p->current.start[1] != ':' ||
        peek_token(p).type != TOKEN_LPAREN) return 0;
    *out = NULL;
    start = p->current;
    begin = start.start;
    advance(p);
    consume(p, TOKEN_LPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_LPAREN_FUNC);

    if (!check(p, TOKEN_RPAREN)) {
        do {
            ASTVarDecl array_suffix;
            int has_colon;
            int has_bang;
            if (match(p, TOKEN_ELLIPSIS)) break;
            if (!match(p, TOKEN_IDENTIFIER)) {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                    "Expected a parameter name in function type signature.");
                return 0;
            }
            has_colon = match(p, TOKEN_COLON);
            has_bang = match(p, TOKEN_NOT);
            if (!has_colon && !has_bang) {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                    "Expected a parameter type after the parameter name.");
                return 0;
            }
            if (!parser_parse_type_name(p, &parameter_type, 1, has_bang)) return 0;
            free(parameter_type);
            parameter_type = NULL;
            memset(&array_suffix, 0, sizeof(array_suffix));
            if (!parse_array_type_suffix(p, &array_suffix)) {
                if (array_suffix.inline_type) ast_destroy(array_suffix.inline_type);
                return 0;
            }
            if (array_suffix.inline_type) ast_destroy(array_suffix.inline_type);
        } while (match(p, TOKEN_COMMA));
    }

    consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_PARAMS);
    {
        int legacy_arrow = match(p, TOKEN_GT);
        int has_bang = match(p, TOKEN_NOT);
        if (!legacy_arrow && !has_bang) {
            error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                "Expected '!ReturnType' after function type parameters.");
            return 0;
        }
        if (!parser_parse_type_name(p, &return_type, 1, has_bang)) return 0;
    }
    free(return_type);
    return_type = NULL;
    {
        ASTVarDecl array_suffix;
        memset(&array_suffix, 0, sizeof(array_suffix));
        if (!parse_array_type_suffix(p, &array_suffix)) {
            if (array_suffix.inline_type) ast_destroy(array_suffix.inline_type);
            return 0;
        }
        if (array_suffix.inline_type) ast_destroy(array_suffix.inline_type);
    }

    end = p->previous.start + p->previous.length;
    if (end < begin) return 0;
    raw_length = (size_t)(end - begin);
    signature = (char*)parser_malloc(p, raw_length + 1u);
    if (!signature) return 0;
    for (i = 0u; i < raw_length; ++i) {
        unsigned char ch = (unsigned char)begin[i];
        if (!isspace(ch) && ch != '>') signature[normalized_length++] = (char)ch;
    }
    signature[normalized_length] = '\0';
    *out = signature;
    return 1;
}

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
         } else if (parser_is_type_start(p)) {
                 Token type_start=p->previous;
                 if (!parser_parse_type_name(p, &decl->type_name, 0, 0)) {
                     ast_destroy((ASTNode*)decl);
                     return NULL;
                 }
                 if (decl->type_name && (strncmp(decl->type_name,"f:(",3u)!=0 || !strchr(decl->type_name,'[')) &&
                     !(decl->inline_type &&
                       (decl->inline_type->type==AST_STRUCT_DECL || decl->inline_type->type==AST_UNION_DECL))) {
                     decl->type_expr=parser_build_type_expr(p,decl->type_name,&type_start,&p->previous,decl->is_hard_typed);
                     if(!decl->type_expr) { ast_destroy((ASTNode*)decl); return NULL; }
                     if (decl->inline_type && decl->inline_type->type == AST_ARRAY_TYPE &&
                         !parser_attach_array_type_expr(p, &decl->type_expr, decl->inline_type)) {
                         ast_destroy((ASTNode*)decl); return NULL;
                     }
                 }
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
    Token return_start = p->previous;
    ASTUnaryExpr* ret = (ASTUnaryExpr*)parser_ast_create(p, AST_RETURN);
    if (!ret) return NULL;
    ret->base.line = return_start.line;
    ret->base.col = return_start.col;
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
    if (p->previous.type == TOKEN_SEMICOLON)
        parser_stamp_node_tokens(p, (ASTNode*)ret, &return_start, &p->previous);
    return (ASTNode*)ret;
}

static ASTNode* parse_func_decl(Parser* p) {
    Token function_start = p->previous;
    ASTNodeType function_kind = p->previous.type == TOKEN_KW_EXTERN
        ? AST_EXTERN_DECL : AST_FUNC_DECL;
    ASTFuncDecl* func = (ASTFuncDecl*)parser_ast_create(p, function_kind);
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
        func->name = (char*)parser_malloc(p, (size_t)t.length - 1u);
        if (!func->name) {
            ast_destroy((ASTNode*)func);
            return NULL;
        }
        memcpy(func->name, t.start + 2, (size_t)t.length - 2u);
        func->name[t.length - 2] = '\0';
    } else if (p->previous.type == TOKEN_KW_EXTERN) {
        Token t=p->previous;
        if(t.length<=4){ast_destroy((ASTNode*)func);return NULL;}
        func->name=(char*)parser_malloc(p,(size_t)t.length-3u);
        if(!func->name){ast_destroy((ASTNode*)func);return NULL;}
        memcpy(func->name,t.start+4,(size_t)t.length-4u);
        func->name[t.length-4u]='\0';
    } else if (match(p, TOKEN_IDENTIFIER)) {
        Token t = p->previous;
        func->name = (char*)parser_malloc(p, (size_t)t.length + 1u);
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
            int has_colon = 0;
            if (match(p, TOKEN_ELLIPSIS)) {
                ASTNode* marker = parser_ast_create(p, AST_VARIADIC_PARAM);
                if (!marker) { ast_destroy((ASTNode*)func); return NULL; }
                marker->line = p->previous.line;
                marker->col = p->previous.col;
                parser_stamp_node_token(p, marker, &p->previous);
                if (!func->params) func->params = marker;
                else param_tail->next = marker;
                /* Ellipsis is the final parameter-list marker. */
                break;
            }
            if (!match(p, TOKEN_IDENTIFIER)) {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                    msg_get(MSG_EXPECT_PARAM_NAME));
                ast_destroy((ASTNode*)func);
                return NULL;
            }
            param_token = p->previous;
            param = (ASTVarDecl*)parser_ast_create(p, AST_VAR_DECL);
            if (!param) {
                ast_destroy((ASTNode*)func);
                return NULL;
            }
            param->base.line = param_token.line;
            param->base.col = param_token.col;
            parser_stamp_node_token(p, (ASTNode*)param, &param_token);
            param->is_const = 0;
            param->name = (char*)parser_malloc(p, (size_t)param_token.length + 1u);
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

            /*
             * ':' separates a name from an ordinary type. For hard types,
             * both a!f32 and a:!f32 are accepted.
             */
            has_colon = match(p, TOKEN_COLON);
            if (match(p, TOKEN_NOT)) has_bang = 1;
            if (parser_is_type_start(p)) {
                if (!has_colon && !has_bang) {
                    error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                        msg_get(MSG_EXPECT_TYPE_BANG));
                    ast_destroy((ASTNode*)func);
                    return NULL;
                }
                 {
                     Token type_start=has_bang?p->previous:p->current;
                     if (!parser_parse_type_name(p, &param->type_name, 1, has_bang)) {
                         ast_destroy((ASTNode*)func);
                         return NULL;
                     }
                     if(param->type_name && (strncmp(param->type_name,"f:(",3u)!=0 || !strchr(param->type_name,'['))) {
                         param->type_expr=parser_build_type_expr(p,param->type_name,&type_start,&p->previous,has_bang);
                         if(!param->type_expr) { ast_destroy((ASTNode*)func); return NULL; }
                     }
                 }
                 param->is_hard_typed = has_bang;
                if (!parse_array_type_suffix(p, param)) {
                    ast_destroy((ASTNode*)func);
                    return NULL;
                }
                if (param->inline_type && param->type_expr &&
                    !parser_attach_array_type_expr(p, &param->type_expr, param->inline_type)) {
                    ast_destroy((ASTNode*)func);
                    return NULL;
                }
            } else if (has_bang && p->current.type == TOKEN_LBRACKET) {
                /* README form: a![] — element type is inferred/unspecified. */
                param->is_hard_typed = 1;
                if (!parse_array_type_suffix(p, param)) {
                    ast_destroy((ASTNode*)func);
                    return NULL;
                }
            } else if (has_colon && !has_bang) {
                param->initializer = parse_expression(p);
                if (!param->initializer) {
                    ast_destroy((ASTNode*)func);
                    return NULL;
                }
            } else if (has_colon || has_bang) {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                    msg_get(MSG_EXPECT_TYPE_BANG));
                ast_destroy((ASTNode*)func);
                return NULL;
            }
            parser_stamp_node_tokens(p, (ASTNode*)param, &param_token, &p->previous);
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
        block = (ASTBlock*)parser_ast_create(p, AST_BLOCK);
        if (!block) {
            ast_destroy(expr);
            ast_destroy((ASTNode*)func);
            return NULL;
        }
        ret = (ASTUnaryExpr*)parser_ast_create(p, AST_RETURN);
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
        if (parser_is_type_start(p)) {
                 {
                     Token return_type_start=has_bang?p->previous:p->current;
                     if (!parser_parse_type_name(p, &func->return_type, 1, has_bang)) {
                         ast_destroy((ASTNode*)func);
                         return NULL;
                     }
                     if(func->return_type && (strncmp(func->return_type,"f:(",3u)!=0 || !strchr(func->return_type,'['))) {
                         func->return_type_expr=parser_build_type_expr(p,func->return_type,&return_type_start,&p->previous,has_bang);
                         if(!func->return_type_expr) { ast_destroy((ASTNode*)func); return NULL; }
                     }
                 }
            if (p->current.type == TOKEN_LBRACKET) {
                ASTVarDecl return_array;
                memset(&return_array, 0, sizeof(return_array));
                if (!parse_array_type_suffix(p, &return_array)) {
                    ast_destroy((ASTNode*)func);
                    return NULL;
                }
                func->return_array_type = return_array.inline_type;
                return_array.inline_type = NULL;
                if (func->return_type_expr &&
                    !parser_attach_array_type_expr(p, &func->return_type_expr,
                        func->return_array_type)) {
                    ast_destroy((ASTNode*)func);
                    return NULL;
                }
            }
        } else if (has_bang) {
            error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                msg_get(MSG_EXPECT_TYPE_BANG));
            ast_destroy((ASTNode*)func);
            return NULL;
        }
    }

    if (function_kind == AST_EXTERN_DECL && p->ctx->active_linkname) {
        func->linkname = parser_strdup(p, p->ctx->active_linkname);
        if (!func->linkname) {
            ast_destroy((ASTNode*)func);
            return NULL;
        }
    }

    /*
     * EXF is a declaration, not a function definition. The semicolon form
     * exposes an external symbol without inventing a local function body.
     * Keep the legacy body form accepted for source compatibility.
     */
    if (function_kind == AST_EXTERN_DECL && match(p, TOKEN_SEMICOLON)) {
        parser_stamp_node_tokens(p, (ASTNode*)func, &function_start, &p->previous);
        return (ASTNode*)func;
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
    ASTBlock* block;
    Token block_start = p->previous;
    Token block_end;
    if (p->nesting_depth >= PARSER_MAX_NESTING_DEPTH) {
        error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
            "Maximum parser nesting depth exceeded.");
        return NULL;
    }
    ++p->nesting_depth;
    block = (ASTBlock*)parser_ast_create(p, AST_BLOCK);
    if (!block) {
        --p->nesting_depth;
        return NULL;
    }
    block->base.line = p->current.line; // { karakterinin konumu
    block->base.col = p->current.col;
    ASTNode* head = NULL;
    ASTNode* tail = NULL;

    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        Token statement_start = p->current;
        ASTNode* stmt = parse_statement(p);
        if (parser_status_is_terminal(p->ctx->status)) {
            ast_destroy(stmt);
            ast_destroy(head);
            ast_destroy((ASTNode*)block);
            --p->nesting_depth;
            return NULL;
        }
        if (stmt) {
            if (!head) head = stmt;
            else tail->next = stmt;
            tail = stmt;
        } else {
            /* A failed statement must make progress before synchronization. */
            if (p->current.type == statement_start.type &&
                p->current.source_offset == statement_start.source_offset &&
                p->current.line == statement_start.line &&
                p->current.col == statement_start.col &&
                p->current.type != TOKEN_EOF) {
                advance(p);
            }
            parser_synchronize_statement(p);
        }
    }
    consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
    block_end = p->previous;
    /* `}?;` is one TOKEN_HALT after the closing brace. */
    if (match(p, TOKEN_HALT)) {
        Token cleanup_token = p->previous;
        ASTNode* cleanup = parser_ast_create(p, AST_BLOCK_HALT);
        if (!cleanup) {
            ast_destroy(head);
            ast_destroy((ASTNode*)block);
            --p->nesting_depth;
            return NULL;
        }
        cleanup->line = cleanup_token.line;
        cleanup->col = cleanup_token.col;
        parser_stamp_node_token(p, cleanup, &cleanup_token);
        block_end = cleanup_token;
        if (!head) head = cleanup;
        else tail->next = cleanup;
        tail = cleanup;
    }
    block->statements = head;
    if ((block_start.type == TOKEN_LBRACE || block_start.type == TOKEN_THREAD_BLOCK) &&
        (block_end.type == TOKEN_RBRACE || block_end.type == TOKEN_HALT))
        parser_stamp_node_tokens(p, (ASTNode*)block, &block_start, &block_end);
    --p->nesting_depth;
    return (ASTNode*)block;
}

static ASTNode* parse_fast_exec_block(Parser* p) {
    Token fast_start = p->previous;
    Token block_end;
    ASTFastExecBlock* block;
    ASTNode* head = NULL;
    ASTNode* tail = NULL;

    if (p->nesting_depth >= PARSER_MAX_NESTING_DEPTH) {
        error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
            "Maximum parser nesting depth exceeded.");
        return NULL;
    }
    if (!check(p, TOKEN_LBRACE)) {
        error_at(p, &p->current, ERR_PARSER_EXPECTED_BRACE,
            "Expected '{' after fastexec.");
        return NULL;
    }
    advance(p); /* Consume '{'; the next token remains ordinary Nexus Flow. */
    ++p->nesting_depth;
    ++p->fast_exec_depth;
    block = (ASTFastExecBlock*)parser_ast_create(p, AST_FASTEXEC_BLOCK);
    if (!block) {
        --p->fast_exec_depth;
        --p->nesting_depth;
        return NULL;
    }
    block->base.line = fast_start.line;
    block->base.col = fast_start.col;

    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        Token statement_start = p->current;
        ASTNode* statement = parse_statement(p);
        if (parser_status_is_terminal(p->ctx->status)) {
            ast_destroy(statement);
            ast_destroy(head);
            ast_destroy((ASTNode*)block);
            --p->fast_exec_depth;
            --p->nesting_depth;
            return NULL;
        }
        if (statement) {
            if (!head) head = statement;
            else tail->next = statement;
            tail = statement;
        } else {
            if (p->current.type == statement_start.type &&
                p->current.source_offset == statement_start.source_offset &&
                p->current.line == statement_start.line &&
                p->current.col == statement_start.col &&
                p->current.type != TOKEN_EOF) {
                advance(p);
            }
            parser_synchronize_statement(p);
        }
    }

    if (check(p, TOKEN_RBRACE)) {
        advance(p);
        block_end = p->previous;
    } else {
        error_at(p, &p->current, ERR_PARSER_EXPECTED_BRACE,
            msg_get(MSG_EXPECT_RBRACE_BLOCK));
        block_end = p->previous;
    }

    if (match(p, TOKEN_HALT)) {
        Token cleanup_token = p->previous;
        ASTNode* cleanup = parser_ast_create(p, AST_BLOCK_HALT);
        if (!cleanup) {
            ast_destroy(head);
            ast_destroy((ASTNode*)block);
            --p->fast_exec_depth;
            --p->nesting_depth;
            return NULL;
        }
        cleanup->line = cleanup_token.line;
        cleanup->col = cleanup_token.col;
        parser_stamp_node_token(p, cleanup, &cleanup_token);
        block_end = cleanup_token;
        if (!head) head = cleanup;
        else tail->next = cleanup;
        tail = cleanup;
    }

    block->statements = head;
    if (block_end.type == TOKEN_RBRACE || block_end.type == TOKEN_HALT)
        parser_stamp_node_tokens(p, (ASTNode*)block, &fast_start, &block_end);
    --p->fast_exec_depth;
    --p->nesting_depth;
    return (ASTNode*)block;
}

static ASTNode* parse_asm_label(Parser* p) {
    Token asm_start = p->previous;
    Token label_token;
    Token body_token;
    Token close_token;
    ASTAsmLabel* node;
    int scope_valid = p->fast_exec_depth != 0u;
    int label_valid = 1;

    if (!scope_valid) {
        error_at(p, &asm_start, ERR_PARSER_UNEXPECTED_TOKEN,
            "Inline ASM declarations are only valid inside fastexec.");
    }
    if (!check(p, TOKEN_COLON)) {
        error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
            "Expected ':' after asm.");
        return NULL;
    }
    advance(p);
    if (!check(p, TOKEN_IDENTIFIER)) {
        error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
            "Expected an ASM label after 'asm:'.");
        return NULL;
    }
    label_token = p->current;
    if (label_token.length <= 0 || label_token.start[0] == '_') {
        error_at(p, &label_token, ERR_PARSER_UNEXPECTED_TOKEN,
            "ASM labels cannot begin with '_' or a special character.");
        label_valid = 0;
    }
    advance(p);
    if (!check(p, TOKEN_LBRACE)) {
        error_at(p, &p->current, ERR_PARSER_EXPECTED_BRACE,
            "Expected '{' before raw ASM body.");
        return NULL;
    }
    advance(p); /* Lexer emits TOKEN_ASM_BODY here, without tokenizing its bytes. */
    if (!check(p, TOKEN_ASM_BODY)) {
        if (p->current.type != TOKEN_EOF) {
            error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
                "Expected an opaque ASM body token.");
        }
        return NULL;
    }
    body_token = p->current;
    node = (ASTAsmLabel*)parser_ast_create(p, AST_ASM_LABEL);
    if (!node) return NULL;
    node->label = (char*)parser_malloc(p, (size_t)label_token.length + 1u);
    node->source_text = (char*)parser_malloc(p, body_token.source_length + 1u);
    if (!node->label || !node->source_text) {
        ast_destroy((ASTNode*)node);
        return NULL;
    }
    memcpy(node->label, label_token.start, (size_t)label_token.length);
    node->label[label_token.length] = '\0';
    if (body_token.source_length != 0u)
        memcpy(node->source_text, body_token.start, body_token.source_length);
    node->source_text[body_token.source_length] = '\0';
    node->source_length = body_token.source_length;
    node->base.line = asm_start.line;
    node->base.col = asm_start.col;

    advance(p); /* The next ordinary token must be the matching outer '}'. */
    if (!check(p, TOKEN_RBRACE)) {
        error_at(p, &p->current, ERR_PARSER_EXPECTED_BRACE,
            "Expected '}' after raw ASM body.");
        ast_destroy((ASTNode*)node);
        return NULL;
    }
    close_token = p->current;
    advance(p);
    parser_stamp_node_tokens(p, (ASTNode*)node, &asm_start, &close_token);
    if (!scope_valid || !label_valid) {
        ast_destroy((ASTNode*)node);
        return NULL;
    }
    return (ASTNode*)node;
}

static ASTNode* parse_struct_decl(Parser* p, int is_packed, char* name) {
    ASTStructDecl* decl = (ASTStructDecl*)parser_ast_create(p, AST_STRUCT_DECL);
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
                decl->name = (char*)parser_malloc(p, n + 1u);
                if (decl->name) { memcpy(decl->name, t.start + 7, n); decl->name[n] = '\0'; }
            } else if (match(p, TOKEN_IDENTIFIER)) {
                Token id = p->previous;
                decl->name = (char*)parser_malloc(p, (size_t)id.length + 1u);
                if (decl->name) { memcpy(decl->name, id.start, (size_t)id.length); decl->name[id.length] = '\0'; }
            }
        } else {
            if (p->current.type == TOKEN_COLON) advance(p);
            if (match(p, TOKEN_IDENTIFIER)) {
                Token t = p->previous;
                decl->name = (char*)parser_malloc(p, (size_t)t.length + 1u);
                if (decl->name) { memcpy(decl->name, t.start, (size_t)t.length); decl->name[t.length] = '\0'; }
            }
        }
    }
    if (!decl->name || !parse_struct_body(p, decl)) {
        if (!decl->name) error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, "Expected a struct name.");
        ast_destroy((ASTNode*)decl);
        return NULL;
    }
    return (ASTNode*)decl;
}

static ASTNode* parse_union_decl(Parser* p, char* name) {
    ASTStructDecl* decl = (ASTStructDecl*)parser_ast_create(p, AST_UNION_DECL);
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
                decl->name = (char*)parser_malloc(p, n + 1u);
                if (decl->name) { memcpy(decl->name, t.start + 6, n); decl->name[n] = '\0'; }
            } else if (match(p, TOKEN_IDENTIFIER)) {
                Token id = p->previous;
                decl->name = (char*)parser_malloc(p, (size_t)id.length + 1u);
                if (decl->name) { memcpy(decl->name, id.start, (size_t)id.length); decl->name[id.length] = '\0'; }
            }
        } else {
            if (p->current.type == TOKEN_COLON) advance(p);
            if (match(p, TOKEN_IDENTIFIER)) {
                Token t = p->previous;
                decl->name = (char*)parser_malloc(p, (size_t)t.length + 1u);
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
    ASTEnumDecl* decl = (ASTEnumDecl*)parser_ast_create(p, AST_ENUM_DECL);
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
                decl->name = (char*)parser_malloc(p, n + 1u);
                if (decl->name) { memcpy(decl->name, t.start + 5, n); decl->name[n] = '\0'; }
            } else if (match(p, TOKEN_IDENTIFIER)) {
                Token id = p->previous;
                decl->name = (char*)parser_malloc(p, (size_t)id.length + 1u);
                if (decl->name) { memcpy(decl->name, id.start, (size_t)id.length); decl->name[id.length] = '\0'; }
            }
        } else {
            if (p->current.type == TOKEN_COLON) advance(p);
            if (match(p, TOKEN_IDENTIFIER)) {
                Token t = p->previous;
                decl->name = (char*)parser_malloc(p, (size_t)t.length + 1u);
                if (decl->name) { memcpy(decl->name, t.start, (size_t)t.length); decl->name[t.length] = '\0'; }
            }
        }
    }
    if (!decl->name) { error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_FUNC_NAME)); ast_destroy((ASTNode*)decl); return NULL; }
    consume(p, TOKEN_LBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_LBRACE_FUNC);
    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        ASTEnumVariant* var;
        Token vt;
        if (!match(p, TOKEN_IDENTIFIER)) {
            if (p->current.type != TOKEN_ERROR) {
                error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
                    msg_get(ERR_PARSER_UNEXPECTED_TOKEN));
            }
            advance(p);
            continue;
        }
        vt = p->previous;
        var = (ASTEnumVariant*)parser_ast_create(p, AST_ENUM_VARIANT);
        if (!var) { ast_destroy(head); ast_destroy((ASTNode*)decl); return NULL; }
        var->name = (char*)parser_malloc(p, (size_t)vt.length + 1u);
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
                if (!match(p, TOKEN_IDENTIFIER)) {
                    error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                        "Expect field name in enum variant.");
                    ast_destroy(data_head);
                    ast_destroy(head);
                    ast_destroy((ASTNode*)decl);
                    return NULL;
                }
                ft = p->previous;
                field = (ASTVarDecl*)parser_ast_create(p, AST_VAR_DECL);
                if (!field) {
                    ast_destroy(data_head);
                    ast_destroy(head);
                    ast_destroy((ASTNode*)decl);
                    return NULL;
                }
                field->base.line = ft.line; field->base.col = ft.col; field->is_const = 1;
                parser_stamp_node_token(p, (ASTNode*)field, &ft);
                field->name = (char*)parser_malloc(p, (size_t)ft.length + 1u);
                if (!field->name) { ast_destroy((ASTNode*)field); ast_destroy(data_head); ast_destroy(head); ast_destroy((ASTNode*)decl); return NULL; }
                memcpy(field->name, ft.start, (size_t)ft.length); field->name[ft.length] = '\0';
                if (!match(p, TOKEN_NOT)) { error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN, msg_get(MSG_EXPECT_TYPE_BANG)); ast_destroy((ASTNode*)field); ast_destroy(data_head); ast_destroy(head); ast_destroy((ASTNode*)decl); return NULL; }
                if ((p->current.type >= TOKEN_TYPE_U8 && p->current.type <= TOKEN_TYPE_ANY) || p->current.type == TOKEN_IDENTIFIER) {
                    Token tt = p->current;
                    field->type_name = (char*)parser_malloc(p, (size_t)tt.length + 1u);
                    if (!field->type_name) { ast_destroy((ASTNode*)field); ast_destroy(data_head); ast_destroy(head); ast_destroy((ASTNode*)decl); return NULL; }
                    memcpy(field->type_name, tt.start, (size_t)tt.length); field->type_name[tt.length] = '\0';
                    field->is_hard_typed = 1; advance(p);
                    parser_stamp_node_tokens(p, (ASTNode*)field, &ft, &p->previous);
                } else { error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_RET_TYPE)); ast_destroy((ASTNode*)field); ast_destroy(data_head); ast_destroy(head); ast_destroy((ASTNode*)decl); return NULL; }
                if (!data_head) data_head = (ASTNode*)field; else data_tail->next = (ASTNode*)field;
                data_tail = (ASTNode*)field;
            } while (match(p, TOKEN_COMMA));
            /* Transfer the payload list to the variant only after it is complete.
             * Error paths above still own data_head separately and also destroy head. */
            var->data = data_head;
            consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_PARAMS);
        }
        parser_stamp_node_tokens(p, (ASTNode*)var, &vt, &p->previous);
        if (match(p, TOKEN_COMMA)) {}
    }
    decl->variants = head;
    consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
    return (ASTNode*)decl;
}

static ASTNode* parse_newtype_decl(Parser* p) {
    Token t = p->previous;
    char* raw;
    char* name_part;
    char* decl_name = NULL;
    if (t.length < 3) {
        error_at(p, &p->previous, ERR_PARSER_INVALID_VAR, msg_get(ERR_PARSER_INVALID_VAR));
        return NULL;
    }
    raw = (char*)parser_malloc(p, (size_t)t.length + 1u);
    if (!raw) return NULL;
    memcpy(raw, t.start, (size_t)t.length);
    raw[t.length] = '\0';
    name_part = raw + 3;

    if (strncmp(name_part, "struct:", 7) == 0 ||
        strncmp(name_part, "enum:", 5) == 0 ||
        strncmp(name_part, "union:", 6) == 0) {
        int kind = strncmp(name_part, "struct:", 7) == 0 ? 1 :
            (strncmp(name_part, "enum:", 5) == 0 ? 2 : 3);
        size_t prefix = kind == 1 ? 7u : (kind == 2 ? 5u : 6u);
        size_t n = strlen(name_part + prefix);
        decl_name = (char*)parser_malloc(p, n + 1u);
        if (!decl_name) { free(raw); return NULL; }
        memcpy(decl_name, name_part + prefix, n + 1u);
        free(raw);
        if (kind == 1) return parse_struct_decl(p, 0, decl_name);
        if (kind == 2) return parse_enum_decl(p, decl_name);
        return parse_union_decl(p, decl_name);
    }

    {
        int kind = strcmp(name_part, "struct") == 0 ? 1 :
            (strcmp(name_part, "enum") == 0 ? 2 :
            (strcmp(name_part, "union") == 0 ? 3 : 0));
        int separated = *name_part == '\0';
        if (separated) {
            if (p->current.type == TOKEN_KW_STRUCT || p->current.type == TOKEN_PRE_STRUCT) kind = 1;
            else if (p->current.type == TOKEN_KW_ENUM || p->current.type == TOKEN_PRE_ENUM) kind = 2;
            else if (p->current.type == TOKEN_KW_UNION || p->current.type == TOKEN_PRE_UNION) kind = 3;
        }
        if (kind != 0) {
            free(raw);
            raw = NULL;
            if (separated) {
                Token kt = p->current;
                size_t prefix = kt.type == TOKEN_PRE_STRUCT ? 7u :
                    (kt.type == TOKEN_PRE_ENUM ? 5u : (kt.type == TOKEN_PRE_UNION ? 6u : 0u));
                advance(p);
                if (prefix > 0u && kt.length > (int)prefix) {
                    size_t n = (size_t)kt.length - prefix;
                    decl_name = (char*)parser_malloc(p, n + 1u);
                    if (!decl_name) return NULL;
                    memcpy(decl_name, kt.start + prefix, n);
                    decl_name[n] = '\0';
                    if (kind == 1) return parse_struct_decl(p, 0, decl_name);
                    if (kind == 2) return parse_enum_decl(p, decl_name);
                    return parse_union_decl(p, decl_name);
                }
            }
            if (p->current.type == TOKEN_LBRACE) {
                if (kind == 1 || kind == 3) {
                    ASTStructDecl* aggregate = (ASTStructDecl*)parser_ast_create(p, kind == 1 ? AST_STRUCT_DECL : AST_UNION_DECL);
                    ASTNode* head = NULL;
                    ASTNode* tail = NULL;
                    if (!aggregate) return NULL;
                    aggregate->base.line = p->previous.line;
                    aggregate->base.col = p->previous.col;
                    aggregate->is_packed = 0;
                    aggregate->is_union = (kind == 3);
                    consume(p, TOKEN_LBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_LBRACE_FUNC);
                    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
                        if (match(p, TOKEN_PRE_VAR)) {
                            Token field_start = p->previous;
                            ASTNode* field = parse_var_decl(p);
                            if (!field) { ast_destroy(head); ast_destroy((ASTNode*)aggregate); return NULL; }
                            parser_stamp_node_tokens(p, field, &field_start, &p->previous);
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
                    aggregate->fields = head;
                    consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
                    if (!match(p, TOKEN_IDENTIFIER)) {
                        error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_FUNC_NAME));
                        ast_destroy((ASTNode*)aggregate);
                        return NULL;
                    }
                    t = p->previous;
                    aggregate->name = (char*)parser_malloc(p, (size_t)t.length + 1u);
                    if (!aggregate->name) { ast_destroy((ASTNode*)aggregate); return NULL; }
                    memcpy(aggregate->name, t.start, (size_t)t.length);
                    aggregate->name[t.length] = '\0';
                    consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
                    return (ASTNode*)aggregate;
                } else {
                    ASTEnumDecl* aggregate = (ASTEnumDecl*)parser_ast_create(p, AST_ENUM_DECL);
                    ASTNode* head = NULL;
                    ASTNode* tail = NULL;
                    if (!aggregate) return NULL;
                    aggregate->base.line = p->previous.line;
                    consume(p, TOKEN_LBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_LBRACE_FUNC);
                    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
                        if (match(p, TOKEN_IDENTIFIER)) {
                            ASTEnumVariant* variant = (ASTEnumVariant*)parser_ast_create(p, AST_ENUM_VARIANT);
                            if (!variant) { ast_destroy(head); ast_destroy((ASTNode*)aggregate); return NULL; }
                            t = p->previous;
                            variant->base.line = t.line;
                            variant->base.col = t.col;
                            variant->name = (char*)parser_malloc(p, (size_t)t.length + 1u);
                            if (!variant->name) { ast_destroy((ASTNode*)variant); ast_destroy(head); ast_destroy((ASTNode*)aggregate); return NULL; }
                            memcpy(variant->name, t.start, (size_t)t.length);
                            variant->name[t.length] = '\0';
                            if (match(p, TOKEN_LPAREN)) {
                                while (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) advance(p);
                                consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_PARAMS);
                            }
                            parser_stamp_node_tokens(p, (ASTNode*)variant, &t, &p->previous);
                            if (!head) head = (ASTNode*)variant; else tail->next = (ASTNode*)variant;
                            tail = (ASTNode*)variant;
                            if (match(p, TOKEN_COMMA)) {}
                        } else {
                            if (p->current.type != TOKEN_ERROR) {
                                error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
                                    msg_get(ERR_PARSER_UNEXPECTED_TOKEN));
                            }
                            advance(p);
                        }
                    }
                    aggregate->variants = head;
                    consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
                    if (!match(p, TOKEN_IDENTIFIER)) {
                        error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_FUNC_NAME));
                        ast_destroy((ASTNode*)aggregate);
                        return NULL;
                    }
                    t = p->previous;
                    aggregate->name = (char*)parser_malloc(p, (size_t)t.length + 1u);
                    if (!aggregate->name) { ast_destroy((ASTNode*)aggregate); return NULL; }
                    memcpy(aggregate->name, t.start, (size_t)t.length);
                    aggregate->name[t.length] = '\0';
                    consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
                    return (ASTNode*)aggregate;
                }
            }
            if (p->current.type == TOKEN_COLON) advance(p);
            if (match(p, TOKEN_IDENTIFIER)) {
                t = p->previous;
                decl_name = (char*)parser_malloc(p, (size_t)t.length + 1u);
                if (!decl_name) return NULL;
                memcpy(decl_name, t.start, (size_t)t.length);
                decl_name[t.length] = '\0';
            }
            if (kind == 1) return parse_struct_decl(p, 0, decl_name);
            if (kind == 2) return parse_enum_decl(p, decl_name);
            return parse_union_decl(p, decl_name);
        }
        if (separated) {
            free(raw);
            error_at(p, &p->previous, ERR_PARSER_INVALID_VAR, msg_get(ERR_PARSER_INVALID_VAR));
            return NULL;
        }
    }

    {
        ASTNewTypeDecl* decl = (ASTNewTypeDecl*)parser_ast_create(p, AST_NEWTYPE_DECL);
        size_t name_length = strlen(name_part);
        if (!decl) { free(raw); return NULL; }
        decl->name = (char*)parser_malloc(p, name_length + 1u);
        if (!decl->name) { free(raw); ast_destroy((ASTNode*)decl); return NULL; }
        memcpy(decl->name, name_part, name_length + 1u);
        free(raw);
        consume(p, TOKEN_ASSIGN, ERR_PARSER_UNEXPECTED_TOKEN, MSG_UNEXPECTED_CHAR);
        if (parser_is_type_start(p)) {
                 {
                     Token target_type_start=p->current;
                     if (!parser_parse_type_name(p, &decl->target_type, 0, 0)) {
                         ast_destroy((ASTNode*)decl);
                         return NULL;
                     }
                     if(decl->target_type) {
                         decl->target_type_expr=parser_build_type_expr(p,decl->target_type,&target_type_start,&p->previous,0);
                         if(!decl->target_type_expr) { ast_destroy((ASTNode*)decl); return NULL; }
                     }
                 }
        } else {
            error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_RET_TYPE));
            ast_destroy((ASTNode*)decl);
            return NULL;
        }
        consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
        return (ASTNode*)decl;
    }
}

static ASTNode* parse_group_body(Parser* p, ASTGroupDecl* group) {
    ASTNode* head = NULL;
    ASTNode* tail = NULL;
    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        Token member_start = p->current;
        ASTNode* member = NULL;

        /* Nested group alias: child => group { ... } */
        if (p->current.type == TOKEN_IDENTIFIER && peek_token(p).type == TOKEN_FAT_ARROW &&
            peek_token_ahead(p, 2).type == TOKEN_KW_GROUP) {
            Token alias = p->current;
            ASTGroupDecl* child = (ASTGroupDecl*)parser_ast_create(p, AST_GROUP_DECL);
            if (!child) { ast_destroy(head); return NULL; }
            child->base.line = alias.line; child->base.col = alias.col;
            child->name = (char*)parser_malloc(p, (size_t)alias.length + 1u);
            if (!child->name) { ast_destroy((ASTNode*)child); ast_destroy(head); return NULL; }
            memcpy(child->name, alias.start, (size_t)alias.length); child->name[alias.length] = '\0';
            advance(p); advance(p); advance(p); /* alias => group */
            if (!match(p, TOKEN_LBRACE)) {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_BRACE, msg_get(MSG_EXPECT_LBRACE_FUNC));
                ast_destroy((ASTNode*)child); ast_destroy(head); return NULL;
            }
            (void)parse_group_body(p, child);
            if (p->ctx->status == PARSER_STATUS_OUT_OF_MEMORY) { ast_destroy((ASTNode*)child); ast_destroy(head); return NULL; }
            consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
            member = (ASTNode*)child;
        } else if (p->current.type == TOKEN_IDENTIFIER &&
                   parser_identifier_is(&p->current, "default") &&
                   peek_token(p).type == TOKEN_LBRACE) {
            /* Default dispatch is represented as METHOD("default", BLOCK). */
            ASTMethod* method = (ASTMethod*)parser_ast_create(p, AST_METHOD);
            if (!method) { ast_destroy(head); return NULL; }
            method->base.line = p->current.line; method->base.col = p->current.col;
            method->public_name = parser_strdup(p, "default");
            advance(p); advance(p); /* default { */
            method->implementation = parse_block(p);
            if (!method->public_name || !method->implementation) {
                ast_destroy((ASTNode*)method); ast_destroy(head); return NULL;
            }
            member = (ASTNode*)method;
        } else if (p->current.type == TOKEN_IDENTIFIER && peek_token(p).type == TOKEN_FAT_ARROW) {
            ASTMethod* method = (ASTMethod*)parser_ast_create(p, AST_METHOD);
            Token mt = p->current;
            if (!method) { ast_destroy(head); return NULL; }
            method->base.line = mt.line; method->base.col = mt.col;
            method->public_name = (char*)parser_malloc(p, (size_t)mt.length + 1u);
            if (!method->public_name) { ast_destroy((ASTNode*)method); ast_destroy(head); return NULL; }
            memcpy(method->public_name, mt.start, (size_t)mt.length); method->public_name[mt.length] = '\0';
            advance(p); advance(p);
            if (match(p, TOKEN_KW_FN) || match(p, TOKEN_PRE_FUNC)) {
                Token implementation_start = p->previous;
                method->implementation = parse_func_decl(p);
                if (!method->implementation) { ast_destroy((ASTNode*)method); ast_destroy(head); return NULL; }
                parser_stamp_node_tokens(p, method->implementation, &implementation_start, &p->previous);
            } else {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, "Expected function after '=>'");
                ast_destroy((ASTNode*)method); ast_destroy(head); return NULL;
            }
            member = (ASTNode*)method;
            (void)match(p, TOKEN_SEMICOLON);
        }

        if (!member) {
            if (match(p, TOKEN_KW_STRUCT) || match(p, TOKEN_PRE_STRUCT)) member = parse_struct_decl(p, 0, NULL);
            else if (match(p, TOKEN_NOT)) {
                if (match(p, TOKEN_KW_STRUCT) || match(p, TOKEN_PRE_STRUCT)) member = parse_struct_decl(p, 1, NULL);
                else { error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN, msg_get(MSG_EXPECT_TYPE_BANG)); ast_destroy(head); return NULL; }
            } else if (match(p, TOKEN_KW_ENUM) || match(p, TOKEN_PRE_ENUM)) member = parse_enum_decl(p, NULL);
            else if (match(p, TOKEN_KW_UNION) || match(p, TOKEN_PRE_UNION)) member = parse_union_decl(p, NULL);
            else if (match(p, TOKEN_PRE_VAR) || match(p, TOKEN_PRE_CONST)) {
                member = parse_var_decl(p);
                if (member) consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
            } else if (match(p, TOKEN_KW_FN) || match(p, TOKEN_PRE_FUNC)) member = parse_func_decl(p);
            else {
                if (p->current.type != TOKEN_ERROR)
                    error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN, msg_get(ERR_PARSER_UNEXPECTED_TOKEN));
                advance(p);
                continue;
            }
        }
        if (!member) { ast_destroy(head); return NULL; }
        parser_stamp_node_tokens(p, member, &member_start, &p->previous);
        if (!head) head = member; else tail->next = member;
        tail = member;
    }
    group->members = head;
    return (ASTNode*)group;
}

static ASTNode* parse_group_decl(Parser* p) {
    ASTGroupDecl* group = (ASTGroupDecl*)parser_ast_create(p, AST_GROUP_DECL);
    if (!group) return NULL;
    group->base.line = p->previous.line; group->base.col = p->previous.col;
    if (match(p, TOKEN_IDENTIFIER)) {
        Token t = p->previous;
        group->name = (char*)parser_malloc(p, (size_t)t.length + 1u);
        if (!group->name) { ast_destroy((ASTNode*)group); return NULL; }
        memcpy(group->name, t.start, (size_t)t.length); group->name[t.length] = '\0';
    }
    if (!match(p, TOKEN_LBRACE)) {
        error_at(p, &p->current, ERR_PARSER_EXPECTED_BRACE, msg_get(MSG_EXPECT_LBRACE_FUNC));
        ast_destroy((ASTNode*)group); return NULL;
    }
    (void)parse_group_body(p, group);
    if (p->ctx->status == PARSER_STATUS_OUT_OF_MEMORY) { ast_destroy((ASTNode*)group); return NULL; }
    consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
    return (ASTNode*)group;
}
static ASTNode* parse_if_branch(Parser* p) {
    if (match(p, TOKEN_LBRACE)) return parse_block(p);
    return parse_statement(p);
}

static ASTNode* parse_conditional_action(Parser* p) {
    Token opening = p->current;
    ASTNode* condition;
    ASTIf* node;

    advance(p); /* '(' */
    condition = parse_expression(p);
    if (!condition) return NULL;
    if (!check(p, TOKEN_RPAREN)) {
        error_at(p, &p->current, ERR_PARSER_EXPECTED_PAREN,
            msg_get(MSG_EXPECT_RPAREN_EXPR));
        ast_destroy(condition);
        return NULL;
    }
    advance(p); /* ')' */
    if (!match(p, TOKEN_PIPE)) {
        ast_destroy(condition);
        return NULL; /* Lookahead made this unreachable unless input changed. */
    }

    node = (ASTIf*)parser_ast_create(p, AST_IF);
    if (!node) {
        ast_destroy(condition);
        return NULL;
    }
    node->base.line = opening.line;
    node->base.col = opening.col;
    node->condition = condition;
    node->then_branch = parse_statement(p);
    node->else_branch = NULL;
    if (!node->then_branch) {
        ast_destroy((ASTNode*)node);
        return NULL;
    }
    parser_stamp_node_tokens(p, (ASTNode*)node, &opening, &p->previous);
    return (ASTNode*)node;
}

static ASTNode* parse_if_statement(Parser* p) {
    ASTIf* node;
    Token keyword = p->current;

    if (p->current.type != TOKEN_KW_IF &&
        p->current.type != TOKEN_KW_ELSIF) {
        error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
            "Expected 'if' or 'elsif'.");
        return NULL;
    }
    advance(p);

    node = (ASTIf*)parser_ast_create(p, AST_IF);
    if (!node) return NULL;
    node->base.line = keyword.line;
    node->base.col = keyword.col;

    if (!check(p, TOKEN_LPAREN)) {
        error_at(p, &p->current, ERR_PARSER_EXPECTED_PAREN,
            msg_get(MSG_EXPECT_LPAREN_FUNC));
        ast_destroy((ASTNode*)node);
        return NULL;
    }
    advance(p);
    node->condition = parse_expression(p);
    if (!node->condition || !check(p, TOKEN_RPAREN)) {
        if (node->condition) {
            error_at(p, &p->current, ERR_PARSER_EXPECTED_PAREN,
                msg_get(MSG_EXPECT_RPAREN_EXPR));
        }
        ast_destroy((ASTNode*)node);
        return NULL;
    }
    advance(p);

    node->then_branch = parse_if_branch(p);
    if (!node->then_branch) {
        ast_destroy((ASTNode*)node);
        return NULL;
    }

    if (check(p, TOKEN_KW_ELSIF)) {
        node->else_branch = parse_if_statement(p);
    } else if (match(p, TOKEN_KW_ELS)) {
        node->else_branch = parse_if_branch(p);
    }
    if ((check(p, TOKEN_KW_ELSIF) || check(p, TOKEN_KW_ELS)) &&
        !node->else_branch) {
        ast_destroy((ASTNode*)node);
        return NULL;
    }
    parser_stamp_node_tokens(p, (ASTNode*)node, &keyword, &p->previous);
    return (ASTNode*)node;
}

static int parser_identifier_is(const Token* token, const char* word) {
    size_t n;
    if (!token || !word || token->type != TOKEN_IDENTIFIER) return 0;
    n = strlen(word);
    return token->length == n && memcmp(token->start, word, n) == 0;
}

static ASTNode* parse_pattern(Parser* p) {
    Token token = p->current;
    if (token.type == TOKEN_IDENTIFIER) {
        ASTNode* pattern;
        advance(p);
        if (match(p, TOKEN_LPAREN)) {
            ASTPatternConstructor* ctor = (ASTPatternConstructor*)parser_ast_create(p, AST_PATTERN_CONSTRUCTOR);
            ASTNode* head = NULL;
            ASTNode* tail = NULL;
            if (!ctor) return NULL;
            ctor->base.line = token.line; ctor->base.col = token.col;
            ctor->name = (char*)parser_malloc(p, (size_t)token.length + 1u);
            if (!ctor->name) { ast_destroy((ASTNode*)ctor); return NULL; }
            memcpy(ctor->name, token.start, (size_t)token.length);
            ctor->name[token.length] = '\0';
            if (check(p, TOKEN_RPAREN)) {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                    "Constructor patterns require at least one binding.");
                ast_destroy((ASTNode*)ctor);
                return NULL;
            }
            do {
                Token binding_token = p->current;
                ASTPatternBinding* binding;
                if (!check(p, TOKEN_IDENTIFIER)) {
                    error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                        "Expected a pattern binding name.");
                    ast_destroy((ASTNode*)ctor); ast_destroy(head); return NULL;
                }
                advance(p);
                binding = (ASTPatternBinding*)parser_ast_create(p, AST_PATTERN_BINDING);
                if (!binding) { ast_destroy((ASTNode*)ctor); ast_destroy(head); return NULL; }
                binding->base.line = binding_token.line; binding->base.col = binding_token.col;
                binding->name = (char*)parser_malloc(p, (size_t)binding_token.length + 1u);
                if (!binding->name) { ast_destroy((ASTNode*)binding); ast_destroy((ASTNode*)ctor); ast_destroy(head); return NULL; }
                memcpy(binding->name, binding_token.start, (size_t)binding_token.length);
                binding->name[binding_token.length] = '\0';
                parser_stamp_node_token(p, (ASTNode*)binding, &binding_token);
                if (!head) head = (ASTNode*)binding; else tail->next = (ASTNode*)binding;
                tail = (ASTNode*)binding;
            } while (match(p, TOKEN_COMMA));
            if (!check(p, TOKEN_RPAREN)) {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_PAREN,
                    msg_get(MSG_EXPECT_RPAREN_EXPR));
                ast_destroy((ASTNode*)ctor); ast_destroy(head); return NULL;
            }
            advance(p);
            ctor->bindings = head;
            parser_stamp_node_tokens(p, (ASTNode*)ctor, &token, &p->previous);
            return (ASTNode*)ctor;
        }
        pattern = parser_ast_create(p, AST_PATTERN_BINDING);
        if (!pattern) return NULL;
        ((ASTPatternBinding*)pattern)->base.line = token.line;
        ((ASTPatternBinding*)pattern)->base.col = token.col;
        ((ASTPatternBinding*)pattern)->name = (char*)parser_malloc(p, (size_t)token.length + 1u);
        if (!((ASTPatternBinding*)pattern)->name) { ast_destroy(pattern); return NULL; }
        memcpy(((ASTPatternBinding*)pattern)->name, token.start, (size_t)token.length);
        ((ASTPatternBinding*)pattern)->name[token.length] = '\0';
        parser_stamp_node_token(p, pattern, &token);
        return pattern;
    }
    if (token.type == TOKEN_NUMBER || token.type == TOKEN_STRING ||
        token.type == TOKEN_CHAR || token.type == TOKEN_TRUE || token.type == TOKEN_FALSE || token.type == TOKEN_NULL) {
        return parse_primary(p);
    }
    error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
        "Expected a literal, binding, or constructor pattern.");
    return NULL;
}

static ASTNode* parse_select_scan(Parser* p, int is_scan) {
    Token keyword = p->current;
    ASTNode* root = NULL;
    ASTNode* arms = NULL;
    ASTNode* tail = NULL;
    int seen_default = 0;
    int seen_scan_else = 0;
    advance(p); /* contextual identifier: select / scan */
    if (!check(p, TOKEN_LPAREN)) {
        error_at(p, &p->current, ERR_PARSER_EXPECTED_PAREN, msg_get(MSG_EXPECT_LPAREN_FUNC));
        return NULL;
    }
    advance(p);
    if (is_scan) {
        ASTScan* n=(ASTScan*)parser_ast_create(p,AST_SCAN);
        if(!n)return NULL;
        n->base.line=keyword.line; n->base.col=keyword.col; root=(ASTNode*)n;
        n->subject=parse_expression(p);
        if(!n->subject){ast_destroy(root);return NULL;}
    } else {
        ASTSelect* n=(ASTSelect*)parser_ast_create(p,AST_SELECT);
        if(!n)return NULL;
        n->base.line=keyword.line; n->base.col=keyword.col; root=(ASTNode*)n;
        n->selector=parse_expression(p);
        if(!n->selector){ast_destroy(root);return NULL;}
    }
    if (!check(p,TOKEN_RPAREN)) {
        error_at(p,&p->current,ERR_PARSER_EXPECTED_PAREN,msg_get(MSG_EXPECT_RPAREN_EXPR));
        ast_destroy(root); return NULL;
    }
    advance(p);
    if (!check(p,TOKEN_LBRACE)) {
        error_at(p,&p->current,ERR_PARSER_EXPECTED_BRACE,msg_get(MSG_EXPECT_LBRACE_FUNC));
        ast_destroy(root); return NULL;
    }
    advance(p);
    while (p->current.type!=TOKEN_RBRACE && p->current.type!=TOKEN_EOF) {
        Token arm_start = p->current;
        ASTNode* arm=NULL;
        ASTNode* pattern=NULL;
        ASTNode* guard=NULL;
        ASTNode* body=NULL;
        int is_default=0;
        int is_scan_else=0;
        if (is_scan && seen_scan_else) {
            error_at(p,&p->current,ERR_PARSER_UNEXPECTED_TOKEN,
                "A scan else arm must be the final arm.");
            ast_destroy(root); ast_destroy(arms); return NULL;
        }
        if (!is_scan && parser_identifier_is(&p->current,"default")) {
            is_default=1; advance(p);
            if(seen_default) {
                error_at(p,&p->previous,ERR_PARSER_UNEXPECTED_TOKEN,"select may contain only one default arm.");
                ast_destroy(root); ast_destroy(arms); return NULL;
            }
            seen_default=1;
        } else if (is_scan && parser_identifier_is(&p->current,"else")) {
            is_scan_else=1;
            seen_scan_else=1;
            advance(p);
        } else {
            pattern=parse_pattern(p);
            if(!pattern){ast_destroy(root);ast_destroy(arms);return NULL;}
            if(parser_identifier_is(&p->current,"when")) {
                advance(p);
                guard=parse_logical_or(p);
                if(!guard) {
                    ast_destroy(pattern); ast_destroy(root); ast_destroy(arms); return NULL;
                }
            }
        }
        if((is_default || is_scan_else) && parser_identifier_is(&p->current,"when")) {
            error_at(p,&p->current,ERR_PARSER_UNEXPECTED_TOKEN,
                "A default/else arm cannot have a when guard.");
            ast_destroy(pattern); ast_destroy(guard); ast_destroy(root); ast_destroy(arms);
            return NULL;
        }
        if(!check(p,TOKEN_PIPE)) {
            error_at(p,&p->current,ERR_PARSER_UNEXPECTED_TOKEN,"Expected '->' after the pattern.");
            ast_destroy(pattern); ast_destroy(guard); ast_destroy(root); ast_destroy(arms); return NULL;
        }
        advance(p);
        if(check(p,TOKEN_LBRACE)){advance(p);body=parse_block(p);}
        else body=parse_statement(p);
        if(!body){ast_destroy(pattern);ast_destroy(guard);ast_destroy(root);ast_destroy(arms);return NULL;}
        if(guard) {
            ASTIf* guarded_body=(ASTIf*)parser_ast_create(p,AST_IF);
            if(!guarded_body) {
                ast_destroy(pattern); ast_destroy(guard); ast_destroy(body);
                ast_destroy(root); ast_destroy(arms); return NULL;
            }
            guarded_body->base.line=guard->line;
            guarded_body->base.col=guard->col;
            guarded_body->condition=guard;
            guarded_body->then_branch=body;
            parser_stamp_node_child_range((ASTNode*)guarded_body, guard, body);
            body=(ASTNode*)guarded_body;
        }
        if(is_scan) {
            ASTScanArm* n=(ASTScanArm*)parser_ast_create(p,AST_SCAN_ARM);
            if(!n){ast_destroy(pattern);ast_destroy(body);ast_destroy(root);ast_destroy(arms);return NULL;}
            n->base.line=keyword.line; n->base.col=keyword.col; n->pattern=pattern; n->body=body; arm=(ASTNode*)n;
        } else {
            ASTSelectArm* n=(ASTSelectArm*)parser_ast_create(p,AST_SELECT_ARM);
            if(!n){ast_destroy(pattern);ast_destroy(body);ast_destroy(root);ast_destroy(arms);return NULL;}
            n->base.line=keyword.line; n->base.col=keyword.col; n->pattern=pattern; n->body=body; n->is_default=is_default; arm=(ASTNode*)n;
        }
        parser_stamp_node_tokens(p, arm, &arm_start, &p->previous);
        if(!arms)arms=arm;else tail->next=arm;tail=arm;
        if(check(p,TOKEN_COMMA)||check(p,TOKEN_SEMICOLON))advance(p);
    }
    if(!check(p,TOKEN_RBRACE)) {
        error_at(p,&p->current,ERR_PARSER_EXPECTED_BRACE,msg_get(MSG_EXPECT_RBRACE_BLOCK));
        ast_destroy(root); ast_destroy(arms); return NULL;
    }
    advance(p);
    if(is_scan)((ASTScan*)root)->arms=arms;else ((ASTSelect*)root)->arms=arms;
    parser_stamp_node_tokens(p, root, &keyword, &p->previous);
    return root;
}

static ASTNode* parse_simple_parameter_list(Parser* p) {
    ASTNode* head=NULL; ASTNode* tail=NULL;
    if(check(p,TOKEN_RPAREN)) return NULL;
    do {
        Token t=p->current;
        ASTVarDecl* param;
        if(!check(p,TOKEN_IDENTIFIER)) {
            error_at(p,&p->current,ERR_PARSER_EXPECTED_EXPRESSION,msg_get(MSG_EXPECT_PARAM_NAME));
            ast_destroy(head); return NULL;
        }
        advance(p);
        param=(ASTVarDecl*)parser_ast_create(p,AST_VAR_DECL);
        if(!param){ast_destroy(head);return NULL;}
        param->base.line=t.line; param->base.col=t.col;
        parser_stamp_node_token(p, (ASTNode*)param, &t);
        param->name=(char*)parser_malloc(p,(size_t)t.length+1u);
        if(!param->name){ast_destroy((ASTNode*)param);ast_destroy(head);return NULL;}
        memcpy(param->name,t.start,(size_t)t.length); param->name[t.length]='\0';
        if(!head)head=(ASTNode*)param;else tail->next=(ASTNode*)param;
        tail=(ASTNode*)param;
    } while(match(p,TOKEN_COMMA));
    return head;
}

static ASTNode* parse_ruleset(Parser* p) {
    Token keyword=p->current, name_token;
    ASTRulesetDecl* node;
    ASTNode* rules=NULL; ASTNode* tail=NULL;
    int seen_default=0;
    advance(p);
    if(match(p,TOKEN_LBRACKET)) {
        if(!match(p,TOKEN_IDENTIFIER)){error_at(p,&p->current,ERR_PARSER_EXPECTED_EXPRESSION,"Expected ruleset name.");return NULL;}
        name_token=p->previous;
        consume(p,TOKEN_RBRACKET,ERR_PARSER_EXPECTED_BRACE,MSG_EXPECT_RBRACE_BLOCK);
    } else if(match(p,TOKEN_IDENTIFIER)) name_token=p->previous;
    else {error_at(p,&p->current,ERR_PARSER_EXPECTED_EXPRESSION,"Expected ruleset name.");return NULL;}
    node=(ASTRulesetDecl*)parser_ast_create(p,AST_RULESET_DECL);
    if(!node)return NULL;
    node->base.line=keyword.line; node->base.col=keyword.col;
    node->name=(char*)parser_malloc(p,(size_t)name_token.length+1u);
    if(!node->name){ast_destroy((ASTNode*)node);return NULL;}
    memcpy(node->name,name_token.start,(size_t)name_token.length);node->name[name_token.length]='\0';
    if(!check(p,TOKEN_LPAREN)){error_at(p,&p->current,ERR_PARSER_EXPECTED_PAREN,msg_get(MSG_EXPECT_LPAREN_FUNC));ast_destroy((ASTNode*)node);return NULL;}
    advance(p); node->params=parse_simple_parameter_list(p);
    if(!check(p,TOKEN_RPAREN)){error_at(p,&p->current,ERR_PARSER_EXPECTED_PAREN,msg_get(MSG_EXPECT_RPAREN_PARAMS));ast_destroy((ASTNode*)node);return NULL;}
    advance(p);
    if(!check(p,TOKEN_LBRACE)){error_at(p,&p->current,ERR_PARSER_EXPECTED_BRACE,msg_get(MSG_EXPECT_LBRACE_FUNC));ast_destroy((ASTNode*)node);return NULL;}
    advance(p);
    while(p->current.type!=TOKEN_RBRACE && p->current.type!=TOKEN_EOF) {
        Token rule_start = p->current;
        ASTNode* rule=NULL;
        if(parser_identifier_is(&p->current,"always") || parser_identifier_is(&p->current,"default")) {
            int is_always=parser_identifier_is(&p->current,"always");
            Token special=p->current; ASTNode* action;
            advance(p);
            if(!match(p,TOKEN_PIPE)){error_at(p,&p->current,ERR_PARSER_UNEXPECTED_TOKEN,"Expected '->' after rule keyword.");ast_destroy((ASTNode*)node);ast_destroy(rules);return NULL;}
            action=parse_statement(p);
            if(!action){ast_destroy((ASTNode*)node);ast_destroy(rules);return NULL;}
            if(!is_always && seen_default++){error_at(p,&special,ERR_PARSER_UNEXPECTED_TOKEN,"Only one default rule is allowed.");ast_destroy(action);ast_destroy((ASTNode*)node);ast_destroy(rules);return NULL;}
            if(is_always){ASTRuleAlways* n=(ASTRuleAlways*)parser_ast_create(p,AST_RULE_ALWAYS);if(!n){ast_destroy(action);ast_destroy((ASTNode*)node);ast_destroy(rules);return NULL;}n->base.line=special.line;n->base.col=special.col;n->action=action;rule=(ASTNode*)n;}
            else {ASTRuleDefault* n=(ASTRuleDefault*)parser_ast_create(p,AST_RULE_DEFAULT);if(!n){ast_destroy(action);ast_destroy((ASTNode*)node);ast_destroy(rules);return NULL;}n->base.line=special.line;n->base.col=special.col;n->action=action;rule=(ASTNode*)n;}
        } else {
            ASTNode* condition=parse_range(p);
            if(!condition){ast_destroy((ASTNode*)node);ast_destroy(rules);return NULL;}
            if(match(p,TOKEN_COLON)) {
                if(!match(p,TOKEN_FAT_ARROW)){error_at(p,&p->current,ERR_PARSER_UNEXPECTED_TOKEN,"Expected ':=>' barrier operator.");ast_destroy(condition);ast_destroy((ASTNode*)node);ast_destroy(rules);return NULL;}
                ASTNode* message=parse_statement(p);
                ASTRuleBarrier* n;
                if(!message){ast_destroy(condition);ast_destroy((ASTNode*)node);ast_destroy(rules);return NULL;}
                n=(ASTRuleBarrier*)parser_ast_create(p,AST_RULE_BARRIER);
                if(!n){ast_destroy(condition);ast_destroy(message);ast_destroy((ASTNode*)node);ast_destroy(rules);return NULL;}
                n->base.line=condition->line;n->base.col=condition->col;n->condition=condition;n->message=message;rule=(ASTNode*)n;
            } else if(match(p,TOKEN_PIPE)) {
                ASTNode* action=parse_statement(p);
                ASTRuleTrigger* n;
                if(!action){ast_destroy(condition);ast_destroy((ASTNode*)node);ast_destroy(rules);return NULL;}
                n=(ASTRuleTrigger*)parser_ast_create(p,AST_RULE_TRIGGER);
                if(!n){ast_destroy(condition);ast_destroy(action);ast_destroy((ASTNode*)node);ast_destroy(rules);return NULL;}
                n->base.line=condition->line;n->base.col=condition->col;n->condition=condition;n->action=action;rule=(ASTNode*)n;
            } else {
                error_at(p,&p->current,ERR_PARSER_UNEXPECTED_TOKEN,"Expected ':=>' or '->' in ruleset.");
                ast_destroy(condition);ast_destroy((ASTNode*)node);ast_destroy(rules);return NULL;
            }
        }
        parser_stamp_node_tokens(p, rule, &rule_start, &p->previous);
        if(!rules)rules=rule;else tail->next=rule;tail=rule;
    }
    if(!check(p,TOKEN_RBRACE)){error_at(p,&p->current,ERR_PARSER_EXPECTED_BRACE,msg_get(MSG_EXPECT_RBRACE_BLOCK));ast_destroy((ASTNode*)node);ast_destroy(rules);return NULL;}
    advance(p); node->rules=rules;
    parser_stamp_node_tokens(p, (ASTNode*)node, &keyword, &p->previous);
    return (ASTNode*)node;
}

static ASTNode* parse_apply_rules(Parser* p) {
    Token keyword=p->current,name;
    ASTApplyRules* node;
    ASTNode* args=NULL; ASTNode* tail=NULL;
    advance(p); advance(p); /* apply rules */
    if(!check(p,TOKEN_LPAREN)){error_at(p,&p->current,ERR_PARSER_EXPECTED_PAREN,msg_get(MSG_EXPECT_LPAREN_FUNC));return NULL;}
    advance(p);
    if(!match(p,TOKEN_IDENTIFIER)){error_at(p,&p->current,ERR_PARSER_EXPECTED_EXPRESSION,"Expected ruleset name.");return NULL;}
    name=p->previous;
    if(!check(p,TOKEN_LPAREN)){error_at(p,&p->current,ERR_PARSER_EXPECTED_PAREN,msg_get(MSG_EXPECT_LPAREN_FUNC));return NULL;}
    advance(p);
    if(!check(p,TOKEN_RPAREN)) {
        do { ASTNode* arg=parse_expression(p); if(!arg){ast_destroy(args);return NULL;} if(!args)args=arg;else tail->next=arg;tail=arg; } while(match(p,TOKEN_COMMA));
    }
    if(!check(p,TOKEN_RPAREN)){error_at(p,&p->current,ERR_PARSER_EXPECTED_PAREN,msg_get(MSG_EXPECT_RPAREN_ARGS));ast_destroy(args);return NULL;}
    advance(p);
    if(!check(p,TOKEN_RPAREN)){error_at(p,&p->current,ERR_PARSER_EXPECTED_PAREN,msg_get(MSG_EXPECT_RPAREN_ARGS));ast_destroy(args);return NULL;}
    advance(p);
    node=(ASTApplyRules*)parser_ast_create(p,AST_APPLY_RULES);
    if(!node){ast_destroy(args);return NULL;}
    node->base.line=keyword.line;node->base.col=keyword.col;
    node->ruleset_name=(char*)parser_malloc(p,(size_t)name.length+1u);
    if(!node->ruleset_name){ast_destroy((ASTNode*)node);ast_destroy(args);return NULL;}
    memcpy(node->ruleset_name,name.start,(size_t)name.length);node->ruleset_name[name.length]='\0';
    node->args=args; (void)match(p,TOKEN_SEMICOLON);
    parser_stamp_node_tokens(p, (ASTNode*)node, &keyword, &p->previous);
    return (ASTNode*)node;
}

static ASTNode* parse_on_block(Parser* p) {
    Token keyword=p->current, ruleset, event;
    ASTOnBlock* node;
    advance(p);
    if(!match(p,TOKEN_IDENTIFIER)){error_at(p,&p->current,ERR_PARSER_EXPECTED_EXPRESSION,"Expected ruleset name after on.");return NULL;}
    ruleset=p->previous;
    if(!match(p,TOKEN_DOT) || !match(p,TOKEN_IDENTIFIER)){error_at(p,&p->current,ERR_PARSER_UNEXPECTED_TOKEN,"Expected Ruleset.Event after on.");return NULL;}
    event=p->previous;
    if(!check(p,TOKEN_LBRACE)){error_at(p,&p->current,ERR_PARSER_EXPECTED_BRACE,msg_get(MSG_EXPECT_LBRACE_FUNC));return NULL;}
    advance(p);
    node=(ASTOnBlock*)parser_ast_create(p,AST_ON_BLOCK);if(!node)return NULL;
    node->base.line=keyword.line;node->base.col=keyword.col;
    node->ruleset_name=(char*)parser_malloc(p,(size_t)ruleset.length+1u);
    node->event_name=(char*)parser_malloc(p,(size_t)event.length+1u);
    if(!node->ruleset_name || !node->event_name){ast_destroy((ASTNode*)node);return NULL;}
    memcpy(node->ruleset_name,ruleset.start,(size_t)ruleset.length);node->ruleset_name[ruleset.length]='\0';
    memcpy(node->event_name,event.start,(size_t)event.length);node->event_name[event.length]='\0';
    node->body=parse_block(p);
    if(!node->body){ast_destroy((ASTNode*)node);return NULL;}
    parser_stamp_node_tokens(p, (ASTNode*)node, &keyword, &p->previous);
    return (ASTNode*)node;
}

static ASTNode* parse_unroll(Parser* p) {
    Token keyword=p->current; ASTNode* count; ASTNode* loop; ASTUnroll* node;
    advance(p); advance(p); /* unroll ( */
    count=parse_expression(p);
    if(!count || !check(p,TOKEN_RPAREN)){if(count)ast_destroy(count);error_at(p,&p->current,ERR_PARSER_EXPECTED_PAREN,msg_get(MSG_EXPECT_RPAREN_EXPR));return NULL;}
    advance(p);
    if(!match(p,TOKEN_PIPE)){error_at(p,&p->current,ERR_PARSER_UNEXPECTED_TOKEN,"Expected '->' after unroll count.");ast_destroy(count);return NULL;}
    loop=parse_expression(p);
    if(!loop){ast_destroy(count);return NULL;}
    if(loop->type!=AST_LOOP){error_at(p,&p->current,ERR_PARSER_UNEXPECTED_TOKEN,"unroll target must be a loop.");ast_destroy(count);ast_destroy(loop);return NULL;}
    node=(ASTUnroll*)parser_ast_create(p,AST_UNROLL);
    if(!node){ast_destroy(count);ast_destroy(loop);return NULL;}
    node->base.line=keyword.line;node->base.col=keyword.col;node->count=count;node->loop=loop;
    parser_stamp_node_tokens(p, (ASTNode*)node, &keyword, &p->previous);
    return (ASTNode*)node;
}

static ASTNode* parse_lambda(Parser* p) {
    Token keyword=p->previous; ASTLambda* node;
    if(!check(p,TOKEN_LPAREN)){error_at(p,&p->current,ERR_PARSER_EXPECTED_PAREN,msg_get(MSG_EXPECT_LPAREN_FUNC));return NULL;}
    advance(p);
    node=(ASTLambda*)parser_ast_create(p,AST_LAMBDA);if(!node)return NULL;
    node->base.line=keyword.line;node->base.col=keyword.col;
    node->params=parse_simple_parameter_list(p);
    if(!check(p,TOKEN_RPAREN)){error_at(p,&p->current,ERR_PARSER_EXPECTED_PAREN,msg_get(MSG_EXPECT_RPAREN_PARAMS));ast_destroy((ASTNode*)node);return NULL;}
    advance(p);
    if(match(p,TOKEN_GT)){node->body=parse_expression(p);node->body_is_expression=1;}
    else if(check(p,TOKEN_LBRACE)){advance(p);node->body=parse_block(p);node->body_is_expression=0;}
    else {error_at(p,&p->current,ERR_PARSER_UNEXPECTED_TOKEN,"Expected '>' expression or block after lambda parameters.");ast_destroy((ASTNode*)node);return NULL;}
    if(!node->body){ast_destroy((ASTNode*)node);return NULL;}
    parser_stamp_node_tokens(p, (ASTNode*)node, &keyword, &p->previous);
    return (ASTNode*)node;
}

static ASTNode* parse_linkname_directive(Parser* p) {
    Token directive_start = p->previous;
    Token value_token;
    char* value;
    size_t value_length;

    if (p->nesting_depth != 0u) {
        error_at(p, &directive_start, ERR_PARSER_UNEXPECTED_TOKEN,
            "linkname directives are only valid at program scope.");
        return NULL;
    }
    if (!match(p, TOKEN_IDENTIFIER) ||
        !parser_identifier_is(&p->previous, "linkname")) {
        error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
            "Expected 'linkname' after '!!='.");
        return NULL;
    }
    if (!match(p, TOKEN_ASSIGN)) {
        error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
            "Expected '=' after linkname.");
        return NULL;
    }
    if (!match(p, TOKEN_STRING)) {
        error_at(p, &p->current, ERR_PARSER_UNEXPECTED_TOKEN,
            "Expected a string literal as linkname source.");
        return NULL;
    }
    value_token = p->previous;
    if (value_token.length < 2 || value_token.start[0] != '"' ||
        value_token.start[value_token.length - 1] != '"') {
        error_at(p, &value_token, ERR_PARSER_UNEXPECTED_TOKEN,
            "Malformed linkname string literal.");
        return NULL;
    }
    value_length = (size_t)value_token.length - 2u;
    if (value_length == 0u) {
        error_at(p, &value_token, ERR_PARSER_UNEXPECTED_TOKEN,
            "linkname source cannot be empty.");
        return NULL;
    }
    value = (char*)parser_malloc(p, value_length + 1u);
    if (!value) return NULL;
    if (value_length) memcpy(value, value_token.start + 1, value_length);
    value[value_length] = '\0';
    if (!match(p, TOKEN_SEMICOLON)) {
        free(value);
        error_at(p, &p->current, ERR_PARSER_EXPECTED_SEMICOLON,
            msg_get(MSG_EXPECT_SEMICOLON_VAR));
        return NULL;
    }
    if (p->ctx->active_linkname) free(p->ctx->active_linkname);
    p->ctx->active_linkname = value;
    p->consumed_metadata = 1;
    return NULL;
}

static ASTNode* parse_statement(Parser* p) {
    if (p->panic_mode) p->panic_mode = 0; // Her yeni ifadede hata modunu sıfırla (Basit senkronizasyon)

    if (match(p, TOKEN_FORCE_ASSIGN)) return parse_linkname_directive(p);

    if (match(p, TOKEN_SEMICOLON)) {
        Token semicolon = p->previous;
        ASTBlock* nop = (ASTBlock*)parser_ast_create(p, AST_BLOCK);
        if (!nop) return NULL;
        nop->base.line = semicolon.line;
        nop->base.col = semicolon.col;
        nop->statements = NULL;
        parser_stamp_node_token(p, (ASTNode*)nop, &semicolon);
        return (ASTNode*)nop;
    }

    int is_pub = 0;
    int is_exp = 0;

    if (match(p, TOKEN_KW_EXP)) is_exp = 1;
    if (match(p, TOKEN_KW_PUP)) is_pub = 1;

    if (p->current.type==TOKEN_IDENTIFIER && parser_identifier_is(&p->current,"rules") &&
        (peek_token(p).type==TOKEN_IDENTIFIER || peek_token(p).type==TOKEN_LBRACKET)) {
        return parse_ruleset(p);
    }
    if (p->current.type==TOKEN_IDENTIFIER) {
        Token look1=peek_token(p);
        Token look2=peek_token_ahead(p,2);
        if (parser_identifier_is(&p->current,"apply") &&
            parser_identifier_is(&look1,"rules") && look2.type==TOKEN_LPAREN)
            return parse_apply_rules(p);
        if (parser_identifier_is(&p->current,"on") &&
            look1.type==TOKEN_IDENTIFIER && look2.type==TOKEN_DOT)
            return parse_on_block(p);
    }
    if (p->current.type==TOKEN_IDENTIFIER && parser_identifier_is(&p->current,"unroll") &&
        peek_token(p).type==TOKEN_LPAREN) {
        return parse_unroll(p);
    }
    if (p->current.type==TOKEN_IDENTIFIER &&
        (parser_identifier_is(&p->current,"select") || parser_identifier_is(&p->current,"scan")) &&
        peek_token(p).type==TOKEN_LPAREN) {
        return parse_select_scan(p, parser_identifier_is(&p->current,"scan"));
    }
    if (check(p, TOKEN_KW_IF)) return parse_if_statement(p);
    if (match(p, TOKEN_KW_FASTEXEC)) return parse_fast_exec_block(p);
    if (match(p, TOKEN_KW_ASM)) return parse_asm_label(p);
    if (match(p, TOKEN_KW_USE)) return parse_use(p);
    if (match(p, TOKEN_KW_DEFER)) {
        Token start = p->previous;
        ASTNode* deferred = parse_defer(p);
        if (deferred) parser_stamp_node_tokens(p, deferred, &start, &p->previous);
        return deferred;
    }
    if (match(p, TOKEN_KW_RET)) return parse_return_statement(p);
    if (match(p, TOKEN_KW_BREAK) || match(p, TOKEN_KW_CONTINUE)) {
        Token keyword = p->previous;
        ASTNodeType type = keyword.type == TOKEN_KW_BREAK ? AST_BREAK : AST_CONTINUE;
        ASTNode* control = parser_ast_create(p, type);
        if (!control) return NULL;
        control->line = keyword.line;
        control->col = keyword.col;
        consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON,
            MSG_EXPECT_SEMICOLON_EXPR);
        if (p->previous.type == TOKEN_SEMICOLON)
            parser_stamp_node_tokens(p, control, &keyword, &p->previous);
        return control;
    }

    if (match(p, TOKEN_PRE_MUST)) {
        Token start = p->previous;
        ASTNode* must;
        if (p->nesting_depth != 0u) {
            error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN,
                "MUST dependencies are only valid at program scope.");
            return NULL;
        }
        must = parse_must_decl(p);
        if (must) parser_stamp_node_tokens(p, must, &start, &p->previous);
        return must;
    }

    if (match(p, TOKEN_PRE_VAR) || match(p, TOKEN_PRE_CONST)) {
        Token declaration_start = p->previous;
        ASTVarDecl* decl = (ASTVarDecl*)parse_var_decl(p);
        if (!decl) return NULL;
        decl->is_public = is_pub;
        decl->is_exported = is_exp;
        consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
        if (p->previous.type == TOKEN_SEMICOLON)
            parser_stamp_node_tokens(p, (ASTNode*)decl, &declaration_start, &p->previous);
        return (ASTNode*)decl;
    }
    
    if (match(p, TOKEN_KW_FN) || match(p, TOKEN_PRE_FUNC) || match(p, TOKEN_KW_EXTERN)) {
        Token declaration_start = p->previous;
        int is_extern = p->previous.type == TOKEN_KW_EXTERN;
        ASTFuncDecl* decl = (ASTFuncDecl*)parse_func_decl(p);
        if (!decl) return NULL;
        decl->is_extern = is_extern;
        decl->is_public = is_pub;
        decl->is_exported = is_exp;
        if (p->previous.type == TOKEN_RBRACE || p->previous.type == TOKEN_HALT)
            parser_stamp_node_tokens(p, (ASTNode*)decl, &declaration_start, &p->previous);
        return (ASTNode*)decl;
    }

    if (match(p, TOKEN_KW_STRUCT) || match(p, TOKEN_PRE_STRUCT)) {
        Token start = p->previous;
        ASTStructDecl* decl = (ASTStructDecl*)parse_struct_decl(p, 0, NULL);
        if (!decl) return NULL;
        decl->is_public = is_pub;
        decl->is_exported = is_exp;
        parser_stamp_node_tokens(p, (ASTNode*)decl, &start, &p->previous);
        return (ASTNode*)decl;
    }

    if (match(p, TOKEN_KW_GROUP)) {
        Token start = p->previous;
        ASTGroupDecl* decl = (ASTGroupDecl*)parse_group_decl(p);
        if (!decl) return NULL;
        decl->is_public = is_pub;
        decl->is_exported = is_exp;
        parser_stamp_node_tokens(p, (ASTNode*)decl, &start, &p->previous);
        return (ASTNode*)decl;
    }

    // Special Thread Block: (name)>{...}
    if (p->current.type == TOKEN_LPAREN) {
        Token thread_start = p->current;
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
                    char* name = (char*)parser_malloc(p, (size_t)name_tok.length + 1u);
                    if (!name) return NULL;
                    memcpy(name, name_tok.start, (size_t)name_tok.length);
                    name[name_tok.length] = '\0';
                    {
                        ASTNode* thread = parse_thread_block(p, name);
                        if (thread)
                            parser_stamp_node_tokens(p, thread, &thread_start, &p->previous);
                        return thread;
                    }
                }
            }
        }
    }
    if (match(p, TOKEN_NOT)) {
        Token not_start = p->previous;
        if (match(p, TOKEN_KW_STRUCT) || match(p, TOKEN_PRE_STRUCT)) {
            ASTNode* packed = parse_struct_decl(p, 1, NULL);
            if (packed)
                parser_stamp_node_tokens(p, packed, &not_start, &p->previous);
            return packed; // !struct (Packed)
        }

        // Check for !(listen)
        if (p->current.type == TOKEN_LPAREN) {
            Token lookahead = peek_token(p);
            if (lookahead.type == TOKEN_KW_LISTEN ||
                (lookahead.type == TOKEN_IDENTIFIER && strncmp(lookahead.start, "listen", 6) == 0)) {
                advance(p); // (
                advance(p); // listen
                consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);
                consume(p, TOKEN_COLON, ERR_PARSER_UNEXPECTED_TOKEN, MSG_EXPECT_COLON_VAR);
                
                ASTNexusCapture* listen = (ASTNexusCapture*)parser_ast_create(p, AST_LISTEN);
                if (!listen) return NULL;
                listen->base.line = not_start.line;
                listen->base.col = not_start.col;
                listen->handler_name = parser_strdup(p, "global_listener");
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
                parser_stamp_node_tokens(p, (ASTNode*)listen, &not_start, &p->previous);
                return (ASTNode*)listen;
            }
        }
    }
    if (match(p, TOKEN_KW_ENUM) || match(p, TOKEN_PRE_ENUM)) {
        Token start = p->previous;
        ASTNode* node = parse_enum_decl(p, NULL);
        if (node) parser_stamp_node_tokens(p, node, &start, &p->previous);
        return node;
    }
    if (match(p, TOKEN_KW_UNION) || match(p, TOKEN_PRE_UNION)) {
        Token start = p->previous;
        ASTNode* node = parse_union_decl(p, NULL);
        if (node) parser_stamp_node_tokens(p, node, &start, &p->previous);
        return node;
    }
    if (match(p, TOKEN_PRE_NEWTYPE)) {
        Token start = p->previous;
        ASTNode* node = parse_newtype_decl(p);
        if (node) parser_stamp_node_tokens(p, node, &start, &p->previous);
        return node;
    }
    if (match(p, TOKEN_KW_GROUP)) {
        Token start = p->previous;
        ASTNode* node = parse_group_decl(p);
        if (node) parser_stamp_node_tokens(p, node, &start, &p->previous);
        return node;
    }

    if (match(p, TOKEN_LBRACE)) {
        return parse_block(p);
    }
    /* Contextual alias for the explicit jump operator: jump(label); */
    if (p->current.type == TOKEN_IDENTIFIER &&
        parser_identifier_is(&p->current, "jump") &&
        peek_token(p).type == TOKEN_LPAREN) {
        Token jump_start = p->current;
        Token target;
        ASTNexusJump* jump_node;
        advance(p);
        consume(p, TOKEN_LPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_LPAREN_FUNC);
        if (!match(p, TOKEN_IDENTIFIER)) {
            error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION,
                "Expected a label identifier inside jump(...).");
            return NULL;
        }
        target = p->previous;
        jump_node = (ASTNexusJump*)parser_ast_create(p, AST_NEXUS_JUMP);
        if (!jump_node) return NULL;
        jump_node->base.line = jump_start.line;
        jump_node->base.col = jump_start.col;
        jump_node->label_name = (char*)parser_malloc(p, (size_t)target.length + 1u);
        if (!jump_node->label_name) {
            ast_destroy((ASTNode*)jump_node);
            return NULL;
        }
        memcpy(jump_node->label_name, target.start, (size_t)target.length);
        jump_node->label_name[target.length] = '\0';
        consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);
        consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_EXPR);
        parser_stamp_node_tokens(p, (ASTNode*)jump_node, &jump_start, &p->previous);
        return (ASTNode*)jump_node;
    }

    if (match(p, TOKEN_JUMP)) {
        Token jump_start = p->previous;
        // ?>(label);
        consume(p, TOKEN_LPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_LPAREN_FUNC);
        if (match(p, TOKEN_IDENTIFIER)) {
            ASTNexusJump* jmp = (ASTNexusJump*)parser_ast_create(p, AST_NEXUS_JUMP);
            Token t = p->previous;
            if (!jmp) return NULL;
            jmp->base.line = t.line;
            jmp->base.col = t.col;
            jmp->label_name = (char*)parser_malloc(p, (size_t)t.length + 1u);
            if (!jmp->label_name) {
                ast_destroy((ASTNode*)jmp);
                return NULL;
            }
            memcpy(jmp->label_name, t.start, (size_t)t.length);
            jmp->label_name[t.length] = '\0';
            consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);
            consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_EXPR);
            parser_stamp_node_tokens(p, (ASTNode*)jmp, &jump_start, &p->previous);
            return (ASTNode*)jmp;
        }
        error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_FUNC_NAME));
    }

    if (match(p, TOKEN_HALT)) {
        Token halt_start = p->previous;
        ASTHalt* halt = (ASTHalt*)parser_ast_create(p, AST_NEXUS_HALT);
        if (!halt) return NULL;
        halt->base.line = p->previous.line;
        halt->base.col = p->previous.col;
        if (!match(p, TOKEN_SEMICOLON) &&
            p->current.type != TOKEN_RBRACE &&
            p->current.type != TOKEN_EOF) {
            halt->expr = parse_expression(p);
            if (!halt->expr) {
                ast_destroy((ASTNode*)halt);
                return NULL;
            }
            consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_EXPR);
        }
        parser_stamp_node_tokens(p, (ASTNode*)halt, &halt_start, &p->previous);
        return (ASTNode*)halt;
    }

    if (match(p, TOKEN_INTENT)) {
        Token intent_start = p->previous;
        ASTIntent* in = (ASTIntent*)parser_ast_create(p, AST_NEXUS_INTENT);
        if (!in) return NULL;
        in->base.line = p->previous.line;
        in->base.col = p->previous.col;
        if (match(p, TOKEN_IDENTIFIER)) {
            Token t = p->previous;
            in->intent_name = (char*)parser_malloc(p, (size_t)t.length + 1u);
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
        parser_stamp_node_tokens(p, (ASTNode*)in, &intent_start, &p->previous);
        return (ASTNode*)in;
    }

    if (match(p, TOKEN_FORCE)) {
        Token panic_start = p->previous;
        ASTPanic* panic = (ASTPanic*)parser_ast_create(p, AST_PANIC);
        if (!panic) return NULL;
        panic->base.line = p->previous.line;
        panic->base.col = p->previous.col;
        if (match(p, TOKEN_STRING)) {
            Token t = p->previous;
            panic->message = (char*)parser_malloc(p, (size_t)t.length + 1u);
            if (!panic->message) {
                ast_destroy((ASTNode*)panic);
                return NULL;
            }
            memcpy(panic->message, t.start, (size_t)t.length);
            panic->message[t.length] = '\0';
        } else {
            panic->message = parser_strdup(p, "Critical Panic");
            if (!panic->message) {
                ast_destroy((ASTNode*)panic);
                return NULL;
            }
        }
        consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_EXPR);
        parser_stamp_node_tokens(p, (ASTNode*)panic, &panic_start, &p->previous);
        return (ASTNode*)panic;
    }
    
    /* Conditional action shorthand: (condition) -> break/continue/statement. */
    if (parser_looks_like_conditional_action(p)) {
        return parse_conditional_action(p);
    }

    // Expression Statement
    ASTNode* expr = parse_expression(p);
    if (!expr) return NULL;
    
    // Check for Label Syntax: expr :> statement
    if (match(p, TOKEN_NEXUS_LABEL)) {
        Token label_operator = p->previous;
        OccSourceSpan label_start_span = expr->source_span;
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
            label_name = (char*)parser_malloc(p, length + 1u);
            if (label_name) memcpy(label_name, id->name, length + 1u);
        } else if (target->type == AST_LITERAL) {
            ASTLiteral* lit = (ASTLiteral*)target;
            size_t length = lit->string_value ? strlen(lit->string_value) : 0u;
            if (lit->string_value) {
                label_name = (char*)parser_malloc(p, length + 1u);
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

        ASTNexusLabel* lbl = (ASTNexusLabel*)parser_ast_create(p, AST_NEXUS_LABEL);
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
        if (label_start_span.has_location) {
            OccSourceSpan end_span;
            if (lbl->statement && lbl->statement->source_span.has_location) {
                end_span = lbl->statement->source_span;
            } else {
                parser_make_token_span(p->ctx, &label_operator, &end_span);
            }
            if (end_span.has_location &&
                end_span.source_id == label_start_span.source_id &&
                end_span.end_offset >= label_start_span.start_offset) {
                label_start_span.end_offset = end_span.end_offset;
                label_start_span.end_line = end_span.end_line;
                label_start_span.end_column = end_span.end_column;
                (void)ast_set_source_span((ASTNode*)lbl, &label_start_span);
            }
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
    if (!ctx) return PARSER_STATUS_INVALID_STATE;
    if (!ctx->source_buffer || ctx->ast_root) {
        ctx->status = PARSER_STATUS_INVALID_STATE;
        return (int)ctx->status;
    }
    ctx->status = PARSER_STATUS_OK;
    printf("%s\n", msg_get(MSG_PARSER_START));

    Parser parser;
    memset(&parser, 0, sizeof(parser));
    parser.ctx = ctx;
    parser.current.type = TOKEN_EOF;
    parser.previous.type = TOKEN_EOF;
    parser.had_error = 0;
    parser.panic_mode = 0;

    advance(&parser); // İlk tokeni yükle
    if (parser_status_is_terminal(ctx->status)) {
        return (int)ctx->status;
    }

    ASTProgram* program = (ASTProgram*)parser_ast_create(&parser, AST_PROGRAM);
    if (!program) return (int)ctx->status;
    program->base.line = 1;
    program->base.col = 1;
    parser_stamp_node_source_file(&parser, (ASTNode*)program);
    ctx->ast_root = (void*)program; /* Keep partial trees owned by the context on parse failure. */
    ASTNode* current_stmt = NULL;

    while (!match(&parser, TOKEN_EOF)) {
        Token statement_start = parser.current;
        ASTNode* stmt;
        parser.consumed_metadata = 0;
        stmt = parse_statement(&parser);
        if (parser_status_is_terminal(ctx->status)) {
            ast_destroy(stmt);
            return (int)ctx->status;
        }
        if (stmt) {
            // AST'ye ekle (Bağlı liste mantığı)
            if (program->declarations == NULL) {
                program->declarations = stmt;
            } else {
                current_stmt->next = stmt;
            }
            current_stmt = stmt;
            while (current_stmt->next) current_stmt = current_stmt->next;
        } else {
            if (parser.consumed_metadata) continue;
            if (parser_status_is_terminal(ctx->status)) return (int)ctx->status;
            if (!parser.had_error) {
                ctx->status = PARSER_STATUS_INVALID_STATE;
                return (int)ctx->status;
            }
            // Recover at the next statement boundary without stalling on an unchanged token.
            if (parser.current.type == statement_start.type &&
                parser.current.source_offset == statement_start.source_offset &&
                parser.current.line == statement_start.line &&
                parser.current.col == statement_start.col &&
                parser.current.type != TOKEN_EOF) {
                advance(&parser);
            }
            parser_synchronize_statement(&parser);
        }
    }

    if (parser_status_is_terminal(ctx->status)) return (int)ctx->status;
    if (parser.had_error) {
        ctx->status = PARSER_STATUS_SYNTAX_ERROR;
        return (int)ctx->status;
    }

    printf("%s\n", msg_get(MSG_PARSER_SUCCESS));
    
    // AST Görselleştirme
    if (program) {
        ctx->ast_root = (void*)program; // Semantic analiz için kaydet
        ast_print((ASTNode*)program);
    }
    
    ctx->status = PARSER_STATUS_OK;
    return (int)ctx->status;
}

ParserStatus parser_get_status(const ParserContext* ctx) {
    return ctx ? ctx->status : PARSER_STATUS_INVALID_STATE;
}

void parser_test_fail_allocation_at(ParserContext* ctx, size_t allocation_index) {
    if (!ctx) return;
    ctx->test_allocation_count = 0u;
    ctx->test_fail_allocation_at = allocation_index;
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
        input.span.source_id = ctx->source_id;
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
        if (!occ_diagnostics_add(ctx->diagnostics, &input)) {
            ctx->status = occ_diagnostics_limit_exceeded(ctx->diagnostics)
                ? PARSER_STATUS_DIAGNOSTIC_LIMIT
                : PARSER_STATUS_OUT_OF_MEMORY;
            return;
        }
    }
    Error* err = (Error*)parser_context_malloc(ctx, sizeof(*err), 1);
    if (!err) {
        ctx->status = PARSER_STATUS_OUT_OF_MEMORY;
        return;
    }
    err->code = code;
    err->filename = parser_context_strdup(ctx, ctx->filename ? ctx->filename : "<unknown>");
    err->message = parser_context_strdup(ctx, message ? message : "");
    err->suggestion = suggestion ? parser_context_strdup(ctx, suggestion) : NULL;
    err->hint = hint ? parser_context_strdup(ctx, hint) : NULL;
    if (!err->filename || !err->message ||
        (suggestion && !err->suggestion) || (hint && !err->hint)) {
        free(err->filename);
        free(err->message);
        free(err->suggestion);
        free(err->hint);
        free(err);
        ctx->status = PARSER_STATUS_OUT_OF_MEMORY;
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
int parser_attach_source_files(ParserContext* ctx, OccSourceFileTable* table) {
    uint32_t source_id;
    if (!ctx || !table || !ctx->filename ||
        !occ_source_files_register(table, ctx->filename, &source_id)) return 0;
    ctx->source_files = table;
    ctx->source_id = source_id;
    return 1;
}

void parser_attach_diagnostics(ParserContext* ctx, OccDiagnosticCollector* collector, uint32_t stage_id) {
    if (!ctx) return;
    ctx->diagnostics = collector;
    ctx->diagnostic_stage_id = stage_id ? stage_id : OCC_STAGE_PARSER;
}

int parser_attach_token_stream(
    ParserContext* ctx, const struct Token* tokens, size_t token_count
) {
    size_t i;
    size_t previous_end = 0u;
    if (!ctx || !tokens || token_count == 0 ||
        !ctx->source_buffer ||
        tokens[token_count - 1u].type != TOKEN_EOF) {
        return 0;
    }
    for (i = 0u; i < token_count; ++i) {
        const Token* token = &tokens[i];
        size_t token_end;
        if ((unsigned int)token->type > (unsigned int)TOKEN_ASM_BODY ||
            !token->start || token->length < 0 ||
            token->line <= 0 || token->col <= 0 ||
            token->source_offset > ctx->source_len ||
            token->source_length > ctx->source_len - token->source_offset ||
            token->source_offset < previous_end ||
            (token->type == TOKEN_EOF && i + 1u != token_count) ||
            (token->type != TOKEN_EOF && token->type != TOKEN_ASM_BODY &&
             token->source_length == 0u)) {
            return 0;
        }
        if (token->type != TOKEN_EOF && token->type != TOKEN_ERROR) {
            if ((size_t)token->length != token->source_length ||
                memcmp(token->start,
                    ctx->source_buffer + token->source_offset,
                    token->source_length) != 0) {
                return 0;
            }
        }
        token_end = token->source_offset + token->source_length;
        previous_end = token_end;
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
        if (ctx->active_linkname) free(ctx->active_linkname);
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