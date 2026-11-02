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
    header.producer_stage_id = OCC_STAGE_OIR_LOWERING;
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
    occ_diagnostics_destroy(&diagnostics);
    occ_module_loader_close(&loaded);
    return 0;
}
