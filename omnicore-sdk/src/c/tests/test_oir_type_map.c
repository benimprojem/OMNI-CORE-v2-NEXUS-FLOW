#include "oir_type_map.h"

#include <assert.h>
#include <string.h>

static OirStatus map_name(
    const char* name, uint16_t semantic_flags, uint32_t id, OirModuleType* out
) {
    OirSemanticType semantic_type;
    memset(&semantic_type, 0, sizeof(semantic_type));
    semantic_type.type_id = 1u;
    semantic_type.kind = OIR_SEMANTIC_TYPE_PRIMITIVE;
    semantic_type.flags = semantic_flags;
    return oir_type_map_primitive(
        &semantic_type, (const uint8_t*)name, (uint32_t)strlen(name), id, out);
}

int main(void) {
    OirModuleType type;

    assert(map_name("i32", 0u, 1u, &type) == OIR_STATUS_OK);
    assert(type.id == 1u && type.kind == OIR_MODULE_TYPE_INTEGER);
    assert(type.bit_width == 32u && type.flags == OIR_MODULE_TYPE_FLAG_SIGNED);

    assert(map_name("u32", 0u, 2u, &type) == OIR_STATUS_OK);
    assert(type.kind == OIR_MODULE_TYPE_INTEGER && type.bit_width == 32u);
    assert(type.flags == 0u);

    assert(map_name("f64", OIR_SEMANTIC_TYPE_FLAG_HARD |
        OIR_SEMANTIC_TYPE_FLAG_CONST, 3u, &type) == OIR_STATUS_OK);
    assert(type.kind == OIR_MODULE_TYPE_FLOAT && type.bit_width == 64u);
    assert(type.flags == (OIR_MODULE_TYPE_FLAG_HARD | OIR_MODULE_TYPE_FLAG_CONST));

    assert(map_name("bool", 0u, 4u, &type) == OIR_STATUS_OK);
    assert(type.kind == OIR_MODULE_TYPE_BOOLEAN && type.bit_width == 1u);

    assert(map_name("str", 0u, 5u, &type) == OIR_STATUS_INVALID_SECTION);
    assert(map_name("char", 0u, 6u, &type) == OIR_STATUS_INVALID_SECTION);
    assert(map_name("byte", 0u, 7u, &type) == OIR_STATUS_INVALID_SECTION);
    assert(map_name("void", 0u, 8u, &type) == OIR_STATUS_INVALID_SECTION);
    assert(map_name("any", 0u, 9u, &type) == OIR_STATUS_INVALID_SECTION);
    assert(map_name("i32", 0u, 0u, &type) == OIR_STATUS_INVALID_ARGUMENT);

    return 0;
}
