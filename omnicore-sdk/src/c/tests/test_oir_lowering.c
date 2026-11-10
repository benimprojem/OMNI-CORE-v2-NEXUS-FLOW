#ifdef NDEBUG
#undef NDEBUG
#endif
#include "oir_lowering.h"
#include "occ_module_loader.h"
#include "oir_ast_tlv.h"
#include "parser.h"
#include "ast.h"
#include "semantic.h"
#include "oir_ast_frontend_writer.h"
#include "oir_ast_frontend_index.h"
#include "oir_semantic_ast_artifact.h"
#include "oir_ast_graph.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put16(uint8_t* p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t* p, uint32_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
static void put64(uint8_t* p, uint64_t v) {
    put32(p,(uint32_t)v); put32(p+4,(uint32_t)(v>>32));
}
static void write_field(OirAstTlvWriter* w, uint16_t id, uint8_t type,
    const uint8_t* bytes, uint32_t length) {
    assert(oir_ast_tlv_write(w,id,type,0u,bytes,length)==OIR_STATUS_OK);
}

typedef struct TestResolver {
    const OirAstPointerIndexEntry* entries;
    size_t capacity;
} TestResolver;

static uint32_t test_resolve_node_id(void* user_context, const ASTNode* node) {
    const TestResolver* resolver = (const TestResolver*)user_context;
    return oir_ast_frontend_index_lookup(resolver->entries, resolver->capacity, node);
}

static int test_lowering_stage_module(
    const OccLoadedStageModule* loaded,
    const uint8_t* semantic_bytes,
    size_t semantic_length,
    uint32_t expected_type_count,
    uint32_t expected_block_count
) {
    OccModuleArtifactInputV1 input;
    OccDiagnosticCollector diagnostics;
    uint8_t* output = NULL;
    size_t output_length = 0u;
    OirArtifactView envelope;
    OirSectionView payload;
    OirModuleView module;
    OirModuleFunction function;
    uint32_t i;
    int found_payload = 0;
    int ok = 0;

    if (!loaded || !semantic_bytes || semantic_length == 0u) return 0;
    memset(&input, 0, sizeof(input));
    input.bytes = semantic_bytes;
    input.length = (uint64_t)semantic_length;
    occ_diagnostics_init(&diagnostics);

    /* 64 bytes intentionally forces the loader's NEED_OUTPUT_BUFFER retry path. */
    if (occ_module_loader_invoke(loaded, &input, 1u, 1u, &diagnostics,
            64u, 16u * 1024u * 1024u, &output, &output_length) !=
            OCC_MODULE_LOADER_OK ||
        !output || occ_diagnostics_count(&diagnostics) != 0u ||
        oir_artifact_open(output, output_length, &envelope) != OIR_STATUS_OK ||
        envelope.header.artifact_kind != OIR_KIND_MODULE ||
        envelope.header.payload_schema_major != OIR_MODULE_SCHEMA_MAJOR ||
        envelope.header.payload_schema_minor != OIR_MODULE_SCHEMA_MINOR ||
        envelope.header.producer_stage_id != OCC_STAGE_OIR_LOWERING ||
        envelope.header.validity != OIR_VALIDITY_VALID ||
        envelope.header.job_id != 1u) goto cleanup;

    for (i = 0u; i < envelope.section_count; ++i) {
        OirSectionView section;
        if (oir_artifact_get_section(&envelope, i, &section) != OIR_STATUS_OK)
            goto cleanup;
        if (section.type == OIR_SECTION_PAYLOAD) {
            if (found_payload) goto cleanup;
            payload = section;
            found_payload = 1;
        }
    }
    if (!found_payload ||
        oir_module_open(payload.data, (size_t)payload.length, &module) != OIR_STATUS_OK ||
        module.type_count != expected_type_count || module.function_count != 1u ||
        module.block_count != expected_block_count || module.instruction_count < 2u ||
        module.value_count == 0u ||
        oir_module_get_function(&module, 0u, &function) != OIR_STATUS_OK ||
        function.id != 1u || function.entry_block_id != 1u) goto cleanup;
    ok = 1;

cleanup:
    free(output);
    occ_diagnostics_destroy(&diagnostics);
    return ok;
}

