#include "oir_ast_frontend_writer.h"

#include "oir_ast_field_schema.h"
#include "oir_token_map.h"

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct AstEmitContext {
    const OirAstPointerIndexEntry* index_entries;
    size_t index_capacity;
    uint32_t node_count;
    uint8_t* field_data;
    size_t field_capacity;
    size_t field_used;
    uint8_t* string_table;
    size_t string_capacity;
    size_t string_used;
    OirAstNodeRecord* records;
    uint32_t emitted_nodes;
    int measure_only;
} AstEmitContext;

static void put_u16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}
static void put_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void put_u64(uint8_t* p, uint64_t v) {
    put_u32(p, (uint32_t)v); put_u32(p + 4, (uint32_t)(v >> 32));
}

static int valid_utf8(const uint8_t* s, size_t n) {
    size_t i = 0;
    while (i < n) {
        uint8_t a = s[i];
        uint32_t cp;
        size_t extra;
        if (a == 0u) return 0;
        if (a <= 0x7fu) { ++i; continue; }
        if (a >= 0xc2u && a <= 0xdfu) { cp = a & 0x1fu; extra = 1u; }
        else if (a >= 0xe0u && a <= 0xefu) { cp = a & 0x0fu; extra = 2u; }
        else if (a >= 0xf0u && a <= 0xf4u) { cp = a & 0x07u; extra = 3u; }
        else return 0;
        if (extra > n - i - 1u) return 0;
        for (size_t j = 1u; j <= extra; ++j) {
            uint8_t b = s[i + j];
            if ((b & 0xc0u) != 0x80u) return 0;
            cp = (cp << 6) | (uint32_t)(b & 0x3fu);
        }
        if ((extra == 1u && cp < 0x80u) ||
            (extra == 2u && cp < 0x800u) ||
            (extra == 3u && cp < 0x10000u) ||
            (cp >= 0xd800u && cp <= 0xdfffu) || cp > 0x10ffffu) return 0;
        i += extra + 1u;
    }
    return 1;
}

static OirStatus bounded_string_length(const char* s, uint32_t* out_length) {
    size_t n;
    if (!s || !out_length) return OIR_STATUS_INVALID_ARGUMENT;
    for (n = 0; n <= OIR_AST_STRING_MAX; ++n) {
        if (s[n] == '\0') {
            if (!valid_utf8((const uint8_t*)s, n)) return OIR_STATUS_INVALID_SECTION;
            *out_length = (uint32_t)n;
            return OIR_STATUS_OK;
        }
    }
    return OIR_STATUS_INVALID_SECTION;
}

static OirStatus append_tlv(
    AstEmitContext* ctx, uint16_t* last_field_id, uint16_t field_id,
    uint8_t type, const uint8_t* value, uint32_t value_length
) {
    size_t total = (size_t)OIR_AST_TLV_HEADER_SIZE + value_length;
    if (field_id == 0u || field_id <= *last_field_id ||
        ctx->field_used > UINT32_MAX || total > UINT32_MAX - ctx->field_used) {
        return OIR_STATUS_OVERFLOW;
    }
    if (!ctx->measure_only) {
        OirAstTlvWriter writer;
        OirStatus status;
        if (ctx->field_used > ctx->field_capacity ||
            total > ctx->field_capacity - ctx->field_used) return OIR_STATUS_BUFFER_TOO_SMALL;
        oir_ast_tlv_writer_init(&writer, ctx->field_data + ctx->field_used,
            ctx->field_capacity - ctx->field_used);
        status = oir_ast_tlv_write(&writer, field_id, type, 0u, value, value_length);
        if (status != OIR_STATUS_OK) return status;
        ctx->field_used += writer.used;
    } else {
        ctx->field_used += total;
    }
    *last_field_id = field_id;
    return OIR_STATUS_OK;
}

