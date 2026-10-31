#ifndef OMNICORE_OIR_TOKEN_STREAM_H
#define OMNICORE_OIR_TOKEN_STREAM_H

#include "oir.h"
#include "lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OIR_TOKEN_STREAM_SCHEMA_MAJOR 1u
#define OIR_TOKEN_STREAM_SCHEMA_MINOR 0u
#define OIR_TOKEN_STREAM_HEADER_SIZE 16u
#define OIR_TOKEN_STREAM_RECORD_SIZE 32u
#define OIR_TOKEN_STREAM_NO_SOURCE_OFFSET UINT64_MAX

typedef struct OirTokenStreamInput {
    uint16_t token_id;
    uint16_t flags;
    uint32_t diagnostic_id;
    uint64_t source_offset;
    uint32_t lexeme_length;
    uint32_t line;
    uint32_t column;
} OirTokenStreamInput;

typedef struct OirTokenStreamPayloadView {
    const uint8_t* payload;
    size_t payload_length;
    uint32_t token_count;
    uint32_t source_id;
    uint64_t source_length;
    uint32_t diagnostic_count;
    size_t records_offset;
} OirTokenStreamPayloadView;

typedef struct OirTokenStreamRecordView {
    uint16_t token_id;
    uint16_t flags;
    uint32_t diagnostic_id;
    uint64_t source_offset;
    uint32_t lexeme_length;
    uint32_t line;
    uint32_t column;
} OirTokenStreamRecordView;

/* Convert one frontend token into its stable OIR record input. */
OirStatus oir_token_stream_input_from_lexer_token(
    const Token* token,
    OirTokenStreamInput* out_input
);

/*
 * Validate schema 1.0 against the associated source length and diagnostic
 * count. The payload remains caller-owned and must outlive the returned view.
 */
OirStatus oir_token_stream_open(
    const uint8_t* payload,
    size_t payload_length,
    uint32_t expected_source_id,
    uint64_t source_length,
    uint32_t diagnostic_count,
    OirTokenStreamPayloadView* out_view
);

/* Read one validated token record without copying the payload. */
OirStatus oir_token_stream_get(
    const OirTokenStreamPayloadView* view,
    uint32_t index,
    OirTokenStreamRecordView* out_record
);

/* Serialize stable OIR token IDs and source spans into a schema 1.0 payload. */
OirStatus oir_token_stream_write(
    uint8_t* output,
    size_t capacity,
    uint32_t source_id,
    uint64_t source_length,
    uint32_t diagnostic_count,
    const OirTokenStreamInput* tokens,
    uint32_t token_count,
    size_t* out_written
);

#ifdef __cplusplus
}
#endif

#endif
