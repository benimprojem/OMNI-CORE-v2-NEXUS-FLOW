#include "occ_module_api.h"
#include "oir_module.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CODEGEN_MAX_VALUES 65536u
#define CODEGEN_ASSEMBLY_SCHEMA_MAJOR 1u
#define CODEGEN_ASSEMBLY_SCHEMA_MINOR 0u

typedef struct AssemblyWriter {
    char* bytes;
    size_t capacity;
    size_t length;
} AssemblyWriter;

static const OccStageModuleV1 g_codegen_module;

static int asm_emit(AssemblyWriter* writer, const char* format, ...) {
    va_list args;
    va_list count_args;
    int required;
    if (!writer || !format) return 0;
    va_start(args, format);
    va_copy(count_args, args);
    required = vsnprintf(NULL, 0u, format, count_args);
    va_end(count_args);
    if (required < 0 || (size_t)required > SIZE_MAX - writer->length) {
        va_end(args);
        return 0;
    }
    if (writer->bytes) {
        size_t remaining;
        if (writer->length >= writer->capacity) {
            va_end(args);
            return 0;
        }
        remaining = writer->capacity - writer->length;
        if ((size_t)required >= remaining ||
            vsnprintf(writer->bytes + writer->length, remaining, format, args) != required) {
            va_end(args);
            return 0;
        }
    }
    va_end(args);
    writer->length += (size_t)required;
    return 1;
}

static int valid_assembler_identifier(const uint8_t* name, uint32_t length) {
    uint32_t i;
    if (!name || length == 0u) return 0;
    if (!((name[0] >= 'A' && name[0] <= 'Z') ||
          (name[0] >= 'a' && name[0] <= 'z') || name[0] == '_')) return 0;
    for (i = 1u; i < length; ++i) {
        if (!((name[i] >= 'A' && name[i] <= 'Z') ||
              (name[i] >= 'a' && name[i] <= 'z') ||
              (name[i] >= '0' && name[i] <= '9') || name[i] == '_')) return 0;
    }
    return 1;
}

static int get_value(const OirModuleView* module, uint32_t id, OirModuleValue* out) {
    return id != 0u && id <= module->value_count &&
        oir_module_get_value(module, id - 1u, out) == OIR_STATUS_OK &&
        out->id == id;
}

static int get_type(const OirModuleView* module, uint32_t id, OirModuleType* out) {
    return id != 0u && id <= module->type_count &&
        oir_module_get_type(module, id - 1u, out) == OIR_STATUS_OK &&
        out->id == id;
}

static int value_operand(
    const OirModuleView* module,
    const OirModuleInstruction* instruction,
    uint32_t operand_index,
    OirModuleValue* out_value
) {
    OirModuleOperand operand;
    if (operand_index >= instruction->operand_count ||
        oir_module_get_operand(module,
            instruction->first_operand + operand_index, &operand) != OIR_STATUS_OK ||
        operand.kind != OIR_OPERAND_VALUE ||
        !get_value(module, operand.reference_id, out_value)) return 0;
    return 1;
}

static int immediate_operand(
    const OirModuleView* module,
    const OirModuleInstruction* instruction,
    uint32_t operand_index,
    OirModuleOperand* out_operand
) {
    if (operand_index >= instruction->operand_count ||
        oir_module_get_operand(module,
            instruction->first_operand + operand_index, out_operand) != OIR_STATUS_OK ||
        out_operand->kind != OIR_OPERAND_INTEGER_IMMEDIATE) return 0;
    return 1;
}

/* The first backend slice is deliberately limited to one straight-line
 * x86-64 bare-metal function with i32/i64/bool scalar operations. */
