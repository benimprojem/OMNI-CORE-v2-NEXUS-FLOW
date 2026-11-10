#include "module_interface.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    goto fail; \
} } while (0)

static int test_public_export_snapshot(void) {
    SemanticResult semantic;
    OccModuleInterface interface;
    SemanticResultStatus semantic_status;
    OccModuleInterfaceStatus interface_status;
    uint32_t type_id = 0u, group_id = 0u, function_id = 0u;
    uint32_t private_id = 0u, group_scope = 0u;
    memset(&semantic, 0, sizeof(semantic));
    memset(&interface, 0, sizeof(interface));

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
    CHECK(interface.schema_major == OCC_MODULE_INTERFACE_SCHEMA_MAJOR);
    CHECK(strcmp(interface.canonical_identity,
        "file:///sdk/dsplibs/math.nxf") == 0);
    CHECK(interface.symbol_count == 2u);
    CHECK(interface.type_count == 1u);
    CHECK(interface.relation_count == 1u);
    CHECK(interface.symbols[0].source_symbol_id == group_id);
    CHECK(strcmp(interface.symbols[0].name, "Math") == 0);
    CHECK((interface.symbols[0].flags &
        SEMANTIC_RESULT_SYMBOL_FLAG_EXPORTED) != 0u);
    CHECK(interface.symbols[1].source_symbol_id == function_id);
    CHECK(strcmp(interface.symbols[1].name, "sum") == 0);
    CHECK((interface.symbols[1].flags &
        SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC) != 0u);
    CHECK(interface.types[0].source_type_id == type_id);
    CHECK(strcmp(interface.types[0].name, "u32") == 0);
    CHECK(interface.relations[0].owner_source_symbol_id == group_id);
    CHECK(interface.relations[0].related_source_symbol_id == function_id);
    CHECK(strcmp(interface.relations[0].name, "sum") == 0);
    CHECK(interface.symbols[0].source_symbol_id != private_id);

    occ_module_interface_destroy(&interface);
    semantic_result_destroy(&semantic);
    return 1;
fail:
    occ_module_interface_destroy(&interface);
    semantic_result_destroy(&semantic);
    return 0;
}

static int test_requires_finalized_semantic_result(void) {
    SemanticResult semantic;
    OccModuleInterface interface;
    OccModuleInterfaceStatus status;
    memset(&semantic, 0, sizeof(semantic));
    memset(&interface, 0, sizeof(interface));
    CHECK(semantic_result_init(&semantic) == SEMANTIC_RESULT_OK);
    status = occ_module_interface_build("module://unfinished",
        &semantic, &interface);
    CHECK(status == OCC_MODULE_INTERFACE_INVALID_SEMANTIC_RESULT);
    semantic_result_destroy(&semantic);
    return 1;
fail:
    occ_module_interface_destroy(&interface);
    semantic_result_destroy(&semantic);
    return 0;
}

int main(void) {
    if (!test_public_export_snapshot()) return 1;
    if (!test_requires_finalized_semantic_result()) return 1;
    return 0;
}
