#ifdef NDEBUG
#undef NDEBUG
#endif
#include "occ_module_loader.h"
#include "oir.h"
#include "oir_module.h"
#include "diagnostics.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int build_module_artifact(uint8_t** out_bytes, size_t* out_length) {
    static const uint8_t strings[] = "main";
    OirModuleType types[2];
    OirModuleFunction functions[1];
    OirModuleBlock blocks[1];
    OirModuleValue values[3];
    OirModuleInstruction instructions[4];
    OirModuleOperand operands[5];
    OirModuleInput input;
    OirArtifactHeader header;
    OirSectionInput section;
    uint8_t module_bytes[4096];
    size_t module_length = 0u;
    size_t artifact_capacity;
    uint8_t* artifact;

    memset(types, 0, sizeof(types));
    types[0].id = 1u;
    types[0].kind = OIR_MODULE_TYPE_INTEGER;
    types[0].bit_width = 32u;
    types[0].flags = OIR_MODULE_TYPE_FLAG_SIGNED;
    types[1].id = 2u;
    types[1].kind = OIR_MODULE_TYPE_FUNCTION;

    memset(functions, 0, sizeof(functions));
    functions[0].id = 1u;
    functions[0].name_offset = 0u;
    functions[0].name_length = 4u;
    functions[0].function_type_id = 2u;
    functions[0].entry_block_id = 1u;

    memset(blocks, 0, sizeof(blocks));
    blocks[0].id = 1u;
    blocks[0].function_id = 1u;
    blocks[0].first_instruction = 0u;
    blocks[0].instruction_count = 4u;

    memset(values, 0, sizeof(values));
    values[0].id = 1u; values[0].type_id = 1u; values[0].defining_instruction_id = 1u;
    values[1].id = 2u; values[1].type_id = 1u; values[1].defining_instruction_id = 2u;
    values[2].id = 3u; values[2].type_id = 1u; values[2].defining_instruction_id = 3u;

    memset(instructions, 0, sizeof(instructions));
    instructions[0].id = 1u; instructions[0].block_id = 1u;
    instructions[0].opcode_id = OIR_OP_CONST_INT;
    instructions[0].result_value_id = 1u;
    instructions[0].first_operand = 0u; instructions[0].operand_count = 1u;
    instructions[1].id = 2u; instructions[1].block_id = 1u;
    instructions[1].opcode_id = OIR_OP_CONST_INT;
    instructions[1].result_value_id = 2u;
    instructions[1].first_operand = 1u; instructions[1].operand_count = 1u;
    instructions[2].id = 3u; instructions[2].block_id = 1u;
    instructions[2].opcode_id = OIR_OP_ADD;
    instructions[2].result_value_id = 3u;
    instructions[2].first_operand = 2u; instructions[2].operand_count = 2u;
    instructions[3].id = 4u; instructions[3].block_id = 1u;
    instructions[3].opcode_id = OIR_OP_RETURN;
    instructions[3].first_operand = 4u; instructions[3].operand_count = 1u;

    memset(operands, 0, sizeof(operands));
    operands[0].kind = OIR_OPERAND_INTEGER_IMMEDIATE; operands[0].immediate = 10u;
    operands[1].kind = OIR_OPERAND_INTEGER_IMMEDIATE; operands[1].immediate = 20u;
    operands[2].kind = OIR_OPERAND_VALUE; operands[2].reference_id = 1u;
    operands[3].kind = OIR_OPERAND_VALUE; operands[3].reference_id = 2u;
    operands[4].kind = OIR_OPERAND_VALUE; operands[4].reference_id = 3u;

    memset(&input, 0, sizeof(input));
    input.types = types; input.type_count = 2u;
    input.functions = functions; input.function_count = 1u;
    input.blocks = blocks; input.block_count = 1u;
    input.values = values; input.value_count = 3u;
    input.instructions = instructions; input.instruction_count = 4u;
    input.operands = operands; input.operand_count = 5u;
    input.strings = strings; input.strings_length = 4u;

    assert(oir_module_write(module_bytes, sizeof(module_bytes),
        &input, &module_length) == OIR_STATUS_OK);
    artifact_capacity = OIR_HEADER_SIZE + OIR_SECTION_HEADER_SIZE + module_length;
    artifact = (uint8_t*)malloc(artifact_capacity);
    if (!artifact) return 0;

    memset(&header, 0, sizeof(header));
    header.container_major = OIR_CONTAINER_MAJOR;
    header.container_minor = OIR_CONTAINER_MINOR;
    header.artifact_kind = OIR_KIND_MODULE;
    header.payload_schema_major = OIR_MODULE_SCHEMA_MAJOR;
    header.payload_schema_minor = OIR_MODULE_SCHEMA_MINOR;
    header.producer_stage_id = OCC_STAGE_OIR_VERIFY;
    header.validity = OIR_VALIDITY_VALID;
    header.job_id = 77u;

    memset(&section, 0, sizeof(section));
    section.type = OIR_SECTION_PAYLOAD;
    section.version = 1u;
    section.flags = OIR_SECTION_FLAG_REQUIRED;
    section.data = module_bytes;
    section.length = module_length;

    assert(oir_artifact_write(artifact, artifact_capacity, &header,
        &section, 1u, out_length) == OIR_STATUS_OK);
    *out_bytes = artifact;
    return 1;
}

