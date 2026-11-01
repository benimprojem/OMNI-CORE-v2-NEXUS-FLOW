#include "oir_lowering.h"
#include "oir_type_map.h"

#include <string.h>

#define AST_PROGRAM 1u
#define AST_BLOCK 2u
#define AST_FUNC_DECL 4u
#define AST_LITERAL 13u
#define AST_RETURN 26u

#define TOKEN_TRUE 7u
#define TOKEN_FALSE 8u

static uint16_t rd16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t rd64(const uint8_t* p) {
    return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32);
}

static OirStatus get_field(
    const OirAstNodeTableView* ast, uint32_t node_id, uint16_t field_id,
    uint8_t expected_type, OirAstTlvField* out
) {
    OirStatus status;
    if (node_id == 0u || node_id > ast->node_count) return OIR_STATUS_INVALID_SECTION;
    status = oir_ast_node_table_get_field(ast, node_id - 1u, field_id, out);
    if (status != OIR_STATUS_OK) return OIR_STATUS_INVALID_SECTION;
    return out->value_type == expected_type ? OIR_STATUS_OK : OIR_STATUS_INVALID_SECTION;
}

static OirStatus get_node(
    const OirAstNodeTableView* ast, uint32_t node_id, OirAstNodeRecord* out
) {
    if (node_id == 0u || node_id > ast->node_count) return OIR_STATUS_INVALID_SECTION;
    return oir_ast_node_table_get(ast, node_id - 1u, out);
}

static OirStatus get_node_ref(
    const OirAstNodeTableView* ast, uint32_t owner, uint16_t field_id,
    uint32_t* out_id
) {
    OirAstTlvField field;
    OirStatus status = get_field(ast, owner, field_id, OIR_AST_VALUE_NODE_REF, &field);
    if (status != OIR_STATUS_OK) return status;
    *out_id = rd32(field.value);
    return OIR_STATUS_OK;
}

static OirStatus get_string_ref(
    const OirAstNodeTableView* ast, uint32_t owner, uint16_t field_id,
    const uint8_t** out_bytes, uint32_t* out_length
) {
    OirAstTlvField field;
    OirStatus status = get_field(ast, owner, field_id, OIR_AST_VALUE_STRING_REF, &field);
    if (status != OIR_STATUS_OK) return status;
    *out_length = rd32(field.value + 4);
    return oir_ast_node_table_get_string(
        ast, rd32(field.value), *out_length, out_bytes);
}

static OirStatus find_annotation(
    const OirSemanticAnnotationsView* view, uint32_t node_id,
    OirSemanticAnnotation* out
) {
    uint32_t lo = 0u, hi = view->record_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        OirSemanticAnnotation item;
        OirStatus status = oir_semantic_annotations_get(view, mid, &item);
        if (status != OIR_STATUS_OK) return status;
        if (item.node_id < node_id) lo = mid + 1u;
        else hi = mid;
    }
    if (lo >= view->record_count) return OIR_STATUS_INVALID_SECTION;
    if (oir_semantic_annotations_get(view, lo, out) != OIR_STATUS_OK ||
        out->node_id != node_id || out->resolved_type_id == 0u) {
        return OIR_STATUS_INVALID_SECTION;
    }
    return OIR_STATUS_OK;
}

static OirStatus find_semantic_type(
    const OirSemanticTypesView* view, uint32_t type_id,
    OirSemanticType* out_type, const uint8_t** out_name
) {
    uint32_t i;
    for (i = 0u; i < view->record_count; ++i) {
        OirSemanticType item;
        OirStatus status = oir_semantic_types_get(view, i, &item, out_name);
        if (status != OIR_STATUS_OK) return status;
        if (item.type_id == type_id) {
            *out_type = item;
            return OIR_STATUS_OK;
        }
    }
    return OIR_STATUS_INVALID_SECTION;
}

