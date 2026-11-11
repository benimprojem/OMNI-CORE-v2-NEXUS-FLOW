#include "occ_module_api.h"
#include "oir_module.h"

#include <stdlib.h>
#include <string.h>

#define OIR_VERIFY_MAX_BLOCKS 8192u
#define OIR_VERIFY_MAX_RECORDS 1000000u
#define OIR_VERIFY_MAX_UNUSED_WARNINGS 32u
#define OIR_VERIFY_MAX_SSA_DEFINITION_ERRORS 32u

/* Stable OIR Verifier diagnostic codes. Keep these independent of the OCC ABI. */
enum {
    OIR_VERIFY_DIAG_MODULE_LIMIT = 8607u,
    OIR_VERIFY_DIAG_INTERNAL_WORKSPACE = 8608u,
    OIR_VERIFY_DIAG_PAYLOAD_DECODE = 8609u,
    OIR_VERIFY_DIAG_TYPE_RECORD = 8610u,
    OIR_VERIFY_DIAG_BLOCK_OWNERSHIP = 8611u,
    OIR_VERIFY_DIAG_INSTRUCTION_COVERAGE = 8612u,
    OIR_VERIFY_DIAG_FUNCTION_ENTRY = 8613u,
    OIR_VERIFY_DIAG_SSA_DEFINITION = 8614u,
    OIR_VERIFY_DIAG_OPCODE_SHAPE = 8615u,
    OIR_VERIFY_DIAG_OPCODE_TYPE = 8616u,
    OIR_VERIFY_DIAG_CFG_EDGE = 8617u,
    OIR_VERIFY_DIAG_TERMINATOR = 8618u,
    OIR_VERIFY_DIAG_DOMINANCE = 8619u,
    OIR_VERIFY_DIAG_SSA_USE = 8620u,
    OIR_VERIFY_WARN_UNUSED_VALUE = 8621u,
    OIR_VERIFY_NOTE_UNUSED_VALUE_LIMIT = 8622u,
    OIR_VERIFY_DIAG_TYPE_ERROR_LIMIT = 8623u,
    OIR_VERIFY_DIAG_SSA_DEFINITION_ERROR_LIMIT = 8624u,
    OIR_VERIFY_DIAG_OPCODE_TYPE_ERROR_LIMIT = 8625u
};

typedef struct OirVerifyFailure {
    uint32_t code;
    uint32_t function_id;
    uint32_t block_id;
    uint32_t instruction_id;
    uint32_t value_id;
    uint32_t type_id;
} OirVerifyFailure;

static const OccStageModuleV1 g_oir_verify_module;

static int get_type(const OirModuleView* m, uint32_t id, OirModuleType* out) {
    return id != 0u && id <= m->type_count &&
        oir_module_get_type(m, id - 1u, out) == OIR_STATUS_OK && out->id == id;
}
static int get_value(const OirModuleView* m, uint32_t id, OirModuleValue* out) {
    return id != 0u && id <= m->value_count &&
        oir_module_get_value(m, id - 1u, out) == OIR_STATUS_OK && out->id == id;
}
static int get_operand(const OirModuleView* m, const OirModuleInstruction* ins,
    uint32_t index, OirModuleOperand* out) {
    return index < ins->operand_count &&
        oir_module_get_operand(m, ins->first_operand + index, out) == OIR_STATUS_OK;
}
static int get_value_operand(const OirModuleView* m, const OirModuleInstruction* ins,
    uint32_t index, OirModuleValue* out) {
    OirModuleOperand op;
    return get_operand(m, ins, index, &op) &&
        op.kind == OIR_OPERAND_VALUE && get_value(m, op.reference_id, out);
}
static int get_block_operand(const OirModuleView* m, const OirModuleInstruction* ins,
    uint32_t index, uint32_t* out_id) {
    OirModuleOperand op;
    if (!get_operand(m, ins, index, &op) || op.kind != OIR_OPERAND_BLOCK ||
        op.reference_id == 0u || op.reference_id > m->block_count) return 0;
    *out_id = op.reference_id;
    return 1;
}
static int type_is_numeric(const OirModuleType* t) {
    if (!t) return 0;
    if (t->kind == OIR_MODULE_TYPE_INTEGER)
        return t->bit_width == 8u || t->bit_width == 16u ||
            t->bit_width == 32u || t->bit_width == 64u;
    if (t->kind == OIR_MODULE_TYPE_FLOAT)
        return t->bit_width == 32u || t->bit_width == 64u;
    return 0;
}
static int type_is_bitwise(const OirModuleType* t) {
    if (!t) return 0;
    return t->kind == OIR_MODULE_TYPE_INTEGER ||
        t->kind == OIR_MODULE_TYPE_BIT ||
        t->kind == OIR_MODULE_TYPE_BYTE ||
        t->kind == OIR_MODULE_TYPE_BIT_VECTOR ||
        t->kind == OIR_MODULE_TYPE_BYTE_BLOCK;
}
/* Shared local type contracts for constant and arithmetic instructions.
   This subset is record-local once the type and SSA value tables are valid. */
static int validate_instruction_type_contract(const OirModuleView* m, const OirModuleInstruction* ins) {
    OirModuleValue result, a, b;
    OirModuleType rt;
    if (!m || !ins) return 0;
    switch (ins->opcode_id) {
        case OIR_OP_CONST_INT:
            return ins->result_value_id != 0u && get_value(m, ins->result_value_id, &result) && get_type(m, result.type_id, &rt) && rt.kind == OIR_MODULE_TYPE_INTEGER;
        case OIR_OP_CONST_FLOAT:
            return ins->result_value_id != 0u && get_value(m, ins->result_value_id, &result) && get_type(m, result.type_id, &rt) && rt.kind == OIR_MODULE_TYPE_FLOAT;
        case OIR_OP_CONST_BOOL: {
            OirModuleOperand immediate;
            return ins->result_value_id != 0u && get_value(m, ins->result_value_id, &result) && get_type(m, result.type_id, &rt) && rt.kind == OIR_MODULE_TYPE_BOOLEAN && ins->operand_count == 1u && get_operand(m, ins, 0u, &immediate) && immediate.kind == OIR_OPERAND_INTEGER_IMMEDIATE && immediate.immediate <= 1u;
        }
        case OIR_OP_ADD: case OIR_OP_SUB: case OIR_OP_MUL: case OIR_OP_DIV: case OIR_OP_REM:
            return ins->result_value_id != 0u && get_value(m, ins->result_value_id, &result) && get_type(m, result.type_id, &rt) && type_is_numeric(&rt) && get_value_operand(m, ins, 0u, &a) && get_value_operand(m, ins, 1u, &b) && a.type_id == result.type_id && b.type_id == result.type_id && (ins->opcode_id != OIR_OP_REM || rt.kind == OIR_MODULE_TYPE_INTEGER);
        case OIR_OP_NEG:
            return ins->result_value_id != 0u && get_value(m, ins->result_value_id, &result) && get_type(m, result.type_id, &rt) && type_is_numeric(&rt) && get_value_operand(m, ins, 0u, &a) && a.type_id == result.type_id;
        case OIR_OP_BIT_AND: case OIR_OP_BIT_OR: case OIR_OP_BIT_XOR:
            return ins->result_value_id != 0u &&
                get_value(m, ins->result_value_id, &result) &&
                get_type(m, result.type_id, &rt) && type_is_bitwise(&rt) &&
                get_value_operand(m, ins, 0u, &a) &&
                get_value_operand(m, ins, 1u, &b) &&
                a.type_id == result.type_id && b.type_id == result.type_id;
        case OIR_OP_BIT_NOT:
            return ins->result_value_id != 0u &&
                get_value(m, ins->result_value_id, &result) &&
                get_type(m, result.type_id, &rt) && type_is_bitwise(&rt) &&
                get_value_operand(m, ins, 0u, &a) &&
                a.type_id == result.type_id;
        case OIR_OP_BIT_SHL: case OIR_OP_BIT_SHR:
            return ins->result_value_id != 0u &&
                get_value(m, ins->result_value_id, &result) &&
                get_type(m, result.type_id, &rt) &&
                rt.kind == OIR_MODULE_TYPE_INTEGER &&
                get_value_operand(m, ins, 0u, &a) &&
                get_value_operand(m, ins, 1u, &b) &&
                a.type_id == result.type_id &&
                get_type(m, b.type_id, &rt) &&
                rt.kind == OIR_MODULE_TYPE_INTEGER;
        case OIR_OP_CMP_EQ: case OIR_OP_CMP_NE: case OIR_OP_CMP_LT:
        case OIR_OP_CMP_LE: case OIR_OP_CMP_GT: case OIR_OP_CMP_GE:
            return ins->result_value_id != 0u && get_value(m, ins->result_value_id, &result) && get_type(m, result.type_id, &rt) && rt.kind == OIR_MODULE_TYPE_BOOLEAN && get_value_operand(m, ins, 0u, &a) && get_value_operand(m, ins, 1u, &b) && a.type_id == b.type_id;
        case OIR_OP_CAST: {
            OirModuleOperand target_type;
            OirModuleType cast_type;
            return ins->result_value_id != 0u && get_value(m, ins->result_value_id, &result) && get_type(m, result.type_id, &rt) && rt.kind != OIR_MODULE_TYPE_FUNCTION && get_value_operand(m, ins, 0u, &a) && get_operand(m, ins, 1u, &target_type) && target_type.kind == OIR_OPERAND_TYPE && target_type.reference_id == result.type_id && get_type(m, target_type.reference_id, &cast_type) && cast_type.kind != OIR_MODULE_TYPE_FUNCTION;
        }
        default: return 1;
    }
}

