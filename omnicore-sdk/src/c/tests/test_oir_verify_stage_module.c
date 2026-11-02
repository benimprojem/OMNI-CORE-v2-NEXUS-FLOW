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
    static const uint8_t metadata[] = "verifier-payload-preservation";
    OirModuleType types[2];
    OirModuleFunction functions[1];
    OirModuleBlock blocks[1];
    OirModuleValue values[3];
    OirModuleInstruction instructions[4];
    OirModuleOperand operands[5];
    OirModuleInput input;
    OirArtifactHeader header;
    OirSectionInput sections[2];
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

    artifact_capacity = OIR_HEADER_SIZE + 2u * OIR_SECTION_HEADER_SIZE +
        module_length + sizeof(metadata) - 1u;
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
    memset(sections, 0, sizeof(sections));
    sections[0].type = OIR_SECTION_PAYLOAD; sections[0].version = 1u;
    sections[0].flags = OIR_SECTION_FLAG_REQUIRED;
    sections[0].data = module_bytes; sections[0].length = module_length;
    sections[1].type = OIR_SECTION_METADATA; sections[1].version = 1u;
    sections[1].data = metadata; sections[1].length = sizeof(metadata) - 1u;
    assert(oir_artifact_write(artifact, artifact_capacity, &header,
        sections, 2u, out_length) == OIR_STATUS_OK);
    *out = artifact;
    return 1;
}

