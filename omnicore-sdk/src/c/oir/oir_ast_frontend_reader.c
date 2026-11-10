#include "oir_ast_frontend_reader.h"

#include "oir_ast_map.h"
#include "oir_ast_frontend_index.h"
#include "oir_token_map.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

static uint16_t get_u16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t get_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t get_u64(const uint8_t* p) {
    return (uint64_t)get_u32(p) | ((uint64_t)get_u32(p + 4) << 32);
}
static int get_i32(const uint8_t* p, int* out) {
    uint32_t bits = get_u32(p);
    int32_t value;
    memcpy(&value,&bits,sizeof(value));
    if (!out) return 0;
#if INT_MAX < INT32_MAX || INT_MIN > INT32_MIN
    if ((int64_t)value < INT_MIN || (int64_t)value > INT_MAX) return 0;
#endif
    *out=(int)value;
    return 1;
}
static int get_i64(const uint8_t* p, long long* out) {
    uint64_t bits = get_u64(p);
    int64_t value;
    memcpy(&value,&bits,sizeof(value));
    if (!out) return 0;
    *out=(long long)value;
    return 1;
}
static int get_f64(const uint8_t* p, double* out) {
    uint64_t bits;
    if (!out || sizeof(double)!=8u || DBL_MANT_DIG!=53 || DBL_MAX_EXP!=1024) return 0;
    bits=get_u64(p);
    memcpy(out,&bits,sizeof(bits));
    return 1;
}

static OirStatus allocate_string(
    const OirAstNodeTableView* view,
    const OirAstTlvField* field,
    char** out_string
) {
    uint32_t offset,length;
    char* value;
    if(!field || !out_string) return OIR_STATUS_INVALID_ARGUMENT;
    *out_string=NULL;
    if(field->value_type==OIR_AST_VALUE_NULL) return OIR_STATUS_OK;
    if(field->value_type!=OIR_AST_VALUE_STRING_REF || field->value_length!=8u) {
        return OIR_STATUS_INVALID_SECTION;
    }
    offset=get_u32(field->value);
    length=get_u32(field->value+4);
    if(offset>view->string_table_length ||
        length>view->string_table_length-offset ||
        length>OIR_AST_STRING_MAX) return OIR_STATUS_INVALID_SECTION;
    value=(char*)malloc((size_t)length+1u);
    if(!value) return OIR_STATUS_OUT_OF_MEMORY;
    if(length) memcpy(value,view->string_table+offset,length);
    value[length]='\0';
    *out_string=value;
    return OIR_STATUS_OK;
}

static void free_disconnected_node(ASTNode* node) {
    if(!node) return;
    switch(node->type) {
        case AST_VAR_DECL: {
            ASTVarDecl* n=(ASTVarDecl*)node; free(n->name); free(n->type_name); break;
        }
        case AST_FUNC_DECL: case AST_EXTERN_DECL: {
            ASTFuncDecl* n=(ASTFuncDecl*)node; free(n->name); free(n->return_type); free(n->linkname); break;
        }
        case AST_STRUCT_DECL: case AST_UNION_DECL:
            free(((ASTStructDecl*)node)->name); break;
        case AST_ENUM_DECL: free(((ASTEnumDecl*)node)->name); break;
        case AST_NEWTYPE_DECL: {
            ASTNewTypeDecl* n=(ASTNewTypeDecl*)node; free(n->name); free(n->target_type); break;
        }
        case AST_ENUM_VARIANT: free(((ASTEnumVariant*)node)->name); break;
        case AST_GROUP_DECL: free(((ASTGroupDecl*)node)->name); break;
        case AST_METHOD: free(((ASTMethod*)node)->public_name); break;
        case AST_LITERAL: free(((ASTLiteral*)node)->string_value); break;
        case AST_IDENTIFIER: free(((ASTIdentifier*)node)->name); break;
        case AST_PATTERN_BINDING: free(((ASTPatternBinding*)node)->name); break;
        case AST_PATTERN_CONSTRUCTOR: free(((ASTPatternConstructor*)node)->name); break;
        case AST_RULESET_DECL: free(((ASTRulesetDecl*)node)->name); break;
        case AST_APPLY_RULES: free(((ASTApplyRules*)node)->ruleset_name); break;
        case AST_ON_BLOCK: { ASTOnBlock* n=(ASTOnBlock*)node; free(n->ruleset_name); free(n->event_name); break; }
        case AST_MEMBER_ACCESS: free(((ASTMemberAccess*)node)->member_name); break;
        case AST_NEXUS_CAPTURE: case AST_LISTEN:
            free(((ASTNexusCapture*)node)->handler_name); break;
        case AST_NEXUS_LABEL: free(((ASTNexusLabel*)node)->label_name); break;
        case AST_NEXUS_JUMP: free(((ASTNexusJump*)node)->label_name); break;
        case AST_NEXUS_INTENT: free(((ASTIntent*)node)->intent_name); break;
        case AST_AI_LOGIC: free(((ASTAiLogic*)node)->query); break;
        case AST_PANIC: free(((ASTPanic*)node)->message); break;
        case AST_THREAD_BLOCK: free(((ASTThreadBlock*)node)->name); break;
        case AST_USE: {
            ASTUse* n=(ASTUse*)node; free(n->module_name); free(n->alias); free(n->namespace_name); break;
        }
        case AST_NAMED_ARG: free(((ASTNamedArg*)node)->name); break;
        case AST_MUST_DECL: free(((ASTMustDecl*)node)->name); break;
        case AST_ASM_LABEL: { ASTAsmLabel* n=(ASTAsmLabel*)node; free(n->label); free(n->source_text); break; }
        case AST_STRUCT_LITERAL: free(((ASTStructLiteral*)node)->type_name); break;
        case AST_STRUCT_FIELD_INIT: free(((ASTStructFieldInit*)node)->name); break;
        case AST_TYPE_EXPR: free(((ASTTypeExpr*)node)->name); break;
        default: break;
    }
    free(node);
}

