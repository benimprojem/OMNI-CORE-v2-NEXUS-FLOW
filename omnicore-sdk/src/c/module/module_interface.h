#ifndef OCC_MODULE_INTERFACE_H
#define OCC_MODULE_INTERFACE_H

#include <stddef.h>
#include <stdint.h>
#include "../semantic/semantic_result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OCC_MODULE_INTERFACE_SCHEMA_MAJOR 1u
#define OCC_MODULE_INTERFACE_SCHEMA_MINOR 0u

struct OirSemanticAstArtifactView;

typedef enum OccModuleInterfaceStatus {
    OCC_MODULE_INTERFACE_OK = 0,
    OCC_MODULE_INTERFACE_INVALID_ARGUMENT = 1,
    OCC_MODULE_INTERFACE_INVALID_SEMANTIC_RESULT = 2,
    OCC_MODULE_INTERFACE_OUT_OF_MEMORY = 3,
    OCC_MODULE_INTERFACE_LIMIT_EXCEEDED = 4
} OccModuleInterfaceStatus;

/* A portable, owned snapshot of the public/exported declaration surface.
 * IDs remain source SemanticResult IDs; they are not renumbered in this view.
 * The snapshot deliberately preserves both PUBLIC and EXPORTED bits so that
 * import visibility policy remains a Semantic responsibility.
 */
typedef struct OccModuleInterfaceSymbol {
    uint32_t source_symbol_id;
    uint32_t scope_id;
    uint32_t declaration_node_id;
    uint32_t type_id;
    uint16_t kind;
    uint16_t flags;
    char* name;
} OccModuleInterfaceSymbol;

typedef struct OccModuleInterfaceType {
    uint32_t source_type_id;
    uint16_t kind;
    uint16_t flags;
    uint32_t referenced_symbol_id;
    uint32_t base_type_id;
    char* name;
} OccModuleInterfaceType;

typedef struct OccModuleInterfaceRelation {
    uint32_t owner_source_symbol_id;
    uint32_t related_source_symbol_id;
    uint32_t related_node_id;
    uint32_t type_id;
    uint32_t ordinal;
    uint16_t kind;
    uint16_t flags;
    char* name;
} OccModuleInterfaceRelation;

typedef struct OccModuleInterface {
    char* canonical_identity;
    uint32_t schema_major;
    uint32_t schema_minor;
    OccModuleInterfaceSymbol* symbols;
    size_t symbol_count;
    OccModuleInterfaceType* types;
    size_t type_count;
    OccModuleInterfaceRelation* relations;
    size_t relation_count;
} OccModuleInterface;

/* Build from a finalized SemanticResult. Only declarations marked PUBLIC or
 * EXPORTED are published. The full type table is intentionally not copied:
 * only types referenced by published symbols or their published relations,
 * plus their base-type chain, are included. Referenced symbol IDs are retained
 * for later visibility validation by the importing Semantic stage.
 */
OccModuleInterfaceStatus occ_module_interface_build(
    const char* canonical_identity,
    const SemanticResult* semantic_result,
    OccModuleInterface* out_interface
);

/* Build the same publication snapshot from a fully validated SEMANTIC_AST
 * artifact view. All source IDs remain local to the producing module. */
OccModuleInterfaceStatus occ_module_interface_build_from_oir_view(
    const char* canonical_identity,
    const struct OirSemanticAstArtifactView* semantic_artifact,
    OccModuleInterface* out_interface
);

void occ_module_interface_destroy(OccModuleInterface* module_interface);
const char* occ_module_interface_status_string(OccModuleInterfaceStatus status);

#ifdef __cplusplus
}
#endif

#endif