static void test_put_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static int build_cfg_module_artifact(uint8_t** out, size_t* out_length, int variant) {
    int diamond = variant == 1;
    static const uint8_t strings[] = "mainother";
    OirModuleType types[3];
    OirModuleFunction functions[2];
    OirModuleBlock blocks[4];
    OirModuleValue values[4];
    OirModuleInstruction instructions[8];
    OirModuleOperand operands[11];
    OirModuleInput input;
    OirArtifactHeader header;
    OirSectionInput section;
    uint8_t module_bytes[4096];
    uint8_t* artifact;
    uint32_t block_count = variant == 5 ? 1u : (diamond ? 4u : 2u);
    uint32_t value_count = variant == 5 ? 2u : (diamond ? 4u : 2u);
    uint32_t instruction_count = variant == 5 ? 2u : (diamond ? 8u : 4u);
    uint32_t operand_count = variant == 5 ? 2u : (diamond ? 11u : 5u);
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
    if (variant == 2 || variant == 3) {
        functions[1].id = 2u; functions[1].name_offset = 4u;
        functions[1].name_length = 5u; functions[1].function_type_id = 3u;
        functions[1].entry_block_id = 2u;
    }

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

    if (variant == 2) {
        /* The branch is structurally valid, but its target belongs to function 2. */
        blocks[1].function_id = 2u;
    } else if (variant == 3) {
        /* Function 2 names block 2 as its entry, but block 2 belongs to function 1. */
        blocks[1].function_id = 1u;
    } else if (variant == 5) {
        /* A structurally decodable block with no terminator must be rejected. */
        blocks[0].id = 1u; blocks[0].function_id = 1u;
        blocks[0].first_instruction = 0u; blocks[0].instruction_count = 2u;
        values[0].id = 1u; values[0].type_id = 1u; values[0].defining_instruction_id = 1u;
        values[1].id = 2u; values[1].type_id = 1u; values[1].defining_instruction_id = 2u;
        instructions[0].id = 1u; instructions[0].block_id = 1u;
        instructions[0].opcode_id = OIR_OP_CONST_INT; instructions[0].result_value_id = 1u;
        instructions[0].first_operand = 0u; instructions[0].operand_count = 1u;
        instructions[1].id = 2u; instructions[1].block_id = 1u;
        instructions[1].opcode_id = OIR_OP_CONST_INT; instructions[1].result_value_id = 2u;
        instructions[1].first_operand = 1u; instructions[1].operand_count = 1u;
        operands[0].kind = OIR_OPERAND_INTEGER_IMMEDIATE; operands[0].immediate = 7u;
        operands[1].kind = OIR_OPERAND_INTEGER_IMMEDIATE; operands[1].immediate = 9u;
    }

    memset(&input, 0, sizeof(input));
    input.types = types; input.type_count = 3u;
    input.functions = functions; input.function_count =
        (variant == 2 || variant == 3) ? 2u : 1u;
    input.blocks = blocks; input.block_count = block_count;
    input.values = values; input.value_count = value_count;
    input.instructions = instructions; input.instruction_count = instruction_count;
    input.operands = operands; input.operand_count = operand_count;
    input.strings = strings; input.strings_length =
        (variant == 2 || variant == 3) ? 9u : 4u;
    assert(oir_module_write(module_bytes, sizeof(module_bytes), &input,
        &module_length) == OIR_STATUS_OK);

    /* Patch only after the codec writer has emitted a valid wire payload.
       These malformed semantic relationships belong to the independent verifier. */
    if (variant == 4) {
        size_t instructions_offset = 36u + 3u * 12u + 20u +
            (size_t)block_count * 16u + (size_t)value_count * 16u;
        test_put_u32(module_bytes + instructions_offset + 2u * 24u + 12u, 1u);
    } else if (variant == 6) {
        size_t instructions_offset = 36u + 3u * 12u + 20u +
            (size_t)block_count * 16u + (size_t)value_count * 16u;
        size_t operands_offset = instructions_offset +
            (size_t)instruction_count * 24u;
        /* The codec's shared opcode registry rejects this malformed wire shape. */
        test_put_u32(module_bytes + instructions_offset + 2u * 24u + 20u, 3u);
        test_put_u32(module_bytes + operands_offset + 4u * 16u,
            OIR_OPERAND_INTEGER_IMMEDIATE);
    } else if (variant == 7) {
        /* A nonzero but unsupported integer width passes the wire codec. */
        test_put_u32(module_bytes + 36u + 6u, 24u);
    } else if (variant == 8) {
        size_t blocks_offset = 36u + 3u * 12u + 20u;
        /* Block 2 overlaps block 1's final instruction range. */
        test_put_u32(module_bytes + blocks_offset + 16u + 8u, 1u);
    } else if (variant == 9) {
        size_t values_offset = 36u + 3u * 12u + 20u +
            (size_t)block_count * 16u;
        /* ADD's result is boolean rather than a numeric type. */
        test_put_u32(module_bytes + values_offset + 16u + 4u, 2u);
    }

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
    header.job_id = 502u + (uint32_t)variant;
    memset(&section, 0, sizeof(section));
    section.type = OIR_SECTION_PAYLOAD; section.version = 1u;
    section.flags = OIR_SECTION_FLAG_REQUIRED;
    section.data = module_bytes; section.length = module_length;
    assert(oir_artifact_write(artifact, artifact_capacity, &header,
        &section, 1u, out_length) == OIR_STATUS_OK);
    *out = artifact;
    return 1;
}

static void assert_last_diagnostic_code(const OccDiagnosticCollector* diagnostics,
    uint32_t expected_code, const char* expected_context) {
    OccDiagnosticView view;
    size_t count = occ_diagnostics_count(diagnostics);
    assert(count > 0u);
    assert(occ_diagnostics_get(diagnostics, count - 1u, &view));
    assert(view.record->code == expected_code);
    if (expected_context) assert(strstr(view.message, expected_context) != NULL);
}