static void cleanup_disconnected(ASTNode** nodes, uint32_t count) {
    uint32_t i;
    if(!nodes) return;
    for(i=0;i<count;i++) {
        free_disconnected_node(nodes[i]);
        nodes[i]=NULL;
    }
}

static OirStatus validate_scalar_ranges(const OirAstNodeTableView* view) {
    uint32_t i;
    for(i=0;i<view->node_count;i++) {
        OirAstNodeRecord record;
        OirAstTlvReader reader;
        OirAstTlvField field;
        int has_field=0;
        ASTNodeType frontend_type;
        OirStatus status=oir_ast_node_table_get(view,i,&record);
        if(status!=OIR_STATUS_OK) return status;
        if(record.start_line>(uint32_t)INT_MAX ||
            record.start_column>(uint32_t)INT_MAX) return OIR_STATUS_INVALID_SECTION;
        if(!oir_ast_node_id_to_frontend(record.node_kind_id,&frontend_type)) {
            return OIR_STATUS_INVALID_SECTION;
        }
        (void)frontend_type;
        oir_ast_tlv_reader_init(&reader,view->field_data+record.field_data_offset,
            record.field_data_length);
        do {
            status=oir_ast_tlv_reader_next(&reader,&field,&has_field);
            if(status!=OIR_STATUS_OK) return status;
            if(!has_field) break;
            if(field.value_type==OIR_AST_VALUE_NODE_REF &&
                field.field_id > oir_ast_frontend_child_field_count(frontend_type)) {
                /* Unknown optional references cannot be dropped: they own nodes. */
                return OIR_STATUS_INVALID_SECTION;
            }
            if(field.value_type==OIR_AST_VALUE_TOKEN_ID) {
                OmniTokenType token;
                if(!oir_token_id_to_omni(get_u16(field.value),&token)) {
                    return OIR_STATUS_INVALID_SECTION;
                }
            } else if(field.value_type==OIR_AST_VALUE_U32 &&
                record.node_kind_id==13u && field.field_id==2u) {
                if(get_u32(field.value)>4u) return OIR_STATUS_INVALID_SECTION;
            } else if(field.value_type==OIR_AST_VALUE_U32 &&
                record.node_kind_id==108u && field.field_id==1u) {
                if(get_u32(field.value)<1u || get_u32(field.value)>6u)
                    return OIR_STATUS_INVALID_SECTION;
            } else if(field.value_type==OIR_AST_VALUE_I32) {
                int value;
                if(!get_i32(field.value,&value)) return OIR_STATUS_INVALID_SECTION;
                if(record.node_kind_id==25u && field.field_id==1u &&
                    (value<0 || value>3)) return OIR_STATUS_INVALID_SECTION;
            } else if(field.value_type==OIR_AST_VALUE_I64) {
                long long value;
                if(!get_i64(field.value,&value)) return OIR_STATUS_INVALID_SECTION;
            } else if(field.value_type==OIR_AST_VALUE_F64) {
                double value;
                if(!get_f64(field.value,&value)) return OIR_STATUS_INVALID_SECTION;
            }
        } while(has_field);
    }
    return OIR_STATUS_OK;
}

