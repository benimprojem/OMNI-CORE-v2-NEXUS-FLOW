#include "oir_ast_frontend_index.h"

#include <stdint.h>
#include <string.h>

static size_t pointer_hash(const ASTNode* node) {
    uintptr_t value = (uintptr_t)node;
    value ^= value >> 17;
    value *= (uintptr_t)0xed5ad4bbu;
    value ^= value >> 11;
    value *= (uintptr_t)0xac4c1b51u;
    value ^= value >> 15;
    return (size_t)value;
}

static OirStatus insert_node(
    OirAstPointerIndexEntry* entries,
    size_t capacity,
    const ASTNode* node,
    uint32_t node_id
) {
    size_t mask = capacity - 1u;
    size_t slot = pointer_hash(node) & mask;
    size_t probes;
    for (probes = 0; probes < capacity; ++probes) {
        OirAstPointerIndexEntry* entry = &entries[slot];
        if (!entry->node) {
            entry->node = node;
            entry->node_id = node_id;
            return OIR_STATUS_OK;
        }
        if (entry->node == node) return OIR_STATUS_INVALID_SECTION;
        slot = (slot + 1u) & mask;
    }
    return OIR_STATUS_BUFFER_TOO_SMALL;
}

uint32_t oir_ast_frontend_index_lookup(
    const OirAstPointerIndexEntry* entries,
    size_t capacity,
    const ASTNode* node
) {
    size_t mask, slot, probes;
    if (!entries || !node || capacity == 0u || (capacity & (capacity - 1u)) != 0u) {
        return 0u;
    }
    mask = capacity - 1u;
    slot = pointer_hash(node) & mask;
    for (probes = 0; probes < capacity; ++probes) {
        const OirAstPointerIndexEntry* entry = &entries[slot];
        if (!entry->node) return 0u;
        if (entry->node == node) return entry->node_id;
        slot = (slot + 1u) & mask;
    }
    return 0u;
}