static uint32_t validate_instruction_type_records(const OirModuleView* m, OirVerifyFailure* failures, uint32_t capacity, uint32_t* omitted) {
    uint32_t i, count = 0u;
    if (omitted) *omitted = 0u;
    for (i = 0u; i < m->instruction_count; ++i) {
        OirModuleInstruction ins;
        if (oir_module_get_instruction(m, i, &ins) != OIR_STATUS_OK || !validate_instruction_type_contract(m, &ins)) {
            if (count < capacity) {
                memset(&failures[count], 0, sizeof(failures[count]));
                failures[count].code = OIR_VERIFY_DIAG_OPCODE_TYPE;
                failures[count].instruction_id = i + 1u;
                if (oir_module_get_instruction(m, i, &ins) == OIR_STATUS_OK) failures[count].block_id = ins.block_id;
                ++count;
            } else if (omitted && *omitted != UINT32_MAX) ++(*omitted);
        }
    }
    return count;
}
static int bit_get(const uint64_t* bits, uint32_t id) {
    return (int)((bits[id >> 6] >> (id & 63u)) & UINT64_C(1));
}
static void bit_set(uint64_t* bits, uint32_t id) {
    bits[id >> 6] |= UINT64_C(1) << (id & 63u);
}
static int has_terminator(const OirModuleInstruction* ins) {
    const OirOpcodeDescriptor* d = oir_opcode_lookup(ins->opcode_id);
    return d && (d->flags & OIR_OPCODE_FLAG_TERMINATOR) != 0u;
}

/* Type records are independent: report every malformed entry before
   skipping all passes that consume the type table. */
#define OIR_VERIFY_MAX_TYPE_ERRORS 32u
static uint32_t validate_type_records(const OirModuleView* m,
    OirVerifyFailure* failures, uint32_t capacity, uint32_t* omitted) {
    uint32_t i, count = 0u;
    if (omitted) *omitted = 0u;
    for (i = 0u; i < m->type_count; ++i) {
        OirModuleType t;
        int valid = oir_module_get_type(m, i, &t) == OIR_STATUS_OK;
        if (valid) {
            if (t.kind == OIR_MODULE_TYPE_INTEGER)
                valid = t.bit_width == 8u || t.bit_width == 16u ||
                    t.bit_width == 32u || t.bit_width == 64u;
            else if (t.kind == OIR_MODULE_TYPE_FLOAT)
                valid = t.bit_width == 32u || t.bit_width == 64u;
            else if (t.kind == OIR_MODULE_TYPE_BOOLEAN ||
                     t.kind == OIR_MODULE_TYPE_BIT)
                valid = t.bit_width == 1u;
            else if (t.kind == OIR_MODULE_TYPE_BYTE)
                valid = t.bit_width == 8u;
            else if (t.kind == OIR_MODULE_TYPE_BIT_VECTOR ||
                     t.kind == OIR_MODULE_TYPE_BYTE_BLOCK)
                valid = t.bit_width == 8u || t.bit_width == 16u ||
                    t.bit_width == 32u || t.bit_width == 64u ||
                    t.bit_width == 128u;
            else valid = t.kind == OIR_MODULE_TYPE_FUNCTION;
        }
        if (!valid) {
            if (count < capacity) {
                memset(&failures[count], 0, sizeof(failures[count]));
                failures[count].code = OIR_VERIFY_DIAG_TYPE_RECORD;
                failures[count].type_id = i + 1u;
                ++count;
            } else if (omitted && *omitted != UINT32_MAX) {
                ++(*omitted);
            }
        }
    }
    return count;
}

/* Pass 1: validate block/function ownership and instruction coverage.
   Type records have already passed their independent prerequisite pass. */
static int validate_structure(const OirModuleView* m,
    uint32_t* block_function, uint8_t* covered, OirVerifyFailure* failure) {
    uint32_t i, j, f;

    /* Establish unique block ownership and exact, non-overlapping instruction ranges. */
    for (i = 0u; i < m->block_count; ++i) {
        OirModuleBlock block;
        if (failure) { memset(failure, 0, sizeof(*failure)); failure->code = OIR_VERIFY_DIAG_BLOCK_OWNERSHIP; failure->block_id = i + 1u; }
        if (oir_module_get_block(m, i, &block) != OIR_STATUS_OK ||
            block.id != i + 1u || block.function_id == 0u ||
            block.function_id > m->function_count || block.instruction_count == 0u ||
            block.first_instruction > m->instruction_count ||
            block.instruction_count > m->instruction_count - block.first_instruction)
            return 0;

        block_function[block.id] = block.function_id;
        for (j = 0u; j < block.instruction_count; ++j) {
            uint32_t index = block.first_instruction + j;
            OirModuleInstruction ins;
            if (failure) { memset(failure, 0, sizeof(*failure)); failure->code = OIR_VERIFY_DIAG_INSTRUCTION_COVERAGE; failure->instruction_id = index + 1u; failure->block_id = block.id; failure->function_id = block.function_id; }
            if (covered[index] ||
                oir_module_get_instruction(m, index, &ins) != OIR_STATUS_OK ||
                ins.id != index + 1u || ins.block_id != block.id) return 0;
            covered[index] = 1u;
        }
    }
    for (i = 1u; i <= m->instruction_count; ++i) {
        if (!covered[i - 1u]) {
            if (failure) { memset(failure, 0, sizeof(*failure)); failure->code = OIR_VERIFY_DIAG_INSTRUCTION_COVERAGE; failure->instruction_id = i; }
            return 0;
        }
    }

    /* Functions must have a valid function type and an entry block they own. */
    for (f = 0u; f < m->function_count; ++f) {
        OirModuleFunction fn;
        if (failure) { memset(failure, 0, sizeof(*failure)); failure->code = OIR_VERIFY_DIAG_FUNCTION_ENTRY; failure->function_id = f + 1u; }
        OirModuleType ft;
        int owns_block = 0;
        if (oir_module_get_function(m, f, &fn) != OIR_STATUS_OK ||
            fn.id != f + 1u || !get_type(m, fn.function_type_id, &ft) ||
            ft.kind != OIR_MODULE_TYPE_FUNCTION || fn.entry_block_id == 0u ||
            fn.entry_block_id > m->block_count ||
            block_function[fn.entry_block_id] != fn.id) return 0;
        if (failure) failure->block_id = fn.entry_block_id;
        for (i = 1u; i <= m->block_count; ++i)
            if (block_function[i] == fn.id) owns_block = 1;
        if (!owns_block) return 0;
    }

    return 1;
}

