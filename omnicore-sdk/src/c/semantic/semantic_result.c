#include "semantic_result.h"

#include <stdlib.h>
#include <string.h>

#define SEMANTIC_RESULT_MAX_NAME_BYTES (16u * 1024u * 1024u)

static int should_fail_allocation(SemanticResult* result) {
    if (!result) return 0;
    if (result->test_allocation_attempts != UINT32_MAX) ++result->test_allocation_attempts;
    return result->test_allocation_fail_at != 0u && result->test_allocation_attempts == result->test_allocation_fail_at;
}
static void* result_reallocate(SemanticResult* result, void* pointer, size_t size) {
    if (should_fail_allocation(result)) return NULL;
    return realloc(pointer, size);
}
static void* result_allocate(SemanticResult* result, size_t size) {
    if (should_fail_allocation(result)) return NULL;
    return malloc(size);
}
void semantic_result_test_fail_allocation_at(SemanticResult* result, uint32_t allocation_index) {
    if (!result) return;
    result->test_allocation_fail_at = allocation_index;
    result->test_allocation_attempts = 0u;
}
uint32_t semantic_result_test_get_allocation_attempts(const SemanticResult* result) {
    return result ? result->test_allocation_attempts : 0u;
}
static SemanticResultStatus reserve_array(
    SemanticResult* result,
    void** array,
    uint32_t* capacity,
    uint32_t required,
    size_t element_size
) {
    uint32_t next_capacity;
    void* next;

    if (required <= *capacity) return SEMANTIC_RESULT_OK;
    if (required > SEMANTIC_RESULT_MAX_RECORDS ||
        element_size == 0u ||
        (size_t)required > SIZE_MAX / element_size) {
        return SEMANTIC_RESULT_LIMIT_EXCEEDED;
    }

    next_capacity = *capacity ? *capacity : 8u;
    while (next_capacity < required) {
        if (next_capacity > SEMANTIC_RESULT_MAX_RECORDS / 2u) {
            next_capacity = SEMANTIC_RESULT_MAX_RECORDS;
            break;
        }
        next_capacity *= 2u;
    }
    if (next_capacity < required ||
        (size_t)next_capacity > SIZE_MAX / element_size) {
        return SEMANTIC_RESULT_LIMIT_EXCEEDED;
    }

    next = result_reallocate(result, *array, (size_t)next_capacity * element_size);
    if (!next) return SEMANTIC_RESULT_OUT_OF_MEMORY;
    *array = next;
    *capacity = next_capacity;
    return SEMANTIC_RESULT_OK;
}

static int valid_utf8(const char* text) {
    const uint8_t* bytes = (const uint8_t*)text;
    size_t length, i = 0u;

    if (!text) return 1;
    length = strlen(text);
    if (length > SEMANTIC_RESULT_MAX_NAME_BYTES) return 0;

    while (i < length) {
        uint8_t first = bytes[i];
        uint32_t codepoint;
        uint32_t extra;

        if (first == 0u) return 0;
        if (first <= 0x7fu) {
            ++i;
            continue;
        }
        if (first >= 0xc2u && first <= 0xdfu) {
            extra = 1u;
            codepoint = (uint32_t)(first & 0x1fu);
        } else if (first >= 0xe0u && first <= 0xefu) {
            extra = 2u;
            codepoint = (uint32_t)(first & 0x0fu);
        } else if (first >= 0xf0u && first <= 0xf4u) {
            extra = 3u;
            codepoint = (uint32_t)(first & 0x07u);
        } else {
            return 0;
        }

        if ((size_t)extra > length - i - 1u) return 0;
        while (extra != 0u) {
            uint8_t next = bytes[++i];
            if ((next & 0xc0u) != 0x80u) return 0;
            codepoint = (codepoint << 6) | (uint32_t)(next & 0x3fu);
            --extra;
        }

        if (codepoint < 0x80u ||
            (codepoint < 0x800u && first >= 0xe0u) ||
            (codepoint < 0x10000u && first >= 0xf0u) ||
            (codepoint >= 0xd800u && codepoint <= 0xdfffu) ||
            codepoint > 0x10ffffu) {
            return 0;
        }
        ++i;
    }
    return 1;
}

static char* copy_name(SemanticResult* result, const char* name) {
    size_t length;
    char* copy;

    if (!name) return NULL;
    length = strlen(name);
    if (length > SEMANTIC_RESULT_MAX_NAME_BYTES || !valid_utf8(name)) return NULL;
    copy = (char*)result_allocate(result, length + 1u);
    if (!copy) return NULL;
    memcpy(copy, name, length + 1u);
    return copy;
}

static int names_equal(const char* left, const char* right) {
    if (!left || !right) return left == right;
    return strcmp(left, right) == 0;
}

static int valid_type_kind(uint16_t kind) {
    return kind >= SEMANTIC_RESULT_TYPE_PRIMITIVE &&
        kind <= SEMANTIC_RESULT_TYPE_REFERENCE;
}

static int valid_symbol_kind(uint16_t kind) {
    return kind >= SEMANTIC_RESULT_SYMBOL_VARIABLE &&
        kind <= SEMANTIC_RESULT_SYMBOL_GROUP;
}

