#ifdef NDEBUG
#undef NDEBUG
#endif
#include "oir_semantic_ast_artifact.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static void set_section(
    OirSectionInput* section,
    uint16_t type,
    uint16_t version,
    uint32_t flags,
    const uint8_t* data,
    size_t length
) {
    section->type = type;
    section->version = version;
    section->flags = flags;
    section->data = data;
    section->length = length;
}

int main(void) {
    uint8_t tlv_bytes[16];
    uint8_t ast_payload[OIR_AST_PAYLOAD_HEADER_SIZE +
        OIR_AST_NODE_RECORD_SIZE + sizeof(tlv_bytes)];
    uint8_t scopes[OIR_SEMANTIC_SCOPES_HEADER_SIZE +
        OIR_SEMANTIC_SCOPE_RECORD_SIZE];
    uint8_t types[OIR_SEMANTIC_TYPES_HEADER_SIZE];
    uint8_t symbols[OIR_SEMANTIC_SYMBOLS_HEADER_SIZE];
    uint8_t annotations[OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE];
    uint8_t relations[OIR_SEMANTIC_RELATIONS_HEADER_SIZE];
    uint8_t conversions[OIR_SEMANTIC_CONVERSIONS_HEADER_SIZE];
    uint8_t artifact[2048];
    uint8_t bad_ast[sizeof(ast_payload)];
    uint8_t zero_ref[4] = { 0u, 0u, 0u, 0u };
    OirAstTlvWriter tlv_writer;
    OirAstNodeRecord root;
    OirArtifactHeader header;
    OirSectionInput sections[7];
    OirSemanticScope root_scope = { 1u, 0u, 0u, 0u, 0u };
    OirSemanticAstArtifactView view;
    uint32_t incoming[1], owner_scope[2];
    uint8_t visited[1];
    OirAstGraphStackEntry stack[1];
    size_t tlv_written = 0u, ast_written = 0u, scope_written = 0u;
    size_t type_written = 0u, symbol_written = 0u, annotation_written = 0u;
    size_t relation_written = 0u, conversion_written = 0u;
    size_t artifact_written = 0u;

    oir_ast_tlv_writer_init(&tlv_writer, tlv_bytes, sizeof(tlv_bytes));
    CHECK(oir_ast_tlv_write(&tlv_writer, 1u, OIR_AST_VALUE_NODE_REF,
        0u, zero_ref, sizeof(zero_ref)) == OIR_STATUS_OK);
    tlv_written = tlv_writer.used;

    memset(&root, 0, sizeof(root));
    root.node_id = 1u;
    root.node_kind_id = 1u; /* AST_PROGRAM */
    root.field_data_length = (uint32_t)tlv_written;
    CHECK(oir_ast_node_table_write(ast_payload, sizeof(ast_payload),
        &root, 1u, 1u, tlv_bytes, (uint32_t)tlv_written,
        NULL, 0u, &ast_written) == OIR_STATUS_OK);

    CHECK(oir_semantic_scopes_write(scopes, sizeof(scopes),
        &root_scope, 1u, &scope_written) == OIR_STATUS_OK);
    CHECK(oir_semantic_types_write(types, sizeof(types),
        NULL, 0u, NULL, 0u, &type_written) == OIR_STATUS_OK);
    CHECK(oir_semantic_symbols_write(symbols, sizeof(symbols),
        NULL, 0u, NULL, 0u, &symbol_written) == OIR_STATUS_OK);
    CHECK(oir_semantic_annotations_write(annotations, sizeof(annotations),
        NULL, 0u, &annotation_written) == OIR_STATUS_OK);
    CHECK(oir_semantic_relations_write(relations, sizeof(relations),
        NULL, 0u, NULL, 0u, &relation_written) == OIR_STATUS_OK);
    CHECK(oir_semantic_conversions_write(conversions, sizeof(conversions),
        NULL, 0u, &conversion_written) == OIR_STATUS_OK);

    memset(&header, 0, sizeof(header));
    header.container_major = OIR_CONTAINER_MAJOR;
    header.container_minor = OIR_CONTAINER_MINOR;
    header.artifact_kind = OIR_KIND_SEMANTIC_AST;
    header.payload_schema_major = OIR_SEMANTIC_AST_SCHEMA_MAJOR;
    header.payload_schema_minor = OIR_SEMANTIC_AST_SCHEMA_MINOR;
    header.validity = OIR_VALIDITY_VALID;
    header.job_id = 42u;

    set_section(&sections[0], OIR_SECTION_PAYLOAD, 1u,
        OIR_SECTION_FLAG_REQUIRED, ast_payload, ast_written);
    set_section(&sections[1], OIR_SECTION_SEMANTIC_SCOPES, 1u,
        OIR_SECTION_FLAG_REQUIRED, scopes, scope_written);
    set_section(&sections[2], OIR_SECTION_SEMANTIC_TYPES, 1u,
        OIR_SECTION_FLAG_REQUIRED, types, type_written);
    set_section(&sections[3], OIR_SECTION_SEMANTIC_SYMBOLS, 1u,
        OIR_SECTION_FLAG_REQUIRED, symbols, symbol_written);
    set_section(&sections[4], OIR_SECTION_SEMANTIC_ANNOTATIONS, 1u,
        OIR_SECTION_FLAG_REQUIRED, annotations, annotation_written);
    set_section(&sections[5], OIR_SECTION_SEMANTIC_RELATIONS, 1u,
        OIR_SECTION_FLAG_REQUIRED, relations, relation_written);
    set_section(&sections[6], OIR_SECTION_SEMANTIC_CONVERSIONS, 1u,
        0u, conversions, conversion_written);

    CHECK(oir_artifact_write(artifact, sizeof(artifact), &header,
        sections, 6u, &artifact_written) == OIR_STATUS_OK);
    CHECK(oir_semantic_ast_artifact_open(artifact, artifact_written, &view,
        incoming, 1u, visited, 1u, stack, 1u, owner_scope, 2u) ==
        OIR_STATUS_OK);
    CHECK(view.envelope.header.job_id == 42u &&
        view.ast.node_count == 1u && view.scopes.record_count == 1u &&
        view.types.record_count == 0u && view.symbols.record_count == 0u &&
        view.annotations.record_count == 0u && view.relations.record_count == 0u &&
        view.has_conversions == 0u);

    /* The optional conversions section is accepted when present. */
    CHECK(oir_artifact_write(artifact, sizeof(artifact), &header,
        sections, 7u, &artifact_written) == OIR_STATUS_OK);
    CHECK(oir_semantic_ast_artifact_open(artifact, artifact_written, &view,
        incoming, 1u, visited, 1u, stack, 1u, owner_scope, 2u) ==
        OIR_STATUS_OK);
    CHECK(view.has_conversions == 1u && view.conversions.record_count == 0u);

    /* Missing mandatory RELATIONS section. */
    CHECK(oir_artifact_write(artifact, sizeof(artifact), &header,
        sections, 5u, &artifact_written) == OIR_STATUS_OK);
    CHECK(oir_semantic_ast_artifact_open(artifact, artifact_written, &view,
        incoming, 1u, visited, 1u, stack, 1u, owner_scope, 2u) ==
        OIR_STATUS_INVALID_SECTION);

    /* A mandatory semantic section cannot be duplicated. */
    {
        OirSectionInput duplicate[7];
        memcpy(duplicate, sections, 6u * sizeof(sections[0]));
        duplicate[6] = sections[1];
        CHECK(oir_artifact_write(artifact, sizeof(artifact), &header,
            duplicate, 7u, &artifact_written) == OIR_STATUS_OK);
        CHECK(oir_semantic_ast_artifact_open(artifact, artifact_written, &view,
            incoming, 1u, visited, 1u, stack, 1u, owner_scope, 2u) ==
            OIR_STATUS_INVALID_SECTION);
    }

    /* Section versions and required flags are part of the artifact contract. */
    {
        OirSectionInput bad_sections[6];
        memcpy(bad_sections, sections, sizeof(bad_sections));
        bad_sections[1].version = 2u;
        CHECK(oir_artifact_write(artifact, sizeof(artifact), &header,
            bad_sections, 6u, &artifact_written) == OIR_STATUS_OK);
        CHECK(oir_semantic_ast_artifact_open(artifact, artifact_written, &view,
            incoming, 1u, visited, 1u, stack, 1u, owner_scope, 2u) ==
            OIR_STATUS_INVALID_SECTION);

        memcpy(bad_sections, sections, sizeof(bad_sections));
        bad_sections[1].flags = 0u;
        CHECK(oir_artifact_write(artifact, sizeof(artifact), &header,
            bad_sections, 6u, &artifact_written) == OIR_STATUS_OK);
        CHECK(oir_semantic_ast_artifact_open(artifact, artifact_written, &view,
            incoming, 1u, visited, 1u, stack, 1u, owner_scope, 2u) ==
            OIR_STATUS_INVALID_SECTION);
    }

    /* Wrong artifact kind or non-final validity cannot be opened as SEMANTIC_AST. */
    {
        OirArtifactHeader bad_header = header;
        bad_header.artifact_kind = OIR_KIND_AST;
        CHECK(oir_artifact_write(artifact, sizeof(artifact), &bad_header,
            sections, 6u, &artifact_written) == OIR_STATUS_OK);
        CHECK(oir_semantic_ast_artifact_open(artifact, artifact_written, &view,
            incoming, 1u, visited, 1u, stack, 1u, owner_scope, 2u) ==
            OIR_STATUS_INVALID_HEADER);

        bad_header = header;
        bad_header.validity = OIR_VALIDITY_PARTIAL;
        CHECK(oir_artifact_write(artifact, sizeof(artifact), &bad_header,
            sections, 6u, &artifact_written) == OIR_STATUS_OK);
        CHECK(oir_semantic_ast_artifact_open(artifact, artifact_written, &view,
            incoming, 1u, visited, 1u, stack, 1u, owner_scope, 2u) ==
            OIR_STATUS_INVALID_HEADER);
    }

    /* The embedded AST itself is validated before semantic views are published. */
    {
        OirSectionInput bad_ast_sections[6];
        memcpy(bad_ast, ast_payload, ast_written);
        /* Corrupt the root NODE_REF value to an out-of-range node ID. */
        bad_ast[OIR_AST_PAYLOAD_HEADER_SIZE + OIR_AST_NODE_RECORD_SIZE +
            OIR_AST_TLV_HEADER_SIZE] = 2u;
        memcpy(bad_ast_sections, sections, sizeof(bad_ast_sections));
        bad_ast_sections[0].data = bad_ast;
        CHECK(oir_artifact_write(artifact, sizeof(artifact), &header,
            bad_ast_sections, 6u, &artifact_written) == OIR_STATUS_OK);
        CHECK(oir_semantic_ast_artifact_open(artifact, artifact_written, &view,
            incoming, 1u, visited, 1u, stack, 1u, owner_scope, 2u) !=
            OIR_STATUS_OK);
    }

    puts("OIR SEMANTIC_AST complete artifact validation tests passed.");
    return 0;
}