static int verify_codegen_module(
    const OirModuleView* module,
    OirModuleFunction* out_function,
    OirModuleType* out_return_type,
    const uint8_t** out_name
) {
    OirModuleBlock block;
    OirModuleType function_type;
    uint8_t* defined = NULL;
    uint32_t i;
    int valid = 0;

    if (!module || !out_function || !out_return_type || !out_name ||
        module->function_count != 1u || module->block_count != 1u ||
        module->instruction_count < 2u ||
        module->value_count == 0u || module->value_count > CODEGEN_MAX_VALUES ||
        module->type_count < 2u) return 0;

    if (oir_module_get_function(module, 0u, out_function) != OIR_STATUS_OK ||
        out_function->id != 1u || out_function->entry_block_id != 1u ||
        !get_type(module, out_function->function_type_id, &function_type) ||
        function_type.kind != OIR_MODULE_TYPE_FUNCTION ||
        !get_type(module, 1u, out_return_type) ||
        oir_module_get_block(module, 0u, &block) != OIR_STATUS_OK ||
        block.id != 1u || block.function_id != 1u ||
        block.first_instruction != 0u ||
        block.instruction_count != module->instruction_count ||
        out_function->name_length == 0u ||
        out_function->name_offset > module->strings_length ||
        out_function->name_length > module->strings_length - out_function->name_offset) {
        return 0;
    }

    *out_name = module->strings + out_function->name_offset;
    if (!valid_assembler_identifier(*out_name, out_function->name_length)) return 0;

    if (out_return_type->kind == OIR_MODULE_TYPE_INTEGER) {
        if (out_return_type->bit_width != 32u && out_return_type->bit_width != 64u) return 0;
    } else if (out_return_type->kind != OIR_MODULE_TYPE_BOOLEAN) {
        return 0;
    }

    defined = (uint8_t*)calloc((size_t)module->value_count + 1u, 1u);
    if (!defined) return 0;

    for (i = 0u; i < module->instruction_count; ++i) {
        OirModuleInstruction instruction;
        OirModuleValue result_value;
        OirModuleValue a, b;
        OirModuleOperand immediate;
        OirModuleType result_type;
        uint32_t j;

        if (oir_module_get_instruction(module, i, &instruction) != OIR_STATUS_OK ||
            instruction.id != i + 1u || instruction.block_id != 1u) goto cleanup;

        if (instruction.result_value_id != 0u) {
            if (!get_value(module, instruction.result_value_id, &result_value) ||
                result_value.defining_instruction_id != instruction.id ||
                defined[instruction.result_value_id] ||
                !get_type(module, result_value.type_id, &result_type)) goto cleanup;
            defined[instruction.result_value_id] = 1u;
        } else {
            memset(&result_value, 0, sizeof(result_value));
            memset(&result_type, 0, sizeof(result_type));
        }

        for (j = 0u; j < instruction.operand_count; ++j) {
            OirModuleOperand operand;
            if (oir_module_get_operand(module, instruction.first_operand + j, &operand) != OIR_STATUS_OK)
                goto cleanup;
            if (operand.kind == OIR_OPERAND_VALUE) {
                OirModuleValue used;
                if (!get_value(module, operand.reference_id, &used) ||
                    !defined[operand.reference_id] ||
                    used.defining_instruction_id >= instruction.id) goto cleanup;
            }
        }

        switch (instruction.opcode_id) {
            case OIR_OP_CONST_INT:
                if (instruction.operand_count != 1u ||
                    result_type.kind != OIR_MODULE_TYPE_INTEGER ||
                    (result_type.bit_width != 32u && result_type.bit_width != 64u) ||
                    !immediate_operand(module, &instruction, 0u, &immediate) ||
                    (result_type.bit_width == 32u && immediate.immediate > UINT32_MAX))
                    goto cleanup;
                break;
            case OIR_OP_CONST_BOOL:
                if (instruction.operand_count != 1u ||
                    result_type.kind != OIR_MODULE_TYPE_BOOLEAN ||
                    !immediate_operand(module, &instruction, 0u, &immediate) ||
                    immediate.immediate > 1u) goto cleanup;
                break;
            case OIR_OP_ADD:
            case OIR_OP_SUB:
            case OIR_OP_MUL:
            case OIR_OP_DIV:
            case OIR_OP_REM:
                if (instruction.operand_count != 2u ||
                    result_type.kind != OIR_MODULE_TYPE_INTEGER ||
                    (result_type.bit_width != 32u && result_type.bit_width != 64u) ||
                    !value_operand(module, &instruction, 0u, &a) ||
                    !value_operand(module, &instruction, 1u, &b) ||
                    a.type_id != result_value.type_id || b.type_id != result_value.type_id)
                    goto cleanup;
                break;
            case OIR_OP_NEG:
                if (instruction.operand_count != 1u ||
                    result_type.kind != OIR_MODULE_TYPE_INTEGER ||
                    (result_type.bit_width != 32u && result_type.bit_width != 64u) ||
                    !value_operand(module, &instruction, 0u, &a) ||
                    a.type_id != result_value.type_id) goto cleanup;
                break;
            case OIR_OP_RETURN:
                if (instruction.result_value_id != 0u || instruction.operand_count != 1u ||
                    i + 1u != module->instruction_count ||
                    !value_operand(module, &instruction, 0u, &a) ||
                    a.type_id != out_return_type->id) goto cleanup;
                break;
            default:
                goto cleanup;
        }
    }

    for (i = 1u; i <= module->value_count; ++i) {
        if (!defined[i]) goto cleanup;
    }
    valid = 1;

cleanup:
    free(defined);
    return valid;
}

