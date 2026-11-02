#include "oir_ast_map.h"

/*
 * This table is the sole C mapping between frontend AST tags and the
 * published OIR_AST_NODE_REGISTRY.md identifiers. Never derive IDs by
 * casting ASTNodeType: enum insertions/reordering must not alter the wire.
 */
typedef struct OirAstNodeMapEntry {
    ASTNodeType frontend_type;
    uint16_t oir_id;
} OirAstNodeMapEntry;

static const OirAstNodeMapEntry node_map[OIR_AST_NODE_KIND_COUNT] = {
    { AST_PROGRAM, 1u },
    { AST_BLOCK, 2u },
    { AST_VAR_DECL, 3u },
    { AST_FUNC_DECL, 4u },
    { AST_STRUCT_DECL, 5u },
    { AST_ENUM_DECL, 6u },
    { AST_UNION_DECL, 7u },
    { AST_NEWTYPE_DECL, 8u },
    { AST_ENUM_VARIANT, 9u },
    { AST_GROUP_DECL, 10u },
    { AST_METHOD, 11u },
    { AST_EXTERN_DECL, 12u },
    { AST_LITERAL, 13u },
    { AST_IDENTIFIER, 14u },
    { AST_BINARY_EXPR, 15u },
    { AST_UNARY_EXPR, 16u },
    { AST_POSTFIX_EXPR, 17u },
    { AST_ASSIGNMENT, 18u },
    { AST_FUNC_CALL, 19u },
    { AST_MEMBER_ACCESS, 20u },
    { AST_INDEX_ACCESS, 21u },
    { AST_CAST, 22u },
    { AST_TUPLE, 23u },
    { AST_IF, 24u },
    { AST_LOOP, 25u },
    { AST_RETURN, 26u },
    { AST_BREAK, 27u },
    { AST_CONTINUE, 28u },
    { AST_ASM_BLOCK, 29u },
    { AST_NEXUS_CAPTURE, 30u },
    { AST_NEXUS_PIPE, 31u },
    { AST_NEXUS_FLOW, 32u },
    { AST_NEXUS_LABEL, 33u },
    { AST_NEXUS_JUMP, 34u },
    { AST_NEXUS_HALT, 35u },
    { AST_NEXUS_INTENT, 36u },
    { AST_AI_LOGIC, 37u },
    { AST_UNPACK, 38u },
    { AST_PANIC, 39u },
    { AST_SPAWN, 40u },
    { AST_LISTEN, 41u },
    { AST_ECHO, 42u },
    { AST_SEND, 43u },
    { AST_RECEIVE, 44u },
    { AST_DONE, 45u },
    { AST_AREA, 46u },
    { AST_ADDR, 47u },
    { AST_LEN, 48u },
    { AST_PEEK, 49u },
    { AST_POKE, 50u },
    { AST_INB, 51u },
    { AST_OUTB, 52u },
    { AST_IRQ, 53u },
    { AST_INTR, 54u },
    { AST_REG, 55u },
    { AST_PRNT, 56u },
    { AST_PRMT, 57u },
    { AST_WRITE, 58u },
    { AST_FWRITE, 59u },
    { AST_TYPEOF, 60u },
    { AST_SIZEOF, 61u },
    { AST_IS_OK, 62u },
    { AST_IS_ERR, 63u },
    { AST_IS_SOME, 64u },
    { AST_IS_NONE, 65u },
    { AST_IS_NULL, 66u },
    { AST_EXIT, 67u },
    { AST_SWAP, 68u },
    { AST_DEFER, 69u },
    { AST_THREAD_BLOCK, 70u },
    { AST_USE, 71u },
    { AST_ZONE, 72u },
    { AST_DATE, 73u },
    { AST_DATENOW, 74u },
    { AST_TIME, 75u },
    { AST_CLOCK, 76u },
    { AST_FREE, 77u },
    { AST_ZONE_SYNC, 78u },
    { AST_ZONE_VIEW, 79u },
    { AST_ZONE_LOCK, 80u },
    { AST_SELECT, 81u },
    { AST_SELECT_ARM, 82u },
    { AST_SCAN, 83u },
    { AST_SCAN_ARM, 84u },
    { AST_PATTERN_BINDING, 85u },
    { AST_PATTERN_CONSTRUCTOR, 86u },
    { AST_RULESET_DECL, 87u },
    { AST_RULE_BARRIER, 88u },
    { AST_RULE_TRIGGER, 89u },
    { AST_RULE_ALWAYS, 90u },
    { AST_RULE_DEFAULT, 91u },
    { AST_APPLY_RULES, 92u },
    { AST_ON_BLOCK, 93u },
    { AST_LAMBDA, 94u },
    { AST_RELOCATE, 95u },
    { AST_UNROLL, 96u },
    { AST_ARRAY_LITERAL, 97u },
    { AST_ARRAY_TYPE, 98u },
    { AST_ARRAY_DIMENSION, 99u }
};

uint16_t oir_ast_node_id_from_frontend(ASTNodeType type) {
    uint16_t i;
    for (i = 0; i < OIR_AST_NODE_KIND_COUNT; ++i) {
        if (node_map[i].frontend_type == type) return node_map[i].oir_id;
    }
    return 0u;
}

int oir_ast_node_id_to_frontend(uint16_t node_id, ASTNodeType* out_type) {
    uint16_t i;
    if (!out_type || node_id == 0u || node_id > OIR_AST_NODE_KIND_COUNT) {
        return 0;
    }
    for (i = 0; i < OIR_AST_NODE_KIND_COUNT; ++i) {
        if (node_map[i].oir_id == node_id) {
            *out_type = node_map[i].frontend_type;
            return 1;
        }
    }
    return 0;
}