static int valid_relation_kind(uint16_t kind) {
    return kind >= SEMANTIC_RESULT_RELATION_FUNCTION_PARAMETER &&
        kind <= SEMANTIC_RESULT_RELATION_GROUP_DEFAULT_HANDLER;
}

static int relation_owner_kind_matches(uint16_t relation_kind, uint16_t symbol_kind) {
    switch (relation_kind) {
        case SEMANTIC_RESULT_RELATION_FUNCTION_PARAMETER:
            return symbol_kind == SEMANTIC_RESULT_SYMBOL_FUNCTION;
        case SEMANTIC_RESULT_RELATION_AGGREGATE_FIELD:
            return symbol_kind == SEMANTIC_RESULT_SYMBOL_STRUCT ||
                symbol_kind == SEMANTIC_RESULT_SYMBOL_UNION;
        case SEMANTIC_RESULT_RELATION_ENUM_VARIANT:
        case SEMANTIC_RESULT_RELATION_ENUM_PAYLOAD_FIELD:
            return symbol_kind == SEMANTIC_RESULT_SYMBOL_ENUM;
        case SEMANTIC_RESULT_RELATION_GROUP_MEMBER:
        case SEMANTIC_RESULT_RELATION_GROUP_TYPE_BINDING:
        case SEMANTIC_RESULT_RELATION_GROUP_METHOD_IMPLEMENTATION:
        case SEMANTIC_RESULT_RELATION_GROUP_DEFAULT_HANDLER:
            return symbol_kind == SEMANTIC_RESULT_SYMBOL_GROUP;
        default:
            return 0;
    }
}

static int scope_is_ancestor(
    const SemanticResult* result,
    uint32_t ancestor_scope_id,
    uint32_t descendant_scope_id
) {
    uint32_t current = descendant_scope_id;

    while (current != 0u) {
        const SemanticResultScope* scope;
        if (current == ancestor_scope_id) return 1;
        if (current > result->scope_count) return 0;
        scope = &result->scopes[current - 1u];
        current = scope->parent_scope_id;
    }
    return 0;
}

/* Qualified Group access may target a symbol in the Group child scope,
 * but only when the Group symbol is visible and a relation exposes the target. */
static int group_relation_exposes_symbol(
    const SemanticResult* result,
    uint32_t annotation_scope_id,
    uint32_t target_symbol_id
) {
    uint32_t relation_index;
    if (!result || target_symbol_id == 0u) return 0;
    for (relation_index = 0u;
         relation_index < result->relation_count; ++relation_index) {
        const SemanticResultRelation* relation =
            &result->relations[relation_index];
        const SemanticResultSymbol* group;
        if (relation->related_symbol_id != target_symbol_id ||
            (relation->kind != SEMANTIC_RESULT_RELATION_GROUP_MEMBER &&
             relation->kind != SEMANTIC_RESULT_RELATION_GROUP_TYPE_BINDING &&
             relation->kind != SEMANTIC_RESULT_RELATION_GROUP_METHOD_IMPLEMENTATION) ||
            relation->owner_symbol_id == 0u ||
            relation->owner_symbol_id > result->symbol_count) continue;
        group = &result->symbols[relation->owner_symbol_id - 1u];
        if (group->kind == SEMANTIC_RESULT_SYMBOL_GROUP &&
            scope_is_ancestor(result, group->scope_id, annotation_scope_id))
            return 1;
    }
    return 0;
}

static int compare_u32(const void* left, const void* right) {
    uint32_t a = *(const uint32_t*)left;
    uint32_t b = *(const uint32_t*)right;
    return a < b ? -1 : (a > b ? 1 : 0);
}

SemanticResultStatus semantic_result_init(SemanticResult* result) {
    SemanticResultStatus status;

    if (!result) return SEMANTIC_RESULT_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));
    status = reserve_array(
        result, (void**)&result->scopes, &result->scope_capacity, 1u,
        sizeof(*result->scopes));
    if (status != SEMANTIC_RESULT_OK) return status;

    result->scopes[0].scope_id = 1u;
    result->scopes[0].parent_scope_id = 0u;
    result->scopes[0].depth = 0u;
    result->scopes[0].flags = 0u;
    result->scopes[0].owner_node_id = 0u;
    result->scope_count = 1u;
    return SEMANTIC_RESULT_OK;
}

void semantic_result_destroy(SemanticResult* result) {
    uint32_t i;
    if (!result) return;

    for (i = 0u; i < result->type_count; ++i) free(result->types[i].name);
    for (i = 0u; i < result->symbol_count; ++i) free(result->symbols[i].name);
    for (i = 0u; i < result->relation_count; ++i) free(result->relations[i].name);

    free(result->scopes);
    free(result->types);
    free(result->symbols);
    free(result->annotations);
    free(result->relations);
    free(result->conversions);
    memset(result, 0, sizeof(*result));
}

