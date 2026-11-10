#include "oir_lowering.h"
#include "oir_type_map.h"
#include <string.h>

#define N_PROGRAM 1u
#define N_BLOCK 2u
#define N_VAR_DECL 3u
#define N_FUNCTION 4u
#define N_LITERAL 13u
#define N_IDENTIFIER 14u
#define N_BINARY 15u
#define N_ASSIGNMENT 18u
#define N_IF 24u
#define N_RETURN 26u
#define T_NUMBER 4u
#define T_TRUE 7u
#define T_FALSE 8u
#define T_RET 9u
#define T_PLUS 96u
#define T_MINUS 97u
#define T_STAR 98u
#define T_SLASH 99u
#define T_EQ 109u
#define T_NEQ 110u
#define T_GT 111u
#define T_LT 112u
#define T_GTE 113u
#define T_LTE 114u
#define T_ASSIGN 102u
#define T_BIT_AND 130u
#define T_BIT_OR 131u
#define T_BIT_XOR 132u

static uint16_t u16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t u64(const uint8_t* p) {
    return (uint64_t)u32(p) | ((uint64_t)u32(p + 4) << 32);
}
static OirStatus node(const OirAstNodeTableView* a, uint32_t id, OirAstNodeRecord* n) {
    if (!id || id > a->node_count) return OIR_STATUS_INVALID_SECTION;
    return oir_ast_node_table_get(a, id - 1u, n);
}
static OirStatus field(const OirAstNodeTableView* a, uint32_t id, uint16_t fid,
    uint8_t kind, OirAstTlvField* f) {
    if (!id || id > a->node_count ||
        oir_ast_node_table_get_field(a, id - 1u, fid, f) != OIR_STATUS_OK ||
        f->value_type != kind) return OIR_STATUS_INVALID_SECTION;
    return OIR_STATUS_OK;
}
static OirStatus ref(const OirAstNodeTableView* a, uint32_t id, uint16_t fid, uint32_t* out) {
    OirAstTlvField f;
    OirStatus s = field(a, id, fid, OIR_AST_VALUE_NODE_REF, &f);
    if (s == OIR_STATUS_OK) *out = u32(f.value);
    return s;
}
static OirStatus token(const OirAstNodeTableView* a, uint32_t id, uint16_t fid, uint16_t* out) {
    OirAstTlvField f;
    OirStatus s = field(a, id, fid, OIR_AST_VALUE_TOKEN_ID, &f);
    if (s != OIR_STATUS_OK || f.value_length != 2u) return OIR_STATUS_INVALID_SECTION;
    *out = u16(f.value);
    return OIR_STATUS_OK;
}
static OirStatus string_ref(const OirAstNodeTableView* a, uint32_t id, uint16_t fid,
    const uint8_t** bytes, uint32_t* length) {
    OirAstTlvField f;
    OirStatus s = field(a, id, fid, OIR_AST_VALUE_STRING_REF, &f);
    if (s != OIR_STATUS_OK) return s;
    *length = u32(f.value + 4);
    return oir_ast_node_table_get_string(a, u32(f.value), *length, bytes);
}
static OirStatus annotation_get(const OirSemanticAstArtifactView* a,
    uint32_t node_id, OirSemanticAnnotation* out) {
    uint32_t lo = 0u, hi = a->annotations.record_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        OirSemanticAnnotation x;
        OirStatus s = oir_semantic_annotations_get(&a->annotations, mid, &x);
        if (s != OIR_STATUS_OK) return s;
        if (x.node_id < node_id) lo = mid + 1u;
        else hi = mid;
    }
    if (lo >= a->annotations.record_count ||
        oir_semantic_annotations_get(&a->annotations, lo, out) != OIR_STATUS_OK ||
        out->node_id != node_id) return OIR_STATUS_INVALID_SECTION;
    return OIR_STATUS_OK;
}
static OirStatus annotation_type(const OirSemanticAstArtifactView* a, uint32_t node_id,
    OirSemanticType* out, const uint8_t** name) {
    OirSemanticAnnotation x;
    uint32_t i;
    OirStatus s = annotation_get(a, node_id, &x);
    if (s != OIR_STATUS_OK || !x.resolved_type_id) return OIR_STATUS_INVALID_SECTION;
    for (i = 0u; i < a->types.record_count; ++i) {
        OirSemanticType t;
        const uint8_t* n = NULL;
        s = oir_semantic_types_get(&a->types, i, &t, &n);
        if (s != OIR_STATUS_OK) return s;
        if (t.type_id == x.resolved_type_id) {
            *out = t; *name = n;
            return OIR_STATUS_OK;
        }
    }
    return OIR_STATUS_INVALID_SECTION;
}