static int build_float_module_artifact(
    uint8_t** out_bytes,
    size_t* out_length,
    uint16_t bit_width,
    uint64_t job_id
) {
    static const uint8_t strings[] = "main";
    OirModuleType types[2];
    OirModuleFunction functions[1];
    OirModuleBlock blocks[1];
    OirModuleValue values[5];
    OirModuleInstruction instructions[6];
    OirModuleOperand operands[8];
    OirModuleInput input;
    OirArtifactHeader header;
    OirSectionInput section;
    uint8_t module_bytes[4096];
    size_t module_length = 0u;
    size_t artifact_capacity;
    uint8_t* artifact;
    double d1 = 1.5, d2 = 2.25, d3 = 4.0;
    uint64_t bits1, bits2, bits3;

    memcpy(&bits1, &d1, sizeof(bits1));
    memcpy(&bits2, &d2, sizeof(bits2));
    memcpy(&bits3, &d3, sizeof(bits3));

    memset(types, 0, sizeof(types));
    types[0].id = 1u;
    types[0].kind = OIR_MODULE_TYPE_FLOAT;
    types[0].bit_width = bit_width;
    types[1].id = 2u;
    types[1].kind = OIR_MODULE_TYPE_FUNCTION;

    memset(functions, 0, sizeof(functions));
    functions[0].id = 1u;
    functions[0].name_offset = 0u;
    functions[0].name_length = 4u;
    functions[0].function_type_id = 2u;
    functions[0].entry_block_id = 1u;

    memset(blocks, 0, sizeof(blocks));
    blocks[0].id = 1u;
    blocks[0].function_id = 1u;
    blocks[0].first_instruction = 0u;
    blocks[0].instruction_count = 6u;

    memset(values, 0, sizeof(values));
    values[0].id = 1u; values[0].type_id = 1u; values[0].defining_instruction_id = 1u;
    values[1].id = 2u; values[1].type_id = 1u; values[1].defining_instruction_id = 2u;
    values[2].id = 3u; values[2].type_id = 1u; values[2].defining_instruction_id = 3u;
    values[3].id = 4u; values[3].type_id = 1u; values[3].defining_instruction_id = 4u;
    values[4].id = 5u; values[4].type_id = 1u; values[4].defining_instruction_id = 5u;

    memset(instructions, 0, sizeof(instructions));
    instructions[0].id = 1u; instructions[0].block_id = 1u;
    instructions[0].opcode_id = OIR_OP_CONST_FLOAT;
    instructions[0].result_value_id = 1u;
    instructions[0].first_operand = 0u; instructions[0].operand_count = 1u;
    instructions[1].id = 2u; instructions[1].block_id = 1u;
    instructions[1].opcode_id = OIR_OP_CONST_FLOAT;
    instructions[1].result_value_id = 2u;
    instructions[1].first_operand = 1u; instructions[1].operand_count = 1u;
    instructions[2].id = 3u; instructions[2].block_id = 1u;
    instructions[2].opcode_id = OIR_OP_CONST_FLOAT;
    instructions[2].result_value_id = 3u;
    instructions[2].first_operand = 2u; instructions[2].operand_count = 1u;
    instructions[3].id = 4u; instructions[3].block_id = 1u;
    instructions[3].opcode_id = OIR_OP_MUL;
    instructions[3].result_value_id = 4u;
    instructions[3].first_operand = 3u; instructions[3].operand_count = 2u;
    instructions[4].id = 5u; instructions[4].block_id = 1u;
    instructions[4].opcode_id = OIR_OP_ADD;
    instructions[4].result_value_id = 5u;
    instructions[4].first_operand = 5u; instructions[4].operand_count = 2u;
    instructions[5].id = 6u; instructions[5].block_id = 1u;
    instructions[5].opcode_id = OIR_OP_RETURN;
    instructions[5].first_operand = 7u; instructions[5].operand_count = 1u;

    memset(operands, 0, sizeof(operands));
    operands[0].kind = OIR_OPERAND_FLOAT_IMMEDIATE; operands[0].immediate = bits1;
    operands[1].kind = OIR_OPERAND_FLOAT_IMMEDIATE; operands[1].immediate = bits2;
    operands[2].kind = OIR_OPERAND_FLOAT_IMMEDIATE; operands[2].immediate = bits3;
    operands[3].kind = OIR_OPERAND_VALUE; operands[3].reference_id = 2u;
    operands[4].kind = OIR_OPERAND_VALUE; operands[4].reference_id = 3u;
    operands[5].kind = OIR_OPERAND_VALUE; operands[5].reference_id = 1u;
    operands[6].kind = OIR_OPERAND_VALUE; operands[6].reference_id = 4u;
    operands[7].kind = OIR_OPERAND_VALUE; operands[7].reference_id = 5u;

    memset(&input, 0, sizeof(input));
    input.types = types; input.type_count = 2u;
    input.functions = functions; input.function_count = 1u;
    input.blocks = blocks; input.block_count = 1u;
    input.values = values; input.value_count = 5u;
    input.instructions = instructions; input.instruction_count = 6u;
    input.operands = operands; input.operand_count = 8u;
    input.strings = strings; input.strings_length = 4u;

    assert(oir_module_write(module_bytes, sizeof(module_bytes),
        &input, &module_length) == OIR_STATUS_OK);
    artifact_capacity = OIR_HEADER_SIZE + OIR_SECTION_HEADER_SIZE + module_length;
    artifact = (uint8_t*)malloc(artifact_capacity);
    if (!artifact) return 0;

    memset(&header, 0, sizeof(header));
    header.container_major = OIR_CONTAINER_MAJOR;
    header.container_minor = OIR_CONTAINER_MINOR;
    header.artifact_kind = OIR_KIND_MODULE;
    header.payload_schema_major = OIR_MODULE_SCHEMA_MAJOR;
    header.payload_schema_minor = OIR_MODULE_SCHEMA_MINOR;
    header.producer_stage_id = OCC_STAGE_OIR_VERIFY;
    header.validity = OIR_VALIDITY_VALID;
    header.job_id = job_id;

    memset(&section, 0, sizeof(section));
    section.type = OIR_SECTION_PAYLOAD;
    section.version = 1u;
    section.flags = OIR_SECTION_FLAG_REQUIRED;
    section.data = module_bytes;
    section.length = module_length;

    assert(oir_artifact_write(artifact, artifact_capacity, &header,
        &section, 1u, out_length) == OIR_STATUS_OK);
    *out_bytes = artifact;
    return 1;
}

