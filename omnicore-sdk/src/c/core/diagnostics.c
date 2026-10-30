#include "diagnostics.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int reserve_records(OccDiagnosticCollector* collector, size_t needed) {
    size_t capacity;
    OccDiagnosticRecord* records;

    if (needed <= collector->capacity) return 1;
    capacity = collector->capacity ? collector->capacity : 8u;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2u) return 0;
        capacity *= 2u;
    }
    if (capacity > SIZE_MAX / sizeof(*records)) return 0;
    records = (OccDiagnosticRecord*)realloc(
        collector->records, capacity * sizeof(*records));
    if (!records) return 0;
    collector->records = records;
    collector->capacity = capacity;
    return 1;
}

static int reserve_text(OccDiagnosticCollector* collector, size_t needed) {
    size_t capacity;
    char* text;

    if (needed <= collector->text_capacity) return 1;
    capacity = collector->text_capacity ? collector->text_capacity : 256u;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2u) return 0;
        capacity *= 2u;
    }
    text = (char*)realloc(collector->text_arena, capacity);
    if (!text) return 0;
    collector->text_arena = text;
    collector->text_capacity = capacity;
    return 1;
}

void occ_diagnostics_init(OccDiagnosticCollector* collector) {
    if (!collector) return;
    memset(collector, 0, sizeof(*collector));
}

void occ_diagnostics_destroy(OccDiagnosticCollector* collector) {
    if (!collector) return;
    free(collector->records);
    free(collector->text_arena);
    memset(collector, 0, sizeof(*collector));
}

int occ_diagnostics_add(
    OccDiagnosticCollector* collector,
    const OccDiagnosticInput* input
) {
    size_t message_length;
    size_t hint_length;
    size_t required_text;
    size_t extra_text;
    OccDiagnosticRecord record;

    if (!collector || !input) return 0;
    if (collector->allocation_failed) return 0;
    if (input->code == 0 ||
        input->severity < OCC_DIAG_NOTE ||
        input->severity > OCC_DIAG_FATAL ||
        (!input->message && input->message_length != 0) ||
        (!input->hint && input->hint_length != 0) ||
        input->message_length == 0 ||
        memchr(input->message, '\0', input->message_length) != NULL ||
        (input->hint_length &&
            memchr(input->hint, '\0', input->hint_length) != NULL)) {
        return 0;
    }

    if (input->span.has_location) {
        if (input->span.end_offset < input->span.start_offset ||
            input->span.start_line == 0 || input->span.start_column == 0 ||
            input->span.end_line == 0 || input->span.end_column == 0) {
            return 0;
        }
    }

    message_length = input->message_length;
    hint_length = input->hint_length;
    if (message_length > UINT32_MAX || hint_length > UINT32_MAX ||
        collector->text_length > UINT32_MAX ||
        collector->count == SIZE_MAX ||
        collector->severity_counts[input->severity] == SIZE_MAX ||
        collector->text_length > SIZE_MAX - 1u) {
        return 0;
    }
    if (message_length > SIZE_MAX - collector->text_length - 1u) return 0;
    extra_text = message_length + 1u;
    if (hint_length) {
        if (extra_text > SIZE_MAX - collector->text_length - 1u ||
            hint_length > SIZE_MAX - collector->text_length - extra_text - 1u) {
            return 0;
        }
        extra_text += hint_length + 1u;
    }
    required_text = collector->text_length + extra_text;
    if (required_text > UINT32_MAX) return 0;

    if (!reserve_records(collector, collector->count + 1u) ||
        !reserve_text(collector, required_text)) {
        collector->allocation_failed = 1;
        return 0;
    }

    memset(&record, 0, sizeof(record));
    record.code = input->code;
    record.stage_id = input->stage_id;
    record.message_id = input->message_id;
    record.severity = input->severity;
    record.span = input->span;
    if (!record.span.has_location) {
        memset(&record.span, 0, sizeof(record.span));
    }

    record.message_offset = (uint32_t)collector->text_length;
    record.message_length = (uint32_t)message_length;
    memcpy(collector->text_arena + collector->text_length,
        input->message, message_length);
    collector->text_length += message_length;
    collector->text_arena[collector->text_length++] = '\0';

    record.hint_offset = (uint32_t)collector->text_length;
    record.hint_length = (uint32_t)hint_length;
    if (hint_length) {
        memcpy(collector->text_arena + collector->text_length,
            input->hint, hint_length);
        collector->text_length += hint_length;
        collector->text_arena[collector->text_length++] = '\0';
    }

    collector->records[collector->count++] = record;
    collector->severity_counts[input->severity]++;
    return 1;
}

size_t occ_diagnostics_count(const OccDiagnosticCollector* collector) {
    return collector ? collector->count : 0;
}

size_t occ_diagnostics_count_severity(
    const OccDiagnosticCollector* collector,
    OccDiagnosticSeverity severity
) {
    if (!collector || severity < OCC_DIAG_NOTE || severity > OCC_DIAG_FATAL) {
        return 0;
    }
    return collector->severity_counts[severity];
}

int occ_diagnostics_get(
    const OccDiagnosticCollector* collector,
    size_t index,
    OccDiagnosticView* out_view
) {
    const OccDiagnosticRecord* record;
    if (!collector || !out_view || index >= collector->count) return 0;
    record = &collector->records[index];
    out_view->record = record;
    out_view->message = collector->text_arena + record->message_offset;
    out_view->hint = record->hint_length
        ? collector->text_arena + record->hint_offset
        : NULL;
    return 1;
}

int occ_diagnostics_blocks_output(const OccDiagnosticCollector* collector) {
    if (!collector) return 1;
    if (collector->allocation_failed) return 1;
    return collector->severity_counts[OCC_DIAG_WARNING] != 0 ||
        collector->severity_counts[OCC_DIAG_ERROR] != 0 ||
        collector->severity_counts[OCC_DIAG_FATAL] != 0;
}
