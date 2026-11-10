#include "oir_module_interface_artifact.h"
#include "../core/diagnostics.h"

#include <stdlib.h>
#include <string.h>

#define OMI_PAYLOAD_HEADER_SIZE 24u
#define OMI_SYMBOL_FIXED_SIZE 24u
#define OMI_TYPE_FIXED_SIZE 20u
#define OMI_RELATION_FIXED_SIZE 28u

static void write_u16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xffu);
    p[1] = (uint8_t)(v >> 8);
}
static void write_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
static uint16_t read_u16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t read_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int add_size(size_t* value, size_t amount) {
    if (amount > SIZE_MAX - *value) return 0;
    *value += amount;
    return 1;
}

static int string_length_u32(const char* value, uint32_t* out_length) {
    size_t length;
    if (!value) {
        *out_length = 0u;
        return 1;
    }
    length = strlen(value);
    if (length > UINT32_MAX) return 0;
    *out_length = (uint32_t)length;
    return 1;
}

static int add_string_size(size_t* total, const char* value) {
    uint32_t length;
    return string_length_u32(value, &length) && add_size(total, length);
}

static int write_string(uint8_t* payload, size_t capacity, size_t* offset,
    const char* value) {
    uint32_t length;
    if (!string_length_u32(value, &length) || *offset > capacity ||
        (size_t)length > capacity - *offset) return 0;
    if (length) memcpy(payload + *offset, value, length);
    *offset += length;
    return 1;
}

static char* read_string(const uint8_t* payload, size_t length, size_t* offset,
    uint32_t string_length) {
    char* value;
    if (*offset > length || (size_t)string_length > length - *offset)
        return NULL;
    if (string_length == 0u) return NULL;
    if (memchr(payload + *offset, 0, string_length) != NULL) return NULL;
    value = (char*)malloc((size_t)string_length + 1u);
    if (!value) return NULL;
    memcpy(value, payload + *offset, string_length);
    value[string_length] = '\0';
    *offset += string_length;
    return value;
}

static int validate_interface(const OccModuleInterface* interface) {
    uint8_t* symbol_ids = NULL;
    uint8_t* type_ids = NULL;
    size_t i;
    int valid = 0;
    const size_t bitmap_size = (size_t)SEMANTIC_RESULT_MAX_RECORDS + 1u;

    if (!interface || !interface->canonical_identity ||
        !interface->canonical_identity[0] ||
        interface->schema_major != OCC_MODULE_INTERFACE_SCHEMA_MAJOR ||
        interface->schema_minor > OCC_MODULE_INTERFACE_SCHEMA_MINOR ||
        (interface->symbol_count && !interface->symbols) ||
        (interface->type_count && !interface->types) ||
        (interface->relation_count && !interface->relations) ||
        interface->symbol_count > SEMANTIC_RESULT_MAX_RECORDS ||
        interface->type_count > SEMANTIC_RESULT_MAX_RECORDS ||
        interface->relation_count > SEMANTIC_RESULT_MAX_RECORDS)
        return 0;

    symbol_ids = (uint8_t*)calloc(bitmap_size, 1u);
    type_ids = (uint8_t*)calloc(bitmap_size, 1u);
    if (!symbol_ids || !type_ids) goto done;

    for (i = 0; i < interface->symbol_count; ++i) {
        const OccModuleInterfaceSymbol* s = &interface->symbols[i];
        if (!s->source_symbol_id ||
            s->source_symbol_id > SEMANTIC_RESULT_MAX_RECORDS ||
            !s->name || !s->name[0] ||
            (s->type_id > SEMANTIC_RESULT_MAX_RECORDS))
            goto done;
        if (symbol_ids[s->source_symbol_id]) goto done;
        symbol_ids[s->source_symbol_id] = 1u;
    }
    for (i = 0; i < interface->type_count; ++i) {
        const OccModuleInterfaceType* t = &interface->types[i];
        if (!t->source_type_id ||
            t->source_type_id > SEMANTIC_RESULT_MAX_RECORDS ||
            t->base_type_id > SEMANTIC_RESULT_MAX_RECORDS ||
            t->referenced_symbol_id > SEMANTIC_RESULT_MAX_RECORDS)
            goto done;
        if (type_ids[t->source_type_id]) goto done;
        type_ids[t->source_type_id] = 1u;
    }
    for (i = 0; i < interface->symbol_count; ++i) {
        uint32_t type_id = interface->symbols[i].type_id;
        if (type_id && !type_ids[type_id]) goto done;
    }
    for (i = 0; i < interface->type_count; ++i) {
        uint32_t base_type_id = interface->types[i].base_type_id;
        if (base_type_id && !type_ids[base_type_id]) goto done;
    }
    for (i = 0; i < interface->relation_count; ++i) {
        const OccModuleInterfaceRelation* r = &interface->relations[i];
        if (!r->owner_source_symbol_id ||
            r->owner_source_symbol_id > SEMANTIC_RESULT_MAX_RECORDS ||
            !symbol_ids[r->owner_source_symbol_id] ||
            r->related_source_symbol_id > SEMANTIC_RESULT_MAX_RECORDS ||
            (r->related_source_symbol_id &&
             !symbol_ids[r->related_source_symbol_id]) ||
            r->type_id > SEMANTIC_RESULT_MAX_RECORDS ||
            (r->type_id && !type_ids[r->type_id]))
            goto done;
    }
    valid = 1;

done:
    free(symbol_ids);
    free(type_ids);
    return valid;
}