int main(int argc, char** argv) {
    OccLoadedStageModule loaded;
    OccDiagnosticCollector diagnostics;
    uint8_t *input = NULL, *output = NULL;
    size_t input_length = 0u, output_length = 0u;
    OirArtifactView artifact, input_artifact;
    OirSectionView input_section, output_section;
    uint8_t* input_snapshot = NULL;
    char error[256];

    assert(argc == 2);
    occ_diagnostics_init(&diagnostics);
    assert(occ_module_loader_open(&loaded, argv[1], error, sizeof(error)) ==
        OCC_MODULE_LOADER_OK);
    assert(loaded.module->descriptor.module_id == 0x00010006u);
    assert(loaded.module->descriptor.stage_id == OCC_STAGE_OIR_VERIFY);
    assert(loaded.module->descriptor.output_artifact_kind == OIR_KIND_MODULE);

    assert(build_module_artifact(&input, &input_length, 0));
    input_snapshot = (uint8_t*)malloc(input_length);
    assert(input_snapshot != NULL);
    memcpy(input_snapshot, input, input_length);
    {
        OccModuleArtifactInputV1 stage_input;
        memset(&stage_input, 0, sizeof(stage_input));
        stage_input.bytes = input; stage_input.length = input_length;
        assert(occ_module_loader_invoke(&loaded, &stage_input, 1u, 501u,
            &diagnostics, 256u, 1024u * 1024u, &output, &output_length) ==
            OCC_MODULE_LOADER_OK);
    }
    assert(oir_artifact_open(input, input_length, &input_artifact) == OIR_STATUS_OK);
    assert(oir_artifact_open(output, output_length, &artifact) == OIR_STATUS_OK);
    assert(input_artifact.section_count == artifact.section_count);
    for (uint32_t section_index = 0u; section_index < input_artifact.section_count; ++section_index) {
        assert(oir_artifact_get_section(&input_artifact, section_index, &input_section) == OIR_STATUS_OK);
        assert(oir_artifact_get_section(&artifact, section_index, &output_section) == OIR_STATUS_OK);
        assert(input_section.type == output_section.type);
        assert(input_section.version == output_section.version);
        assert(input_section.flags == output_section.flags);
        assert(input_section.length == output_section.length);
        assert(memcmp(input_section.data, output_section.data,
            (size_t)input_section.length) == 0);
    }
    assert(memcmp(input, input_snapshot, input_length) == 0);
    free(input_snapshot); input_snapshot = NULL;
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
    assert_last_diagnostic_code(&diagnostics, 8620u, "value=2");
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
        assert(occ_module_loader_invoke(&loaded, &stage_input, 1u, 503u,
            &diagnostics, 256u, 1024u * 1024u, &output, &output_length) ==
            OCC_MODULE_LOADER_INVOKE_FAILED);
    }
    assert(output == NULL);
    assert(occ_diagnostics_count_severity(&diagnostics, OCC_DIAG_ERROR) >= 2u);
    assert_last_diagnostic_code(&diagnostics, 8620u, "value=2");
    free(input); input = NULL; input_length = 0u;

    /* Reject malformed CFG ownership, function entries, SSA definitions, terminators,
       opcode wire shapes, type records, overlapping block ranges and opcode types. */
    {
        static const uint32_t expected_codes[] = {
            0u, 0u, 8617u, 8613u, 8614u, 8618u, 8609u, 8610u, 8612u, 8616u
        };
        static const char* expected_contexts[] = {
            NULL, NULL, "block=1", "function=2", "value=2",
            "block=1", NULL, "type=1", "instruction=2", "instruction=3"
        };
        int variant;
        for (variant = 2; variant <= 9; ++variant) {
            OccModuleArtifactInputV1 stage_input;
            assert(build_cfg_module_artifact(&input, &input_length, variant));
            memset(&stage_input, 0, sizeof(stage_input));
            stage_input.bytes = input; stage_input.length = input_length;
            assert(occ_module_loader_invoke(&loaded, &stage_input, 1u,
                502u + (uint32_t)variant, &diagnostics, 256u,
                1024u * 1024u, &output, &output_length) ==
                OCC_MODULE_LOADER_INVOKE_FAILED);
            assert(output == NULL && output_length == 0u);
            assert_last_diagnostic_code(&diagnostics, expected_codes[variant], expected_contexts[variant]);
            free(input); input = NULL; input_length = 0u;
        }
    }

    free(input_snapshot);
    free(input);
    occ_module_loader_close(&loaded);
    occ_diagnostics_destroy(&diagnostics);
    return 0;
}