static OirStatus emit_optional_span_value(AstEmitContext* c, uint16_t* last,
    uint16_t id, uint8_t type, const uint8_t* value, uint32_t length) {
    size_t offset=c->field_used;
    OirStatus status=append_tlv(c,last,id,type,value,length);
    if(status!=OIR_STATUS_OK)return status;
    if(!c->measure_only)c->field_data[offset+3u]=OIR_AST_TLV_OPTIONAL;
    return OIR_STATUS_OK;
}
static OirStatus emit_optional_span_u64(AstEmitContext* c,uint16_t* last,uint16_t id,uint64_t v){
    uint8_t b[8];put_u64(b,v);return emit_optional_span_value(c,last,id,OIR_AST_VALUE_U64,b,8u);
}
static OirStatus emit_optional_span_u32(AstEmitContext* c,uint16_t* last,uint16_t id,uint32_t v){
    uint8_t b[4];put_u32(b,v);return emit_optional_span_value(c,last,id,OIR_AST_VALUE_U32,b,4u);
}

static OirStatus emit_null(AstEmitContext* c, uint16_t* last, uint16_t id) {
    return append_tlv(c, last, id, OIR_AST_VALUE_NULL, NULL, 0u);
}
static OirStatus emit_bool(AstEmitContext* c, uint16_t* last, uint16_t id, int v) {
    uint8_t b;
    if (v != 0 && v != 1) return OIR_STATUS_INVALID_SECTION;
    b = (uint8_t)v;
    return append_tlv(c, last, id, OIR_AST_VALUE_BOOL, &b, 1u);
}
static OirStatus emit_u32(AstEmitContext* c, uint16_t* last, uint16_t id, uint32_t v) {
    uint8_t b[4]; put_u32(b, v);
    return append_tlv(c, last, id, OIR_AST_VALUE_U32, b, 4u);
}
static OirStatus emit_i32(AstEmitContext* c, uint16_t* last, uint16_t id, int64_t v) {
    uint8_t b[4];
    if (v < INT32_MIN || v > INT32_MAX) return OIR_STATUS_INVALID_SECTION;
    put_u32(b, (uint32_t)(int32_t)v);
    return append_tlv(c, last, id, OIR_AST_VALUE_I32, b, 4u);
}
static OirStatus emit_i64(AstEmitContext* c, uint16_t* last, uint16_t id, int64_t v) {
    uint8_t b[8]; put_u64(b, (uint64_t)v);
    return append_tlv(c, last, id, OIR_AST_VALUE_I64, b, 8u);
}
static OirStatus emit_f64(AstEmitContext* c, uint16_t* last, uint16_t id, double v) {
    uint64_t bits;
    uint8_t b[8];
    if (sizeof(double) != 8u || DBL_MANT_DIG != 53 || DBL_MAX_EXP != 1024) {
        return OIR_STATUS_INVALID_SECTION;
    }
    memcpy(&bits, &v, sizeof(bits));
    put_u64(b, bits);
    return append_tlv(c, last, id, OIR_AST_VALUE_F64, b, 8u);
}
static OirStatus emit_token(AstEmitContext* c, uint16_t* last, uint16_t id, OmniTokenType token) {
    uint16_t stable = oir_token_id_from_omni(token);
    uint8_t b[2];
    if (stable == 0u) return OIR_STATUS_INVALID_SECTION;
    put_u16(b, stable);
    return append_tlv(c, last, id, OIR_AST_VALUE_TOKEN_ID, b, 2u);
}
static OirStatus emit_node_ref(
    AstEmitContext* c, uint16_t* last, uint16_t field_id, const ASTNode* node
) {
    uint32_t id = node ? oir_ast_frontend_index_lookup(
        c->index_entries, c->index_capacity, node) : 0u;
    uint8_t b[4];
    if (node && (id == 0u || id > c->node_count)) return OIR_STATUS_INVALID_SECTION;
    put_u32(b, id);
    return append_tlv(c, last, field_id, OIR_AST_VALUE_NODE_REF, b, 4u);
}
static OirStatus emit_optional_node_ref(
    AstEmitContext* c, uint16_t* last, uint16_t field_id, const ASTNode* node
) {
    size_t offset = c->field_used;
    OirStatus status = emit_node_ref(c, last, field_id, node);
    if (status != OIR_STATUS_OK) return status;
    if (!c->measure_only) c->field_data[offset + 3u] = OIR_AST_TLV_OPTIONAL;
    return OIR_STATUS_OK;
}

