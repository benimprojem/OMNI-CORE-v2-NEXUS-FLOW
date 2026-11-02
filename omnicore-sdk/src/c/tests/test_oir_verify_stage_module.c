#include "occ_module_loader.h"
#include "oir_module.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int build_module_artifact(uint8_t** out, size_t* out_length, int invalid_ssa) {
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
    uint8_t module_bytes[2048];
    uint8_t* artifact;
    size_t module_length = 0u, artifact_capacity;

    memset(types, 0, sizeof(types));
    types[0].id = 1u; types[0].kind = OIR_MODULE_TYPE_INTEGER;
    types[0].bit_width = 32u; types[0].flags = OIR_MODULE_TYPE_FLAG_SIGNED;
    types[1].id = 2u; types[1].kind = OIR_MODULE_TYPE_FUNCTION;
    memset(functions, 0, sizeof(functions));
    functions[0].id = 1u; functions[0].name_length = 4u;
    functions[0].function_type_id = 2u; functions[0].entry_block_id = 1u;
    memset(blocks, 0, sizeof(blocks));
    blocks[0].id = 1u; blocks[0].function_id = 1u;
    blocks[0].first_instruction = 0u; blocks[0].instruction_count = 4u;

    memset(values, 0, sizeof(values));
    values[0].id = 1u; values[0].type_id = 1u; values[0].defining_instruction_id = 1u;
    values[1].id = 2u; values[1].type_id = 1u; values[1].defining_instruction_id = 3u;
    values[2].id = 3u; values[2].type_id = 1u; values[2].defining_instruction_id = 2u;

    memset(instructions, 0, sizeof(instructions));
    instructions[0].id = 1u; instructions[0].block_id = 1u;
    instructions[0].opcode_id = OIR_OP_CONST_INT; instructions[0].result_value_id = 1u;
    instructions[0].first_operand = 0u; instructions[0].operand_count = 1u;
    instructions[1].id = 2u; instructions[1].block_id = 1u;
    instructions[1].opcode_id = OIR_OP_ADD; instructions[1].result_value_id = 3u;
    instructions[1].first_operand = 1u; instructions[1].operand_count = 2u;
    instructions[2].id = 3u; instructions[2].block_id = 1u;
    instructions[2].opcode_id = OIR_OP_CONST_INT; instructions[2].result_value_id = 2u;
    instructions[2].first_operand = 3u; instructions[2].operand_count = 1u;
    instructions[3].id = 4u; instructions[3].block_id = 1u;
    instructions[3].opcode_id = OIR_OP_RETURN;
    instructions[3].first_operand = 4u; instructions[3].operand_count = 1u;

    memset(operands, 0, sizeof(operands));
    operands[0].kind = OIR_OPERAND_INTEGER_IMMEDIATE; operands[0].immediate = 7u;
    operands[1].kind = OIR_OPERAND_VALUE; operands[1].reference_id = 1u;
    operands[2].kind = OIR_OPERAND_VALUE; operands[2].reference_id = invalid_ssa ? 2u : 1u;
    operands[3].kind = OIR_OPERAND_INTEGER_IMMEDIATE; operands[3].immediate = 9u;
    operands[4].kind = OIR_OPERAND_VALUE; operands[4].reference_id = 3u;

    memset(&input, 0, sizeof(input));
    input.types = types; input.type_count = 2u;
    input.functions = functions; input.function_count = 1u;
    input.blocks = blocks; input.block_count = 1u;
    input.values = values; input.value_count = 3u;
    input.instructions = instructions; input.instruction_count = 4u;
    input.operands = operands; input.operand_count = 5u;
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
    header.producer_stage_id = OCC_STAGE_OIR_LOWERING;
    header.validity = OIR_VALIDITY_VALID;
    header.job_id = 501u;
    memset(&section, 0, sizeof(section));
    section.type = OIR_SECTION_PAYLOAD; section.version = 1u;
    section.flags = OIR_SECTION_FLAG_REQUIRED;
    section.data = module_bytes; section.length = module_length;
    assert(oir_artifact_write(artifact, artifact_capacity, &header,
        &section, 1u, out_length) == OIR_STATUS_OK);
    *out = artifact;
    return 1;
}

