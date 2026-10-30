#ifndef OMNICORE_OIR_DIAGNOSTICS_H
#define OMNICORE_OIR_DIAGNOSTICS_H

#include "oir.h"
#include "diagnostics.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OIR_DIAGNOSTICS_SCHEMA_MAJOR 1u
#define OIR_DIAGNOSTICS_SCHEMA_MINOR 0u
#define OIR_DIAGNOSTICS_HEADER_SIZE 16u
#define OIR_DIAGNOSTICS_RECORD_SIZE 68u
#define OIR_DIAGNOSTICS_NO_HINT UINT32_MAX

typedef struct OirDiagnosticsPayloadView {
    const uint8_t* payload;
    size_t payload_length;
    uint32_t record_count;
    uint32_t string_table_length;
    size_t records_offset;
    size_t strings_offset;
} OirDiagnosticsPayloadView;

typedef struct OirDiagnosticEntryView {
    uint32_t code;
    uint32_t stage_id;
    uint32_t message_id;
    uint8_t severity;
    OccSourceSpan span;
    const char* message;
    uint32_t message_length;
    const char* hint;
    uint32_t hint_length;
} OirDiagnosticEntryView;

/* Validate schema 1.0 and expose a zero-copy read-only view. */
OirStatus oir_diagnostics_payload_open(
    const uint8_t* payload,
    size_t payload_length,
    OirDiagnosticsPayloadView* out_view
);

/* Read one validated record. Returned strings point into the payload buffer. */
OirStatus oir_diagnostics_payload_get(
    const OirDiagnosticsPayloadView* view,
    uint32_t index,
    OirDiagnosticEntryView* out_entry
);

/* Serialize a job-owned collector into a schema 1.0 DIAGNOSTICS payload. */
OirStatus oir_diagnostics_payload_write(
    uint8_t* output,
    size_t capacity,
    const OccDiagnosticCollector* collector,
    size_t* out_written
);

#ifdef __cplusplus
}
#endif

#endif
