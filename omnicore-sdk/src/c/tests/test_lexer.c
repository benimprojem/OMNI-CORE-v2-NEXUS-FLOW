#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char* source = "v:x = 12;\n@ !!= -> ?-> ?=> !-> << >> ...> name";
    const OmniTokenType expected[] = {
        TOKEN_PRE_VAR, TOKEN_ASSIGN, TOKEN_NUMBER, TOKEN_SEMICOLON,
        TOKEN_INTENT, TOKEN_FORCE_ASSIGN, TOKEN_PIPE,
        TOKEN_ELSE_FLOW, TOKEN_FALLBACK, TOKEN_IGNORE, TOKEN_LSHIFT,
        TOKEN_RSHIFT, TOKEN_UNPACK, TOKEN_IDENTIFIER, TOKEN_EOF
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
        if (i == 4 && (token.line != 2 || token.col != 1)) {
            fprintf(stderr, "Intent token has incorrect source position: %d:%d\n",
                token.line, token.col);
            free(ctx.source_buffer);
            return 1;
        }
    }

    free(ctx.source_buffer);
    return 0;
}