static uint32_t value_offset(uint32_t value_id) {
    return value_id * 8u;
}

static int emit_load_value(
    AssemblyWriter* writer,
    const OirModuleValue* value
) {
    uint32_t offset;
    if (!writer || !value || value->id == 0u) return 0;
    offset = value_offset(value->id);
    if (value->type_id == 0u) return 0;
    return asm_emit(writer, "    mov eax, dword [rbp - %u]\n", offset);
}

static int emit_instruction(
    AssemblyWriter* writer,
    const OirModuleView* module,
    const OirModuleInstruction* instruction
) {
    OirModuleValue result_value, a, b;
    OirModuleType type;
    OirModuleOperand immediate;
    uint32_t offset;
    int is_64;

    if (instruction->opcode_id == OIR_OP_RETURN) {
        if (!value_operand(module, instruction, 0u, &a)) return 0;
        if (!get_type(module, a.type_id, &type)) return 0;
        offset = value_offset(a.id);
        if (type.kind == OIR_MODULE_TYPE_BOOLEAN || type.bit_width == 32u) {
            return asm_emit(writer,
                "    mov eax, dword [rbp - %u]\n"
                "    mov rsp, rbp\n"
                "    pop rbp\n"
                "    ret\n", offset);
        }
        return asm_emit(writer,
            "    mov rax, qword [rbp - %u]\n"
            "    mov rsp, rbp\n"
            "    pop rbp\n"
            "    ret\n", offset);
    }

    if (!get_value(module, instruction->result_value_id, &result_value) ||
        !get_type(module, result_value.type_id, &type)) return 0;
    offset = value_offset(result_value.id);
    is_64 = type.bit_width == 64u;

    switch (instruction->opcode_id) {
        case OIR_OP_CONST_INT:
        case OIR_OP_CONST_BOOL:
            if (!immediate_operand(module, instruction, 0u, &immediate)) return 0;
            if (is_64) {
                return asm_emit(writer,
                    "    mov rax, 0x%016llX\n"
                    "    mov qword [rbp - %u], rax\n",
                    (unsigned long long)immediate.immediate, offset);
            }
            return asm_emit(writer,
                "    mov eax, 0x%08X\n"
                "    mov dword [rbp - %u], eax\n",
                (unsigned int)immediate.immediate, offset);

        case OIR_OP_ADD:
        case OIR_OP_SUB:
        case OIR_OP_MUL:
        case OIR_OP_DIV:
        case OIR_OP_REM:
            if (!value_operand(module, instruction, 0u, &a) ||
                !value_operand(module, instruction, 1u, &b)) return 0;
            if (is_64) {
                if (!asm_emit(writer,
                    "    mov rax, qword [rbp - %u]\n"
                    "    mov rbx, qword [rbp - %u]\n",
                    value_offset(a.id), value_offset(b.id))) return 0;
            } else {
                if (!asm_emit(writer,
                    "    mov eax, dword [rbp - %u]\n"
                    "    mov ebx, dword [rbp - %u]\n",
                    value_offset(a.id), value_offset(b.id))) return 0;
            }
            if (instruction->opcode_id == OIR_OP_ADD) {
                if (!asm_emit(writer, is_64 ? "    add rax, rbx\n" : "    add eax, ebx\n")) return 0;
            } else if (instruction->opcode_id == OIR_OP_SUB) {
                if (!asm_emit(writer, is_64 ? "    sub rax, rbx\n" : "    sub eax, ebx\n")) return 0;
            } else if (instruction->opcode_id == OIR_OP_MUL) {
                if (!asm_emit(writer, is_64 ? "    imul rax, rbx\n" : "    imul eax, ebx\n")) return 0;
            } else {
                int is_signed = (type.flags & OIR_MODULE_TYPE_FLAG_SIGNED) != 0u;
                if (is_64) {
                    if (!asm_emit(writer, is_signed
                        ? "    cqo\n    idiv rbx\n"
                        : "    xor edx, edx\n    div rbx\n")) return 0;
                } else {
                    if (!asm_emit(writer, is_signed
                        ? "    cdq\n    idiv ebx\n"
                        : "    xor edx, edx\n    div ebx\n")) return 0;
                }
                if (instruction->opcode_id == OIR_OP_REM) {
                    if (!asm_emit(writer, is_64
                        ? "    mov rax, rdx\n"
                        : "    mov eax, edx\n")) return 0;
                }
            }
            return asm_emit(writer, is_64
                ? "    mov qword [rbp - %u], rax\n"
                : "    mov dword [rbp - %u], eax\n", offset);

        case OIR_OP_NEG:
            if (!value_operand(module, instruction, 0u, &a)) return 0;
            if (is_64) {
                if (!asm_emit(writer, "    mov rax, qword [rbp - %u]\n    neg rax\n", value_offset(a.id))) return 0;
                return asm_emit(writer, "    mov qword [rbp - %u], rax\n", offset);
            }
            if (!asm_emit(writer, "    mov eax, dword [rbp - %u]\n    neg eax\n", value_offset(a.id))) return 0;
            return asm_emit(writer, "    mov dword [rbp - %u], eax\n", offset);

        default:
            return 0;
    }
}