static int test_lowering_stage_rejects_bad_input(
    const OccLoadedStageModule* loaded
) {
    static const uint8_t invalid_bytes[] = { 'N', 'O', 'T', 'O', 'I', 'R' };
    OccModuleArtifactInputV1 input;
    OccDiagnosticCollector diagnostics;
    uint8_t* output = NULL;
    size_t output_length = 0u;
    uint32_t status;

    memset(&input, 0, sizeof(input));
    input.bytes = invalid_bytes;
    input.length = sizeof(invalid_bytes);
    occ_diagnostics_init(&diagnostics);
    status = occ_module_loader_invoke(loaded, &input, 1u, 1u, &diagnostics,
        64u, 16u * 1024u * 1024u, &output, &output_length);
    free(output);
    occ_diagnostics_destroy(&diagnostics);
    /* Any loader rejection is valid here; the contract is that malformed
     * input never yields a published output artifact. */
    return status != OCC_MODULE_LOADER_OK &&
        output == NULL && output_length == 0u;
}

static int test_real_frontend_to_module(const OccLoadedStageModule* stage_module, const uint8_t* source, size_t source_length, int arithmetic) {
    ParserContext* parser = NULL;
    SemanticContext semantic;
    OirAstPointerIndexEntry index[64];
    OirAstNodeRecord records[64];
    uint8_t fields[4096], strings[1024], ast_payload[8192];
    size_t ast_length = 0u;
    uint32_t node_count = 0u;
    OirAstFrontendRequirements requirements;
    TestResolver resolver;
    uint8_t semantic_artifact[32768];
    size_t semantic_length = 0u;
    OirSemanticAstArtifactView artifact;
    uint32_t incoming[64], owner_scope[65];
    uint8_t visited[64];
    OirAstGraphStackEntry graph_stack[64];
    OirLoweringWorkspace workspace;
    OirModuleType module_types[3];
    OirModuleFunction functions[1];
    OirModuleBlock blocks[16];
    OirModuleValue values[16];
    OirLoweringLocal locals[8];
    OirModuleInstruction instructions[32];
    OirModuleOperand operands[64];
    uint8_t module_strings[64], module_bytes[4096];
    size_t module_length = 0u;
    OirModuleView module;
    int ok = 0;
    int semantic_initialized = 0;

    memset(&semantic, 0, sizeof(semantic));
    memset(&workspace, 0, sizeof(workspace));
    memset(&artifact, 0, sizeof(artifact));
    memset(&module, 0, sizeof(module));
    parser = parser_init_source("real-lowering.nxf", source, source_length);
    if (!parser || parser_run(parser) != PARSER_STATUS_OK ||
        parser_get_status(parser) != PARSER_STATUS_OK || !parser->ast_root)
        goto cleanup;

    if (oir_ast_frontend_index_build((ASTNode*)parser->ast_root,
            index, 64u, &node_count) != OIR_STATUS_OK ||
        oir_ast_frontend_measure((ASTNode*)parser->ast_root,
            index, 64u, node_count, &requirements) != OIR_STATUS_OK ||
        requirements.field_data_length > sizeof(fields) ||
        requirements.string_table_length > sizeof(strings) ||
        requirements.payload_size > sizeof(ast_payload))
        goto cleanup;
    if (oir_ast_frontend_write_payload((ASTNode*)parser->ast_root,
            index, 64u, node_count, records, 64u,
            fields, sizeof(fields), strings, sizeof(strings),
            ast_payload, sizeof(ast_payload), &ast_length, &requirements) != OIR_STATUS_OK)
        goto cleanup;

    resolver.entries = index;
    resolver.capacity = 64u;
    semantic_init(&semantic, "real-lowering.nxf", NULL);
    semantic_initialized = 1;
    semantic_attach_node_id_resolver(&semantic, test_resolve_node_id, &resolver);
    semantic_check(&semantic, (ASTNode*)parser->ast_root);
    if (semantic.error_count != 0) {
        if (arithmetic == 7)
            fprintf(stderr, "bitwise semantic errors=%d\\n", semantic.error_count);
        goto cleanup;
    }
    if (semantic_finalize_result(&semantic, node_count) != SEMANTIC_RESULT_OK ||
        semantic.result.finalized != 1u) goto cleanup;
    if (oir_semantic_ast_artifact_write(ast_payload, ast_length,
            &semantic.result, 1u, semantic_artifact,
            sizeof(semantic_artifact), &semantic_length) != OIR_STATUS_OK)
        goto cleanup;
    if (oir_semantic_ast_artifact_open(semantic_artifact, semantic_length,
            &artifact, incoming, 64u, visited, 64u, graph_stack, 64u,
            owner_scope, 65u) != OIR_STATUS_OK)
        goto cleanup;

    workspace.types = module_types; workspace.type_capacity = 3u;
    workspace.functions = functions; workspace.function_capacity = 1u;
    workspace.blocks = blocks; workspace.block_capacity = 16u;
    workspace.values = values; workspace.value_capacity = 16u;
    workspace.locals = locals; workspace.local_capacity = 8u;
    workspace.instructions = instructions; workspace.instruction_capacity = 32u;
    workspace.operands = operands; workspace.operand_capacity = 64u;
    workspace.strings = module_strings; workspace.string_capacity = sizeof(module_strings);
    {
        OirStatus lower_status = oir_lower_semantic_ast_return(&artifact, &workspace,
            module_bytes, sizeof(module_bytes), &module_length);
        if (lower_status != OIR_STATUS_OK) {
            if (arithmetic == 7) fprintf(stderr, "bitwise lower status=%u\\n", (unsigned)lower_status);
            goto cleanup;
        }
    }
    if (oir_module_open(module_bytes, module_length, &module) != OIR_STATUS_OK)
        goto cleanup;
    if (module.type_count != ((arithmetic == 5 || arithmetic == 6) ? 3u : 2u) ||
        module.function_count != 1u) goto cleanup;
    {
        OirModuleType return_type;
        if (oir_module_get_type(&module, 0u, &return_type) != OIR_STATUS_OK ||
            return_type.kind != OIR_MODULE_TYPE_INTEGER ||
            (return_type.flags & OIR_MODULE_TYPE_FLAG_HARD) == 0u) goto cleanup;
    }
    if (arithmetic == 0) {
        OirModuleInstruction constant;
        OirModuleOperand immediate;
        if (module.instruction_count != 2u || module.operand_count != 2u ||
            module.value_count != 1u ||
            oir_module_get_instruction(&module, 0u, &constant) != OIR_STATUS_OK ||
            constant.opcode_id != OIR_OP_CONST_INT ||
            oir_module_get_operand(&module, 0u, &immediate) != OIR_STATUS_OK ||
            immediate.kind != OIR_OPERAND_INTEGER_IMMEDIATE ||
            immediate.immediate != 42u) goto cleanup;
    } else if (arithmetic == 1) {
        OirModuleInstruction mul, add, ret;
        OirModuleOperand left, right;
        if (module.instruction_count != 6u || module.operand_count != 8u ||
            module.value_count != 5u ||
            oir_module_get_instruction(&module, 3u, &mul) != OIR_STATUS_OK ||
            oir_module_get_instruction(&module, 4u, &add) != OIR_STATUS_OK ||
            oir_module_get_instruction(&module, 5u, &ret) != OIR_STATUS_OK ||
            mul.opcode_id != OIR_OP_MUL || add.opcode_id != OIR_OP_ADD ||
            ret.opcode_id != OIR_OP_RETURN ||
            oir_module_get_operand(&module, mul.first_operand, &left) != OIR_STATUS_OK ||
            oir_module_get_operand(&module, mul.first_operand + 1u, &right) != OIR_STATUS_OK ||
            left.reference_id != 2u || right.reference_id != 3u ||
            oir_module_get_operand(&module, add.first_operand, &left) != OIR_STATUS_OK ||
            oir_module_get_operand(&module, add.first_operand + 1u, &right) != OIR_STATUS_OK ||
            left.reference_id != 1u || right.reference_id != 4u) goto cleanup;
    } else if (arithmetic == 2) {
        OirModuleInstruction constant, ret;
        OirModuleOperand immediate, returned;
        if (module.instruction_count != 2u || module.operand_count != 2u ||
            module.value_count != 1u ||
            oir_module_get_instruction(&module, 0u, &constant) != OIR_STATUS_OK ||
            oir_module_get_instruction(&module, 1u, &ret) != OIR_STATUS_OK ||
            constant.opcode_id != OIR_OP_CONST_INT || ret.opcode_id != OIR_OP_RETURN ||
            oir_module_get_operand(&module, constant.first_operand, &immediate) != OIR_STATUS_OK ||
            immediate.kind != OIR_OPERAND_INTEGER_IMMEDIATE || immediate.immediate != 42u ||
            oir_module_get_operand(&module, ret.first_operand, &returned) != OIR_STATUS_OK ||
            returned.kind != OIR_OPERAND_VALUE || returned.reference_id != 1u)
            goto cleanup;
    } else if (arithmetic == 3) {
        OirModuleInstruction add, ret;
        OirModuleOperand left, right, returned, immediate;
        if (module.instruction_count != 4u || module.operand_count != 5u ||
            module.value_count != 3u ||
            oir_module_get_instruction(&module, 2u, &add) != OIR_STATUS_OK ||
            oir_module_get_instruction(&module, 3u, &ret) != OIR_STATUS_OK ||
            add.opcode_id != OIR_OP_ADD || ret.opcode_id != OIR_OP_RETURN ||
            oir_module_get_operand(&module, 1u, &immediate) != OIR_STATUS_OK ||
            immediate.kind != OIR_OPERAND_INTEGER_IMMEDIATE || immediate.immediate != 2u ||
            oir_module_get_operand(&module, add.first_operand, &left) != OIR_STATUS_OK ||
            oir_module_get_operand(&module, add.first_operand + 1u, &right) != OIR_STATUS_OK ||
            left.kind != OIR_OPERAND_VALUE || left.reference_id != 1u ||
            right.kind != OIR_OPERAND_VALUE || right.reference_id != 2u ||
            oir_module_get_operand(&module, ret.first_operand, &returned) != OIR_STATUS_OK ||
            returned.kind != OIR_OPERAND_VALUE || returned.reference_id != 3u)
            goto cleanup;
    } else if (arithmetic == 5) {
        OirModuleInstruction compare, branch;
        OirModuleOperand target_true, target_false;
        OirModuleFunction function;
        if (oir_module_get_instruction(&module, 2u, &compare) != OIR_STATUS_OK ||
            oir_module_get_instruction(&module, 3u, &branch) != OIR_STATUS_OK ||
            oir_module_get_operand(&module, branch.first_operand + 1u, &target_true) != OIR_STATUS_OK ||
            oir_module_get_operand(&module, branch.first_operand + 2u, &target_false) != OIR_STATUS_OK ||
            oir_module_get_function(&module, 0u, &function) != OIR_STATUS_OK)
            goto cleanup;
        if (module.block_count != 3u || module.instruction_count != 7u ||
            module.operand_count != 10u || module.value_count != 4u ||
            compare.opcode_id != OIR_OP_CMP_LT ||
            branch.opcode_id != OIR_OP_COND_BRANCH ||
            target_true.kind != OIR_OPERAND_BLOCK || target_true.reference_id != 2u ||
            target_false.kind != OIR_OPERAND_BLOCK || target_false.reference_id != 3u ||
            function.function_type_id != 3u) {
            fprintf(stderr, "if fixture: blocks=%u ins=%u operands=%u values=%u cmp=%u branch=%u true=%u/%u false=%u/%u fn_type=%u\\n",
                module.block_count, module.instruction_count, module.operand_count,
                module.value_count, compare.opcode_id, branch.opcode_id,
                target_true.kind, target_true.reference_id,
                target_false.kind, target_false.reference_id, function.function_type_id);
            goto cleanup;
        }
    } else if (arithmetic == 6) {
        /* if/elsif/els lowers to a nested, five-block CFG. */
        if (module.block_count != 5u || module.instruction_count != 12u ||
            module.operand_count != 18u || module.value_count != 7u)
            goto cleanup;
    } else if (arithmetic == 7 || arithmetic == 8) {
        OirModuleInstruction bit_op, ret;
        OirModuleOperand left, right, returned;
        uint16_t expected_opcode = arithmetic == 7 ? OIR_OP_BIT_AND : OIR_OP_BIT_SHL;
        if (module.instruction_count != 4u || module.operand_count != 5u ||
            module.value_count != 3u ||
            oir_module_get_instruction(&module, 2u, &bit_op) != OIR_STATUS_OK ||
            oir_module_get_instruction(&module, 3u, &ret) != OIR_STATUS_OK ||
            bit_op.opcode_id != expected_opcode || ret.opcode_id != OIR_OP_RETURN ||
            oir_module_get_operand(&module, bit_op.first_operand, &left) != OIR_STATUS_OK ||
            oir_module_get_operand(&module, bit_op.first_operand + 1u, &right) != OIR_STATUS_OK ||
            left.kind != OIR_OPERAND_VALUE || left.reference_id != 1u ||
            right.kind != OIR_OPERAND_VALUE || right.reference_id != 2u ||
            oir_module_get_operand(&module, ret.first_operand, &returned) != OIR_STATUS_OK ||
            returned.kind != OIR_OPERAND_VALUE || returned.reference_id != 3u)
            goto cleanup;
    } else {
        OirModuleInstruction add, mul, sub, ret;
        OirModuleOperand left, right, returned, immediate;
        if (module.instruction_count != 9u || module.operand_count != 12u ||
            module.value_count != 8u ||
            oir_module_get_instruction(&module, 3u, &add) != OIR_STATUS_OK ||
            oir_module_get_instruction(&module, 5u, &mul) != OIR_STATUS_OK ||
            oir_module_get_instruction(&module, 7u, &sub) != OIR_STATUS_OK ||
            oir_module_get_instruction(&module, 8u, &ret) != OIR_STATUS_OK ||
            add.opcode_id != OIR_OP_ADD || mul.opcode_id != OIR_OP_MUL ||
            sub.opcode_id != OIR_OP_SUB || ret.opcode_id != OIR_OP_RETURN)
            goto cleanup;
        if (oir_module_get_operand(&module,add.first_operand,&left)!=OIR_STATUS_OK ||
            oir_module_get_operand(&module,add.first_operand+1u,&right)!=OIR_STATUS_OK ||
            left.kind!=OIR_OPERAND_VALUE || left.reference_id!=1u ||
            right.kind!=OIR_OPERAND_VALUE || right.reference_id!=3u ||
            oir_module_get_operand(&module,mul.first_operand,&left)!=OIR_STATUS_OK ||
            oir_module_get_operand(&module,mul.first_operand+1u,&right)!=OIR_STATUS_OK ||
            left.kind!=OIR_OPERAND_VALUE || left.reference_id!=4u ||
            right.kind!=OIR_OPERAND_VALUE || right.reference_id!=5u ||
            oir_module_get_operand(&module,sub.first_operand,&left)!=OIR_STATUS_OK ||
            oir_module_get_operand(&module,sub.first_operand+1u,&right)!=OIR_STATUS_OK ||
            left.kind!=OIR_OPERAND_VALUE || left.reference_id!=6u ||
            right.kind!=OIR_OPERAND_VALUE || right.reference_id!=7u ||
            oir_module_get_operand(&module,ret.first_operand,&returned)!=OIR_STATUS_OK ||
            returned.kind!=OIR_OPERAND_VALUE || returned.reference_id!=8u)
            goto cleanup;
        if (oir_module_get_operand(&module,0u,&immediate)!=OIR_STATUS_OK ||
            immediate.kind!=OIR_OPERAND_INTEGER_IMMEDIATE || immediate.immediate!=5u ||
            oir_module_get_operand(&module,1u,&immediate)!=OIR_STATUS_OK ||
            immediate.kind!=OIR_OPERAND_INTEGER_IMMEDIATE || immediate.immediate!=2u ||
            oir_module_get_operand(&module,2u,&immediate)!=OIR_STATUS_OK ||
            immediate.kind!=OIR_OPERAND_INTEGER_IMMEDIATE || immediate.immediate!=1u ||
            oir_module_get_operand(&module,5u,&immediate)!=OIR_STATUS_OK ||
            immediate.kind!=OIR_OPERAND_INTEGER_IMMEDIATE || immediate.immediate!=2u ||
            oir_module_get_operand(&module,8u,&immediate)!=OIR_STATUS_OK ||
            immediate.kind!=OIR_OPERAND_INTEGER_IMMEDIATE || immediate.immediate!=3u)
            goto cleanup;
    }
    if (!test_lowering_stage_module(stage_module, semantic_artifact, semantic_length,
            module.type_count, module.block_count)) {
        fprintf(stderr, "if fixture: direct lowerer passed but stage DLL invocation failed\\n");
        goto cleanup;
    }
    ok = 1;

cleanup:
    if (semantic_initialized) semantic_destroy(&semantic);
    if (parser) parser_destroy(parser);
    return ok;
}

