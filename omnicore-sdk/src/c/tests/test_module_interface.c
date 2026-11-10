#include "module_interface.h"
#include "oir_module_interface_artifact.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    goto fail; \
} } while (0)

static int test_public_export_snapshot_and_oir_roundtrip(void) {
    SemanticResult semantic;
    OccModuleInterface interface;
    OirModuleInterfaceArtifactView decoded;
    SemanticResultStatus semantic_status;
    OccModuleInterfaceStatus interface_status;
    OirStatus oir_status;
    uint32_t type_id = 0u, group_id = 0u, function_id = 0u;
    uint32_t private_id = 0u, group_scope = 0u;
    uint8_t* bytes = NULL;
    size_t written = 0u;
    size_t capacity = 4096u;
    memset(&semantic, 0, sizeof(semantic));
    memset(&interface, 0, sizeof(interface));
    memset(&decoded, 0, sizeof(decoded));

    CHECK(semantic_result_init(&semantic) == SEMANTIC_RESULT_OK);
    CHECK(semantic_result_intern_type(&semantic,
        SEMANTIC_RESULT_TYPE_PRIMITIVE, 0u, "u32", 0u, 0u,
        &type_id) == SEMANTIC_RESULT_OK);
    CHECK(semantic_result_add_symbol(&semantic, 1u, 1u,
        SEMANTIC_RESULT_SYMBOL_GROUP,
        SEMANTIC_RESULT_SYMBOL_FLAG_EXPORTED,
        "Math", 0u, &group_id) == SEMANTIC_RESULT_OK);
    CHECK(semantic_result_add_scope(&semantic, 1u, 1u, 0u, 2u,
        &group_scope) == SEMANTIC_RESULT_OK);
    CHECK(semantic_result_add_symbol(&semantic, group_scope, 3u,
        SEMANTIC_RESULT_SYMBOL_FUNCTION,
        SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC,
        "sum", type_id, &function_id) == SEMANTIC_RESULT_OK);
    CHECK(semantic_result_add_symbol(&semantic, 1u, 4u,
        SEMANTIC_RESULT_SYMBOL_VARIABLE, 0u,
        "internal_value", type_id, &private_id) == SEMANTIC_RESULT_OK);
    CHECK(semantic_result_add_relation(&semantic, group_id,
        SEMANTIC_RESULT_RELATION_GROUP_MEMBER, 0u,
        function_id, 3u, "sum", type_id) == SEMANTIC_RESULT_OK);
    semantic_status = semantic_result_finalize(&semantic, 4u);
    CHECK(semantic_status == SEMANTIC_RESULT_OK);

    interface_status = occ_module_interface_build(
        "file:///sdk/dsplibs/math.nxf", &semantic, &interface);
    CHECK(interface_status == OCC_MODULE_INTERFACE_OK);
    CHECK(interface.symbol_count == 2u && interface.type_count == 1u);
    CHECK(interface.relation_count == 1u);
    CHECK(interface.symbols[0].source_symbol_id == group_id);
    CHECK(interface.symbols[1].source_symbol_id == function_id);
    CHECK(interface.symbols[0].source_symbol_id != private_id);

    bytes = (uint8_t*)malloc(capacity);
    CHECK(bytes != NULL);
    oir_status = oir_module_interface_artifact_write(
        &interface, 77u, bytes, capacity, &written);
    CHECK(oir_status == OIR_STATUS_OK && written > 0u);
    CHECK(oir_module_interface_artifact_open(bytes, written, &decoded) ==
        OIR_STATUS_OK);
    CHECK(decoded.envelope.header.artifact_kind == OIR_KIND_MODULE_INTERFACE);
    CHECK(decoded.envelope.header.job_id == 77u);
    CHECK(strcmp(decoded.module_interface.canonical_identity,
        "file:///sdk/dsplibs/math.nxf") == 0);
    CHECK(decoded.module_interface.symbol_count == 2u);
    CHECK(decoded.module_interface.type_count == 1u);
    CHECK(decoded.module_interface.relation_count == 1u);
    CHECK(decoded.module_interface.symbols[0].source_symbol_id == group_id);
    CHECK(strcmp(decoded.module_interface.symbols[0].name, "Math") == 0);
    CHECK((decoded.module_interface.symbols[0].flags &
        SEMANTIC_RESULT_SYMBOL_FLAG_EXPORTED) != 0u);
    CHECK(strcmp(decoded.module_interface.symbols[1].name, "sum") == 0);
    CHECK((decoded.module_interface.symbols[1].flags &
        SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC) != 0u);
    CHECK(strcmp(decoded.module_interface.types[0].name, "u32") == 0);
    CHECK(decoded.module_interface.relations[0].related_source_symbol_id ==
        function_id);

    oir_module_interface_artifact_destroy(&decoded);
    bytes[OIR_HEADER_SIZE + OIR_SECTION_HEADER_SIZE] ^= 0x01u;
    CHECK(oir_module_interface_artifact_open(bytes, written, &decoded) ==
        OIR_STATUS_INVALID_SECTION);
    occ_module_interface_destroy(&interface);
    semantic_result_destroy(&semantic);
    free(bytes);
    return 1;
fail:
    oir_module_interface_artifact_destroy(&decoded);
    occ_module_interface_destroy(&interface);
    semantic_result_destroy(&semantic);
    free(bytes);
    return 0;
}

static int test_rejects_corrupt_payload_and_nonfinalized_input(void) {
    SemanticResult semantic;
    OccModuleInterface interface;
    OirModuleInterfaceArtifactView decoded;
    uint8_t bytes[128];
    size_t written = 0u;
    memset(&semantic, 0, sizeof(semantic));
    memset(&interface, 0, sizeof(interface));
    memset(&decoded, 0, sizeof(decoded));
    CHECK(semantic_result_init(&semantic) == SEMANTIC_RESULT_OK);
    CHECK(occ_module_interface_build("module://unfinished",
        &semantic, &interface) == OCC_MODULE_INTERFACE_INVALID_SEMANTIC_RESULT);
    memset(bytes, 0, sizeof(bytes));
    CHECK(oir_module_interface_artifact_open(bytes, sizeof(bytes), &decoded) !=
        OIR_STATUS_OK);
    semantic_result_destroy(&semantic);
    occ_module_interface_destroy(&interface);
    return 1;
fail:
    oir_module_interface_artifact_destroy(&decoded);
    occ_module_interface_destroy(&interface);
    semantic_result_destroy(&semantic);
    return 0;
}

int main(void) {
    if (!test_public_export_snapshot_and_oir_roundtrip()) return 1;
    if (!test_rejects_corrupt_payload_and_nonfinalized_input()) return 1;
    return 0;
}
