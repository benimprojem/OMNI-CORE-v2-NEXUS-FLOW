#include "oir_lowering.h"
#include "oir_ast_tlv.h"

#include <assert.h>
#include <string.h>

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

int main(void) {
    uint8_t fields[256], ast_payload[1024], types_payload[128], annotations_payload[64];
    uint8_t module_bytes[1024], strings[] = {'m','a','i','n','i','3','2'};
    uint8_t type_name[] = {'i','3','2'};
    uint8_t tmp[8];
    OirAstNodeRecord nodes[5];
    OirAstTlvWriter writer;
    size_t fields_used=0u, ast_length=0u, types_length=0u, annotations_length=0u, module_length=0u;
    OirSemanticType semantic_type;
    OirSemanticAnnotation annotation;
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
    put32(tmp,9u); write_field(&writer,1u,OIR_AST_VALUE_TOKEN_ID,tmp,4u);
    put32(tmp,5u); write_field(&writer,2u,OIR_AST_VALUE_NODE_REF,tmp,4u);
    nodes[3].field_data_length=(uint32_t)writer.used; fields_used+=writer.used;

    nodes[4].field_data_offset=(uint32_t)fields_used;
    oir_ast_tlv_writer_init(&writer,fields+fields_used,sizeof(fields)-fields_used);
    put32(tmp,4u); write_field(&writer,1u,OIR_AST_VALUE_TOKEN_ID,tmp,4u);
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

    memset(&annotation,0,sizeof(annotation));
    annotation.node_id=5u; annotation.scope_id=1u;
    annotation.resolved_type_id=1u; annotation.value_category=OIR_SEMANTIC_VALUE_RVALUE;
    assert(oir_semantic_annotations_write(annotations_payload,sizeof(annotations_payload),
        &annotation,1u,&annotations_length)==OIR_STATUS_OK);
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
    return 0;
}
