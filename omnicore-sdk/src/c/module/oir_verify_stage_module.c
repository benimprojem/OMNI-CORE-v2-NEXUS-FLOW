#include "occ_module_api.h"
#include "oir_module.h"

#include <stdlib.h>
#include <string.h>

#define OIR_VERIFY_MAX_BLOCKS 8192u
#define OIR_VERIFY_MAX_RECORDS 1000000u

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

static int validate_module(const OirModuleView* m) {
    uint32_t *block_function = NULL;
    uint32_t *definition_instruction = NULL, *definition_block = NULL;
    uint8_t *covered = NULL, *reachable = NULL;
    uint32_t *queue = NULL;
    uint64_t *pred = NULL, *dom = NULL;
    size_t words, matrix_words;
    uint32_t i, j, f;
    int ok = 0;

    if (!m || m->function_count == 0u || m->block_count == 0u ||
        m->instruction_count == 0u || m->block_count > OIR_VERIFY_MAX_BLOCKS ||
        m->function_count > OIR_VERIFY_MAX_RECORDS ||
        m->instruction_count > OIR_VERIFY_MAX_RECORDS ||
        m->value_count > OIR_VERIFY_MAX_RECORDS) return 0;

    words = ((size_t)m->block_count + 1u + 63u) / 64u;
    if (words == 0u || (size_t)m->block_count + 1u > SIZE_MAX / words) return 0;
    matrix_words = ((size_t)m->block_count + 1u) * words;
    if (matrix_words > SIZE_MAX / sizeof(uint64_t)) return 0;

    block_function = (uint32_t*)calloc((size_t)m->block_count + 1u, sizeof(uint32_t));
    definition_instruction = (uint32_t*)calloc((size_t)m->value_count + 1u, sizeof(uint32_t));
    definition_block = (uint32_t*)calloc((size_t)m->value_count + 1u, sizeof(uint32_t));
    covered = (uint8_t*)calloc((size_t)m->instruction_count + 1u, 1u);
    reachable = (uint8_t*)calloc((size_t)m->block_count + 1u, 1u);
    queue = (uint32_t*)calloc((size_t)m->block_count + 1u, sizeof(uint32_t));
    pred = (uint64_t*)calloc(matrix_words, sizeof(uint64_t));
    dom = (uint64_t*)calloc(matrix_words, sizeof(uint64_t));
    if (!block_function || !definition_instruction ||
        !definition_block || !covered || !reachable || !queue || !pred || !dom) goto done;

    /* Primitive type widths must match the published OIR type map. */
    for (i = 0u; i < m->type_count; ++i) {
        OirModuleType t;
        if (oir_module_get_type(m, i, &t) != OIR_STATUS_OK) goto done;
        if (t.kind == OIR_MODULE_TYPE_INTEGER) {
            if (t.bit_width != 8u && t.bit_width != 16u &&
                t.bit_width != 32u && t.bit_width != 64u) goto done;
        } else if (t.kind == OIR_MODULE_TYPE_FLOAT) {
            if (t.bit_width != 32u && t.bit_width != 64u) goto done;
        } else if (t.kind == OIR_MODULE_TYPE_BOOLEAN) {
            if (t.bit_width != 1u) goto done;
        } else if (t.kind != OIR_MODULE_TYPE_FUNCTION) {
            goto done;
        }
    }

    /* Establish unique block ownership and exact, non-overlapping instruction ranges. */
    for (i = 0u; i < m->block_count; ++i) {
        OirModuleBlock b;
        if (oir_module_get_block(m, i, &b) != OIR_STATUS_OK ||
            b.id != i + 1u || b.function_id == 0u || b.function_id > m->function_count ||
            b.instruction_count == 0u ||
            b.first_instruction > m->instruction_count ||
            b.instruction_count > m->instruction_count - b.first_instruction) goto done;
        block_function[b.id] = b.function_id;
        for (j = 0u; j < b.instruction_count; ++j) {
            uint32_t index = b.first_instruction + j;
            OirModuleInstruction ins;
            if (covered[index] ||
                oir_module_get_instruction(m, index, &ins) != OIR_STATUS_OK ||
                ins.id != index + 1u || ins.block_id != b.id) goto done;
            covered[index] = 1u;
        }
    }
    for (i = 1u; i <= m->instruction_count; ++i) if (!covered[i - 1u]) goto done;

    /* Functions must have a valid function type and an entry block they own. */
    for (f = 0u; f < m->function_count; ++f) {
        OirModuleFunction fn;
        OirModuleType ft;
        int owns_block = 0;
        if (oir_module_get_function(m, f, &fn) != OIR_STATUS_OK ||
            fn.id != f + 1u || !get_type(m, fn.function_type_id, &ft) ||
            ft.kind != OIR_MODULE_TYPE_FUNCTION || fn.entry_block_id == 0u ||
            fn.entry_block_id > m->block_count ||
            block_function[fn.entry_block_id] != fn.id) goto done;
        for (i = 1u; i <= m->block_count; ++i)
            if (block_function[i] == fn.id) owns_block = 1;
        if (!owns_block) goto done;
    }

    /* Every value has exactly one matching definition record. */
    for (i = 0u; i < m->value_count; ++i) {
        OirModuleValue v;
        OirModuleInstruction def;
        if (oir_module_get_value(m, i, &v) != OIR_STATUS_OK ||
            v.id != i + 1u || v.defining_instruction_id == 0u ||
            v.defining_instruction_id > m->instruction_count ||
            !get_type(m, v.type_id, &(OirModuleType){0}) ||
            oir_module_get_instruction(m, v.defining_instruction_id - 1u, &def) != OIR_STATUS_OK ||
            def.result_value_id != v.id) goto done;
        definition_instruction[v.id] = v.defining_instruction_id;
        definition_block[v.id] = def.block_id;
    }
    for (i = 0u; i < m->instruction_count; ++i) {
        OirModuleInstruction ins;
        OirModuleValue result;
        if (oir_module_get_instruction(m, i, &ins) != OIR_STATUS_OK) goto done;
        if (ins.result_value_id != 0u) {
            if (!get_value(m, ins.result_value_id, &result) ||
                result.defining_instruction_id != ins.id ||
                definition_instruction[ins.result_value_id] != ins.id) goto done;
        }
    }

    /* Validate opcode-specific types and collect CFG predecessor edges. */
    for (i = 0u; i < m->instruction_count; ++i) {
        OirModuleInstruction ins;
        OirModuleValue a, b, result;
        OirModuleType at, bt, rt;
        uint32_t target_a, target_b;
        const OirOpcodeDescriptor* desc;
        if (oir_module_get_instruction(m, i, &ins) != OIR_STATUS_OK ||
            ins.block_id == 0u || ins.block_id > m->block_count) goto done;
        desc = oir_opcode_lookup(ins.opcode_id);
        if (!desc) goto done;
        if (ins.result_value_id && (!get_value(m, ins.result_value_id, &result) ||
            !get_type(m, result.type_id, &rt))) goto done;
        if (!ins.result_value_id) memset(&rt, 0, sizeof(rt));

        switch (ins.opcode_id) {
            case OIR_OP_CONST_INT:
                if (!ins.result_value_id || ins.operand_count != 1u ||
                    rt.kind != OIR_MODULE_TYPE_INTEGER) goto done;
                break;
            case OIR_OP_CONST_FLOAT:
                if (!ins.result_value_id || ins.operand_count != 1u ||
                    rt.kind != OIR_MODULE_TYPE_FLOAT) goto done;
                break;
            case OIR_OP_CONST_BOOL: {
                OirModuleOperand immediate;
                if (!ins.result_value_id || ins.operand_count != 1u ||
                    rt.kind != OIR_MODULE_TYPE_BOOLEAN ||
                    !get_operand(m, &ins, 0u, &immediate) ||
                    immediate.kind != OIR_OPERAND_INTEGER_IMMEDIATE ||
                    immediate.immediate > 1u) goto done;
                break;
            }
            case OIR_OP_ADD: case OIR_OP_SUB: case OIR_OP_MUL: case OIR_OP_DIV:
            case OIR_OP_REM:
                if (!ins.result_value_id || !type_is_numeric(&rt) ||
                    !get_value_operand(m, &ins, 0u, &a) ||
                    !get_value_operand(m, &ins, 1u, &b) ||
                    a.type_id != result.type_id || b.type_id != result.type_id ||
                    (ins.opcode_id == OIR_OP_REM && rt.kind != OIR_MODULE_TYPE_INTEGER)) goto done;
                break;
            case OIR_OP_NEG:
                if (!ins.result_value_id || !type_is_numeric(&rt) ||
                    !get_value_operand(m, &ins, 0u, &a) ||
                    a.type_id != result.type_id) goto done;
                break;
            case OIR_OP_CMP_EQ: case OIR_OP_CMP_NE: case OIR_OP_CMP_LT:
            case OIR_OP_CMP_LE: case OIR_OP_CMP_GT: case OIR_OP_CMP_GE:
                if (!ins.result_value_id || rt.kind != OIR_MODULE_TYPE_BOOLEAN ||
                    !get_value_operand(m, &ins, 0u, &a) ||
                    !get_value_operand(m, &ins, 1u, &b) || a.type_id != b.type_id) goto done;
                break;
            case OIR_OP_CAST: {
                OirModuleOperand target_type;
                if (!ins.result_value_id || !get_value_operand(m, &ins, 0u, &a) ||
                    !get_operand(m, &ins, 1u, &target_type) ||
                    target_type.kind != OIR_OPERAND_TYPE ||
                    target_type.reference_id != result.type_id) goto done;
                break;
            }
            case OIR_OP_CALL: {
                OirModuleOperand callee;
                if (!get_operand(m, &ins, 0u, &callee) ||
                    callee.kind != OIR_OPERAND_FUNCTION || callee.reference_id == 0u ||
                    callee.reference_id > m->function_count) goto done;
                break;
            }
            case OIR_OP_LOAD:
                if (!ins.result_value_id || !get_value_operand(m, &ins, 0u, &a)) goto done;
                break;
            case OIR_OP_STORE:
                /* Pointer/value type compatibility is deferred until the
                   memory and pointer type model is part of the OIR schema. */
                if (ins.result_value_id || !get_value_operand(m, &ins, 0u, &a) ||
                    !get_value_operand(m, &ins, 1u, &b)) goto done;
                break;
            case OIR_OP_BRANCH:
                if (ins.result_value_id || !get_block_operand(m, &ins, 0u, &target_a) ||
                    block_function[target_a] != block_function[ins.block_id]) goto done;
                bit_set(pred + (size_t)target_a * words, ins.block_id);
                break;
            case OIR_OP_COND_BRANCH:
                if (ins.result_value_id || !get_value_operand(m, &ins, 0u, &a) ||
                    !get_type(m, a.type_id, &at) || at.kind != OIR_MODULE_TYPE_BOOLEAN ||
                    !get_block_operand(m, &ins, 1u, &target_a) ||
                    !get_block_operand(m, &ins, 2u, &target_b) ||
                    block_function[target_a] != block_function[ins.block_id] ||
                    block_function[target_b] != block_function[ins.block_id]) goto done;
                bit_set(pred + (size_t)target_a * words, ins.block_id);
                bit_set(pred + (size_t)target_b * words, ins.block_id);
                break;
            case OIR_OP_RETURN:
            case OIR_OP_UNREACHABLE:
                if (ins.result_value_id) goto done;
                break;
            default:
                goto done;
        }
        (void)bt;
    }

    /* A block has exactly one final terminator; no instruction may follow it. */
    for (i = 1u; i <= m->block_count; ++i) {
        OirModuleBlock b;
        OirModuleInstruction last;
        if (oir_module_get_block(m, i - 1u, &b) != OIR_STATUS_OK ||
            oir_module_get_instruction(m, b.first_instruction + b.instruction_count - 1u,
                &last) != OIR_STATUS_OK || !has_terminator(&last)) goto done;
        for (j = 0u; j + 1u < b.instruction_count; ++j) {
            OirModuleInstruction ins;
            if (oir_module_get_instruction(m, b.first_instruction + j, &ins) != OIR_STATUS_OK ||
                has_terminator(&ins)) goto done;
        }
    }

    /* Per-function reachability and iterative dominators for SSA use validation. */
    for (f = 0u; f < m->function_count; ++f) {
        OirModuleFunction fn;
        uint32_t head = 0u, tail = 0u, b;
        int changed = 1;
        if (oir_module_get_function(m, f, &fn) != OIR_STATUS_OK) goto done;
        memset(reachable, 0, (size_t)m->block_count + 1u);
        queue[tail++] = fn.entry_block_id;
        reachable[fn.entry_block_id] = 1u;
        while (head < tail) {
            uint32_t current = queue[head++];
            OirModuleBlock cb;
            OirModuleInstruction term;
            uint32_t targets[2], count = 0u, k;
            if (oir_module_get_block(m, current - 1u, &cb) != OIR_STATUS_OK ||
                oir_module_get_instruction(m,
                    cb.first_instruction + cb.instruction_count - 1u, &term) != OIR_STATUS_OK) goto done;
            if (term.opcode_id == OIR_OP_BRANCH) {
                if (!get_block_operand(m, &term, 0u, &targets[count++])) goto done;
            } else if (term.opcode_id == OIR_OP_COND_BRANCH) {
                if (!get_block_operand(m, &term, 1u, &targets[count++]) ||
                    !get_block_operand(m, &term, 2u, &targets[count++])) goto done;
            }
            for (k = 0u; k < count; ++k) {
                uint32_t t = targets[k];
                if (block_function[t] != fn.id) goto done;
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
                temp = (uint64_t*)malloc(words * sizeof(uint64_t));
                if (!temp) goto done;
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
                if (!have_pred) { free(temp); goto done; }
                bit_set(temp, b);
                if (memcmp(row, temp, words * sizeof(uint64_t)) != 0) {
                    memcpy(row, temp, words * sizeof(uint64_t));
                    changed = 1;
                }
                free(temp);
            }
        }
    }

    /* Validate every SSA use: local definitions precede uses; external-block
       definitions must dominate the use block. */
    for (i = 0u; i < m->instruction_count; ++i) {
        OirModuleInstruction ins;
        if (oir_module_get_instruction(m, i, &ins) != OIR_STATUS_OK) goto done;
        for (j = 0u; j < ins.operand_count; ++j) {
            OirModuleOperand op;
            if (!get_operand(m, &ins, j, &op)) goto done;
            if (op.kind == OIR_OPERAND_VALUE) {
                OirModuleValue v;
                uint32_t def_id, def_block;
                if (!get_value(m, op.reference_id, &v)) goto done;
                def_id = definition_instruction[v.id];
                def_block = definition_block[v.id];
                if (def_id == 0u || def_id >= ins.id) {
                    if (def_block == ins.block_id || def_id == 0u) goto done;
                }
                if (def_block != ins.block_id &&
                    !bit_get(dom + (size_t)ins.block_id * words, def_block)) goto done;
                if (def_block == ins.block_id && def_id >= ins.id) goto done;
            }
        }
    }

    ok = 1;
done:
    free(block_function);
    free(definition_instruction); free(definition_block);
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
    OccModuleResultV1* result, uint32_t code, const char* message) {
    OccModuleDiagnosticV1 d;
    result->failure_code = code;
    if (!invocation->diagnostic_sink.report) return OCC_MODULE_STATUS_FAILED;
    memset(&d, 0, sizeof(d));
    d.struct_size = (uint32_t)sizeof(d);
    d.code = code; d.message_id = code; d.severity = OCC_DIAG_ERROR;
    d.message = message; d.message_length = strlen(message);
    if (invocation->diagnostic_sink.report(invocation->diagnostic_sink.user_context, &d))
        result->diagnostics_count = 1u;
    return OCC_MODULE_STATUS_FAILED;
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
    uint32_t rc = OCC_MODULE_STATUS_FAILED;

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
            "OIR Verifier requires a valid OIR_MODULE 1.1 artifact from the lowering stage.");

    for (i = 0u; i < input.section_count; ++i) {
        OirSectionView section;
        if (oir_artifact_get_section(&input, i, &section) != OIR_STATUS_OK)
            return report_verify_error(invocation, result, 8602u,
                "OIR Verifier could not read an artifact section.");
        if (section.type == OIR_SECTION_PAYLOAD) {
            if (++payload_count != 1u || section.version != 1u ||
                section.length > (uint64_t)SIZE_MAX ||
                oir_module_open(section.data, (size_t)section.length, &module) != OIR_STATUS_OK ||
                !validate_module(&module))
                return report_verify_error(invocation, result, 8603u,
                    "OIR_MODULE failed structural, control-flow, type or SSA verification.");
        }
    }
    if (payload_count != 1u || !find_sections(&input, &sections))
        return report_verify_error(invocation, result, 8604u,
            "OIR_MODULE must contain exactly one required payload section.");

    for (i = 0u; i < input.section_count; ++i) {
        if (sections[i].length > SIZE_MAX - required - OIR_SECTION_HEADER_SIZE) {
            free(sections);
            return report_verify_error(invocation, result, 8605u,
                "Verified OIR artifact size overflowed the host size range.");
        }
        required += OIR_SECTION_HEADER_SIZE + (size_t)sections[i].length;
    }
    if (required > (size_t)invocation->output_capacity) {
        result->required_output_capacity = (uint64_t)required;
        free(sections);
        return OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER;
    }

    header = input.header;
    header.producer_stage_id = OCC_STAGE_OIR_VERIFY;
    header.validity = OIR_VALIDITY_VALID;
    status = oir_artifact_write(invocation->output_buffer,
        (size_t)invocation->output_capacity, &header, sections,
        input.section_count, &written);
    free(sections);
    if (status != OIR_STATUS_OK || written != required)
        return report_verify_error(invocation, result, 8606u,
            "The verified OIR_MODULE artifact could not be serialized.");

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