static OirStatus populate_scalar_fields(
    const OirAstNodeTableView* view,
    const OirAstNodeRecord* record,
    ASTNode* node
) {
    OirAstTlvReader reader;
    OirAstTlvField field;
    int has_field=0;
    OirStatus status;
    unsigned span_mask=0u;
    OccSourceSpan span;
    memset(&span,0,sizeof(span));
    oir_ast_tlv_reader_init(&reader,view->field_data+record->field_data_offset,
        record->field_data_length);
    do {
        status=oir_ast_tlv_reader_next(&reader,&field,&has_field);
        if(status!=OIR_STATUS_OK) return status;
        if(!has_field) break;
        if(field.field_id>=0xFFF0u && field.field_id<=0xFFF3u){
            unsigned bit=(unsigned)(field.field_id-0xFFF0u);
            if((field.flags&OIR_AST_TLV_OPTIONAL)==0u)return OIR_STATUS_INVALID_SECTION;
            span_mask|=1u<<bit;
            if(bit==0u){if(field.value_type!=OIR_AST_VALUE_U64)return OIR_STATUS_INVALID_SECTION;span.start_offset=get_u64(field.value);}
            else if(bit==1u){if(field.value_type!=OIR_AST_VALUE_U64)return OIR_STATUS_INVALID_SECTION;span.end_offset=get_u64(field.value);}
            else if(bit==2u){if(field.value_type!=OIR_AST_VALUE_U32)return OIR_STATUS_INVALID_SECTION;span.end_line=get_u32(field.value);}
            else {if(field.value_type!=OIR_AST_VALUE_U32)return OIR_STATUS_INVALID_SECTION;span.end_column=get_u32(field.value);}
            continue;
        }
        switch(node->type) {
            case AST_VAR_DECL: {
                ASTVarDecl* n=(ASTVarDecl*)node;
                if(field.field_id==1u) status=allocate_string(view,&field,&n->name);
                else if(field.field_id==2u) status=allocate_string(view,&field,&n->type_name);
                else if(field.field_id>=3u && field.field_id<=6u) {
                    int v=(int)field.value[0];
                    if(field.field_id==3u)n->is_const=v;
                    else if(field.field_id==4u)n->is_hard_typed=v;
                    else if(field.field_id==5u)n->is_public=v;
                    else n->is_exported=v;
                }
                break;
            }
            case AST_FUNC_DECL: case AST_EXTERN_DECL: {
                ASTFuncDecl* n=(ASTFuncDecl*)node;
                if(field.field_id==1u) status=allocate_string(view,&field,&n->name);
                else if(field.field_id==2u) status=allocate_string(view,&field,&n->return_type);
                else if(field.field_id==3u)n->is_extern=(int)field.value[0];
                else if(field.field_id==4u)n->is_public=(int)field.value[0];
                else if(field.field_id==5u)n->is_exported=(int)field.value[0];
                else if(field.field_id==9u)status=allocate_string(view,&field,&n->linkname);
                break;
            }
            case AST_STRUCT_DECL: case AST_UNION_DECL: {
                ASTStructDecl* n=(ASTStructDecl*)node;
                if(field.field_id==1u) status=allocate_string(view,&field,&n->name);
                else if(field.field_id==2u)n->is_packed=(int)field.value[0];
                else if(field.field_id==3u)n->is_union=(int)field.value[0];
                else if(field.field_id==4u)n->is_public=(int)field.value[0];
                else if(field.field_id==5u)n->is_exported=(int)field.value[0];
                break;
            }
            case AST_ENUM_DECL:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTEnumDecl*)node)->name);
                break;
            case AST_NEWTYPE_DECL: {
                ASTNewTypeDecl* n=(ASTNewTypeDecl*)node;
                if(field.field_id==1u) status=allocate_string(view,&field,&n->name);
                else if(field.field_id==2u) status=allocate_string(view,&field,&n->target_type);
                break;
            }
            case AST_ENUM_VARIANT:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTEnumVariant*)node)->name);
                break;
            case AST_GROUP_DECL: {
                ASTGroupDecl* n=(ASTGroupDecl*)node;
                if(field.field_id==1u) status=allocate_string(view,&field,&n->name);
                else if(field.field_id==2u)n->is_public=(int)field.value[0];
                else if(field.field_id==3u)n->is_exported=(int)field.value[0];
                break;
            }
            case AST_METHOD:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTMethod*)node)->public_name);
                break;
            case AST_LITERAL: {
                ASTLiteral* n=(ASTLiteral*)node;
                if(field.field_id==1u) {
                    if(!oir_token_id_to_omni(get_u16(field.value),&n->type)) return OIR_STATUS_INVALID_SECTION;
                } else if(field.field_id==2u)n->lit_type=(int)get_u32(field.value);
                else if(field.field_id==3u) {
                    if(!get_i64(field.value,&n->int_value)) return OIR_STATUS_INVALID_SECTION;
                } else if(field.field_id==4u) {
                    if(!get_f64(field.value,&n->float_value)) return OIR_STATUS_INVALID_SECTION;
                } else if(field.field_id==5u) status=allocate_string(view,&field,&n->string_value);
                break;
            }
            case AST_IDENTIFIER:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTIdentifier*)node)->name);
                break;
            case AST_PATTERN_BINDING:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTPatternBinding*)node)->name);
                break;
            case AST_PATTERN_CONSTRUCTOR:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTPatternConstructor*)node)->name);
                break;
            case AST_RULESET_DECL:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTRulesetDecl*)node)->name);
                break;
            case AST_APPLY_RULES:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTApplyRules*)node)->ruleset_name);
                break;
            case AST_ON_BLOCK: {
                ASTOnBlock* n=(ASTOnBlock*)node;
                if(field.field_id==1u) status=allocate_string(view,&field,&n->ruleset_name);
                else if(field.field_id==2u) status=allocate_string(view,&field,&n->event_name);
                break;
            }
            case AST_LAMBDA:
                if(field.field_id==3u) ((ASTLambda*)node)->body_is_expression=(int)field.value[0];
                break;
            case AST_SELECT_ARM:
                if(field.field_id==3u) ((ASTSelectArm*)node)->is_default=(int)field.value[0];
                break;
            case AST_BINARY_EXPR: case AST_ASSIGNMENT:
            case AST_UNARY_EXPR: case AST_POSTFIX_EXPR: case AST_RETURN:
            case AST_NEXUS_PIPE: case AST_NEXUS_FLOW: {
                if(field.field_id==1u) {
                    OmniTokenType token;
                    if(!oir_token_id_to_omni(get_u16(field.value),&token)) return OIR_STATUS_INVALID_SECTION;
                    if(node->type==AST_UNARY_EXPR || node->type==AST_POSTFIX_EXPR ||
                        node->type==AST_RETURN) {
                        ((ASTUnaryExpr*)node)->op=token;
                    } else ((ASTBinaryExpr*)node)->op=token;
                } else if((node->type==AST_NEXUS_PIPE || node->type==AST_NEXUS_FLOW) &&
                    (field.field_id==4u || field.field_id==5u)) {
                    int value;
                    if(!get_i32(field.value,&value)) return OIR_STATUS_INVALID_SECTION;
                    if(field.field_id==4u)((ASTNexusFlow*)node)->retry_count=value;
                    else ((ASTNexusFlow*)node)->retry_delay=value;
                }
                break;
            }
            case AST_MEMBER_ACCESS:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTMemberAccess*)node)->member_name);
                break;
            case AST_CAST:
                break;
            case AST_LOOP:
                if(field.field_id==1u && !get_i32(field.value,&((ASTLoop*)node)->loop_type)) {
                    return OIR_STATUS_INVALID_SECTION;
                }
                break;
            case AST_NEXUS_CAPTURE: case AST_LISTEN:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTNexusCapture*)node)->handler_name);
                break;
            case AST_NEXUS_LABEL:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTNexusLabel*)node)->label_name);
                break;
            case AST_NEXUS_JUMP:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTNexusJump*)node)->label_name);
                break;
            case AST_NEXUS_INTENT:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTIntent*)node)->intent_name);
                break;
            case AST_AI_LOGIC:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTAiLogic*)node)->query);
                break;
            case AST_PANIC:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTPanic*)node)->message);
                break;
            case AST_THREAD_BLOCK:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTThreadBlock*)node)->name);
                break;
            case AST_USE: {
                ASTUse* n=(ASTUse*)node;
                if(field.field_id==1u) status=allocate_string(view,&field,&n->module_name);
                else if(field.field_id==2u) status=allocate_string(view,&field,&n->alias);
                else if(field.field_id==3u) status=allocate_string(view,&field,&n->namespace_name);
                break;
            }
            case AST_NAMED_ARG:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTNamedArg*)node)->name);
                break;
            case AST_MUST_DECL:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTMustDecl*)node)->name);
                break;
            case AST_ASM_LABEL: {
                ASTAsmLabel* a=(ASTAsmLabel*)node;
                if(field.field_id==1u) status=allocate_string(view,&field,&a->label);
                else if(field.field_id==2u) {
                    status=allocate_string(view,&field,&a->source_text);
                    if(status==OIR_STATUS_OK && a->source_text) a->source_length=strlen(a->source_text);
                }
                break;
            }
            case AST_STRUCT_LITERAL:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTStructLiteral*)node)->type_name);
                break;
            case AST_STRUCT_FIELD_INIT:
                if(field.field_id==1u) status=allocate_string(view,&field,&((ASTStructFieldInit*)node)->name);
                break;
            case AST_TYPE_EXPR: {
                ASTTypeExpr* n=(ASTTypeExpr*)node;
                if(field.field_id==1u)n->kind=get_u32(field.value);
                else if(field.field_id==2u)n->flags=get_u32(field.value);
                else if(field.field_id==3u)status=allocate_string(view,&field,&n->name);
                break;
            }
            default: break;
        }
        if(status!=OIR_STATUS_OK) return status;
    } while(has_field);
    if(span_mask!=0u){
        if(span_mask!=0x0Fu || record->start_line==0u || record->start_column==0u ||
           span.end_offset<span.start_offset || span.end_line==0u || span.end_column==0u)
            return OIR_STATUS_INVALID_SECTION;
        span.has_location=1;
        span.source_id=0u;
        span.start_line=record->start_line;
        span.start_column=record->start_column;
        if(!ast_set_source_span(node,&span))return OIR_STATUS_INVALID_SECTION;
    }
    return OIR_STATUS_OK;
}