typedef struct ExprState {
    const OirSemanticAstArtifactView* artifact;
    OirLoweringWorkspace* ws;
    const uint8_t* type_name;
    uint32_t type_length;
    uint32_t values, instructions, operands;
    uint32_t local_count;
    uint32_t current_block_id;
    uint32_t block_count;
    uint32_t bool_type_id;
} ExprState;

static OirStatus check_type(ExprState* st, uint32_t id) {
    OirSemanticType t;
    const uint8_t* name = NULL;
    OirStatus s = annotation_type(st->artifact, id, &t, &name);
    if (s != OIR_STATUS_OK || !name || t.kind != OIR_SEMANTIC_TYPE_PRIMITIVE ||
        t.name_length != st->type_length ||
        memcmp(name, st->type_name, st->type_length) != 0)
        return OIR_STATUS_INVALID_SECTION;
    return OIR_STATUS_OK;
}

static OirStatus ensure_bool_type(ExprState* st, uint32_t node_id) {
    OirSemanticType type;
    const uint8_t* name = NULL;
    OirStatus status;
    if (!st || !node_id) return OIR_STATUS_INVALID_ARGUMENT;
    if (st->bool_type_id != 0u) return OIR_STATUS_OK;
    if (st->ws->type_capacity < 3u) return OIR_STATUS_BUFFER_TOO_SMALL;
    status = annotation_type(st->artifact, node_id, &type, &name);
    if (status != OIR_STATUS_OK || !name ||
        type.kind != OIR_SEMANTIC_TYPE_PRIMITIVE ||
        type.name_length != 4u || memcmp(name, "bool", 4u) != 0)
        return OIR_STATUS_INVALID_SECTION;
    status = oir_type_map_primitive(&type, name, type.name_length, 2u,
        &st->ws->types[1]);
    if (status == OIR_STATUS_OK) st->bool_type_id = 2u;
    return status;
}

static OirStatus begin_block(ExprState* st, uint32_t block_id) {
    OirModuleBlock* block;
    if (!st || block_id == 0u || block_id > st->ws->block_capacity)
        return OIR_STATUS_BUFFER_TOO_SMALL;
    block = &st->ws->blocks[block_id - 1u];
    memset(block, 0, sizeof(*block));
    block->id = block_id;
    block->function_id = 1u;
    block->first_instruction = st->instructions;
    st->current_block_id = block_id;
    if (block_id > st->block_count) st->block_count = block_id;
    return OIR_STATUS_OK;
}

static OirStatus finish_block(ExprState* st, uint32_t block_id) {
    OirModuleBlock* block;
    if (!st || block_id == 0u || block_id > st->block_count)
        return OIR_STATUS_INVALID_ARGUMENT;
    block = &st->ws->blocks[block_id - 1u];
    if (st->instructions < block->first_instruction)
        return OIR_STATUS_INVALID_SECTION;
    block->instruction_count = st->instructions - block->first_instruction;
    return block->instruction_count ? OIR_STATUS_OK : OIR_STATUS_INVALID_SECTION;
}
static OirStatus alloc_inst(ExprState* st, uint32_t operands,
    uint32_t* id, uint32_t* first) {
    if (st->instructions >= st->ws->instruction_capacity ||
        st->operands > st->ws->operand_capacity ||
        operands > st->ws->operand_capacity - st->operands)
        return OIR_STATUS_BUFFER_TOO_SMALL;
    *id = ++st->instructions;
    *first = st->operands;
    st->operands += operands;
    return OIR_STATUS_OK;
}
static OirStatus lower_expr(ExprState*, uint32_t, uint32_t*, uint32_t);

static OirStatus lower_identifier(ExprState* st, uint32_t node_id, uint32_t* out) {
    OirSemanticAnnotation a;
    uint32_t i;
    OirStatus s = annotation_get(st->artifact, node_id, &a);
    if (s != OIR_STATUS_OK || !a.resolved_symbol_id ||
        (s = check_type(st, node_id)) != OIR_STATUS_OK)
        return OIR_STATUS_INVALID_SECTION;
    for (i = 0u; i < st->local_count; ++i) {
        if (st->ws->locals[i].symbol_id == a.resolved_symbol_id) {
            *out = st->ws->locals[i].value_id;
            return OIR_STATUS_OK;
        }
    }
    return OIR_STATUS_INVALID_SECTION;
}

static OirStatus symbol_matches_declaration(const OirSemanticAstArtifactView* a,
    uint32_t symbol_id, uint32_t declaration_node_id, uint32_t type_id) {
    uint32_t i;
    for (i = 0u; i < a->symbols.record_count; ++i) {
        OirSemanticSymbol symbol;
        OirStatus s = oir_semantic_symbols_get(&a->symbols, i, &symbol, NULL);
        if (s != OIR_STATUS_OK) return s;
        if (symbol.symbol_id == symbol_id) {
            return symbol.kind == OIR_SEMANTIC_SYMBOL_VARIABLE &&
                symbol.declaration_node_id == declaration_node_id &&
                symbol.type_id == type_id ? OIR_STATUS_OK : OIR_STATUS_INVALID_SECTION;
        }
    }
    return OIR_STATUS_INVALID_SECTION;
}