static OirStatus emit_string(
    AstEmitContext* c, uint16_t* last, uint16_t field_id, const char* s
) {
    uint32_t length, offset;
    uint8_t b[8];
    OirStatus status;
    if (!s) return emit_null(c, last, field_id);
    status = bounded_string_length(s, &length);
    if (status != OIR_STATUS_OK) return status;
    if (c->string_used > UINT32_MAX || length > UINT32_MAX - c->string_used) {
        return OIR_STATUS_OVERFLOW;
    }
    offset = (uint32_t)c->string_used;
    if (!c->measure_only) {
        if (c->string_used > c->string_capacity ||
            length > c->string_capacity - c->string_used) return OIR_STATUS_BUFFER_TOO_SMALL;
        if (length) memcpy(c->string_table + c->string_used, s, length);
    }
    c->string_used += length;
    put_u32(b, offset);
    put_u32(b + 4, length);
    return append_tlv(c, last, field_id, OIR_AST_VALUE_STRING_REF, b, 8u);
}

static OirStatus emit_node_fields(AstEmitContext* c, const ASTNode* node) {
    uint16_t last = 0u;
    OirStatus s = OIR_STATUS_OK;
#define STR(id, value) do { s = emit_string(c, &last, (id), (value)); if (s != OIR_STATUS_OK) return s; } while (0)
#define REF(id, value) do { s = emit_node_ref(c, &last, (id), (const ASTNode*)(value)); if (s != OIR_STATUS_OK) return s; } while (0)
#define BOOL(id, value) do { s = emit_bool(c, &last, (id), (value)); if (s != OIR_STATUS_OK) return s; } while (0)
#define I32(id, value) do { s = emit_i32(c, &last, (id), (int64_t)(value)); if (s != OIR_STATUS_OK) return s; } while (0)
#define U32(id, value) do { s = emit_u32(c, &last, (id), (uint32_t)(value)); if (s != OIR_STATUS_OK) return s; } while (0)
#define TOKEN(id, value) do { s = emit_token(c, &last, (id), (value)); if (s != OIR_STATUS_OK) return s; } while (0)
    switch (node->type) {
        case AST_PROGRAM: REF(1, ((const ASTProgram*)node)->declarations); break;
        case AST_BLOCK: case AST_ASM_BLOCK: REF(1, ((const ASTBlock*)node)->statements); break;
        case AST_FASTEXEC_BLOCK: REF(1,((const ASTFastExecBlock*)node)->statements); break;
        case AST_ASM_LABEL: {
            const ASTAsmLabel* a=(const ASTAsmLabel*)node;
            if(!a->source_text || strlen(a->source_text)!=a->source_length) return OIR_STATUS_INVALID_SECTION;
            STR(1,a->label); STR(2,a->source_text); break;
        }
        case AST_VAR_DECL: {
            const ASTVarDecl* n = (const ASTVarDecl*)node;
            STR(1,n->name); STR(2,n->type_name);
            BOOL(3,n->is_const); BOOL(4,n->is_hard_typed);
            BOOL(5,n->is_public); BOOL(6,n->is_exported);
            REF(7,n->initializer); REF(8,n->inline_type); break;
        }
        case AST_FUNC_DECL: case AST_EXTERN_DECL: {
            const ASTFuncDecl* n = (const ASTFuncDecl*)node;
            STR(1,n->name); STR(2,n->return_type);
            BOOL(3,n->is_extern); BOOL(4,n->is_public); BOOL(5,n->is_exported);
            REF(6,n->params); REF(7,n->body);
            if (n->return_array_type) {
                s = emit_optional_node_ref(c, &last, 8u, n->return_array_type);
                if (s != OIR_STATUS_OK) return s;
            }
            break;
        }
        case AST_STRUCT_DECL: case AST_UNION_DECL: {
            const ASTStructDecl* n = (const ASTStructDecl*)node;
            STR(1,n->name); BOOL(2,n->is_packed); BOOL(3,n->is_union);
            BOOL(4,n->is_public); BOOL(5,n->is_exported); REF(6,n->fields); break;
        }
        case AST_ENUM_DECL: {
            const ASTEnumDecl* n = (const ASTEnumDecl*)node;
            STR(1,n->name); REF(2,n->variants); break;
        }
        case AST_NEWTYPE_DECL: {
            const ASTNewTypeDecl* n = (const ASTNewTypeDecl*)node;
            STR(1,n->name); STR(2,n->target_type); break;
        }
        case AST_ENUM_VARIANT: {
            const ASTEnumVariant* n = (const ASTEnumVariant*)node;
            STR(1,n->name); REF(2,n->data); break;
        }
        case AST_GROUP_DECL: {
            const ASTGroupDecl* n = (const ASTGroupDecl*)node;
            STR(1,n->name); BOOL(2,n->is_public); BOOL(3,n->is_exported); REF(4,n->members); break;
        }
        case AST_METHOD: {
            const ASTMethod* n = (const ASTMethod*)node;
            STR(1,n->public_name); REF(2,n->implementation); break;
        }
        case AST_LITERAL: {
            const ASTLiteral* n = (const ASTLiteral*)node;
            int literal_kind = n->lit_type;
            int64_t integer_value = (int64_t)n->int_value;
            double float_value = n->float_value;
            if (n->type == TOKEN_NUMBER && n->string_value && n->string_value[0]) {
                char* end = NULL;
                errno = 0;
                if (strchr(n->string_value, '.') || strchr(n->string_value, 'e') ||
                    strchr(n->string_value, 'E')) {
                    float_value = strtod(n->string_value, &end);
                    if (errno == ERANGE || end == n->string_value || *end != '\0')
                        return OIR_STATUS_INVALID_SECTION;
                    literal_kind = 1;
                } else {
                    long long parsed = strtoll(n->string_value, &end, 10);
                    if (errno == ERANGE || end == n->string_value || *end != '\0')
                        return OIR_STATUS_INVALID_SECTION;
                    integer_value = (int64_t)parsed;
                    literal_kind = 0;
                }
            } else if (n->type == TOKEN_STRING) {
                literal_kind = 2;
            } else if (n->type == TOKEN_CHAR) {
                literal_kind = 3;
            } else if (n->type == TOKEN_TRUE || n->type == TOKEN_FALSE) {
                literal_kind = 4;
            }
            if (literal_kind < 0 || literal_kind > 4) return OIR_STATUS_INVALID_SECTION;
            TOKEN(1,n->type); U32(2,literal_kind);
            s = emit_i64(c,&last,3,integer_value); if (s != OIR_STATUS_OK) return s;
            s = emit_f64(c,&last,4,float_value); if (s != OIR_STATUS_OK) return s;
            STR(5,n->string_value); break;
        }
        case AST_IDENTIFIER: STR(1,((const ASTIdentifier*)node)->name); break;
        case AST_BINARY_EXPR: case AST_ASSIGNMENT: {
            const ASTBinaryExpr* n = (const ASTBinaryExpr*)node;
            TOKEN(1,n->op); REF(2,n->left); REF(3,n->right); break;
        }
        case AST_INDEX_ACCESS: {
            const ASTIndexAccess* n = (const ASTIndexAccess*)node;
            REF(1,n->target); REF(2,n->index); break;
        }
        case AST_UNARY_EXPR: case AST_POSTFIX_EXPR: case AST_RETURN: {
            const ASTUnaryExpr* n = (const ASTUnaryExpr*)node;
            TOKEN(1,n->op); REF(2,n->operand); break;
        }
        case AST_CAST: {
            const ASTCast* n = (const ASTCast*)node;
            REF(1,n->value); REF(2,n->target_type); break;
        }
        case AST_FUNC_CALL: case AST_SPAWN: case AST_ECHO: case AST_SEND:
        case AST_RECEIVE: case AST_DONE: case AST_AREA: case AST_ZONE:
        case AST_ADDR: case AST_LEN: case AST_PEEK: case AST_POKE: case AST_INB:
        case AST_OUTB: case AST_IRQ: case AST_INTR: case AST_REG: case AST_PRNT:
        case AST_PRMT: case AST_WRITE: case AST_FWRITE: case AST_TYPEOF:
        case AST_SIZEOF: case AST_IS_OK: case AST_IS_ERR: case AST_IS_SOME:
        case AST_IS_NONE: case AST_IS_NULL: case AST_EXIT: case AST_SWAP:
        case AST_DATE: case AST_DATENOW: case AST_TIME: case AST_CLOCK: case AST_FREE:
        case AST_ZONE_SYNC: case AST_ZONE_VIEW: case AST_ZONE_LOCK: {
            const ASTCall* n = (const ASTCall*)node;
            REF(1,n->callee); REF(2,n->args); break;
        }
        case AST_MEMBER_ACCESS: {
            const ASTMemberAccess* n = (const ASTMemberAccess*)node;
            STR(1,n->member_name); REF(2,n->object); break;
        }
        case AST_TUPLE: REF(1,((const ASTTuple*)node)->elements); break;
        case AST_ARRAY_LITERAL: REF(1,((const ASTArrayLiteral*)node)->elements); break;
        case AST_STRUCT_LITERAL: { const ASTStructLiteral* n=(const ASTStructLiteral*)node; STR(1,n->type_name); REF(2,n->fields); break; }
        case AST_STRUCT_FIELD_INIT: { const ASTStructFieldInit* n=(const ASTStructFieldInit*)node; STR(1,n->name); REF(2,n->value); break; }
        case AST_ARRAY_TYPE: REF(1,((const ASTArrayType*)node)->dimensions); break;
        case AST_ARRAY_DIMENSION: REF(1,((const ASTArrayDimension*)node)->length); break;
        case AST_VARIADIC_PARAM: break;
        case AST_NAMED_ARG: { const ASTNamedArg* n=(const ASTNamedArg*)node; STR(1,n->name); REF(2,n->value); break; }
        case AST_MUST_DECL: { const ASTMustDecl* n=(const ASTMustDecl*)node; STR(1,n->name); REF(2,n->source); break; }
        case AST_BLOCK_HALT: break;
        case AST_IF: {
            const ASTIf* n = (const ASTIf*)node;
            REF(1,n->condition); REF(2,n->then_branch); REF(3,n->else_branch); break;
        }
        case AST_LOOP: {
            const ASTLoop* n = (const ASTLoop*)node;
            if (n->loop_type < 0 || n->loop_type > 3) return OIR_STATUS_INVALID_SECTION;
            I32(1,n->loop_type); REF(2,n->init); REF(3,n->condition);
            REF(4,n->increment); REF(5,n->body); REF(6,n->collection);
            REF(7,n->variable); break;
        }
        case AST_NEXUS_CAPTURE: case AST_LISTEN: {
            const ASTNexusCapture* n = (const ASTNexusCapture*)node;
            STR(1,n->handler_name); REF(2,n->expr); break;
        }
        case AST_NEXUS_PIPE: case AST_NEXUS_FLOW: {
            const ASTNexusFlow* n = (const ASTNexusFlow*)node;
            TOKEN(1,n->op); REF(2,n->left); REF(3,n->right);
            I32(4,n->retry_count); I32(5,n->retry_delay); REF(6,n->catch_block); break;
        }
        case AST_NEXUS_LABEL: {
            const ASTNexusLabel* n = (const ASTNexusLabel*)node;
            STR(1,n->label_name); REF(2,n->statement); break;
        }
        case AST_NEXUS_JUMP: STR(1,((const ASTNexusJump*)node)->label_name); break;
        case AST_NEXUS_HALT: REF(1,((const ASTHalt*)node)->expr); break;
        case AST_NEXUS_INTENT: {
            const ASTIntent* n = (const ASTIntent*)node;
            STR(1,n->intent_name); REF(2,n->target); break;
        }
        case AST_AI_LOGIC: STR(1,((const ASTAiLogic*)node)->query); break;
        case AST_UNPACK: REF(1,((const ASTUnpack*)node)->expr); break;
        case AST_PANIC: STR(1,((const ASTPanic*)node)->message); break;
        case AST_DEFER: REF(1,((const ASTDefer*)node)->body); break;
        case AST_THREAD_BLOCK: {
            const ASTThreadBlock* n = (const ASTThreadBlock*)node;
            STR(1,n->name); REF(2,n->body); REF(3,n->mode); REF(4,n->timeout); break;
        }
        case AST_USE: {
            const ASTUse* n = (const ASTUse*)node;
            STR(1,n->module_name); STR(2,n->alias); break;
        }
        case AST_SELECT: { const ASTSelect* n=(const ASTSelect*)node; REF(1,n->selector); REF(2,n->arms); break; }
        case AST_SELECT_ARM: { const ASTSelectArm* n=(const ASTSelectArm*)node; REF(1,n->pattern); REF(2,n->body); BOOL(3,n->is_default); break; }
        case AST_SCAN: { const ASTScan* n=(const ASTScan*)node; REF(1,n->subject); REF(2,n->arms); break; }
        case AST_SCAN_ARM: { const ASTScanArm* n=(const ASTScanArm*)node; REF(1,n->pattern); REF(2,n->body); break; }
        case AST_PATTERN_BINDING: STR(1,((const ASTPatternBinding*)node)->name); break;
        case AST_PATTERN_CONSTRUCTOR: { const ASTPatternConstructor* n=(const ASTPatternConstructor*)node; STR(1,n->name); REF(2,n->bindings); break; }
        case AST_RULESET_DECL: { const ASTRulesetDecl* n=(const ASTRulesetDecl*)node; STR(1,n->name); REF(2,n->params); REF(3,n->rules); break; }
        case AST_RULE_BARRIER: { const ASTRuleBarrier* n=(const ASTRuleBarrier*)node; REF(1,n->condition); REF(2,n->message); break; }
        case AST_RULE_TRIGGER: { const ASTRuleTrigger* n=(const ASTRuleTrigger*)node; REF(1,n->condition); REF(2,n->action); break; }
        case AST_RULE_ALWAYS: REF(1,((const ASTRuleAlways*)node)->action); break;
        case AST_RULE_DEFAULT: REF(1,((const ASTRuleDefault*)node)->action); break;
        case AST_APPLY_RULES: { const ASTApplyRules* n=(const ASTApplyRules*)node; STR(1,n->ruleset_name); REF(2,n->args); break; }
        case AST_ON_BLOCK: { const ASTOnBlock* n=(const ASTOnBlock*)node; STR(1,n->ruleset_name); STR(2,n->event_name); REF(3,n->body); break; }
        case AST_LAMBDA: { const ASTLambda* n=(const ASTLambda*)node; REF(1,n->params); REF(2,n->body); BOOL(3,n->body_is_expression); break; }
        case AST_RELOCATE: { const ASTRelocate* n=(const ASTRelocate*)node; REF(1,n->source); REF(2,n->target); break; }
        case AST_UNROLL: { const ASTUnroll* n=(const ASTUnroll*)node; REF(1,n->count); REF(2,n->loop); break; }
        case AST_BREAK: case AST_CONTINUE: break;
        default: return OIR_STATUS_INVALID_SECTION;
    }
#undef STR
#undef REF
#undef BOOL
#undef I32
#undef U32
    if(node->source_span.has_location){
        const OccSourceSpan* span=&node->source_span;
        if(span->end_offset<span->start_offset || !span->start_line || !span->start_column ||
           !span->end_line || !span->end_column || node->line!=(int)span->start_line ||
           node->col!=(int)span->start_column)return OIR_STATUS_INVALID_SECTION;
        s=emit_optional_span_u64(c,&last,0xFFF0u,span->start_offset);if(s!=OIR_STATUS_OK)return s;
        s=emit_optional_span_u64(c,&last,0xFFF1u,span->end_offset);if(s!=OIR_STATUS_OK)return s;
        s=emit_optional_span_u32(c,&last,0xFFF2u,span->end_line);if(s!=OIR_STATUS_OK)return s;
        s=emit_optional_span_u32(c,&last,0xFFF3u,span->end_column);if(s!=OIR_STATUS_OK)return s;
    }
    return s;
}