OirStatus oir_module_interface_artifact_write(
    const OccModuleInterface* interface,
    uint64_t job_id,
    uint8_t* output,
    size_t output_capacity,
    size_t* out_written
) {
    size_t payload_size = OMI_PAYLOAD_HEADER_SIZE;
    size_t offset;
    uint8_t* payload = NULL;
    OirArtifactHeader header;
    OirSectionInput section;
    OirStatus status;
    size_t i;
    uint32_t identity_length;

    if (out_written) *out_written = 0u;
    if (!interface || !output || !out_written) return OIR_STATUS_INVALID_ARGUMENT;
    if (!validate_interface(interface)) return OIR_STATUS_INVALID_SECTION;
    if (!string_length_u32(interface->canonical_identity, &identity_length))
        return OIR_STATUS_OVERFLOW;
    if (!add_size(&payload_size, identity_length)) return OIR_STATUS_OVERFLOW;

    for (i = 0; i < interface->symbol_count; ++i)
        if (!add_size(&payload_size, OMI_SYMBOL_FIXED_SIZE) ||
            !add_string_size(&payload_size, interface->symbols[i].name))
            return OIR_STATUS_OVERFLOW;
    for (i = 0; i < interface->type_count; ++i)
        if (!add_size(&payload_size, OMI_TYPE_FIXED_SIZE) ||
            !add_string_size(&payload_size, interface->types[i].name))
            return OIR_STATUS_OVERFLOW;
    for (i = 0; i < interface->relation_count; ++i)
        if (!add_size(&payload_size, OMI_RELATION_FIXED_SIZE) ||
            !add_string_size(&payload_size, interface->relations[i].name))
            return OIR_STATUS_OVERFLOW;

    if (payload_size > UINT32_MAX) return OIR_STATUS_OVERFLOW;
    payload = (uint8_t*)malloc(payload_size);
    if (!payload) return OIR_STATUS_OUT_OF_MEMORY;

    payload[0] = 'O'; payload[1] = 'M'; payload[2] = 'I'; payload[3] = '1';
    write_u16(payload + 4, OIR_MODULE_INTERFACE_PAYLOAD_SCHEMA_MAJOR);
    write_u16(payload + 6, OIR_MODULE_INTERFACE_PAYLOAD_SCHEMA_MINOR);
    write_u32(payload + 8, identity_length);
    write_u32(payload + 12, (uint32_t)interface->symbol_count);
    write_u32(payload + 16, (uint32_t)interface->type_count);
    write_u32(payload + 20, (uint32_t)interface->relation_count);
    offset = OMI_PAYLOAD_HEADER_SIZE;
    if (!write_string(payload, payload_size, &offset,
            interface->canonical_identity)) {
        free(payload);
        return OIR_STATUS_INVALID_SECTION;
    }

    for (i = 0; i < interface->symbol_count; ++i) {
        const OccModuleInterfaceSymbol* s = &interface->symbols[i];
        uint32_t name_length;
        (void)string_length_u32(s->name, &name_length);
        write_u32(payload + offset, s->source_symbol_id);
        write_u32(payload + offset + 4, s->scope_id);
        write_u32(payload + offset + 8, s->declaration_node_id);
        write_u32(payload + offset + 12, s->type_id);
        write_u16(payload + offset + 16, s->kind);
        write_u16(payload + offset + 18, s->flags);
        write_u32(payload + offset + 20, name_length);
        offset += OMI_SYMBOL_FIXED_SIZE;
        if (!write_string(payload, payload_size, &offset, s->name)) {
            free(payload); return OIR_STATUS_INVALID_SECTION;
        }
    }
    for (i = 0; i < interface->type_count; ++i) {
        const OccModuleInterfaceType* t = &interface->types[i];
        uint32_t name_length;
        (void)string_length_u32(t->name, &name_length);
        write_u32(payload + offset, t->source_type_id);
        write_u16(payload + offset + 4, t->kind);
        write_u16(payload + offset + 6, t->flags);
        write_u32(payload + offset + 8, t->referenced_symbol_id);
        write_u32(payload + offset + 12, t->base_type_id);
        write_u32(payload + offset + 16, name_length);
        offset += OMI_TYPE_FIXED_SIZE;
        if (!write_string(payload, payload_size, &offset, t->name)) {
            free(payload); return OIR_STATUS_INVALID_SECTION;
        }
    }
    for (i = 0; i < interface->relation_count; ++i) {
        const OccModuleInterfaceRelation* r = &interface->relations[i];
        uint32_t name_length;
        (void)string_length_u32(r->name, &name_length);
        write_u32(payload + offset, r->owner_source_symbol_id);
        write_u32(payload + offset + 4, r->related_source_symbol_id);
        write_u32(payload + offset + 8, r->related_node_id);
        write_u32(payload + offset + 12, r->type_id);
        write_u32(payload + offset + 16, r->ordinal);
        write_u16(payload + offset + 20, r->kind);
        write_u16(payload + offset + 22, r->flags);
        write_u32(payload + offset + 24, name_length);
        offset += OMI_RELATION_FIXED_SIZE;
        if (!write_string(payload, payload_size, &offset, r->name)) {
            free(payload); return OIR_STATUS_INVALID_SECTION;
        }
    }

    memset(&header, 0, sizeof(header));
    header.container_major = OIR_CONTAINER_MAJOR;
    header.container_minor = OIR_CONTAINER_MINOR;
    header.artifact_kind = OIR_KIND_MODULE_INTERFACE;
    header.payload_schema_major = OIR_MODULE_INTERFACE_PAYLOAD_SCHEMA_MAJOR;
    header.payload_schema_minor = OIR_MODULE_INTERFACE_PAYLOAD_SCHEMA_MINOR;
    header.producer_stage_id = OCC_STAGE_SEMANTIC;
    header.validity = OIR_VALIDITY_VALID;
    header.job_id = job_id;

    section.type = OIR_SECTION_PAYLOAD;
    section.version = 1u;
    section.flags = OIR_SECTION_FLAG_REQUIRED;
    section.data = payload;
    section.length = payload_size;
    status = oir_artifact_write(output, output_capacity, &header,
        &section, 1u, out_written);
    free(payload);
    return status;
}