SemanticResultStatus semantic_result_add_scope(
    SemanticResult* result,
    uint32_t parent_scope_id,
    uint32_t depth,
    uint32_t flags,
    uint32_t owner_node_id,
    uint32_t* out_scope_id
) {
    SemanticResultStatus status;
    const SemanticResultScope* parent;
    SemanticResultScope* scope;

    if (!result || !out_scope_id) return SEMANTIC_RESULT_INVALID_ARGUMENT;
    if (result->finalized || parent_scope_id == 0u ||
        parent_scope_id > result->scope_count || owner_node_id == 0u ||
        flags != 0u || result->scope_count >= SEMANTIC_RESULT_MAX_RECORDS) {
        return SEMANTIC_RESULT_INVALID_DATA;
    }

    parent = &result->scopes[parent_scope_id - 1u];
    if (parent->depth + 1u != depth) return SEMANTIC_RESULT_INVALID_DATA;

    status = reserve_array(
        result, (void**)&result->scopes, &result->scope_capacity,
        result->scope_count + 1u, sizeof(*result->scopes));
    if (status != SEMANTIC_RESULT_OK) return status;

    scope = &result->scopes[result->scope_count];
    scope->scope_id = result->scope_count + 1u;
    scope->parent_scope_id = parent_scope_id;
    scope->depth = depth;
    scope->flags = flags;
    scope->owner_node_id = owner_node_id;
    ++result->scope_count;
    result->finalized = 0u;
    *out_scope_id = scope->scope_id;
    return SEMANTIC_RESULT_OK;
}

SemanticResultStatus semantic_result_intern_type(
    SemanticResult* result,
    uint16_t kind,
    uint16_t flags,
    const char* name,
    uint32_t referenced_symbol_id,
    uint32_t base_type_id,
    uint32_t* out_type_id
) {
    uint32_t i;
    uint32_t new_id;
    SemanticResultStatus status;
    char* name_copy = NULL;
    SemanticResultType* type;

    if (!result || !out_type_id) return SEMANTIC_RESULT_INVALID_ARGUMENT;
    if (result->finalized || !valid_type_kind(kind) ||
        (flags & ~(SEMANTIC_RESULT_TYPE_FLAG_CONST |
            SEMANTIC_RESULT_TYPE_FLAG_HARD)) != 0u ||
        !valid_utf8(name)) {
        return SEMANTIC_RESULT_INVALID_DATA;
    }
    if (kind == SEMANTIC_RESULT_TYPE_USER_DEFINED) {
        if (referenced_symbol_id == 0u) return SEMANTIC_RESULT_INVALID_DATA;
    } else if (referenced_symbol_id != 0u) {
        return SEMANTIC_RESULT_INVALID_DATA;
    }

    new_id = result->type_count + 1u;
    if (base_type_id >= new_id && base_type_id != 0u) {
        return SEMANTIC_RESULT_INVALID_DATA;
    }

    for (i = 0u; i < result->type_count; ++i) {
        const SemanticResultType* existing = &result->types[i];
        if (existing->kind == kind && existing->flags == flags &&
            existing->referenced_symbol_id == referenced_symbol_id &&
            existing->base_type_id == base_type_id &&
            names_equal(existing->name, name)) {
            *out_type_id = existing->type_id;
            return SEMANTIC_RESULT_OK;
        }
    }

    if (result->type_count >= SEMANTIC_RESULT_MAX_RECORDS) {
        return SEMANTIC_RESULT_LIMIT_EXCEEDED;
    }
    if (name) {
        name_copy = copy_name(result, name);
        if (!name_copy) {
            return valid_utf8(name)
                ? SEMANTIC_RESULT_OUT_OF_MEMORY
                : SEMANTIC_RESULT_INVALID_DATA;
        }
    }

    status = reserve_array(
        result, (void**)&result->types, &result->type_capacity,
        result->type_count + 1u, sizeof(*result->types));
    if (status != SEMANTIC_RESULT_OK) {
        free(name_copy);
        return status;
    }

    type = &result->types[result->type_count];
    type->type_id = new_id;
    type->kind = kind;
    type->flags = flags;
    type->name = name_copy;
    type->referenced_symbol_id = referenced_symbol_id;
    type->base_type_id = base_type_id;
    ++result->type_count;
    result->finalized = 0u;
    *out_type_id = new_id;
    return SEMANTIC_RESULT_OK;
}