static OirStatus emit_tree(AstEmitContext* c, const ASTNode* node, uint32_t depth) {
    uint32_t id;
    uint16_t field, field_count, kind;
    OirStatus status;
    if (!node) return OIR_STATUS_OK;
    if (depth > OIR_AST_NODE_DEPTH_MAX || c->emitted_nodes >= c->node_count ||
        node->line < 0 || node->col < 0 ||
        ((node->line == 0) != (node->col == 0))) {
        return OIR_STATUS_INVALID_SECTION;
    }
    id = oir_ast_frontend_index_lookup(c->index_entries,c->index_capacity,node);
    if (id == 0u || id != c->emitted_nodes + 1u) return OIR_STATUS_INVALID_SECTION;
    kind = oir_ast_node_id_from_frontend(node->type);
    if (kind == 0u) return OIR_STATUS_INVALID_SECTION;
    ++c->emitted_nodes;

    if (!c->measure_only) {
        OirAstNodeRecord* record = &c->records[id - 1u];
        memset(record,0,sizeof(*record));
        record->node_id = id;
        record->node_kind_id = kind;
        record->start_line = (uint32_t)node->line;
        record->start_column = (uint32_t)node->col;
        record->next_node_id = node->next ?
            oir_ast_frontend_index_lookup(c->index_entries,c->index_capacity,node->next) : 0u;
        if (node->next && record->next_node_id == 0u) return OIR_STATUS_INVALID_SECTION;
        record->field_data_offset = (uint32_t)c->field_used;
    }
    {
        size_t before = c->field_used;
        status = emit_node_fields(c,node);
        if (status != OIR_STATUS_OK) return status;
        if (!c->measure_only) {
            size_t length = c->field_used - before;
            if (length > UINT32_MAX) return OIR_STATUS_OVERFLOW;
            c->records[id - 1u].field_data_length = (uint32_t)length;
        }
    }

    field_count = oir_ast_frontend_child_field_count(node->type);
    for (field=1u; field<=field_count; ++field) {
        const ASTNode* child = oir_ast_frontend_child_at(node,field);
        if (child) {
            status = emit_tree(c,child,depth+1u);
            if (status != OIR_STATUS_OK) return status;
        }
    }
    return emit_tree(c,node->next,depth);
}

