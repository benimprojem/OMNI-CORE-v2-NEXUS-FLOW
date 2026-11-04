#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expect_invalid_numeric_literal(const char* source) {
    ParserContext ctx;
    Token token;
    size_t length;
    if (!source) return 0;
    length = strlen(source);
    memset(&ctx, 0, sizeof(ctx));
    ctx.source_buffer = (char*)malloc(length + 1u);
    if (!ctx.source_buffer) return 0;
    memcpy(ctx.source_buffer, source, length + 1u);
    ctx.source_len = length;
    ctx.current_line = 1;
    ctx.current_col = 1;

    token = lexer_next_token(&ctx);
    if (token.type != TOKEN_ERROR || token.source_offset != 0u ||
        token.source_length != length || ctx.pos != length ||
        lexer_next_token(&ctx).type != TOKEN_EOF) {
        fprintf(stderr, "Unsupported numeric literal was not rejected as one token: %s\n", source);
        free(ctx.source_buffer);
        return 0;
    }
    free(ctx.source_buffer);
    return 1;
}

static int test_opaque_asm_body_token(void) {
    static const char source[] =
        "asm: ADD {\n"
        "  mov rax, %x:reg\r\n"
        "  ldm r0, {r1, r2}\n"
        "  db \"}\"\n"
        "}";
    ParserContext ctx;
    Token token;
    const char* raw_start;
    const char* raw_end;
    size_t raw_length;
    memset(&ctx,0,sizeof(ctx));
    ctx.source_buffer=(char*)source;
    ctx.source_len=sizeof(source)-1u;
    ctx.current_line=1;
    ctx.current_col=1;

    token=lexer_next_token(&ctx);
    if(token.type!=TOKEN_KW_ASM) return 0;
    token=lexer_next_token(&ctx);
    if(token.type!=TOKEN_COLON) return 0;
    token=lexer_next_token(&ctx);
    if(token.type!=TOKEN_IDENTIFIER || token.length!=3 || memcmp(token.start,"ADD",3u)!=0) return 0;
    token=lexer_next_token(&ctx);
    if(token.type!=TOKEN_LBRACE) return 0;
    raw_start=strchr(source,'{')+1;
    raw_end=strrchr(source,'}');
    raw_length=(size_t)(raw_end-raw_start);
    token=lexer_next_token(&ctx); /* repeated token request must preserve pending raw body */
    if(token.type!=TOKEN_ASM_BODY || token.start!=raw_start ||
        token.source_offset!=(size_t)(raw_start-source) ||
        token.source_length!=raw_length || token.length!=(int)raw_length ||
        memcmp(token.start,raw_start,raw_length)!=0) {
        fprintf(stderr,"Raw ASM body was not emitted losslessly.\n");
        return 0;
    }
    token=lexer_next_token(&ctx);
    if(token.type!=TOKEN_RBRACE || token.source_offset!=(size_t)(raw_end-source)) {
        fprintf(stderr,"Raw ASM closing brace was not preserved as a delimiter.\n");
        return 0;
    }
    return lexer_next_token(&ctx).type==TOKEN_EOF;
}


static int test_unterminated_asm_body(void) {
    static const char source[]="asm: BROKEN { mov rax, %value";
    ParserContext ctx;
    Token token;
    const char* body=strchr(source,'{')+1;
    memset(&ctx,0,sizeof(ctx));
    ctx.source_buffer=(char*)source;
    ctx.source_len=sizeof(source)-1u;
    ctx.current_line=1;
    ctx.current_col=1;
    if(lexer_next_token(&ctx).type!=TOKEN_KW_ASM ||
        lexer_next_token(&ctx).type!=TOKEN_COLON ||
        lexer_next_token(&ctx).type!=TOKEN_IDENTIFIER ||
        lexer_next_token(&ctx).type!=TOKEN_LBRACE) return 0;
    token=lexer_next_token(&ctx);
    return token.type==TOKEN_ERROR &&
        token.source_offset==(size_t)(body-source) &&
        token.source_length==sizeof(source)-1u-(size_t)(body-source) &&
        ctx.pos==ctx.source_len;
}

