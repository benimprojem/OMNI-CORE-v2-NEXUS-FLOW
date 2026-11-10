#ifndef OCC_MODULE_IMPORT_BUNDLE_H
#define OCC_MODULE_IMPORT_BUNDLE_H

#include "oir_module_imports_artifact.h"
#include "oir_module_interface_artifact.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OccModuleImportBundleStatus {
    OCC_MODULE_IMPORT_BUNDLE_OK = 0,
    OCC_MODULE_IMPORT_BUNDLE_INVALID_ARGUMENT = 1,
    OCC_MODULE_IMPORT_BUNDLE_IMPORTER_MISMATCH = 2,
    OCC_MODULE_IMPORT_BUNDLE_INTERFACE_MISSING = 3,
    OCC_MODULE_IMPORT_BUNDLE_DUPLICATE_INTERFACE = 4,
    OCC_MODULE_IMPORT_BUNDLE_UNUSED_INTERFACE = 5,
    OCC_MODULE_IMPORT_BUNDLE_OUT_OF_MEMORY = 6,
    OCC_MODULE_IMPORT_BUNDLE_LIMIT_EXCEEDED = 7
} OccModuleImportBundleStatus;

typedef struct OccModuleImportBindingView {
    const OirModuleImportRecord* binding;
    const OccModuleInterface* module_interface;
} OccModuleImportBindingView;

/* Non-owning views into the manifest and decoded Module Interface artifacts.
 * Destroy releases only the binding-view array, never the input artifacts. */
typedef struct OccModuleImportBundle {
    const OirModuleImports* imports;
    OccModuleImportBindingView* bindings;
    size_t binding_count;
} OccModuleImportBundle;

/* Match every manifest dependency identity to exactly one supplied verified
 * Module Interface. Multiple USE records may share one interface. */
OccModuleImportBundleStatus occ_module_import_bundle_bind(
    const char* expected_importer_identity,
    const OirModuleImports* imports,
    const OirModuleInterfaceArtifactView* interfaces,
    size_t interface_count,
    OccModuleImportBundle* out_bundle
);

void occ_module_import_bundle_destroy(OccModuleImportBundle* bundle);
const char* occ_module_import_bundle_status_string(OccModuleImportBundleStatus status);

#ifdef __cplusplus
}
#endif
#endif