/* SSA value records can be checked independently once type records are valid.
   Collect bounded record-local errors before building definition lookup tables. */
static uint32_t validate_ssa_value_records(const OirModuleView* m,
    OirVerifyFailure* failures, uint32_t capacity, uint32_t* omitted) {
    uint32_t i, count = 0u;
    if (omitted) *omitted = 0u;
    for (i = 0u; i < m->value_count; ++i) {
        OirModuleValue value;
        OirModuleType value_type;
        OirModuleInstruction definition;
        int valid = oir_module_get_value(m, i, &value) == OIR_STATUS_OK;
        if (valid) {
            valid = value.id == i + 1u &&
                value.defining_instruction_id != 0u &&
                value.defining_instruction_id <= m->instruction_count &&
                get_type(m, value.type_id, &value_type) &&
                value_type.kind != OIR_MODULE_TYPE_FUNCTION &&
                oir_module_get_instruction(m,
                    value.defining_instruction_id - 1u, &definition) == OIR_STATUS_OK &&
                definition.result_value_id == value.id;
        }
        if (!valid) {
            if (count < capacity) {
                memset(&failures[count], 0, sizeof(failures[count]));
                failures[count].code = OIR_VERIFY_DIAG_SSA_DEFINITION;
                failures[count].value_id = i + 1u;
                ++count;
            } else if (omitted && *omitted != UINT32_MAX) {
                ++(*omitted);
            }
        }
    }
    return count;
}

/* Once every SSA value record is valid, independently verify the reverse
   instruction -> value links. A value may have one defining instruction only;
   duplicate result references are instruction-local failures and can be bounded. */
static uint32_t validate_instruction_result_records(const OirModuleView* m,
    OirVerifyFailure* failures, uint32_t capacity, uint32_t* omitted) {
    uint32_t i, count = 0u;
    if (omitted) *omitted = 0u;
    for (i = 0u; i < m->instruction_count; ++i) {
        OirModuleInstruction instruction;
        OirModuleValue value;
        int valid = oir_module_get_instruction(m, i, &instruction) == OIR_STATUS_OK;
        if (valid && instruction.result_value_id != 0u) {
            valid = get_value(m, instruction.result_value_id, &value) &&
                value.defining_instruction_id == instruction.id;
            if (!valid) {
                if (count < capacity) {
                    memset(&failures[count], 0, sizeof(failures[count]));
                    failures[count].code = OIR_VERIFY_DIAG_SSA_DEFINITION;
                    failures[count].instruction_id = i + 1u;
                    failures[count].value_id = instruction.result_value_id;
                    ++count;
                } else if (omitted && *omitted != UINT32_MAX) {
                    ++(*omitted);
                }
            }
        }
    }
    return count;
}

/* Pass 2: validate the SSA value table and build definition lookup tables. */
static int validate_ssa_definitions(const OirModuleView* m,
    uint32_t* definition_instruction, uint32_t* definition_block,
    OirVerifyFailure* failure) {
    uint32_t i;

    /* Every value has exactly one matching definition record. */
    for (i = 0u; i < m->value_count; ++i) {
        OirModuleValue v;
        if (failure) { memset(failure, 0, sizeof(*failure)); failure->code = OIR_VERIFY_DIAG_SSA_DEFINITION; failure->value_id = i + 1u; }
        OirModuleType value_type;
        OirModuleInstruction def;
        if (oir_module_get_value(m, i, &v) != OIR_STATUS_OK ||
            v.id != i + 1u || v.defining_instruction_id == 0u ||
            v.defining_instruction_id > m->instruction_count ||
            !get_type(m, v.type_id, &value_type) ||
            value_type.kind == OIR_MODULE_TYPE_FUNCTION ||
            oir_module_get_instruction(m, v.defining_instruction_id - 1u, &def) != OIR_STATUS_OK ||
            def.result_value_id != v.id) return 0;
        definition_instruction[v.id] = v.defining_instruction_id;
        definition_block[v.id] = def.block_id;
    }
    for (i = 0u; i < m->instruction_count; ++i) {
        OirModuleInstruction ins;
        OirModuleValue result;
        if (oir_module_get_instruction(m, i, &ins) != OIR_STATUS_OK) return 0;
        if (ins.result_value_id != 0u) {
            if (!get_value(m, ins.result_value_id, &result) ||
                result.defining_instruction_id != ins.id ||
                definition_instruction[ins.result_value_id] != ins.id) return 0;
        }
    }

    return 1;
}

