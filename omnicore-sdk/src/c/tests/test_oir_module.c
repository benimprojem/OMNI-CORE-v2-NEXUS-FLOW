#include "oir_module.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    static const uint8_t name[] = { 'f', 'o', 'o' };
    const OirModuleType types[] = {
        { 1u, OIR_MODULE_TYPE_INTEGER, 32u, OIR_MODULE_TYPE_FLAG_SIGNED },
        { 2u, OIR_MODULE_TYPE_INTEGER, 32u, 0u },
        { 3u, OIR_MODULE_TYPE_FUNCTION, 0u, 0u }
    };
    const OirModuleFunction functions[] = { { 1u, 0u, 3u, 3u, 1u } };
    const OirModuleBlock blocks[] = { { 1u, 1u, 0u, 2u } };
    const OirModuleValue values[] = { { 1u, 1u, 1u, 0u } };
    const OirModuleInstruction instructions[] = {
        { 1u, 1u, OIR_OP_CONST_INT, 0u, 1u, 0u, 0u },
        { 2u, 1u, OIR_OP_RETURN, 0u, 0u, 0u, 1u }
    };
    const OirModuleOperand operands[] = { { OIR_OPERAND_VALUE, 1u, 0u } };
    OirModuleInput input;
    OirModuleView view;
    OirModuleInstruction decoded;
    uint8_t payload[256];
    size_t written = 0;

    memset(&input, 0, sizeof(input));
    input.types = types; input.type_count = 3u;
    input.functions = functions; input.function_count = 1u;
    input.blocks = blocks; input.block_count = 1u;
    input.values = values; input.value_count = 1u;
    input.instructions = instructions; input.instruction_count = 2u;
    input.operands = operands; input.operand_count = 1u;
    input.strings = name; input.strings_length = sizeof(name);

    assert(oir_module_write(payload, sizeof(payload), &input, &written) == OIR_STATUS_OK);
    assert(oir_module_open(payload, written, &view) == OIR_STATUS_OK);
    assert(view.type_count == 3u && view.function_count == 1u);
    {
        OirModuleType signed_type, unsigned_type;
        assert(oir_module_get_type(&view, 0u, &signed_type) == OIR_STATUS_OK);
        assert(oir_module_get_type(&view, 1u, &unsigned_type) == OIR_STATUS_OK);
        assert(signed_type.bit_width == 32u && unsigned_type.bit_width == 32u);
        assert(signed_type.flags == OIR_MODULE_TYPE_FLAG_SIGNED);
        assert(unsigned_type.flags == 0u);
    }
    assert(view.block_count == 1u && view.value_count == 1u);
    assert(view.instruction_count == 2u && view.operand_count == 1u);
    assert(oir_module_get_instruction(&view, 1u, &decoded) == OIR_STATUS_OK);
    assert(decoded.opcode_id == OIR_OP_RETURN && decoded.operand_count == 1u);

    /* Unknown type flags are rejected rather than silently reinterpreted. */
    payload[45] = 0x80u;
    assert(oir_module_open(payload, written, &view) == OIR_STATUS_INVALID_SECTION);
    payload[45] = 0u;
    /* Signedness flags are only valid on integer types. */
    payload[56] = 0x01u;
    assert(oir_module_open(payload, written, &view) == OIR_STATUS_INVALID_SECTION);
    payload[56] = 0u;
    /* Corrupt an instruction opcode; structural validation must reject it. */
    payload[112 + 8] = 0xffu;
    assert(oir_module_open(payload, written, &view) == OIR_STATUS_INVALID_SECTION);
    return 0;
}