static int build_cfg_module_artifact(uint8_t** out_bytes, size_t* out_length) {
    static const uint8_t strings[] = "main";
    OirModuleType types[3];
    OirModuleFunction functions[1];
    OirModuleBlock blocks[3];
    OirModuleValue values[5];
    OirModuleInstruction instructions[8];
    OirModuleOperand operands[11];
    OirModuleInput input;
    OirArtifactHeader header;
    OirSectionInput section;
    uint8_t module_bytes[4096];
    uint8_t* artifact;
    size_t module_length = 0u, artifact_capacity;

    memset(types, 0, sizeof(types));
    types[0].id = 1u; types[0].kind = OIR_MODULE_TYPE_INTEGER;
    types[0].bit_width = 32u; types[0].flags = OIR_MODULE_TYPE_FLAG_SIGNED;
    types[1].id = 2u; types[1].kind = OIR_MODULE_TYPE_BOOLEAN;
    types[1].bit_width = 1u;
    types[2].id = 3u; types[2].kind = OIR_MODULE_TYPE_FUNCTION;

    memset(functions, 0, sizeof(functions));
    functions[0].id = 1u; functions[0].name_length = 4u;
    functions[0].function_type_id = 3u; functions[0].entry_block_id = 1u;

    memset(blocks, 0, sizeof(blocks));
    blocks[0].id = 1u; blocks[0].function_id = 1u;
    blocks[0].first_instruction = 0u; blocks[0].instruction_count = 4u;
    blocks[1].id = 2u; blocks[1].function_id = 1u;
    blocks[1].first_instruction = 4u; blocks[1].instruction_count = 2u;
    blocks[2].id = 3u; blocks[2].function_id = 1u;
    blocks[2].first_instruction = 6u; blocks[2].instruction_count = 2u;

    memset(values, 0, sizeof(values));
    values[0].id = 1u; values[0].type_id = 1u; values[0].defining_instruction_id = 1u;
    values[1].id = 2u; values[1].type_id = 1u; values[1].defining_instruction_id = 2u;
    values[2].id = 3u; values[2].type_id = 2u; values[2].defining_instruction_id = 3u;
    values[3].id = 4u; values[3].type_id = 1u; values[3].defining_instruction_id = 5u;
    values[4].id = 5u; values[4].type_id = 1u; values[4].defining_instruction_id = 7u;

    memset(instructions, 0, sizeof(instructions));
    instructions[0].id = 1u; instructions[0].block_id = 1u;
    instructions[0].opcode_id = OIR_OP_CONST_INT; instructions[0].result_value_id = 1u;
    instructions[0].first_operand = 0u; instructions[0].operand_count = 1u;
    instructions[1].id = 2u; instructions[1].block_id = 1u;
    instructions[1].opcode_id = OIR_OP_CONST_INT; instructions[1].result_value_id = 2u;
    instructions[1].first_operand = 1u; instructions[1].operand_count = 1u;
    instructions[2].id = 3u; instructions[2].block_id = 1u;
    instructions[2].opcode_id = OIR_OP_CMP_LT; instructions[2].result_value_id = 3u;
    instructions[2].first_operand = 2u; instructions[2].operand_count = 2u;
    instructions[3].id = 4u; instructions[3].block_id = 1u;
    instructions[3].opcode_id = OIR_OP_COND_BRANCH;
    instructions[3].first_operand = 4u; instructions[3].operand_count = 3u;
    instructions[4].id = 5u; instructions[4].block_id = 2u;
    instructions[4].opcode_id = OIR_OP_CONST_INT; instructions[4].result_value_id = 4u;
    instructions[4].first_operand = 7u; instructions[4].operand_count = 1u;
    instructions[5].id = 6u; instructions[5].block_id = 2u;
    instructions[5].opcode_id = OIR_OP_RETURN;
    instructions[5].first_operand = 8u; instructions[5].operand_count = 1u;
    instructions[6].id = 7u; instructions[6].block_id = 3u;
    instructions[6].opcode_id = OIR_OP_CONST_INT; instructions[6].result_value_id = 5u;
    instructions[6].first_operand = 9u; instructions[6].operand_count = 1u;
    instructions[7].id = 8u; instructions[7].block_id = 3u;
    instructions[7].opcode_id = OIR_OP_RETURN;
    instructions[7].first_operand = 10u; instructions[7].operand_count = 1u;

    memset(operands, 0, sizeof(operands));
    operands[0].kind = OIR_OPERAND_INTEGER_IMMEDIATE; operands[0].immediate = 42u;
    operands[1].kind = OIR_OPERAND_INTEGER_IMMEDIATE; operands[1].immediate = 24u;
    operands[2].kind = OIR_OPERAND_VALUE; operands[2].reference_id = 1u;
    operands[3].kind = OIR_OPERAND_VALUE; operands[3].reference_id = 2u;
    operands[4].kind = OIR_OPERAND_VALUE; operands[4].reference_id = 3u;
    operands[5].kind = OIR_OPERAND_BLOCK; operands[5].reference_id = 2u;
    operands[6].kind = OIR_OPERAND_BLOCK; operands[6].reference_id = 3u;
    operands[7].kind = OIR_OPERAND_INTEGER_IMMEDIATE; operands[7].immediate = 10u;
    operands[8].kind = OIR_OPERAND_VALUE; operands[8].reference_id = 4u;
    operands[9].kind = OIR_OPERAND_INTEGER_IMMEDIATE; operands[9].immediate = 20u;
    operands[10].kind = OIR_OPERAND_VALUE; operands[10].reference_id = 5u;

    memset(&input, 0, sizeof(input));
    input.types = types; input.type_count = 3u;
    input.functions = functions; input.function_count = 1u;
    input.blocks = blocks; input.block_count = 3u;
    input.values = values; input.value_count = 5u;
    input.instructions = instructions; input.instruction_count = 8u;
    input.operands = operands; input.operand_count = 11u;
    input.strings = strings; input.strings_length = 4u;
    assert(oir_module_write(module_bytes, sizeof(module_bytes), &input,
        &module_length) == OIR_STATUS_OK);

    artifact_capacity = OIR_HEADER_SIZE + OIR_SECTION_HEADER_SIZE + module_length;
    artifact = (uint8_t*)malloc(artifact_capacity);
    if (!artifact) return 0;
    memset(&header, 0, sizeof(header));
    header.container_major = OIR_CONTAINER_MAJOR;
    header.container_minor = OIR_CONTAINER_MINOR;
    header.artifact_kind = OIR_KIND_MODULE;
    header.payload_schema_major = OIR_MODULE_SCHEMA_MAJOR;
    header.payload_schema_minor = OIR_MODULE_SCHEMA_MINOR;
    header.producer_stage_id = OCC_STAGE_OIR_VERIFY;
    header.validity = OIR_VALIDITY_VALID;
    header.job_id = 80u;
    memset(&section, 0, sizeof(section));
    section.type = OIR_SECTION_PAYLOAD; section.version = 1u;
    section.flags = OIR_SECTION_FLAG_REQUIRED;
    section.data = module_bytes; section.length = module_length;
    assert(oir_artifact_write(artifact, artifact_capacity, &header,
        &section, 1u, out_length) == OIR_STATUS_OK);
    *out_bytes = artifact;
    return 1;
}