static int set_child_pointer(ASTNode* node, uint16_t field_id, ASTNode* child) {
    switch(node->type) {
        case AST_PROGRAM:
            if(field_id==1u)((ASTProgram*)node)->declarations=child; else return 0; break;
        case AST_BLOCK: case AST_ASM_BLOCK:
            if(field_id==1u)((ASTBlock*)node)->statements=child; else return 0; break;
        case AST_FASTEXEC_BLOCK:
            if(field_id==1u)((ASTFastExecBlock*)node)->statements=child; else return 0; break;
        case AST_NAMED_ARG:
            if(field_id==2u)((ASTNamedArg*)node)->value=child; else return 0; break;
        case AST_MUST_DECL:
            if(field_id==2u)((ASTMustDecl*)node)->source=child; else return 0; break;
        case AST_VAR_DECL: {
            ASTVarDecl* n=(ASTVarDecl*)node;
            if(field_id==7u)n->initializer=child; else if(field_id==8u)n->inline_type=child; else if(field_id==9u)n->type_expr=child; else return 0; break;
        }
        case AST_FUNC_DECL: case AST_EXTERN_DECL: {
            ASTFuncDecl* n=(ASTFuncDecl*)node;
            if(field_id==6u)n->params=child; else if(field_id==7u)n->body=child; else if(field_id==8u)n->return_array_type=child; else if(field_id==10u)n->return_type_expr=child; else return 0; break;
        }
        case AST_STRUCT_DECL: case AST_UNION_DECL:
            if(field_id==6u)((ASTStructDecl*)node)->fields=child; else return 0; break;
        case AST_NEWTYPE_DECL:
            if(field_id==3u)((ASTNewTypeDecl*)node)->target_type_expr=child; else return 0; break;
        case AST_ENUM_DECL:
            if(field_id==2u)((ASTEnumDecl*)node)->variants=child; else return 0; break;
        case AST_ENUM_VARIANT:
            if(field_id==2u)((ASTEnumVariant*)node)->data=child; else return 0; break;
        case AST_GROUP_DECL:
            if(field_id==4u)((ASTGroupDecl*)node)->members=child; else return 0; break;
        case AST_METHOD:
            if(field_id==2u)((ASTMethod*)node)->implementation=child; else return 0; break;
        case AST_BINARY_EXPR: case AST_ASSIGNMENT: {
            ASTBinaryExpr* n=(ASTBinaryExpr*)node;
            if(field_id==2u)n->left=child; else if(field_id==3u)n->right=child; else return 0; break;
        }
        case AST_INDEX_ACCESS: {
            ASTIndexAccess* n=(ASTIndexAccess*)node;
            if(field_id==1u)n->target=child; else if(field_id==2u)n->index=child; else return 0; break;
        }
        case AST_UNARY_EXPR: case AST_POSTFIX_EXPR: case AST_RETURN:
            if(field_id==2u)((ASTUnaryExpr*)node)->operand=child; else return 0; break;
        case AST_CAST: {
            ASTCast* n=(ASTCast*)node;
            if(field_id==1u)n->value=child;
            else if(field_id==2u)n->target_type=child;
            else return 0;
            break;
        }
        case AST_FUNC_CALL: case AST_SPAWN: case AST_ECHO: case AST_SEND:
        case AST_RECEIVE: case AST_DONE: case AST_AREA: case AST_ZONE: case AST_ADDR:
        case AST_LEN: case AST_PEEK: case AST_POKE: case AST_INB: case AST_OUTB:
        case AST_IRQ: case AST_INTR: case AST_REG: case AST_PRNT: case AST_PRMT:
        case AST_WRITE: case AST_FWRITE: case AST_TYPEOF: case AST_SIZEOF:
        case AST_IS_OK: case AST_IS_ERR: case AST_IS_SOME: case AST_IS_NONE:
        case AST_IS_NULL: case AST_EXIT: case AST_SWAP: case AST_DATE:
        case AST_DATENOW: case AST_TIME: case AST_CLOCK: case AST_FREE:
        case AST_ZONE_SYNC: case AST_ZONE_VIEW: case AST_ZONE_LOCK: {
            ASTCall* n=(ASTCall*)node;
            if(field_id==1u) n->callee=child;
            else if(field_id==2u) n->args=child;
            else return 0;
            break;
        }
        case AST_MEMBER_ACCESS:
            if(field_id==2u) ((ASTMemberAccess*)node)->object=child;
            else return 0;
            break;
        case AST_TUPLE:
            if(field_id==1u) ((ASTTuple*)node)->elements=child;
            else return 0;
            break;
        case AST_ARRAY_LITERAL:
            if(field_id==1u) ((ASTArrayLiteral*)node)->elements=child;
            else return 0;
            break;
        case AST_STRUCT_LITERAL:
            if(field_id==2u) ((ASTStructLiteral*)node)->fields=child;
            else return 0;
            break;
        case AST_STRUCT_FIELD_INIT:
            if(field_id==2u) ((ASTStructFieldInit*)node)->value=child;
            else return 0;
            break;
        case AST_ARRAY_TYPE:
            if(field_id==1u) ((ASTArrayType*)node)->dimensions=child;
            else return 0;
            break;
        case AST_ARRAY_DIMENSION:
            if(field_id==1u) ((ASTArrayDimension*)node)->length=child;
            else return 0;
            break;
        case AST_TYPE_EXPR: {
            ASTTypeExpr* n=(ASTTypeExpr*)node;
            if(field_id==4u)n->child=child;
            else if(field_id==5u)n->params=child;
            else if(field_id==6u)n->dimensions=child;
            else return 0;
            break;
        }
        case AST_SELECT: { ASTSelect* n=(ASTSelect*)node; if(field_id==1u)n->selector=child; else if(field_id==2u)n->arms=child; else return 0; break; }
        case AST_SELECT_ARM: { ASTSelectArm* n=(ASTSelectArm*)node; if(field_id==1u)n->pattern=child; else if(field_id==2u)n->body=child; else return 0; break; }
        case AST_SCAN: { ASTScan* n=(ASTScan*)node; if(field_id==1u)n->subject=child; else if(field_id==2u)n->arms=child; else return 0; break; }
        case AST_SCAN_ARM: { ASTScanArm* n=(ASTScanArm*)node; if(field_id==1u)n->pattern=child; else if(field_id==2u)n->body=child; else return 0; break; }
        case AST_PATTERN_CONSTRUCTOR:
            if(field_id==2u)((ASTPatternConstructor*)node)->bindings=child; else return 0; break;
        case AST_RULESET_DECL: { ASTRulesetDecl* n=(ASTRulesetDecl*)node; if(field_id==2u)n->params=child; else if(field_id==3u)n->rules=child; else return 0; break; }
        case AST_RULE_BARRIER: { ASTRuleBarrier* n=(ASTRuleBarrier*)node; if(field_id==1u)n->condition=child; else if(field_id==2u)n->message=child; else return 0; break; }
        case AST_RULE_TRIGGER: { ASTRuleTrigger* n=(ASTRuleTrigger*)node; if(field_id==1u)n->condition=child; else if(field_id==2u)n->action=child; else return 0; break; }
        case AST_RULE_ALWAYS: if(field_id==1u)((ASTRuleAlways*)node)->action=child; else return 0; break;
        case AST_RULE_DEFAULT: if(field_id==1u)((ASTRuleDefault*)node)->action=child; else return 0; break;
        case AST_APPLY_RULES: if(field_id==2u)((ASTApplyRules*)node)->args=child; else return 0; break;
        case AST_ON_BLOCK: if(field_id==3u)((ASTOnBlock*)node)->body=child; else return 0; break;
        case AST_LAMBDA: { ASTLambda* n=(ASTLambda*)node; if(field_id==1u)n->params=child; else if(field_id==2u)n->body=child; else return 0; break; }
        case AST_RELOCATE: { ASTRelocate* n=(ASTRelocate*)node; if(field_id==1u)n->source=child; else if(field_id==2u)n->target=child; else return 0; break; }
        case AST_UNROLL: { ASTUnroll* n=(ASTUnroll*)node; if(field_id==1u)n->count=child; else if(field_id==2u)n->loop=child; else return 0; break; }
        case AST_IF: {
            ASTIf* n=(ASTIf*)node;
            if(field_id==1u) n->condition=child;
            else if(field_id==2u) n->then_branch=child;
            else if(field_id==3u) n->else_branch=child;
            else return 0;
            break;
        }
        case AST_LOOP: {
            ASTLoop* n=(ASTLoop*)node;
            switch(field_id) {
                case 2:n->init=child;break; case 3:n->condition=child;break;
                case 4:n->increment=child;break; case 5:n->body=child;break;
                case 6:n->collection=child;break; case 7:n->variable=child;break;
                default:return 0;
            } break;
        }
        case AST_NEXUS_CAPTURE: case AST_LISTEN:
            if(field_id==2u) ((ASTNexusCapture*)node)->expr=child;
            else return 0;
            break;
        case AST_NEXUS_PIPE: case AST_NEXUS_FLOW: {
            ASTNexusFlow* n=(ASTNexusFlow*)node;
            if(field_id==2u)n->left=child; else if(field_id==3u)n->right=child;
            else if(field_id==6u) n->catch_block=child;
            else return 0;
            break;
        }
        case AST_NEXUS_LABEL:
            if(field_id==2u) ((ASTNexusLabel*)node)->statement=child;
            else return 0;
            break;
        case AST_NEXUS_HALT:
            if(field_id==1u) ((ASTHalt*)node)->expr=child;
            else return 0;
            break;
        case AST_NEXUS_INTENT:
            if(field_id==2u) ((ASTIntent*)node)->target=child;
            else return 0;
            break;
        case AST_UNPACK:
            if(field_id==1u) ((ASTUnpack*)node)->expr=child;
            else return 0;
            break;
        case AST_DEFER:
            if(field_id==1u) ((ASTDefer*)node)->body=child;
            else return 0;
            break;
        case AST_THREAD_BLOCK: {
            ASTThreadBlock* n=(ASTThreadBlock*)node;
            if(field_id==2u)n->body=child; else if(field_id==3u)n->mode=child;
            else if(field_id==4u) n->timeout=child;
            else return 0;
            break;
        }
        default: return 0;
    }
    return 1;
}