SemanticResultStatus semantic_result_add_symbol(
    SemanticResult* result,
    uint32_t scope_id,
    uint32_t declaration_node_id,
    uint16_t kind,
    uint16_t flags,
    const char* name,
    uint32_t type_id,
    uint32_t* out_symbol_id
) {
    uint32_t i;
    char* name_copy;
    SemanticResultStatus status;
    SemanticResultSymbol* symbol;

    if (!result || !out_symbol_id) return SEMANTIC_RESULT_INVALID_ARGUMENT;
    if (result->finalized || scope_id == 0u || scope_id > result->scope_count ||
        declaration_node_id == 0u || !valid_symbol_kind(kind) ||
        ((flags & SEMANTIC_RESULT_SYMBOL_FLAG_IMMUTABLE) != 0u &&
            (kind != SEMANTIC_RESULT_SYMBOL_VARIABLE ||
             (flags & (SEMANTIC_RESULT_SYMBOL_FLAG_CONST |
                       SEMANTIC_RESULT_SYMBOL_FLAG_IMPLICIT)) != 0u)) ||
        (flags & ~(SEMANTIC_RESULT_SYMBOL_FLAG_CONST |
            SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC |
            SEMANTIC_RESULT_SYMBOL_FLAG_EXPORTED |
            SEMANTIC_RESULT_SYMBOL_FLAG_PACKED |
            SEMANTIC_RESULT_SYMBOL_FLAG_HARD |
            SEMANTIC_RESULT_SYMBOL_FLAG_IMPLICIT |
            SEMANTIC_RESULT_SYMBOL_FLAG_IMMUTABLE)) != 0u ||
        !name || !*name || !valid_utf8(name) || type_id > result->type_count) {
        return SEMANTIC_RESULT_INVALID_DATA;
    }

    /* The current language checker uses one name namespace per lexical scope. */
    for (i = 0u; i < result->symbol_count; ++i) {
        if (result->symbols[i].scope_id == scope_id &&
            strcmp(result->symbols[i].name, name) == 0) {
            return SEMANTIC_RESULT_INVALID_DATA;
        }
    }
    if (result->symbol_count >= SEMANTIC_RESULT_MAX_RECORDS) {
        return SEMANTIC_RESULT_LIMIT_EXCEEDED;
    }

    name_copy = copy_name(result, name);
    if (!name_copy) {
        return valid_utf8(name)
            ? SEMANTIC_RESULT_OUT_OF_MEMORY
            : SEMANTIC_RESULT_INVALID_DATA;
    }

    status = reserve_array(
        result, (void**)&result->symbols, &result->symbol_capacity,
        result->symbol_count + 1u, sizeof(*result->symbols));
    if (status != SEMANTIC_RESULT_OK) {
        free(name_copy);
        return status;
    }

    symbol = &result->symbols[result->symbol_count];
    symbol->symbol_id = result->symbol_count + 1u;
    symbol->scope_id = scope_id;
    symbol->declaration_node_id = declaration_node_id;
    symbol->kind = kind;
    symbol->flags = flags;
    symbol->name = name_copy;
    symbol->type_id = type_id;
    symbol->first_relation = 0u;
    symbol->relation_count = 0u;
    ++result->symbol_count;
    result->finalized = 0u;
    *out_symbol_id = symbol->symbol_id;
    return SEMANTIC_RESULT_OK;
}

SemanticResultStatus semantic_result_set_symbol_type(
    SemanticResult* result,
    uint32_t symbol_id,
    uint32_t type_id
) {
    if (!result || symbol_id == 0u || symbol_id > result->symbol_count) {
        return SEMANTIC_RESULT_INVALID_ARGUMENT;
    }
    if (result->finalized || type_id > result->type_count) {
        return SEMANTIC_RESULT_INVALID_DATA;
    }
    result->symbols[symbol_id - 1u].type_id = type_id;
    return SEMANTIC_RESULT_OK;
}

uint32_t semantic_result_find_visible_symbol(
    const SemanticResult* result,
    uint32_t scope_id,
    const char* name
) {
    uint32_t current = scope_id;
    if (!result || !name || scope_id == 0u ||
        scope_id > result->scope_count) return 0u;

    while (current != 0u) {
        uint32_t i = result->symbol_count;
        while (i != 0u) {
            const SemanticResultSymbol* symbol = &result->symbols[--i];
            if (symbol->scope_id == current &&
                strcmp(symbol->name, name) == 0) {
                return symbol->symbol_id;
            }
        }
        current = result->scopes[current - 1u].parent_scope_id;
    }
    return 0u;
}

SemanticResultStatus semantic_result_set_annotation(
    SemanticResult* result,
    uint32_t node_id,
    uint32_t scope_id,
    uint32_t resolved_symbol_id,
    uint32_t resolved_type_id,
    uint16_t value_category,
    uint16_t flags
) {
    uint32_t low = 0u, high;
    SemanticResultStatus status;
    SemanticResultAnnotation annotation;

    if (!result) return SEMANTIC_RESULT_INVALID_ARGUMENT;
    if (result->finalized || node_id == 0u ||
        scope_id == 0u || scope_id > result->scope_count ||
        resolved_symbol_id > result->symbol_count ||
        resolved_type_id > result->type_count ||
        value_category > SEMANTIC_RESULT_VALUE_FUNCTION_DESIGNATOR ||
        (flags & (uint16_t)~SEMANTIC_RESULT_ANNOTATION_FLAG_ADDRESS_RECEIVER) != 0u) {
        return SEMANTIC_RESULT_INVALID_DATA;
    }

    high = result->annotation_count;
    while (low < high) {
        uint32_t mid = low + (high - low) / 2u;
        if (result->annotations[mid].node_id < node_id) low = mid + 1u;
        else high = mid;
    }

    annotation.node_id = node_id;
    annotation.scope_id = scope_id;
    annotation.resolved_symbol_id = resolved_symbol_id;
    annotation.resolved_type_id = resolved_type_id;
    annotation.value_category = value_category;
    annotation.flags = flags;

    if (low < result->annotation_count &&
        result->annotations[low].node_id == node_id) {
        result->annotations[low] = annotation;
        return SEMANTIC_RESULT_OK;
    }

    if (result->annotation_count >= SEMANTIC_RESULT_MAX_RECORDS) {
        return SEMANTIC_RESULT_LIMIT_EXCEEDED;
    }
    status = reserve_array(
        result, (void**)&result->annotations, &result->annotation_capacity,
        result->annotation_count + 1u, sizeof(*result->annotations));
    if (status != SEMANTIC_RESULT_OK) return status;

    memmove(&result->annotations[low + 1u], &result->annotations[low],
        (size_t)(result->annotation_count - low) * sizeof(*result->annotations));
    result->annotations[low] = annotation;
    ++result->annotation_count;
    result->finalized = 0u;
    return SEMANTIC_RESULT_OK;
}

