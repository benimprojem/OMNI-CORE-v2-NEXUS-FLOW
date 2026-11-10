#ifndef OMNICORE_OIR_MODULE_OPCODE_H
#define OMNICORE_OIR_MODULE_OPCODE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initial OIR_MODULE opcode registry. IDs are wire identifiers, never C enum ordinals. */
#define OIR_OPCODE_REGISTRY_MAJOR 1u
#define OIR_OPCODE_REGISTRY_MINOR 2u
#define OIR_OPCODE_MAX_FIXED_OPERANDS 3u

typedef enum OirOperandKind {
    OIR_OPERAND_VALUE = 1u << 0,
    OIR_OPERAND_BLOCK = 1u << 1,
    OIR_OPERAND_FUNCTION = 1u << 2,
    OIR_OPERAND_TYPE = 1u << 3,
    OIR_OPERAND_INTEGER_IMMEDIATE = 1u << 4,
    OIR_OPERAND_FLOAT_IMMEDIATE = 1u << 5
} OirOperandKind;

typedef enum OirOpcodeResultRule {
    OIR_OPCODE_RESULT_REQUIRED = 1,
    OIR_OPCODE_RESULT_FORBIDDEN = 2,
    OIR_OPCODE_RESULT_OPTIONAL = 3
} OirOpcodeResultRule;

typedef enum OirOpcodeFlag {
    OIR_OPCODE_FLAG_PURE = 1u << 0,
    OIR_OPCODE_FLAG_MAY_TRAP = 1u << 1,
    OIR_OPCODE_FLAG_READS_MEMORY = 1u << 2,
    OIR_OPCODE_FLAG_WRITES_MEMORY = 1u << 3,
    OIR_OPCODE_FLAG_TERMINATOR = 1u << 4
} OirOpcodeFlag;

typedef enum OirOpcodeId {
    OIR_OP_INVALID = 0,

    OIR_OP_CONST_INT = 1,
    OIR_OP_CONST_FLOAT = 2,
    OIR_OP_CONST_BOOL = 3,

    OIR_OP_ADD = 16,
    OIR_OP_SUB = 17,
    OIR_OP_MUL = 18,
    OIR_OP_DIV = 19,
    OIR_OP_REM = 20,
    OIR_OP_NEG = 21,

    OIR_OP_BIT_AND = 22,
    OIR_OP_BIT_OR = 23,
    OIR_OP_BIT_XOR = 24,
    OIR_OP_BIT_NOT = 25,
    OIR_OP_BIT_SHL = 26,
    OIR_OP_BIT_SHR = 27,

    OIR_OP_CMP_EQ = 32,
    OIR_OP_CMP_NE = 33,
    OIR_OP_CMP_LT = 34,
    OIR_OP_CMP_LE = 35,
    OIR_OP_CMP_GT = 36,
    OIR_OP_CMP_GE = 37,

    OIR_OP_CAST = 48,
    OIR_OP_CALL = 64,

    OIR_OP_LOAD = 80,
    OIR_OP_STORE = 81,

    OIR_OP_BRANCH = 96,
    OIR_OP_COND_BRANCH = 97,
    OIR_OP_RETURN = 98,
    OIR_OP_UNREACHABLE = 99
} OirOpcodeId;

typedef struct OirOpcodeDescriptor {
    uint16_t opcode_id;
    const char* name;
    uint8_t min_operand_count;
    uint8_t max_operand_count;
    uint8_t result_rule;
    uint8_t reserved;
    uint32_t flags;
    uint32_t operand_kind_mask[OIR_OPCODE_MAX_FIXED_OPERANDS];
    uint32_t variadic_operand_kind_mask;
} OirOpcodeDescriptor;

/* Registry lookup returns immutable static descriptors, or NULL for unknown IDs. */
const OirOpcodeDescriptor* oir_opcode_lookup(uint16_t opcode_id);
const OirOpcodeDescriptor* oir_opcode_lookup_name(const char* name);
uint32_t oir_opcode_registry_count(void);

/*
 * Validate only the stable structural contract: operand count/kinds and result
 * presence. Type compatibility and SSA/use-def checks belong to the module
 * verifier and are deliberately not inferred by this registry.
 */
int oir_opcode_validate_shape(
    uint16_t opcode_id,
    const uint32_t* operand_kinds,
    uint32_t operand_count,
    int has_result
);

#ifdef __cplusplus
}
#endif

#endif
