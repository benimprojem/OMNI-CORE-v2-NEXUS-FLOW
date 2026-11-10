#include "oir_module_opcode.h"

#include <stddef.h>
#include <string.h>

#define VALUE OIR_OPERAND_VALUE
#define BLOCK OIR_OPERAND_BLOCK
#define FUNCTION OIR_OPERAND_FUNCTION
#define TYPE OIR_OPERAND_TYPE

static const OirOpcodeDescriptor g_opcodes[] = {
    { OIR_OP_CONST_INT, "const.int", 1u, 1u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { OIR_OPERAND_INTEGER_IMMEDIATE, 0u, 0u }, 0u },
    { OIR_OP_CONST_FLOAT, "const.float", 1u, 1u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { OIR_OPERAND_FLOAT_IMMEDIATE, 0u, 0u }, 0u },
    { OIR_OP_CONST_BOOL, "const.bool", 1u, 1u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { OIR_OPERAND_INTEGER_IMMEDIATE, 0u, 0u }, 0u },

    { OIR_OP_ADD, "add", 2u, 2u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { VALUE, VALUE, 0u }, 0u },
    { OIR_OP_SUB, "sub", 2u, 2u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { VALUE, VALUE, 0u }, 0u },
    { OIR_OP_MUL, "mul", 2u, 2u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { VALUE, VALUE, 0u }, 0u },
    { OIR_OP_DIV, "div", 2u, 2u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE | OIR_OPCODE_FLAG_MAY_TRAP,
      { VALUE, VALUE, 0u }, 0u },
    { OIR_OP_REM, "rem", 2u, 2u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE | OIR_OPCODE_FLAG_MAY_TRAP,
      { VALUE, VALUE, 0u }, 0u },
    { OIR_OP_NEG, "neg", 1u, 1u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { VALUE, 0u, 0u }, 0u },

    { OIR_OP_BIT_AND, "bit.and", 2u, 2u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { VALUE, VALUE, 0u }, 0u },
    { OIR_OP_BIT_OR, "bit.or", 2u, 2u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { VALUE, VALUE, 0u }, 0u },
    { OIR_OP_BIT_XOR, "bit.xor", 2u, 2u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { VALUE, VALUE, 0u }, 0u },
    { OIR_OP_BIT_NOT, "bit.not", 1u, 1u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { VALUE, 0u, 0u }, 0u },
    { OIR_OP_BIT_SHL, "bit.shl", 2u, 2u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE | OIR_OPCODE_FLAG_MAY_TRAP, { VALUE, VALUE, 0u }, 0u },
    { OIR_OP_BIT_SHR, "bit.shr", 2u, 2u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE | OIR_OPCODE_FLAG_MAY_TRAP, { VALUE, VALUE, 0u }, 0u },

    { OIR_OP_CMP_EQ, "cmp.eq", 2u, 2u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { VALUE, VALUE, 0u }, 0u },
    { OIR_OP_CMP_NE, "cmp.ne", 2u, 2u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { VALUE, VALUE, 0u }, 0u },
    { OIR_OP_CMP_LT, "cmp.lt", 2u, 2u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { VALUE, VALUE, 0u }, 0u },
    { OIR_OP_CMP_LE, "cmp.le", 2u, 2u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { VALUE, VALUE, 0u }, 0u },
    { OIR_OP_CMP_GT, "cmp.gt", 2u, 2u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { VALUE, VALUE, 0u }, 0u },
    { OIR_OP_CMP_GE, "cmp.ge", 2u, 2u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { VALUE, VALUE, 0u }, 0u },

    { OIR_OP_CAST, "cast", 2u, 2u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_PURE, { VALUE, TYPE, 0u }, 0u },
    { OIR_OP_CALL, "call", 1u, 255u, OIR_OPCODE_RESULT_OPTIONAL, 0u,
      OIR_OPCODE_FLAG_MAY_TRAP, { FUNCTION, VALUE, VALUE }, VALUE },

    { OIR_OP_LOAD, "load", 1u, 1u, OIR_OPCODE_RESULT_REQUIRED, 0u,
      OIR_OPCODE_FLAG_READS_MEMORY | OIR_OPCODE_FLAG_MAY_TRAP,
      { VALUE, 0u, 0u }, 0u },
    { OIR_OP_STORE, "store", 2u, 2u, OIR_OPCODE_RESULT_FORBIDDEN, 0u,
      OIR_OPCODE_FLAG_WRITES_MEMORY | OIR_OPCODE_FLAG_MAY_TRAP,
      { VALUE, VALUE, 0u }, 0u },

    { OIR_OP_BRANCH, "branch", 1u, 1u, OIR_OPCODE_RESULT_FORBIDDEN, 0u,
      OIR_OPCODE_FLAG_TERMINATOR, { BLOCK, 0u, 0u }, 0u },
    { OIR_OP_COND_BRANCH, "cond.branch", 3u, 3u, OIR_OPCODE_RESULT_FORBIDDEN, 0u,
      OIR_OPCODE_FLAG_TERMINATOR, { VALUE, BLOCK, BLOCK }, 0u },
    { OIR_OP_RETURN, "return", 0u, 1u, OIR_OPCODE_RESULT_FORBIDDEN, 0u,
      OIR_OPCODE_FLAG_TERMINATOR, { VALUE, 0u, 0u }, 0u },
    { OIR_OP_UNREACHABLE, "unreachable", 0u, 0u, OIR_OPCODE_RESULT_FORBIDDEN, 0u,
      OIR_OPCODE_FLAG_TERMINATOR | OIR_OPCODE_FLAG_MAY_TRAP,
      { 0u, 0u, 0u }, 0u }
};

