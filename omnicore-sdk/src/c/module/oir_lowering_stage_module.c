#include "occ_module_api.h"
#include "oir_lowering.h"
#include "oir_semantic_ast_artifact.h"
#include "oir_ast_node_table.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static const OccStageModuleV1 g_oir_lowering_module;

static OirStatus find_payload(
    const OirArtifactView* artifact, uint16_t type, OirSectionView* out
) {
    uint32_t i;
    int found = 0;
    if (!artifact || !out) return OIR_STATUS_INVALID_ARGUMENT;
    for (i = 0u; i < artifact->section_count; ++i) {
        OirSectionView section;
        OirStatus status = oir_artifact_get_section(artifact, i, &section);
        if (status != OIR_STATUS_OK) return status;
        if (section.type == type) {
            if (found) return OIR_STATUS_DUPLICATE_PAYLOAD;
            *out = section;
            found = 1;
        }
    }
    return found ? OIR_STATUS_OK : OIR_STATUS_MISSING_PAYLOAD;
}

static int add_array_bytes(size_t* total, size_t count, size_t element_size) {
    if (!total || (count != 0u && element_size > SIZE_MAX / count)) return 0;
    if (count * element_size > SIZE_MAX - *total) return 0;
    *total += count * element_size;
    return 1;
}

static int report_lowering_error(
    const OccModuleInvocationV1* invocation,
    OccModuleResultV1* result,
    uint32_t code,
    const char* message
) {
    OccModuleDiagnosticV1 diagnostic;
    if (!invocation || !result || !message) return 0;
    result->failure_code = code;
    if (!invocation->diagnostic_sink.report) return 1;

    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.struct_size = (uint32_t)sizeof(diagnostic);
    diagnostic.code = code;
    diagnostic.message_id = code;
    diagnostic.severity = OCC_DIAG_ERROR;
    diagnostic.message = message;
    diagnostic.message_length = strlen(message);
    if (!invocation->diagnostic_sink.report(
        invocation->diagnostic_sink.user_context, &diagnostic)) return 0;
    result->diagnostics_count = 1u;
    return 1;
}

