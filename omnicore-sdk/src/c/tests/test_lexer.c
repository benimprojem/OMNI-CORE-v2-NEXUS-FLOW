#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
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

    return 0;
}
