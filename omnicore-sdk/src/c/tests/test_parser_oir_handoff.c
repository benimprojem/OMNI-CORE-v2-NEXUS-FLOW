#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include "oir_token_stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    ParserContext* context;
    Token* frontend_tokens = NULL;
    OirTokenStreamInput* inputs = NULL;
    Token* parser_tokens = NULL;
    uint8_t* payload = NULL;
    size_t count = 0, capacity = 0, payload_length = 0;
    OirTokenStreamPayloadView view;
    int result = 1;

    if (argc != 2) return 2;
    context = parser_init(argv[1]);
    if (!context) {
        fprintf(stderr, "Could not initialize source for OIR parser handoff.\n");
        return 1;
    }

    capacity = 32;
    frontend_tokens = (Token*)malloc(capacity * sizeof(*frontend_tokens));
    if (!frontend_tokens) goto cleanup;

    for (;;) {
        Token token = lexer_next_token(context);
        if (count == capacity) {
            size_t next_capacity = capacity * 2u;
            Token* next = (Token*)realloc(
                frontend_tokens, next_capacity * sizeof(*frontend_tokens));
            if (!next) goto cleanup;
            frontend_tokens = next;
            capacity = next_capacity;
        }
        frontend_tokens[count++] = token;
        if (token.type == TOKEN_EOF) break;
        if (token.type == TOKEN_ERROR) {
            fprintf(stderr, "Valid fixture unexpectedly produced a lexer error.\n");
            goto cleanup;
        }
        if (count > 100000u) goto cleanup;
    }

    inputs = (OirTokenStreamInput*)calloc(count, sizeof(*inputs));
    if (!inputs) goto cleanup;
    for (size_t i = 0; i < count; ++i) {
        if (oir_token_stream_input_from_lexer_token(&frontend_tokens[i], &inputs[i]) !=
            OIR_STATUS_OK) {
            fprintf(stderr, "Could not convert frontend token %zu to OIR.\n", i);
            goto cleanup;
        }
    }

    payload_length = OIR_TOKEN_STREAM_HEADER_SIZE +
        count * OIR_TOKEN_STREAM_RECORD_SIZE;
    payload = (uint8_t*)malloc(payload_length);
    if (!payload) goto cleanup;
    if (oir_token_stream_write(payload, payload_length, 0,
        (uint64_t)context->source_len, 0, inputs, (uint32_t)count,
        &payload_length) != OIR_STATUS_OK) {
        fprintf(stderr, "Could not serialize OIR TOKEN_STREAM.\n");
        goto cleanup;
    }

    if (oir_token_stream_open(payload, payload_length, 0,
        (uint64_t)context->source_len, 0, &view) != OIR_STATUS_OK) {
        fprintf(stderr, "Could not validate serialized OIR TOKEN_STREAM.\n");
        goto cleanup;
    }

    parser_tokens = (Token*)calloc(count, sizeof(*parser_tokens));
    if (!parser_tokens) goto cleanup;
    for (size_t i = 0; i < count; ++i) {
        OirTokenStreamRecordView record;
        if (oir_token_stream_get(&view, (uint32_t)i, &record) != OIR_STATUS_OK ||
            oir_token_stream_record_to_lexer_token(
                &record, (const uint8_t*)context->source_buffer,
                (uint64_t)context->source_len, NULL, 0,
                &parser_tokens[i]) != OIR_STATUS_OK) {
            fprintf(stderr, "Could not reconstruct parser token %zu from OIR.\n", i);
            goto cleanup;
        }
    }

    if (!parser_attach_token_stream(context, parser_tokens, count)) {
        fprintf(stderr, "Parser rejected a valid terminal-EOF token stream.\n");
        goto cleanup;
    }
    if (parser_run(context) != 0 || !context->ast_root ||
        ((ASTNode*)context->ast_root)->type != AST_PROGRAM) {
        fprintf(stderr, "Parser failed to consume the OIR token stream.\n");
        goto cleanup;
    }

    result = 0;

cleanup:
    free(parser_tokens);
    free(payload);
    free(inputs);
    free(frontend_tokens);
    parser_destroy(context);
    return result;
}