static uint32_t OCC_MODULE_CALL oir_lowering_process(
    const OccModuleInvocationV1* invocation,
    OccModuleResultV1* result
) {
    OirArtifactView envelope;
    OirSectionView ast_section;
    OirAstNodeTableView ast_table;
    OirSemanticAstArtifactView semantic_artifact;
    OirLoweringWorkspace workspace;
    OirArtifactHeader output_header;
    OirSectionInput output_section;
    OirModuleView module_view;
    uint32_t *incoming = NULL, *owner_scope = NULL;
    uint8_t *visited = NULL, *module_bytes = NULL;
    OirAstGraphStackEntry *graph_stack = NULL;
    size_t node_count, values_capacity, instruction_capacity;
    size_t operand_capacity, string_capacity, module_capacity = OIR_MODULE_HEADER_SIZE;
    size_t artifact_required, module_written = 0u, artifact_written = 0u;
    OirStatus status;
    uint32_t module_status = OCC_MODULE_STATUS_FAILED;

    memset(&workspace, 0, sizeof(workspace));
    memset(&semantic_artifact, 0, sizeof(semantic_artifact));
    result->bytes_written = 0u;
    result->required_output_capacity = 0u;
    result->output_validity = OIR_VALIDITY_NONE;
    result->failure_code = 0u;
    result->diagnostics_count = 0u;

    if (occ_module_validate_invocation_v1(
            &g_oir_lowering_module.descriptor, invocation, result) !=
            OCC_MODULE_STATUS_OK ||
        invocation->input_count != 1u ||
        invocation->inputs[0].length > (uint64_t)SIZE_MAX ||
        invocation->output_capacity > (uint64_t)SIZE_MAX) {
        return OCC_MODULE_STATUS_INVALID_INVOCATION;
    }

    status = oir_artifact_open(
        invocation->inputs[0].bytes,
        (size_t)invocation->inputs[0].length,
        &envelope);
    if (status != OIR_STATUS_OK ||
        envelope.header.artifact_kind != OIR_KIND_SEMANTIC_AST ||
        envelope.header.payload_schema_major != OIR_SEMANTIC_AST_SCHEMA_MAJOR ||
        envelope.header.payload_schema_minor != OIR_SEMANTIC_AST_SCHEMA_MINOR ||
        envelope.header.producer_stage_id != OCC_STAGE_SEMANTIC ||
        envelope.header.validity != OIR_VALIDITY_VALID ||
        envelope.header.job_id != invocation->job_id ||
        find_payload(&envelope, OIR_SECTION_PAYLOAD, &ast_section) != OIR_STATUS_OK ||
        ast_section.length > (uint64_t)SIZE_MAX ||
        oir_ast_node_table_open(
            ast_section.data, (size_t)ast_section.length, &ast_table) != OIR_STATUS_OK ||
        ast_table.node_count == 0u ||
        ast_table.node_count > OIR_AST_NODE_COUNT_MAX) {
        return OCC_MODULE_STATUS_INVALID_INVOCATION;
    }

    node_count = (size_t)ast_table.node_count;
    if (node_count == SIZE_MAX) return OCC_MODULE_STATUS_INVALID_INVOCATION;

    incoming = (uint32_t*)calloc(node_count, sizeof(*incoming));
    visited = (uint8_t*)calloc(node_count, sizeof(*visited));
    graph_stack = (OirAstGraphStackEntry*)calloc(node_count, sizeof(*graph_stack));
    owner_scope = (uint32_t*)calloc(node_count + 1u, sizeof(*owner_scope));
    if (!incoming || !visited || !graph_stack || !owner_scope) goto allocation_failure;

    status = oir_semantic_ast_artifact_open(
        invocation->inputs[0].bytes,
        (size_t)invocation->inputs[0].length,
        &semantic_artifact,
        incoming, node_count,
        visited, node_count,
        graph_stack, node_count,
        owner_scope, node_count + 1u);
    if (status != OIR_STATUS_OK) return_invalid:
        {
            module_status = OCC_MODULE_STATUS_INVALID_INVOCATION;
            goto cleanup;
        }

    values_capacity = node_count;
    if (values_capacity > OIR_MODULE_TABLE_COUNT_MAX)
        values_capacity = OIR_MODULE_TABLE_COUNT_MAX;
    instruction_capacity = node_count + 1u;
    if (instruction_capacity > OIR_MODULE_TABLE_COUNT_MAX)
        instruction_capacity = OIR_MODULE_TABLE_COUNT_MAX;
    operand_capacity = node_count > (OIR_MODULE_TABLE_COUNT_MAX - 1u) / 2u
        ? OIR_MODULE_TABLE_COUNT_MAX : node_count * 2u + 1u;
    string_capacity = ast_table.string_table_length;
    if (string_capacity == 0u) string_capacity = 1u;

    workspace.type_capacity = 2u;
    workspace.function_capacity = 1u;
    workspace.block_capacity = 1u;
    workspace.value_capacity = (uint32_t)values_capacity;
    workspace.instruction_capacity = (uint32_t)instruction_capacity;
    workspace.operand_capacity = (uint32_t)operand_capacity;
    workspace.string_capacity = (uint32_t)string_capacity;
    workspace.local_capacity = (uint32_t)node_count;

    workspace.types = (OirModuleType*)calloc(workspace.type_capacity, sizeof(*workspace.types));
    workspace.functions = (OirModuleFunction*)calloc(workspace.function_capacity, sizeof(*workspace.functions));
    workspace.blocks = (OirModuleBlock*)calloc(workspace.block_capacity, sizeof(*workspace.blocks));
    workspace.values = (OirModuleValue*)calloc(workspace.value_capacity, sizeof(*workspace.values));
    workspace.instructions = (OirModuleInstruction*)calloc(workspace.instruction_capacity, sizeof(*workspace.instructions));
    workspace.operands = (OirModuleOperand*)calloc(workspace.operand_capacity, sizeof(*workspace.operands));
    workspace.strings = (uint8_t*)malloc(string_capacity);
    workspace.locals = (OirLoweringLocal*)calloc(workspace.local_capacity, sizeof(*workspace.locals));
    if (!workspace.types || !workspace.functions || !workspace.blocks ||
        !workspace.values || !workspace.instructions || !workspace.operands ||
        !workspace.strings || !workspace.locals) goto allocation_failure;

    if (!add_array_bytes(&module_capacity, workspace.type_capacity, 12u) ||
        !add_array_bytes(&module_capacity, workspace.function_capacity, 20u) ||
        !add_array_bytes(&module_capacity, workspace.block_capacity, 16u) ||
        !add_array_bytes(&module_capacity, workspace.value_capacity, 16u) ||
        !add_array_bytes(&module_capacity, workspace.instruction_capacity, 24u) ||
        !add_array_bytes(&module_capacity, workspace.operand_capacity, 16u) ||
        !add_array_bytes(&module_capacity, string_capacity, 1u)) {
        module_status = OCC_MODULE_STATUS_FAILED;
        (void)report_lowering_error(invocation, result, 8405u,
            "OIR lowering workspace size overflow.");
        goto cleanup;
    }
    module_bytes = (uint8_t*)malloc(module_capacity);
    if (!module_bytes) goto allocation_failure;

    status = oir_lower_semantic_ast_return(
        &semantic_artifact, &workspace, module_bytes, module_capacity, &module_written);
    if (status != OIR_STATUS_OK || module_written == 0u) {
        module_status = OCC_MODULE_STATUS_FAILED;
        if (status == OIR_STATUS_BUFFER_TOO_SMALL) {
            (void)report_lowering_error(invocation, result, 8403u,
                "The current scalar lowering workspace limit was exceeded.");
        } else if (status == OIR_STATUS_OUT_OF_MEMORY) {
            (void)report_lowering_error(invocation, result, 8404u,
                "Memory allocation failed while lowering the semantic artifact.");
        } else {
            (void)report_lowering_error(invocation, result, 8402u,
                "The semantic artifact is valid, but this construct is outside the current scalar lowering subset.");
        }
        goto cleanup;
    }

    status = oir_module_open(module_bytes, module_written, &module_view);
    if (status != OIR_STATUS_OK) {
        module_status = OCC_MODULE_STATUS_FAILED;
        (void)report_lowering_error(invocation, result, 8406u,
            "The generated OIR_MODULE payload failed validation.");
        goto cleanup;
    }

    if (module_written > SIZE_MAX - OIR_HEADER_SIZE - OIR_SECTION_HEADER_SIZE) {
        module_status = OCC_MODULE_STATUS_FAILED;
        (void)report_lowering_error(invocation, result, 8405u,
            "OIR_MODULE artifact size overflow.");
        goto cleanup;
    }
    artifact_required = OIR_HEADER_SIZE + OIR_SECTION_HEADER_SIZE + module_written;
    result->required_output_capacity = (uint64_t)artifact_required;
    if (invocation->output_capacity < (uint64_t)artifact_required) {
        module_status = OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER;
        goto cleanup;
    }

    memset(&output_header, 0, sizeof(output_header));
    output_header.container_major = OIR_CONTAINER_MAJOR;
    output_header.container_minor = OIR_CONTAINER_MINOR;
    output_header.artifact_kind = OIR_KIND_MODULE;
    output_header.payload_schema_major = OIR_MODULE_SCHEMA_MAJOR;
    output_header.payload_schema_minor = OIR_MODULE_SCHEMA_MINOR;
    output_header.producer_stage_id = OCC_STAGE_OIR_LOWERING;
    output_header.validity = OIR_VALIDITY_VALID;
    output_header.job_id = invocation->job_id;

    memset(&output_section, 0, sizeof(output_section));
    output_section.type = OIR_SECTION_PAYLOAD;
    output_section.version = 1u;
    output_section.flags = OIR_SECTION_FLAG_REQUIRED;
    output_section.data = module_bytes;
    output_section.length = (uint64_t)module_written;
    status = oir_artifact_write(
        invocation->output_buffer,
        (size_t)invocation->output_capacity,
        &output_header, &output_section, 1u, &artifact_written);
    if (status != OIR_STATUS_OK || artifact_written != artifact_required) {
        module_status = OCC_MODULE_STATUS_FAILED;
        (void)report_lowering_error(invocation, result, 8406u,
            "OIR_MODULE artifact serialization failed.");
        goto cleanup;
    }

    result->bytes_written = (uint64_t)artifact_written;
    result->required_output_capacity = 0u;
    result->output_validity = OIR_VALIDITY_VALID;
    result->failure_code = 0u;
    module_status = OCC_MODULE_STATUS_OK;
    goto cleanup;

allocation_failure:
    module_status = OCC_MODULE_STATUS_FAILED;
    (void)report_lowering_error(invocation, result, 8404u,
        "Memory allocation failed in the OIR lowering stage.");

cleanup:
    free(incoming);
    free(visited);
    free(graph_stack);
    free(owner_scope);
    free(workspace.types);
    free(workspace.functions);
    free(workspace.blocks);
    free(workspace.values);
    free(workspace.instructions);
    free(workspace.operands);
    free(workspace.strings);
    free(workspace.locals);
    free(module_bytes);
    return module_status;
}

static const OccStageModuleV1 g_oir_lowering_module = {
    (uint32_t)sizeof(OccStageModuleV1), 0u,
    {
        (uint32_t)sizeof(OccStageDescriptorV1),
        OCC_MODULE_ABI_MAJOR, OCC_MODULE_ABI_MINOR,
        0x00010004u, OCC_STAGE_OIR_LOWERING,
        1u, 0u, 0u, 0u,
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_SEMANTIC_AST),
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_SEMANTIC_AST),
        OIR_KIND_MODULE, 1u, 1u, 0u,
        (uint64_t)OCC_MODULE_CAP_REENTRANT |
            (uint64_t)OCC_MODULE_CAP_DETERMINISTIC |
            (uint64_t)OCC_MODULE_CAP_STATELESS |
            (uint64_t)OCC_MODULE_CAP_DIAGNOSTIC_REPORTING,
        "occ-oir-lowering"
    },
    oir_lowering_process
};

OCC_MODULE_EXPORT const OccStageModuleV1* OCC_MODULE_CALL
occ_get_stage_module_v1(void) {
    return &g_oir_lowering_module;
}