/* Pass 3: validate opcode shape/types and collect CFG predecessor edges. */
static int validate_instructions(const OirModuleView* m,
    const uint32_t* block_function, uint64_t* pred, size_t words,
    OirVerifyFailure* failure) {
    uint32_t i, j;

    /* Validate opcode-specific types and collect CFG predecessor edges. */
    for (i = 0u; i < m->instruction_count; ++i) {
        OirModuleInstruction ins;
        if (failure) { memset(failure, 0, sizeof(*failure)); failure->code = OIR_VERIFY_DIAG_OPCODE_TYPE; failure->instruction_id = i + 1u; }
        OirModuleValue a, b, result;
        OirModuleType at, bt, rt;
        uint32_t target_a, target_b;
        const OirOpcodeDescriptor* desc;
        if (oir_module_get_instruction(m, i, &ins) != OIR_STATUS_OK ||
            ins.block_id == 0u || ins.block_id > m->block_count) return 0;
        if (failure) { failure->block_id = ins.block_id; failure->function_id = block_function[ins.block_id]; }
        uint32_t operand_kinds[255];
        desc = oir_opcode_lookup(ins.opcode_id);
        if (!desc || ins.operand_count > 255u) return 0;
        for (j = 0u; j < ins.operand_count; ++j) {
            OirModuleOperand operand;
            if (!get_operand(m, &ins, j, &operand)) return 0;
            operand_kinds[j] = operand.kind;
        }
        if (failure) failure->code = OIR_VERIFY_DIAG_OPCODE_SHAPE;
        if (!oir_opcode_validate_shape(ins.opcode_id, operand_kinds,
            ins.operand_count, ins.result_value_id != 0u)) return 0;
        if (failure) failure->code = OIR_VERIFY_DIAG_OPCODE_TYPE;
        if (!validate_instruction_type_contract(m, &ins)) return 0;
        if (ins.result_value_id && (!get_value(m, ins.result_value_id, &result) ||
            !get_type(m, result.type_id, &rt))) return 0;
        if (!ins.result_value_id) memset(&rt, 0, sizeof(rt));

        switch (ins.opcode_id) {
            case OIR_OP_CONST_INT: case OIR_OP_CONST_FLOAT:
            case OIR_OP_CONST_BOOL: case OIR_OP_ADD: case OIR_OP_SUB:
            case OIR_OP_MUL: case OIR_OP_DIV: case OIR_OP_REM:
            case OIR_OP_NEG: case OIR_OP_BIT_AND: case OIR_OP_BIT_OR:
            case OIR_OP_BIT_XOR: case OIR_OP_BIT_NOT:
            case OIR_OP_BIT_SHL: case OIR_OP_BIT_SHR:
            case OIR_OP_CMP_EQ: case OIR_OP_CMP_NE:
            case OIR_OP_CMP_LT: case OIR_OP_CMP_LE: case OIR_OP_CMP_GT:
            case OIR_OP_CMP_GE: case OIR_OP_CAST:
                break;
            case OIR_OP_CALL: {
                OirModuleOperand callee;
                if (!get_operand(m, &ins, 0u, &callee) ||
                    callee.kind != OIR_OPERAND_FUNCTION || callee.reference_id == 0u ||
                    callee.reference_id > m->function_count) return 0;
                break;
            }
            case OIR_OP_LOAD:
                if (!ins.result_value_id || !get_value_operand(m, &ins, 0u, &a)) return 0;
                break;
            case OIR_OP_STORE:
                /* Pointer/value type compatibility is deferred until the
                   memory and pointer type model is part of the OIR schema. */
                if (ins.result_value_id || !get_value_operand(m, &ins, 0u, &a) ||
                    !get_value_operand(m, &ins, 1u, &b)) return 0;
                break;
            case OIR_OP_BRANCH:
                if (failure) failure->code = OIR_VERIFY_DIAG_CFG_EDGE;
                if (ins.result_value_id || !get_block_operand(m, &ins, 0u, &target_a) ||
                    block_function[target_a] != block_function[ins.block_id]) return 0;
                bit_set(pred + (size_t)target_a * words, ins.block_id);
                break;
            case OIR_OP_COND_BRANCH:
                if (failure) failure->code = OIR_VERIFY_DIAG_CFG_EDGE;
                if (ins.result_value_id || !get_value_operand(m, &ins, 0u, &a) ||
                    !get_type(m, a.type_id, &at) || at.kind != OIR_MODULE_TYPE_BOOLEAN ||
                    !get_block_operand(m, &ins, 1u, &target_a) ||
                    !get_block_operand(m, &ins, 2u, &target_b) ||
                    block_function[target_a] != block_function[ins.block_id] ||
                    block_function[target_b] != block_function[ins.block_id]) return 0;
                bit_set(pred + (size_t)target_a * words, ins.block_id);
                bit_set(pred + (size_t)target_b * words, ins.block_id);
                break;
            case OIR_OP_RETURN:
            case OIR_OP_UNREACHABLE:
                if (ins.result_value_id) return 0;
                break;
            default:
                return 0;
        }
        (void)bt;
    }

    return 1;
}

/* Pass 4: enforce exactly one final terminator in every basic block. */
static int validate_terminators(const OirModuleView* m, OirVerifyFailure* failure) {
    uint32_t i, j;

    /* A block has exactly one final terminator; no instruction may follow it. */
    for (i = 1u; i <= m->block_count; ++i) {
        OirModuleBlock b;
        if (failure) { memset(failure, 0, sizeof(*failure)); failure->code = OIR_VERIFY_DIAG_TERMINATOR; failure->block_id = i; }
        OirModuleInstruction last;
        if (oir_module_get_block(m, i - 1u, &b) != OIR_STATUS_OK ||
            oir_module_get_instruction(m, b.first_instruction + b.instruction_count - 1u,
                &last) != OIR_STATUS_OK) return 0;
        if (failure) failure->instruction_id = last.id;
        if (!has_terminator(&last)) return 0;
        for (j = 0u; j + 1u < b.instruction_count; ++j) {
            OirModuleInstruction ins;
            if (oir_module_get_instruction(m, b.first_instruction + j, &ins) != OIR_STATUS_OK ||
                has_terminator(&ins)) return 0;
        }
    }

    return 1;
}

/* Pass 5: compute reachability and dominators for each function. */
static int validate_dominance(const OirModuleView* m,
    const uint32_t* block_function, const uint64_t* pred, uint64_t* dom,
    uint8_t* reachable, uint32_t* queue, size_t words,
    OirVerifyFailure* failure) {
    uint32_t f, j;

    /* Per-function reachability and iterative dominators for SSA use validation. */
    for (f = 0u; f < m->function_count; ++f) {
        OirModuleFunction fn;
        uint32_t head = 0u, tail = 0u, b;
        int changed = 1;
        if (failure) { memset(failure, 0, sizeof(*failure)); failure->code = OIR_VERIFY_DIAG_DOMINANCE; failure->function_id = f + 1u; }
        if (oir_module_get_function(m, f, &fn) != OIR_STATUS_OK) return 0;
        memset(reachable, 0, (size_t)m->block_count + 1u);
        queue[tail++] = fn.entry_block_id;
        reachable[fn.entry_block_id] = 1u;
        while (head < tail) {
            uint32_t current = queue[head++];
            if (failure) failure->block_id = current;
            OirModuleBlock cb;
            OirModuleInstruction term;
            uint32_t targets[2], count = 0u, k;
            if (oir_module_get_block(m, current - 1u, &cb) != OIR_STATUS_OK ||
                oir_module_get_instruction(m,
                    cb.first_instruction + cb.instruction_count - 1u, &term) != OIR_STATUS_OK) return 0;
            if (term.opcode_id == OIR_OP_BRANCH) {
                if (!get_block_operand(m, &term, 0u, &targets[count++])) return 0;
            } else if (term.opcode_id == OIR_OP_COND_BRANCH) {
                if (!get_block_operand(m, &term, 1u, &targets[count++]) ||
                    !get_block_operand(m, &term, 2u, &targets[count++])) return 0;
            }
            for (k = 0u; k < count; ++k) {
                uint32_t t = targets[k];
                if (failure) failure->block_id = t;
                if (block_function[t] != fn.id) return 0;
                if (!reachable[t]) { reachable[t] = 1u; queue[tail++] = t; }
            }
        }
        for (b = 1u; b <= m->block_count; ++b) {
            uint64_t* row = dom + (size_t)b * words;
            if (block_function[b] != fn.id) continue;
            if (reachable[b]) {
                for (j = 1u; j <= m->block_count; ++j)
                    if (block_function[j] == fn.id && reachable[j]) bit_set(row, j);
            } else {
                /* Unreachable blocks remain independently verifiable. They
                   cannot dominate another block or consume external SSA defs. */
                memset(row, 0, words * sizeof(uint64_t));
                bit_set(row, b);
            }
            if (b == fn.entry_block_id) {
                memset(row, 0, words * sizeof(uint64_t));
                bit_set(row, b);
            }
        }
        while (changed) {
            changed = 0;
            for (b = 1u; b <= m->block_count; ++b) {
                uint64_t* row = dom + (size_t)b * words;
                uint64_t* pred_row;
                uint64_t* temp;
                int have_pred = 0;
                if (block_function[b] != fn.id || b == fn.entry_block_id || !reachable[b]) continue;
                if (failure) failure->block_id = b;
                temp = (uint64_t*)malloc(words * sizeof(uint64_t));
                if (!temp) return 0;
                for (j = 0u; j < words; ++j) temp[j] = UINT64_MAX;
                pred_row = pred + (size_t)b * words;
                for (j = 1u; j <= m->block_count; ++j) {
                    if (bit_get(pred_row, j) && reachable[j]) {
                        uint32_t k;
                        uint64_t* pdom = dom + (size_t)j * words;
                        for (k = 0u; k < words; ++k) temp[k] &= pdom[k];
                        have_pred = 1;
                    }
                }
                if (!have_pred) { free(temp); return 0; }
                bit_set(temp, b);
                if (memcmp(row, temp, words * sizeof(uint64_t)) != 0) {
                    memcpy(row, temp, words * sizeof(uint64_t));
                    changed = 1;
                }
                free(temp);
            }
        }
    }

    return 1;
}