static OirStatus lower_assignment_statement(ExprState* st, uint32_t node_id) {
    OirAstNodeRecord n, lhs;
    OirSemanticAnnotation left_annotation;
    OirSemanticSymbol symbol;
    OirSemanticType symbol_type;
    const uint8_t* symbol_type_name = NULL;
    uint32_t left_id = 0u, right_id = 0u, i, local_index = UINT32_MAX;
    uint16_t op = 0u;
    OirStatus s;

    if ((s=node(&st->artifact->ast,node_id,&n))!=OIR_STATUS_OK ||
        n.node_kind_id!=N_ASSIGNMENT ||
        (s=token(&st->artifact->ast,node_id,1u,&op))!=OIR_STATUS_OK ||
        op!=T_ASSIGN ||
        (s=ref(&st->artifact->ast,node_id,2u,&left_id))!=OIR_STATUS_OK ||
        !left_id ||
        (s=ref(&st->artifact->ast,node_id,3u,&right_id))!=OIR_STATUS_OK ||
        !right_id ||
        (s=node(&st->artifact->ast,left_id,&lhs))!=OIR_STATUS_OK ||
        lhs.node_kind_id!=N_IDENTIFIER ||
        (s=annotation_get(st->artifact,left_id,&left_annotation))!=OIR_STATUS_OK ||
        !left_annotation.resolved_symbol_id ||
        (s=check_type(st,left_id))!=OIR_STATUS_OK)
        return OIR_STATUS_INVALID_SECTION;

    for (i=0u;i<st->artifact->symbols.record_count;i++) {
        if (oir_semantic_symbols_get(&st->artifact->symbols,i,&symbol,NULL)!=OIR_STATUS_OK)
            return OIR_STATUS_INVALID_SECTION;
        if (symbol.symbol_id==left_annotation.resolved_symbol_id) break;
    }
    if (i>=st->artifact->symbols.record_count ||
        symbol.kind!=OIR_SEMANTIC_SYMBOL_VARIABLE ||
        symbol.type_id!=left_annotation.resolved_type_id)
        return OIR_STATUS_INVALID_SECTION;

    for (i=0u;i<st->artifact->types.record_count;i++) {
        if (oir_semantic_types_get(&st->artifact->types,i,&symbol_type,
                &symbol_type_name)!=OIR_STATUS_OK)
            return OIR_STATUS_INVALID_SECTION;
        if (symbol_type.type_id==symbol.type_id) break;
    }
    if (i>=st->artifact->types.record_count || !symbol_type_name ||
        (symbol_type.flags&OIR_SEMANTIC_TYPE_FLAG_CONST)!=0u ||
        symbol_type.kind!=OIR_SEMANTIC_TYPE_PRIMITIVE ||
        symbol_type.name_length!=st->type_length ||
        memcmp(symbol_type_name,st->type_name,st->type_length)!=0)
        return OIR_STATUS_INVALID_SECTION;

    for (i=0u;i<st->local_count;i++) {
        if (st->ws->locals[i].symbol_id==symbol.symbol_id) {
            local_index=i;
            break;
        }
    }
    if (local_index==UINT32_MAX) return OIR_STATUS_INVALID_SECTION;

    {
        uint32_t new_value_id=0u;
        if ((s=lower_expr(st,right_id,&new_value_id,0u))!=OIR_STATUS_OK)
            return s;
        st->ws->locals[local_index].value_id=new_value_id;
    }
    return OIR_STATUS_OK;
}

