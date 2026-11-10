#include "oir_module.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    static const uint8_t name[] = { 'f', 'o', 'o' };
    const OirModuleType types[] = {
        { 1u, OIR_MODULE_TYPE_INTEGER, 32u, OIR_MODULE_TYPE_FLAG_SIGNED },
        { 2u, OIR_MODULE_TYPE_INTEGER, 32u, 0u },
        { 3u, OIR_MODULE_TYPE_FLOAT, 32u, 0u },
        { 4u, OIR_MODULE_TYPE_FUNCTION, 0u, 0u },
        { 5u, OIR_MODULE_TYPE_BIT, 1u, 0u },
        { 6u, OIR_MODULE_TYPE_BYTE, 8u, 0u },
        { 7u, OIR_MODULE_TYPE_BIT_VECTOR, 16u, OIR_MODULE_TYPE_FLAG_HARD },
        { 8u, OIR_MODULE_TYPE_BYTE_BLOCK, 32u, OIR_MODULE_TYPE_FLAG_CONST }
    };
    const OirModuleFunction functions[] = { { 1u, 0u, 3u, 4u, 1u } };
    const OirModuleBlock blocks[] = { { 1u, 1u, 0u, 2u } };
    const OirModuleValue values[] = { { 1u, 1u, 1u, 0u } };
    const OirModuleInstruction instructions[] = {
        { 1u, 1u, OIR_OP_CONST_INT, 0u, 1u, 0u, 1u },
        { 2u, 1u, OIR_OP_RETURN, 0u, 0u, 1u, 1u }
    };
    const OirModuleOperand operands[] = {
        { OIR_OPERAND_INTEGER_IMMEDIATE, 0u, 42u },
        { OIR_OPERAND_VALUE, 1u, 0u }
    };
    OirModuleInput input;
    OirModuleView view;
    OirModuleInstruction decoded;
    uint8_t payload[512], legacy_payload[512];
    size_t written = 0, legacy_written = 0;

    memset(&input, 0, sizeof(input));
    input.types = types; input.type_count = 8u;
    input.functions = functions; input.function_count = 1u;
    input.blocks = blocks; input.block_count = 1u;
    input.values = values; input.value_count = 1u;
    input.instructions = instructions; input.instruction_count = 2u;
    input.operands = operands; input.operand_count = 2u;
    input.strings = name; input.strings_length = sizeof(name);

    assert(oir_module_write(payload, sizeof(payload), &input, &written) == OIR_STATUS_OK);
    assert(oir_module_open(payload, written, &view) == OIR_STATUS_OK);
    assert(payload[0] == OIR_MODULE_SCHEMA_MAJOR && payload[2] == OIR_MODULE_SCHEMA_MINOR);
    assert(view.type_count == 8u && view.function_count == 1u);
    {
        OirModuleType signed_type, unsigned_type, float_type;
        assert(oir_module_get_type(&view, 0u, &signed_type) == OIR_STATUS_OK);
        assert(oir_module_get_type(&view, 1u, &unsigned_type) == OIR_STATUS_OK);
        assert(oir_module_get_type(&view, 2u, &float_type) == OIR_STATUS_OK);
        assert(signed_type.bit_width == 32u && unsigned_type.bit_width == 32u);
        assert(signed_type.flags == OIR_MODULE_TYPE_FLAG_SIGNED);
        assert(unsigned_type.flags == 0u);
        assert(float_type.kind == OIR_MODULE_TYPE_FLOAT && float_type.flags == 0u);
        {
            OirModuleType bit_type, byte_type, bit_vector_type, byte_block_type;
            assert(oir_module_get_type(&view, 4u, &bit_type) == OIR_STATUS_OK);
            assert(oir_module_get_type(&view, 5u, &byte_type) == OIR_STATUS_OK);
            assert(oir_module_get_type(&view, 6u, &bit_vector_type) == OIR_STATUS_OK);
            assert(oir_module_get_type(&view, 7u, &byte_block_type) == OIR_STATUS_OK);
            assert(bit_type.kind == OIR_MODULE_TYPE_BIT && bit_type.bit_width == 1u);
            assert(byte_type.kind == OIR_MODULE_TYPE_BYTE && byte_type.bit_width == 8u);
            assert(bit_vector_type.kind == OIR_MODULE_TYPE_BIT_VECTOR &&
                bit_vector_type.bit_width == 16u &&
                bit_vector_type.flags == OIR_MODULE_TYPE_FLAG_HARD);
            assert(byte_block_type.kind == OIR_MODULE_TYPE_BYTE_BLOCK &&
                byte_block_type.bit_width == 32u &&
                byte_block_type.flags == OIR_MODULE_TYPE_FLAG_CONST);
        }
    }
    assert(view.block_count == 1u && view.value_count == 1u);
    assert(view.instruction_count == 2u && view.operand_count == 2u);
    assert(oir_module_get_instruction(&view, 1u, &decoded) == OIR_STATUS_OK);
    assert(decoded.opcode_id == OIR_OP_RETURN && decoded.operand_count == 1u);
    {
        OirModuleInstruction constant;
        OirModuleOperand immediate;
        assert(oir_module_get_instruction(&view, 0u, &constant) == OIR_STATUS_OK);
        assert(constant.opcode_id == OIR_OP_CONST_INT && constant.operand_count == 1u);
        assert(oir_module_get_operand(&view, 0u, &immediate) == OIR_STATUS_OK);
        assert(immediate.kind == OIR_OPERAND_INTEGER_IMMEDIATE);
        assert(immediate.immediate == 42u);
    }

    /* Unknown type flags are rejected rather than silently reinterpreted. */
    payload[45] = 0x80u;
    assert(oir_module_open(payload, written, &view) == OIR_STATUS_INVALID_SECTION);
    payload[45] = 0u;
    /* Signedness flags are only valid on integer types. */
    payload[68] = 0x01u;
    assert(oir_module_open(payload, written, &view) == OIR_STATUS_INVALID_SECTION);
    payload[68] = 0u;
    /* Corrupt an instruction opcode; structural validation must reject it. */
    payload[192] = 0xffu;
    assert(oir_module_open(payload, written, &view) == OIR_STATUS_INVALID_SECTION);
    payload[192] = 0u;

    /* Schema 1.1 remains readable when it contains only the original type kinds. */
    {
        OirModuleInput legacy_input = input;
        legacy_input.type_count = 4u;
        assert(oir_module_write(legacy_payload, sizeof(legacy_payload),
            &legacy_input, &legacy_written) == OIR_STATUS_OK);
        legacy_payload[2] = 1u;
        legacy_payload[3] = 0u;
        assert(oir_module_open(legacy_payload, legacy_written, &view) == OIR_STATUS_OK);
        assert(view.type_count == 4u);
    }

    /* New type kinds are not legal under the old 1.1 schema minor. */
    payload[2] = 1u;
    payload[3] = 0u;
    assert(oir_module_open(payload, written, &view) == OIR_STATUS_INVALID_SECTION);
    return 0;
}
