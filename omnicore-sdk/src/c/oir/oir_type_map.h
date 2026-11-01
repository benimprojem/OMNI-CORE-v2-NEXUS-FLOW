#ifndef OMNICORE_OIR_TYPE_MAP_H
#define OMNICORE_OIR_TYPE_MAP_H

#include "oir_module.h"
#include "oir_semantic_ast.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Map one validated SEMANTIC_AST primitive type to the initial OIR scalar
 * registry. The caller supplies the destination module-local type ID.
 *
 * Only i8/i16/i32/i64, u8/u16/u32/u64, f32/f64 and bool are supported.
 * Unsupported primitive names (including str, char, byte, void and any)
 * fail closed until their OIR representations are specified.
 */
OirStatus oir_type_map_primitive(
    const OirSemanticType* semantic_type,
    const uint8_t* primitive_name,
    uint32_t primitive_name_length,
    uint32_t module_type_id,
    OirModuleType* out_type
);

#ifdef __cplusplus
}
#endif

#endif
