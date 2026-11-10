#ifndef OMNICORE_OIR_H
#define OMNICORE_OIR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OIR_CONTAINER_MAJOR 1u
#define OIR_CONTAINER_MINOR 0u
#define OIR_HEADER_SIZE 44u
#define OIR_SECTION_HEADER_SIZE 16u
#define OIR_MAGIC_0 ((uint8_t)'O')
#define OIR_MAGIC_1 ((uint8_t)'I')
#define OIR_MAGIC_2 ((uint8_t)'R')
#define OIR_MAGIC_3 ((uint8_t)'1')

#define OIR_SECTION_FLAG_REQUIRED 0x00000001u

typedef enum OirArtifactKind {
    OIR_KIND_SOURCE_UNIT = 1,
    OIR_KIND_TOKEN_STREAM = 2,
    OIR_KIND_AST = 3,
    OIR_KIND_SEMANTIC_AST = 4,
    OIR_KIND_MODULE = 5,
    OIR_KIND_TARGET_IR = 6,
    OIR_KIND_ASSEMBLY = 7,
    OIR_KIND_OBJECT = 8,
    OIR_KIND_LINK_INPUT = 9,
    OIR_KIND_MODULE_INTERFACE = 10
} OirArtifactKind;

typedef enum OirArtifactValidity {
    OIR_VALIDITY_VALID = 1,
    OIR_VALIDITY_PARTIAL = 2,
    OIR_VALIDITY_INVALID = 3,
    /* Pipeline result only: a serialized container must always carry a payload. */
    OIR_VALIDITY_NONE = 4
} OirArtifactValidity;

typedef enum OirSectionType {
    OIR_SECTION_PAYLOAD = 1,
    OIR_SECTION_SOURCE_MAP = 2,
    OIR_SECTION_DIAGNOSTICS = 3,
    OIR_SECTION_DEPENDENCIES = 4,
    OIR_SECTION_METADATA = 5,
    OIR_SECTION_CHECKSUM = 6,
    /* SEMANTIC_AST schema 1.0 section registry. */
    OIR_SECTION_SEMANTIC_SCOPES = 7,
    OIR_SECTION_SEMANTIC_TYPES = 8,
    OIR_SECTION_SEMANTIC_SYMBOLS = 9,
    OIR_SECTION_SEMANTIC_ANNOTATIONS = 10,
    OIR_SECTION_SEMANTIC_RELATIONS = 11,
    OIR_SECTION_SEMANTIC_CONVERSIONS = 12
} OirSectionType;

typedef enum OirStatus {
    OIR_STATUS_OK = 0,
    OIR_STATUS_INVALID_ARGUMENT,
    OIR_STATUS_BUFFER_TOO_SMALL,
    OIR_STATUS_BAD_MAGIC,
    OIR_STATUS_UNSUPPORTED_VERSION,
    OIR_STATUS_INVALID_HEADER,
    OIR_STATUS_INVALID_SECTION,
    OIR_STATUS_MISSING_PAYLOAD,
    OIR_STATUS_DUPLICATE_PAYLOAD,
    OIR_STATUS_UNKNOWN_REQUIRED_SECTION,
    OIR_STATUS_LENGTH_MISMATCH,
    OIR_STATUS_OVERFLOW,
    OIR_STATUS_OUT_OF_MEMORY
} OirStatus;

/* Logical host-side header. It is encoded field-by-field, never dumped as a C struct. */
typedef struct OirArtifactHeader {
    uint16_t container_major;
    uint16_t container_minor;
    uint32_t artifact_kind;
    uint16_t payload_schema_major;
    uint16_t payload_schema_minor;
    uint32_t producer_stage_id;
    uint8_t validity;
    uint64_t job_id;
} OirArtifactHeader;

typedef struct OirSectionInput {
    uint16_t type;
    uint16_t version;
    uint32_t flags;
    const uint8_t* data;
    uint64_t length;
} OirSectionInput;

typedef struct OirSectionView {
    uint16_t type;
    uint16_t version;
    uint32_t flags;
    const uint8_t* data;
    uint64_t length;
} OirSectionView;

typedef struct OirArtifactView {
    const uint8_t* bytes;
    size_t length;
    OirArtifactHeader header;
    uint32_t section_count;
    size_t sections_offset;
} OirArtifactView;

/*
 * Validate a complete serialized OIR container and expose a read-only view.
 * The input buffer remains owned by the caller and must outlive the view.
 */
OirStatus oir_artifact_open(
    const uint8_t* bytes,
    size_t length,
    OirArtifactView* out_view
);

/* Retrieve a section by zero-based index after successful oir_artifact_open(). */
OirStatus oir_artifact_get_section(
    const OirArtifactView* view,
    uint32_t index,
    OirSectionView* out_section
);

/*
 * Serialize one artifact into caller-owned storage.
 * Exactly one OIR_SECTION_PAYLOAD section is required.
 * Section data must not overlap the output buffer.
 */
OirStatus oir_artifact_write(
    uint8_t* output,
    size_t capacity,
    const OirArtifactHeader* header,
    const OirSectionInput* sections,
    uint32_t section_count,
    size_t* out_written
);

const char* oir_status_string(OirStatus status);

#ifdef __cplusplus
}
#endif

#endif