OirStatus oir_lower_semantic_ast_literal_return(
    const OirSemanticAstArtifactView* artifact,
    OirLoweringWorkspace* ws,
    uint8_t* output,
    size_t output_capacity,
    size_t* out_written
) {
    OirAstNodeRecord program, function, body, statement, literal;
    OirAstTlvField field;
    OirSemanticAnnotation annotation;
    OirSemanticType semantic_type;
    OirModuleInput module;
    const uint8_t *function_name = NULL, *declared_type = NULL, *semantic_name = NULL;
    uint32_t function_name_length = 0u, declared_type_length = 0u;
    uint32_t declaration_id = 0u, body_id = 0u, expression_id = 0u;
    uint32_t semantic_type_id = 0u, token_id = 0u, literal_kind = 0u;
    uint64_t immediate = 0u;
    OirStatus status;

    if (out_written) *out_written = 0u;
    if (!artifact || !ws || !output || !out_written ||
        !ws->types || !ws->functions || !ws->blocks || !ws->values ||
        !ws->instructions || !ws->operands || !ws->strings) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    if (ws->type_capacity < 2u || ws->function_capacity < 1u ||
        ws->block_capacity < 1u || ws->value_capacity < 1u ||
        ws->instruction_capacity < 2u || ws->operand_capacity < 2u ||
        ws->string_capacity == 0u) return OIR_STATUS_BUFFER_TOO_SMALL;

    status = get_node(&artifact->ast, artifact->ast.root_node_id, &program);
    if (status != OIR_STATUS_OK || program.node_kind_id != AST_PROGRAM)
        return OIR_STATUS_INVALID_SECTION;
    status = get_node_ref(&artifact->ast, program.node_id, 1u, &declaration_id);
    if (status != OIR_STATUS_OK || declaration_id == 0u)
        return OIR_STATUS_INVALID_SECTION;
    status = get_node(&artifact->ast, declaration_id, &function);
    if (status != OIR_STATUS_OK || function.node_kind_id != AST_FUNC_DECL ||
        function.next_node_id != 0u) return OIR_STATUS_INVALID_SECTION;

    status = get_field(&artifact->ast, function.node_id, 3u, OIR_AST_VALUE_BOOL, &field);
    if (status != OIR_STATUS_OK || field.value[0] != 0u) return OIR_STATUS_INVALID_SECTION;
    status = get_node_ref(&artifact->ast, function.node_id, 6u, &declaration_id);
    if (status != OIR_STATUS_OK || declaration_id != 0u) return OIR_STATUS_INVALID_SECTION;

    status = get_string_ref(&artifact->ast, function.node_id, 1u,
        &function_name, &function_name_length);
    if (status != OIR_STATUS_OK || function_name_length == 0u ||
        function_name_length > ws->string_capacity) return OIR_STATUS_INVALID_SECTION;
    status = get_string_ref(&artifact->ast, function.node_id, 2u,
        &declared_type, &declared_type_length);
    if (status != OIR_STATUS_OK || declared_type_length == 0u)
        return OIR_STATUS_INVALID_SECTION;

    status = get_node_ref(&artifact->ast, function.node_id, 7u, &body_id);
    if (status != OIR_STATUS_OK || body_id == 0u) return OIR_STATUS_INVALID_SECTION;
    status = get_node(&artifact->ast, body_id, &body);
    if (status != OIR_STATUS_OK || body.node_kind_id != AST_BLOCK) return OIR_STATUS_INVALID_SECTION;
    status = get_node_ref(&artifact->ast, body_id, 1u, &expression_id);
    if (status != OIR_STATUS_OK || expression_id == 0u) return OIR_STATUS_INVALID_SECTION;
    status = get_node(&artifact->ast, expression_id, &statement);
    if (status != OIR_STATUS_OK || statement.node_kind_id != AST_RETURN ||
        statement.next_node_id != 0u) return OIR_STATUS_INVALID_SECTION;
    status = get_node_ref(&artifact->ast, statement.node_id, 2u, &declaration_id);
    if (status != OIR_STATUS_OK || declaration_id == 0u) return OIR_STATUS_INVALID_SECTION;
    status = get_node(&artifact->ast, declaration_id, &literal);
    if (status != OIR_STATUS_OK || literal.node_kind_id != AST_LITERAL)
        return OIR_STATUS_INVALID_SECTION;

    status = get_field(&artifact->ast, literal.node_id, 1u, OIR_AST_VALUE_TOKEN_ID, &field);
    if (status != OIR_STATUS_OK) return status;
    token_id = rd32(field.value);
    status = get_field(&artifact->ast, literal.node_id, 2u, OIR_AST_VALUE_U32, &field);
    if (status != OIR_STATUS_OK) return status;
    literal_kind = rd32(field.value);

    status = find_annotation(&artifact->annotations, literal.node_id, &annotation);
    if (status != OIR_STATUS_OK) return status;
    semantic_type_id = annotation.resolved_type_id;
    status = find_semantic_type(&artifact->types, semantic_type_id,
        &semantic_type, &semantic_name);
    if (status != OIR_STATUS_OK || !semantic_name) return OIR_STATUS_INVALID_SECTION;
    if (declared_type_length != semantic_type.name_length ||
        memcmp(declared_type, semantic_name, declared_type_length) != 0)
        return OIR_STATUS_INVALID_SECTION;
    status = oir_type_map_primitive(&semantic_type, semantic_name,
        semantic_type.name_length, 1u, &ws->types[0]);
    if (status != OIR_STATUS_OK) return status;

    if (literal_kind == 0u && ws->types[0].kind == OIR_MODULE_TYPE_INTEGER) {
        status = get_field(&artifact->ast, literal.node_id, 3u, OIR_AST_VALUE_I64, &field);
        if (status != OIR_STATUS_OK) return status;
        immediate = rd64(field.value);
        ws->instructions[0].opcode_id = OIR_OP_CONST_INT;
        ws->operands[0].kind = OIR_OPERAND_INTEGER_IMMEDIATE;
    } else if (literal_kind == 1u && ws->types[0].kind == OIR_MODULE_TYPE_FLOAT) {
        status = get_field(&artifact->ast, literal.node_id, 4u, OIR_AST_VALUE_F64, &field);
        if (status != OIR_STATUS_OK) return status;
        immediate = rd64(field.value);
        ws->instructions[0].opcode_id = OIR_OP_CONST_FLOAT;
        ws->operands[0].kind = OIR_OPERAND_FLOAT_IMMEDIATE;
    } else if (literal_kind == 4u && ws->types[0].kind == OIR_MODULE_TYPE_BOOLEAN) {
        if (token_id != TOKEN_TRUE && token_id != TOKEN_FALSE)
            return OIR_STATUS_INVALID_SECTION;
        immediate = token_id == TOKEN_TRUE ? 1u : 0u;
        ws->instructions[0].opcode_id = OIR_OP_CONST_BOOL;
        ws->operands[0].kind = OIR_OPERAND_INTEGER_IMMEDIATE;
    } else {
        return OIR_STATUS_INVALID_SECTION;
    }

    memcpy(ws->strings, function_name, function_name_length);
    ws->types[1].id = 2u;
    ws->types[1].kind = OIR_MODULE_TYPE_FUNCTION;
    ws->types[1].bit_width = 0u;
    ws->types[1].flags = 0u;

    ws->functions[0].id = 1u;
    ws->functions[0].name_offset = 0u;
    ws->functions[0].name_length = function_name_length;
    ws->functions[0].function_type_id = 2u;
    ws->functions[0].entry_block_id = 1u;

    ws->blocks[0].id = 1u;
    ws->blocks[0].function_id = 1u;
    ws->blocks[0].first_instruction = 1u;
    ws->blocks[0].instruction_count = 2u;

    ws->values[0].id = 1u;
    ws->values[0].type_id = 1u;
    ws->values[0].defining_instruction_id = 1u;
    ws->values[0].flags = 0u;

    memset(&ws->instructions[0], 0, sizeof(ws->instructions[0]));
    ws->instructions[0].id = 1u;
    ws->instructions[0].block_id = 1u;
    ws->instructions[0].opcode_id = literal_kind == 0u ? OIR_OP_CONST_INT :
        (literal_kind == 1u ? OIR_OP_CONST_FLOAT : OIR_OP_CONST_BOOL);
    ws->instructions[0].result_value_id = 1u;
    ws->instructions[0].first_operand = 0u;
    ws->instructions[0].operand_count = 1u;

    ws->operands[0].kind = literal_kind == 1u
        ? OIR_OPERAND_FLOAT_IMMEDIATE : OIR_OPERAND_INTEGER_IMMEDIATE;
    ws->operands[0].reference_id = 0u;
    ws->operands[0].immediate = immediate;

    memset(&ws->instructions[1], 0, sizeof(ws->instructions[1]));
    ws->instructions[1].id = 2u;
    ws->instructions[1].block_id = 1u;
    ws->instructions[1].opcode_id = OIR_OP_RETURN;
    ws->instructions[1].result_value_id = 0u;
    ws->instructions[1].first_operand = 1u;
    ws->instructions[1].operand_count = 1u;

    ws->operands[1].kind = OIR_OPERAND_VALUE;
    ws->operands[1].reference_id = 1u;
    ws->operands[1].immediate = 0u;

    memset(&module, 0, sizeof(module));
    module.types = ws->types; module.type_count = 2u;
    module.functions = ws->functions; module.function_count = 1u;
    module.blocks = ws->blocks; module.block_count = 1u;
    module.values = ws->values; module.value_count = 1u;
    module.instructions = ws->instructions; module.instruction_count = 2u;
    module.operands = ws->operands; module.operand_count = 2u;
    module.strings = ws->strings; module.strings_length = function_name_length;
    return oir_module_write(output, output_capacity, &module, out_written);
}