static int build_float_compare_module_artifact(uint8_t** out_bytes, size_t* out_length) {
    static const uint8_t strings[] = "main";
    OirModuleType types[3];
    OirModuleFunction functions[1];
    OirModuleBlock blocks[3];
    OirModuleValue values[5];
    OirModuleInstruction instructions[8];
    OirModuleOperand operands[11];
    OirModuleInput input;
    OirArtifactHeader header;
    OirSectionInput section;
    uint8_t module_bytes[4096];
    uint8_t* artifact;
    double constants[4] = { 1.5, 2.0, 10.0, 20.0 };
    uint64_t bits[4];
    size_t module_length = 0u, artifact_capacity;
    uint32_t i;

    for (i = 0u; i < 4u; ++i) memcpy(&bits[i], &constants[i], sizeof(bits[i]));
    memset(types, 0, sizeof(types));
    types[0].id = 1u; types[0].kind = OIR_MODULE_TYPE_FLOAT; types[0].bit_width = 64u;
    types[1].id = 2u; types[1].kind = OIR_MODULE_TYPE_BOOLEAN; types[1].bit_width = 1u;
    types[2].id = 3u; types[2].kind = OIR_MODULE_TYPE_FUNCTION;

    memset(functions, 0, sizeof(functions));
    functions[0].id = 1u; functions[0].name_length = 4u;
    functions[0].function_type_id = 3u; functions[0].entry_block_id = 1u;

    memset(blocks, 0, sizeof(blocks));
    blocks[0].id = 1u; blocks[0].function_id = 1u;
    blocks[0].first_instruction = 0u; blocks[0].instruction_count = 4u;
    blocks[1].id = 2u; blocks[1].function_id = 1u;
    blocks[1].first_instruction = 4u; blocks[1].instruction_count = 2u;
    blocks[2].id = 3u; blocks[2].function_id = 1u;
    blocks[2].first_instruction = 6u; blocks[2].instruction_count = 2u;

    memset(values, 0, sizeof(values));
    values[0].id = 1u; values[0].type_id = 1u; values[0].defining_instruction_id = 1u;
    values[1].id = 2u; values[1].type_id = 1u; values[1].defining_instruction_id = 2u;
    values[2].id = 3u; values[2].type_id = 2u; values[2].defining_instruction_id = 3u;
    values[3].id = 4u; values[3].type_id = 1u; values[3].defining_instruction_id = 5u;
    values[4].id = 5u; values[4].type_id = 1u; values[4].defining_instruction_id = 7u;

    memset(instructions, 0, sizeof(instructions));
    instructions[0].id = 1u; instructions[0].block_id = 1u;
    instructions[0].opcode_id = OIR_OP_CONST_FLOAT; instructions[0].result_value_id = 1u;
    instructions[0].first_operand = 0u; instructions[0].operand_count = 1u;
    instructions[1].id = 2u; instructions[1].block_id = 1u;
    instructions[1].opcode_id = OIR_OP_CONST_FLOAT; instructions[1].result_value_id = 2u;
    instructions[1].first_operand = 1u; instructions[1].operand_count = 1u;
    instructions[2].id = 3u; instructions[2].block_id = 1u;
    instructions[2].opcode_id = OIR_OP_CMP_LT; instructions[2].result_value_id = 3u;
    instructions[2].first_operand = 2u; instructions[2].operand_count = 2u;
    instructions[3].id = 4u; instructions[3].block_id = 1u;
    instructions[3].opcode_id = OIR_OP_COND_BRANCH;
    instructions[3].first_operand = 4u; instructions[3].operand_count = 3u;
    instructions[4].id = 5u; instructions[4].block_id = 2u;
    instructions[4].opcode_id = OIR_OP_CONST_FLOAT; instructions[4].result_value_id = 4u;
    instructions[4].first_operand = 7u; instructions[4].operand_count = 1u;
    instructions[5].id = 6u; instructions[5].block_id = 2u;
    instructions[5].opcode_id = OIR_OP_RETURN;
    instructions[5].first_operand = 8u; instructions[5].operand_count = 1u;
    instructions[6].id = 7u; instructions[6].block_id = 3u;
    instructions[6].opcode_id = OIR_OP_CONST_FLOAT; instructions[6].result_value_id = 5u;
    instructions[6].first_operand = 9u; instructions[6].operand_count = 1u;
    instructions[7].id = 8u; instructions[7].block_id = 3u;
    instructions[7].opcode_id = OIR_OP_RETURN;
    instructions[7].first_operand = 10u; instructions[7].operand_count = 1u;

    memset(operands, 0, sizeof(operands));
    for (i = 0u; i < 4u; ++i) {
        operands[i].kind = OIR_OPERAND_FLOAT_IMMEDIATE;
        operands[i].immediate = bits[i];
    }
    operands[4].kind = OIR_OPERAND_VALUE; operands[4].reference_id = 3u;
    operands[5].kind = OIR_OPERAND_BLOCK; operands[5].reference_id = 2u;
    operands[6].kind = OIR_OPERAND_BLOCK; operands[6].reference_id = 3u;
    operands[7].kind = OIR_OPERAND_FLOAT_IMMEDIATE; operands[7].immediate = bits[2];
    operands[8].kind = OIR_OPERAND_VALUE; operands[8].reference_id = 4u;
    operands[9].kind = OIR_OPERAND_FLOAT_IMMEDIATE; operands[9].immediate = bits[3];
    operands[10].kind = OIR_OPERAND_VALUE; operands[10].reference_id = 5u;

    memset(&input, 0, sizeof(input));
    input.types = types; input.type_count = 3u;
    input.functions = functions; input.function_count = 1u;
    input.blocks = blocks; input.block_count = 3u;
    input.values = values; input.value_count = 5u;
    input.instructions = instructions; input.instruction_count = 8u;
    input.operands = operands; input.operand_count = 11u;
    input.strings = strings; input.strings_length = 4u;
    assert(oir_module_write(module_bytes, sizeof(module_bytes), &input,
        &module_length) == OIR_STATUS_OK);

    artifact_capacity = OIR_HEADER_SIZE + OIR_SECTION_HEADER_SIZE + module_length;
    artifact = (uint8_t*)malloc(artifact_capacity);
    if (!artifact) return 0;
    memset(&header, 0, sizeof(header));
    header.container_major = OIR_CONTAINER_MAJOR;
    header.container_minor = OIR_CONTAINER_MINOR;
    header.artifact_kind = OIR_KIND_MODULE;
    header.payload_schema_major = OIR_MODULE_SCHEMA_MAJOR;
    header.payload_schema_minor = OIR_MODULE_SCHEMA_MINOR;
    header.producer_stage_id = OCC_STAGE_OIR_VERIFY;
    header.validity = OIR_VALIDITY_VALID;
    header.job_id = 81u;
    memset(&section, 0, sizeof(section));
    section.type = OIR_SECTION_PAYLOAD; section.version = 1u;
    section.flags = OIR_SECTION_FLAG_REQUIRED;
    section.data = module_bytes; section.length = module_length;
    assert(oir_artifact_write(artifact, artifact_capacity, &header,
        &section, 1u, out_length) == OIR_STATUS_OK);
    *out_bytes = artifact;
    return 1;
}