const ASTNode* oir_ast_frontend_child_at(const ASTNode* node, uint16_t field_id) {
    switch (node->type) {
        case AST_PROGRAM:
            return field_id == 1u ? ((const ASTProgram*)node)->declarations : NULL;
        case AST_BLOCK:
        case AST_ASM_BLOCK:
            return field_id == 1u ? ((const ASTBlock*)node)->statements : NULL;
        case AST_VAR_DECL: {
            const ASTVarDecl* n = (const ASTVarDecl*)node;
            if (field_id == 7u) return n->initializer;
            if (field_id == 8u) return n->inline_type;
            return NULL;
        }
        case AST_FUNC_DECL:
        case AST_EXTERN_DECL: {
            const ASTFuncDecl* n = (const ASTFuncDecl*)node;
            if (field_id == 6u) return n->params;
            if (field_id == 7u) return n->body;
            return NULL;
        }
        case AST_STRUCT_DECL:
        case AST_UNION_DECL:
            return field_id == 6u ? ((const ASTStructDecl*)node)->fields : NULL;
        case AST_ENUM_DECL:
            return field_id == 2u ? ((const ASTEnumDecl*)node)->variants : NULL;
        case AST_ENUM_VARIANT:
            return field_id == 2u ? ((const ASTEnumVariant*)node)->data : NULL;
        case AST_GROUP_DECL:
            return field_id == 4u ? ((const ASTGroupDecl*)node)->members : NULL;
        case AST_METHOD:
            return field_id == 2u ? ((const ASTMethod*)node)->implementation : NULL;
        case AST_BINARY_EXPR:
        case AST_ASSIGNMENT:
        case AST_INDEX_ACCESS: {
            const ASTBinaryExpr* n = (const ASTBinaryExpr*)node;
            if (field_id == 2u) return n->left;
            if (field_id == 3u) return n->right;
            return NULL;
        }
        case AST_UNARY_EXPR:
        case AST_POSTFIX_EXPR:
        case AST_RETURN:
            return field_id == 2u ? ((const ASTUnaryExpr*)node)->operand : NULL;
        case AST_CAST: {
            const ASTCast* cast = (const ASTCast*)node;
            if (field_id == 1u) return cast->value;
            if (field_id == 2u) return cast->target_type;
            return NULL;
        }
        case AST_FUNC_CALL:
        case AST_SPAWN:
        case AST_ECHO:
        case AST_SEND:
        case AST_RECEIVE:
        case AST_DONE:
        case AST_AREA:
        case AST_ZONE:
        case AST_ADDR:
        case AST_LEN:
        case AST_PEEK:
        case AST_POKE:
        case AST_INB:
        case AST_OUTB:
        case AST_IRQ:
        case AST_INTR:
        case AST_REG:
        case AST_PRNT:
        case AST_PRMT:
        case AST_WRITE:
        case AST_FWRITE:
        case AST_TYPEOF:
        case AST_SIZEOF:
        case AST_IS_OK:
        case AST_IS_ERR:
        case AST_IS_SOME:
        case AST_IS_NONE:
        case AST_IS_NULL:
        case AST_EXIT:
        case AST_SWAP:
        case AST_DATE:
        case AST_DATENOW:
        case AST_TIME:
        case AST_CLOCK:
        case AST_FREE:
        case AST_ZONE_SYNC:
        case AST_ZONE_VIEW:
        case AST_ZONE_LOCK: {
            const ASTCall* n = (const ASTCall*)node;
            if (field_id == 1u) return n->callee;
            if (field_id == 2u) return n->args;
            return NULL;
        }
        case AST_MEMBER_ACCESS:
            return field_id == 2u ? ((const ASTMemberAccess*)node)->object : NULL;
        case AST_TUPLE:
            return field_id == 1u ? ((const ASTTuple*)node)->elements : NULL;
        case AST_ARRAY_LITERAL:
            return field_id == 1u ? ((const ASTArrayLiteral*)node)->elements : NULL;
        case AST_SELECT: { const ASTSelect* n=(const ASTSelect*)node; return field_id==1u?n->selector:(field_id==2u?n->arms:NULL); }
        case AST_SELECT_ARM: { const ASTSelectArm* n=(const ASTSelectArm*)node; return field_id==1u?n->pattern:(field_id==2u?n->body:NULL); }
        case AST_SCAN: { const ASTScan* n=(const ASTScan*)node; return field_id==1u?n->subject:(field_id==2u?n->arms:NULL); }
        case AST_SCAN_ARM: { const ASTScanArm* n=(const ASTScanArm*)node; return field_id==1u?n->pattern:(field_id==2u?n->body:NULL); }
        case AST_PATTERN_CONSTRUCTOR: return field_id==2u?((const ASTPatternConstructor*)node)->bindings:NULL;
        case AST_RULESET_DECL: { const ASTRulesetDecl* n=(const ASTRulesetDecl*)node; return field_id==2u?n->params:(field_id==3u?n->rules:NULL); }
        case AST_RULE_BARRIER: { const ASTRuleBarrier* n=(const ASTRuleBarrier*)node; return field_id==1u?n->condition:(field_id==2u?n->message:NULL); }
        case AST_RULE_TRIGGER: { const ASTRuleTrigger* n=(const ASTRuleTrigger*)node; return field_id==1u?n->condition:(field_id==2u?n->action:NULL); }
        case AST_RULE_ALWAYS: return field_id==1u?((const ASTRuleAlways*)node)->action:NULL;
        case AST_RULE_DEFAULT: return field_id==1u?((const ASTRuleDefault*)node)->action:NULL;
        case AST_APPLY_RULES: return field_id==2u?((const ASTApplyRules*)node)->args:NULL;
        case AST_ON_BLOCK: return field_id==3u?((const ASTOnBlock*)node)->body:NULL;
        case AST_LAMBDA: { const ASTLambda* n=(const ASTLambda*)node; return field_id==1u?n->params:(field_id==2u?n->body:NULL); }
        case AST_RELOCATE: { const ASTRelocate* n=(const ASTRelocate*)node; return field_id==1u?n->source:(field_id==2u?n->target:NULL); }
        case AST_UNROLL: { const ASTUnroll* n=(const ASTUnroll*)node; return field_id==1u?n->count:(field_id==2u?n->loop:NULL); }
        case AST_IF: {
            const ASTIf* n = (const ASTIf*)node;
            if (field_id == 1u) return n->condition;
            if (field_id == 2u) return n->then_branch;
            if (field_id == 3u) return n->else_branch;
            return NULL;
        }
        case AST_LOOP: {
            const ASTLoop* n = (const ASTLoop*)node;
            switch (field_id) {
                case 2: return n->init;
                case 3: return n->condition;
                case 4: return n->increment;
                case 5: return n->body;
                case 6: return n->collection;
                case 7: return n->variable;
                default: return NULL;
            }
        }
        case AST_NEXUS_CAPTURE:
        case AST_LISTEN:
            return field_id == 2u ? ((const ASTNexusCapture*)node)->expr : NULL;
        case AST_NEXUS_PIPE:
        case AST_NEXUS_FLOW: {
            const ASTNexusFlow* n = (const ASTNexusFlow*)node;
            if (field_id == 2u) return n->left;
            if (field_id == 3u) return n->right;
            if (field_id == 6u) return n->catch_block;
            return NULL;
        }
        case AST_NEXUS_LABEL:
            return field_id == 2u ? ((const ASTNexusLabel*)node)->statement : NULL;
        case AST_NEXUS_HALT:
            return field_id == 1u ? ((const ASTHalt*)node)->expr : NULL;
        case AST_NEXUS_INTENT:
            return field_id == 2u ? ((const ASTIntent*)node)->target : NULL;
        case AST_UNPACK:
            return field_id == 1u ? ((const ASTUnpack*)node)->expr : NULL;
        case AST_DEFER:
            return field_id == 1u ? ((const ASTDefer*)node)->body : NULL;
        case AST_THREAD_BLOCK: {
            const ASTThreadBlock* n = (const ASTThreadBlock*)node;
            if (field_id == 2u) return n->body;
            if (field_id == 3u) return n->mode;
            if (field_id == 4u) return n->timeout;
            return NULL;
        }
        default:
            return NULL;
    }
}