/* Pass 6: validate local SSA order and cross-block dominance for every use. */
static int validate_ssa_uses(const OirModuleView* m,
    const uint32_t* block_function, const uint32_t* definition_instruction,
    const uint32_t* definition_block, const uint64_t* dom, size_t words,
    uint32_t* use_count, OirVerifyFailure* failure) {
    uint32_t i, j;

    /* Validate every SSA use: local definitions precede uses; external-block
       definitions must dominate the use block. */
    for (i = 0u; i < m->instruction_count; ++i) {
        OirModuleInstruction ins;
        if (failure) { memset(failure, 0, sizeof(*failure)); failure->code = OIR_VERIFY_DIAG_SSA_USE; failure->instruction_id = i + 1u; }
        if (oir_module_get_instruction(m, i, &ins) != OIR_STATUS_OK) return 0;
        if (failure) { failure->block_id = ins.block_id; failure->function_id = block_function[ins.block_id]; }
        for (j = 0u; j < ins.operand_count; ++j) {
            OirModuleOperand op;
            if (!get_operand(m, &ins, j, &op)) return 0;
            if (op.kind == OIR_OPERAND_VALUE) {
                OirModuleValue v;
                uint32_t def_id, def_block;
                if (failure) failure->value_id = op.reference_id;
                if (!get_value(m, op.reference_id, &v)) return 0;
                def_id = definition_instruction[v.id];
                def_block = definition_block[v.id];
                if (def_id == 0u || def_id >= ins.id) {
                    if (def_block == ins.block_id || def_id == 0u) return 0;
                }
                if (def_block != ins.block_id &&
                    !bit_get(dom + (size_t)ins.block_id * words, def_block)) return 0;
                if (def_block == ins.block_id && def_id >= ins.id) return 0;
                if (use_count[op.reference_id] == UINT32_MAX) return 0;
                use_count[op.reference_id]++;
            }
        }
    }

    return 1;
}

static int validate_module(const OirModuleView* m, OirVerifyFailure* failure,
    uint32_t** out_use_count) {
    uint32_t *block_function = NULL;
    uint32_t *definition_instruction = NULL, *definition_block = NULL;
    uint32_t *use_count = NULL;
    uint8_t *covered = NULL, *reachable = NULL;
    uint32_t *queue = NULL;
    uint64_t *pred = NULL, *dom = NULL;
    size_t words, matrix_words;
    int ok = 0;

    if (failure) memset(failure, 0, sizeof(*failure));
    if (out_use_count) *out_use_count = NULL;
    if (!out_use_count || !m || m->function_count == 0u || m->block_count == 0u ||
        m->instruction_count == 0u || m->block_count > OIR_VERIFY_MAX_BLOCKS ||
        m->function_count > OIR_VERIFY_MAX_RECORDS ||
        m->instruction_count > OIR_VERIFY_MAX_RECORDS ||
        m->value_count > OIR_VERIFY_MAX_RECORDS) {
        if (failure) failure->code = OIR_VERIFY_DIAG_MODULE_LIMIT;
        return 0;
    }

    words = ((size_t)m->block_count + 1u + 63u) / 64u;
    if (words == 0u || (size_t)m->block_count + 1u > SIZE_MAX / words) {
        if (failure) failure->code = OIR_VERIFY_DIAG_MODULE_LIMIT;
        return 0;
    }
    matrix_words = ((size_t)m->block_count + 1u) * words;
    if (matrix_words > SIZE_MAX / sizeof(uint64_t)) {
        if (failure) failure->code = OIR_VERIFY_DIAG_MODULE_LIMIT;
        return 0;
    }

    block_function = (uint32_t*)calloc((size_t)m->block_count + 1u, sizeof(uint32_t));
    definition_instruction = (uint32_t*)calloc((size_t)m->value_count + 1u, sizeof(uint32_t));
    definition_block = (uint32_t*)calloc((size_t)m->value_count + 1u, sizeof(uint32_t));
    use_count = (uint32_t*)calloc((size_t)m->value_count + 1u, sizeof(uint32_t));
    covered = (uint8_t*)calloc((size_t)m->instruction_count + 1u, 1u);
    reachable = (uint8_t*)calloc((size_t)m->block_count + 1u, 1u);
    queue = (uint32_t*)calloc((size_t)m->block_count + 1u, sizeof(uint32_t));
    pred = (uint64_t*)calloc(matrix_words, sizeof(uint64_t));
    dom = (uint64_t*)calloc(matrix_words, sizeof(uint64_t));
    if (!block_function || !definition_instruction ||
        !definition_block || !use_count || !covered || !reachable || !queue || !pred || !dom) {
        if (failure) failure->code = OIR_VERIFY_DIAG_INTERNAL_WORKSPACE;
        goto done;
    }

    if (!validate_structure(m, block_function, covered, failure)) goto done;

    if (!validate_ssa_definitions(m, definition_instruction, definition_block, failure))
        goto done;

    if (!validate_instructions(m, block_function, pred, words, failure)) goto done;

    if (!validate_terminators(m, failure)) goto done;

    if (!validate_dominance(m, block_function, pred, dom, reachable, queue, words, failure) ||
        !validate_ssa_uses(m, block_function, definition_instruction, definition_block, dom, words, use_count, failure))
        goto done;

    ok = 1;
    *out_use_count = use_count;
    use_count = NULL;
done:
    free(block_function);
    free(definition_instruction); free(definition_block); free(use_count);
    free(covered); free(reachable); free(queue); free(pred); free(dom);
    return ok;
}

static int find_sections(const OirArtifactView* artifact, OirSectionInput** out_sections) {
    OirSectionInput* sections;
    uint32_t i;
    int payload_count = 0;
    if (!artifact || !out_sections || artifact->section_count == 0u) return 0;
    sections = (OirSectionInput*)calloc(artifact->section_count, sizeof(*sections));
    if (!sections) return 0;
    for (i = 0u; i < artifact->section_count; ++i) {
        OirSectionView view;
        if (oir_artifact_get_section(artifact, i, &view) != OIR_STATUS_OK) {
            free(sections); return 0;
        }
        sections[i].type = view.type;
        sections[i].version = view.version;
        sections[i].flags = view.flags;
        sections[i].data = view.data;
        sections[i].length = view.length;
        if (view.type == OIR_SECTION_PAYLOAD) ++payload_count;
    }
    if (payload_count != 1) { free(sections); return 0; }
    *out_sections = sections;
    return 1;
}