SemanticResultStatus semantic_result_add_relation(
    SemanticResult* result,
    uint32_t owner_symbol_id,
    uint16_t kind,
    uint32_t ordinal,
    uint32_t related_symbol_id,
    uint32_t related_node_id,
    const char* name,
    uint32_t type_id
) {
    uint32_t low = 0u, high;
    char* name_copy = NULL;
    SemanticResultStatus status;
    SemanticResultRelation relation;

    if (!result) return SEMANTIC_RESULT_INVALID_ARGUMENT;
    if (result->finalized || owner_symbol_id == 0u ||
        owner_symbol_id > result->symbol_count ||
        !valid_relation_kind(kind) || related_node_id == 0u ||
        type_id > result->type_count || !valid_utf8(name)) {
        return SEMANTIC_RESULT_INVALID_DATA;
    }
    if (result->relation_count >= SEMANTIC_RESULT_MAX_RECORDS) {
        return SEMANTIC_RESULT_LIMIT_EXCEEDED;
    }

    if (name) {
        name_copy = copy_name(result, name);
        if (!name_copy) {
            return valid_utf8(name)
                ? SEMANTIC_RESULT_OUT_OF_MEMORY
                : SEMANTIC_RESULT_INVALID_DATA;
        }
    }

    relation.owner_symbol_id = owner_symbol_id;
    relation.kind = kind;
    relation.flags = 0u;
    relation.ordinal = ordinal;
    relation.related_symbol_id = related_symbol_id;
    relation.related_node_id = related_node_id;
    relation.name = name_copy;
    relation.type_id = type_id;

    high = result->relation_count;
    while (low < high) {
        uint32_t mid = low + (high - low) / 2u;
        const SemanticResultRelation* current = &result->relations[mid];
        int before = current->owner_symbol_id < owner_symbol_id ||
            (current->owner_symbol_id == owner_symbol_id &&
                (current->kind < kind ||
                    (current->kind == kind && current->ordinal < ordinal)));
        if (before) low = mid + 1u;
        else high = mid;
    }

    if (low < result->relation_count &&
        result->relations[low].owner_symbol_id == owner_symbol_id &&
        result->relations[low].kind == kind &&
        result->relations[low].ordinal == ordinal) {
        free(name_copy);
        return SEMANTIC_RESULT_INVALID_DATA;
    }

    status = reserve_array(
        result, (void**)&result->relations, &result->relation_capacity,
        result->relation_count + 1u, sizeof(*result->relations));
    if (status != SEMANTIC_RESULT_OK) {
        free(name_copy);
        return status;
    }

    memmove(&result->relations[low + 1u], &result->relations[low],
        (size_t)(result->relation_count - low) * sizeof(*result->relations));
    result->relations[low] = relation;
    ++result->relation_count;
    result->finalized = 0u;
    return SEMANTIC_RESULT_OK;
}

