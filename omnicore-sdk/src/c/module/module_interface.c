#include "module_interface.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char* interface_strdup(const char* value) {
    size_t length;
    char* copy;
    if (!value) return NULL;
    length = strlen(value);
    if (length == SIZE_MAX) return NULL;
    copy = (char*)malloc(length + 1u);
    if (copy) memcpy(copy, value, length + 1u);
    return copy;
}

static int symbol_is_visible(const SemanticResultSymbol* symbol) {
    return symbol && (symbol->flags &
        (SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC |
         SEMANTIC_RESULT_SYMBOL_FLAG_EXPORTED)) != 0u;
}

static OccModuleInterfaceStatus select_type_closure(
    const SemanticResult* result,
    OccModuleInterface* interface,
    uint8_t* selected_types,
    uint32_t initial_type_id
) {
    uint32_t current = initial_type_id;
    while (current != 0u) {
        const SemanticResultType* source;
        OccModuleInterfaceType* grown;
        OccModuleInterfaceType* target;
        if (current > result->type_count)
            return OCC_MODULE_INTERFACE_INVALID_SEMANTIC_RESULT;
        if (selected_types[current]) break;
        if (interface->type_count >= SEMANTIC_RESULT_MAX_RECORDS)
            return OCC_MODULE_INTERFACE_LIMIT_EXCEEDED;
        source = &result->types[current - 1u];
        grown = (OccModuleInterfaceType*)realloc(interface->types,
            (interface->type_count + 1u) * sizeof(*grown));
        if (!grown) return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
        interface->types = grown;
        target = &interface->types[interface->type_count];
        memset(target, 0, sizeof(*target));
        target->source_type_id = source->type_id;
        target->kind = source->kind;
        target->flags = source->flags;
        target->referenced_symbol_id = source->referenced_symbol_id;
        target->base_type_id = source->base_type_id;
        target->name = source->name ? interface_strdup(source->name) : NULL;
        if (source->name && !target->name)
            return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
        ++interface->type_count;
        selected_types[current] = 1u;
        current = source->base_type_id;
    }
    return OCC_MODULE_INTERFACE_OK;
}

static OccModuleInterfaceStatus append_symbol(
    const SemanticResultSymbol* source,
    OccModuleInterface* interface,
    uint8_t* selected_symbols
) {
    OccModuleInterfaceSymbol* grown;
    OccModuleInterfaceSymbol* target;
    if (interface->symbol_count >= SEMANTIC_RESULT_MAX_RECORDS)
        return OCC_MODULE_INTERFACE_LIMIT_EXCEEDED;
    grown = (OccModuleInterfaceSymbol*)realloc(interface->symbols,
        (interface->symbol_count + 1u) * sizeof(*grown));
    if (!grown) return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
    interface->symbols = grown;
    target = &interface->symbols[interface->symbol_count];
    memset(target, 0, sizeof(*target));
    target->source_symbol_id = source->symbol_id;
    target->scope_id = source->scope_id;
    target->declaration_node_id = source->declaration_node_id;
    target->type_id = source->type_id;
    target->kind = source->kind;
    target->flags = source->flags;
    target->name = interface_strdup(source->name);
    if (!target->name) return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
    ++interface->symbol_count;
    selected_symbols[source->symbol_id] = 1u;
    return OCC_MODULE_INTERFACE_OK;
}

static OccModuleInterfaceStatus append_relation(
    const SemanticResultRelation* source,
    OccModuleInterface* interface
) {
    OccModuleInterfaceRelation* grown;
    OccModuleInterfaceRelation* target;
    if (interface->relation_count >= SEMANTIC_RESULT_MAX_RECORDS)
        return OCC_MODULE_INTERFACE_LIMIT_EXCEEDED;
    grown = (OccModuleInterfaceRelation*)realloc(interface->relations,
        (interface->relation_count + 1u) * sizeof(*grown));
    if (!grown) return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
    interface->relations = grown;
    target = &interface->relations[interface->relation_count];
    memset(target, 0, sizeof(*target));
    target->owner_source_symbol_id = source->owner_symbol_id;
    target->related_source_symbol_id = source->related_symbol_id;
    target->related_node_id = source->related_node_id;
    target->type_id = source->type_id;
    target->ordinal = source->ordinal;
    target->kind = source->kind;
    target->flags = source->flags;
    target->name = source->name ? interface_strdup(source->name) : NULL;
    if (source->name && !target->name)
        return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
    ++interface->relation_count;
    return OCC_MODULE_INTERFACE_OK;
}