int main(int argc, char** argv) {
    OccLoadedStageModule loaded;
    OccDiagnosticCollector diagnostics;
    OccModuleArtifactInputV1 input;
    OirArtifactView output_artifact;
    OirSectionView payload;
    uint8_t* module_bytes = NULL;
    size_t module_length = 0u;
    uint8_t* output = NULL;
    size_t output_length = 0u;
    char* assembly;
    char error[512];
    uint32_t i;
    int found_payload = 0;

    assert(argc == 2);
    memset(&loaded, 0, sizeof(loaded));
    assert(build_module_artifact(&module_bytes, &module_length));
    assert(occ_module_loader_open(&loaded, argv[1], error, sizeof(error)) ==
        OCC_MODULE_LOADER_OK);
    assert(loaded.module->descriptor.stage_id == OCC_STAGE_CODEGEN);
    assert(loaded.module->descriptor.output_artifact_kind == OIR_KIND_ASSEMBLY);

    memset(&input, 0, sizeof(input));
    input.bytes = module_bytes;
    input.length = module_length;
    occ_diagnostics_init(&diagnostics);

    /* Force the output-buffer retry path for the assembly artifact. */
    assert(occ_module_loader_invoke(&loaded, &input, 1u, 77u,
        &diagnostics, 64u, 1024u * 1024u,
        &output, &output_length) == OCC_MODULE_LOADER_OK);
    assert(output && output_length > 96u);
    assert(occ_diagnostics_count(&diagnostics) == 0u);
    assert(oir_artifact_open(output, output_length, &output_artifact) == OIR_STATUS_OK);
    assert(output_artifact.header.artifact_kind == OIR_KIND_ASSEMBLY);
    assert(output_artifact.header.payload_schema_major == 1u);
    assert(output_artifact.header.payload_schema_minor == 0u);
    assert(output_artifact.header.producer_stage_id == OCC_STAGE_CODEGEN);
    assert(output_artifact.header.validity == OIR_VALIDITY_VALID);
    assert(output_artifact.header.job_id == 77u);

    memset(&payload, 0, sizeof(payload));
    for (i = 0u; i < output_artifact.section_count; ++i) {
        OirSectionView section;
        assert(oir_artifact_get_section(&output_artifact, i, &section) == OIR_STATUS_OK);
        if (section.type == OIR_SECTION_PAYLOAD) {
            assert(!found_payload);
            payload = section;
            found_payload = 1;
        }
    }
    assert(found_payload && payload.length > 0u);
    assembly = (char*)malloc((size_t)payload.length + 1u);
    assert(assembly);
    memcpy(assembly, payload.data, (size_t)payload.length);
    assembly[payload.length] = '\0';
    assert(strstr(assembly, "global _kernel_main") != NULL);
    assert(strstr(assembly, "call main") != NULL);
    assert(strstr(assembly, "add eax, ebx") != NULL);
    assert(strstr(assembly, "ret") != NULL);

    free(assembly);
    free(output);
    free(module_bytes);

    /* The same backend must also preserve f64 values through SSE2 operations.
     * The BareMetal64 scalar ABI returns the IEEE bit pattern in RAX. */
    assembly = NULL;
    output = NULL;
    output_length = 0u;
    module_bytes = NULL;
    module_length = 0u;
    found_payload = 0;
    assert(build_float_module_artifact(&module_bytes, &module_length, 64u, 78u));
    input.bytes = module_bytes;
    input.length = module_length;
    assert(occ_module_loader_invoke(&loaded, &input, 1u, 78u,
        &diagnostics, 64u, 1024u * 1024u,
        &output, &output_length) == OCC_MODULE_LOADER_OK);
    assert(output && output_length > 96u);
    assert(oir_artifact_open(output, output_length, &output_artifact) == OIR_STATUS_OK);
    assert(output_artifact.header.artifact_kind == OIR_KIND_ASSEMBLY);
    assert(output_artifact.header.payload_schema_major == 1u);
    assert(output_artifact.header.payload_schema_minor == 0u);
    assert(output_artifact.header.producer_stage_id == OCC_STAGE_CODEGEN);
    assert(output_artifact.header.job_id == 78u);

    memset(&payload, 0, sizeof(payload));
    for (i = 0u; i < output_artifact.section_count; ++i) {
        OirSectionView section;
        assert(oir_artifact_get_section(&output_artifact, i, &section) == OIR_STATUS_OK);
        if (section.type == OIR_SECTION_PAYLOAD) {
            assert(!found_payload);
            payload = section;
            found_payload = 1;
        }
    }
    assert(found_payload && payload.length > 0u);
    assembly = (char*)malloc((size_t)payload.length + 1u);
    assert(assembly);
    memcpy(assembly, payload.data, (size_t)payload.length);
    assembly[payload.length] = '\0';
    assert(strstr(assembly, "mulsd xmm0, xmm1") != NULL);
    assert(strstr(assembly, "addsd xmm0, xmm1") != NULL);
    assert(strstr(assembly, "movq rax, xmm0") != NULL);

    free(assembly);
    free(output);
    free(module_bytes);

    /* f32 constants enter the OIR payload as F64 values and are explicitly
     * rounded by the backend with CVTSD2SS before SSE single-precision ops. */
    assembly = NULL;
    output = NULL;
    output_length = 0u;
    module_bytes = NULL;
    module_length = 0u;
    found_payload = 0;
    assert(build_float_module_artifact(&module_bytes, &module_length, 32u, 79u));
    input.bytes = module_bytes;
    input.length = module_length;
    assert(occ_module_loader_invoke(&loaded, &input, 1u, 79u,
        &diagnostics, 64u, 1024u * 1024u,
        &output, &output_length) == OCC_MODULE_LOADER_OK);
    assert(output && output_length > 96u);
    assert(oir_artifact_open(output, output_length, &output_artifact) == OIR_STATUS_OK);
    assert(output_artifact.header.artifact_kind == OIR_KIND_ASSEMBLY);
    assert(output_artifact.header.payload_schema_major == 1u);
    assert(output_artifact.header.payload_schema_minor == 0u);
    assert(output_artifact.header.producer_stage_id == OCC_STAGE_CODEGEN);
    assert(output_artifact.header.job_id == 79u);

    memset(&payload, 0, sizeof(payload));
    for (i = 0u; i < output_artifact.section_count; ++i) {
        OirSectionView section;
        assert(oir_artifact_get_section(&output_artifact, i, &section) == OIR_STATUS_OK);
        if (section.type == OIR_SECTION_PAYLOAD) {
            assert(!found_payload);
            payload = section;
            found_payload = 1;
        }
    }
    assert(found_payload && payload.length > 0u);
    assembly = (char*)malloc((size_t)payload.length + 1u);
    assert(assembly);
    memcpy(assembly, payload.data, (size_t)payload.length);
    assembly[payload.length] = '\0';
    assert(strstr(assembly, "cvtsd2ss xmm0, xmm0") != NULL);
    assert(strstr(assembly, "mulss xmm0, xmm1") != NULL);
    assert(strstr(assembly, "addss xmm0, xmm1") != NULL);
    assert(strstr(assembly, "movd eax, xmm0") != NULL);

    free(assembly);
    free(output);
    free(module_bytes);

    /* Multi-block conditional CFG: both branch targets are emitted as labels,
       and the branch uses the boolean SSA slot as its condition. */
    assembly = NULL;
    output = NULL;
    output_length = 0u;
    module_bytes = NULL;
    module_length = 0u;
    found_payload = 0;
    assert(build_cfg_module_artifact(&module_bytes, &module_length));
    input.bytes = module_bytes;
    input.length = module_length;
    assert(occ_module_loader_invoke(&loaded, &input, 1u, 80u,
        &diagnostics, 64u, 1024u * 1024u,
        &output, &output_length) == OCC_MODULE_LOADER_OK);
    assert(oir_artifact_open(output, output_length, &output_artifact) == OIR_STATUS_OK);
    memset(&payload, 0, sizeof(payload));
    for (i = 0u; i < output_artifact.section_count; ++i) {
        OirSectionView section_view;
        assert(oir_artifact_get_section(&output_artifact, i, &section_view) == OIR_STATUS_OK);
        if (section_view.type == OIR_SECTION_PAYLOAD) {
            assert(!found_payload);
            payload = section_view;
            found_payload = 1;
        }
    }
    assert(found_payload && payload.length > 0u);
    assembly = (char*)malloc((size_t)payload.length + 1u);
    assert(assembly);
    memcpy(assembly, payload.data, (size_t)payload.length);
    assembly[payload.length] = '\0';
    assert(strstr(assembly, "jmp .Lbb_1") != NULL);
    assert(strstr(assembly, ".Lbb_2:") != NULL);
    assert(strstr(assembly, ".Lbb_3:") != NULL);
    assert(strstr(assembly, "cmp eax, ebx") != NULL);
    assert(strstr(assembly, "setl al") != NULL);
    assert(strstr(assembly, "movzx eax, al") != NULL);
    assert(strstr(assembly, "test eax, eax") != NULL);
    assert(strstr(assembly, "jne .Lbb_2") != NULL);
    assert(strstr(assembly, "jmp .Lbb_3") != NULL);

    free(assembly);
    free(output);
    free(module_bytes);

    /* IEEE floating comparison uses UCOMISD and explicitly filters unordered
       NaN flags for ordered less-than semantics. */
    assembly = NULL;
    output = NULL;
    output_length = 0u;
    module_bytes = NULL;
    module_length = 0u;
    found_payload = 0;
    assert(build_float_compare_module_artifact(&module_bytes, &module_length));
    input.bytes = module_bytes;
    input.length = module_length;
    assert(occ_module_loader_invoke(&loaded, &input, 1u, 81u,
        &diagnostics, 64u, 1024u * 1024u,
        &output, &output_length) == OCC_MODULE_LOADER_OK);
    assert(oir_artifact_open(output, output_length, &output_artifact) == OIR_STATUS_OK);
    memset(&payload, 0, sizeof(payload));
    for (i = 0u; i < output_artifact.section_count; ++i) {
        OirSectionView section_view;
        assert(oir_artifact_get_section(&output_artifact, i, &section_view) == OIR_STATUS_OK);
        if (section_view.type == OIR_SECTION_PAYLOAD) {
            assert(!found_payload);
            payload = section_view;
            found_payload = 1;
        }
    }
    assert(found_payload && payload.length > 0u);
    assembly = (char*)malloc((size_t)payload.length + 1u);
    assert(assembly);
    memcpy(assembly, payload.data, (size_t)payload.length);
    assembly[payload.length] = '\0';
    assert(strstr(assembly, "ucomisd xmm0, xmm1") != NULL);
    assert(strstr(assembly, "setb al") != NULL);
    assert(strstr(assembly, "setnp dl") != NULL);
    assert(strstr(assembly, "and al, dl") != NULL);

    free(assembly);
    free(output);
    free(module_bytes);
    occ_diagnostics_destroy(&diagnostics);
    occ_module_loader_close(&loaded);
    return 0;
}