SemanticResultStatus semantic_result_add_conversion(
    SemanticResult* result,
    uint32_t node_id,
    uint32_t operand_ordinal,
    uint32_t source_type_id,
    uint32_t destination_type_id,
    uint16_t kind,
    uint16_t flags
) {
    uint32_t low = 0u, high;
    SemanticResultStatus status;
    SemanticResultConversion conversion;

    if (!result) return SEMANTIC_RESULT_INVALID_ARGUMENT;
    if (result->finalized || node_id == 0u ||
        source_type_id == 0u || source_type_id > result->type_count ||
        destination_type_id == 0u || destination_type_id > result->type_count ||
        kind < SEMANTIC_RESULT_CONVERSION_IDENTITY ||
        kind > SEMANTIC_RESULT_CONVERSION_HARD_TYPE ||
        (flags != SEMANTIC_RESULT_CONVERSION_FLAG_IMPLICIT &&
            flags != SEMANTIC_RESULT_CONVERSION_FLAG_EXPLICIT)) {
        return SEMANTIC_RESULT_INVALID_DATA;
    }
    if (result->conversion_count >= SEMANTIC_RESULT_MAX_RECORDS) {
        return SEMANTIC_RESULT_LIMIT_EXCEEDED;
    }

    high = result->conversion_count;
    while (low < high) {
        uint32_t mid = low + (high - low) / 2u;
        const SemanticResultConversion* current = &result->conversions[mid];
        int before = current->node_id < node_id ||
            (current->node_id == node_id &&
                current->operand_ordinal < operand_ordinal);
        if (before) low = mid + 1u;
        else high = mid;
    }
    if (low < result->conversion_count &&
        result->conversions[low].node_id == node_id &&
        result->conversions[low].operand_ordinal == operand_ordinal) {
        return SEMANTIC_RESULT_INVALID_DATA;
    }

    status = reserve_array(
        result, (void**)&result->conversions, &result->conversion_capacity,
        result->conversion_count + 1u, sizeof(*result->conversions));
    if (status != SEMANTIC_RESULT_OK) return status;

    conversion.node_id = node_id;
    conversion.operand_ordinal = operand_ordinal;
    conversion.source_type_id = source_type_id;
    conversion.destination_type_id = destination_type_id;
    conversion.kind = kind;
    conversion.flags = flags;

    memmove(&result->conversions[low + 1u], &result->conversions[low],
        (size_t)(result->conversion_count - low) * sizeof(*result->conversions));
    result->conversions[low] = conversion;
    ++result->conversion_count;
    result->finalized = 0u;
    return SEMANTIC_RESULT_OK;
}