static uint32_t report_verify_error(const OccModuleInvocationV1* invocation,
    OccModuleResultV1* result, uint32_t code, const char* message,
    const OirVerifyFailure* failure) {
    OccModuleDiagnosticV1 d;
    char contextual_message[384];
    const char* reported_message = message;
    const char* hint = NULL;
    const char* remediation = NULL;
    char contextual_hint[768];
    switch (code) {
        case OIR_VERIFY_DIAG_MODULE_LIMIT: message = "OIR module exceeds verifier resource limits."; remediation = "Reduce the module's block, function, instruction or value counts; also check section/string-table sizes and size arithmetic."; break;
        case OIR_VERIFY_DIAG_INTERNAL_WORKSPACE: message = "OIR Verifier could not allocate its validation workspace."; remediation = "Retry with a smaller module or free memory; if reproducible on a small valid module, report it as a verifier allocation defect."; break;
        case OIR_VERIFY_DIAG_PAYLOAD_DECODE: message = "OIR_MODULE payload is malformed or violates the registered opcode wire contract."; remediation = "Regenerate the OIR payload with the canonical OIR writer and check its schema version, section layout and each opcode's registered operand encoding."; break;
        case OIR_VERIFY_DIAG_TYPE_RECORD: message = "OIR type record is invalid."; remediation = "Check the referenced type record's kind and bit width; use only schema-supported integer, float, boolean or function type forms."; break;
        case OIR_VERIFY_DIAG_TYPE_ERROR_LIMIT: message = "Additional invalid OIR type records were omitted from diagnostics."; remediation = "Fix the reported type records, then rerun verification to reveal any remaining invalid entries."; break;
        case OIR_VERIFY_DIAG_SSA_DEFINITION_ERROR_LIMIT: message = "Additional inconsistent OIR SSA definition relationships were omitted from diagnostics."; remediation = "Fix the reported SSA value records and instruction result links, then rerun verification to reveal any remaining invalid relationships."; break;
        case OIR_VERIFY_DIAG_OPCODE_TYPE_ERROR_LIMIT: message = "Additional invalid record-local opcode type contracts were omitted from diagnostics."; remediation = "Fix the reported constant and arithmetic result/operand type contracts, then rerun verification to reveal any remaining invalid instructions."; break;
        case OIR_VERIFY_DIAG_BLOCK_OWNERSHIP: message = "OIR block ownership or instruction range is invalid."; remediation = "Assign each block to an existing function and ensure its non-empty instruction range stays within the instruction table."; break;
        case OIR_VERIFY_DIAG_INSTRUCTION_COVERAGE: message = "OIR instruction coverage is incomplete, overlapping or inconsistent."; remediation = "Make block instruction ranges cover every instruction exactly once, without overlap, and ensure each instruction's block_id matches its owning block."; break;
        case OIR_VERIFY_DIAG_FUNCTION_ENTRY: message = "OIR function entry block or function type is invalid."; remediation = "Give the function a valid function-type record and an entry_block_id that names a block owned by that same function."; break;
        case OIR_VERIFY_DIAG_SSA_DEFINITION: message = "OIR SSA value definition is missing, duplicated or inconsistent."; remediation = "Ensure every SSA value has one valid defining instruction, that the instruction's result_value_id points back to it, and that its type is valid."; break;
        case OIR_VERIFY_DIAG_OPCODE_SHAPE: message = "OIR opcode operand shape or result contract is invalid."; remediation = "Compare the opcode's operand count, operand kinds and result-presence flag with its registered opcode descriptor."; break;
        case OIR_VERIFY_DIAG_OPCODE_TYPE: message = "OIR opcode operand or result type is invalid."; remediation = "Make operand and result types satisfy the opcode contract; for arithmetic use matching numeric types, for bitwise operations use matching integer/bit/byte representation types, and for branch conditions use boolean values."; break;
        case OIR_VERIFY_DIAG_CFG_EDGE: message = "OIR control-flow edge is invalid or crosses function ownership."; remediation = "Check that each branch target exists, is a block in the same function, and that conditional branches use a boolean condition."; break;
        case OIR_VERIFY_DIAG_TERMINATOR: message = "OIR basic block has a missing or misplaced terminator."; remediation = "End every basic block with exactly one terminator and move any ordinary instruction before it."; break;
        case OIR_VERIFY_DIAG_DOMINANCE: message = "OIR control-flow dominance analysis failed."; remediation = "Check entry-block reachability and predecessor/branch relationships; ensure the CFG is consistent before relying on dominance."; break;
        case OIR_VERIFY_DIAG_SSA_USE: message = "OIR SSA use is undefined, precedes its definition or is not dominated."; remediation = "Define the value before its use in the same block, or move its definition to a block that dominates every use; values from sibling branches need an explicit merge once supported."; break;
        default:
            if (code >= 8601u && code <= 8606u)
                remediation = "Check the OIR container header, artifact kind, schema version, required sections, section bounds and job/producer metadata.";
            break;
    }
    if (code >= 8601u && code <= OIR_VERIFY_DIAG_PAYLOAD_DECODE) {
        hint = "OIR validation passes were skipped because the artifact envelope or payload could not be safely opened.";
    } else if (code >= OIR_VERIFY_DIAG_TYPE_RECORD &&
        code <= OIR_VERIFY_DIAG_SSA_DEFINITION) {
        hint = "Opcode, CFG, terminator, dominance and SSA-use passes were skipped because prerequisite structure or definitions failed.";
    } else if (code >= OIR_VERIFY_DIAG_OPCODE_SHAPE &&
        code <= OIR_VERIFY_DIAG_CFG_EDGE) {
        hint = "Terminator, dominance and SSA-use passes were skipped because instruction validation failed.";
    } else if (code == OIR_VERIFY_DIAG_OPCODE_TYPE_ERROR_LIMIT) {
        hint = "Structural, opcode, CFG, terminator, dominance and SSA-use passes were skipped because independent local opcode type contracts failed.";
    } else if (code == OIR_VERIFY_DIAG_TERMINATOR) {
        hint = "Dominance and SSA-use passes were skipped because terminator validation failed.";
    } else if (code == OIR_VERIFY_DIAG_DOMINANCE) {
        hint = "SSA-use validation was skipped because dominance analysis failed.";
    }
    if (remediation) {
        if (hint) {
            (void)snprintf(contextual_hint, sizeof(contextual_hint),
                "%s Suggested fix: %s", hint, remediation);
        } else {
            (void)snprintf(contextual_hint, sizeof(contextual_hint),
                "Suggested fix: %s", remediation);
        }
        hint = contextual_hint;
    }
    reported_message = message;
    if (failure && (failure->function_id || failure->block_id ||
        failure->instruction_id || failure->value_id || failure->type_id)) {
        (void)snprintf(contextual_message, sizeof(contextual_message),
            "%s [function=%u, block=%u, instruction=%u, value=%u, type=%u]",
            message, failure->function_id, failure->block_id,
            failure->instruction_id, failure->value_id, failure->type_id);
        reported_message = contextual_message;
    }
    if (result->diagnostics_count == 0u) result->failure_code = code;
    if (!invocation->diagnostic_sink.report) return OCC_MODULE_STATUS_FAILED;
    memset(&d, 0, sizeof(d));
    d.struct_size = (uint32_t)sizeof(d);
    d.code = code; d.message_id = code; d.severity = OCC_DIAG_ERROR;
    d.message = reported_message; d.message_length = strlen(reported_message);
    d.hint = hint; d.hint_length = hint ? strlen(hint) : 0u;
    if (invocation->diagnostic_sink.report(invocation->diagnostic_sink.user_context, &d) &&
        result->diagnostics_count != UINT32_MAX)
        result->diagnostics_count++;
    return OCC_MODULE_STATUS_FAILED;
}

