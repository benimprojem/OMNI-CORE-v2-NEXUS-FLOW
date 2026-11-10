#ifdef NDEBUG
#undef NDEBUG
#endif
#include "semantic_import_environment.h"

#include <assert.h>
#include <string.h>

int main(void) {
    OirModuleImportRecord records[2];
    OirModuleImports imports;
    OccModuleInterfaceSymbol symbols[3];
    OccModuleInterfaceType types[1];
    OccModuleInterface interface;
    OccModuleImportBindingView bundle_bindings[2];
    OccModuleImportBundle bundle;
    OccSemanticImportEnvironment environment;
    OccSemanticImportEnvironmentBinding binding;
    OccSemanticImportedSymbol symbol;
    OccSemanticImportedType type;

    memset(records, 0, sizeof(records));
    memset(&imports, 0, sizeof(imports));
    memset(symbols, 0, sizeof(symbols));
    memset(types, 0, sizeof(types));
    memset(&interface, 0, sizeof(interface));
    memset(bundle_bindings, 0, sizeof(bundle_bindings));
    memset(&bundle, 0, sizeof(bundle));
    memset(&environment, 0, sizeof(environment));

    records[0].use_ordinal = 1u;
    records[0].requested_path = "math";
    records[0].dependency_identity = "file:///lib/math.nxf";
    records[0].alias = "m";
    records[1].use_ordinal = 2u;
    records[1].requested_path = "math";
    records[1].dependency_identity = "file:///lib/math.nxf";
    records[1].namespace_name = "core";

    imports.importer_identity = "file:///project/main.nxf";
    imports.records = records;
    imports.record_count = 2u;

    symbols[0].source_symbol_id = 4u;
    symbols[0].scope_id = 1u;
    symbols[0].type_id = 1u;
    symbols[0].kind = SEMANTIC_RESULT_SYMBOL_FUNCTION;
    symbols[0].flags = SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC;
    symbols[0].name = "sum";
    symbols[1].source_symbol_id = 5u;
    symbols[1].scope_id = 1u;
    symbols[1].type_id = 1u;
    symbols[1].kind = SEMANTIC_RESULT_SYMBOL_FUNCTION;
    symbols[1].flags = SEMANTIC_RESULT_SYMBOL_FLAG_EXPORTED;
    symbols[1].name = "average";
    symbols[2].source_symbol_id = 6u;
    symbols[2].scope_id = 1u;
    symbols[2].type_id = 1u;
    symbols[2].kind = SEMANTIC_RESULT_SYMBOL_FUNCTION;
    symbols[2].flags = SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC;
    symbols[2].name = "sum";

    types[0].source_type_id = 1u;
    types[0].kind = SEMANTIC_RESULT_TYPE_PRIMITIVE;
    types[0].name = "u32";

    interface.canonical_identity = "file:///lib/math.nxf";
    interface.schema_major = OCC_MODULE_INTERFACE_SCHEMA_MAJOR;
    interface.schema_minor = OCC_MODULE_INTERFACE_SCHEMA_MINOR;
    interface.symbols = symbols;
    interface.symbol_count = 3u;
    interface.types = types;
    interface.type_count = 1u;

    bundle_bindings[0].binding = &records[0];
    bundle_bindings[0].module_interface = &interface;
    bundle_bindings[1].binding = &records[1];
    bundle_bindings[1].module_interface = &interface;
    bundle.imports = &imports;
    bundle.bindings = bundle_bindings;
    bundle.binding_count = 2u;

    assert(occ_semantic_import_environment_init(&bundle, &environment) ==
        OCC_SEMANTIC_IMPORT_ENV_OK);
    assert(environment.binding_count == 2u);
    assert(occ_semantic_import_environment_get_binding(
        &environment, 0u, &binding) == OCC_SEMANTIC_IMPORT_ENV_OK);
    assert(binding.use_ordinal == 1u);
    assert(strcmp(binding.alias, "m") == 0);
    assert(strcmp(binding.dependency_identity, "file:///lib/math.nxf") == 0);
    assert(occ_semantic_import_environment_get_binding(
        &environment, 1u, &binding) == OCC_SEMANTIC_IMPORT_ENV_OK);
    assert(binding.use_ordinal == 2u);
    assert(strcmp(binding.namespace_name, "core") == 0);

    assert(occ_semantic_import_environment_find_symbol(
        &environment, 0u, "average", &symbol) == OCC_SEMANTIC_IMPORT_ENV_OK);
    assert(symbol.source_symbol_id == 5u);
    assert(symbol.flags == SEMANTIC_RESULT_SYMBOL_FLAG_EXPORTED);
    assert(strcmp(symbol.name, "average") == 0);
    assert(occ_semantic_import_environment_find_symbol(
        &environment, 0u, "sum", &symbol) == OCC_SEMANTIC_IMPORT_ENV_AMBIGUOUS);
    assert(occ_semantic_import_environment_find_symbol(
        &environment, 0u, "missing", &symbol) == OCC_SEMANTIC_IMPORT_ENV_NOT_FOUND);

    assert(occ_semantic_import_environment_find_type(
        &environment, 0u, 1u, &type) == OCC_SEMANTIC_IMPORT_ENV_OK);
    assert(type.source_type_id == 1u);
    assert(strcmp(type.name, "u32") == 0);
    assert(occ_semantic_import_environment_find_type(
        &environment, 0u, 99u, &type) == OCC_SEMANTIC_IMPORT_ENV_NOT_FOUND);

    occ_semantic_import_environment_destroy(&environment);
    assert(environment.bindings == NULL && environment.binding_count == 0u);
    return 0;
}
