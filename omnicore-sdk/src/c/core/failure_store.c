#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "failure_store.h"

#include "../oir/oir.h"
#include "../oir/oir_failure_artifact.h"
#include "../oir/oir_diagnostics.h"
#include "../oir/oir_payload.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define OCC_MKDIR(path) _mkdir(path)
#else
#include <unistd.h>
#define OCC_MKDIR(path) mkdir(path, 0777)
#endif

static int path_format(
    char* output, size_t capacity, const char* directory,
    uint64_t job_id, const char* suffix
) {
    int written;
    size_t length;
    if (!output || capacity == 0u || !directory || !suffix) return 0;
    length = strlen(directory);
    if (length == 0u) return 0;
    written = snprintf(output, capacity, "%s%sjob-%llu.oir%s",
        directory,
        (directory[length - 1u] == '/' || directory[length - 1u] == '\\') ? "" :
#ifdef _WIN32
        "\\",
#else
        "/",
#endif
        (unsigned long long)job_id, suffix);
    return written >= 0 && (size_t)written < capacity;
}

static int ensure_directory(const char* directory) {
    struct stat info;
    if (stat(directory, &info) == 0) return S_ISDIR(info.st_mode);
    if (OCC_MKDIR(directory) == 0) return 1;
    if (errno == EEXIST && stat(directory, &info) == 0) {
        return S_ISDIR(info.st_mode);
    }
    return 0;
}

int occ_failure_store_save(
    const char* directory,
    uint64_t job_id,
    uint32_t producer_stage_id,
    const char* source_path,
    const uint8_t* source,
    size_t source_length,
    const OccDiagnosticCollector* diagnostics,
    char* out_path,
    size_t out_path_capacity
) {
    size_t path_length;
    size_t source_capacity;
    size_t diagnostics_capacity;
    size_t artifact_capacity;
    size_t written = 0u;
    uint8_t* artifact = NULL;
    char final_path[4096];
    char temporary_path[4096];
    FILE* file = NULL;
    int ok = 0;
    OirStatus status;

    if (out_path && out_path_capacity) out_path[0] = '\0';
    if (!directory || !source_path || (!source && source_length != 0u) ||
        !diagnostics || diagnostics->count == 0u ||
        diagnostics->allocation_failed || !out_path || out_path_capacity == 0u) {
        return 0;
    }
    path_length = strlen(source_path);
    if (path_length > UINT32_MAX) return 0;
    if (source_length > SIZE_MAX - OIR_SOURCE_UNIT_HEADER_SIZE - path_length) return 0;
    source_capacity = OIR_SOURCE_UNIT_HEADER_SIZE + path_length + source_length;

    if (diagnostics->count >
        (SIZE_MAX - OIR_DIAGNOSTICS_HEADER_SIZE) / OIR_DIAGNOSTICS_RECORD_SIZE) return 0;
    diagnostics_capacity = OIR_DIAGNOSTICS_HEADER_SIZE +
        diagnostics->count * OIR_DIAGNOSTICS_RECORD_SIZE;
    if (diagnostics->text_length > SIZE_MAX - diagnostics_capacity) return 0;
    diagnostics_capacity += diagnostics->text_length;

    if (diagnostics_capacity >
        SIZE_MAX - OIR_HEADER_SIZE - 2u * OIR_SECTION_HEADER_SIZE) return 0;
    if (source_capacity > SIZE_MAX - OIR_HEADER_SIZE -
        2u * OIR_SECTION_HEADER_SIZE - diagnostics_capacity) return 0;
    artifact_capacity = OIR_HEADER_SIZE + 2u * OIR_SECTION_HEADER_SIZE +
        source_capacity + diagnostics_capacity;

    if (!ensure_directory(directory) ||
        !path_format(final_path, sizeof(final_path), directory, job_id, "") ||
        !path_format(temporary_path, sizeof(temporary_path), directory, job_id, ".tmp") ||
        strlen(final_path) + 1u > out_path_capacity) {
        return 0;
    }

    /* Do not overwrite an earlier diagnostic bundle for the same job ID. */
    file = fopen(final_path, "rb");
    if (file) {
        fclose(file);
        return 0;
    }

    artifact = (uint8_t*)malloc(artifact_capacity);
    if (!artifact) return 0;
    status = oir_failure_artifact_write(
        artifact, artifact_capacity, job_id, producer_stage_id,
        (const uint8_t*)source_path, (uint32_t)path_length,
        source, (uint64_t)source_length, diagnostics, &written);
    if (status != OIR_STATUS_OK) goto cleanup;

    file = fopen(temporary_path, "wb");
    if (!file) goto cleanup;
    if (fwrite(artifact, 1u, written, file) != written ||
        fflush(file) != 0) {
        fclose(file);
        file = NULL;
        goto cleanup;
    }
    if (fclose(file) != 0) {
        file = NULL;
        goto cleanup;
    }
    file = NULL;

    if (rename(temporary_path, final_path) != 0) goto cleanup;
    memcpy(out_path, final_path, strlen(final_path) + 1u);
    ok = 1;

cleanup:
    if (file) fclose(file);
    if (!ok) remove(temporary_path);
    free(artifact);
    return ok;
}