static OirStatus run_measure(
    const ASTNode* root,
    const OirAstPointerIndexEntry* entries,
    size_t entry_capacity,
    uint32_t node_count,
    OirAstFrontendRequirements* req
) {
    AstEmitContext c;
    uint64_t payload_size;
    OirStatus status;
    if (!root || root->type != AST_PROGRAM || root->next != NULL ||
        !entries || entry_capacity < 2u ||
        (entry_capacity & (entry_capacity-1u)) != 0u ||
        node_count == 0u || node_count > OIR_AST_NODE_COUNT_MAX ||
        !req || oir_ast_frontend_index_lookup(entries,entry_capacity,root) != 1u) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    memset(&c,0,sizeof(c));
    c.index_entries=entries; c.index_capacity=entry_capacity;
    c.node_count=node_count; c.measure_only=1;
    status=emit_tree(&c,root,1u);
    if(status!=OIR_STATUS_OK) return status;
    if(c.emitted_nodes!=node_count || c.field_used>UINT32_MAX || c.string_used>UINT32_MAX) {
        return OIR_STATUS_INVALID_SECTION;
    }
    payload_size=(uint64_t)OIR_AST_PAYLOAD_HEADER_SIZE+
        (uint64_t)node_count*OIR_AST_NODE_RECORD_SIZE+
        (uint64_t)c.field_used+(uint64_t)c.string_used;
#if SIZE_MAX < UINT64_MAX
    if(payload_size>(uint64_t)SIZE_MAX) return OIR_STATUS_OVERFLOW;
#endif
    req->node_count=node_count;
    req->field_data_length=(uint32_t)c.field_used;
    req->string_table_length=(uint32_t)c.string_used;
    req->payload_size=(size_t)payload_size;
    return OIR_STATUS_OK;
}

