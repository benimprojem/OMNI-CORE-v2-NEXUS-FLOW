#include "lexer.h"
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>

// Yardımcı: Karakter kontrolü
static char peek(ParserContext* ctx) {
    if (ctx->pos >= ctx->source_len) return '\0';
    return ctx->source_buffer[ctx->pos];
}

static char peek_next(ParserContext* ctx, size_t offset) {
    if (!ctx || !ctx->source_buffer || ctx->pos >= ctx->source_len ||
        offset >= ctx->source_len - ctx->pos) return '\0';
    return ctx->source_buffer[ctx->pos + offset];
}

static char advance(ParserContext* ctx) {
    char c = peek(ctx);
    if (ctx->pos >= ctx->source_len) return '\0';
    ctx->pos++;
    if (c == '\n') {
        if (ctx->current_line < INT_MAX) ++ctx->current_line;
        ctx->current_col = 1;
    } else if (ctx->current_col < INT_MAX) {
        ++ctx->current_col;
    }
    return c;
}

static bool match(ParserContext* ctx, char expected) {
    if (peek(ctx) == expected) {
        advance(ctx);
        return true;
    }
    return false;
}

static Token error_token(ParserContext* ctx, const char* message);

static Token make_token(
    ParserContext* ctx, OmniTokenType type, const char* start, size_t length
) {
    Token token;
    if (length > (size_t)INT_MAX) {
        return error_token(ctx, "Token exceeds maximum supported length");
    }
    token.type = type;
    token.start = start;
    token.length = (int)length;
    token.line = ctx->token_start_line;
    token.col = ctx->token_start_col;
    token.source_offset = ctx->token_start_offset;
    token.source_length = length;
    token.diagnostic_id = 0;
    return token;
}

static Token error_token(ParserContext* ctx, const char* message) {
    Token token;
    size_t message_length = message ? strlen(message) : 0u;
    token.type = TOKEN_ERROR;
    token.start = message ? message : "Lexer error";
    token.length = message_length > (size_t)INT_MAX ? INT_MAX : (int)message_length;
    token.line = ctx->token_start_line;
    token.col = ctx->token_start_col;
    token.source_offset = ctx->token_start_offset;
    token.source_length = ctx->pos - ctx->token_start_offset;
    token.diagnostic_id = 0;

    /*
     * Lexer errors become job-owned diagnostics at their origin. The token
     * carries a stable 1-based reference so downstream stages do not
     * re-attribute or duplicate the same finding.
     */
    if (ctx->diagnostics && message_length != 0u) {
        OccDiagnosticInput input;
        if (occ_diagnostics_count(ctx->diagnostics) >= UINT32_MAX) {
            ctx->status = PARSER_STATUS_OUT_OF_MEMORY;
            return token;
        }
        size_t i;
        uint32_t line = (uint32_t)(ctx->token_start_line > 0 ? ctx->token_start_line : 1);
        uint32_t column = (uint32_t)(ctx->token_start_col > 0 ? ctx->token_start_col : 1);
        size_t end = ctx->pos <= ctx->source_len ? ctx->pos : ctx->source_len;
        memset(&input, 0, sizeof(input));
        input.code = ERR_LEXER_ERROR;
        input.stage_id = OCC_STAGE_LEXER;
        input.message_id = ERR_LEXER_ERROR;
        input.severity = OCC_DIAG_ERROR;
        input.message = message;
        input.message_length = message_length;
        input.span.source_id = 0u;
        input.span.start_offset = (uint64_t)ctx->token_start_offset;
        input.span.end_offset = (uint64_t)end;
        input.span.start_line = line;
        input.span.start_column = column;
        input.span.has_location = 1u;
        for (i = ctx->token_start_offset; i < end; ++i) {
            if (ctx->source_buffer[i] == '\n') {
                if (line != UINT32_MAX) ++line;
                column = 1u;
            } else if (column != UINT32_MAX) {
                ++column;
            }
        }
        input.span.end_line = line;
        input.span.end_column = column;
        {
            size_t diagnostic_index = occ_diagnostics_count(ctx->diagnostics);
            if (occ_diagnostics_add(ctx->diagnostics, &input)) {
                token.diagnostic_id = (uint32_t)(diagnostic_index + 1u);
            } else {
                /* A lexical finding must never disappear silently on collector failure. */
                ctx->status = PARSER_STATUS_OUT_OF_MEMORY;
            }
        }
    }
    return token;
}