OccModuleInterfaceStatus occ_module_interface_build(
    const char* canonical_identity,
    const SemanticResult* semantic_result,
    OccModuleInterface* out_interface
) {
    OccModuleInterfaceStatus status;
    uint8_t* selected_symbols = NULL;
    uint8_t* selected_types = NULL;
    uint32_t i;
    if (!canonical_identity || !canonical_identity[0] ||
        !semantic_result || !out_interface)
        return OCC_MODULE_INTERFACE_INVALID_ARGUMENT;
    memset(out_interface, 0, sizeof(*out_interface));
    if (!semantic_result->finalized || !semantic_result->scopes ||
        semantic_result->scope_count == 0u ||
        (semantic_result->symbol_count && !semantic_result->symbols) ||
        (semantic_result->type_count && !semantic_result->types) ||
        (semantic_result->relation_count && !semantic_result->relations))
        return OCC_MODULE_INTERFACE_INVALID_SEMANTIC_RESULT;

    selected_symbols = (uint8_t*)calloc(
        (size_t)semantic_result->symbol_count + 1u, 1u);
    selected_types = (uint8_t*)calloc(
        (size_t)semantic_result->type_count + 1u, 1u);
    if (!selected_symbols || !selected_types) {
        free(selected_symbols);
        free(selected_types);
        return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
    }

    out_interface->canonical_identity = interface_strdup(canonical_identity);
    if (!out_interface->canonical_identity) {
        free(selected_symbols);
        free(selected_types);
        return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
    }
    out_interface->schema_major = OCC_MODULE_INTERFACE_SCHEMA_MAJOR;
    out_interface->schema_minor = OCC_MODULE_INTERFACE_SCHEMA_MINOR;

    for (i = 0u; i < semantic_result->symbol_count; ++i) {
        const SemanticResultSymbol* symbol = &semantic_result->symbols[i];
        if (!symbol_is_visible(symbol)) continue;
        status = append_symbol(symbol, out_interface, selected_symbols);
        if (status != OCC_MODULE_INTERFACE_OK) goto fail;
        status = select_type_closure(semantic_result, out_interface,
            selected_types, symbol->type_id);
        if (status != OCC_MODULE_INTERFACE_OK) goto fail;
    }

    for (i = 0u; i < semantic_result->relation_count; ++i) {
        const SemanticResultRelation* relation = &semantic_result->relations[i];
        if (relation->owner_symbol_id == 0u ||
            relation->owner_symbol_id > semantic_result->symbol_count ||
            !selected_symbols[relation->owner_symbol_id]) continue;
        if (relation->related_symbol_id != 0u &&
            !selected_symbols[relation->related_symbol_id]) continue;
        status = append_relation(relation, out_interface);
        if (status != OCC_MODULE_INTERFACE_OK) goto fail;
        status = select_type_closure(semantic_result, out_interface,
            selected_types, relation->type_id);
        if (status != OCC_MODULE_INTERFACE_OK) goto fail;
    }

    free(selected_symbols);
    free(selected_types);
    return OCC_MODULE_INTERFACE_OK;

fail:
    free(selected_symbols);
    free(selected_types);
    occ_module_interface_destroy(out_interface);
    return status;
}

void occ_module_interface_destroy(OccModuleInterface* module_interface) {
    size_t i;
    if (!module_interface) return;
    free(module_interface->canonical_identity);
    for (i = 0u; i < module_interface->symbol_count; ++i)
        free(module_interface->symbols[i].name);
    for (i = 0u; i < module_interface->type_count; ++i)
        free(module_interface->types[i].name);
    for (i = 0u; i < module_interface->relation_count; ++i)
        free(module_interface->relations[i].name);
    free(module_interface->symbols);
    free(module_interface->types);
    free(module_interface->relations);
    memset(module_interface, 0, sizeof(*module_interface));
}

const char* occ_module_interface_status_string(OccModuleInterfaceStatus status) {
    switch (status) {
        case OCC_MODULE_INTERFACE_OK: return "ok";
        case OCC_MODULE_INTERFACE_INVALID_ARGUMENT: return "invalid argument";
        case OCC_MODULE_INTERFACE_INVALID_SEMANTIC_RESULT: return "invalid semantic result";
        case OCC_MODULE_INTERFACE_OUT_OF_MEMORY: return "out of memory";
        case OCC_MODULE_INTERFACE_LIMIT_EXCEEDED: return "interface limit exceeded";
        default: return "unknown module interface status";
    }
}