static OirStatus lower_literal(ExprState* st, const OirAstNodeRecord* n, uint32_t* out) {
    OirAstTlvField f;
    uint16_t tok, opcode;
    uint32_t kind, iid, first, vid, operand_kind, result_type_id = 1u;
    uint64_t imm;
    OirModuleType* ty;
    OirStatus s;
    if ((s = token(&st->artifact->ast, n->node_id, 1u, &tok)) != OIR_STATUS_OK ||
        (s = field(&st->artifact->ast, n->node_id, 2u, OIR_AST_VALUE_U32, &f)) != OIR_STATUS_OK)
        return s;
    kind = u32(f.value);
    if (kind == 4u && (tok == T_TRUE || tok == T_FALSE)) {
        if ((s = ensure_bool_type(st, n->node_id)) != OIR_STATUS_OK) return s;
        result_type_id = st->bool_type_id;
        ty = &st->ws->types[result_type_id - 1u];
        imm = tok == T_TRUE ? 1u : 0u;
        opcode = OIR_OP_CONST_BOOL;
        operand_kind = OIR_OPERAND_INTEGER_IMMEDIATE;
    } else {
        if ((s = check_type(st, n->node_id)) != OIR_STATUS_OK) return s;
        ty = &st->ws->types[0];
        if (kind == 0u && tok == T_NUMBER && ty->kind == OIR_MODULE_TYPE_INTEGER) {
            s = field(&st->artifact->ast, n->node_id, 3u, OIR_AST_VALUE_I64, &f);
            if (s != OIR_STATUS_OK) return s;
            imm = u64(f.value); opcode = OIR_OP_CONST_INT;
            operand_kind = OIR_OPERAND_INTEGER_IMMEDIATE;
        } else if (kind == 1u && tok == T_NUMBER && ty->kind == OIR_MODULE_TYPE_FLOAT) {
            s = field(&st->artifact->ast, n->node_id, 4u, OIR_AST_VALUE_F64, &f);
            if (s != OIR_STATUS_OK) return s;
            imm = u64(f.value); opcode = OIR_OP_CONST_FLOAT;
            operand_kind = OIR_OPERAND_FLOAT_IMMEDIATE;
        } else {
            return OIR_STATUS_INVALID_SECTION;
        }
    }

    if (st->values >= st->ws->value_capacity) return OIR_STATUS_BUFFER_TOO_SMALL;
    if ((s = alloc_inst(st, 1u, &iid, &first)) != OIR_STATUS_OK) return s;
    vid = ++st->values;
    memset(&st->ws->instructions[iid-1u], 0, sizeof(st->ws->instructions[0]));
    st->ws->instructions[iid-1u].id = iid;
    st->ws->instructions[iid-1u].block_id = st->current_block_id;
    st->ws->instructions[iid-1u].opcode_id = opcode;
    st->ws->instructions[iid-1u].result_value_id = vid;
    st->ws->instructions[iid-1u].first_operand = first;
    st->ws->instructions[iid-1u].operand_count = 1u;
    memset(&st->ws->operands[first], 0, sizeof(st->ws->operands[0]));
    st->ws->operands[first].kind = operand_kind;
    st->ws->operands[first].immediate = imm;
    st->ws->values[vid-1u].id = vid;
    st->ws->values[vid-1u].type_id = result_type_id;
    st->ws->values[vid-1u].defining_instruction_id = iid;
    st->ws->values[vid-1u].flags = 0u;
    *out = vid;
    return OIR_STATUS_OK;
}