static int build_cfg_module_artifact(uint8_t** out, size_t* out_length, int diamond) {
    static const uint8_t strings[] = "main";
    OirModuleType types[3];
    OirModuleFunction functions[1];
    OirModuleBlock blocks[4];
    OirModuleValue values[4];
    OirModuleInstruction instructions[8];
    OirModuleOperand operands[11];
    OirModuleInput input;
    OirArtifactHeader header;
    OirSectionInput section;
    uint8_t module_bytes[4096];
    uint8_t* artifact;
    uint32_t block_count = diamond ? 4u : 2u;
    uint32_t value_count = diamond ? 4u : 2u;
    uint32_t instruction_count = diamond ? 8u : 4u;
    uint32_t operand_count = diamond ? 11u : 5u;
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
    memset(values, 0, sizeof(values));
    memset(instructions, 0, sizeof(instructions));
    memset(operands, 0, sizeof(operands));

    if (!diamond) {
        blocks[0].id = 1u; blocks[0].function_id = 1u;
        blocks[0].first_instruction = 0u; blocks[0].instruction_count = 2u;
        blocks[1].id = 2u; blocks[1].function_id = 1u;
        blocks[1].first_instruction = 2u; blocks[1].instruction_count = 2u;

        values[0].id = 1u; values[0].type_id = 1u; values[0].defining_instruction_id = 1u;
        values[1].id = 2u; values[1].type_id = 1u; values[1].defining_instruction_id = 3u;

        instructions[0].id = 1u; instructions[0].block_id = 1u;
        instructions[0].opcode_id = OIR_OP_CONST_INT; instructions[0].result_value_id = 1u;
        instructions[0].first_operand = 0u; instructions[0].operand_count = 1u;
        instructions[1].id = 2u; instructions[1].block_id = 1u;
        instructions[1].opcode_id = OIR_OP_BRANCH;
        instructions[1].first_operand = 1u; instructions[1].operand_count = 1u;
        instructions[2].id = 3u; instructions[2].block_id = 2u;
        instructions[2].opcode_id = OIR_OP_ADD; instructions[2].result_value_id = 2u;
        instructions[2].first_operand = 2u; instructions[2].operand_count = 2u;
        instructions[3].id = 4u; instructions[3].block_id = 2u;
        instructions[3].opcode_id = OIR_OP_RETURN;
        instructions[3].first_operand = 4u; instructions[3].operand_count = 1u;

        operands[0].kind = OIR_OPERAND_INTEGER_IMMEDIATE; operands[0].immediate = 7u;
        operands[1].kind = OIR_OPERAND_BLOCK; operands[1].reference_id = 2u;
        operands[2].kind = OIR_OPERAND_VALUE; operands[2].reference_id = 1u;
        operands[3].kind = OIR_OPERAND_VALUE; operands[3].reference_id = 1u;
        operands[4].kind = OIR_OPERAND_VALUE; operands[4].reference_id = 2u;
    } else {
        uint32_t i;
        for (i = 0u; i < 4u; ++i) {
            blocks[i].id = i + 1u; blocks[i].function_id = 1u;
            blocks[i].first_instruction = i * 2u; blocks[i].instruction_count = 2u;
        }
        values[0].id = 1u; values[0].type_id = 2u; values[0].defining_instruction_id = 1u;
        values[1].id = 2u; values[1].type_id = 1u; values[1].defining_instruction_id = 3u;
        values[2].id = 3u; values[2].type_id = 1u; values[2].defining_instruction_id = 5u;
        values[3].id = 4u; values[3].type_id = 1u; values[3].defining_instruction_id = 7u;

        instructions[0].id = 1u; instructions[0].block_id = 1u;
        instructions[0].opcode_id = OIR_OP_CONST_BOOL; instructions[0].result_value_id = 1u;
        instructions[0].first_operand = 0u; instructions[0].operand_count = 1u;
        instructions[1].id = 2u; instructions[1].block_id = 1u;
        instructions[1].opcode_id = OIR_OP_COND_BRANCH;
        instructions[1].first_operand = 1u; instructions[1].operand_count = 3u;
        instructions[2].id = 3u; instructions[2].block_id = 2u;
        instructions[2].opcode_id = OIR_OP_CONST_INT; instructions[2].result_value_id = 2u;
        instructions[2].first_operand = 4u; instructions[2].operand_count = 1u;
        instructions[3].id = 4u; instructions[3].block_id = 2u;
        instructions[3].opcode_id = OIR_OP_BRANCH;
        instructions[3].first_operand = 5u; instructions[3].operand_count = 1u;
        instructions[4].id = 5u; instructions[4].block_id = 3u;
        instructions[4].opcode_id = OIR_OP_CONST_INT; instructions[4].result_value_id = 3u;
        instructions[4].first_operand = 6u; instructions[4].operand_count = 1u;
        instructions[5].id = 6u; instructions[5].block_id = 3u;
        instructions[5].opcode_id = OIR_OP_BRANCH;
        instructions[5].first_operand = 7u; instructions[5].operand_count = 1u;
        instructions[6].id = 7u; instructions[6].block_id = 4u;
        instructions[6].opcode_id = OIR_OP_ADD; instructions[6].result_value_id = 4u;
        instructions[6].first_operand = 8u; instructions[6].operand_count = 2u;
        instructions[7].id = 8u; instructions[7].block_id = 4u;
        instructions[7].opcode_id = OIR_OP_RETURN;
        instructions[7].first_operand = 10u; instructions[7].operand_count = 1u;

        operands[0].kind = OIR_OPERAND_INTEGER_IMMEDIATE; operands[0].immediate = 1u;
        operands[1].kind = OIR_OPERAND_VALUE; operands[1].reference_id = 1u;
        operands[2].kind = OIR_OPERAND_BLOCK; operands[2].reference_id = 2u;
        operands[3].kind = OIR_OPERAND_BLOCK; operands[3].reference_id = 3u;
        operands[4].kind = OIR_OPERAND_INTEGER_IMMEDIATE; operands[4].immediate = 7u;
        operands[5].kind = OIR_OPERAND_BLOCK; operands[5].reference_id = 4u;
        operands[6].kind = OIR_OPERAND_INTEGER_IMMEDIATE; operands[6].immediate = 9u;
        operands[7].kind = OIR_OPERAND_BLOCK; operands[7].reference_id = 4u;
        operands[8].kind = OIR_OPERAND_VALUE; operands[8].reference_id = 2u;
        operands[9].kind = OIR_OPERAND_VALUE; operands[9].reference_id = 3u;
        operands[10].kind = OIR_OPERAND_VALUE; operands[10].reference_id = 4u;
    }

    memset(&input, 0, sizeof(input));
    input.types = types; input.type_count = 3u;
    input.functions = functions; input.function_count = 1u;
    input.blocks = blocks; input.block_count = block_count;
    input.values = values; input.value_count = value_count;
    input.instructions = instructions; input.instruction_count = instruction_count;
    input.operands = operands; input.operand_count = operand_count;
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
    header.producer_stage_id = OCC_STAGE_OIR_LOWERING;
    header.validity = OIR_VALIDITY_VALID;
    header.job_id = diamond ? 504u : 502u;
    memset(&section, 0, sizeof(section));
    section.type = OIR_SECTION_PAYLOAD; section.version = 1u;
    section.flags = OIR_SECTION_FLAG_REQUIRED;
    section.data = module_bytes; section.length = module_length;
    assert(oir_artifact_write(artifact, artifact_capacity, &header,
        &section, 1u, out_length) == OIR_STATUS_OK);
    *out = artifact;
    return 1;
}

