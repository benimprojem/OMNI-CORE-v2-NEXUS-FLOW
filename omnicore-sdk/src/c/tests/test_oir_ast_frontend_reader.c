#ifdef NDEBUG
#undef NDEBUG
#endif
#include "oir_ast_frontend_reader.h"
#include "oir_ast_frontend_writer.h"

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
#define LEGACY_FLAT_KIND_COUNT 100u
#define MAX_NODES (LEGACY_FLAT_KIND_COUNT + 1u)
#define FIELD_CAPACITY 8192u
#define STRING_CAPACITY 2048u
#define OUTPUT_CAPACITY 16384u

static void put_u32(uint8_t* p,uint32_t v) {
    p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);
}
static ASTNode* nth_next(ASTNode* node,uint32_t count) {
    while(node && count--) node=node->next;
    return node;
}

static OirStatus encode_fixture(
    ASTNode* root,
    OirAstPointerIndexEntry* index,
    OirAstNodeRecord* records,
    uint8_t* fields,
    uint8_t* strings,
    uint8_t* output,
    uint32_t* out_count,
    size_t* out_written,
    OirAstFrontendRequirements* req
) {
    OirStatus status=oir_ast_frontend_index_build(root,index,INDEX_CAPACITY,out_count);
    if(status!=OIR_STATUS_OK)return status;
    status=oir_ast_frontend_measure(root,index,INDEX_CAPACITY,*out_count,req);
    if(status!=OIR_STATUS_OK)return status;
    return oir_ast_frontend_write_payload(root,index,INDEX_CAPACITY,*out_count,
        records,MAX_NODES,fields,FIELD_CAPACITY,strings,STRING_CAPACITY,
        output,OUTPUT_CAPACITY,out_written,req);
}