OirStatus oir_ast_frontend_measure(
    const ASTNode* root,
    const OirAstPointerIndexEntry* index_entries,
    size_t index_capacity,
    uint32_t node_count,
    OirAstFrontendRequirements* out_requirements
) {
    if(out_requirements) memset(out_requirements,0,sizeof(*out_requirements));
    return run_measure(root,index_entries,index_capacity,node_count,out_requirements);
}

OirStatus oir_ast_frontend_write_payload(
    const ASTNode* root,
    const OirAstPointerIndexEntry* index_entries,
    size_t index_capacity,
    uint32_t node_count,
    OirAstNodeRecord* records,
    size_t records_capacity,
    uint8_t* field_data,
    size_t field_capacity,
    uint8_t* string_table,
    size_t string_capacity,
    uint8_t* output,
    size_t output_capacity,
    size_t* out_written,
    OirAstFrontendRequirements* out_requirements
) {
    OirAstFrontendRequirements req;
    AstEmitContext c;
    OirStatus status;
    OirAstNodeTableView view;
    if(out_written) *out_written=0u;
    if(out_requirements) memset(out_requirements,0,sizeof(*out_requirements));
    if(!out_written || !records || !output) return OIR_STATUS_INVALID_ARGUMENT;
    status=run_measure(root,index_entries,index_capacity,node_count,&req);
    if(status!=OIR_STATUS_OK) return status;
    if(out_requirements) *out_requirements=req;
    if(records_capacity<node_count || field_capacity<req.field_data_length ||
        string_capacity<req.string_table_length || output_capacity<req.payload_size ||
        (req.field_data_length && !field_data) ||
        (req.string_table_length && !string_table)) return OIR_STATUS_BUFFER_TOO_SMALL;

    memset(records,0,(size_t)node_count*sizeof(*records));
    memset(&c,0,sizeof(c));
    c.index_entries=index_entries; c.index_capacity=index_capacity;
    c.node_count=node_count; c.records=records;
    c.field_data=field_data; c.field_capacity=field_capacity;
    c.string_table=string_table; c.string_capacity=string_capacity;
    c.measure_only=0;
    status=emit_tree(&c,root,1u);
    if(status!=OIR_STATUS_OK) return status;
    if(c.emitted_nodes!=node_count || c.field_used!=req.field_data_length ||
        c.string_used!=req.string_table_length) return OIR_STATUS_INVALID_SECTION;

    status=oir_ast_node_table_write(output,output_capacity,records,node_count,1u,
        field_data,req.field_data_length,string_table,req.string_table_length,out_written);
    if(status!=OIR_STATUS_OK) return status;
    status=oir_ast_node_table_open(output,*out_written,&view);
    if(status==OIR_STATUS_OK) status=oir_ast_field_schema_validate(&view);
    if(status!=OIR_STATUS_OK) {
        *out_written=0u;
        return status;
    }
    return OIR_STATUS_OK;
}
