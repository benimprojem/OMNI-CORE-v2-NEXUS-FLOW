#include "semantic_import_environment.h"

#include <stdlib.h>
#include <string.h>

OccSemanticImportEnvironmentStatus occ_semantic_import_environment_init(
    const OccModuleImportBundle* bundle,
    OccSemanticImportEnvironment* out_environment
) {
    size_t i;
    if (!bundle || !out_environment ||
        (bundle->binding_count && !bundle->bindings))
        return OCC_SEMANTIC_IMPORT_ENV_INVALID_ARGUMENT;
    memset(out_environment, 0, sizeof(*out_environment));
    if (bundle->binding_count > SIZE_MAX / sizeof(*out_environment->bindings))
        return OCC_SEMANTIC_IMPORT_ENV_LIMIT_EXCEEDED;
    if (bundle->binding_count) {
        out_environment->bindings = (OccSemanticImportEnvironmentBinding*)
            calloc(bundle->binding_count, sizeof(*out_environment->bindings));
        if (!out_environment->bindings)
            return OCC_SEMANTIC_IMPORT_ENV_OUT_OF_MEMORY;
    }
    out_environment->binding_count = bundle->binding_count;
    for (i = 0u; i < bundle->binding_count; ++i) {
        const OccModuleImportBindingView* source = &bundle->bindings[i];
        OccSemanticImportEnvironmentBinding* target =
            &out_environment->bindings[i];
        if (!source->binding || !source->module_interface ||
            !source->binding->dependency_identity ||
            !source->module_interface->canonical_identity ||
            strcmp(source->binding->dependency_identity,
                   source->module_interface->canonical_identity) != 0) {
            occ_semantic_import_environment_destroy(out_environment);
            return OCC_SEMANTIC_IMPORT_ENV_INVALID_ARGUMENT;
        }
        target->use_ordinal = source->binding->use_ordinal;
        target->requested_path = source->binding->requested_path;
        target->dependency_identity = source->binding->dependency_identity;
        target->alias = source->binding->alias;
        target->namespace_name = source->binding->namespace_name;
        target->module_interface = source->module_interface;
    }
    return OCC_SEMANTIC_IMPORT_ENV_OK;
}

OccSemanticImportEnvironmentStatus occ_semantic_import_environment_get_binding(
    const OccSemanticImportEnvironment* environment,
    size_t binding_index,
    OccSemanticImportEnvironmentBinding* out_binding
) {
    if (!environment || !out_binding ||
        (environment->binding_count && !environment->bindings))
        return OCC_SEMANTIC_IMPORT_ENV_INVALID_ARGUMENT;
    if (binding_index >= environment->binding_count)
        return OCC_SEMANTIC_IMPORT_ENV_NOT_FOUND;
    *out_binding = environment->bindings[binding_index];
    return OCC_SEMANTIC_IMPORT_ENV_OK;
}

OccSemanticImportEnvironmentStatus occ_semantic_import_environment_find_symbol(
    const OccSemanticImportEnvironment* environment,
    size_t binding_index,
    const char* name,
    OccSemanticImportedSymbol* out_symbol
) {
    const OccSemanticImportEnvironmentBinding* binding;
    const OccModuleInterface* interface;
    const OccModuleInterfaceSymbol* match = NULL;
    size_t i;
    if (!environment || !name || !name[0] || !out_symbol ||
        (environment->binding_count && !environment->bindings))
        return OCC_SEMANTIC_IMPORT_ENV_INVALID_ARGUMENT;
    if (binding_index >= environment->binding_count)
        return OCC_SEMANTIC_IMPORT_ENV_NOT_FOUND;
    binding = &environment->bindings[binding_index];
    interface = binding->module_interface;
    if (!interface || (interface->symbol_count && !interface->symbols))
        return OCC_SEMANTIC_IMPORT_ENV_INVALID_ARGUMENT;
    for (i = 0u; i < interface->symbol_count; ++i) {
        const OccModuleInterfaceSymbol* candidate = &interface->symbols[i];
        if (candidate->name && strcmp(candidate->name, name) == 0) {
            if (match) return OCC_SEMANTIC_IMPORT_ENV_AMBIGUOUS;
            match = candidate;
        }
    }
    if (!match) return OCC_SEMANTIC_IMPORT_ENV_NOT_FOUND;
    out_symbol->source_symbol_id = match->source_symbol_id;
    out_symbol->source_type_id = match->type_id;
    out_symbol->source_scope_id = match->scope_id;
    out_symbol->kind = match->kind;
    out_symbol->flags = match->flags;
    out_symbol->name = match->name;
    return OCC_SEMANTIC_IMPORT_ENV_OK;
}

OccSemanticImportEnvironmentStatus occ_semantic_import_environment_find_type(
    const OccSemanticImportEnvironment* environment,
    size_t binding_index,
    uint32_t source_type_id,
    OccSemanticImportedType* out_type
) {
    const OccModuleInterface* interface;
    const OccModuleInterfaceType* match = NULL;
    size_t i;
    if (!environment || !out_type || source_type_id == 0u ||
        (environment->binding_count && !environment->bindings))
        return OCC_SEMANTIC_IMPORT_ENV_INVALID_ARGUMENT;
    if (binding_index >= environment->binding_count)
        return OCC_SEMANTIC_IMPORT_ENV_NOT_FOUND;
    interface = environment->bindings[binding_index].module_interface;
    if (!interface || (interface->type_count && !interface->types))
        return OCC_SEMANTIC_IMPORT_ENV_INVALID_ARGUMENT;
    for (i = 0u; i < interface->type_count; ++i) {
        const OccModuleInterfaceType* candidate = &interface->types[i];
        if (candidate->source_type_id == source_type_id) {
            if (match) return OCC_SEMANTIC_IMPORT_ENV_AMBIGUOUS;
            match = candidate;
        }
    }
    if (!match) return OCC_SEMANTIC_IMPORT_ENV_NOT_FOUND;
    out_type->source_type_id = match->source_type_id;
    out_type->referenced_symbol_id = match->referenced_symbol_id;
    out_type->base_type_id = match->base_type_id;
    out_type->kind = match->kind;
    out_type->flags = match->flags;
    out_type->name = match->name;
    return OCC_SEMANTIC_IMPORT_ENV_OK;
}

void occ_semantic_import_environment_destroy(
    OccSemanticImportEnvironment* environment
) {
    if (!environment) return;
    free(environment->bindings);
    memset(environment, 0, sizeof(*environment));
}

const char* occ_semantic_import_environment_status_string(
    OccSemanticImportEnvironmentStatus status
) {
    switch (status) {
        case OCC_SEMANTIC_IMPORT_ENV_OK: return "ok";
        case OCC_SEMANTIC_IMPORT_ENV_INVALID_ARGUMENT: return "invalid argument";
        case OCC_SEMANTIC_IMPORT_ENV_OUT_OF_MEMORY: return "out of memory";
        case OCC_SEMANTIC_IMPORT_ENV_NOT_FOUND: return "not found";
        case OCC_SEMANTIC_IMPORT_ENV_AMBIGUOUS: return "ambiguous";
        case OCC_SEMANTIC_IMPORT_ENV_LIMIT_EXCEEDED: return "limit exceeded";
        default: return "unknown semantic import environment status";
    }
}