int main(void) {
    ASTProgram root;
    AstStorage nodes[OIR_AST_NODE_KIND_COUNT];
    OirAstPointerIndexEntry index[INDEX_CAPACITY];
    OirAstNodeRecord records[MAX_NODES];
    uint8_t fields[FIELD_CAPACITY], strings[STRING_CAPACITY], output[OUTPUT_CAPACITY];
    ASTNode* decoded[MAX_NODES];
    uint32_t incoming[MAX_NODES], count=0u, i;
    uint8_t visited[MAX_NODES];
    OirAstGraphStackEntry stack[MAX_NODES];
    OirAstFrontendRequirements req;
    ASTNode* decoded_root=NULL;
    size_t written=0u;
    ASTNodeType expected_type;

    memset(&root,0,sizeof(root));
    memset(nodes,0,sizeof(nodes));
    root.base.type=AST_PROGRAM;
    root.base.line=1; root.base.col=1;
    for(i=0;i<LEGACY_FLAT_KIND_COUNT;i++) {
        assert(oir_ast_node_id_to_frontend((uint16_t)(i+1u),&expected_type));
        nodes[i].base.type=expected_type;
        nodes[i].base.line=(int)i+2;
        nodes[i].base.col=(int)i+3;
        if(i+1u<LEGACY_FLAT_KIND_COUNT)nodes[i].base.next=&nodes[i+1u].base;
    }
    root.declarations=&nodes[0].base;

    nodes[2].var_decl.name="amount";
    nodes[2].var_decl.type_name="i64";
    nodes[2].var_decl.is_const=1;
    nodes[2].var_decl.is_hard_typed=1;
    nodes[2].var_decl.is_public=0;
    nodes[2].var_decl.is_exported=1;
    nodes[3].func_decl.name="calculate";
    nodes[3].func_decl.return_type="f64";
    nodes[3].func_decl.is_public=1;
    nodes[4].struct_decl.name="Packed";
    nodes[4].struct_decl.is_packed=1;
    nodes[5].enum_decl.name="State";
    nodes[7].newtype_decl.name="Meters";
    nodes[7].newtype_decl.target_type="u32";
    nodes[10].method.public_name="display";
    nodes[12].literal.type=TOKEN_STRING;
    nodes[12].literal.lit_type=2;
    nodes[12].literal.int_value=-922;
    nodes[12].literal.float_value=9.75;
    nodes[12].literal.string_value="literal value";
    nodes[13].identifier.name="nexus";
    nodes[19].member.member_name="field";
    nodes[24].loop.loop_type=3;
    nodes[31].flow.op=TOKEN_PIPE;
    nodes[31].flow.retry_count=3;
    nodes[31].flow.retry_delay=125;
    nodes[32].label.label_name="finish";
    nodes[33].jump.label_name="finish";
    nodes[35].intent.intent_name="persist";
    nodes[36].ai_logic.query="classify";
    nodes[38].panic.message="failure";
    nodes[69].thread_block.name="worker";
    nodes[70].use.module_name="std.io";
    nodes[70].use.alias="io";

    assert(encode_fixture((ASTNode*)&root,index,records,fields,strings,output,
        &count,&written,&req)==OIR_STATUS_OK);
    assert(count==MAX_NODES);
    assert(oir_ast_frontend_read_payload(output,written,decoded,MAX_NODES,
        incoming,MAX_NODES,visited,MAX_NODES,stack,MAX_NODES,&decoded_root)==OIR_STATUS_OK);
    assert(decoded_root && decoded_root->type==AST_PROGRAM);
    assert(decoded_root->line==1 && decoded_root->col==1);

    {
        ASTNode* current=((ASTProgram*)decoded_root)->declarations;
        for(i=0;i<LEGACY_FLAT_KIND_COUNT;i++) {
            assert(current!=NULL);
            assert(oir_ast_node_id_to_frontend((uint16_t)(i+1u),&expected_type));
            assert(current->type==expected_type);
            assert(current->line==(int)i+2);
            assert(current->col==(int)i+3);
            current=current->next;
        }
        assert(current==NULL);
    }
    {
        ASTNode* first=((ASTProgram*)decoded_root)->declarations;
        ASTVarDecl* var=(ASTVarDecl*)first->next->next;
        ASTFuncDecl* fn=(ASTFuncDecl*)first->next->next->next;
        ASTLiteral* literal=(ASTLiteral*)nth_next(first,12u);
        ASTIdentifier* identifier=(ASTIdentifier*)nth_next(first,13u);
        ASTLoop* loop=(ASTLoop*)nth_next(first,24u);
        assert(strcmp(var->name,"amount")==0 && strcmp(var->type_name,"i64")==0);
        assert(var->is_const==1 && var->is_hard_typed==1 && var->is_public==0 && var->is_exported==1);
        assert(strcmp(fn->name,"calculate")==0 && strcmp(fn->return_type,"f64")==0 && fn->is_public==1);
        assert(literal->type==TOKEN_STRING && literal->lit_type==2);
        assert(literal->int_value==-922 && literal->float_value==9.75);
        assert(strcmp(literal->string_value,"literal value")==0);
        assert(strcmp(identifier->name,"nexus")==0);
        assert(loop->loop_type==3);
    }
    ast_destroy(decoded_root);
    decoded_root=NULL;

    /* Invalid graph ownership is rejected before touching caller node storage. */
    memset(decoded,0xa5,sizeof(decoded));
    put_u32(output+OIR_AST_PAYLOAD_HEADER_SIZE+16u,2u); /* root.next_node_id */
    assert(oir_ast_frontend_read_payload(output,written,decoded,MAX_NODES,
        incoming,MAX_NODES,visited,MAX_NODES,stack,MAX_NODES,&decoded_root)==OIR_STATUS_INVALID_SECTION);
    assert(decoded_root==NULL);
    assert(((const uint8_t*)decoded)[0]==0xa5u);
    put_u32(output+OIR_AST_PAYLOAD_HEADER_SIZE+16u,0u);

    /* Node storage capacity is checked before allocations. */
    assert(oir_ast_frontend_read_payload(output,written,decoded,MAX_NODES-1u,
        incoming,MAX_NODES,visited,MAX_NODES,stack,MAX_NODES,&decoded_root)==OIR_STATUS_BUFFER_TOO_SMALL);
    assert(decoded_root==NULL);

    /* A valid payload with an out-of-range literal tag is rejected pre-allocation. */
    {
        OirAstNodeTableView view;
        OirAstNodeRecord literal_record;
        size_t literal_value_offset;
        assert(oir_ast_node_table_open(output,written,&view)==OIR_STATUS_OK);
        assert(oir_ast_node_table_get(&view,13u,&literal_record)==OIR_STATUS_OK);
        /* Field 1 is TOKEN_ID (10 bytes total); field 2 U32 starts after it. */
        literal_value_offset=OIR_AST_PAYLOAD_HEADER_SIZE+
            (size_t)view.node_count*OIR_AST_NODE_RECORD_SIZE+
            literal_record.field_data_offset+10u+OIR_AST_TLV_HEADER_SIZE;
        put_u32(output+literal_value_offset,99u);
        assert(oir_ast_frontend_read_payload(output,written,decoded,MAX_NODES,
            incoming,MAX_NODES,visited,MAX_NODES,stack,MAX_NODES,&decoded_root)==OIR_STATUS_INVALID_SECTION);
        assert(decoded_root==NULL);
        put_u32(output+literal_value_offset,2u);
    }

    /* Verify array literals preserve their ordered child chain. */
    {
        ASTProgram array_root;
        ASTArrayLiteral array;
        ASTIdentifier first,second;
        ASTNode* array_decoded=NULL;
        ASTArrayLiteral* decoded_array;
        ASTNode* decoded_first;
        memset(&array_root,0,sizeof(array_root));
        memset(&array,0,sizeof(array));
        memset(&first,0,sizeof(first));
        memset(&second,0,sizeof(second));
        array_root.base.type=AST_PROGRAM;
        array.base.type=AST_ARRAY_LITERAL;
        first.base.type=AST_IDENTIFIER; first.name="one";
        second.base.type=AST_IDENTIFIER; second.name="two";
        array_root.declarations=(ASTNode*)&array;
        array.elements=(ASTNode*)&first;
        first.base.next=(ASTNode*)&second;
        assert(encode_fixture((ASTNode*)&array_root,index,records,fields,strings,output,
            &count,&written,&req)==OIR_STATUS_OK);
        assert(oir_ast_frontend_read_payload(output,written,decoded,MAX_NODES,
            incoming,MAX_NODES,visited,MAX_NODES,stack,MAX_NODES,&array_decoded)==OIR_STATUS_OK);
        decoded_array=(ASTArrayLiteral*)((ASTProgram*)array_decoded)->declarations;
        assert(decoded_array && decoded_array->base.type==AST_ARRAY_LITERAL);
        decoded_first=decoded_array->elements;
        assert(decoded_first && strcmp(((ASTIdentifier*)decoded_first)->name,"one")==0);
        assert(decoded_first->next && strcmp(((ASTIdentifier*)decoded_first->next)->name,"two")==0);
        ast_destroy(array_decoded);
    }

    /* Verify array indexing preserves both ordered child references. */
    {
        ASTProgram index_root;
        ASTIndexAccess access;
        ASTIdentifier target,index_node;
        ASTNode* index_decoded=NULL;
        ASTIndexAccess* decoded_access;
        memset(&index_root,0,sizeof(index_root));
        memset(&access,0,sizeof(access));
        memset(&target,0,sizeof(target));
        memset(&index_node,0,sizeof(index_node));
        index_root.base.type=AST_PROGRAM;
        access.base.type=AST_INDEX_ACCESS;
        target.base.type=AST_IDENTIFIER; target.name="items";
        index_node.base.type=AST_IDENTIFIER; index_node.name="position";
        index_root.declarations=(ASTNode*)&access;
        access.target=(ASTNode*)&target;
        access.index=(ASTNode*)&index_node;
        assert(encode_fixture((ASTNode*)&index_root,index,records,fields,strings,output,
            &count,&written,&req)==OIR_STATUS_OK);
        assert(count==4u);
        assert(oir_ast_frontend_read_payload(output,written,decoded,MAX_NODES,
            incoming,MAX_NODES,visited,MAX_NODES,stack,MAX_NODES,&index_decoded)==OIR_STATUS_OK);
        decoded_access=(ASTIndexAccess*)((ASTProgram*)index_decoded)->declarations;
        assert(decoded_access && decoded_access->base.type==AST_INDEX_ACCESS);
        assert(strcmp(((ASTIdentifier*)decoded_access->target)->name,"items")==0);
        assert(strcmp(((ASTIdentifier*)decoded_access->index)->name,"position")==0);
        ast_destroy(index_decoded);
    }

    /* Verify a nested ownership graph is materialized into real child pointers. */
    {
        ASTProgram nested_root;
        ASTNexusFlow flow;
        ASTIdentifier lhs,rhs,catch_node;
        ASTNode* nested_decoded=NULL;
        ASTNexusFlow* decoded_flow;
        memset(&nested_root,0,sizeof(nested_root));
        memset(&flow,0,sizeof(flow));
        memset(&lhs,0,sizeof(lhs));
        memset(&rhs,0,sizeof(rhs));
        memset(&catch_node,0,sizeof(catch_node));
        nested_root.base.type=AST_PROGRAM;
        flow.base.type=AST_NEXUS_FLOW; flow.op=TOKEN_PIPE;
        flow.retry_count=2; flow.retry_delay=50;
        lhs.base.type=AST_IDENTIFIER; lhs.name="left";
        rhs.base.type=AST_IDENTIFIER; rhs.name="right";
        catch_node.base.type=AST_IDENTIFIER; catch_node.name="catch";
        nested_root.declarations=(ASTNode*)&flow;
        flow.left=(ASTNode*)&lhs; flow.right=(ASTNode*)&rhs;
        flow.catch_block=(ASTNode*)&catch_node;
        assert(encode_fixture((ASTNode*)&nested_root,index,records,fields,strings,output,
            &count,&written,&req)==OIR_STATUS_OK);
        assert(oir_ast_frontend_read_payload(output,written,decoded,MAX_NODES,
            incoming,MAX_NODES,visited,MAX_NODES,stack,MAX_NODES,&nested_decoded)==OIR_STATUS_OK);
        decoded_flow=(ASTNexusFlow*)((ASTProgram*)nested_decoded)->declarations;
        assert(decoded_flow && decoded_flow->op==TOKEN_PIPE);
        assert(decoded_flow->retry_count==2 && decoded_flow->retry_delay==50);
        assert(strcmp(((ASTIdentifier*)decoded_flow->left)->name,"left")==0);
        assert(strcmp(((ASTIdentifier*)decoded_flow->right)->name,"right")==0);
        assert(strcmp(((ASTIdentifier*)decoded_flow->catch_block)->name,"catch")==0);
        ast_destroy(nested_decoded);
    }

    puts("OIR AST frontend reader tests passed.");
    return 0;
}
