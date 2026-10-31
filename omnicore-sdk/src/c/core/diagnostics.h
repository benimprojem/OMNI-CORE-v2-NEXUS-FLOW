#ifndef OMNICORE_DIAGNOSTICS_H
#define OMNICORE_DIAGNOSTICS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stable stage identifiers shared by the frontend and future stage modules. */
typedef enum OccDiagnosticStage {
    OCC_STAGE_LEXER = 1,
    OCC_STAGE_PARSER = 2,
    OCC_STAGE_SEMANTIC = 3,
    OCC_STAGE_OIR = 4,
    OCC_STAGE_CODEGEN = 5,
    OCC_STAGE_OCC_HOST = 6
} OccDiagnosticStage;

typedef enum OccDiagnosticSeverity {
    OCC_DIAG_NOTE = 1,
    OCC_DIAG_WARNING = 2,
    OCC_DIAG_ERROR = 3,
    OCC_DIAG_FATAL = 4
} OccDiagnosticSeverity;

typedef struct OccSourceSpan {
    uint32_t source_id;
    uint64_t start_offset;
    uint64_t end_offset;
    uint32_t start_line;
    uint32_t start_column;
    uint32_t end_line;
    uint32_t end_column;
    uint8_t has_location;
} OccSourceSpan;

/* Internal job-owned record. Text is stored in the collector arena by offset. */
typedef struct OccDiagnosticRecord {
    uint32_t code;
    uint32_t stage_id;
    uint32_t message_id;
    OccSourceSpan span;
    uint8_t severity;
    uint32_t message_offset;
    uint32_t message_length;
    uint32_t hint_offset;
    uint32_t hint_length;
} OccDiagnosticRecord;

typedef struct OccDiagnosticInput {
    uint32_t code;
    uint32_t stage_id;
    uint32_t message_id;
    uint8_t severity;
    OccSourceSpan span;
    const char* message;
    size_t message_length;
    const char* hint;
    size_t hint_length;
} OccDiagnosticInput;

typedef struct OccDiagnosticView {
    const OccDiagnosticRecord* record;
    const char* message;
    const char* hint;
} OccDiagnosticView;

/* Job-level diagnostic resource policy; byte counts include NUL terminators. */
#define OCC_DIAGNOSTICS_MAX_COUNT 65536u
#define OCC_DIAGNOSTICS_MAX_TEXT_BYTES (16u * 1024u * 1024u)
#define OCC_DIAGNOSTICS_MAX_MESSAGE_BYTES (1024u * 1024u)

/* Job-level diagnostic resource policy; byte counts include NUL terminators. */
#define OCC_DIAGNOSTICS_MAX_COUNT 65536u
#define OCC_DIAGNOSTICS_MAX_TEXT_BYTES (16u * 1024u * 1024u)
#define OCC_DIAGNOSTICS_MAX_MESSAGE_BYTES (1024u * 1024u)

typedef struct OccDiagnosticCollector {
    OccDiagnosticRecord* records;
    size_t count;
    size_t capacity;
    char* text_arena;
    size_t text_length;
    size_t text_capacity;
    size_t severity_counts[5];
    int allocation_failed;
    int limit_exceeded;
} OccDiagnosticCollector;

void occ_diagnostics_init(OccDiagnosticCollector* collector);
void occ_diagnostics_destroy(OccDiagnosticCollector* collector);

/* Copies message/hint into collector-owned storage. No caller pointer is retained. */
int occ_diagnostics_add(
    OccDiagnosticCollector* collector,
    const OccDiagnosticInput* input
);

size_t occ_diagnostics_count(const OccDiagnosticCollector* collector);
size_t occ_diagnostics_count_severity(
    const OccDiagnosticCollector* collector,
    OccDiagnosticSeverity severity
);
int occ_diagnostics_get(
    const OccDiagnosticCollector* collector,
    size_t index,
    OccDiagnosticView* out_view
);

/* Strict default gate: warning, error or fatal blocks final output. */
int occ_diagnostics_blocks_output(const OccDiagnosticCollector* collector);

#ifdef __cplusplus
}
#endif

#endif
