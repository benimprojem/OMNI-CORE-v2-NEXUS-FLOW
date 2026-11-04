#include "oir_token_stream.h"
#include "oir_token_map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__); \
        free(buffer); \
        return 1; \
    } \
} while (0)

int main(void) {
    OirTokenStreamInput tokens[5];
    OirTokenStreamPayloadView view;
    OirTokenStreamRecordView record;
    uint8_t* buffer = NULL;
    size_t capacity = OIR_TOKEN_STREAM_HEADER_SIZE +
        5u * OIR_TOKEN_STREAM_RECORD_SIZE;
    size_t written = 0;

    {
        Token frontend = {0};
        OirTokenStreamInput converted;
        frontend.type = TOKEN_ERROR;
        frontend.start = "invalid byte";
        frontend.length = 12;
        frontend.line = 1;
        frontend.col = 4;
        frontend.source_offset = 3;
        frontend.source_length = 1;
        frontend.diagnostic_id = 1;
        CHECK(oir_token_stream_input_from_lexer_token(&frontend, &converted) ==
            OIR_STATUS_OK, "convert frontend lexer error token");
        CHECK(converted.token_id == oir_token_id_from_omni(TOKEN_ERROR) &&
            converted.diagnostic_id == 1 && converted.source_offset == 3 &&
            converted.lexeme_length == 1,
            "preserve lexer error span and diagnostic reference");

        frontend.type = TOKEN_IDENTIFIER;
        frontend.length = 1;
        frontend.source_offset = 0;
        frontend.source_length = 1;
        frontend.diagnostic_id = 0;
        CHECK(oir_token_stream_input_from_lexer_token(&frontend, &converted) ==
            OIR_STATUS_OK && converted.token_id ==
                oir_token_id_from_omni(TOKEN_IDENTIFIER),
            "convert ordinary frontend token");
    }

    memset(tokens, 0, sizeof(tokens));
    tokens[0].token_id = oir_token_id_from_omni(TOKEN_IDENTIFIER);
    tokens[0].source_offset = 0;
    tokens[0].lexeme_length = 1;
    tokens[0].line = 1;
    tokens[0].column = 1;

    tokens[1].token_id = oir_token_id_from_omni(TOKEN_ASSIGN);
    tokens[1].source_offset = 2;
    tokens[1].lexeme_length = 1;
    tokens[1].line = 1;
    tokens[1].column = 3;

    tokens[2].token_id = oir_token_id_from_omni(TOKEN_NUMBER);
    tokens[2].source_offset = 4;
    tokens[2].lexeme_length = 2;
    tokens[2].line = 1;
    tokens[2].column = 5;

    tokens[3].token_id = oir_token_id_from_omni(TOKEN_SEMICOLON);
    tokens[3].source_offset = 6;
    tokens[3].lexeme_length = 1;
    tokens[3].line = 1;
    tokens[3].column = 7;

    tokens[4].token_id = oir_token_id_from_omni(TOKEN_EOF);
    tokens[4].source_offset = 8;
    tokens[4].line = 2;
    tokens[4].column = 1;

    buffer = (uint8_t*)malloc(capacity);
    CHECK(buffer != NULL, "allocate output buffer");

    CHECK(oir_token_stream_write(buffer, capacity, 0, 8, 0,
        tokens, 5, &written) == OIR_STATUS_OK && written == capacity,
        "write valid token stream");
    CHECK(oir_token_stream_open(buffer, written, 0, 8, 0, &view) == OIR_STATUS_OK,
        "open valid token stream");

    /* A malformed serialized stream must not bypass coordinate ordering. */
    buffer[OIR_TOKEN_STREAM_HEADER_SIZE + 2u * OIR_TOKEN_STREAM_RECORD_SIZE + 24u] = 2u;
    buffer[OIR_TOKEN_STREAM_HEADER_SIZE + 2u * OIR_TOKEN_STREAM_RECORD_SIZE + 25u] = 0u;
    buffer[OIR_TOKEN_STREAM_HEADER_SIZE + 2u * OIR_TOKEN_STREAM_RECORD_SIZE + 26u] = 0u;
    buffer[OIR_TOKEN_STREAM_HEADER_SIZE + 2u * OIR_TOKEN_STREAM_RECORD_SIZE + 27u] = 0u;
    CHECK(oir_token_stream_open(buffer, written, 0, 8, 0, &view) ==
        OIR_STATUS_INVALID_SECTION,
        "reject serialized token coordinates that move backwards");
    CHECK(oir_token_stream_write(buffer, capacity, 0, 8, 0,
        tokens, 5, &written) == OIR_STATUS_OK,
        "restore valid token stream after malformed input");
    CHECK(oir_token_stream_open(buffer, written, 0, 8, 0, &view) == OIR_STATUS_OK,
        "reopen restored token stream");
    CHECK(view.token_count == 5 && view.source_id == 0,
        "token stream header should round-trip");

    CHECK(oir_token_stream_get(&view, 2, &record) == OIR_STATUS_OK,
        "read middle token");
    CHECK(record.token_id == oir_token_id_from_omni(TOKEN_NUMBER) &&
        record.source_offset == 4 && record.lexeme_length == 2 &&
        record.line == 1 && record.column == 5,
        "token record fields should round-trip");

    CHECK(oir_token_stream_write(buffer, capacity - 1u, 0, 8, 0,
        tokens, 5, &written) == OIR_STATUS_BUFFER_TOO_SMALL,
        "reject undersized output buffer");
    CHECK(oir_token_stream_open(buffer, capacity - 1u, 0, 8, 0, &view) ==
        OIR_STATUS_LENGTH_MISMATCH, "reject truncated token stream");
    CHECK(oir_token_stream_open(buffer, capacity, 1, 8, 0, &view) ==
        OIR_STATUS_INVALID_SECTION, "reject mismatched source identity");

    {
        uint8_t saved = buffer[OIR_TOKEN_STREAM_HEADER_SIZE + 28];
        buffer[OIR_TOKEN_STREAM_HEADER_SIZE + 28] = 1;
        CHECK(oir_token_stream_open(buffer, capacity, 0, 8, 0, &view) ==
            OIR_STATUS_INVALID_SECTION, "reject non-zero reserved record field");
        buffer[OIR_TOKEN_STREAM_HEADER_SIZE + 28] = saved;
    }

    {
        OirTokenStreamInput bad[5];
        memcpy(bad, tokens, sizeof(tokens));
        bad[1].source_offset = 0;
        CHECK(oir_token_stream_write(buffer, capacity, 0, 8, 0,
            bad, 5, &written) == OIR_STATUS_INVALID_SECTION,
            "reject overlapping token spans");
        memcpy(bad, tokens, sizeof(tokens));
        bad[0].token_id = 999;
        CHECK(oir_token_stream_write(buffer, capacity, 0, 8, 0,
            bad, 5, &written) == OIR_STATUS_INVALID_SECTION,
            "reject unknown token ID");
        memcpy(bad, tokens, sizeof(tokens));
        bad[2].column = 2u;
        CHECK(oir_token_stream_write(buffer, capacity, 0, 8, 0,
            bad, 5, &written) == OIR_STATUS_INVALID_SECTION,
            "reject token coordinates that move backwards");
        memcpy(bad, tokens, sizeof(tokens));
        bad[4].token_id = oir_token_id_from_omni(TOKEN_SEMICOLON);
        CHECK(oir_token_stream_write(buffer, capacity, 0, 8, 0,
            bad, 5, &written) == OIR_STATUS_INVALID_SECTION,
            "require terminal EOF token");
    }

    {
        OirTokenStreamInput errors[2];
        memset(errors, 0, sizeof(errors));
        errors[0].token_id = oir_token_id_from_omni(TOKEN_ERROR);
        errors[0].diagnostic_id = 1;
        errors[0].source_offset = OIR_TOKEN_STREAM_NO_SOURCE_OFFSET;
        errors[0].line = 1;
        errors[0].column = 1;
        errors[1].token_id = oir_token_id_from_omni(TOKEN_EOF);
        errors[1].source_offset = 8;
        errors[1].line = 2;
        errors[1].column = 1;

        CHECK(oir_token_stream_write(buffer, capacity, 0, 8, 1,
            errors, 2, &written) == OIR_STATUS_OK,
            "allow error token linked to an existing diagnostic");
        CHECK(oir_token_stream_open(buffer, written, 0, 8, 1, &view) ==
            OIR_STATUS_OK, "open diagnostic-linked token stream");
        CHECK(oir_token_stream_get(&view, 0, &record) == OIR_STATUS_OK &&
            record.diagnostic_id == 1 &&
            record.source_offset == OIR_TOKEN_STREAM_NO_SOURCE_OFFSET,
            "preserve non-source error token diagnostic ID");

        errors[0].diagnostic_id = 0;
        CHECK(oir_token_stream_write(buffer, capacity, 0, 8, 1,
            errors, 2, &written) == OIR_STATUS_INVALID_SECTION,
            "reject error token without diagnostic reference");
    }

    {
        Token raw_body={0};
        Token decoded={0};
        OirTokenStreamInput raw_tokens[2]={{0}};
        raw_body.type=TOKEN_ASM_BODY;
        raw_body.start="";
        raw_body.length=0;
        raw_body.line=1;
        raw_body.col=1;
        raw_body.source_offset=0u;
        raw_body.source_length=0u;
        CHECK(oir_token_stream_input_from_lexer_token(&raw_body,&raw_tokens[0])==OIR_STATUS_OK &&
            raw_tokens[0].token_id==164u && raw_tokens[0].lexeme_length==0u,
            "convert empty opaque ASM body token");
        raw_tokens[1].token_id=oir_token_id_from_omni(TOKEN_EOF);
        raw_tokens[1].source_offset=0u;
        raw_tokens[1].line=1u;
        raw_tokens[1].column=1u;
        CHECK(oir_token_stream_write(buffer,capacity,0u,0u,0u,raw_tokens,2u,&written)==OIR_STATUS_OK,
            "write zero-length ASM body token");
        CHECK(oir_token_stream_open(buffer,written,0u,0u,0u,&view)==OIR_STATUS_OK,
            "open zero-length ASM body token stream");
        CHECK(oir_token_stream_get(&view,0u,&record)==OIR_STATUS_OK &&
            record.token_id==164u && record.lexeme_length==0u,
            "read zero-length ASM body record");
        CHECK(oir_token_stream_record_to_lexer_token(&record,(const uint8_t*)"",0u,
            NULL,0u,&decoded)==OIR_STATUS_OK && decoded.type==TOKEN_ASM_BODY &&
            decoded.length==0 && decoded.source_length==0u,
            "restore zero-length ASM body token");
    }

    free(buffer);
    puts("OIR token stream payload tests passed.");
    return 0;
}