static int report_unused_value_warnings(
    const OccModuleInvocationV1* invocation, OccModuleResultV1* result,
    const OirModuleView* module, const uint32_t* use_count) {
    uint32_t i, emitted = 0u, omitted = 0u;
    if (!invocation || !result || !module || !use_count ||
        !invocation->diagnostic_sink.report) return 0;
    for (i = 0u; i < module->value_count; ++i) {
        OirModuleValue value;
        OccModuleDiagnosticV1 diagnostic;
        char message[192];
        char hint[256];
        int message_length, hint_length;
        if (use_count[i + 1u] != 0u) continue;
        if (emitted >= OIR_VERIFY_MAX_UNUSED_WARNINGS) {
            omitted++;
            continue;
        }
        if (oir_module_get_value(module, i, &value) != OIR_STATUS_OK) return 0;
        message_length = snprintf(message, sizeof(message),
            "OIR SSA value is defined but never used. [value=%u]", value.id);
        hint_length = snprintf(hint, sizeof(hint),
            "Warning only; output remains valid. If this value represents an unused source variable, remove its declaration; otherwise remove the dead computation or keep it if a later pass will consume it.");
        if (message_length < 0 || (size_t)message_length >= sizeof(message) ||
            hint_length < 0 || (size_t)hint_length >= sizeof(hint)) return 0;
        memset(&diagnostic, 0, sizeof(diagnostic));
        diagnostic.struct_size = (uint32_t)sizeof(diagnostic);
        diagnostic.code = OIR_VERIFY_WARN_UNUSED_VALUE;
        diagnostic.message_id = OIR_VERIFY_WARN_UNUSED_VALUE;
        diagnostic.severity = OCC_DIAG_WARNING;
        diagnostic.message = message;
        diagnostic.message_length = (uint64_t)message_length;
        diagnostic.hint = hint;
        diagnostic.hint_length = (uint64_t)hint_length;
        if (!invocation->diagnostic_sink.report(
                invocation->diagnostic_sink.user_context, &diagnostic)) return 0;
        result->diagnostics_count++;
        emitted++;
    }
    if (omitted != 0u) {
        OccModuleDiagnosticV1 diagnostic;
        char message[160];
        int message_length = snprintf(message, sizeof(message),
            "OIR Verifier omitted %u additional unused-value warnings (limit %u).",
            omitted, OIR_VERIFY_MAX_UNUSED_WARNINGS);
        if (message_length < 0 || (size_t)message_length >= sizeof(message)) return 0;
        memset(&diagnostic, 0, sizeof(diagnostic));
        diagnostic.struct_size = (uint32_t)sizeof(diagnostic);
        diagnostic.code = OIR_VERIFY_NOTE_UNUSED_VALUE_LIMIT;
        diagnostic.message_id = OIR_VERIFY_NOTE_UNUSED_VALUE_LIMIT;
        diagnostic.severity = OCC_DIAG_NOTE;
        diagnostic.message = message;
        diagnostic.message_length = (uint64_t)message_length;
        if (!invocation->diagnostic_sink.report(
                invocation->diagnostic_sink.user_context, &diagnostic)) return 0;
        result->diagnostics_count++;
    }
    return 1;
}