uint16_t oir_ast_frontend_child_field_count(ASTNodeType type) {
    switch (type) {
        case AST_PROGRAM: case AST_BLOCK: case AST_ASM_BLOCK:
        case AST_TUPLE: case AST_ARRAY_LITERAL: case AST_NEXUS_HALT: case AST_UNPACK: case AST_DEFER:
            return 1u;
        case AST_STRUCT_DECL: case AST_UNION_DECL: return 6u;
        case AST_GROUP_DECL: return 4u;
        case AST_ENUM_DECL: case AST_ENUM_VARIANT: case AST_METHOD:
        case AST_MEMBER_ACCESS: case AST_NEXUS_CAPTURE: case AST_LISTEN:
        case AST_NEXUS_LABEL: case AST_NEXUS_INTENT:
            return 2u;
        case AST_VAR_DECL: return 8u;
        case AST_FUNC_DECL: case AST_EXTERN_DECL: return 7u;
        case AST_BINARY_EXPR: case AST_ASSIGNMENT: case AST_INDEX_ACCESS:
        case AST_IF: return 3u;
        case AST_UNARY_EXPR: case AST_POSTFIX_EXPR: case AST_RETURN:
        case AST_CAST: return 2u;
        case AST_FUNC_CALL: case AST_SPAWN: case AST_ECHO: case AST_SEND:
        case AST_RECEIVE: case AST_DONE: case AST_AREA: case AST_ZONE:
        case AST_ADDR: case AST_LEN: case AST_PEEK: case AST_POKE:
        case AST_INB: case AST_OUTB: case AST_IRQ: case AST_INTR: case AST_REG:
        case AST_PRNT: case AST_PRMT: case AST_WRITE: case AST_FWRITE:
        case AST_TYPEOF: case AST_SIZEOF: case AST_IS_OK: case AST_IS_ERR:
        case AST_IS_SOME: case AST_IS_NONE: case AST_IS_NULL: case AST_EXIT:
        case AST_SWAP: case AST_DATE: case AST_DATENOW: case AST_TIME:
        case AST_CLOCK: case AST_FREE: case AST_ZONE_SYNC: case AST_ZONE_VIEW:
        case AST_ZONE_LOCK: return 2u;
        case AST_LOOP: return 7u;
        case AST_NEXUS_PIPE: case AST_NEXUS_FLOW: return 6u;
        case AST_THREAD_BLOCK: return 4u;
        case AST_SELECT: case AST_SCAN: case AST_SCAN_ARM: case AST_PATTERN_CONSTRUCTOR: return 2u;
        case AST_SELECT_ARM: case AST_LAMBDA: return 3u;
        case AST_PATTERN_BINDING: return 0u;
        case AST_RULESET_DECL: case AST_ON_BLOCK: return 3u;
        case AST_RULE_BARRIER: case AST_RULE_TRIGGER: case AST_APPLY_RULES:
        case AST_RELOCATE: case AST_UNROLL: return 2u;
        case AST_RULE_ALWAYS: case AST_RULE_DEFAULT: return 1u;
        default: return 0u;
    }
}