void oir_module_interface_artifact_destroy(OirModuleInterfaceArtifactView* view) {
    if (!view) return;
    occ_module_interface_destroy(&view->module_interface);
    memset(view, 0, sizeof(*view));
}

OirStatus oir_module_interface_artifact_open(
    const uint8_t* artifact_bytes,
    size_t artifact_length,
    OirModuleInterfaceArtifactView* out_view
) {
    OirModuleInterfaceArtifactView result;
    OirSectionView section;
    const uint8_t* p;
    size_t length, offset;
    uint32_t identity_length, symbol_count, type_count, relation_count;
    uint32_t i;
    uint8_t payload_seen = 0u;
    OirStatus status;

    if (!out_view) return OIR_STATUS_INVALID_ARGUMENT;
    memset(out_view, 0, sizeof(*out_view));
    memset(&result, 0, sizeof(result));
    if (!artifact_bytes) return OIR_STATUS_INVALID_ARGUMENT;

    status = oir_artifact_open(artifact_bytes, artifact_length, &result.envelope);
    if (status != OIR_STATUS_OK) return status;
    if (result.envelope.header.artifact_kind != OIR_KIND_MODULE_INTERFACE ||
        result.envelope.header.payload_schema_major !=
            OIR_MODULE_INTERFACE_PAYLOAD_SCHEMA_MAJOR ||
        result.envelope.header.payload_schema_minor >
            OIR_MODULE_INTERFACE_PAYLOAD_SCHEMA_MINOR ||
        (result.envelope.header.producer_stage_id != OCC_STAGE_SEMANTIC &&
         result.envelope.header.producer_stage_id != OCC_STAGE_OIR_VERIFY) ||
        result.envelope.header.validity != OIR_VALIDITY_VALID)
        return OIR_STATUS_INVALID_HEADER;

    for (i = 0u; i < result.envelope.section_count; ++i) {
        status = oir_artifact_get_section(&result.envelope, i, &section);
        if (status != OIR_STATUS_OK) goto fail;
        if (section.type == OIR_SECTION_PAYLOAD) {
            if (payload_seen || section.version != 1u ||
                (section.flags & OIR_SECTION_FLAG_REQUIRED) == 0u) {
                status = OIR_STATUS_INVALID_SECTION; goto fail;
            }
            payload_seen = 1u;
            p = section.data;
            length = (size_t)section.length;
            if (length < OMI_PAYLOAD_HEADER_SIZE ||
                p[0] != 'O' || p[1] != 'M' || p[2] != 'I' || p[3] != '1' ||
                read_u16(p + 4) != OIR_MODULE_INTERFACE_PAYLOAD_SCHEMA_MAJOR ||
                read_u16(p + 6) > OIR_MODULE_INTERFACE_PAYLOAD_SCHEMA_MINOR) {
                status = OIR_STATUS_INVALID_SECTION; goto fail;
            }
            identity_length = read_u32(p + 8);
            symbol_count = read_u32(p + 12);
            type_count = read_u32(p + 16);
            relation_count = read_u32(p + 20);
            if (symbol_count > SEMANTIC_RESULT_MAX_RECORDS ||
                type_count > SEMANTIC_RESULT_MAX_RECORDS ||
                relation_count > SEMANTIC_RESULT_MAX_RECORDS ||
                identity_length == 0u) {
                status = OIR_STATUS_INVALID_SECTION; goto fail;
            }
            offset = OMI_PAYLOAD_HEADER_SIZE;
            result.module_interface.canonical_identity =
                read_string(p, length, &offset, identity_length);
            if (!result.module_interface.canonical_identity) {
                status = OIR_STATUS_INVALID_SECTION; goto fail;
            }
            result.module_interface.schema_major = read_u16(p + 4);
            result.module_interface.schema_minor = read_u16(p + 6);

            if (symbol_count) {
                result.module_interface.symbols = (OccModuleInterfaceSymbol*)
                    calloc(symbol_count, sizeof(OccModuleInterfaceSymbol));
                if (!result.module_interface.symbols) {
                    status = OIR_STATUS_OUT_OF_MEMORY; goto fail;
                }
            }
            result.module_interface.symbol_count = symbol_count;
            for (i = 0u; i < symbol_count; ++i) {
                OccModuleInterfaceSymbol* s = &result.module_interface.symbols[i];
                uint32_t name_length;
                if (offset > length || length - offset < OMI_SYMBOL_FIXED_SIZE) {
                    status = OIR_STATUS_INVALID_SECTION; goto fail;
                }
                s->source_symbol_id = read_u32(p + offset);
                s->scope_id = read_u32(p + offset + 4);
                s->declaration_node_id = read_u32(p + offset + 8);
                s->type_id = read_u32(p + offset + 12);
                s->kind = read_u16(p + offset + 16);
                s->flags = read_u16(p + offset + 18);
                name_length = read_u32(p + offset + 20);
                offset += OMI_SYMBOL_FIXED_SIZE;
                s->name = read_string(p, length, &offset, name_length);
                if (!s->source_symbol_id || !s->name) {
                    status = OIR_STATUS_INVALID_SECTION; goto fail;
                }
            }
            if (type_count) {
                result.module_interface.types = (OccModuleInterfaceType*)
                    calloc(type_count, sizeof(OccModuleInterfaceType));
                if (!result.module_interface.types) {
                    status = OIR_STATUS_OUT_OF_MEMORY; goto fail;
                }
            }
            result.module_interface.type_count = type_count;
            for (i = 0u; i < type_count; ++i) {
                OccModuleInterfaceType* t = &result.module_interface.types[i];
                uint32_t name_length;
                if (offset > length || length - offset < OMI_TYPE_FIXED_SIZE) {
                    status = OIR_STATUS_INVALID_SECTION; goto fail;
                }
                t->source_type_id = read_u32(p + offset);
                t->kind = read_u16(p + offset + 4);
                t->flags = read_u16(p + offset + 6);
                t->referenced_symbol_id = read_u32(p + offset + 8);
                t->base_type_id = read_u32(p + offset + 12);
                name_length = read_u32(p + offset + 16);
                offset += OMI_TYPE_FIXED_SIZE;
                t->name = read_string(p, length, &offset, name_length);
                if (!t->source_type_id || (name_length && !t->name)) {
                    status = OIR_STATUS_INVALID_SECTION; goto fail;
                }
            }
            if (relation_count) {
                result.module_interface.relations = (OccModuleInterfaceRelation*)
                    calloc(relation_count, sizeof(OccModuleInterfaceRelation));
                if (!result.module_interface.relations) {
                    status = OIR_STATUS_OUT_OF_MEMORY; goto fail;
                }
            }
            result.module_interface.relation_count = relation_count;
            for (i = 0u; i < relation_count; ++i) {
                OccModuleInterfaceRelation* r = &result.module_interface.relations[i];
                uint32_t name_length;
                if (offset > length || length - offset < OMI_RELATION_FIXED_SIZE) {
                    status = OIR_STATUS_INVALID_SECTION; goto fail;
                }
                r->owner_source_symbol_id = read_u32(p + offset);
                r->related_source_symbol_id = read_u32(p + offset + 4);
                r->related_node_id = read_u32(p + offset + 8);
                r->type_id = read_u32(p + offset + 12);
                r->ordinal = read_u32(p + offset + 16);
                r->kind = read_u16(p + offset + 20);
                r->flags = read_u16(p + offset + 22);
                name_length = read_u32(p + offset + 24);
                offset += OMI_RELATION_FIXED_SIZE;
                r->name = read_string(p, length, &offset, name_length);
                if (!r->owner_source_symbol_id || (name_length && !r->name)) {
                    status = OIR_STATUS_INVALID_SECTION; goto fail;
                }
            }
            if (offset != length || !validate_interface(&result.module_interface)) {
                status = OIR_STATUS_INVALID_SECTION; goto fail;
            }
        } else if ((section.flags & OIR_SECTION_FLAG_REQUIRED) != 0u) {
            status = OIR_STATUS_UNKNOWN_REQUIRED_SECTION; goto fail;
        }
    }
    if (!payload_seen) { status = OIR_STATUS_MISSING_PAYLOAD; goto fail; }
    *out_view = result;
    return OIR_STATUS_OK;

fail:
    oir_module_interface_artifact_destroy(&result);
    return status;
}