static int emit_module_assembly(
    AssemblyWriter* writer,
    const OirModuleView* module,
    const OirModuleFunction* function,
    const OirModuleType* return_type,
    const uint8_t* function_name
) {
    uint32_t i;
    uint32_t frame_size = module->value_count * 8u;
    if (frame_size == 0u || frame_size > UINT32_MAX - 15u) return 0;
    frame_size = (frame_size + 15u) & ~15u;

    if (!asm_emit(writer,
        "; OCC x86-64 bare-metal backend from OIR_MODULE 1.1\n"
        "[BITS 64]\n"
        "section .text\n"
        "global _kernel_main\n"
        "global %.*s\n\n"
        "_kernel_main:\n"
        "    call %.*s\n"
        "    hlt\n\n"
        "%.*s:\n"
        "    push rbp\n"
        "    mov rbp, rsp\n"
        "    sub rsp, %u\n",
        (int)function->name_length, (const char*)function_name,
        (int)function->name_length, (const char*)function_name,
        (int)function->name_length, (const char*)function_name,
        frame_size)) return 0;

    (void)return_type;
    for (i = 0u; i < module->instruction_count; ++i) {
        OirModuleInstruction instruction;
        if (oir_module_get_instruction(module, i, &instruction) != OIR_STATUS_OK ||
            !emit_instruction(writer, module, &instruction)) return 0;
    }
    return 1;
}