typedef struct BuildContext {
    OirAstPointerIndexEntry* entries;
    size_t capacity;
    uint32_t count;
} BuildContext;

static OirStatus visit_node(
    BuildContext* context,
    const ASTNode* node,
    uint32_t depth
) {
    uint16_t field, field_count;
    uint16_t kind;
    OirStatus status;
    if (!node) return OIR_STATUS_OK;
    if (depth > OIR_AST_NODE_DEPTH_MAX) return OIR_STATUS_INVALID_SECTION;
    if (context->count >= OIR_AST_NODE_COUNT_MAX) return OIR_STATUS_INVALID_SECTION;
    if (node->line < 0 || node->col < 0 ||
        ((node->line == 0) != (node->col == 0))) return OIR_STATUS_INVALID_SECTION;
    kind = oir_ast_node_id_from_frontend(node->type);
    if (kind == 0u) return OIR_STATUS_INVALID_SECTION;
    if (context->count >= context->capacity / 2u) return OIR_STATUS_BUFFER_TOO_SMALL;
    status = insert_node(context->entries, context->capacity, node, context->count + 1u);
    if (status != OIR_STATUS_OK) return status;
    ++context->count;

    field_count = oir_ast_frontend_child_field_count(node->type);
    for (field = 1u; field <= field_count; ++field) {
        const ASTNode* child = oir_ast_frontend_child_at(node, field);
        if (child) {
            status = visit_node(context, child, depth + 1u);
            if (status != OIR_STATUS_OK) return status;
        }
    }
    return visit_node(context, node->next, depth);
}

OirStatus oir_ast_frontend_index_build(
    const ASTNode* root,
    OirAstPointerIndexEntry* entries,
    size_t entry_capacity,
    uint32_t* out_node_count
) {
    BuildContext context;
    OirStatus status;
    if (out_node_count) *out_node_count = 0u;
    if (!root || !entries || !out_node_count ||
        entry_capacity == 0u ||
        (entry_capacity & (entry_capacity - 1u)) != 0u ||
        root->type != AST_PROGRAM || root->next != NULL ||
        entry_capacity < 2u ||
        entry_capacity > SIZE_MAX / sizeof(*entries)) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    memset(entries, 0, entry_capacity * sizeof(*entries));
    context.entries = entries;
    context.capacity = entry_capacity;
    context.count = 0u;
    status = visit_node(&context, root, 1u);
    if (status != OIR_STATUS_OK) {
        memset(entries, 0, entry_capacity * sizeof(*entries));
        return status;
    }
    *out_node_count = context.count;
    return OIR_STATUS_OK;
}