int main(int argc, char** argv) {
    OccLoadedStageModule stage_module;
    char loader_error[512];
    memset(&stage_module, 0, sizeof(stage_module));
    if (argc != 2) return 2;
    if (occ_module_loader_open(&stage_module, argv[1], loader_error,
            sizeof(loader_error)) != OCC_MODULE_LOADER_OK) {
        fprintf(stderr, "Could not load OIR lowering stage: %s\\n", loader_error);
        return 1;
    }
    uint8_t fields[256], ast_payload[1024], types_payload[128], annotations_payload[64];
    uint8_t module_bytes[1024], strings[] = {'m','a','i','n','i','3','2'};
    uint8_t type_name[] = {'i','3','2'};
    uint8_t tmp[8];
    OirAstNodeRecord nodes[5];
    OirAstTlvWriter writer;
    size_t fields_used=0u, ast_length=0u, types_length=0u, annotations_length=0u, module_length=0u;
    OirSemanticType semantic_type;
    OirSemanticAnnotation annotations[2];
    OirSemanticAstArtifactView artifact;
    OirLoweringWorkspace workspace;
    OirModuleType module_types[2];
    OirModuleFunction functions[1];
    OirModuleBlock blocks[1];
    OirModuleValue values[1];
    OirModuleInstruction instructions[2];
    OirModuleOperand operands[2];
    uint8_t module_strings[16];
    OirModuleView module;
    OirSemanticTypesView types_view;
    OirSemanticAnnotationsView annotations_view;

    memset(nodes,0,sizeof(nodes));
    nodes[0].node_id=1u; nodes[0].node_kind_id=1u;
    nodes[1].node_id=2u; nodes[1].node_kind_id=4u;
    nodes[2].node_id=3u; nodes[2].node_kind_id=2u;
    nodes[3].node_id=4u; nodes[3].node_kind_id=26u;
    nodes[4].node_id=5u; nodes[4].node_kind_id=13u;

    nodes[0].field_data_offset=(uint32_t)fields_used;
    oir_ast_tlv_writer_init(&writer,fields+fields_used,sizeof(fields)-fields_used);
    put32(tmp,2u); write_field(&writer,1u,OIR_AST_VALUE_NODE_REF,tmp,4u);
    nodes[0].field_data_length=(uint32_t)writer.used; fields_used+=writer.used;

    nodes[1].field_data_offset=(uint32_t)fields_used;
    oir_ast_tlv_writer_init(&writer,fields+fields_used,sizeof(fields)-fields_used);
    put32(tmp,0u); put32(tmp+4,4u); write_field(&writer,1u,OIR_AST_VALUE_STRING_REF,tmp,8u);
    put32(tmp,4u); put32(tmp+4,3u); write_field(&writer,2u,OIR_AST_VALUE_STRING_REF,tmp,8u);
    tmp[0]=0u; write_field(&writer,3u,OIR_AST_VALUE_BOOL,tmp,1u);
    tmp[0]=0u; write_field(&writer,4u,OIR_AST_VALUE_BOOL,tmp,1u);
    tmp[0]=0u; write_field(&writer,5u,OIR_AST_VALUE_BOOL,tmp,1u);
    put32(tmp,0u); write_field(&writer,6u,OIR_AST_VALUE_NODE_REF,tmp,4u);
    put32(tmp,3u); write_field(&writer,7u,OIR_AST_VALUE_NODE_REF,tmp,4u);
    nodes[1].field_data_length=(uint32_t)writer.used; fields_used+=writer.used;

    nodes[2].field_data_offset=(uint32_t)fields_used;
    oir_ast_tlv_writer_init(&writer,fields+fields_used,sizeof(fields)-fields_used);
    put32(tmp,4u); write_field(&writer,1u,OIR_AST_VALUE_NODE_REF,tmp,4u);
    nodes[2].field_data_length=(uint32_t)writer.used; fields_used+=writer.used;

    nodes[3].field_data_offset=(uint32_t)fields_used;
    oir_ast_tlv_writer_init(&writer,fields+fields_used,sizeof(fields)-fields_used);
    put16(tmp,9u); write_field(&writer,1u,OIR_AST_VALUE_TOKEN_ID,tmp,2u);
    put32(tmp,5u); write_field(&writer,2u,OIR_AST_VALUE_NODE_REF,tmp,4u);
    nodes[3].field_data_length=(uint32_t)writer.used; fields_used+=writer.used;

    nodes[4].field_data_offset=(uint32_t)fields_used;
    oir_ast_tlv_writer_init(&writer,fields+fields_used,sizeof(fields)-fields_used);
    put16(tmp,4u); write_field(&writer,1u,OIR_AST_VALUE_TOKEN_ID,tmp,2u);
    put32(tmp,0u); write_field(&writer,2u,OIR_AST_VALUE_U32,tmp,4u);
    put64(tmp,42u); write_field(&writer,3u,OIR_AST_VALUE_I64,tmp,8u);
    nodes[4].field_data_length=(uint32_t)writer.used; fields_used+=writer.used;

    assert(oir_ast_node_table_write(ast_payload,sizeof(ast_payload),nodes,5u,1u,
        fields,(uint32_t)fields_used,strings,sizeof(strings),&ast_length)==OIR_STATUS_OK);

    memset(&semantic_type,0,sizeof(semantic_type));
    semantic_type.type_id=1u; semantic_type.kind=OIR_SEMANTIC_TYPE_PRIMITIVE;
    semantic_type.name_offset=0u; semantic_type.name_length=3u;
    assert(oir_semantic_types_write(types_payload,sizeof(types_payload),&semantic_type,1u,
        type_name,sizeof(type_name),&types_length)==OIR_STATUS_OK);
    assert(oir_semantic_types_open(types_payload,types_length,&types_view)==OIR_STATUS_OK);

    memset(annotations,0,sizeof(annotations));
    annotations[0].node_id=2u; annotations[0].scope_id=1u;
    annotations[0].resolved_type_id=1u;
    annotations[0].value_category=OIR_SEMANTIC_VALUE_FUNCTION_DESIGNATOR;
    annotations[1].node_id=5u; annotations[1].scope_id=1u;
    annotations[1].resolved_type_id=1u;
    annotations[1].value_category=OIR_SEMANTIC_VALUE_RVALUE;
    assert(oir_semantic_annotations_write(annotations_payload,sizeof(annotations_payload),
        annotations,2u,&annotations_length)==OIR_STATUS_OK);
    assert(oir_semantic_annotations_open(annotations_payload,annotations_length,
        &annotations_view)==OIR_STATUS_OK);

    memset(&artifact,0,sizeof(artifact));
    assert(oir_ast_node_table_open(ast_payload,ast_length,&artifact.ast)==OIR_STATUS_OK);
    artifact.types=types_view; artifact.annotations=annotations_view;

    memset(&workspace,0,sizeof(workspace));
    workspace.types=module_types; workspace.type_capacity=2u;
    workspace.functions=functions; workspace.function_capacity=1u;
    workspace.blocks=blocks; workspace.block_capacity=1u;
    workspace.values=values; workspace.value_capacity=1u;
    workspace.instructions=instructions; workspace.instruction_capacity=2u;
    workspace.operands=operands; workspace.operand_capacity=2u;
    workspace.strings=module_strings; workspace.string_capacity=sizeof(module_strings);

    assert(oir_lower_semantic_ast_literal_return(&artifact,&workspace,module_bytes,
        sizeof(module_bytes),&module_length)==OIR_STATUS_OK);
    assert(oir_module_open(module_bytes,module_length,&module)==OIR_STATUS_OK);
    assert(module.type_count==2u && module.function_count==1u);
    assert(module.block_count==1u && module.value_count==1u);
    assert(module.instruction_count==2u && module.operand_count==2u);
    {
        OirModuleOperand immediate;
        OirModuleInstruction constant;
        assert(oir_module_get_instruction(&module,0u,&constant)==OIR_STATUS_OK);
        assert(constant.opcode_id==OIR_OP_CONST_INT);
        assert(oir_module_get_operand(&module,0u,&immediate)==OIR_STATUS_OK);
        assert(immediate.kind==OIR_OPERAND_INTEGER_IMMEDIATE && immediate.immediate==42u);
    }
    {
        OirModuleFunction fn;
        assert(oir_module_get_function(&module,0u,&fn)==OIR_STATUS_OK);
        assert(fn.name_length==4u && memcmp(module.strings+fn.name_offset,"main",4u)==0);
    }
    {
        static const uint8_t literal_source[] = "f:main() !i32 { ret 42; }";
        static const uint8_t arithmetic_source[] = "f:main() !i32 { ret 10 + 20 * 3; }";
        static const uint8_t local_source[] = "f:main() !i32 { v:x!i32 = 42; ret x; }";
        static const uint8_t assignment_source[] =
            "f:main() !i32 { v:x!i32 = 40; x = x + 2; ret x; }";
        static const uint8_t multi_assignment_source[] =
            "f:main() !i32 { v:x!i32 = 5; v:y!i32 = 2; "
            "x = x + 1; y = x * 2; x = y - 3; ret x; }";
        static const uint8_t if_else_source[] =
            "f:main() !i32 { v:x!i32 = 42; "
            "if (x < 50) { ret x; } els { ret 0; } }";
        static const uint8_t if_elsif_else_source[] =
            "f:main() !i32 { v:x!i32 = 42; "
            "if (x < 0) { ret 0; } elsif (x > 50) { ret 1; } els { ret x; } }";
        static const uint8_t bitwise_source[] = "f:main() !i32 { v:a!i32 = 5; v:b!i32 = 3; ret a & b; }";
        static const uint8_t bit_shift_source[] = "f:main() !i32 { v:a!i32 = 5; v:b!i32 = 3; ret a <<< b; }";
        assert(test_real_frontend_to_module(&stage_module, literal_source, sizeof(literal_source)-1u, 0));
        assert(test_real_frontend_to_module(&stage_module, arithmetic_source, sizeof(arithmetic_source)-1u, 1));
        assert(test_real_frontend_to_module(&stage_module, local_source, sizeof(local_source)-1u, 2));
        assert(test_real_frontend_to_module(&stage_module, assignment_source, sizeof(assignment_source)-1u, 3));
        assert(test_real_frontend_to_module(&stage_module, multi_assignment_source, sizeof(multi_assignment_source)-1u, 4));
        assert(test_real_frontend_to_module(&stage_module, if_else_source, sizeof(if_else_source)-1u, 5));
        assert(test_real_frontend_to_module(&stage_module, if_elsif_else_source, sizeof(if_elsif_else_source)-1u, 6));
        assert(test_real_frontend_to_module(&stage_module, bitwise_source, sizeof(bitwise_source)-1u, 7));
        assert(test_real_frontend_to_module(&stage_module, bit_shift_source, sizeof(bit_shift_source)-1u, 8));
    }
    assert(test_lowering_stage_rejects_bad_input(&stage_module));
    occ_module_loader_close(&stage_module);
    return 0;
}