static OirStatus lower_binary(ExprState* st, const OirAstNodeRecord* n,
    uint32_t* out, uint32_t depth) {
    uint16_t tok, opcode;
    uint32_t lid, rid, lv, rv, iid, first, vid, result_type_id = 1u;
    OirAstNodeRecord ln, rn;
    OirModuleType* ty = &st->ws->types[0];
    OirStatus s;
    int comparison = 0;
    int bitwise = 0;
    if (depth >= OIR_AST_NODE_DEPTH_MAX) return OIR_STATUS_INVALID_SECTION;
    if ((s = token(&st->artifact->ast, n->node_id, 1u, &tok)) != OIR_STATUS_OK)
        return s;

    switch (tok) {
        case T_PLUS: opcode = OIR_OP_ADD; break;
        case T_MINUS: opcode = OIR_OP_SUB; break;
        case T_STAR: opcode = OIR_OP_MUL; break;
        case T_SLASH: opcode = OIR_OP_DIV; break;
        case T_EQ: opcode = OIR_OP_CMP_EQ; comparison = 1; break;
        case T_NEQ: opcode = OIR_OP_CMP_NE; comparison = 1; break;
        case T_LT: opcode = OIR_OP_CMP_LT; comparison = 1; break;
        case T_LTE: opcode = OIR_OP_CMP_LE; comparison = 1; break;
        case T_GT: opcode = OIR_OP_CMP_GT; comparison = 1; break;
        case T_GTE: opcode = OIR_OP_CMP_GE; comparison = 1; break;
        case T_BIT_AND: opcode = OIR_OP_BIT_AND; bitwise = 1; break;
        case T_BIT_OR: opcode = OIR_OP_BIT_OR; bitwise = 1; break;
        case T_BIT_XOR: opcode = OIR_OP_BIT_XOR; bitwise = 1; break;
        default: return OIR_STATUS_INVALID_SECTION;
    }

    if (comparison) {
        if ((s = ensure_bool_type(st, n->node_id)) != OIR_STATUS_OK)
            return s;
        if (ty->kind != OIR_MODULE_TYPE_INTEGER && ty->kind != OIR_MODULE_TYPE_FLOAT)
            return OIR_STATUS_INVALID_SECTION;
        result_type_id = st->bool_type_id;
    } else {
        if ((s = check_type(st, n->node_id)) != OIR_STATUS_OK)
            return s;
        if (bitwise) {
            if (ty->kind != OIR_MODULE_TYPE_INTEGER &&
                ty->kind != OIR_MODULE_TYPE_BIT &&
                ty->kind != OIR_MODULE_TYPE_BYTE &&
                ty->kind != OIR_MODULE_TYPE_BIT_VECTOR &&
                ty->kind != OIR_MODULE_TYPE_BYTE_BLOCK)
                return OIR_STATUS_INVALID_SECTION;
        } else if (ty->kind != OIR_MODULE_TYPE_INTEGER &&
                   ty->kind != OIR_MODULE_TYPE_FLOAT) {
            return OIR_STATUS_INVALID_SECTION;
        }
    }

    if ((s = ref(&st->artifact->ast, n->node_id, 2u, &lid)) != OIR_STATUS_OK || !lid ||
        (s = ref(&st->artifact->ast, n->node_id, 3u, &rid)) != OIR_STATUS_OK || !rid)
        return OIR_STATUS_INVALID_SECTION;
    if ((s = node(&st->artifact->ast, lid, &ln)) != OIR_STATUS_OK || ln.next_node_id ||
        (s = node(&st->artifact->ast, rid, &rn)) != OIR_STATUS_OK || rn.next_node_id)
        return OIR_STATUS_INVALID_SECTION;
    if ((s = lower_expr(st, lid, &lv, depth+1u)) != OIR_STATUS_OK ||
        (s = lower_expr(st, rid, &rv, depth+1u)) != OIR_STATUS_OK) return s;
    if (st->values >= st->ws->value_capacity) return OIR_STATUS_BUFFER_TOO_SMALL;
    if ((s = alloc_inst(st, 2u, &iid, &first)) != OIR_STATUS_OK) return s;
    vid = ++st->values;
    memset(&st->ws->instructions[iid-1u], 0, sizeof(st->ws->instructions[0]));
    st->ws->instructions[iid-1u].id = iid;
    st->ws->instructions[iid-1u].block_id = st->current_block_id;
    st->ws->instructions[iid-1u].opcode_id = opcode;
    st->ws->instructions[iid-1u].result_value_id = vid;
    st->ws->instructions[iid-1u].first_operand = first;
    st->ws->instructions[iid-1u].operand_count = 2u;
    memset(&st->ws->operands[first], 0, 2u*sizeof(st->ws->operands[0]));
    st->ws->operands[first].kind = OIR_OPERAND_VALUE;
    st->ws->operands[first].reference_id = lv;
    st->ws->operands[first+1u].kind = OIR_OPERAND_VALUE;
    st->ws->operands[first+1u].reference_id = rv;
    st->ws->values[vid-1u].id = vid;
    st->ws->values[vid-1u].type_id = result_type_id;
    st->ws->values[vid-1u].defining_instruction_id = iid;
    st->ws->values[vid-1u].flags = 0u;
    *out = vid;
    return OIR_STATUS_OK;
}

static OirStatus lower_expr(ExprState* st, uint32_t id, uint32_t* out, uint32_t depth) {
    OirAstNodeRecord n;
    OirStatus s;
    if (depth > OIR_AST_NODE_DEPTH_MAX) return OIR_STATUS_INVALID_SECTION;
    if ((s = node(&st->artifact->ast, id, &n)) != OIR_STATUS_OK || n.next_node_id)
        return OIR_STATUS_INVALID_SECTION;
    if (n.node_kind_id == N_LITERAL) return lower_literal(st, &n, out);
    if (n.node_kind_id == N_IDENTIFIER) return lower_identifier(st, id, out);
    if (n.node_kind_id == N_BINARY) return lower_binary(st, &n, out, depth);
    return OIR_STATUS_INVALID_SECTION;
}

