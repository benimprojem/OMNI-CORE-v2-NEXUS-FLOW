#ifdef NDEBUG
#undef NDEBUG
#endif
#include "oir_ast_frontend_writer.h"
#include "oir_ast_field_schema.h"
#include "oir_ast_graph.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef union AstStorage {
    ASTNode base;
    ASTBinaryExpr binary;
    ASTUnaryExpr unary;
    ASTLiteral literal;
    ASTIdentifier identifier;
    ASTCall call;
    ASTMemberAccess member;
    ASTTuple tuple;
    ASTPatternBinding pattern_binding;
    ASTPatternConstructor pattern_constructor;
    ASTSelect select_node;
    ASTSelectArm select_arm;
    ASTScan scan_node;
    ASTScanArm scan_arm;
    ASTRulesetDecl ruleset_decl;
    ASTRuleBarrier rule_barrier;
    ASTRuleTrigger rule_trigger;
    ASTRuleAlways rule_always;
    ASTRuleDefault rule_default;
    ASTApplyRules apply_rules;
    ASTOnBlock on_block;
    ASTLambda lambda_node;
    ASTRelocate relocate_node;
    ASTUnroll unroll_node;
    ASTArrayLiteral array_literal;
    ASTVarDecl var_decl;
    ASTFuncDecl func_decl;
    ASTStructDecl struct_decl;
    ASTEnumVariant enum_variant;
    ASTEnumDecl enum_decl;
    ASTMethod method;
    ASTGroupDecl group;
    ASTUse use;
    ASTDefer defer_node;
    ASTThreadBlock thread_block;
    ASTNewTypeDecl newtype_decl;
    ASTBlock block;
    ASTIf if_node;
    ASTLoop loop;
    ASTNexusCapture capture;
    ASTNexusFlow flow;
    ASTNexusLabel label;
    ASTNexusJump jump;
    ASTHalt halt;
    ASTIntent intent;
    ASTAiLogic ai_logic;
    ASTUnpack unpack;
    ASTPanic panic;
    ASTProgram program;
} AstStorage;

#define INDEX_CAPACITY 256u
#define MAX_NODES (OIR_AST_NODE_KIND_COUNT + 1u)
#define FIELD_CAPACITY 8192u
#define STRING_CAPACITY 2048u
#define OUTPUT_CAPACITY 16384u

