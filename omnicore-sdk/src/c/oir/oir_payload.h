#ifndef OMNICORE_OIR_PAYLOAD_H
#define OMNICORE_OIR_PAYLOAD_H

#include "oir.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OIR_SOURCE_UNIT_SCHEMA_MAJOR 1u
#define OIR_SOURCE_UNIT_SCHEMA_MINOR 0u
#define OIR_SOURCE_UNIT_HEADER_SIZE 24u
#define OIR_SOURCE_ENCODING_UTF8 1u

typedef struct OirSourceUnitView {
    uint32_t source_id;
    uint16_t encoding_id;
    uint16_t flags;
    const uint8_t* path;
    uint32_t path_length;
    const uint8_t* source;
    uint64_t source_length;
} OirSourceUnitView;

/* Validate and expose a SOURCE_UNIT schema 1.0 payload without copying it. */
OirStatus oir_source_unit_open(
    const uint8_t* payload,
    size_t payload_length,
    OirSourceUnitView* out_view
);

/* Encode a SOURCE_UNIT schema 1.0 payload into caller-owned storage. */
OirStatus oir_source_unit_write(
    uint8_t* output,
    size_t capacity,
    uint32_t source_id,
    const uint8_t* path,
    uint32_t path_length,
    const uint8_t* source,
    uint64_t source_length,
    size_t* out_written
);

#ifdef __cplusplus
}
#endif

#endif