// Boşlukları ve Yorumları Atla
static int skip_whitespace(ParserContext* ctx) {
    for (;;) {
        char c = peek(ctx);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(ctx);
                break;
            case '\n':
                advance(ctx);
                break;
            case '#': // Omni Core Yorum Satırı
                while (peek(ctx) != '\n' && peek(ctx) != '\0') advance(ctx);
                break;
            case '/':
                if (peek_next(ctx, 1) == '*') {
                    size_t comment_start = ctx->pos;
                    int comment_line = ctx->current_line;
                    int comment_col = ctx->current_col;
                    int closed = 0;
                    advance(ctx);
                    advance(ctx);
                    while (peek(ctx) != '\0') {
                        if (peek(ctx) == '*' && peek_next(ctx, 1) == '/') {
                            advance(ctx);
                            advance(ctx);
                            closed = 1;
                            break;
                        }
                        advance(ctx);
                    }
                    if (!closed) {
                        ctx->token_start_offset = comment_start;
                        ctx->token_start_line = comment_line;
                        ctx->token_start_col = comment_col;
                        return 0;
                    }
                } else {
                    return 1;
                }
                break;
            default:
                return 1;
        }
    }
}

// Tanımlayıcı veya Anahtar Kelime Kontrolü
static OmniTokenType check_keyword(
    const char* start, size_t length, const char* rest, OmniTokenType type
) {
    if (length == strlen(rest) && memcmp(start, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

static OmniTokenType identifier_type(const char* start, size_t length) {
    /* These literals already have dedicated parser/AST support. Keep them
       ahead of the general keyword dispatch so they cannot become identifiers. */
    if (length == 4 && memcmp(start, "true", 4) == 0) return TOKEN_TRUE;
    if (length == 5 && memcmp(start, "false", 5) == 0) return TOKEN_FALSE;

    // Prefix Kontrolleri (v:, c:, vb.)
    if (length >= 2 && start[1] == ':') {
        switch (start[0]) {
            case 'v': return TOKEN_PRE_VAR;
            case 'c': return TOKEN_PRE_CONST;
            case 'm': return TOKEN_PRE_MUST;
            case 'r': return TOKEN_PRE_RES;
            case 't': return TOKEN_PRE_TYPE;
            case 'o': return TOKEN_PRE_OBJ;
            case 'f': return (length == 2) ? TOKEN_KW_FN : TOKEN_PRE_FUNC;
        }
    }
    
    if (length > 3 && start[0] == 'n' && start[1] == 't' && start[2] == ':') return TOKEN_PRE_NEWTYPE;
    if (length == 3 && memcmp(start, "nt:", 3) == 0) return TOKEN_PRE_NEWTYPE;
    if (length > 6 && memcmp(start, "struct:", 7) == 0) return TOKEN_PRE_STRUCT;
    if (length > 4 && memcmp(start, "enum:", 5) == 0) return TOKEN_PRE_ENUM;
    if (length > 5 && memcmp(start, "union:", 6) == 0) return TOKEN_PRE_UNION;
    
    // Fonksiyon Tanımları
    if (length == 4 && start[0] == 'e' && start[1] == 'x' && start[2] == 'f' && start[3] == ':') return TOKEN_KW_EXTERN;

    // Anahtar Kelimeler
    switch (start[0]) {
        case 'r': 
            if (length == 3 && memcmp(start, "ret", 3) == 0) return TOKEN_KW_RET;
            if (length == 3 && memcmp(start, "reg", 3) == 0) return TOKEN_KW_REG;
            if (length == 7 && memcmp(start, "receive", 7) == 0) return TOKEN_KW_RECEIVE;
            break;
        case 's': 
            if (length > 1 && start[1] == 't') {
                if (length == 3 && start[2] == 'r') return TOKEN_TYPE_STR; // str
                return check_keyword(start, length, "struct", TOKEN_KW_STRUCT);
            }
            if (length == 5 && memcmp(start, "spawn", 5) == 0) return TOKEN_KW_SPAWN;
            if (length == 6 && memcmp(start, "sizeof", 6) == 0) return TOKEN_KW_SIZEOF;
            if (length == 4 && memcmp(start, "swap", 4) == 0) return TOKEN_KW_SWAP;
            if (length == 4 && memcmp(start, "send", 4) == 0) return TOKEN_KW_SEND;
            break;
        case 'c':
            if (length == 4 && memcmp(start, "char", 4) == 0) return TOKEN_TYPE_CHAR;
            if (length == 8 && memcmp(start, "continue", 8) == 0) return TOKEN_KW_CONTINUE;
            if (length == 4 && memcmp(start, "cast", 4) == 0) return TOKEN_KW_CAST;
            if (length == 5 && memcmp(start, "clock", 5) == 0) return TOKEN_KW_CLOCK;
            break;
        case 'e': 
            if (length == 4 && memcmp(start, "enum", 4) == 0) return TOKEN_KW_ENUM;
            if (length == 4 && memcmp(start, "exit", 4) == 0) return TOKEN_KW_EXIT;
            if (length == 3 && memcmp(start, "exp", 3) == 0) return TOKEN_KW_EXP;
            if (length == 4 && memcmp(start, "echo", 4) == 0) return TOKEN_KW_ECHO;
            break;
        case 'u': 
            if (length > 1 && start[1] == 'n') return check_keyword(start, length, "union", TOKEN_KW_UNION);
            if (length == 2 && start[1] == '8') return TOKEN_TYPE_U8;
            if (length == 3 && start[1] == '1' && start[2] == '6') return TOKEN_TYPE_U16;
            if (length == 3 && start[1] == '3' && start[2] == '2') return TOKEN_TYPE_U32;
            if (length == 3 && start[1] == '6' && start[2] == '4') return TOKEN_TYPE_U64;
            if (length == 3 && start[1] == 's' && start[2] == 'e') return TOKEN_KW_USE;
            break;
        case 'g': return check_keyword(start, length, "group", TOKEN_KW_GROUP);
        case 'n': return check_keyword(start, length, "no", TOKEN_KW_NO);
        case 'b':
            if (length == 5 && memcmp(start, "break", 5) == 0) return TOKEN_KW_BREAK;
            if (length == 4 && memcmp(start, "bool", 4) == 0) return TOKEN_TYPE_BOOL;
            if (length == 3 && memcmp(start, "bit", 3) == 0) return TOKEN_TYPE_BIT;
            if (length == 4 && memcmp(start, "byte", 4) == 0) return TOKEN_TYPE_BYTE;
            break;
        case 'a':
            if (length == 3 && memcmp(start, "any", 3) == 0) return TOKEN_TYPE_ANY;
            if (length >= 3 && memcmp(start, "asm", 3) == 0) {
                if (length == 3) return TOKEN_KW_ASM;
                if (length == 7 && memcmp(start, "asmcall", 7) == 0) return TOKEN_KW_ASMCALL;
                if (length == 6 && memcmp(start, "asmjmp", 6) == 0) return TOKEN_KW_ASMJMP;
            }
            if (length == 4 && memcmp(start, "area", 4) == 0) return TOKEN_KW_AREA;
            if (length == 4 && memcmp(start, "addr", 4) == 0) return TOKEN_KW_ADDR;
            if (length == 2 && memcmp(start, "as", 2) == 0) return TOKEN_KW_AS;
            break;
        case 'd':
            if (length == 4 && memcmp(start, "done", 4) == 0) return TOKEN_KW_DONE;
            if (length == 5 && memcmp(start, "defer", 5) == 0) return TOKEN_KW_DEFER;
            if (length == 4 && memcmp(start, "date", 4) == 0) return TOKEN_KW_DATE;
            if (length == 7 && memcmp(start, "datenow", 7) == 0) return TOKEN_KW_DATENOW;
            break;
        case 'l': 
            if (length == 4 && memcmp(start, "loop", 4) == 0) return TOKEN_KW_LOOP;
            if (length == 6 && memcmp(start, "listen", 6) == 0) return TOKEN_KW_LISTEN;
            if (length == 3 && memcmp(start, "len", 3) == 0) return TOKEN_KW_LEN;
            break;
        case 'p':
            if (length == 4 && memcmp(start, "peek", 4) == 0) return TOKEN_KW_PEEK;
            if (length == 4 && memcmp(start, "poke", 4) == 0) return TOKEN_KW_POKE;
            if (length == 4 && memcmp(start, "prnt", 4) == 0) return TOKEN_KW_PRNT;
            if (length == 4 && memcmp(start, "prmt", 4) == 0) return TOKEN_KW_PRMT;
            if (length == 5 && memcmp(start, "panic", 5) == 0) return TOKEN_KW_PANIC;
            if (length == 3 && memcmp(start, "pup", 3) == 0) return TOKEN_KW_PUP;
            break;
        case 'i':
            if (length == 2 && start[1] == '8') return TOKEN_TYPE_I8;
            if (length == 3 && start[1] == '1' && start[2] == '6') return TOKEN_TYPE_I16;
            if (length == 3 && start[1] == '3' && start[2] == '2') return TOKEN_TYPE_I32;
            if (length == 3 && start[1] == '6' && start[2] == '4') return TOKEN_TYPE_I64;
            if (length == 3 && memcmp(start, "inb", 3) == 0) return TOKEN_KW_INB;
            if (length == 3 && memcmp(start, "irq", 3) == 0) return TOKEN_KW_IRQ;
            if (length == 4 && memcmp(start, "intr", 4) == 0) return TOKEN_KW_INTR;
            if (length == 5 && memcmp(start, "is_ok", 5) == 0) return TOKEN_KW_IS_OK;
            if (length == 6 && memcmp(start, "is_err", 6) == 0) return TOKEN_KW_IS_ERR;
            if (length == 7 && memcmp(start, "is_some", 7) == 0) return TOKEN_KW_IS_SOME;
            if (length == 7 && memcmp(start, "is_none", 7) == 0) return TOKEN_KW_IS_NONE;
            if (length == 7 && memcmp(start, "is_null", 7) == 0) return TOKEN_KW_IS_NULL;
            break;
        case 'o':
            if (length == 4 && memcmp(start, "outb", 4) == 0) return TOKEN_KW_OUTB;
            break;
        case 'w':
            if (length == 5 && memcmp(start, "write", 5) == 0) return TOKEN_KW_WRITE;
            break;
        case 'f':
            if (length == 6 && memcmp(start, "fwrite", 6) == 0) return TOKEN_KW_FWRITE;
            if (length == 4 && memcmp(start, "free", 4) == 0) return TOKEN_KW_FREE;
            if (length == 8 && memcmp(start, "fastexec", 8) == 0) return TOKEN_KW_FASTEXEC;
            if (length == 3 && memcmp(start, "f32", 3) == 0) return TOKEN_TYPE_F32;
            if (length == 3 && memcmp(start, "f64", 3) == 0) return TOKEN_TYPE_F64;
            break;
        case 't':
            if (length == 6 && memcmp(start, "typeof", 6) == 0) return TOKEN_KW_TYPEOF;
            if (length == 4 && memcmp(start, "time", 4) == 0) return TOKEN_KW_TIME;
            break;
        case 'h':
            if (length == 3 && memcmp(start, "hex", 3) == 0) return TOKEN_TYPE_HEX;
            break;
        case 'v':
            if (length == 4 && memcmp(start, "void", 4) == 0) return TOKEN_TYPE_VOID;
            break;
        case 'z':
            if (length == 4 && memcmp(start, "zone", 4) == 0) return TOKEN_KW_ZONE;
    }

    return TOKEN_IDENTIFIER;
}

Token lexer_next_token(ParserContext* ctx) {
    if (!skip_whitespace(ctx)) {
        return error_token(ctx, "Unterminated block comment");
    }

    ctx->token_start_offset = ctx->pos;
    ctx->token_start_line = ctx->current_line;
    ctx->token_start_col = ctx->current_col;

    if (ctx->pos >= ctx->source_len) return make_token(ctx, TOKEN_EOF, "EOF", 0);

    const char* start = &ctx->source_buffer[ctx->pos];
    char c = advance(ctx);

    // --- Tanımlayıcılar (Identifiers) ---
    if (isalpha((unsigned char)c) || c == '_') {
        while (isalnum((unsigned char)peek(ctx)) || peek(ctx) == '_' || peek(ctx) == ':') {
            // Fix: Only consume ':' if it is followed by an alphanumeric char or '_' (Prefix style)
            if (peek(ctx) == ':') {
                // Special case for "nt:" to allow "nt: struct"
                size_t len = (size_t)(&ctx->source_buffer[ctx->pos] - start);
                if (len == 2 && start[0] == 'n' && start[1] == 't') {
                    // Allow consuming ':' for nt: prefix even if followed by space
                } 
                // Allow single char prefixes (v:, c:, f:, etc.) followed by space
                else if (len == 1 && strchr("vcmrtof", start[0])) {
                    // Allow consuming ':'
                } else {
                    char next = peek_next(ctx, 1);
                    if (!(isalnum((unsigned char)next) || next == '_')) {
                        break; 
                    }
                }
            }
            advance(ctx);
        }
        size_t length = (size_t)(&ctx->source_buffer[ctx->pos] - start);
        return make_token(ctx, identifier_type(start, length), start, length);
    }

    // --- Sayılar ---
    if (isdigit((unsigned char)c)) {
        while (isdigit((unsigned char)peek(ctx))) advance(ctx);
        if (peek(ctx) == '.' && isdigit((unsigned char)peek_next(ctx, 1u))) {
            advance(ctx); // .
            while (isdigit((unsigned char)peek(ctx))) advance(ctx);
        }
        size_t length = (size_t)(&ctx->source_buffer[ctx->pos] - start);
        return make_token(ctx, TOKEN_NUMBER, start, length);
    }

    // --- Stringler ---
    if (c == '"') {
        while (peek(ctx) != '"' && peek(ctx) != '\0') {
            if (peek(ctx) == '\\') {
                advance(ctx);
                if (peek(ctx) == '\0') break;
                advance(ctx);
            } else {
                advance(ctx);
            }
        }
        if (peek(ctx) == '\0') return error_token(ctx, "Unterminated string literal");
        advance(ctx); // Kapanış tırnağı
        size_t length = (size_t)(&ctx->source_buffer[ctx->pos] - start);
        return make_token(ctx, TOKEN_STRING, start, length);
    }

    // --- Karakterler ---
    if (c == '\'') {
        if (peek(ctx) == '\0') return error_token(ctx, "Unterminated character literal");
        if (peek(ctx) == '\\') {
            advance(ctx); // Escape character.
            if (peek(ctx) == '\0') return error_token(ctx, "Unterminated character literal");
            advance(ctx); // Escaped character, including an escaped quote.
        } else {
            advance(ctx); // Normal karakter
        }

        if (peek(ctx) == '\'') {
            advance(ctx); // Kapanış tırnağı
            size_t length = (size_t)(&ctx->source_buffer[ctx->pos] - start);
            return make_token(ctx, TOKEN_CHAR, start, length);
        }
        return error_token(ctx, "Unterminated character literal");
    }

    // --- Operatörler ---
    switch (c) {
        case '(': 
            if (peek(ctx) == 'e' && peek_next(ctx, 1) == ')') {
                advance(ctx); // e
                advance(ctx); // )
                return make_token(ctx, TOKEN_ERR_HANDLER, start, 3);
            }
            return make_token(ctx, TOKEN_LPAREN, start, 1);
        case ')': return make_token(ctx, TOKEN_RPAREN, start, 1);
        case '{': return make_token(ctx, TOKEN_LBRACE, start, 1);
        case '}': return make_token(ctx, TOKEN_RBRACE, start, 1);
        case '[': return make_token(ctx, TOKEN_LBRACKET, start, 1);
        case ']': return make_token(ctx, TOKEN_RBRACKET, start, 1);
        case ',': return make_token(ctx, TOKEN_COMMA, start, 1);
        case ';': return make_token(ctx, TOKEN_SEMICOLON, start, 1);
        case ':': 
            if (match(ctx, '>')) return make_token(ctx, TOKEN_NEXUS_LABEL, start, 2); // :>
            return make_token(ctx, TOKEN_COLON, start, 1);
        
        case '.':
            if (match(ctx, '.')) {
                if (match(ctx, '.')) {
                    if (match(ctx, '>')) return make_token(ctx, TOKEN_UNPACK, start, 4); // ...>
                    return make_token(ctx, TOKEN_ELLIPSIS, start, 3); // ...
                }
                return make_token(ctx, TOKEN_RANGE, start, 2); // ..
            }
            if (match(ctx, '=')) return make_token(ctx, TOKEN_STRING_APPEND, start, 2); // .=
            return make_token(ctx, TOKEN_DOT, start, 1);

        case '=':
            if (match(ctx, '>')) return make_token(ctx, TOKEN_FAT_ARROW, start, 2); // =>
            if (match(ctx, '?')) {
                if (match(ctx, '>')) return make_token(ctx, TOKEN_IF_FLOW, start, 3); // =?>
            }
            if (match(ctx, '=')) {
                if (match(ctx, '=')) return make_token(ctx, TOKEN_STRICT_EQ, start, 3); // ===
                if (match(ctx, '!')) return make_token(ctx, TOKEN_FORCE_EQ, start, 3); // ==!
                return make_token(ctx, TOKEN_EQ, start, 2); // ==
            }
            return make_token(ctx, TOKEN_ASSIGN, start, 1);

        case '!':
            if (match(ctx, '-')) {
                if (match(ctx, '>')) return make_token(ctx, TOKEN_IGNORE, start, 3); // !->
            }
            if (match(ctx, '!')) {
                if (match(ctx, '=')) return make_token(ctx, TOKEN_FORCE_ASSIGN, start, 3); // !!=
                return make_token(ctx, TOKEN_FORCE, start, 2); // !!
            }
            if (match(ctx, '=')) return make_token(ctx, TOKEN_NEQ, start, 2); // !=
            return make_token(ctx, TOKEN_NOT, start, 1);

        case '?':
            if (match(ctx, '-')) {
                if (match(ctx, '>')) return make_token(ctx, TOKEN_ELSE_FLOW, start, 3); // ?->
            }
            if (match(ctx, '=')) {
                if (match(ctx, '>')) return make_token(ctx, TOKEN_FALLBACK, start, 3); // ?=>
            }
            if (match(ctx, '>')) return make_token(ctx, TOKEN_JUMP, start, 2); // ?>
            if (match(ctx, ';')) return make_token(ctx, TOKEN_HALT, start, 2); // ?;
            if (match(ctx, '?')) return make_token(ctx, TOKEN_AI, start, 2); // ??
            return make_token(ctx, TOKEN_QMARK, start, 1); // ?

        case '-':
            if (match(ctx, '>')) return make_token(ctx, TOKEN_PIPE, start, 2); // ->
            if (match(ctx, '=')) return make_token(ctx, TOKEN_MINUS_EQ, start, 2); // -=
            if (match(ctx, '-')) return make_token(ctx, TOKEN_DEC, start, 2); // --
            return make_token(ctx, TOKEN_MINUS, start, 1);

        case '+':
            if (match(ctx, '=')) return make_token(ctx, TOKEN_PLUS_EQ, start, 2); // +=
            if (match(ctx, '+')) return make_token(ctx, TOKEN_INC, start, 2); // ++
            return make_token(ctx, TOKEN_PLUS, start, 1);

        case '<':
            if (match(ctx, '-')) {
                if (match(ctx, '-')) return make_token(ctx, TOKEN_ITERATE, start, 3); // <--
                return make_token(ctx, TOKEN_CAPTURE, start, 2); // <-
            }
            if (match(ctx, '<')) return make_token(ctx, TOKEN_LSHIFT, start, 2); // <<
            if (match(ctx, '=')) return make_token(ctx, TOKEN_LTE, start, 2); // <=
            return make_token(ctx, TOKEN_LT, start, 1);

        case '>':
            if (match(ctx, '{')) return make_token(ctx, TOKEN_THREAD_BLOCK, start, 2); // >{
            if (match(ctx, '>')) return make_token(ctx, TOKEN_RSHIFT, start, 2); // >>
            if (match(ctx, '=')) return make_token(ctx, TOKEN_GTE, start, 2); // >=
            return make_token(ctx, TOKEN_GT, start, 1);

        case '_':
            if (match(ctx, '>')) return make_token(ctx, TOKEN_RELOCATE, start, 2); // _>
            return make_token(ctx, TOKEN_IDENTIFIER, start, 1);

        case '^':
            if (match(ctx, '^')) return make_token(ctx, TOKEN_POWER, start, 2); // ^^
            if (match(ctx, '=')) return make_token(ctx, TOKEN_BIT_XOR_EQ, start, 2); // ^=
            return make_token(ctx, TOKEN_BIT_XOR, start, 1);

        case '@': return make_token(ctx, TOKEN_INTENT, start, 1);
        case '*': 
            if (match(ctx, '=')) return make_token(ctx, TOKEN_STAR_EQ, start, 2);
            return make_token(ctx, TOKEN_STAR, start, 1);
        case '/': 
            if (match(ctx, '=')) return make_token(ctx, TOKEN_SLASH_EQ, start, 2);
            return make_token(ctx, TOKEN_SLASH, start, 1);
        case '%': return make_token(ctx, TOKEN_PERCENT, start, 1);
        case '&': 
            if (match(ctx, '&')) return make_token(ctx, TOKEN_AND, start, 2);
            if (match(ctx, '=')) return make_token(ctx, TOKEN_BIT_AND_EQ, start, 2); // &=
            return make_token(ctx, TOKEN_BIT_AND, start, 1);
        case '|': 
            if (match(ctx, '|')) return make_token(ctx, TOKEN_OR, start, 2);
            if (match(ctx, '=')) return make_token(ctx, TOKEN_BIT_OR_EQ, start, 2); // |=
            return make_token(ctx, TOKEN_BIT_OR, start, 1);
        case '~': return make_token(ctx, TOKEN_BIT_NOT, start, 1);
    }

    return error_token(ctx, msg_get(MSG_UNEXPECTED_CHAR));
}

const char* token_type_to_string(OmniTokenType type) {
    switch(type) {
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_CHAR: return "CHAR";
        case TOKEN_STRING: return "STRING";
        case TOKEN_CAPTURE: return "CAPTURE (<-)";
        case TOKEN_PIPE: return "PIPE (->)";
        case TOKEN_IF_FLOW: return "IF_FLOW (=?>)";
        case TOKEN_ELSE_FLOW: return "ELSE_FLOW (?->)";
        case TOKEN_FALLBACK: return "FALLBACK (?=>)";
        case TOKEN_IGNORE: return "IGNORE (!->)";
        case TOKEN_FAT_ARROW: return "FAT_ARROW (=>)";
        case TOKEN_ITERATE: return "ITERATE (<--)";
        case TOKEN_JUMP: return "JUMP (?>)";
        case TOKEN_NEXUS_LABEL: return "LABEL (:>)";
        case TOKEN_HALT: return "HALT (?;)";
        case TOKEN_QMARK: return "?";
        case TOKEN_INTENT: return "INTENT (@)";
        case TOKEN_AI: return "AI (\?\?)";
        case TOKEN_TRUE: return "TRUE";
        case TOKEN_FALSE: return "FALSE";
        case TOKEN_FORCE_ASSIGN: return "FORCE_ASSIGN (!!=)";
        case TOKEN_STRING_APPEND: return "APPEND (.=)";
        case TOKEN_RANGE: return "RANGE (..)";
        case TOKEN_ELLIPSIS: return "ELLIPSIS (...)";
        case TOKEN_FORCE: return "FORCE (!!)";
        case TOKEN_STRICT_EQ: return "STRICT_EQ (===)";
        case TOKEN_FORCE_EQ: return "FORCE_EQ (==!)";
        case TOKEN_LSHIFT: return "STREAM_IN (<<)";
        case TOKEN_RSHIFT: return "STREAM_OUT (>>)";
        case TOKEN_UNPACK: return "UNPACK (...>)";
        case TOKEN_RELOCATE: return "RELOCATE (_>)";
        case TOKEN_KW_FN: return "FN (f:)";
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_ASSIGN: return "=";
        case TOKEN_BIT_AND_EQ: return "&=";
        case TOKEN_BIT_OR_EQ: return "|=";
        case TOKEN_BIT_XOR_EQ: return "^=";
        case TOKEN_KW_LOOP: return "LOOP";
        default: return "TOKEN";
    }
}