int main(void) {
    if (!test_opaque_asm_body_token() || !test_unterminated_asm_body()) return 1;
    const char* source = "v:x = 12;\n@ !!= -> ?-> ?=> !-> << >> ...> name asm fastexec break echo send receive f32 f64 bool bit byte hex void any is_err true false";
    const OmniTokenType expected[] = {
        TOKEN_PRE_VAR, TOKEN_ASSIGN, TOKEN_NUMBER, TOKEN_SEMICOLON,
        TOKEN_INTENT, TOKEN_FORCE_ASSIGN, TOKEN_PIPE,
        TOKEN_ELSE_FLOW, TOKEN_FALLBACK, TOKEN_IGNORE, TOKEN_LSHIFT,
        TOKEN_RSHIFT, TOKEN_UNPACK, TOKEN_IDENTIFIER,
        TOKEN_KW_ASM, TOKEN_KW_FASTEXEC, TOKEN_KW_BREAK,
        TOKEN_KW_ECHO, TOKEN_KW_SEND, TOKEN_KW_RECEIVE,
        TOKEN_TYPE_F32, TOKEN_TYPE_F64, TOKEN_TYPE_BOOL,
        TOKEN_TYPE_BIT, TOKEN_TYPE_BYTE, TOKEN_TYPE_HEX,
        TOKEN_TYPE_VOID, TOKEN_TYPE_ANY, TOKEN_KW_IS_ERR,
        TOKEN_TRUE, TOKEN_FALSE, TOKEN_EOF
    };

    ParserContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    size_t length = strlen(source);
    ctx.source_buffer = (char*)malloc(length + 1u);
    if (!ctx.source_buffer) return 1;
    memcpy(ctx.source_buffer, source, length + 1u);
    ctx.source_len = length;
    ctx.current_line = 1;
    ctx.current_col = 1;

    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        Token token = lexer_next_token(&ctx);
        if (token.type != expected[i]) {
            fprintf(stderr, "Token %zu: expected %s, got %s at %d:%d\n",
                i, token_type_to_string(expected[i]),
                token_type_to_string(token.type), token.line, token.col);
            free(ctx.source_buffer);
            return 1;
        }
        if (i == 4 && (token.line != 2 || token.col != 1 ||
            token.source_offset != 10u || token.source_length != 1u)) {
            fprintf(stderr, "Intent token has incorrect source span: %d:%d offset=%zu length=%zu\n",
                token.line, token.col, token.source_offset, token.source_length);
            free(ctx.source_buffer);
            return 1;
        }
    }

    free(ctx.source_buffer);

    {
        const char* multiline = "\"first\nsecond\" name";
        ParserContext multi_ctx;
        memset(&multi_ctx, 0, sizeof(multi_ctx));
        multi_ctx.source_len = strlen(multiline);
        multi_ctx.source_buffer = (char*)malloc(multi_ctx.source_len + 1u);
        if (!multi_ctx.source_buffer) return 1;
        memcpy(multi_ctx.source_buffer, multiline, multi_ctx.source_len + 1u);
        multi_ctx.current_line = 1;
        multi_ctx.current_col = 1;

        Token string_token = lexer_next_token(&multi_ctx);
        if (string_token.type != TOKEN_STRING || string_token.line != 1 ||
            string_token.col != 1 || string_token.source_offset != 0u ||
            string_token.source_length != 14u) {
            fprintf(stderr, "Multiline string source span/location is incorrect.\n");
            free(multi_ctx.source_buffer);
            return 1;
        }

        Token identifier = lexer_next_token(&multi_ctx);
        if (identifier.type != TOKEN_IDENTIFIER || identifier.line != 2 ||
            identifier.col != 9 || identifier.source_offset != 15u) {
            fprintf(stderr, "Token after multiline string has incorrect location: %d:%d offset=%zu\n",
                identifier.line, identifier.col, identifier.source_offset);
            free(multi_ctx.source_buffer);
            return 1;
        }
        free(multi_ctx.source_buffer);
    }

    {
        const char* unterminated = "/* comment without terminator";
        ParserContext comment_ctx;
        memset(&comment_ctx, 0, sizeof(comment_ctx));
        comment_ctx.source_len = strlen(unterminated);
        comment_ctx.source_buffer = (char*)malloc(comment_ctx.source_len + 1u);
        if (!comment_ctx.source_buffer) return 1;
        memcpy(comment_ctx.source_buffer, unterminated, comment_ctx.source_len + 1u);
        comment_ctx.current_line = 1;
        comment_ctx.current_col = 1;

        Token error = lexer_next_token(&comment_ctx);
        if (error.type != TOKEN_ERROR || error.line != 1 || error.col != 1 ||
            error.source_offset != 0u ||
            error.source_length != strlen(unterminated)) {
            fprintf(stderr, "Unterminated block comment was not reported with its source span.\n");
            free(comment_ctx.source_buffer);
            return 1;
        }
        if (lexer_next_token(&comment_ctx).type != TOKEN_EOF) {
            fprintf(stderr, "Lexer did not reach EOF after unterminated comment diagnostic.\n");
            free(comment_ctx.source_buffer);
            return 1;
        }
        free(comment_ctx.source_buffer);
    }

    {
        const char escaped_source[] = "\"a\\\"b\" name";
        ParserContext escaped_ctx;
        Token string_token, identifier;
        memset(&escaped_ctx, 0, sizeof(escaped_ctx));
        escaped_ctx.source_len = sizeof(escaped_source) - 1u;
        escaped_ctx.source_buffer = (char*)malloc(sizeof(escaped_source));
        if (!escaped_ctx.source_buffer) return 1;
        memcpy(escaped_ctx.source_buffer, escaped_source, sizeof(escaped_source));
        escaped_ctx.current_line = 1;
        escaped_ctx.current_col = 1;
        string_token = lexer_next_token(&escaped_ctx);
        identifier = lexer_next_token(&escaped_ctx);
        if (string_token.type != TOKEN_STRING || string_token.source_length != 6u ||
            identifier.type != TOKEN_IDENTIFIER || identifier.source_offset != 7u) {
            fprintf(stderr, "Escaped quote incorrectly terminated a string literal.\n");
            free(escaped_ctx.source_buffer);
            return 1;
        }
        free(escaped_ctx.source_buffer);
    }

    {
        const char trailing_escape[] = "'\\";
        ParserContext char_ctx;
        Token error, eof;
        memset(&char_ctx, 0, sizeof(char_ctx));
        char_ctx.source_len = sizeof(trailing_escape) - 1u;
        char_ctx.source_buffer = (char*)malloc(sizeof(trailing_escape));
        if (!char_ctx.source_buffer) return 1;
        memcpy(char_ctx.source_buffer, trailing_escape, sizeof(trailing_escape));
        char_ctx.current_line = 1;
        char_ctx.current_col = 1;
        error = lexer_next_token(&char_ctx);
        eof = lexer_next_token(&char_ctx);
        if (error.type != TOKEN_ERROR || error.source_offset != 0u ||
            error.source_length != char_ctx.source_len || char_ctx.pos != char_ctx.source_len ||
            eof.type != TOKEN_EOF) {
            fprintf(stderr, "Trailing character escape advanced beyond EOF.\n");
            free(char_ctx.source_buffer);
            return 1;
        }
        free(char_ctx.source_buffer);
    }

    {
        const char embedded_nul[] = { 'x', '\0', 'y' };
        ParserContext nul_ctx;
        Token before_nul, nul_error, after_nul, eof;
        memset(&nul_ctx, 0, sizeof(nul_ctx));
        nul_ctx.source_len = sizeof(embedded_nul);
        nul_ctx.source_buffer = (char*)malloc(sizeof(embedded_nul) + 1u);
        if (!nul_ctx.source_buffer) return 1;
        memcpy(nul_ctx.source_buffer, embedded_nul, sizeof(embedded_nul));
        nul_ctx.source_buffer[sizeof(embedded_nul)] = '\0';
        nul_ctx.current_line = 1;
        nul_ctx.current_col = 1;
        before_nul = lexer_next_token(&nul_ctx);
        nul_error = lexer_next_token(&nul_ctx);
        after_nul = lexer_next_token(&nul_ctx);
        eof = lexer_next_token(&nul_ctx);
        if (before_nul.type != TOKEN_IDENTIFIER ||
            nul_error.type != TOKEN_ERROR || nul_error.source_offset != 1u ||
            nul_error.source_length != 1u ||
            after_nul.type != TOKEN_IDENTIFIER || after_nul.source_offset != 2u ||
            eof.type != TOKEN_EOF) {
            fprintf(stderr, "Embedded NUL silently truncated or corrupted the source stream.\\n");
            free(nul_ctx.source_buffer);
            return 1;
        }
        free(nul_ctx.source_buffer);
    }

    {
        const char* numeric_source = "0 12 3.14 1..5";
        const OmniTokenType numeric_expected[] = {
            TOKEN_NUMBER, TOKEN_NUMBER, TOKEN_NUMBER,
            TOKEN_NUMBER, TOKEN_RANGE, TOKEN_NUMBER, TOKEN_EOF
        };
        ParserContext numeric_ctx;
        memset(&numeric_ctx, 0, sizeof(numeric_ctx));
        numeric_ctx.source_len = strlen(numeric_source);
        numeric_ctx.source_buffer = (char*)malloc(numeric_ctx.source_len + 1u);
        if (!numeric_ctx.source_buffer) return 1;
        memcpy(numeric_ctx.source_buffer, numeric_source, numeric_ctx.source_len + 1u);
        numeric_ctx.current_line = 1;
        numeric_ctx.current_col = 1;
        for (size_t i = 0; i < sizeof(numeric_expected) / sizeof(numeric_expected[0]); ++i) {
            Token token = lexer_next_token(&numeric_ctx);
            if (token.type != numeric_expected[i]) {
                fprintf(stderr, "Numeric grammar token %zu: expected %s, got %s\n",
                    i, token_type_to_string(numeric_expected[i]),
                    token_type_to_string(token.type));
                free(numeric_ctx.source_buffer);
                return 1;
            }
        }
        free(numeric_ctx.source_buffer);
    }

    {
        const char* delay_source = "500ms";
        ParserContext delay_ctx;
        Token number, unit, eof;
        memset(&delay_ctx, 0, sizeof(delay_ctx));
        delay_ctx.source_len = strlen(delay_source);
        delay_ctx.source_buffer = (char*)malloc(delay_ctx.source_len + 1u);
        if (!delay_ctx.source_buffer) return 1;
        memcpy(delay_ctx.source_buffer, delay_source, delay_ctx.source_len + 1u);
        delay_ctx.current_line = 1;
        delay_ctx.current_col = 1;
        number = lexer_next_token(&delay_ctx);
        unit = lexer_next_token(&delay_ctx);
        eof = lexer_next_token(&delay_ctx);
        if (number.type != TOKEN_NUMBER || number.source_length != 3u ||
            unit.type != TOKEN_IDENTIFIER || unit.source_length != 2u ||
            strncmp(unit.start, "ms", 2u) != 0 || eof.type != TOKEN_EOF) {
            fprintf(stderr, "Rolling millisecond suffix was not preserved as NUMBER + identifier.\n");
            free(delay_ctx.source_buffer);
            return 1;
        }
        free(delay_ctx.source_buffer);
    }

    {
        const char* invalid_numeric[] = {
            "0xFF", "0b1010", "10E3", "1e-3", "1_000", "12u8", "1.5f32"
        };
        for (size_t i = 0; i < sizeof(invalid_numeric) / sizeof(invalid_numeric[0]); ++i) {
            if (!expect_invalid_numeric_literal(invalid_numeric[i])) return 1;
        }
    }


    {
        static const char lexical_source[] = "\xC2\xA7;";
        ParserContext lexical_ctx;
        OccDiagnosticCollector diagnostics;
        OccSourceFileTable source_files;
        OccDiagnosticView view;
        uint32_t source_id = 0u;
        Token error;
        memset(&lexical_ctx, 0, sizeof(lexical_ctx));
        occ_diagnostics_init(&diagnostics);
        occ_source_files_init(&source_files);
        lexical_ctx.filename = "lib/lexer-input.nxf";
        lexical_ctx.source_len = sizeof(lexical_source) - 1u;
        lexical_ctx.source_buffer = (char*)malloc(sizeof(lexical_source));
        if (!lexical_ctx.source_buffer) {
            occ_source_files_destroy(&source_files);
            occ_diagnostics_destroy(&diagnostics);
            return 1;
        }
        memcpy(lexical_ctx.source_buffer, lexical_source, sizeof(lexical_source));
        lexical_ctx.current_line = 1;
        lexical_ctx.current_col = 1;
        lexical_ctx.diagnostics = &diagnostics;
        if (!parser_attach_source_files(&lexical_ctx, &source_files) ||
            !occ_source_files_get(&source_files, lexical_ctx.source_id) ||
            strcmp(occ_source_files_get(&source_files, lexical_ctx.source_id),
                "lib/lexer-input.nxf") != 0) {
            free(lexical_ctx.source_buffer);
            occ_source_files_destroy(&source_files);
            occ_diagnostics_destroy(&diagnostics);
            return 1;
        }
        source_id = lexical_ctx.source_id;
        error = lexer_next_token(&lexical_ctx);
        if (error.type != TOKEN_ERROR || error.diagnostic_id == 0u ||
            !occ_diagnostics_get(&diagnostics, 0u, &view) ||
            view.record->span.source_id != source_id ||
            view.record->span.start_line != 1u ||
            view.record->span.start_column != 1u) {
            fprintf(stderr, "Lexer diagnostic lost its registered source identity.\\n");
            free(lexical_ctx.source_buffer);
            occ_source_files_destroy(&source_files);
            occ_diagnostics_destroy(&diagnostics);
            return 1;
        }
        free(lexical_ctx.source_buffer);
        occ_source_files_destroy(&source_files);
        occ_diagnostics_destroy(&diagnostics);
    }

    return 0;
}