const OirOpcodeDescriptor* oir_opcode_lookup(uint16_t opcode_id) {
    size_t i;
    for (i = 0u; i < sizeof(g_opcodes) / sizeof(g_opcodes[0]); ++i) {
        if (g_opcodes[i].opcode_id == opcode_id) return &g_opcodes[i];
    }
    return NULL;
}

const OirOpcodeDescriptor* oir_opcode_lookup_name(const char* name) {
    size_t i;
    if (!name || !*name) return NULL;
    for (i = 0u; i < sizeof(g_opcodes) / sizeof(g_opcodes[0]); ++i) {
        if (strcmp(g_opcodes[i].name, name) == 0) return &g_opcodes[i];
    }
    return NULL;
}

uint32_t oir_opcode_registry_count(void) {
    return (uint32_t)(sizeof(g_opcodes) / sizeof(g_opcodes[0]));
}

int oir_opcode_validate_shape(
    uint16_t opcode_id,
    const uint32_t* operand_kinds,
    uint32_t operand_count,
    int has_result
) {
    const OirOpcodeDescriptor* descriptor = oir_opcode_lookup(opcode_id);
    uint32_t i;

    if (!descriptor || operand_count < descriptor->min_operand_count ||
        operand_count > descriptor->max_operand_count ||
        (operand_count != 0u && !operand_kinds)) {
        return 0;
    }

    if ((descriptor->result_rule == OIR_OPCODE_RESULT_REQUIRED && !has_result) ||
        (descriptor->result_rule == OIR_OPCODE_RESULT_FORBIDDEN && has_result)) {
        return 0;
    }

    for (i = 0u; i < operand_count; ++i) {
        uint32_t allowed = i < OIR_OPCODE_MAX_FIXED_OPERANDS
            ? descriptor->operand_kind_mask[i]
            : descriptor->variadic_operand_kind_mask;
        uint32_t kind = operand_kinds[i];

        /* Each operand has exactly one kind; combined/unknown tags are invalid. */
        if (kind == 0u || (kind & (kind - 1u)) != 0u || (allowed & kind) == 0u) {
            return 0;
        }
    }

    return 1;
}

#undef VALUE
#undef BLOCK
#undef FUNCTION
#undef TYPE