static uint32_t OCC_MODULE_CALL oir_verify_process(
    const OccModuleInvocationV1* invocation, OccModuleResultV1* result) {
    OirArtifactView input;
    OirModuleView module;
    OirSectionInput* sections = NULL;
    OirArtifactHeader header;
    OirStatus status;
    size_t required = OIR_HEADER_SIZE, written = 0u;
    uint32_t i, payload_count = 0u;
    uint32_t* unused_value_counts = NULL;
    OirVerifyFailure failure;
    uint32_t rc = OCC_MODULE_STATUS_FAILED;

    memset(&failure, 0, sizeof(failure));
    result->bytes_written = 0u; result->required_output_capacity = 0u;
    result->output_validity = OIR_VALIDITY_NONE; result->failure_code = 0u;
    result->diagnostics_count = 0u;
    if (occ_module_validate_invocation_v1(&g_oir_verify_module.descriptor,
            invocation, result) != OCC_MODULE_STATUS_OK ||
        invocation->input_count != 1u || !invocation->inputs ||
        !invocation->inputs[0].bytes ||
        invocation->inputs[0].length > (uint64_t)SIZE_MAX ||
        invocation->output_capacity > (uint64_t)SIZE_MAX)
        return OCC_MODULE_STATUS_INVALID_INVOCATION;

    status = oir_artifact_open(invocation->inputs[0].bytes,
        (size_t)invocation->inputs[0].length, &input);
    if (status != OIR_STATUS_OK || input.header.artifact_kind != OIR_KIND_MODULE ||
        input.header.payload_schema_major != OIR_MODULE_SCHEMA_MAJOR ||
        input.header.payload_schema_minor != OIR_MODULE_SCHEMA_MINOR ||
        input.header.producer_stage_id != OCC_STAGE_OIR_LOWERING ||
        input.header.validity != OIR_VALIDITY_VALID ||
        input.header.job_id != invocation->job_id)
        return report_verify_error(invocation, result, 8601u,
            "OIR Verifier requires a valid OIR_MODULE 1.1 artifact from the lowering stage.", NULL);

    for (i = 0u; i < input.section_count; ++i) {
        OirSectionView section;
        if (oir_artifact_get_section(&input, i, &section) != OIR_STATUS_OK) {
            free(unused_value_counts);
            return report_verify_error(invocation, result, 8602u,
                "OIR Verifier could not read an artifact section.", NULL);
        }
        if (section.type == OIR_SECTION_PAYLOAD) {
            if (++payload_count != 1u || section.version != 1u ||
                section.length > (uint64_t)SIZE_MAX)
                return report_verify_error(invocation, result, 8603u,
                    "OIR_MODULE payload section metadata is invalid.", NULL);
            if (oir_module_open(section.data, (size_t)section.length, &module) != OIR_STATUS_OK)
                return report_verify_error(invocation, result, OIR_VERIFY_DIAG_PAYLOAD_DECODE,
                    "OIR_MODULE payload could not be decoded.", NULL);
            {
                OirVerifyFailure type_failures[OIR_VERIFY_MAX_TYPE_ERRORS];
                uint32_t omitted_type_errors = 0u;
                uint32_t type_error_count = validate_type_records(&module,
                    type_failures, OIR_VERIFY_MAX_TYPE_ERRORS, &omitted_type_errors);
                uint32_t type_error_index;
                if (type_error_count != 0u || omitted_type_errors != 0u) {
                    for (type_error_index = 0u; type_error_index < type_error_count;
                        ++type_error_index)
                        (void)report_verify_error(invocation, result,
                            OIR_VERIFY_DIAG_TYPE_RECORD, "OIR type record is invalid.",
                            &type_failures[type_error_index]);
                    if (omitted_type_errors != 0u) {
                        OirVerifyFailure overflow_failure;
                        memset(&overflow_failure, 0, sizeof(overflow_failure));
                        overflow_failure.type_id = omitted_type_errors;
                        (void)report_verify_error(invocation, result,
                            OIR_VERIFY_DIAG_TYPE_ERROR_LIMIT,
                            "Additional invalid OIR type records were omitted.", &overflow_failure);
                    }
                    return OCC_MODULE_STATUS_FAILED;
                }
            }
            {
                OirVerifyFailure ssa_failures[OIR_VERIFY_MAX_SSA_DEFINITION_ERRORS];
                uint32_t omitted_ssa_errors = 0u;
                uint32_t ssa_error_count = validate_ssa_value_records(&module,
                    ssa_failures, OIR_VERIFY_MAX_SSA_DEFINITION_ERRORS,
                    &omitted_ssa_errors);
                uint32_t ssa_error_index;
                if (ssa_error_count != 0u || omitted_ssa_errors != 0u) {
                    for (ssa_error_index = 0u; ssa_error_index < ssa_error_count;
                        ++ssa_error_index)
                        (void)report_verify_error(invocation, result,
                            OIR_VERIFY_DIAG_SSA_DEFINITION,
                            "OIR SSA value definition is invalid.",
                            &ssa_failures[ssa_error_index]);
                    if (omitted_ssa_errors != 0u) {
                        OirVerifyFailure overflow_failure;
                        memset(&overflow_failure, 0, sizeof(overflow_failure));
                        overflow_failure.value_id = omitted_ssa_errors;
                        (void)report_verify_error(invocation, result,
                            OIR_VERIFY_DIAG_SSA_DEFINITION_ERROR_LIMIT,
                            "Additional invalid SSA value definitions were omitted.",
                            &overflow_failure);
                    }
                    return OCC_MODULE_STATUS_FAILED;
                }
            }
            {
                OirVerifyFailure instruction_failures[OIR_VERIFY_MAX_SSA_DEFINITION_ERRORS];
                uint32_t omitted_instruction_errors = 0u;
                uint32_t instruction_error_count = validate_instruction_result_records(
                    &module, instruction_failures,
                    OIR_VERIFY_MAX_SSA_DEFINITION_ERRORS,
                    &omitted_instruction_errors);
                uint32_t instruction_error_index;
                if (instruction_error_count != 0u || omitted_instruction_errors != 0u) {
                    for (instruction_error_index = 0u;
                        instruction_error_index < instruction_error_count;
                        ++instruction_error_index)
                        (void)report_verify_error(invocation, result,
                            OIR_VERIFY_DIAG_SSA_DEFINITION,
                            "OIR instruction result has no matching SSA definition record.",
                            &instruction_failures[instruction_error_index]);
                    if (omitted_instruction_errors != 0u) {
                        OirVerifyFailure overflow_failure;
                        memset(&overflow_failure, 0, sizeof(overflow_failure));
                        overflow_failure.value_id = omitted_instruction_errors;
                        (void)report_verify_error(invocation, result,
                            OIR_VERIFY_DIAG_SSA_DEFINITION_ERROR_LIMIT,
                            "Additional inconsistent instruction result links were omitted.",
                            &overflow_failure);
                    }
                    return OCC_MODULE_STATUS_FAILED;
                }
            }
            {
                OirVerifyFailure type_failures[OIR_VERIFY_MAX_SSA_DEFINITION_ERRORS];
                uint32_t omitted_instruction_type_errors = 0u;
                uint32_t instruction_type_error_count = validate_instruction_type_records(
                    &module, type_failures, OIR_VERIFY_MAX_SSA_DEFINITION_ERRORS,
                    &omitted_instruction_type_errors);
                uint32_t type_error_index;
                if (instruction_type_error_count != 0u ||
                    omitted_instruction_type_errors != 0u) {
                    for (type_error_index = 0u;
                        type_error_index < instruction_type_error_count;
                        ++type_error_index)
                        (void)report_verify_error(invocation, result,
                            OIR_VERIFY_DIAG_OPCODE_TYPE,
                            "OIR instruction has an invalid local type contract.",
                            &type_failures[type_error_index]);
                    if (omitted_instruction_type_errors != 0u) {
                        OirVerifyFailure overflow_failure;
                        memset(&overflow_failure, 0, sizeof(overflow_failure));
                        overflow_failure.instruction_id = omitted_instruction_type_errors;
                        (void)report_verify_error(invocation, result,
                            OIR_VERIFY_DIAG_OPCODE_TYPE_ERROR_LIMIT,
                            "Additional invalid instruction type contracts were omitted.",
                            &overflow_failure);
                    }
                    return OCC_MODULE_STATUS_FAILED;
                }
            }
            if (!validate_module(&module, &failure, &unused_value_counts))
                return report_verify_error(invocation, result,
                    failure.code ? failure.code : 8603u,
                    "OIR_MODULE validation failed.", &failure);
        }
    }
    if (payload_count != 1u || !find_sections(&input, &sections)) {
        free(unused_value_counts);
        return report_verify_error(invocation, result, 8604u,
            "OIR_MODULE must contain exactly one required payload section.", NULL);
    }

    for (i = 0u; i < input.section_count; ++i) {
        if (sections[i].length > SIZE_MAX - required - OIR_SECTION_HEADER_SIZE) {
            free(unused_value_counts);
            free(sections);
            return report_verify_error(invocation, result, 8605u,
                "Verified OIR artifact size overflowed the host size range.", NULL);
        }
        required += OIR_SECTION_HEADER_SIZE + (size_t)sections[i].length;
    }
    if (required > (size_t)invocation->output_capacity) {
        result->required_output_capacity = (uint64_t)required;
        free(unused_value_counts);
        free(sections);
        return OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER;
    }

    /* Publish advisory diagnostics only after the output buffer is sufficient;
       NEED_OUTPUT_BUFFER retries must not duplicate or leak diagnostics. */
    if (!report_unused_value_warnings(invocation, result,
            &module, unused_value_counts)) {
        free(unused_value_counts);
        free(sections);
        return OCC_MODULE_STATUS_FAILED;
    }
    free(unused_value_counts);
    unused_value_counts = NULL;

    header = input.header;
    header.producer_stage_id = OCC_STAGE_OIR_VERIFY;
    header.validity = OIR_VALIDITY_VALID;
    status = oir_artifact_write(invocation->output_buffer,
        (size_t)invocation->output_capacity, &header, sections,
        input.section_count, &written);
    free(sections);
    if (status != OIR_STATUS_OK || written != required)
        return report_verify_error(invocation, result, 8606u,
            "The verified OIR_MODULE artifact could not be serialized.", NULL);

    result->bytes_written = (uint64_t)written;
    result->output_validity = OIR_VALIDITY_VALID;
    rc = OCC_MODULE_STATUS_OK;
    return rc;
}

static const OccStageModuleV1 g_oir_verify_module = {
    (uint32_t)sizeof(OccStageModuleV1), 0u,
    {
        (uint32_t)sizeof(OccStageDescriptorV1),
        OCC_MODULE_ABI_MAJOR, OCC_MODULE_ABI_MINOR,
        0x00010006u, OCC_STAGE_OIR_VERIFY,
        1u, 0u, 0u, 0u,
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_MODULE),
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_MODULE),
        OIR_KIND_MODULE, 1u, 1u, 0u,
        (uint64_t)OCC_MODULE_CAP_REENTRANT |
            (uint64_t)OCC_MODULE_CAP_DETERMINISTIC |
            (uint64_t)OCC_MODULE_CAP_STATELESS |
            (uint64_t)OCC_MODULE_CAP_DIAGNOSTIC_REPORTING,
        "occ-oir-module-verifier"
    },
    oir_verify_process
};

OCC_MODULE_EXPORT const OccStageModuleV1* OCC_MODULE_CALL
occ_get_stage_module_v1(void) {
    return &g_oir_verify_module;
}