static OirStatus lower_return_current_block(ExprState* st, uint32_t branch_node_id) {
    OirAstNodeRecord branch, ret;
    uint32_t ret_id = branch_node_id, expr_id = 0u, value_id = 0u;
    uint32_t ret_iid = 0u, ret_first = 0u;
    uint16_t ret_tok = 0u;
    OirStatus status;
    if ((status = node(&st->artifact->ast, branch_node_id, &branch)) != OIR_STATUS_OK)
        return status;
    if (branch.node_kind_id == N_BLOCK) {
        if (branch.next_node_id ||
            (status = ref(&st->artifact->ast, branch_node_id, 1u, &ret_id)) != OIR_STATUS_OK ||
            !ret_id ||
            (status = node(&st->artifact->ast, ret_id, &ret)) != OIR_STATUS_OK ||
            ret.node_kind_id != N_RETURN || ret.next_node_id)
            return OIR_STATUS_INVALID_SECTION;
    } else if (branch.node_kind_id != N_RETURN || branch.next_node_id) {
        return OIR_STATUS_INVALID_SECTION;
    } else {
        ret = branch;
    }
    if ((status = token(&st->artifact->ast, ret_id, 1u, &ret_tok)) != OIR_STATUS_OK ||
        ret_tok != T_RET ||
        (status = ref(&st->artifact->ast, ret_id, 2u, &expr_id)) != OIR_STATUS_OK ||
        !expr_id ||
        (status = lower_expr(st, expr_id, &value_id, 0u)) != OIR_STATUS_OK)
        return OIR_STATUS_INVALID_SECTION;
    if (value_id == 0u || value_id > st->values ||
        st->ws->values[value_id - 1u].type_id != 1u)
        return OIR_STATUS_INVALID_SECTION;
    if ((status = alloc_inst(st, 1u, &ret_iid, &ret_first)) != OIR_STATUS_OK)
        return status;
    memset(&st->ws->instructions[ret_iid - 1u], 0, sizeof(st->ws->instructions[0]));
    st->ws->instructions[ret_iid - 1u].id = ret_iid;
    st->ws->instructions[ret_iid - 1u].block_id = st->current_block_id;
    st->ws->instructions[ret_iid - 1u].opcode_id = OIR_OP_RETURN;
    st->ws->instructions[ret_iid - 1u].first_operand = ret_first;
    st->ws->instructions[ret_iid - 1u].operand_count = 1u;
    st->ws->operands[ret_first].kind = OIR_OPERAND_VALUE;
    st->ws->operands[ret_first].reference_id = value_id;
    st->ws->operands[ret_first].immediate = 0u;
    return finish_block(st, st->current_block_id);
}

static OirStatus lower_branch(ExprState* st, uint32_t branch_node_id, uint32_t block_id);

static OirStatus lower_if_tree(ExprState* st, uint32_t if_node_id) {
    OirAstNodeRecord if_node;
    uint32_t condition_id = 0u, then_branch_id = 0u, else_branch_id = 0u;
    uint32_t then_block_id, else_block_id, condition_value = 0u;
    uint32_t iid = 0u, first = 0u;
    OirStatus status;
    if ((status = node(&st->artifact->ast, if_node_id, &if_node)) != OIR_STATUS_OK ||
        if_node.node_kind_id != N_IF || if_node.next_node_id ||
        (status = ref(&st->artifact->ast, if_node_id, 1u, &condition_id)) != OIR_STATUS_OK ||
        !condition_id ||
        (status = ref(&st->artifact->ast, if_node_id, 2u, &then_branch_id)) != OIR_STATUS_OK ||
        !then_branch_id ||
        (status = ref(&st->artifact->ast, if_node_id, 3u, &else_branch_id)) != OIR_STATUS_OK ||
        !else_branch_id)
        return OIR_STATUS_INVALID_SECTION;
    if (st->ws->block_capacity < 2u ||
        st->block_count > st->ws->block_capacity - 2u)
        return OIR_STATUS_BUFFER_TOO_SMALL;
    then_block_id = ++st->block_count;
    else_block_id = ++st->block_count;

    if ((status = lower_expr(st, condition_id, &condition_value, 0u)) != OIR_STATUS_OK)
        return status;
    if (condition_value == 0u || condition_value > st->values ||
        st->bool_type_id == 0u ||
        st->ws->values[condition_value - 1u].type_id != st->bool_type_id)
        return OIR_STATUS_INVALID_SECTION;

    if ((status = alloc_inst(st, 3u, &iid, &first)) != OIR_STATUS_OK)
        return status;
    memset(&st->ws->instructions[iid - 1u], 0, sizeof(st->ws->instructions[0]));
    st->ws->instructions[iid - 1u].id = iid;
    st->ws->instructions[iid - 1u].block_id = st->current_block_id;
    st->ws->instructions[iid - 1u].opcode_id = OIR_OP_COND_BRANCH;
    st->ws->instructions[iid - 1u].first_operand = first;
    st->ws->instructions[iid - 1u].operand_count = 3u;
    st->ws->operands[first].kind = OIR_OPERAND_VALUE;
    st->ws->operands[first].reference_id = condition_value;
    st->ws->operands[first + 1u].kind = OIR_OPERAND_BLOCK;
    st->ws->operands[first + 1u].reference_id = then_block_id;
    st->ws->operands[first + 2u].kind = OIR_OPERAND_BLOCK;
    st->ws->operands[first + 2u].reference_id = else_block_id;
    if ((status = finish_block(st, st->current_block_id)) != OIR_STATUS_OK)
        return status;

    if ((status = lower_branch(st, then_branch_id, then_block_id)) != OIR_STATUS_OK)
        return status;
    return lower_branch(st, else_branch_id, else_block_id);
}

