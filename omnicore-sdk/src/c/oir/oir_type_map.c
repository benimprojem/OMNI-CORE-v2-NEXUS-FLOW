#include "oir_type_map.h"

#include <string.h>

typedef struct OirPrimitiveMapEntry {
    const char* name;
    uint16_t kind;
    uint16_t width;
    uint32_t flags;
} OirPrimitiveMapEntry;

static const OirPrimitiveMapEntry g_primitive_types[] = {
    { "i8",  OIR_MODULE_TYPE_INTEGER, 8u,  OIR_MODULE_TYPE_FLAG_SIGNED },
    { "i16", OIR_MODULE_TYPE_INTEGER, 16u, OIR_MODULE_TYPE_FLAG_SIGNED },
    { "i32", OIR_MODULE_TYPE_INTEGER, 32u, OIR_MODULE_TYPE_FLAG_SIGNED },
    { "i64", OIR_MODULE_TYPE_INTEGER, 64u, OIR_MODULE_TYPE_FLAG_SIGNED },
    { "u8",  OIR_MODULE_TYPE_INTEGER, 8u,  0u },
    { "u16", OIR_MODULE_TYPE_INTEGER, 16u, 0u },
    { "u32", OIR_MODULE_TYPE_INTEGER, 32u, 0u },
    { "u64", OIR_MODULE_TYPE_INTEGER, 64u, 0u },
    { "f32", OIR_MODULE_TYPE_FLOAT,   32u, 0u },
    { "f64", OIR_MODULE_TYPE_FLOAT,   64u, 0u },
    { "bool",OIR_MODULE_TYPE_BOOLEAN, 1u,  0u },
    { "bit", OIR_MODULE_TYPE_BIT, 1u, 0u },
    { "byte", OIR_MODULE_TYPE_BYTE, 8u, 0u },
    { "8bit", OIR_MODULE_TYPE_BIT_VECTOR, 8u, 0u },
    { "16bit", OIR_MODULE_TYPE_BIT_VECTOR, 16u, 0u },
    { "32bit", OIR_MODULE_TYPE_BIT_VECTOR, 32u, 0u },
    { "64bit", OIR_MODULE_TYPE_BIT_VECTOR, 64u, 0u },
    { "128bit", OIR_MODULE_TYPE_BIT_VECTOR, 128u, 0u },
    { "1byte", OIR_MODULE_TYPE_BYTE_BLOCK, 8u, 0u },
    { "2byte", OIR_MODULE_TYPE_BYTE_BLOCK, 16u, 0u },
    { "4byte", OIR_MODULE_TYPE_BYTE_BLOCK, 32u, 0u },
    { "8byte", OIR_MODULE_TYPE_BYTE_BLOCK, 64u, 0u },
    { "16byte", OIR_MODULE_TYPE_BYTE_BLOCK, 128u, 0u }
};

OirStatus oir_type_map_primitive(
    const OirSemanticType* semantic_type,
    const uint8_t* primitive_name,
    uint32_t primitive_name_length,
    uint32_t module_type_id,
    OirModuleType* out_type
) {
    size_t i;
    uint32_t flags = 0u;

    if (!semantic_type || !primitive_name || primitive_name_length == 0u ||
        module_type_id == 0u || !out_type) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    memset(out_type, 0, sizeof(*out_type));

    if (semantic_type->kind != OIR_SEMANTIC_TYPE_PRIMITIVE ||
        (semantic_type->flags & ~(OIR_SEMANTIC_TYPE_FLAG_CONST |
            OIR_SEMANTIC_TYPE_FLAG_HARD)) != 0u) {
        return OIR_STATUS_INVALID_SECTION;
    }

    for (i = 0u; i < sizeof(g_primitive_types) / sizeof(g_primitive_types[0]); ++i) {
        const size_t name_length = strlen(g_primitive_types[i].name);
        if (name_length == primitive_name_length &&
            memcmp(primitive_name, g_primitive_types[i].name, name_length) == 0) {
            out_type->id = module_type_id;
            out_type->kind = g_primitive_types[i].kind;
            out_type->bit_width = g_primitive_types[i].width;
            flags = g_primitive_types[i].flags;
            if ((semantic_type->flags & OIR_SEMANTIC_TYPE_FLAG_HARD) != 0u) {
                flags |= OIR_MODULE_TYPE_FLAG_HARD;
            }
            if ((semantic_type->flags & OIR_SEMANTIC_TYPE_FLAG_CONST) != 0u) {
                flags |= OIR_MODULE_TYPE_FLAG_CONST;
            }
            out_type->flags = flags;
            return OIR_STATUS_OK;
        }
    }

    return OIR_STATUS_INVALID_SECTION;
}