static ASTNode* type_expr_array_dimensions_reader(ASTNode* node) {
    while (node && node->type == AST_TYPE_EXPR) {
        ASTTypeExpr* type = (ASTTypeExpr*)node;
        if (type->kind == AST_TYPE_ARRAY) return type->dimensions;
        if (type->kind != AST_TYPE_QUALIFIED) return NULL;
        node = type->child;
    }
    return NULL;
}

OirStatus oir_ast_frontend_read_payload(
    const uint8_t* payload,
    size_t payload_length,
    ASTNode** node_storage,
    size_t node_capacity,
    uint32_t* incoming_counts,
    size_t incoming_capacity,
    uint8_t* visited,
    size_t visited_capacity,
    OirAstGraphStackEntry* graph_stack,
    size_t graph_stack_capacity,
    ASTNode** out_root
) {
    OirAstNodeTableView view;
    OirStatus status;
    uint32_t i;
    if(out_root) *out_root=NULL;
    if(!out_root || !node_storage) return OIR_STATUS_INVALID_ARGUMENT;
    status=oir_ast_node_table_open(payload,payload_length,&view);
    if(status!=OIR_STATUS_OK) return status;
    if(node_capacity<view.node_count) return OIR_STATUS_BUFFER_TOO_SMALL;
    status=oir_ast_field_schema_validate(&view);
    if(status!=OIR_STATUS_OK) return status;
    status=oir_ast_graph_validate(&view,incoming_counts,incoming_capacity,
        visited,visited_capacity,graph_stack,graph_stack_capacity);
    if(status!=OIR_STATUS_OK) return status;
    status=validate_scalar_ranges(&view);
    if(status!=OIR_STATUS_OK) return status;

    memset(node_storage,0,(size_t)view.node_count*sizeof(*node_storage));
    for(i=0;i<view.node_count;i++) {
        OirAstNodeRecord record;
        ASTNodeType type;
        status=oir_ast_node_table_get(&view,i,&record);
        if(status!=OIR_STATUS_OK) goto fail;
        if(!oir_ast_node_id_to_frontend(record.node_kind_id,&type)) {
            status=OIR_STATUS_INVALID_SECTION; goto fail;
        }
        node_storage[i]=ast_create(type);
        if(!node_storage[i]) { status=OIR_STATUS_OUT_OF_MEMORY; goto fail; }
        node_storage[i]->line=(int)record.start_line;
        node_storage[i]->col=(int)record.start_column;
    }

    /* Populate scalars and allocate strings while every node is disconnected. */
    for(i=0;i<view.node_count;i++) {
        OirAstNodeRecord record;
        status=oir_ast_node_table_get(&view,i,&record);
        if(status!=OIR_STATUS_OK) goto fail;
        status=populate_scalar_fields(&view,&record,node_storage[i]);
        if(status!=OIR_STATUS_OK) goto fail;
    }

    /* No-fail ownership-link phase. Validation above makes every target safe. */
    for(i=0;i<view.node_count;i++) {
        OirAstNodeRecord record;
        OirAstTlvReader reader;
        OirAstTlvField field;
        int has_field=0;
        status=oir_ast_node_table_get(&view,i,&record);
        if(status!=OIR_STATUS_OK) goto fail;
        oir_ast_tlv_reader_init(&reader,view.field_data+record.field_data_offset,
            record.field_data_length);
        do {
            uint32_t target_id;
            status=oir_ast_tlv_reader_next(&reader,&field,&has_field);
            if(status!=OIR_STATUS_OK) goto fail;
            if(!has_field) break;
            if(field.value_type==OIR_AST_VALUE_NODE_REF) {
                target_id=get_u32(field.value);
                if(target_id!=0u &&
                    !set_child_pointer(node_storage[i],field.field_id,node_storage[target_id-1u])) {
                    /* An unknown optional NODE_REF cannot be materialized safely:
                     * ignoring it would orphan an allocated node and leak it. */
                    status=OIR_STATUS_INVALID_SECTION; goto fail;
                }
            }
        } while(has_field);
        if(record.next_node_id!=0u) {
            node_storage[i]->next=node_storage[record.next_node_id-1u];
        }
    }

    /* Rebuild non-owning legacy aliases after all owned type edges exist. */
    for(i=0;i<view.node_count;i++) {
        ASTNode* node=node_storage[i];
        if(node->type==AST_VAR_DECL) {
            ASTVarDecl* decl=(ASTVarDecl*)node;
            ASTNode* dimensions=type_expr_array_dimensions_reader(decl->type_expr);
            if(dimensions && !decl->inline_type) decl->inline_type=dimensions;
        } else if(node->type==AST_FUNC_DECL || node->type==AST_EXTERN_DECL) {
            ASTFuncDecl* decl=(ASTFuncDecl*)node;
            ASTNode* dimensions=type_expr_array_dimensions_reader(decl->return_type_expr);
            if(dimensions && !decl->return_array_type) decl->return_array_type=dimensions;
        }
    }

    *out_root=node_storage[0];
    for(i=0;i<view.node_count;i++) node_storage[i]=NULL;
    return OIR_STATUS_OK;

fail:
    cleanup_disconnected(node_storage,view.node_count);
    *out_root=NULL;
    return status;
}