int main(int argc, char** argv) {
    OccLoadedStageModule loaded;
    OccDiagnosticCollector diagnostics;
    uint8_t *input = NULL, *output = NULL;
    size_t input_length = 0u, output_length = 0u;
    OirArtifactView artifact;
    char error[256];

    assert(argc == 2);
    occ_diagnostics_init(&diagnostics);
    assert(occ_module_loader_open(&loaded, argv[1], error, sizeof(error)) ==
        OCC_MODULE_LOADER_OK);
    assert(loaded.module->descriptor.module_id == 0x00010006u);
    assert(loaded.module->descriptor.stage_id == OCC_STAGE_OIR_VERIFY);
    assert(loaded.module->descriptor.output_artifact_kind == OIR_KIND_MODULE);

    assert(build_module_artifact(&input, &input_length, 0));
    {
        OccModuleArtifactInputV1 stage_input;
        memset(&stage_input, 0, sizeof(stage_input));
        stage_input.bytes = input; stage_input.length = input_length;
        assert(occ_module_loader_invoke(&loaded, &stage_input, 1u, 501u,
            &diagnostics, 256u, 1024u * 1024u, &output, &output_length) ==
            OCC_MODULE_LOADER_OK);
    }
    assert(oir_artifact_open(output, output_length, &artifact) == OIR_STATUS_OK);
    assert(artifact.header.artifact_kind == OIR_KIND_MODULE);
    assert(artifact.header.payload_schema_major == OIR_MODULE_SCHEMA_MAJOR);
    assert(artifact.header.payload_schema_minor == OIR_MODULE_SCHEMA_MINOR);
    assert(artifact.header.producer_stage_id == OCC_STAGE_OIR_VERIFY);
    assert(artifact.header.validity == OIR_VALIDITY_VALID);
    assert(artifact.header.job_id == 501u);
    free(output); output = NULL; output_length = 0u;
    free(input); input = NULL; input_length = 0u;

    /* The wire codec accepts the record shape, but the SSA use precedes its
       definition. The independent verifier must reject it. */
    assert(build_module_artifact(&input, &input_length, 1));
    {
        OccModuleArtifactInputV1 stage_input;
        memset(&stage_input, 0, sizeof(stage_input));
        stage_input.bytes = input; stage_input.length = input_length;
        assert(occ_module_loader_invoke(&loaded, &stage_input, 1u, 501u,
            &diagnostics, 256u, 1024u * 1024u, &output, &output_length) ==
            OCC_MODULE_LOADER_INVOKE_FAILED);
    }
    assert(output == NULL);
    assert(occ_diagnostics_count_severity(&diagnostics, OCC_DIAG_ERROR) >= 1u);
    free(input); input = NULL; input_length = 0u; output = NULL; output_length = 0u;

    /* A definition in a predecessor block must dominate its downstream use. */
    assert(build_cfg_module_artifact(&input, &input_length, 0));
    {
        OccModuleArtifactInputV1 stage_input;
        memset(&stage_input, 0, sizeof(stage_input));
        stage_input.bytes = input; stage_input.length = input_length;
        assert(occ_module_loader_invoke(&loaded, &stage_input, 1u, 502u,
            &diagnostics, 256u, 1024u * 1024u, &output, &output_length) ==
            OCC_MODULE_LOADER_OK);
    }
    assert(oir_artifact_open(output, output_length, &artifact) == OIR_STATUS_OK);
    assert(artifact.header.producer_stage_id == OCC_STAGE_OIR_VERIFY);
    free(input); input = NULL; input_length = 0u;
    free(output); output = NULL; output_length = 0u;

    /* Neither branch-local value dominates the join block. */
    assert(build_cfg_module_artifact(&input, &input_length, 1));
    {
        OccModuleArtifactInputV1 stage_input;
        memset(&stage_input, 0, sizeof(stage_input));
        stage_input.bytes = input; stage_input.length = input_length;
        assert(occ_module_loader_invoke(&loaded, &stage_input, 1u, 504u,
            &diagnostics, 256u, 1024u * 1024u, &output, &output_length) ==
            OCC_MODULE_LOADER_INVOKE_FAILED);
    }
    assert(output == NULL);
    assert(occ_diagnostics_count_severity(&diagnostics, OCC_DIAG_ERROR) >= 2u);

    free(input);
    occ_module_loader_close(&loaded);
    occ_diagnostics_destroy(&diagnostics);
    return 0;
}