SemanticResultStatus semantic_result_finalize(
    SemanticResult* result,
    uint32_t ast_node_count
) {
    uint32_t i;
    uint32_t* owner_nodes = NULL;

    if (!result || ast_node_count == 0u) return SEMANTIC_RESULT_INVALID_ARGUMENT;
    if (result->scope_count == 0u ||
        result->scopes[0].scope_id != 1u ||
        result->scopes[0].parent_scope_id != 0u ||
        result->scopes[0].depth != 0u ||
        result->scopes[0].flags != 0u ||
        result->scopes[0].owner_node_id != 0u) {
        return SEMANTIC_RESULT_INVALID_DATA;
    }

    if (result->scope_count > 1u) {
        owner_nodes = (uint32_t*)result_allocate(
            result, (size_t)(result->scope_count - 1u) * sizeof(*owner_nodes));
        if (!owner_nodes) return SEMANTIC_RESULT_OUT_OF_MEMORY;
    }

    for (i = 1u; i < result->scope_count; ++i) {
        const SemanticResultScope* scope = &result->scopes[i];
        if (scope->scope_id != i + 1u ||
            scope->parent_scope_id == 0u ||
            scope->parent_scope_id >= scope->scope_id ||
            scope->depth != result->scopes[scope->parent_scope_id - 1u].depth + 1u ||
            scope->flags != 0u || scope->owner_node_id == 0u ||
            scope->owner_node_id > ast_node_count) {
            free(owner_nodes);
            return SEMANTIC_RESULT_INVALID_DATA;
        }
        owner_nodes[i - 1u] = scope->owner_node_id;
    }
    if (owner_nodes) {
        qsort(owner_nodes, result->scope_count - 1u,
            sizeof(*owner_nodes), compare_u32);
        for (i = 1u; i < result->scope_count - 1u; ++i) {
            if (owner_nodes[i] == owner_nodes[i - 1u]) {
                free(owner_nodes);
                return SEMANTIC_RESULT_INVALID_DATA;
            }
        }
        free(owner_nodes);
    }

    for (i = 0u; i < result->type_count; ++i) {
        const SemanticResultType* type = &result->types[i];
        if (type->type_id != i + 1u || !valid_type_kind(type->kind) ||
            (type->flags & ~(SEMANTIC_RESULT_TYPE_FLAG_CONST |
                SEMANTIC_RESULT_TYPE_FLAG_HARD)) != 0u ||
            (type->base_type_id != 0u && type->base_type_id >= type->type_id) ||
            !valid_utf8(type->name)) {
            return SEMANTIC_RESULT_INVALID_DATA;
        }
        if ((type->kind == SEMANTIC_RESULT_TYPE_POINTER ||
                type->kind == SEMANTIC_RESULT_TYPE_REFERENCE) &&
            (type->base_type_id == 0u ||
                type->base_type_id >= type->type_id ||
                !type->name || (type->kind == SEMANTIC_RESULT_TYPE_POINTER
                    ? strcmp(type->name, "*") != 0
                    : strcmp(type->name, "&") != 0))) {
            return SEMANTIC_RESULT_INVALID_DATA;
        }
        if (type->kind == SEMANTIC_RESULT_TYPE_USER_DEFINED) {
            const SemanticResultSymbol* referenced;
            if (type->referenced_symbol_id == 0u ||
                type->referenced_symbol_id > result->symbol_count) {
                return SEMANTIC_RESULT_INVALID_DATA;
            }
            referenced = &result->symbols[type->referenced_symbol_id - 1u];
            if (referenced->kind != SEMANTIC_RESULT_SYMBOL_STRUCT &&
                referenced->kind != SEMANTIC_RESULT_SYMBOL_UNION &&
                referenced->kind != SEMANTIC_RESULT_SYMBOL_ENUM &&
                referenced->kind != SEMANTIC_RESULT_SYMBOL_NEWTYPE) {
                return SEMANTIC_RESULT_INVALID_DATA;
            }
        } else if (type->referenced_symbol_id != 0u) {
            return SEMANTIC_RESULT_INVALID_DATA;
        }
    }

    for (i = 0u; i < result->symbol_count; ++i) {
        SemanticResultSymbol* symbol = &result->symbols[i];
        uint32_t j;
        if (symbol->symbol_id != i + 1u ||
            symbol->scope_id == 0u || symbol->scope_id > result->scope_count ||
            symbol->declaration_node_id == 0u ||
            symbol->declaration_node_id > ast_node_count ||
            symbol->type_id > result->type_count ||
            !valid_symbol_kind(symbol->kind) ||
            ((symbol->flags & SEMANTIC_RESULT_SYMBOL_FLAG_IMMUTABLE) != 0u &&
                (symbol->kind != SEMANTIC_RESULT_SYMBOL_VARIABLE ||
                 (symbol->flags & SEMANTIC_RESULT_SYMBOL_FLAG_CONST) != 0u)) ||
            (symbol->flags & ~(SEMANTIC_RESULT_SYMBOL_FLAG_CONST |
                SEMANTIC_RESULT_SYMBOL_FLAG_PUBLIC |
                SEMANTIC_RESULT_SYMBOL_FLAG_EXPORTED |
                SEMANTIC_RESULT_SYMBOL_FLAG_PACKED |
                SEMANTIC_RESULT_SYMBOL_FLAG_HARD |
                SEMANTIC_RESULT_SYMBOL_FLAG_IMPLICIT |
            SEMANTIC_RESULT_SYMBOL_FLAG_IMMUTABLE)) != 0u ||
            !symbol->name || !*symbol->name || !valid_utf8(symbol->name)) {
            return SEMANTIC_RESULT_INVALID_DATA;
        }
        symbol->first_relation = 0u;
        symbol->relation_count = 0u;
        for (j = 0u; j < i; ++j) {
            if (result->symbols[j].scope_id == symbol->scope_id &&
                strcmp(result->symbols[j].name, symbol->name) == 0) {
                return SEMANTIC_RESULT_INVALID_DATA;
            }
        }
    }

    for (i = 0u; i < result->annotation_count; ++i) {
        const SemanticResultAnnotation* annotation = &result->annotations[i];
        if (annotation->node_id == 0u ||
            annotation->node_id > ast_node_count ||
            (i != 0u && annotation->node_id <= result->annotations[i - 1u].node_id) ||
            annotation->scope_id == 0u ||
            annotation->scope_id > result->scope_count ||
            annotation->resolved_symbol_id > result->symbol_count ||
            annotation->resolved_type_id > result->type_count ||
            annotation->value_category > SEMANTIC_RESULT_VALUE_FUNCTION_DESIGNATOR ||
            (annotation->flags & (uint16_t)~SEMANTIC_RESULT_ANNOTATION_FLAG_ADDRESS_RECEIVER) != 0u) {
            return SEMANTIC_RESULT_INVALID_DATA;
        }
        if ((annotation->flags &
                SEMANTIC_RESULT_ANNOTATION_FLAG_ADDRESS_RECEIVER) != 0u &&
            (annotation->resolved_symbol_id == 0u ||
                result->symbols[annotation->resolved_symbol_id - 1u].kind !=
                    SEMANTIC_RESULT_SYMBOL_VARIABLE ||
                !result->symbols[annotation->resolved_symbol_id - 1u].name ||
                strcmp(result->symbols[annotation->resolved_symbol_id - 1u].name,
                    "self") != 0)) {
            return SEMANTIC_RESULT_INVALID_DATA;
        }
        if (annotation->resolved_symbol_id != 0u) {
            const SemanticResultSymbol* symbol =
                &result->symbols[annotation->resolved_symbol_id - 1u];
            if (!scope_is_ancestor(
                    result, symbol->scope_id, annotation->scope_id) &&
                !group_relation_exposes_symbol(
                    result, annotation->scope_id,
                    annotation->resolved_symbol_id)) {
                return SEMANTIC_RESULT_INVALID_DATA;
            }
        }
    }

    for (i = 0u; i < result->relation_count; ++i) {
        SemanticResultRelation* relation = &result->relations[i];
        SemanticResultSymbol* owner;
        if (relation->owner_symbol_id == 0u ||
            relation->owner_symbol_id > result->symbol_count ||
            !valid_relation_kind(relation->kind) ||
            relation->flags != 0u ||
            relation->related_node_id == 0u ||
            relation->related_node_id > ast_node_count ||
            relation->related_symbol_id > result->symbol_count ||
            relation->type_id > result->type_count ||
            !valid_utf8(relation->name)) {
            return SEMANTIC_RESULT_INVALID_DATA;
        }
        owner = &result->symbols[relation->owner_symbol_id - 1u];
        if (!relation_owner_kind_matches(relation->kind, owner->kind)) {
            return SEMANTIC_RESULT_INVALID_DATA;
        }
        if (i != 0u) {
            const SemanticResultRelation* previous = &result->relations[i - 1u];
            if (relation->owner_symbol_id < previous->owner_symbol_id ||
                (relation->owner_symbol_id == previous->owner_symbol_id &&
                    relation->kind < previous->kind)) {
                return SEMANTIC_RESULT_INVALID_DATA;
            }
            if (relation->owner_symbol_id == previous->owner_symbol_id &&
                relation->kind == previous->kind) {
                if (previous->ordinal == UINT32_MAX ||
                    relation->ordinal != previous->ordinal + 1u) {
                    return SEMANTIC_RESULT_INVALID_DATA;
                }
            } else if (relation->ordinal != 0u) {
                return SEMANTIC_RESULT_INVALID_DATA;
            }
        } else if (relation->ordinal != 0u) {
            return SEMANTIC_RESULT_INVALID_DATA;
        }

        if ((relation->kind == SEMANTIC_RESULT_RELATION_GROUP_TYPE_BINDING ||
                relation->kind == SEMANTIC_RESULT_RELATION_GROUP_METHOD_IMPLEMENTATION) &&
            relation->related_symbol_id == 0u) {
            return SEMANTIC_RESULT_INVALID_DATA;
        }
        if (relation->kind == SEMANTIC_RESULT_RELATION_GROUP_DEFAULT_HANDLER &&
            (relation->related_symbol_id != 0u || relation->ordinal != 0u ||
                !relation->name || strcmp(relation->name, "default") != 0)) {
            return SEMANTIC_RESULT_INVALID_DATA;
        }
        if ((relation->kind == SEMANTIC_RESULT_RELATION_GROUP_MEMBER ||
                relation->kind == SEMANTIC_RESULT_RELATION_GROUP_TYPE_BINDING ||
                relation->kind == SEMANTIC_RESULT_RELATION_GROUP_METHOD_IMPLEMENTATION) &&
            (!relation->name || !*relation->name)) {
            return SEMANTIC_RESULT_INVALID_DATA;
        }
        if ((relation->kind == SEMANTIC_RESULT_RELATION_GROUP_TYPE_BINDING ||
                relation->kind == SEMANTIC_RESULT_RELATION_GROUP_DEFAULT_HANDLER) &&
            relation->ordinal != 0u) {
            return SEMANTIC_RESULT_INVALID_DATA;
        }
        if (relation->related_symbol_id != 0u) {
            const SemanticResultSymbol* related =
                &result->symbols[relation->related_symbol_id - 1u];
            if (related->declaration_node_id != relation->related_node_id ||
                (relation->kind == SEMANTIC_RESULT_RELATION_GROUP_TYPE_BINDING &&
                    (related->type_id != relation->type_id ||
                     strcmp(owner->name, relation->name) != 0 ||
                     strcmp(related->name, relation->name) != 0)) ||
                (relation->kind == SEMANTIC_RESULT_RELATION_GROUP_MEMBER &&
                    strcmp(related->name, relation->name) != 0)) {
                return SEMANTIC_RESULT_INVALID_DATA;
            }
        }

        if (owner->first_relation == 0u) {
            owner->first_relation = i + 1u;
        }
        ++owner->relation_count;
    }

    for (i = 0u; i < result->conversion_count; ++i) {
        const SemanticResultConversion* conversion = &result->conversions[i];
        if (conversion->node_id == 0u ||
            conversion->node_id > ast_node_count ||
            conversion->source_type_id == 0u ||
            conversion->source_type_id > result->type_count ||
            conversion->destination_type_id == 0u ||
            conversion->destination_type_id > result->type_count ||
            conversion->kind < SEMANTIC_RESULT_CONVERSION_IDENTITY ||
            conversion->kind > SEMANTIC_RESULT_CONVERSION_HARD_TYPE ||
            (conversion->flags != SEMANTIC_RESULT_CONVERSION_FLAG_IMPLICIT &&
                conversion->flags != SEMANTIC_RESULT_CONVERSION_FLAG_EXPLICIT)) {
            return SEMANTIC_RESULT_INVALID_DATA;
        }
        if (i != 0u) {
            const SemanticResultConversion* previous =
                &result->conversions[i - 1u];
            if (conversion->node_id < previous->node_id ||
                (conversion->node_id == previous->node_id &&
                    conversion->operand_ordinal <= previous->operand_ordinal)) {
                return SEMANTIC_RESULT_INVALID_DATA;
            }
        }
    }

    result->finalized = 1u;
    return SEMANTIC_RESULT_OK;
}

const char* semantic_result_status_string(SemanticResultStatus status) {
    switch (status) {
        case SEMANTIC_RESULT_OK: return "ok";
        case SEMANTIC_RESULT_INVALID_ARGUMENT: return "invalid argument";
        case SEMANTIC_RESULT_INVALID_DATA: return "invalid semantic result";
        case SEMANTIC_RESULT_LIMIT_EXCEEDED: return "semantic result limit exceeded";
        case SEMANTIC_RESULT_OUT_OF_MEMORY: return "out of memory";
        default: return "unknown semantic result status";
    }
}