static OirStatus lower_branch(ExprState* st, uint32_t branch_node_id, uint32_t block_id) {
    OirAstNodeRecord branch;
    OirStatus status;
    if ((status = begin_block(st, block_id)) != OIR_STATUS_OK ||
        (status = node(&st->artifact->ast, branch_node_id, &branch)) != OIR_STATUS_OK)
        return status;
    if (branch.node_kind_id == N_IF)
        return lower_if_tree(st, branch_node_id);
    return lower_return_current_block(st, branch_node_id);
}

OirStatus oir_lower_semantic_ast_return(
    const OirSemanticAstArtifactView* a, OirLoweringWorkspace* w,
    uint8_t* output, size_t capacity, size_t* written) {
    OirAstNodeRecord root, fn, body, statement;
    OirAstTlvField f;
    OirSemanticAnnotation fa;
    OirSemanticType ft;
    const uint8_t *fname=NULL, *declared=NULL, *type_name=NULL;
    uint32_t fname_len=0u, declared_len=0u, decl=0u, body_id=0u;
    uint32_t statement_id=0u;
    uint32_t type_off=0u, type_len, function_type_id;
    int hard=0;
    ExprState st;
    OirModuleInput m;
    OirStatus s;
    if (written) *written=0u;
    if (!a || !w || !output || !written || !w->types || !w->functions ||
        !w->blocks || !w->values || !w->instructions || !w->operands || !w->strings)
        return OIR_STATUS_INVALID_ARGUMENT;
    if (w->type_capacity<2u || !w->function_capacity || !w->block_capacity ||
        !w->value_capacity || w->instruction_capacity<2u || w->operand_capacity<2u ||
        !w->string_capacity) return OIR_STATUS_BUFFER_TOO_SMALL;
    if ((s=node(&a->ast,a->ast.root_node_id,&root))!=OIR_STATUS_OK || root.node_kind_id!=N_PROGRAM ||
        (s=ref(&a->ast,root.node_id,1u,&decl))!=OIR_STATUS_OK || !decl ||
        (s=node(&a->ast,decl,&fn))!=OIR_STATUS_OK || fn.node_kind_id!=N_FUNCTION || fn.next_node_id)
        return OIR_STATUS_INVALID_SECTION;
    if ((s=field(&a->ast,fn.node_id,3u,OIR_AST_VALUE_BOOL,&f))!=OIR_STATUS_OK || f.value[0])
        return OIR_STATUS_INVALID_SECTION;
    if ((s=ref(&a->ast,fn.node_id,6u,&decl))!=OIR_STATUS_OK || decl)
        return OIR_STATUS_INVALID_SECTION;
    if ((s=string_ref(&a->ast,fn.node_id,1u,&fname,&fname_len))!=OIR_STATUS_OK ||
        !fname_len || fname_len>w->string_capacity ||
        (s=string_ref(&a->ast,fn.node_id,2u,&declared,&declared_len))!=OIR_STATUS_OK ||
        !declared_len) return OIR_STATUS_INVALID_SECTION;
    if ((s=ref(&a->ast,fn.node_id,7u,&body_id))!=OIR_STATUS_OK || !body_id ||
        (s=node(&a->ast,body_id,&body))!=OIR_STATUS_OK || body.node_kind_id!=N_BLOCK ||
        (s=ref(&a->ast,body_id,1u,&statement_id))!=OIR_STATUS_OK || !statement_id ||
        (s=node(&a->ast,statement_id,&statement))!=OIR_STATUS_OK)
        return OIR_STATUS_INVALID_SECTION;
    {
        uint32_t lo=0u, hi=a->annotations.record_count;
        while (lo<hi) {
            uint32_t mid=lo+(hi-lo)/2u;
            OirSemanticAnnotation x;
            if (oir_semantic_annotations_get(&a->annotations,mid,&x)!=OIR_STATUS_OK)
                return OIR_STATUS_INVALID_SECTION;
            if (x.node_id<fn.node_id) lo=mid+1u; else hi=mid;
        }
        if (lo>=a->annotations.record_count ||
            oir_semantic_annotations_get(&a->annotations,lo,&fa)!=OIR_STATUS_OK ||
            fa.node_id!=fn.node_id || !fa.resolved_type_id) return OIR_STATUS_INVALID_SECTION;
    }
    {
        uint32_t i;
        int found=0;
        for (i=0u;i<a->types.record_count;i++) {
            const uint8_t* n=NULL;
            if (oir_semantic_types_get(&a->types,i,&ft,&n)!=OIR_STATUS_OK)
                return OIR_STATUS_INVALID_SECTION;
            if (ft.type_id==fa.resolved_type_id) { type_name=n; found=1; break; }
        }
        if (!found || !type_name) return OIR_STATUS_INVALID_SECTION;
    }
    type_len=declared_len;
    if (declared[0]=='!') { hard=1; type_off=1u; --type_len; }
    if (type_len!=ft.name_length || memcmp(declared+type_off,type_name,type_len) ||
        hard!=((ft.flags&OIR_SEMANTIC_TYPE_FLAG_HARD)!=0u)) return OIR_STATUS_INVALID_SECTION;
    memset(w->types,0,(size_t)w->type_capacity*sizeof(w->types[0]));
    if ((s=oir_type_map_primitive(&ft,type_name,ft.name_length,1u,&w->types[0]))!=OIR_STATUS_OK)
        return s;
    memset(&st,0,sizeof(st));
    st.artifact=a; st.ws=w; st.type_name=type_name; st.type_length=ft.name_length;
    st.current_block_id=1u;
    st.block_count=1u;
    if (w->types[0].kind==OIR_MODULE_TYPE_BOOLEAN) st.bool_type_id=1u;
    if ((s=begin_block(&st,1u))!=OIR_STATUS_OK) return s;

    /* Straight-line scalar locals are promoted to SSA values; reassignment creates a new version. */
    while (statement.node_kind_id == N_VAR_DECL ||
           statement.node_kind_id == N_ASSIGNMENT) {
        if (statement.node_kind_id == N_ASSIGNMENT) {
            if (lower_assignment_statement(&st,statement.node_id)!=OIR_STATUS_OK)
                return OIR_STATUS_INVALID_SECTION;
            if (!statement.next_node_id) return OIR_STATUS_INVALID_SECTION;
            statement_id=statement.next_node_id;
            if ((s=node(&a->ast,statement_id,&statement))!=OIR_STATUS_OK)
                return OIR_STATUS_INVALID_SECTION;
            continue;
        }
        OirSemanticAnnotation declaration_annotation;
        uint32_t initializer_id=0u, local_value_id=0u;
        if (!w->locals || st.local_count >= w->local_capacity ||
            (s=ref(&a->ast,statement.node_id,7u,&initializer_id))!=OIR_STATUS_OK ||
            !initializer_id ||
            (s=annotation_get(a,statement.node_id,&declaration_annotation))!=OIR_STATUS_OK ||
            !declaration_annotation.resolved_symbol_id ||
            !declaration_annotation.resolved_type_id ||
            (s=check_type(&st,statement.node_id))!=OIR_STATUS_OK ||
            (s=symbol_matches_declaration(a,declaration_annotation.resolved_symbol_id,
                statement.node_id,declaration_annotation.resolved_type_id))!=OIR_STATUS_OK)
            return OIR_STATUS_INVALID_SECTION;
        if ((s=lower_expr(&st,initializer_id,&local_value_id,0u))!=OIR_STATUS_OK)
            return s;
        w->locals[st.local_count].symbol_id=declaration_annotation.resolved_symbol_id;
        w->locals[st.local_count].value_id=local_value_id;
        ++st.local_count;
        if (!statement.next_node_id) return OIR_STATUS_INVALID_SECTION;
        statement_id=statement.next_node_id;
        if ((s=node(&a->ast,statement_id,&statement))!=OIR_STATUS_OK)
            return OIR_STATUS_INVALID_SECTION;
    }
    if (statement.next_node_id) return OIR_STATUS_INVALID_SECTION;
    if (statement.node_kind_id==N_IF) {
        if ((s=lower_if_tree(&st,statement.node_id))!=OIR_STATUS_OK) return s;
    } else if (statement.node_kind_id==N_RETURN) {
        if ((s=lower_return_current_block(&st,statement.node_id))!=OIR_STATUS_OK) return s;
    } else {
        return OIR_STATUS_INVALID_SECTION;
    }

    function_type_id = st.bool_type_id == 2u ? 3u : 2u;
    if (w->type_capacity < function_type_id) return OIR_STATUS_BUFFER_TOO_SMALL;
    w->types[function_type_id - 1u].id=function_type_id;
    w->types[function_type_id - 1u].kind=OIR_MODULE_TYPE_FUNCTION;
    memcpy(w->strings,fname,fname_len);
    w->functions[0].id=1u; w->functions[0].name_offset=0u;
    w->functions[0].name_length=fname_len;
    w->functions[0].function_type_id=function_type_id;
    w->functions[0].entry_block_id=1u;
    memset(&m,0,sizeof(m));
    m.types=w->types; m.type_count=function_type_id;
    m.functions=w->functions; m.function_count=1u;
    m.blocks=w->blocks; m.block_count=st.block_count; m.values=w->values; m.value_count=st.values;
    m.instructions=w->instructions; m.instruction_count=st.instructions;
    m.operands=w->operands; m.operand_count=st.operands;
    m.strings=w->strings; m.strings_length=fname_len;
    return oir_module_write(output,capacity,&m,written);
}