int main(void) {
    ASTProgram root;
    AstStorage nodes[OIR_AST_NODE_KIND_COUNT];
    OirAstPointerIndexEntry index[INDEX_CAPACITY];
    OirAstNodeRecord records[MAX_NODES];
    uint8_t field_data[FIELD_CAPACITY];
    uint8_t strings[STRING_CAPACITY];
    uint8_t output[OUTPUT_CAPACITY];
    OirAstFrontendRequirements req;
    OirAstNodeTableView view;
    uint32_t node_count=0u;
    size_t written=0u;
    uint32_t incoming[MAX_NODES];
    uint8_t visited[MAX_NODES];
    OirAstGraphStackEntry stack[MAX_NODES];
    ASTNodeType frontend_type;
    uint32_t i;

    memset(&root,0,sizeof(root));
    memset(nodes,0,sizeof(nodes));
    root.base.type=AST_PROGRAM;
    for(i=0;i<OIR_AST_NODE_KIND_COUNT;i++) {
        assert(oir_ast_node_id_to_frontend((uint16_t)(i+1u),&frontend_type));
        nodes[i].base.type=frontend_type;
        if(i+1u<OIR_AST_NODE_KIND_COUNT) nodes[i].base.next=&nodes[i+1u].base;
    }
    root.declarations=&nodes[0].base;

    /* Exercise token, signed integer, F64 and string-table encodings. */
    nodes[12].literal.type=TOKEN_STRING;
    nodes[12].literal.lit_type=2;
    nodes[12].literal.int_value=-42;
    nodes[12].literal.float_value=3.25;
    nodes[12].literal.string_value="literal text";
    nodes[13].identifier.name="nexus";

    /* Exercise nullable strings and canonical booleans. */
    nodes[2].var_decl.name="value";
    nodes[2].var_decl.type_name="i32";
    nodes[2].var_decl.is_const=1;
    nodes[2].var_decl.is_hard_typed=1;
    nodes[2].var_decl.is_public=0;
    nodes[2].var_decl.is_exported=1;

    assert(oir_ast_frontend_index_build((ASTNode*)&root,index,INDEX_CAPACITY,&node_count)==OIR_STATUS_OK);
    assert(node_count==MAX_NODES);
    assert(oir_ast_frontend_measure((ASTNode*)&root,index,INDEX_CAPACITY,node_count,&req)==OIR_STATUS_OK);
    assert(req.node_count==MAX_NODES);
    assert(req.field_data_length>0u);
    assert(req.string_table_length==strlen("value")+strlen("i32")+
        strlen("literal text")+strlen("nexus"));

    memset(records,0xa5,sizeof(records));
    memset(field_data,0xa5,sizeof(field_data));
    memset(strings,0xa5,sizeof(strings));
    memset(output,0xa5,sizeof(output));
    assert(oir_ast_frontend_write_payload((ASTNode*)&root,index,INDEX_CAPACITY,node_count,
        records,MAX_NODES,field_data,req.field_data_length-1u,strings,STRING_CAPACITY,
        output,OUTPUT_CAPACITY,&written,&req)==OIR_STATUS_BUFFER_TOO_SMALL);
    assert(written==0u);
    assert(output[0]==0xa5u);
    assert(((const uint8_t*)records)[0]==0xa5u);
    assert(field_data[0]==0xa5u);
    assert(strings[0]==0xa5u);

    assert(oir_ast_frontend_write_payload((ASTNode*)&root,index,INDEX_CAPACITY,node_count,
        records,MAX_NODES,field_data,FIELD_CAPACITY,strings,STRING_CAPACITY,
        output,OUTPUT_CAPACITY,&written,&req)==OIR_STATUS_OK);
    assert(written==req.payload_size);
    assert(oir_ast_node_table_open(output,written,&view)==OIR_STATUS_OK);
    assert(oir_ast_field_schema_validate(&view)==OIR_STATUS_OK);
    assert(oir_ast_graph_validate(&view,incoming,MAX_NODES,visited,MAX_NODES,
        stack,MAX_NODES)==OIR_STATUS_OK);
    assert(view.node_count==MAX_NODES);
    assert(view.string_table_length==req.string_table_length);

    /* The literal token is stable OIR TOKEN_STRING ID 5; lit_type is U32. */
    {
        OirAstNodeRecord literal_record;
        OirAstTlvReader reader;
        OirAstTlvField field;
        int has_field=0;
        assert(oir_ast_node_table_get(&view,13u,&literal_record)==OIR_STATUS_OK);
        oir_ast_tlv_reader_init(&reader,view.field_data+literal_record.field_data_offset,
            literal_record.field_data_length);
        assert(oir_ast_tlv_reader_next(&reader,&field,&has_field)==OIR_STATUS_OK && has_field);
        assert(field.field_id==1u && field.value_type==OIR_AST_VALUE_TOKEN_ID);
        assert(field.value[0]==5u && field.value[1]==0u);
        assert(oir_ast_tlv_reader_next(&reader,&field,&has_field)==OIR_STATUS_OK && has_field);
        assert(field.field_id==2u && field.value_type==OIR_AST_VALUE_U32);
        assert(field.value[0]==2u);
    }

    /* Invalid frontend scalar/string values fail during preflight. */
    nodes[2].var_decl.is_const=2;
    assert(oir_ast_frontend_measure((ASTNode*)&root,index,INDEX_CAPACITY,node_count,&req)==OIR_STATUS_INVALID_SECTION);
    nodes[2].var_decl.is_const=1;
    nodes[12].literal.type=(OmniTokenType)65535;
    assert(oir_ast_frontend_measure((ASTNode*)&root,index,INDEX_CAPACITY,node_count,&req)==OIR_STATUS_INVALID_SECTION);
    nodes[12].literal.type=TOKEN_STRING;
    nodes[13].identifier.name="\xc0\xaf";
    assert(oir_ast_frontend_measure((ASTNode*)&root,index,INDEX_CAPACITY,node_count,&req)==OIR_STATUS_INVALID_SECTION);


    /* A sparse child-field node must serialize exact IDs; empty string != NULL. */
    {
        ASTProgram nested_root;
        ASTNexusFlow nested_flow;
        ASTIdentifier lhs, rhs, catch_node;
        OirAstNodeRecord flow_record, lhs_record;
        OirAstTlvReader reader;
        OirAstTlvField field;
        int has_field=0;
        memset(&nested_root,0,sizeof(nested_root));
        memset(&nested_flow,0,sizeof(nested_flow));
        memset(&lhs,0,sizeof(lhs));
        memset(&rhs,0,sizeof(rhs));
        memset(&catch_node,0,sizeof(catch_node));
        nested_root.base.type=AST_PROGRAM;
        nested_flow.base.type=AST_NEXUS_FLOW;
        nested_flow.op=TOKEN_PIPE;
        nested_flow.retry_count=2;
        nested_flow.retry_delay=50;
        lhs.base.type=AST_IDENTIFIER; lhs.name="";
        rhs.base.type=AST_IDENTIFIER; rhs.name="rhs";
        catch_node.base.type=AST_IDENTIFIER; catch_node.name="catch";
        nested_root.declarations=(ASTNode*)&nested_flow;
        nested_flow.left=(ASTNode*)&lhs;
        nested_flow.right=(ASTNode*)&rhs;
        nested_flow.catch_block=(ASTNode*)&catch_node;

        assert(oir_ast_frontend_index_build((ASTNode*)&nested_root,index,INDEX_CAPACITY,&node_count)==OIR_STATUS_OK);
        assert(node_count==5u);
        assert(oir_ast_frontend_measure((ASTNode*)&nested_root,index,INDEX_CAPACITY,node_count,&req)==OIR_STATUS_OK);
        assert(oir_ast_frontend_write_payload((ASTNode*)&nested_root,index,INDEX_CAPACITY,node_count,
            records,MAX_NODES,field_data,FIELD_CAPACITY,strings,STRING_CAPACITY,
            output,OUTPUT_CAPACITY,&written,&req)==OIR_STATUS_OK);
        assert(oir_ast_node_table_open(output,written,&view)==OIR_STATUS_OK);
        assert(oir_ast_field_schema_validate(&view)==OIR_STATUS_OK);
        assert(oir_ast_graph_validate(&view,incoming,MAX_NODES,visited,MAX_NODES,
            stack,MAX_NODES)==OIR_STATUS_OK);

        assert(oir_ast_node_table_get(&view,1u,&flow_record)==OIR_STATUS_OK);
        oir_ast_tlv_reader_init(&reader,view.field_data+flow_record.field_data_offset,
            flow_record.field_data_length);
        assert(oir_ast_tlv_reader_next(&reader,&field,&has_field)==OIR_STATUS_OK && has_field);
        assert(field.field_id==1u && field.value_type==OIR_AST_VALUE_TOKEN_ID);
        assert(oir_ast_tlv_reader_next(&reader,&field,&has_field)==OIR_STATUS_OK && has_field);
        assert(field.field_id==2u && field.value_type==OIR_AST_VALUE_NODE_REF && field.value[0]==3u);
        assert(oir_ast_tlv_reader_next(&reader,&field,&has_field)==OIR_STATUS_OK && has_field);
        assert(field.field_id==3u && field.value_type==OIR_AST_VALUE_NODE_REF && field.value[0]==4u);
        assert(oir_ast_tlv_reader_next(&reader,&field,&has_field)==OIR_STATUS_OK && has_field);
        assert(field.field_id==4u && field.value_type==OIR_AST_VALUE_I32 && field.value[0]==2u);
        assert(oir_ast_tlv_reader_next(&reader,&field,&has_field)==OIR_STATUS_OK && has_field);
        assert(field.field_id==5u && field.value_type==OIR_AST_VALUE_I32 && field.value[0]==50u);
        assert(oir_ast_tlv_reader_next(&reader,&field,&has_field)==OIR_STATUS_OK && has_field);
        assert(field.field_id==6u && field.value_type==OIR_AST_VALUE_NODE_REF && field.value[0]==5u);

        assert(oir_ast_node_table_get(&view,2u,&lhs_record)==OIR_STATUS_OK);
        oir_ast_tlv_reader_init(&reader,view.field_data+lhs_record.field_data_offset,
            lhs_record.field_data_length);
        assert(oir_ast_tlv_reader_next(&reader,&field,&has_field)==OIR_STATUS_OK && has_field);
        assert(field.field_id==1u && field.value_type==OIR_AST_VALUE_STRING_REF);
        assert(field.value_length==8u);
        assert(field.value[0]==0u && field.value[1]==0u && field.value[2]==0u &&
            field.value[3]==0u && field.value[4]==0u && field.value[5]==0u &&
            field.value[6]==0u && field.value[7]==0u);
    }

    /* Dedicated AST_INDEX_ACCESS wire contract: exactly target/index NODE_REFs. */
    {
        ASTProgram index_root;
        ASTIndexAccess access;
        ASTIdentifier target_node, index_node;
        OirAstNodeRecord access_record;
        OirAstTlvReader reader;
        OirAstTlvField field;
        int has_field=0;
        memset(&index_root,0,sizeof(index_root));
        memset(&access,0,sizeof(access));
        memset(&target_node,0,sizeof(target_node));
        memset(&index_node,0,sizeof(index_node));
        index_root.base.type=AST_PROGRAM;
        access.base.type=AST_INDEX_ACCESS;
        target_node.base.type=AST_IDENTIFIER; target_node.name="items";
        index_node.base.type=AST_IDENTIFIER; index_node.name="i";
        index_root.declarations=(ASTNode*)&access;
        access.target=(ASTNode*)&target_node;
        access.index=(ASTNode*)&index_node;
        assert(oir_ast_frontend_index_build((ASTNode*)&index_root,index,INDEX_CAPACITY,&node_count)==OIR_STATUS_OK);
        assert(node_count==4u);
        assert(oir_ast_frontend_measure((ASTNode*)&index_root,index,INDEX_CAPACITY,node_count,&req)==OIR_STATUS_OK);
        assert(oir_ast_frontend_write_payload((ASTNode*)&index_root,index,INDEX_CAPACITY,node_count,
            records,MAX_NODES,field_data,FIELD_CAPACITY,strings,STRING_CAPACITY,
            output,OUTPUT_CAPACITY,&written,&req)==OIR_STATUS_OK);
        assert(oir_ast_node_table_open(output,written,&view)==OIR_STATUS_OK);
        assert(oir_ast_field_schema_validate(&view)==OIR_STATUS_OK);
        assert(oir_ast_graph_validate(&view,incoming,MAX_NODES,visited,MAX_NODES,
            stack,MAX_NODES)==OIR_STATUS_OK);
        assert(oir_ast_node_table_get(&view,2u,&access_record)==OIR_STATUS_OK);
        oir_ast_tlv_reader_init(&reader,view.field_data+access_record.field_data_offset,
            access_record.field_data_length);
        assert(oir_ast_tlv_reader_next(&reader,&field,&has_field)==OIR_STATUS_OK && has_field);
        printf("DBG field=%u type=%u len=%u val=%u\\n",field.field_id,field.value_type,field.value_length,field.value[0]); assert(field.field_id==1u && field.value_type==OIR_AST_VALUE_NODE_REF && field.value[0]==3u);
        assert(field.field_id==2u && field.value_type==OIR_AST_VALUE_NODE_REF && field.value[0]==4u);
        assert(oir_ast_tlv_reader_next(&reader,&field,&has_field)==OIR_STATUS_OK && !has_field);
    }

    puts("OIR AST frontend writer tests passed.");
    return 0;
}
