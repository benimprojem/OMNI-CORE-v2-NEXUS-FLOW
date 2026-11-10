#include "module_interface.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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

static int type_is_selected(
    const SemanticResult* result,
    const OccModuleInterface* interface,
    uint32_t type_id
) {
    size_t i;
    if (type_id == 0u) return 0;
    for (i = 0u; i < interface->type_count; ++i)
        if (interface->types[i].source_type_id == type_id) return 1;
    (void)result;
    return 0;
}

static int symbol_is_selected(
    const OccModuleInterface* interface, uint32_t symbol_id
) {
    size_t i;
    if (symbol_id == 0u) return 0;
    for (i = 0u; i < interface->symbol_count; ++i)
        if (interface->symbols[i].source_symbol_id == symbol_id) return 1;
    return 0;
}

static OccModuleInterfaceStatus select_type_closure(
    const SemanticResult* result, OccModuleInterface* interface,
    uint32_t initial_type_id
) {
    uint32_t pending[SEMANTIC_RESULT_MAX_RECORDS > 0u ? 64u : 1u];
    size_t pending_count = 0u;
    uint32_t current = initial_type_id;
    if (current == 0u) return OCC_MODULE_INTERFACE_OK;
    if (current > result->type_count) return OCC_MODULE_INTERFACE_INVALID_SEMANTIC_RESULT;
    pending[pending_count++] = current;
    while (pending_count) {
        const SemanticResultType* source;
        OccModuleInterfaceType* target;
        uint32_t id = pending[--pending_count];
        if (type_is_selected(result, interface, id)) continue;
        if (id == 0u || id > result->type_count ||
            interface->type_count >= SEMANTIC_RESULT_MAX_RECORDS)
            return OCC_MODULE_INTERFACE_LIMIT_EXCEEDED;
        source = &result->types[id - 1u];
        target = (OccModuleInterfaceType*)realloc(interface->types,
            (interface->type_count + 1u) * sizeof(*target));
        if (!target) return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
        interface->types = target;
        target = &interface->types[interface->type_count];
        memset(target, 0, sizeof(*target));
        target->source_type_id = source->type_id;
        target->kind = source->kind;
        target->flags = source->flags;
        target->referenced_symbol_id = source->referenced_symbol_id;
        target->base_type_id = source->base_type_id;
        target->name = interface_strdup(source->name);
        if (source->name && !target->name) return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
        ++interface->type_count;
        if (source->base_type_id) {
            if (pending_count >= sizeof(pending) / sizeof(pending[0]))
                return OCC_MODULE_INTERFACE_LIMIT_EXCEEDED;
            pending[pending_count++] = source->base_type_id;
        }
    }
    return OCC_MODULE_INTERFACE_OK;
}

static OccModuleInterfaceStatus append_symbol(
    const SemanticResultSymbol* source, OccModuleInterface* interface
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
    return OCC_MODULE_INTERFACE_OK;
}

static OccModuleInterfaceStatus append_relation(
    const SemanticResultRelation* source, OccModuleInterface* interface
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
    if (source->name && !target->name) return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
    ++interface->relation_count;
    return OCC_MODULE_INTERFACE_OK;
}

OccModuleInterfaceStatus occ_module_interface_build(
    const char* canonical_identity,
    const SemanticResult* semantic_result,
    OccModuleInterface* out_interface
) {
    OccModuleInterfaceStatus status;
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

    out_interface->canonical_identity = interface_strdup(canonical_identity);
    if (!out_interface->canonical_identity) return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
    out_interface->schema_major = OCC_MODULE_INTERFACE_SCHEMA_MAJOR;
    out_interface->schema_minor = OCC_MODULE_INTERFACE_SCHEMA_MINOR;

    for (i = 0u; i < semantic_result->symbol_count; ++i) {
        const SemanticResultSymbol* symbol = &semantic_result->symbols[i];
        if (!symbol_is_visible(symbol)) continue;
        status = append_symbol(symbol, out_interface);
        if (status != OCC_MODULE_INTERFACE_OK) goto fail;
        status = select_type_closure(semantic_result, out_interface, symbol->type_id);
        if (status != OCC_MODULE_INTERFACE_OK) goto fail;
    }
    for (i = 0u; i < semantic_result->relation_count; ++i) {
        const SemanticResultRelation* relation = &semantic_result->relations[i];
        if (!symbol_is_selected(out_interface, relation->owner_symbol_id)) continue;
        if (relation->related_symbol_id != 0u &&
            !symbol_is_selected(out_interface, relation->related_symbol_id))
            continue;
        status = append_relation(relation, out_interface);
        if (status != OCC_MODULE_INTERFACE_OK) goto fail;
        status = select_type_closure(semantic_result, out_interface, relation->type_id);
        if (status != OCC_MODULE_INTERFACE_OK) goto fail;
    }
    return OCC_MODULE_INTERFACE_OK;
fail:
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
