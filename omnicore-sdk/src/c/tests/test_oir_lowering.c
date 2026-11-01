#ifdef NDEBUG
#undef NDEBUG
#endif
#include "oir_lowering.h"
#include "oir_ast_tlv.h"
#include "parser.h"
#include "ast.h"
#include "semantic.h"
#include "oir_ast_frontend_writer.h"
#include "oir_ast_frontend_index.h"
#include "oir_semantic_ast_artifact.h"
#include "oir_ast_graph.h"

#include <assert.h>
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

static int test_real_frontend_to_module(const uint8_t* source, size_t source_length, int arithmetic) {
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
    OirModuleType module_types[2];
    OirModuleFunction functions[1];
    OirModuleBlock blocks[1];
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
    if (semantic.error_count != 0 ||
        semantic_finalize_result(&semantic, node_count) != SEMANTIC_RESULT_OK ||
        semantic.result.finalized != 1u)
        goto cleanup;
    if (oir_semantic_ast_artifact_write(ast_payload, ast_length,
            &semantic.result, 1u, semantic_artifact,
            sizeof(semantic_artifact), &semantic_length) != OIR_STATUS_OK)
        goto cleanup;
    if (oir_semantic_ast_artifact_open(semantic_artifact, semantic_length,
            &artifact, incoming, 64u, visited, 64u, graph_stack, 64u,
            owner_scope, 65u) != OIR_STATUS_OK)
        goto cleanup;

    workspace.types = module_types; workspace.type_capacity = 2u;
    workspace.functions = functions; workspace.function_capacity = 1u;
    workspace.blocks = blocks; workspace.block_capacity = 1u;
    workspace.values = values; workspace.value_capacity = 16u;
    workspace.locals = locals; workspace.local_capacity = 8u;
    workspace.instructions = instructions; workspace.instruction_capacity = 32u;
    workspace.operands = operands; workspace.operand_capacity = 64u;
    workspace.strings = module_strings; workspace.string_capacity = sizeof(module_strings);
    if (oir_lower_semantic_ast_return(&artifact, &workspace,
            module_bytes, sizeof(module_bytes), &module_length) != OIR_STATUS_OK ||
        oir_module_open(module_bytes, module_length, &module) != OIR_STATUS_OK)
        goto cleanup;
    if (module.type_count != 2u || module.function_count != 1u) goto cleanup;
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
    } else {
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
    }
    ok = 1;

cleanup:
    if (semantic_initialized) semantic_destroy(&semantic);
    if (parser) parser_destroy(parser);
    return ok;
}

int main(void) {
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
        assert(test_real_frontend_to_module(literal_source, sizeof(literal_source)-1u, 0));
        assert(test_real_frontend_to_module(arithmetic_source, sizeof(arithmetic_source)-1u, 1));
        assert(test_real_frontend_to_module(local_source, sizeof(local_source)-1u, 2));
    }
    return 0;
}