static uint32_t report_codegen_error(
    const OccModuleInvocationV1* invocation,
    OccModuleResultV1* result,
    uint32_t code,
    const char* message
) {
    OccModuleDiagnosticV1 diagnostic;
    result->failure_code = code;
    if (!invocation->diagnostic_sink.report) return OCC_MODULE_STATUS_FAILED;
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.struct_size = (uint32_t)sizeof(diagnostic);
    diagnostic.code = code;
    diagnostic.message_id = code;
    diagnostic.severity = OCC_DIAG_ERROR;
    diagnostic.message = message;
    diagnostic.message_length = strlen(message);
    if (!invocation->diagnostic_sink.report(
        invocation->diagnostic_sink.user_context, &diagnostic))
        return OCC_MODULE_STATUS_FAILED;
    result->diagnostics_count = 1u;
    return OCC_MODULE_STATUS_FAILED;
}

static uint32_t OCC_MODULE_CALL codegen_stage_process(
    const OccModuleInvocationV1* invocation,
    OccModuleResultV1* result
) {
    OirArtifactView input_artifact;
    OirSectionView input_payload;
    OirModuleView module;
    OirModuleFunction function;
    OirModuleType return_type;
    const uint8_t* function_name = NULL;
    AssemblyWriter counter, writer;
    uint8_t* assembly = NULL;
    OirArtifactHeader output_header;
    OirSectionInput output_section;
    size_t required, written = 0u;
    uint32_t i;
    int found_payload = 0;
    OirStatus status;

    result->bytes_written = 0u;
    result->required_output_capacity = 0u;
    result->output_validity = OIR_VALIDITY_NONE;
    result->failure_code = 0u;
    result->diagnostics_count = 0u;

    if (occ_module_validate_invocation_v1(
        &g_codegen_module.descriptor, invocation, result) != OCC_MODULE_STATUS_OK ||
        invocation->input_count != 1u || !invocation->inputs ||
        !invocation->inputs[0].bytes ||
        invocation->inputs[0].length > (uint64_t)SIZE_MAX ||
        invocation->output_capacity > (uint64_t)SIZE_MAX) {
        return OCC_MODULE_STATUS_INVALID_INVOCATION;
    }

    status = oir_artifact_open(invocation->inputs[0].bytes,
        (size_t)invocation->inputs[0].length, &input_artifact);
    if (status != OIR_STATUS_OK ||
        input_artifact.header.artifact_kind != OIR_KIND_MODULE ||
        input_artifact.header.payload_schema_major != OIR_MODULE_SCHEMA_MAJOR ||
        input_artifact.header.payload_schema_minor != OIR_MODULE_SCHEMA_MINOR ||
        input_artifact.header.producer_stage_id != OCC_STAGE_OIR_LOWERING ||
        input_artifact.header.validity != OIR_VALIDITY_VALID ||
        input_artifact.header.job_id != invocation->job_id) {
        return report_codegen_error(invocation, result, 8501u,
            "Codegen received an invalid or mismatched OIR_MODULE artifact.");
    }

    memset(&input_payload, 0, sizeof(input_payload));
    for (i = 0u; i < input_artifact.section_count; ++i) {
        OirSectionView section;
        if (oir_artifact_get_section(&input_artifact, i, &section) != OIR_STATUS_OK)
            return report_codegen_error(invocation, result, 8502u,
                "Codegen could not read an OIR_MODULE section.");
        if (section.type == OIR_SECTION_PAYLOAD) {
            if (found_payload || section.version != 1u) {
                return report_codegen_error(invocation, result, 8503u,
                    "Codegen input has a duplicate or unsupported payload section.");
            }
            input_payload = section;
            found_payload = 1;
        }
    }
    if (!found_payload || !input_payload.data ||
        input_payload.length > (uint64_t)SIZE_MAX ||
        oir_module_open(input_payload.data, (size_t)input_payload.length, &module) != OIR_STATUS_OK ||
        !verify_codegen_module(&module, &function, &return_type, &function_name)) {
        return report_codegen_error(invocation, result, 8504u,
            "OIR_MODULE is structurally valid but outside the current x86-64 scalar backend subset.");
    }

    memset(&counter, 0, sizeof(counter));
    if (!emit_module_assembly(&counter, &module, &function, &return_type, function_name) ||
        counter.length == 0u ||
        counter.length > SIZE_MAX - OIR_HEADER_SIZE - OIR_SECTION_HEADER_SIZE) {
        return report_codegen_error(invocation, result, 8505u,
            "The current OIR_MODULE could not be lowered to x86-64 assembly.");
    }
    required = OIR_HEADER_SIZE + OIR_SECTION_HEADER_SIZE + counter.length;
    if (required > (size_t)invocation->output_capacity) {
        result->required_output_capacity = (uint64_t)required;
        return OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER;
    }

    assembly = (uint8_t*)malloc(counter.length + 1u);
    if (!assembly) {
        return report_codegen_error(invocation, result, 8506u,
            "Memory allocation failed while generating assembly.");
    }
    memset(&writer, 0, sizeof(writer));
    writer.bytes = (char*)assembly;
    writer.capacity = counter.length + 1u;
    if (!emit_module_assembly(&writer, &module, &function, &return_type, function_name) ||
        writer.length != counter.length) {
        free(assembly);
        return report_codegen_error(invocation, result, 8507u,
            "The assembly writer produced inconsistent output.");
    }

    memset(&output_header, 0, sizeof(output_header));
    output_header.container_major = OIR_CONTAINER_MAJOR;
    output_header.container_minor = OIR_CONTAINER_MINOR;
    output_header.artifact_kind = OIR_KIND_ASSEMBLY;
    output_header.payload_schema_major = CODEGEN_ASSEMBLY_SCHEMA_MAJOR;
    output_header.payload_schema_minor = CODEGEN_ASSEMBLY_SCHEMA_MINOR;
    output_header.producer_stage_id = OCC_STAGE_CODEGEN;
    output_header.validity = OIR_VALIDITY_VALID;
    output_header.job_id = invocation->job_id;

    memset(&output_section, 0, sizeof(output_section));
    output_section.type = OIR_SECTION_PAYLOAD;
    output_section.version = 1u;
    output_section.flags = OIR_SECTION_FLAG_REQUIRED;
    output_section.data = assembly;
    output_section.length = writer.length;

    status = oir_artifact_write(invocation->output_buffer,
        (size_t)invocation->output_capacity, &output_header,
        &output_section, 1u, &written);
    free(assembly);
    if (status != OIR_STATUS_OK || written != required) {
        return report_codegen_error(invocation, result, 8508u,
            "The OIR_ASSEMBLY artifact could not be serialized.");
    }

    result->bytes_written = (uint64_t)written;
    result->output_validity = OIR_VALIDITY_VALID;
    result->required_output_capacity = 0u;
    return OCC_MODULE_STATUS_OK;
}

static const OccStageModuleV1 g_codegen_module = {
    (uint32_t)sizeof(OccStageModuleV1), 0u,
    {
        (uint32_t)sizeof(OccStageDescriptorV1),
        OCC_MODULE_ABI_MAJOR, OCC_MODULE_ABI_MINOR,
        0x00010005u, OCC_STAGE_CODEGEN,
        1u, 0u, 0u, 0u,
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_MODULE),
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_MODULE),
        OIR_KIND_ASSEMBLY, 1u, 1u, 0u,
        (uint64_t)OCC_MODULE_CAP_REENTRANT |
            (uint64_t)OCC_MODULE_CAP_DETERMINISTIC |
            (uint64_t)OCC_MODULE_CAP_STATELESS |
            (uint64_t)OCC_MODULE_CAP_DIAGNOSTIC_REPORTING,
        "occ-codegen-x86_64-nasm-baremetal"
    },
    codegen_stage_process
};

OCC_MODULE_EXPORT const OccStageModuleV1* OCC_MODULE_CALL
occ_get_stage_module_v1(void) {
    return &g_codegen_module;
}
