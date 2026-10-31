#include "oir_failure_artifact.h"

#include "oir_diagnostics.h"
#include "oir_payload.h"

#include <stdlib.h>

OirStatus oir_failure_artifact_write(
    uint8_t* output,
    size_t capacity,
    uint64_t job_id,
    uint32_t producer_stage_id,
    const uint8_t* path,
    uint32_t path_length,
    const uint8_t* source,
    uint64_t source_length,
    const OccDiagnosticCollector* diagnostics,
    size_t* out_written
) {
    uint8_t* source_payload = NULL;
    uint8_t* diagnostics_payload = NULL;
    size_t source_capacity;
    size_t diagnostics_capacity;
    size_t source_written = 0;
    size_t diagnostics_written = 0;
    OirArtifactHeader header = {0};
    OirSectionInput sections[2] = {{0}};
    OirStatus status;

    if (out_written) *out_written = 0;
    if (!output || !out_written || (!path && path_length != 0u) ||
        (!source && source_length != 0u) || !diagnostics ||
        diagnostics->count == 0u || diagnostics->allocation_failed) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
#if SIZE_MAX < UINT64_MAX
    if (source_length > (uint64_t)SIZE_MAX) return OIR_STATUS_OVERFLOW;
#endif
    if ((size_t)path_length > SIZE_MAX - OIR_SOURCE_UNIT_HEADER_SIZE ||
        (size_t)source_length >
            SIZE_MAX - OIR_SOURCE_UNIT_HEADER_SIZE - (size_t)path_length) {
        return OIR_STATUS_OVERFLOW;
    }
    source_capacity = OIR_SOURCE_UNIT_HEADER_SIZE +
        (size_t)path_length + (size_t)source_length;

    if (diagnostics->count >
        (SIZE_MAX - OIR_DIAGNOSTICS_HEADER_SIZE) / OIR_DIAGNOSTICS_RECORD_SIZE) {
        return OIR_STATUS_OVERFLOW;
    }
    diagnostics_capacity = OIR_DIAGNOSTICS_HEADER_SIZE +
        diagnostics->count * OIR_DIAGNOSTICS_RECORD_SIZE;
    if (diagnostics->text_length > SIZE_MAX - diagnostics_capacity) {
        return OIR_STATUS_OVERFLOW;
    }
    diagnostics_capacity += diagnostics->text_length;

    source_payload = (uint8_t*)malloc(source_capacity ? source_capacity : 1u);
    diagnostics_payload = (uint8_t*)malloc(
        diagnostics_capacity ? diagnostics_capacity : 1u);
    if (!source_payload || !diagnostics_payload) {
        status = OIR_STATUS_OUT_OF_MEMORY;
        goto cleanup;
    }

    status = oir_source_unit_write(source_payload, source_capacity, 0u,
        path, path_length, source, source_length, &source_written);
    if (status != OIR_STATUS_OK) goto cleanup;

    status = oir_diagnostics_payload_write(diagnostics_payload,
        diagnostics_capacity, diagnostics, &diagnostics_written);
    if (status != OIR_STATUS_OK) goto cleanup;

    header.container_major = OIR_CONTAINER_MAJOR;
    header.container_minor = OIR_CONTAINER_MINOR;
    header.artifact_kind = OIR_KIND_SOURCE_UNIT;
    header.payload_schema_major = OIR_SOURCE_UNIT_SCHEMA_MAJOR;
    header.payload_schema_minor = OIR_SOURCE_UNIT_SCHEMA_MINOR;
    header.producer_stage_id = producer_stage_id;
    header.validity = OIR_VALIDITY_INVALID;
    header.job_id = job_id;

    sections[0].type = OIR_SECTION_PAYLOAD;
    sections[0].version = OIR_SOURCE_UNIT_SCHEMA_MAJOR;
    sections[0].flags = OIR_SECTION_FLAG_REQUIRED;
    sections[0].data = source_payload;
    sections[0].length = source_written;

    sections[1].type = OIR_SECTION_DIAGNOSTICS;
    sections[1].version = OIR_DIAGNOSTICS_SCHEMA_MAJOR;
    sections[1].flags = OIR_SECTION_FLAG_REQUIRED;
    sections[1].data = diagnostics_payload;
    sections[1].length = diagnostics_written;

    status = oir_artifact_write(output, capacity, &header, sections, 2u,
        out_written);

cleanup:
    free(source_payload);
    free(diagnostics_payload);
    return status;
}
