#include "ast.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* Test-only, process-global allocator fault injection. */
static size_t ast_test_fail_at = 0u;
static size_t ast_test_alloc_count = 0u;

void ast_test_fail_allocation_at(size_t allocation_index) {
    ast_test_alloc_count = 0u;
    ast_test_fail_at = allocation_index;
}

void ast_test_reset_allocation_failure(void) {
    ast_test_alloc_count = 0u;
    ast_test_fail_at = 0u;
}

size_t ast_test_get_allocation_count(void) {
    return ast_test_alloc_count;
}

static int ast_test_should_fail_allocation(void) {
    ast_test_alloc_count++;
    return ast_test_fail_at != 0u &&
        ast_test_alloc_count == ast_test_fail_at;
}

static void* ast_malloc(size_t size) {
    return ast_test_should_fail_allocation() ? NULL : malloc(size);
}

static void* ast_calloc(size_t count, size_t size) {
    return ast_test_should_fail_allocation() ? NULL : calloc(count, size);
}

static void* ast_realloc(void* memory, size_t size) {
    return ast_test_should_fail_allocation() ? NULL : realloc(memory, size);
}

// AST Düğüm tiplerine göre bellek boyutunu belirleyen harita
static size_t get_node_size(ASTNodeType type) {
    switch (type) {
        // --- İfadeler ---
        case AST_BINARY_EXPR: 
        case AST_ASSIGNMENT:
            return sizeof(ASTBinaryExpr);

        case AST_UNARY_EXPR:
        case AST_RETURN:
        case AST_POSTFIX_EXPR:
            return sizeof(ASTUnaryExpr);
        case AST_CAST:
            return sizeof(ASTCast);
            
        case AST_TUPLE: return sizeof(ASTTuple);
        case AST_SELECT: return sizeof(ASTSelect);
        case AST_SELECT_ARM: return sizeof(ASTSelectArm);
        case AST_SCAN: return sizeof(ASTScan);
        case AST_SCAN_ARM: return sizeof(ASTScanArm);
        case AST_PATTERN_BINDING: return sizeof(ASTPatternBinding);
        case AST_PATTERN_CONSTRUCTOR: return sizeof(ASTPatternConstructor);
        case AST_RULESET_DECL: return sizeof(ASTRulesetDecl);
        case AST_RULE_BARRIER: return sizeof(ASTRuleBarrier);
        case AST_RULE_TRIGGER: return sizeof(ASTRuleTrigger);
        case AST_RULE_ALWAYS: return sizeof(ASTRuleAlways);
        case AST_RULE_DEFAULT: return sizeof(ASTRuleDefault);
        case AST_APPLY_RULES: return sizeof(ASTApplyRules);
        case AST_ON_BLOCK: return sizeof(ASTOnBlock);
        case AST_LAMBDA: return sizeof(ASTLambda);
        case AST_RELOCATE: return sizeof(ASTRelocate);
        case AST_UNROLL: return sizeof(ASTUnroll);
        case AST_ARRAY_LITERAL: return sizeof(ASTArrayLiteral);
        case AST_STRUCT_LITERAL: return sizeof(ASTStructLiteral);
        case AST_STRUCT_FIELD_INIT: return sizeof(ASTStructFieldInit);
        case AST_TYPE_EXPR: return sizeof(ASTTypeExpr);
        case AST_ARRAY_TYPE: return sizeof(ASTArrayType);
        case AST_ARRAY_DIMENSION: return sizeof(ASTArrayDimension);
        case AST_VARIADIC_PARAM: return sizeof(ASTVariadicParam);
        case AST_NAMED_ARG: return sizeof(ASTNamedArg);
        case AST_MUST_DECL: return sizeof(ASTMustDecl);
        case AST_FASTEXEC_BLOCK: return sizeof(ASTFastExecBlock);
        case AST_ASM_LABEL: return sizeof(ASTAsmLabel);

        case AST_LITERAL:     return sizeof(ASTLiteral);
        case AST_IDENTIFIER:  return sizeof(ASTIdentifier);
        
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
        case AST_ZONE_LOCK:
            return sizeof(ASTCall);
            
        case AST_MEMBER_ACCESS: return sizeof(ASTMemberAccess);
        case AST_INDEX_ACCESS: return sizeof(ASTIndexAccess);


        // --- Bildirimler ---
        case AST_VAR_DECL:    return sizeof(ASTVarDecl);
        case AST_FUNC_DECL:   
        case AST_EXTERN_DECL: return sizeof(ASTFuncDecl);
        case AST_STRUCT_DECL: 
        case AST_UNION_DECL:  return sizeof(ASTStructDecl);
        case AST_ENUM_DECL:   return sizeof(ASTEnumDecl);
        case AST_NEWTYPE_DECL: return sizeof(ASTNewTypeDecl);
        case AST_ENUM_VARIANT: return sizeof(ASTEnumVariant);
        case AST_GROUP_DECL:   return sizeof(ASTGroupDecl);
        case AST_METHOD:       return sizeof(ASTMethod);
        case AST_USE:          return sizeof(ASTUse);
        case AST_DEFER:        return sizeof(ASTDefer);
        case AST_THREAD_BLOCK: return sizeof(ASTThreadBlock);

        // --- Akış Kontrolü ---
        case AST_BLOCK:       
        case AST_PROGRAM:     // Program kökü de bir blok gibidir (statement listesi)
        case AST_ASM_BLOCK:
            return sizeof(ASTBlock);

        case AST_IF:          return sizeof(ASTIf);
        case AST_LOOP:        return sizeof(ASTLoop);

        // --- Nexus Flow ---
        case AST_NEXUS_CAPTURE: 
        case AST_LISTEN:      // !(listen) bir capture işlemi gibidir
            return sizeof(ASTNexusCapture);

        case AST_NEXUS_FLOW:
        case AST_NEXUS_PIPE:
            return sizeof(ASTNexusFlow);

        case AST_NEXUS_LABEL: return sizeof(ASTNexusLabel);
        case AST_NEXUS_JUMP:  return sizeof(ASTNexusJump);
        case AST_NEXUS_HALT:  return sizeof(ASTHalt);
        case AST_NEXUS_INTENT: return sizeof(ASTIntent);
        case AST_AI_LOGIC:    return sizeof(ASTAiLogic);
        case AST_UNPACK:      return sizeof(ASTUnpack);
        case AST_PANIC:       return sizeof(ASTPanic);

        // Payload taşımayan AST türleri: yalnızca ASTNode başlığı saklanır.
        case AST_BREAK:
        case AST_CONTINUE:
        case AST_BLOCK_HALT:
            return sizeof(ASTNode);

        default: return sizeof(ASTNode);
    }
}

ASTNode* ast_create(ASTNodeType type) {
    size_t size;

    /* Reject values outside the append-only public AST kind inventory. */
    if ((int)type < (int)AST_PROGRAM || (int)type > (int)AST_TYPE_EXPR) {
        return NULL;
    }
    size = get_node_size(type);
    ASTNode* node = (ASTNode*)ast_malloc(size);
    if (node) {
        memset(node, 0, size); // Belleği sıfırla (tüm pointerlar NULL olur)
        node->type = type;
        node->next = NULL;
    }
    return node;
}

int ast_set_source_span(ASTNode* node, const OccSourceSpan* span) {
    if (!node) return 0;
    if (!span) {
        memset(&node->source_span, 0, sizeof(node->source_span));
        node->line = 0;
        node->col = 0;
        return 1;
    }
    if (span->has_location && span->end_offset < span->start_offset) return 0;
    node->source_span = *span;
    if (span->has_location) {
        node->line = (int)span->start_line;
        node->col = (int)span->start_column;
    }
    return 1;
}

// --- AST Printer ---

static void print_indent(int level) {
    for (int i = 0; i < level; i++) printf("  ");
}

static void ast_print_recursive(ASTNode* node, int level) {
    while (node) {
        print_indent(level);

        switch (node->type) {
            case AST_PROGRAM:
                printf("[PROGRAM]\n");
                ast_print_recursive(((ASTProgram*)node)->declarations, level + 1);
                break;
            case AST_BLOCK:
            case AST_ASM_BLOCK:
                printf("[BLOCK]\n");
                ast_print_recursive(((ASTBlock*)node)->statements, level + 1);
                break;
            case AST_FASTEXEC_BLOCK:
                printf("[FASTEXEC]\n");
                ast_print_recursive(((ASTFastExecBlock*)node)->statements, level + 1);
                break;
            case AST_ASM_LABEL: {
                ASTAsmLabel* a=(ASTAsmLabel*)node;
                printf("[ASM LABEL: %s] (%zu bytes)\n",a->label?a->label:"<invalid>",a->source_length);
                break;
            }
            case AST_IF: {
                ASTIf* conditional = (ASTIf*)node;
                printf("[IF]\n");
                print_indent(level + 1); printf("[CONDITION]\n");
                ast_print_recursive(conditional->condition, level + 2);
                print_indent(level + 1); printf("[THEN]\n");
                ast_print_recursive(conditional->then_branch, level + 2);
                if (conditional->else_branch) {
                    print_indent(level + 1); printf("[ELSE / ELSIF]\n");
                    ast_print_recursive(conditional->else_branch, level + 2);
                }
                break;
            }
            case AST_VAR_DECL: {
                ASTVarDecl* decl = (ASTVarDecl*)node;
                printf("VAR_DECL: %s (%s)", decl->name, decl->is_const ? "CONST" : "MUT");
                if (decl->type_name) {
                    printf(" Type: %s%s", decl->is_hard_typed ? "!" : "", decl->type_name);
                }
                if (decl->inline_type) {
                    printf(" [INLINE TYPE]\n");
                    ast_print_recursive(decl->inline_type, level + 1);
                    print_indent(level);
                }
                printf("\n");
                if (decl->initializer) {
                    ast_print_recursive(decl->initializer, level + 1);
                }
                break;
            }
            case AST_FUNC_DECL:
        case AST_EXTERN_DECL: {
                ASTFuncDecl* func = (ASTFuncDecl*)node;
                printf("FUNC_DECL: %s", func->name);
                if (func->return_type) printf(" > %s", func->return_type);
                printf("\n");
                
                if (func->params) {
                    print_indent(level + 1);
                    printf("[PARAMS]\n");
                    ast_print_recursive(func->params, level + 2);
                }
                
                if (func->body) ast_print_recursive(func->body, level + 1);
                break;
            }
            case AST_RETURN: {
                ASTUnaryExpr* ret = (ASTUnaryExpr*)node;
                printf("RETURN\n");
                if (ret->operand) ast_print_recursive(ret->operand, level + 1);
                break;
            }
            case AST_UNARY_EXPR: {
                ASTUnaryExpr* unary = (ASTUnaryExpr*)node;
                printf("UNARY_OP: %s\n", token_type_to_string(unary->op));
                ast_print_recursive(unary->operand, level + 1);
                break;
            }
            case AST_CAST: {
                ASTCast* cast = (ASTCast*)node;
                printf("CAST\n");
                ast_print_recursive(cast->value, level + 1);
                ast_print_recursive(cast->target_type, level + 1);
                break;
            }
            case AST_POSTFIX_EXPR: {
                ASTUnaryExpr* post = (ASTUnaryExpr*)node;
                printf("POSTFIX_OP: %s\n", token_type_to_string(post->op));
                ast_print_recursive(post->operand, level + 1);
                break;
            }
            case AST_BINARY_EXPR: {
                ASTBinaryExpr* bin = (ASTBinaryExpr*)node;
                printf("BINARY_OP: %s\n", token_type_to_string(bin->op));
                ast_print_recursive(bin->left, level + 1);
                ast_print_recursive(bin->right, level + 1);
                break;
            }
            case AST_ASSIGNMENT: {
                ASTBinaryExpr* assign = (ASTBinaryExpr*)node;
                printf("ASSIGNMENT: %s\n", token_type_to_string(assign->op));
                ast_print_recursive(assign->left, level + 1);
                ast_print_recursive(assign->right, level + 1);
                break;
            }
            case AST_LITERAL: {
                ASTLiteral* lit = (ASTLiteral*)node;
                printf("LITERAL: %s\n", lit->string_value);
                break;
            }
            case AST_IDENTIFIER: {
                ASTIdentifier* id = (ASTIdentifier*)node;
                printf("IDENTIFIER: %s\n", id->name);
                break;
            }
            case AST_MEMBER_ACCESS: {
                ASTMemberAccess* mem = (ASTMemberAccess*)node;
                printf("MEMBER_ACCESS: .%s\n", mem->member_name);
                ast_print_recursive(mem->object, level + 1);
                break;
            }
            case AST_INDEX_ACCESS: {
                ASTIndexAccess* index = (ASTIndexAccess*)node;
                printf("INDEX_ACCESS\n");
                ast_print_recursive(index->target, level + 1);
                ast_print_recursive(index->index, level + 1);
                break;
            }
            case AST_TUPLE: {
                ASTTuple* tuple = (ASTTuple*)node;
                printf("TUPLE\n");
                ast_print_recursive(tuple->elements, level + 1);
                break;
            }
            case AST_STRUCT_LITERAL: {
                ASTStructLiteral* n = (ASTStructLiteral*)node;
                printf("STRUCT_LITERAL %s\\n", n->type_name ? n->type_name : "<null>");
                ast_print_recursive(n->fields, level + 1);
                break;
            }
            case AST_STRUCT_FIELD_INIT: {
                ASTStructFieldInit* n = (ASTStructFieldInit*)node;
                printf("STRUCT_FIELD_INIT %s\\n", n->name ? n->name : "<null>");
                ast_print_recursive(n->value, level + 1);
                break;
            }
            case AST_TYPE_EXPR: {
                ASTTypeExpr* n=(ASTTypeExpr*)node;
                printf("TYPE_EXPR kind=%u flags=%u name=%s\\n",n->kind,n->flags,n->name?n->name:"<none>");
                ast_print_recursive(n->child,level+1);
                ast_print_recursive(n->params,level+1);
                ast_print_recursive(n->dimensions,level+1);
                break;
            }
            case AST_ARRAY_LITERAL: {
                ASTArrayLiteral* array = (ASTArrayLiteral*)node;
                printf("ARRAY_LITERAL\n");
                ast_print_recursive(array->elements, level + 1);
                break;
            }
            case AST_ARRAY_TYPE: {
                ASTArrayType* array_type = (ASTArrayType*)node;
                printf("ARRAY_TYPE\n");
                ast_print_recursive(array_type->dimensions, level + 1);
                break;
            }
            case AST_ARRAY_DIMENSION: {
                ASTArrayDimension* dimension = (ASTArrayDimension*)node;
                printf("ARRAY_DIMENSION%s\n", dimension->length ? "" : " DYNAMIC");
                ast_print_recursive(dimension->length, level + 1);
                break;
            }
            case AST_SELECT: { ASTSelect* n=(ASTSelect*)node; printf("SELECT\n"); ast_print_recursive(n->selector,level+1); ast_print_recursive(n->arms,level+1); break; }
            case AST_SELECT_ARM: { ASTSelectArm* n=(ASTSelectArm*)node; printf("SELECT_ARM%s\n",n->is_default?" DEFAULT":""); ast_print_recursive(n->pattern,level+1); ast_print_recursive(n->body,level+1); break; }
            case AST_SCAN: { ASTScan* n=(ASTScan*)node; printf("SCAN\n"); ast_print_recursive(n->subject,level+1); ast_print_recursive(n->arms,level+1); break; }
            case AST_SCAN_ARM: { ASTScanArm* n=(ASTScanArm*)node; printf("SCAN_ARM%s\n",n->pattern?"":" ELSE"); ast_print_recursive(n->pattern,level+1); ast_print_recursive(n->body,level+1); break; }
            case AST_PATTERN_BINDING: printf("PATTERN_BINDING: %s\n",((ASTPatternBinding*)node)->name); break;
            case AST_PATTERN_CONSTRUCTOR: { ASTPatternConstructor* n=(ASTPatternConstructor*)node; printf("PATTERN_CONSTRUCTOR: %s\n",n->name); ast_print_recursive(n->bindings,level+1); break; }
            case AST_RULESET_DECL: { ASTRulesetDecl* n=(ASTRulesetDecl*)node; printf("RULESET: %s\n",n->name); ast_print_recursive(n->params,level+1); ast_print_recursive(n->rules,level+1); break; }
            case AST_RULE_BARRIER: { ASTRuleBarrier* n=(ASTRuleBarrier*)node; printf("RULE_BARRIER\n"); ast_print_recursive(n->condition,level+1); ast_print_recursive(n->message,level+1); break; }
            case AST_RULE_TRIGGER: { ASTRuleTrigger* n=(ASTRuleTrigger*)node; printf("RULE_TRIGGER\n"); ast_print_recursive(n->condition,level+1); ast_print_recursive(n->action,level+1); break; }
            case AST_RULE_ALWAYS: printf("RULE_ALWAYS\n"); ast_print_recursive(((ASTRuleAlways*)node)->action,level+1); break;
            case AST_RULE_DEFAULT: printf("RULE_DEFAULT\n"); ast_print_recursive(((ASTRuleDefault*)node)->action,level+1); break;
            case AST_APPLY_RULES: { ASTApplyRules* n=(ASTApplyRules*)node; printf("APPLY_RULES: %s\n",n->ruleset_name); ast_print_recursive(n->args,level+1); break; }
            case AST_ON_BLOCK: { ASTOnBlock* n=(ASTOnBlock*)node; printf("ON_BLOCK: %s.%s\n",n->ruleset_name,n->event_name); ast_print_recursive(n->body,level+1); break; }
            case AST_LAMBDA: { ASTLambda* n=(ASTLambda*)node; printf("LAMBDA%s\n",n->body_is_expression?" EXPR":" BLOCK"); ast_print_recursive(n->params,level+1); ast_print_recursive(n->body,level+1); break; }
            case AST_RELOCATE: { ASTRelocate* n=(ASTRelocate*)node; printf("RELOCATE\n"); ast_print_recursive(n->source,level+1); ast_print_recursive(n->target,level+1); break; }
            case AST_UNROLL: { ASTUnroll* n=(ASTUnroll*)node; printf("UNROLL\n"); ast_print_recursive(n->count,level+1); ast_print_recursive(n->loop,level+1); break; }
            case AST_NEXUS_CAPTURE:
            case AST_LISTEN: {
                ASTNexusCapture* cap = (ASTNexusCapture*)node;
                printf("NEXUS_CAPTURE: %s\n", cap->handler_name);
                if (cap->expr) ast_print_recursive(cap->expr, level + 1);
                break;
            }
            case AST_NEXUS_FLOW:
            case AST_NEXUS_PIPE: {
                ASTNexusFlow* flow = (ASTNexusFlow*)node;
                printf("NEXUS_FLOW: %s\n", token_type_to_string(flow->op));
                if (flow->retry_count > 0) {
                    printf("  [ROLLING] Count: %d, Delay: %dms\n", flow->retry_count, flow->retry_delay);
                }
                ast_print_recursive(flow->left, level + 1);
                ast_print_recursive(flow->right, level + 1);
                break;
            }
            case AST_LOOP: {
                ASTLoop* loop = (ASTLoop*)node;
                printf("LOOP (Type: %d)\n", loop->loop_type);
                if (loop->init) ast_print_recursive(loop->init, level + 1);
                if (loop->condition) ast_print_recursive(loop->condition, level + 1);
                if (loop->increment) ast_print_recursive(loop->increment, level + 1);
                if (loop->variable) ast_print_recursive(loop->variable, level + 1);
                if (loop->collection) ast_print_recursive(loop->collection, level + 1);
                if (loop->body) ast_print_recursive(loop->body, level + 1);
                break;
            }
            case AST_NEXUS_LABEL: {
                ASTNexusLabel* lbl = (ASTNexusLabel*)node;
                printf("LABEL: %s\n", lbl->label_name);
                if (lbl->statement) ast_print_recursive(lbl->statement, level + 1);
                break;
            }
            case AST_NEXUS_JUMP: {
                ASTNexusJump* jmp = (ASTNexusJump*)node;
                printf("JUMP: %s\n", jmp->label_name);
                break;
            }
            case AST_STRUCT_DECL:
            case AST_UNION_DECL: {
                ASTStructDecl* s = (ASTStructDecl*)node;
                printf("%s: %s %s\n", s->is_union ? "UNION" : "STRUCT", s->name, s->is_packed ? "(PACKED)" : "");
                if (s->fields) {
                    print_indent(level + 1);
                    printf("[FIELDS]\n");
                    ast_print_recursive(s->fields, level + 2);
                }
                break;
            }
            case AST_ENUM_DECL: {
                ASTEnumDecl* e = (ASTEnumDecl*)node;
                printf("ENUM: %s\n", e->name);
                if (e->variants) ast_print_recursive(e->variants, level + 1);
                break;
            }
            case AST_ENUM_VARIANT: {
                ASTEnumVariant* v = (ASTEnumVariant*)node;
                printf("VARIANT: %s\n", v->name);
                if (v->data) ast_print_recursive(v->data, level + 1);
                break;
            }
            case AST_NEWTYPE_DECL: {
                ASTNewTypeDecl* nt = (ASTNewTypeDecl*)node;
                printf("NEWTYPE: %s = %s\n", nt->name, nt->target_type);
                break;
            }
            case AST_GROUP_DECL: {
                ASTGroupDecl* g = (ASTGroupDecl*)node;
                printf("GROUP: %s\n", g->name);
                if (g->members) ast_print_recursive(g->members, level + 1);
                break;
            }
            case AST_METHOD: {
                ASTMethod* m = (ASTMethod*)node;
                printf("METHOD: %s =>\n", m->public_name);
                if (m->implementation) ast_print_recursive(m->implementation, level + 1);
                break;
            }
            case AST_NEXUS_HALT: {
                ASTHalt* h = (ASTHalt*)node;
                printf("HALT (?%s)\n", h->expr ? " [WITH EXPR]" : "");
                if (h->expr) ast_print_recursive(h->expr, level + 1);
                break;
            }
            case AST_NEXUS_INTENT: {
                ASTIntent* i = (ASTIntent*)node;
                printf("INTENT (@): %s\n", i->intent_name);
                if (i->target) ast_print_recursive(i->target, level + 1);
                break;
            }
            case AST_AI_LOGIC: {
                ASTAiLogic* ai = (ASTAiLogic*)node;
                printf("AI_QUERY (%c%c): %s\n", '?', '?', ai->query);
                break;
            }
            case AST_UNPACK: {
                ASTUnpack* u = (ASTUnpack*)node;
                printf("UNPACK (...>)\n");
                ast_print_recursive(u->expr, level + 1);
                break;
            }
            case AST_PANIC: {
                ASTPanic* p = (ASTPanic*)node;
                printf("PANIC (!!): %s\n", p->message);
                break;
            }
            case AST_USE: {
                ASTUse* u = (ASTUse*)node;
                printf("USE: %s%s%s\n", u->module_name, u->alias ? " as " : "", u->alias ? u->alias : "");
                break;
            }
            case AST_DEFER: {
                printf("DEFER\n");
                ast_print_recursive(((ASTDefer*)node)->body, level + 1);
                break;
            }
            case AST_THREAD_BLOCK: {
                ASTThreadBlock* t = (ASTThreadBlock*)node;
                printf("THREAD_BLOCK: %s >{\n", t->name);
                ast_print_recursive(t->body, level + 1);
                break;
            }
            case AST_FUNC_CALL: {
                ASTCall* call = (ASTCall*)node;
                printf("CALL\n");
                ast_print_recursive(call->callee, level + 1);
                ast_print_recursive(call->args, level + 1);
                break;
            }
            case AST_INTR: printf("INTRINSIC: intr\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_IS_ERR: printf("INTRINSIC: is_err\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_IS_SOME: printf("INTRINSIC: is_some\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_IS_NONE: printf("INTRINSIC: is_none\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_IS_NULL: printf("INTRINSIC: is_null\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_EXIT: printf("INTRINSIC: exit\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_SWAP: printf("INTRINSIC: swap\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_ZONE: printf("INTRINSIC: zone\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_DATE: printf("INTRINSIC: date\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_DATENOW: printf("INTRINSIC: datenow\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_TIME: printf("INTRINSIC: time\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_CLOCK: printf("INTRINSIC: clock\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_FREE: printf("INTRINSIC: free\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_ZONE_SYNC: printf("INTRINSIC: zone_sync\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_ZONE_VIEW: printf("INTRINSIC: zone_view\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_ZONE_LOCK: printf("INTRINSIC: zone_lock\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_BREAK: printf("BREAK\n"); break;
            case AST_CONTINUE: printf("CONTINUE\n"); break;
            case AST_BLOCK_HALT: printf("BLOCK_HALT (scope cleanup)\n"); break;
            case AST_VARIADIC_PARAM: printf("VARIADIC_PARAM\n"); break;
            case AST_NAMED_ARG: {
                ASTNamedArg* n=(ASTNamedArg*)node;
                printf("NAMED_ARG: %s\n",n->name?n->name:"<invalid>");
                ast_print_recursive(n->value,level+1);
                break;
            }
            case AST_MUST_DECL: { ASTMustDecl* n=(ASTMustDecl*)node; printf("MUST_DEPENDENCY: %s\n",n->name?n->name:"<invalid>"); ast_print_recursive(n->source,level+1); break; }
            case AST_PRNT: printf("INTRINSIC: prnt\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_PRMT: printf("INTRINSIC: prmt\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_WRITE: printf("INTRINSIC: write\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_FWRITE: printf("INTRINSIC: fwrite\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_TYPEOF: printf("INTRINSIC: typeof\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_SIZEOF: printf("INTRINSIC: sizeof\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_IS_OK: printf("INTRINSIC: is_ok\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_ECHO: printf("INTRINSIC: echo\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_SPAWN: printf("INTRINSIC: spawn\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_SEND: printf("INTRINSIC: send\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_RECEIVE: printf("INTRINSIC: receive\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_DONE: printf("INTRINSIC: done\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_AREA: printf("INTRINSIC: area\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_ADDR: printf("INTRINSIC: addr\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_LEN: printf("INTRINSIC: len\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_PEEK: printf("INTRINSIC: peek\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_POKE: printf("INTRINSIC: poke\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_INB: printf("INTRINSIC: inb\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_OUTB: printf("INTRINSIC: outb\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_IRQ: printf("INTRINSIC: irq\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            case AST_REG: printf("INTRINSIC: reg\n"); ast_print_recursive(((ASTCall*)node)->args, level + 1); break;
            default:
                printf("NODE (Type: %d)\n", node->type);
                break;
        }
        node = node->next;
    }
}

void ast_print(ASTNode* root) {
    if (!root) return;
    printf("\n--- AST VISUALIZATION ---\n");
    ast_print_recursive(root, 0);
    printf("-------------------------\n\n");
}

static void ast_destroy_recursive(ASTNode* node) {
    /*
     * Walk sibling lists iteratively. A flat source file can contain many
     * declarations; recursively following next would make cleanup consume
     * one C stack frame per sibling.
     */
    while (node) {
        ASTNode* next = node->next;
        node->next = NULL;

        switch (node->type) {
        case AST_VAR_DECL: {
            ASTVarDecl* d = (ASTVarDecl*)node;
            if (d->name) free(d->name);
            if (d->type_name) free(d->type_name);
            if (d->initializer) ast_destroy_recursive(d->initializer);
            if (d->inline_type) ast_destroy_recursive(d->inline_type);
            if (d->type_expr) ast_destroy_recursive(d->type_expr);
            break;
        }
        case AST_FUNC_DECL:
        case AST_EXTERN_DECL: {
            ASTFuncDecl* f = (ASTFuncDecl*)node;
            if (f->name) free(f->name);
            if (f->return_type) free(f->return_type);
            if (f->params) ast_destroy_recursive(f->params);
            if (f->body) ast_destroy_recursive(f->body);
            if (f->return_array_type) ast_destroy_recursive(f->return_array_type);
            if (f->linkname) free(f->linkname);
            if (f->return_type_expr) ast_destroy_recursive(f->return_type_expr);
            break;
        }
        case AST_STRUCT_DECL:
        case AST_UNION_DECL: {
            ASTStructDecl* s = (ASTStructDecl*)node;
            if (s->name) free(s->name);
            if (s->fields) ast_destroy_recursive(s->fields);
            break;
        }
        case AST_ENUM_DECL: {
            ASTEnumDecl* e = (ASTEnumDecl*)node;
            if (e->name) free(e->name);
            if (e->variants) ast_destroy_recursive(e->variants);
            break;
        }
        case AST_ENUM_VARIANT: {
            ASTEnumVariant* v = (ASTEnumVariant*)node;
            if (v->name) free(v->name);
            if (v->data) ast_destroy_recursive(v->data);
            break;
        }
        case AST_NEWTYPE_DECL: {
            ASTNewTypeDecl* nt = (ASTNewTypeDecl*)node;
            if (nt->name) free(nt->name);
            if (nt->target_type) free(nt->target_type);
            if (nt->target_type_expr) ast_destroy_recursive(nt->target_type_expr);
            break;
        }
        case AST_GROUP_DECL: {
            ASTGroupDecl* g = (ASTGroupDecl*)node;
            if (g->name) free(g->name);
            if (g->members) ast_destroy_recursive(g->members);
            break;
        }
        case AST_METHOD: {
            ASTMethod* m = (ASTMethod*)node;
            if (m->public_name) free(m->public_name);
            if (m->implementation) ast_destroy_recursive(m->implementation);
            break;
        }
        case AST_BINARY_EXPR:
        case AST_ASSIGNMENT:
        case AST_NEXUS_FLOW:
        case AST_NEXUS_PIPE: {
            ASTBinaryExpr* b = (ASTBinaryExpr*)node;
            if (b->left) ast_destroy_recursive(b->left);
            if (b->right) ast_destroy_recursive(b->right);
            // Nexus flow için ekstra catch_block field'ı kontrolü
            if (node->type == AST_NEXUS_FLOW || node->type == AST_NEXUS_PIPE) {
                 ASTNexusFlow* f = (ASTNexusFlow*)node;
                 if (f->catch_block) ast_destroy_recursive(f->catch_block);
            }
            break;
        }
        case AST_UNARY_EXPR:
        case AST_RETURN:
        case AST_POSTFIX_EXPR: {
            ASTUnaryExpr* u = (ASTUnaryExpr*)node;
            if (u->operand) ast_destroy_recursive(u->operand);
            break;
        }
        case AST_CAST: {
            ASTCast* cast = (ASTCast*)node;
            if (cast->value) ast_destroy_recursive(cast->value);
            if (cast->target_type) ast_destroy_recursive(cast->target_type);
            break;
        }
        case AST_UNPACK: {
            ASTUnpack* u = (ASTUnpack*)node;
            if (u->expr) ast_destroy_recursive(u->expr);
            break;
        }
        case AST_NEXUS_HALT: {
            ASTHalt* h = (ASTHalt*)node;
            if (h->expr) ast_destroy_recursive(h->expr);
            break;
        }
        case AST_LITERAL: {
            ASTLiteral* l = (ASTLiteral*)node;
            if (l->string_value) free(l->string_value);
            break;
        }
        case AST_IDENTIFIER: {
            ASTIdentifier* i = (ASTIdentifier*)node;
            if (i->name) free(i->name);
            break;
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
            ASTCall* c = (ASTCall*)node;
            if (c->callee) ast_destroy_recursive(c->callee);
            if (c->args) ast_destroy_recursive(c->args);
            break;
        }
        case AST_USE: {
            ASTUse* u = (ASTUse*)node;
            if (u->module_name) free(u->module_name);
            if (u->alias) free(u->alias);
            break;
        }
        case AST_DEFER: {
            ASTDefer* d = (ASTDefer*)node;
            if (d->body) ast_destroy_recursive(d->body);
            break;
        }
        case AST_THREAD_BLOCK: {
            ASTThreadBlock* t = (ASTThreadBlock*)node;
            if (t->name) free(t->name);
            if (t->body) ast_destroy_recursive(t->body);
            if (t->mode) ast_destroy_recursive(t->mode);
            if (t->timeout) ast_destroy_recursive(t->timeout);
            break;
        }
        case AST_MEMBER_ACCESS: {
            ASTMemberAccess* m = (ASTMemberAccess*)node;
            if (m->object) ast_destroy_recursive(m->object);
            if (m->member_name) free(m->member_name);
            break;
        }
        case AST_INDEX_ACCESS: {
            ASTIndexAccess* index = (ASTIndexAccess*)node;
            if (index->target) ast_destroy_recursive(index->target);
            if (index->index) ast_destroy_recursive(index->index);
            break;
        }
        case AST_TUPLE: {
            ASTTuple* t = (ASTTuple*)node;
            if (t->elements) ast_destroy_recursive(t->elements);
            break;
        }
        case AST_STRUCT_LITERAL: {
            ASTStructLiteral* n = (ASTStructLiteral*)node;
            free(n->type_name);
            if (n->fields) ast_destroy_recursive(n->fields);
            break;
        }
        case AST_STRUCT_FIELD_INIT: {
            ASTStructFieldInit* n = (ASTStructFieldInit*)node;
            free(n->name);
            if (n->value) ast_destroy_recursive(n->value);
            break;
        }
        case AST_TYPE_EXPR: {
            ASTTypeExpr* n=(ASTTypeExpr*)node;
            free(n->name);
            if(n->child)ast_destroy_recursive(n->child);
            if(n->params)ast_destroy_recursive(n->params);
            if(n->dimensions)ast_destroy_recursive(n->dimensions);
            break;
        }
        case AST_SELECT: { ASTSelect* n=(ASTSelect*)node; if(n->selector)ast_destroy_recursive(n->selector); if(n->arms)ast_destroy_recursive(n->arms); break; }
        case AST_SELECT_ARM: { ASTSelectArm* n=(ASTSelectArm*)node; if(n->pattern)ast_destroy_recursive(n->pattern); if(n->body)ast_destroy_recursive(n->body); break; }
        case AST_SCAN: { ASTScan* n=(ASTScan*)node; if(n->subject)ast_destroy_recursive(n->subject); if(n->arms)ast_destroy_recursive(n->arms); break; }
        case AST_SCAN_ARM: { ASTScanArm* n=(ASTScanArm*)node; if(n->pattern)ast_destroy_recursive(n->pattern); if(n->body)ast_destroy_recursive(n->body); break; }
        case AST_PATTERN_BINDING: free(((ASTPatternBinding*)node)->name); break;
        case AST_PATTERN_CONSTRUCTOR: { ASTPatternConstructor* n=(ASTPatternConstructor*)node; free(n->name); if(n->bindings)ast_destroy_recursive(n->bindings); break; }
        case AST_RULESET_DECL: { ASTRulesetDecl* n=(ASTRulesetDecl*)node; free(n->name); if(n->params)ast_destroy_recursive(n->params); if(n->rules)ast_destroy_recursive(n->rules); break; }
        case AST_RULE_BARRIER: { ASTRuleBarrier* n=(ASTRuleBarrier*)node; if(n->condition)ast_destroy_recursive(n->condition); if(n->message)ast_destroy_recursive(n->message); break; }
        case AST_RULE_TRIGGER: { ASTRuleTrigger* n=(ASTRuleTrigger*)node; if(n->condition)ast_destroy_recursive(n->condition); if(n->action)ast_destroy_recursive(n->action); break; }
        case AST_RULE_ALWAYS: if(((ASTRuleAlways*)node)->action)ast_destroy_recursive(((ASTRuleAlways*)node)->action); break;
        case AST_RULE_DEFAULT: if(((ASTRuleDefault*)node)->action)ast_destroy_recursive(((ASTRuleDefault*)node)->action); break;
        case AST_APPLY_RULES: { ASTApplyRules* n=(ASTApplyRules*)node; free(n->ruleset_name); if(n->args)ast_destroy_recursive(n->args); break; }
        case AST_ON_BLOCK: { ASTOnBlock* n=(ASTOnBlock*)node; free(n->ruleset_name); free(n->event_name); if(n->body)ast_destroy_recursive(n->body); break; }
        case AST_LAMBDA: { ASTLambda* n=(ASTLambda*)node; if(n->params)ast_destroy_recursive(n->params); if(n->body)ast_destroy_recursive(n->body); break; }
        case AST_RELOCATE: { ASTRelocate* n=(ASTRelocate*)node; if(n->source)ast_destroy_recursive(n->source); if(n->target)ast_destroy_recursive(n->target); break; }
        case AST_UNROLL: { ASTUnroll* n=(ASTUnroll*)node; if(n->count)ast_destroy_recursive(n->count); if(n->loop)ast_destroy_recursive(n->loop); break; }
        case AST_ARRAY_LITERAL: if(((ASTArrayLiteral*)node)->elements)ast_destroy_recursive(((ASTArrayLiteral*)node)->elements); break;
        case AST_ARRAY_TYPE: if(((ASTArrayType*)node)->dimensions)ast_destroy_recursive(((ASTArrayType*)node)->dimensions); break;
        case AST_ARRAY_DIMENSION: if(((ASTArrayDimension*)node)->length)ast_destroy_recursive(((ASTArrayDimension*)node)->length); break;
        case AST_NAMED_ARG: { ASTNamedArg* n=(ASTNamedArg*)node; free(n->name); if(n->value)ast_destroy_recursive(n->value); break; }
        case AST_MUST_DECL: { ASTMustDecl* n=(ASTMustDecl*)node; free(n->name); if(n->source)ast_destroy_recursive(n->source); break; }
        case AST_FASTEXEC_BLOCK: { ASTFastExecBlock* n=(ASTFastExecBlock*)node; if(n->statements)ast_destroy_recursive(n->statements); break; }
        case AST_ASM_LABEL: { ASTAsmLabel* n=(ASTAsmLabel*)node; free(n->label); free(n->source_text); break; }
        /* Payload-free nodes: ASTNode header only; no owned children/fields. */
        case AST_BREAK:
        case AST_CONTINUE:
        case AST_BLOCK_HALT:
        case AST_VARIADIC_PARAM:
            break;
        case AST_BLOCK:
        case AST_PROGRAM:
        case AST_ASM_BLOCK: {
            ASTBlock* b = (ASTBlock*)node;
            if (b->statements) ast_destroy_recursive(b->statements);
            break;
        }
        case AST_IF: {
            ASTIf* i = (ASTIf*)node;
            if (i->condition) ast_destroy_recursive(i->condition);
            if (i->then_branch) ast_destroy_recursive(i->then_branch);
            if (i->else_branch) ast_destroy_recursive(i->else_branch);
            break;
        }
        case AST_LOOP: {
            ASTLoop* l = (ASTLoop*)node;
            if (l->init) ast_destroy_recursive(l->init);
            if (l->condition) ast_destroy_recursive(l->condition);
            if (l->increment) ast_destroy_recursive(l->increment);
            if (l->body) ast_destroy_recursive(l->body);
            if (l->collection) ast_destroy_recursive(l->collection);
            if (l->variable) ast_destroy_recursive(l->variable);
            break;
        }
        case AST_NEXUS_CAPTURE:
        case AST_LISTEN: {
            ASTNexusCapture* c = (ASTNexusCapture*)node;
            if (c->handler_name) free(c->handler_name);
            if (c->expr) ast_destroy_recursive(c->expr);
            break;
        }
        case AST_NEXUS_LABEL: {
            ASTNexusLabel* l = (ASTNexusLabel*)node;
            if (l->label_name) free(l->label_name);
            if (l->statement) ast_destroy_recursive(l->statement);
            break;
        }
        case AST_NEXUS_JUMP: {
            ASTNexusJump* j = (ASTNexusJump*)node;
            if (j->label_name) free(j->label_name);
            break;
        }
        case AST_NEXUS_INTENT: {
            ASTIntent* i = (ASTIntent*)node;
            if (i->intent_name) free(i->intent_name);
            if (i->target) ast_destroy_recursive(i->target);
            break;
        }
        case AST_AI_LOGIC: {
            ASTAiLogic* a = (ASTAiLogic*)node;
            if (a->query) free(a->query);
            break;
        }
        case AST_PANIC: {
            ASTPanic* p = (ASTPanic*)node;
            if (p->message) free(p->message);
            break;
        }
        default: break;
    }
    
        free(node);
        node = next;
    }
}

void ast_destroy(ASTNode* root) {
    ast_destroy_recursive(root);
}

#define AST_MAX_STRING_LENGTH (16u * 1024u * 1024u)
#define AST_MAX_NODE_COUNT 1000000u
#define AST_MAX_RECURSION_DEPTH 512u

typedef struct {
    FILE* stream;
    size_t depth;
    int failed;
    uint32_t format_version;
} ASTWriter;

#define AST_STREAM_MAGIC "OCCAST\r\n"
#define AST_STREAM_MAGIC_SIZE 8u
#define AST_STREAM_VERSION 4u


static void ast_write_exact(ASTWriter* writer, const void* source, size_t size) {
    if (!writer || writer->failed) return;
    if (!writer->stream || (size > 0 && !source)) {
        writer->failed = 1;
        return;
    }
    if (size > 0 && fwrite(source, 1, size, writer->stream) != size) {
        writer->failed = 1;
    }
}

static void write_string(ASTWriter* writer, const char* text) {
    if (!writer || writer->failed) return;
    if (!text) {
        int length = -1;
        ast_write_exact(writer, &length, sizeof(length));
        return;
    }

    size_t text_length = strlen(text);
    if (text_length > AST_MAX_STRING_LENGTH) {
        if (writer) writer->failed = 1;
        return;
    }

    int length = (int)text_length;
    ast_write_exact(writer, &length, sizeof(length));
    ast_write_exact(writer, text, text_length);
}

typedef struct {
    FILE* stream;
    size_t node_count;
    size_t depth;
    int failed;
    uint32_t format_version;
    unsigned char replay[AST_STREAM_MAGIC_SIZE];
    size_t replay_size;
    size_t replay_offset;
} ASTReader;

static int ast_read_exact(ASTReader* reader, void* destination, size_t size) {
    size_t copied = 0u;
    if (!reader || reader->failed || !destination) return 0;
    if (size == 0u) return 1;
    if (reader->replay_offset < reader->replay_size) {
        size_t available = reader->replay_size - reader->replay_offset;
        copied = available < size ? available : size;
        memcpy(destination, reader->replay + reader->replay_offset, copied);
        reader->replay_offset += copied;
    }
    if (copied < size &&
        fread((unsigned char*)destination + copied, 1u, size - copied, reader->stream) != size - copied) {
        reader->failed = 1;
        return 0;
    }
    return 1;
}

static char* read_string(ASTReader* reader) {
    int len = -1;
    if (!ast_read_exact(reader, &len, sizeof(len))) return NULL;
    if (len == -1) return NULL;

    if (len < 0 || (unsigned int)len > AST_MAX_STRING_LENGTH) {
        reader->failed = 1;
        return NULL;
    }

    char* value = (char*)ast_malloc((size_t)len + 1u);
    if (!value) {
        reader->failed = 1;
        return NULL;
    }

    if (!ast_read_exact(reader, value, (size_t)len)) {
        free(value);
        return NULL;
    }

    value[len] = '\0';
    return value;
}

static void ast_serialize_recursive(ASTNode* node, ASTWriter* writer) {
    if (!writer || writer->failed) return;
    if (!node) {
        int marker = -1;
        ast_write_exact(writer, &marker, sizeof(marker));
        return;
    }

    /*
     * Serialize sibling lists iteratively. Child fields still recurse into
     * their own lists, but a flat declaration/statement list no longer uses
     * one C stack frame per node.
     */
    if (writer->depth >= AST_MAX_RECURSION_DEPTH) {
        writer->failed = 1;
        return;
    }
    ++writer->depth;

    while (node && !writer->failed) {
    ast_write_exact(writer, &node->type, sizeof(node->type));
    ast_write_exact(writer, &node->line, sizeof(node->line));
    ast_write_exact(writer, &node->col, sizeof(node->col));
    if (writer->format_version) {
        const OccSourceSpan* span = &node->source_span;
        ast_write_exact(writer, &span->has_location, sizeof(span->has_location));
        ast_write_exact(writer, &span->source_id, sizeof(span->source_id));
        ast_write_exact(writer, &span->start_offset, sizeof(span->start_offset));
        ast_write_exact(writer, &span->end_offset, sizeof(span->end_offset));
        ast_write_exact(writer, &span->start_line, sizeof(span->start_line));
        ast_write_exact(writer, &span->start_column, sizeof(span->start_column));
        ast_write_exact(writer, &span->end_line, sizeof(span->end_line));
        ast_write_exact(writer, &span->end_column, sizeof(span->end_column));
    }

    switch(node->type) {
        case AST_VAR_DECL: {
            ASTVarDecl* d = (ASTVarDecl*)node;
            write_string(writer, d->name);
            write_string(writer, d->type_name);
            ast_write_exact(writer, &d->is_const, (size_t)(sizeof(int)) * (size_t)(1));
            ast_write_exact(writer, &d->is_hard_typed, (size_t)(sizeof(int)) * (size_t)(1));
            ast_write_exact(writer, &d->is_public, (size_t)(sizeof(int)) * (size_t)(1));
            ast_write_exact(writer, &d->is_exported, (size_t)(sizeof(int)) * (size_t)(1));
            ast_serialize_recursive(d->initializer, writer);
            ast_serialize_recursive(d->inline_type, writer);
            ast_serialize_recursive(d->type_expr, writer);
            break;
        }
        case AST_FUNC_DECL:
        case AST_EXTERN_DECL: {
            ASTFuncDecl* f = (ASTFuncDecl*)node;
            write_string(writer, f->name);
            write_string(writer, f->return_type);
            ast_write_exact(writer, &f->is_extern, (size_t)(sizeof(int)) * (size_t)(1));
            ast_write_exact(writer, &f->is_public, (size_t)(sizeof(int)) * (size_t)(1));
            ast_write_exact(writer, &f->is_exported, (size_t)(sizeof(int)) * (size_t)(1));
            ast_serialize_recursive(f->params, writer);
            ast_serialize_recursive(f->body, writer);
            ast_serialize_recursive(f->return_array_type, writer);
            write_string(writer, f->linkname);
            ast_serialize_recursive(f->return_type_expr, writer);
            break;
        }
        case AST_STRUCT_DECL:
        case AST_UNION_DECL: {
            ASTStructDecl* s = (ASTStructDecl*)node;
            write_string(writer, s->name);
            ast_write_exact(writer, &s->is_packed, (size_t)(sizeof(int)) * (size_t)(1));
            ast_write_exact(writer, &s->is_union, (size_t)(sizeof(int)) * (size_t)(1));
            ast_write_exact(writer, &s->is_public, (size_t)(sizeof(int)) * (size_t)(1));
            ast_write_exact(writer, &s->is_exported, (size_t)(sizeof(int)) * (size_t)(1));
            ast_serialize_recursive(s->fields, writer);
            break;
        }
        case AST_LITERAL: {
            ASTLiteral* l = (ASTLiteral*)node;
            ast_write_exact(writer, &l->type, (size_t)(sizeof(OmniTokenType)) * (size_t)(1));
            ast_write_exact(writer, &l->lit_type, (size_t)(sizeof(int)) * (size_t)(1));
            ast_write_exact(writer, &l->int_value, (size_t)(sizeof(long long)) * (size_t)(1));
            ast_write_exact(writer, &l->float_value, (size_t)(sizeof(double)) * (size_t)(1));
            write_string(writer, l->string_value);
            break;
        }
        case AST_IDENTIFIER: {
            write_string(writer, ((ASTIdentifier*)node)->name);
            break;
        }
        case AST_BINARY_EXPR:
        case AST_ASSIGNMENT:
        case AST_NEXUS_FLOW:
        case AST_NEXUS_PIPE: {
            ASTBinaryExpr* b = (ASTBinaryExpr*)node;
            ast_write_exact(writer, &b->op, (size_t)(sizeof(OmniTokenType)) * (size_t)(1));
            ast_serialize_recursive(b->left, writer);
            ast_serialize_recursive(b->right, writer);
            if (node->type == AST_NEXUS_FLOW || node->type == AST_NEXUS_PIPE) {
                 ASTNexusFlow* f = (ASTNexusFlow*)node;
                 ast_write_exact(writer, &f->retry_count, (size_t)(sizeof(int)) * (size_t)(1));
                 ast_write_exact(writer, &f->retry_delay, (size_t)(sizeof(int)) * (size_t)(1));
                 ast_serialize_recursive(f->catch_block, writer);
            }
            break;
        }
        case AST_UNARY_EXPR:
        case AST_RETURN:
        case AST_POSTFIX_EXPR: {
            ASTUnaryExpr* u = (ASTUnaryExpr*)node;
            ast_write_exact(writer, &u->op, (size_t)(sizeof(OmniTokenType)) * (size_t)(1));
            ast_serialize_recursive(u->operand, writer);
            break;
        }
        case AST_CAST: {
            ASTCast* cast = (ASTCast*)node;
            ast_serialize_recursive(cast->value, writer);
            ast_serialize_recursive(cast->target_type, writer);
            break;
        }
        case AST_UNPACK: {
            ast_serialize_recursive(((ASTUnpack*)node)->expr, writer);
            break;
        }
        case AST_NEXUS_HALT: {
            ast_serialize_recursive(((ASTHalt*)node)->expr, writer);
            break;
        }
        case AST_BLOCK:
        case AST_PROGRAM:
        case AST_ASM_BLOCK: {
            ast_serialize_recursive(((ASTBlock*)node)->statements, writer);
            break;
        }
        case AST_FUNC_CALL:
        case AST_SPAWN:
        case AST_PRNT:
        case AST_PRMT:
        case AST_WRITE:
        case AST_FWRITE:
        case AST_TYPEOF:
        case AST_SIZEOF:
        case AST_LEN:
        case AST_IS_OK:
        case AST_IS_ERR:
        case AST_IS_SOME:
        case AST_IS_NONE:
        case AST_IS_NULL:
        case AST_EXIT:
        case AST_SWAP:
        case AST_PEEK:
        case AST_POKE:
        case AST_INB:
        case AST_OUTB:
        case AST_IRQ:
        case AST_INTR:
        case AST_REG:
        case AST_AREA:
        case AST_ZONE:
        case AST_ADDR:
        case AST_DATE:
        case AST_DATENOW:
        case AST_TIME:
        case AST_CLOCK:
        case AST_FREE:
        case AST_ZONE_SYNC:
        case AST_ZONE_VIEW:
        case AST_ZONE_LOCK:
        case AST_ECHO:
        case AST_SEND:
        case AST_RECEIVE:
        case AST_DONE: {
            ASTCall* c = (ASTCall*)node;
            ast_serialize_recursive(c->callee, writer);
            ast_serialize_recursive(c->args, writer);
            break;
        }
        case AST_IF: {
            ASTIf* i = (ASTIf*)node;
            ast_serialize_recursive(i->condition, writer);
            ast_serialize_recursive(i->then_branch, writer);
            ast_serialize_recursive(i->else_branch, writer);
            break;
        }
        case AST_LOOP: {
            ASTLoop* l = (ASTLoop*)node;
            ast_write_exact(writer, &l->loop_type, (size_t)(sizeof(int)) * (size_t)(1));
            ast_serialize_recursive(l->init, writer);
            ast_serialize_recursive(l->condition, writer);
            ast_serialize_recursive(l->increment, writer);
            ast_serialize_recursive(l->body, writer);
            ast_serialize_recursive(l->collection, writer);
            ast_serialize_recursive(l->variable, writer);
            break;
        }
        case AST_GROUP_DECL: {
            ASTGroupDecl* g = (ASTGroupDecl*)node;
            write_string(writer, g->name);
            ast_write_exact(writer, &g->is_public, (size_t)(sizeof(int)) * (size_t)(1));
            ast_write_exact(writer, &g->is_exported, (size_t)(sizeof(int)) * (size_t)(1));
            ast_serialize_recursive(g->members, writer);
            break;
        }
        case AST_METHOD: {
            ASTMethod* m = (ASTMethod*)node;
            write_string(writer, m->public_name);
            ast_serialize_recursive(m->implementation, writer);
            break;
        }
        case AST_USE: {
            ASTUse* u = (ASTUse*)node;
            write_string(writer, u->module_name);
            write_string(writer, u->alias);
            break;
        }
        case AST_DEFER: {
            ast_serialize_recursive(((ASTDefer*)node)->body, writer);
            break;
        }
        case AST_THREAD_BLOCK: {
            ASTThreadBlock* t = (ASTThreadBlock*)node;
            write_string(writer, t->name);
            ast_serialize_recursive(t->body, writer);
            ast_serialize_recursive(t->mode, writer);
            ast_serialize_recursive(t->timeout, writer);
            break;
        }
        case AST_MEMBER_ACCESS: {
            ASTMemberAccess* m = (ASTMemberAccess*)node;
            write_string(writer, m->member_name);
            ast_serialize_recursive(m->object, writer);
            break;
        }
        case AST_INDEX_ACCESS: {
            ASTIndexAccess* index = (ASTIndexAccess*)node;
            ast_serialize_recursive(index->target, writer);
            ast_serialize_recursive(index->index, writer);
            break;
        }
        case AST_TUPLE: {
            ast_serialize_recursive(((ASTTuple*)node)->elements, writer);
            break;
        }
        case AST_SELECT: { ASTSelect* n=(ASTSelect*)node; ast_serialize_recursive(n->selector,writer); ast_serialize_recursive(n->arms,writer); break; }
        case AST_SELECT_ARM: { ASTSelectArm* n=(ASTSelectArm*)node; ast_serialize_recursive(n->pattern,writer); ast_serialize_recursive(n->body,writer); ast_write_exact(writer,&n->is_default,sizeof(int)); break; }
        case AST_SCAN: { ASTScan* n=(ASTScan*)node; ast_serialize_recursive(n->subject,writer); ast_serialize_recursive(n->arms,writer); break; }
        case AST_SCAN_ARM: { ASTScanArm* n=(ASTScanArm*)node; ast_serialize_recursive(n->pattern,writer); ast_serialize_recursive(n->body,writer); break; }
        case AST_PATTERN_BINDING: write_string(writer,((ASTPatternBinding*)node)->name); break;
        case AST_PATTERN_CONSTRUCTOR: { ASTPatternConstructor* n=(ASTPatternConstructor*)node; write_string(writer,n->name); ast_serialize_recursive(n->bindings,writer); break; }
        case AST_RULESET_DECL: { ASTRulesetDecl* n=(ASTRulesetDecl*)node; write_string(writer,n->name); ast_serialize_recursive(n->params,writer); ast_serialize_recursive(n->rules,writer); break; }
        case AST_RULE_BARRIER: { ASTRuleBarrier* n=(ASTRuleBarrier*)node; ast_serialize_recursive(n->condition,writer); ast_serialize_recursive(n->message,writer); break; }
        case AST_RULE_TRIGGER: { ASTRuleTrigger* n=(ASTRuleTrigger*)node; ast_serialize_recursive(n->condition,writer); ast_serialize_recursive(n->action,writer); break; }
        case AST_RULE_ALWAYS: ast_serialize_recursive(((ASTRuleAlways*)node)->action,writer); break;
        case AST_RULE_DEFAULT: ast_serialize_recursive(((ASTRuleDefault*)node)->action,writer); break;
        case AST_APPLY_RULES: { ASTApplyRules* n=(ASTApplyRules*)node; write_string(writer,n->ruleset_name); ast_serialize_recursive(n->args,writer); break; }
        case AST_ON_BLOCK: { ASTOnBlock* n=(ASTOnBlock*)node; write_string(writer,n->ruleset_name); write_string(writer,n->event_name); ast_serialize_recursive(n->body,writer); break; }
        case AST_LAMBDA: { ASTLambda* n=(ASTLambda*)node; ast_serialize_recursive(n->params,writer); ast_serialize_recursive(n->body,writer); ast_write_exact(writer,&n->body_is_expression,sizeof(int)); break; }
        case AST_RELOCATE: { ASTRelocate* n=(ASTRelocate*)node; ast_serialize_recursive(n->source,writer); ast_serialize_recursive(n->target,writer); break; }
        case AST_UNROLL: { ASTUnroll* n=(ASTUnroll*)node; ast_serialize_recursive(n->count,writer); ast_serialize_recursive(n->loop,writer); break; }
        case AST_ARRAY_LITERAL: ast_serialize_recursive(((ASTArrayLiteral*)node)->elements,writer); break;
        case AST_STRUCT_LITERAL: {
            ASTStructLiteral* n=(ASTStructLiteral*)node;
            write_string(writer,n->type_name);
            ast_serialize_recursive(n->fields,writer);
            break;
        }
        case AST_STRUCT_FIELD_INIT: {
            ASTStructFieldInit* n=(ASTStructFieldInit*)node;
            write_string(writer,n->name);
            ast_serialize_recursive(n->value,writer);
            break;
        }
        case AST_TYPE_EXPR: {
            ASTTypeExpr* n=(ASTTypeExpr*)node;
            ast_write_exact(writer,&n->kind,sizeof(n->kind));
            ast_write_exact(writer,&n->flags,sizeof(n->flags));
            write_string(writer,n->name);
            ast_serialize_recursive(n->child,writer);
            ast_serialize_recursive(n->params,writer);
            ast_serialize_recursive(n->dimensions,writer);
            break;
        }
        case AST_ARRAY_TYPE: ast_serialize_recursive(((ASTArrayType*)node)->dimensions,writer); break;
        case AST_ARRAY_DIMENSION: ast_serialize_recursive(((ASTArrayDimension*)node)->length,writer); break;
        case AST_FASTEXEC_BLOCK: ast_serialize_recursive(((ASTFastExecBlock*)node)->statements,writer); break;
        case AST_ASM_LABEL: {
            ASTAsmLabel* n=(ASTAsmLabel*)node;
            uint64_t length=(uint64_t)n->source_length;
            if ((size_t)length!=n->source_length ||
                (n->source_text && strlen(n->source_text)!=n->source_length) ||
                (!n->source_text && n->source_length!=0u)) { writer->failed=1; break; }
            write_string(writer,n->label);
            write_string(writer,n->source_text);
            ast_write_exact(writer,&length,sizeof(length));
            break;
        }
        case AST_NAMED_ARG: { ASTNamedArg* n=(ASTNamedArg*)node; write_string(writer,n->name); ast_serialize_recursive(n->value,writer); break; }
        case AST_MUST_DECL: { ASTMustDecl* n=(ASTMustDecl*)node; write_string(writer,n->name); ast_serialize_recursive(n->source,writer); break; }
        case AST_ENUM_DECL: {
            ASTEnumDecl* e = (ASTEnumDecl*)node;
            write_string(writer, e->name);
            ast_serialize_recursive(e->variants, writer);
            break;
        }
        case AST_ENUM_VARIANT: {
            ASTEnumVariant* v = (ASTEnumVariant*)node;
            write_string(writer, v->name);
            ast_serialize_recursive(v->data, writer);
            break;
        }
        case AST_NEWTYPE_DECL: {
            ASTNewTypeDecl* nt = (ASTNewTypeDecl*)node;
            write_string(writer, nt->name);
            write_string(writer, nt->target_type);
            ast_serialize_recursive(nt->target_type_expr, writer);
            break;
        }
        case AST_NEXUS_CAPTURE:
        case AST_LISTEN: {
            ASTNexusCapture* c = (ASTNexusCapture*)node;
            write_string(writer, c->handler_name);
            ast_serialize_recursive(c->expr, writer);
            break;
        }
        case AST_NEXUS_LABEL: {
            ASTNexusLabel* l = (ASTNexusLabel*)node;
            write_string(writer, l->label_name);
            ast_serialize_recursive(l->statement, writer);
            break;
        }
        case AST_NEXUS_JUMP: {
            write_string(writer, ((ASTNexusJump*)node)->label_name);
            break;
        }
        case AST_NEXUS_INTENT: {
            ASTIntent* i = (ASTIntent*)node;
            write_string(writer, i->intent_name);
            ast_serialize_recursive(i->target, writer);
            break;
        }
        case AST_AI_LOGIC: {
            write_string(writer, ((ASTAiLogic*)node)->query);
            break;
        }
        case AST_PANIC: {
            write_string(writer, ((ASTPanic*)node)->message);
            break;
        }
        /* Payload-free nodes serialize only the common node header. */
        case AST_BREAK:
        case AST_CONTINUE:
        case AST_BLOCK_HALT:
        case AST_VARIADIC_PARAM:
            break;
        default: break;
    }

        node = node->next;
    }

    if (!writer->failed) {
        int marker = -1;
        ast_write_exact(writer, &marker, sizeof(marker));
    }
    --writer->depth;
}

int ast_serialize(ASTNode* root, FILE* out) {
    ASTValidationError validation_error;
    ASTWriter writer;
    const uint32_t version = AST_STREAM_VERSION;
    if (!root || !out || !ast_validate(root, &validation_error)) return 0;

    writer.stream = out;
    writer.depth = 0u;
    writer.failed = 0;
    writer.format_version = AST_STREAM_VERSION;
    ast_write_exact(&writer, AST_STREAM_MAGIC, AST_STREAM_MAGIC_SIZE);
    ast_write_exact(&writer, &version, sizeof(version));
    ast_serialize_recursive(root, &writer);
    if (ferror(out)) writer.failed = 1;
    return writer.failed ? 0 : 1;
}

static ASTNode* ast_deserialize_recursive(ASTReader* reader);

static ASTNode* ast_deserialize_one(ASTReader* reader, int type_val) {
    ASTNodeType type = (ASTNodeType)type_val;
    ASTNode* node = ast_create(type);
    if (!node) {
        reader->failed = 1;
        return NULL;
    }

    if (!ast_read_exact(reader, &node->line, sizeof(node->line)) ||
        !ast_read_exact(reader, &node->col, sizeof(node->col))) {
        goto fail;
    }
    if (reader->format_version) {
        OccSourceSpan* span = &node->source_span;
        if (!ast_read_exact(reader, &span->has_location, sizeof(span->has_location)) ||
            !ast_read_exact(reader, &span->source_id, sizeof(span->source_id)) ||
            !ast_read_exact(reader, &span->start_offset, sizeof(span->start_offset)) ||
            !ast_read_exact(reader, &span->end_offset, sizeof(span->end_offset)) ||
            !ast_read_exact(reader, &span->start_line, sizeof(span->start_line)) ||
            !ast_read_exact(reader, &span->start_column, sizeof(span->start_column)) ||
            !ast_read_exact(reader, &span->end_line, sizeof(span->end_line)) ||
            !ast_read_exact(reader, &span->end_column, sizeof(span->end_column))) goto fail;
    }

    switch(type) {
        case AST_VAR_DECL: {
            ASTVarDecl* d = (ASTVarDecl*)node;
            d->name = read_string(reader);
            d->type_name = read_string(reader);
            if (!ast_read_exact(reader, &d->is_const, sizeof(int))) goto fail;
            if (!ast_read_exact(reader, &d->is_hard_typed, sizeof(int))) goto fail;
            if (!ast_read_exact(reader, &d->is_public, sizeof(int))) goto fail;
            if (!ast_read_exact(reader, &d->is_exported, sizeof(int))) goto fail;
            d->initializer = ast_deserialize_recursive(reader);
            d->inline_type = ast_deserialize_recursive(reader);
            if(reader->format_version>=4u) d->type_expr=ast_deserialize_recursive(reader);
            break;
        }
        case AST_FUNC_DECL:
        case AST_EXTERN_DECL: {
            ASTFuncDecl* f = (ASTFuncDecl*)node;
            f->name = read_string(reader);
            f->return_type = read_string(reader);
            if (!ast_read_exact(reader, &f->is_extern, sizeof(int))) goto fail;
            if (!ast_read_exact(reader, &f->is_public, sizeof(int))) goto fail;
            if (!ast_read_exact(reader, &f->is_exported, sizeof(int))) goto fail;
            f->params = ast_deserialize_recursive(reader);
            f->body = ast_deserialize_recursive(reader);
            if (reader->format_version >= 2u)
                f->return_array_type = ast_deserialize_recursive(reader);
            if (reader->format_version >= 3u)
                f->linkname = read_string(reader);
            if (reader->format_version >= 4u)
                f->return_type_expr = ast_deserialize_recursive(reader);
            break;
        }
        case AST_STRUCT_DECL:
        case AST_UNION_DECL: {
            ASTStructDecl* s = (ASTStructDecl*)node;
            s->name = read_string(reader);
            if (!ast_read_exact(reader, &s->is_packed, sizeof(int))) goto fail;
            if (!ast_read_exact(reader, &s->is_union, sizeof(int))) goto fail;
            if (!ast_read_exact(reader, &s->is_public, sizeof(int))) goto fail;
            if (!ast_read_exact(reader, &s->is_exported, sizeof(int))) goto fail;
            s->fields = ast_deserialize_recursive(reader);
            break;
        }
        case AST_LITERAL: {
            ASTLiteral* l = (ASTLiteral*)node;
            if (!ast_read_exact(reader, &l->type, sizeof(OmniTokenType))) goto fail;
            if (!ast_read_exact(reader, &l->lit_type, sizeof(int))) goto fail;
            if (!ast_read_exact(reader, &l->int_value, sizeof(long long))) goto fail;
            if (!ast_read_exact(reader, &l->float_value, sizeof(double))) goto fail;
            l->string_value = read_string(reader);
            break;
        }
        case AST_IDENTIFIER: {
            ((ASTIdentifier*)node)->name = read_string(reader);
            break;
        }
        case AST_BINARY_EXPR:
        case AST_ASSIGNMENT:
        case AST_NEXUS_FLOW:
        case AST_NEXUS_PIPE: {
            ASTBinaryExpr* b = (ASTBinaryExpr*)node;
            if (!ast_read_exact(reader, &b->op, sizeof(OmniTokenType))) goto fail;
            b->left = ast_deserialize_recursive(reader);
            b->right = ast_deserialize_recursive(reader);
            if (type == AST_NEXUS_FLOW || type == AST_NEXUS_PIPE) {
                 ASTNexusFlow* f = (ASTNexusFlow*)node;
                 if (!ast_read_exact(reader, &f->retry_count, sizeof(int))) goto fail;
                 if (!ast_read_exact(reader, &f->retry_delay, sizeof(int))) goto fail;
                 f->catch_block = ast_deserialize_recursive(reader);
            }
            break;
        }
        case AST_UNARY_EXPR:
        case AST_RETURN:
        case AST_POSTFIX_EXPR: {
            ASTUnaryExpr* u = (ASTUnaryExpr*)node;
            if (!ast_read_exact(reader, &u->op, sizeof(OmniTokenType))) goto fail;
            u->operand = ast_deserialize_recursive(reader);
            break;
        }
        case AST_CAST: {
            ASTCast* cast = (ASTCast*)node;
            cast->value = ast_deserialize_recursive(reader);
            if (reader->failed) goto fail;
            cast->target_type = ast_deserialize_recursive(reader);
            break;
        }
        case AST_UNPACK: {
            ((ASTUnpack*)node)->expr = ast_deserialize_recursive(reader);
            break;
        }
        case AST_NEXUS_HALT: {
            ((ASTHalt*)node)->expr = ast_deserialize_recursive(reader);
            break;
        }
        case AST_BLOCK:
        case AST_PROGRAM:
        case AST_ASM_BLOCK: {
            ((ASTBlock*)node)->statements = ast_deserialize_recursive(reader);
            break;
        }
        case AST_FUNC_CALL:
        case AST_SPAWN:
        case AST_PRNT:
        case AST_PRMT:
        case AST_WRITE:
        case AST_FWRITE:
        case AST_TYPEOF:
        case AST_SIZEOF:
        case AST_LEN:
        case AST_IS_OK:
        case AST_IS_ERR:
        case AST_IS_SOME:
        case AST_IS_NONE:
        case AST_IS_NULL:
        case AST_EXIT:
        case AST_SWAP:
        case AST_PEEK:
        case AST_POKE:
        case AST_INB:
        case AST_OUTB:
        case AST_IRQ:
        case AST_INTR:
        case AST_REG:
        case AST_AREA:
        case AST_ZONE:
        case AST_ADDR:
        case AST_DATE:
        case AST_DATENOW:
        case AST_TIME:
        case AST_CLOCK:
        case AST_FREE:
        case AST_ZONE_SYNC:
        case AST_ZONE_VIEW:
        case AST_ZONE_LOCK:
        case AST_ECHO:
        case AST_SEND:
        case AST_RECEIVE:
        case AST_DONE: {
            ASTCall* c = (ASTCall*)node;
            c->callee = ast_deserialize_recursive(reader);
            c->args = ast_deserialize_recursive(reader);
            break;
        }
        case AST_IF: {
            ASTIf* i = (ASTIf*)node;
            i->condition = ast_deserialize_recursive(reader);
            i->then_branch = ast_deserialize_recursive(reader);
            i->else_branch = ast_deserialize_recursive(reader);
            break;
        }
        case AST_LOOP: {
            ASTLoop* l = (ASTLoop*)node;
            if (!ast_read_exact(reader, &l->loop_type, sizeof(int))) goto fail;
            l->init = ast_deserialize_recursive(reader);
            l->condition = ast_deserialize_recursive(reader);
            l->increment = ast_deserialize_recursive(reader);
            l->body = ast_deserialize_recursive(reader);
            l->collection = ast_deserialize_recursive(reader);
            l->variable = ast_deserialize_recursive(reader);
            break;
        }
        case AST_GROUP_DECL: {
            ASTGroupDecl* g = (ASTGroupDecl*)node;
            g->name = read_string(reader);
            if (!ast_read_exact(reader, &g->is_public, sizeof(int))) goto fail;
            if (!ast_read_exact(reader, &g->is_exported, sizeof(int))) goto fail;
            g->members = ast_deserialize_recursive(reader);
            break;
        }
        case AST_METHOD: {
            ASTMethod* m = (ASTMethod*)node;
            m->public_name = read_string(reader);
            m->implementation = ast_deserialize_recursive(reader);
            break;
        }
        case AST_USE: {
            ASTUse* u = (ASTUse*)node;
            u->module_name = read_string(reader);
            u->alias = read_string(reader);
            break;
        }
        case AST_DEFER: {
            ((ASTDefer*)node)->body = ast_deserialize_recursive(reader);
            break;
        }
        case AST_THREAD_BLOCK: {
            ASTThreadBlock* t = (ASTThreadBlock*)node;
            t->name = read_string(reader);
            t->body = ast_deserialize_recursive(reader);
            t->mode = ast_deserialize_recursive(reader);
            t->timeout = ast_deserialize_recursive(reader);
            break;
        }
        case AST_MEMBER_ACCESS: {
            ASTMemberAccess* m = (ASTMemberAccess*)node;
            m->member_name = read_string(reader);
            m->object = ast_deserialize_recursive(reader);
            break;
        }
        case AST_INDEX_ACCESS: {
            ASTIndexAccess* index = (ASTIndexAccess*)node;
            index->target = ast_deserialize_recursive(reader);
            if (reader->failed) goto fail;
            index->index = ast_deserialize_recursive(reader);
            break;
        }
        case AST_TUPLE: {
            ((ASTTuple*)node)->elements = ast_deserialize_recursive(reader);
            break;
        }
        case AST_SELECT: { ASTSelect* n=(ASTSelect*)node; n->selector=ast_deserialize_recursive(reader); if(reader->failed)goto fail; n->arms=ast_deserialize_recursive(reader); break; }
        case AST_SELECT_ARM: { ASTSelectArm* n=(ASTSelectArm*)node; n->pattern=ast_deserialize_recursive(reader); if(reader->failed)goto fail; n->body=ast_deserialize_recursive(reader); if(reader->failed)goto fail; if(!ast_read_exact(reader,&n->is_default,sizeof(int)))goto fail; break; }
        case AST_SCAN: { ASTScan* n=(ASTScan*)node; n->subject=ast_deserialize_recursive(reader); if(reader->failed)goto fail; n->arms=ast_deserialize_recursive(reader); break; }
        case AST_SCAN_ARM: { ASTScanArm* n=(ASTScanArm*)node; n->pattern=ast_deserialize_recursive(reader); if(reader->failed)goto fail; n->body=ast_deserialize_recursive(reader); break; }
        case AST_PATTERN_BINDING: ((ASTPatternBinding*)node)->name=read_string(reader); break;
        case AST_PATTERN_CONSTRUCTOR: { ASTPatternConstructor* n=(ASTPatternConstructor*)node; n->name=read_string(reader); n->bindings=ast_deserialize_recursive(reader); break; }
        case AST_RULESET_DECL: { ASTRulesetDecl* n=(ASTRulesetDecl*)node; n->name=read_string(reader); n->params=ast_deserialize_recursive(reader); if(reader->failed)goto fail; n->rules=ast_deserialize_recursive(reader); break; }
        case AST_RULE_BARRIER: { ASTRuleBarrier* n=(ASTRuleBarrier*)node; n->condition=ast_deserialize_recursive(reader); if(reader->failed)goto fail; n->message=ast_deserialize_recursive(reader); break; }
        case AST_RULE_TRIGGER: { ASTRuleTrigger* n=(ASTRuleTrigger*)node; n->condition=ast_deserialize_recursive(reader); if(reader->failed)goto fail; n->action=ast_deserialize_recursive(reader); break; }
        case AST_RULE_ALWAYS: ((ASTRuleAlways*)node)->action=ast_deserialize_recursive(reader); break;
        case AST_RULE_DEFAULT: ((ASTRuleDefault*)node)->action=ast_deserialize_recursive(reader); break;
        case AST_APPLY_RULES: { ASTApplyRules* n=(ASTApplyRules*)node; n->ruleset_name=read_string(reader); n->args=ast_deserialize_recursive(reader); break; }
        case AST_ON_BLOCK: { ASTOnBlock* n=(ASTOnBlock*)node; n->ruleset_name=read_string(reader); n->event_name=read_string(reader); n->body=ast_deserialize_recursive(reader); break; }
        case AST_LAMBDA: { ASTLambda* n=(ASTLambda*)node; n->params=ast_deserialize_recursive(reader); if(reader->failed)goto fail; n->body=ast_deserialize_recursive(reader); if(reader->failed)goto fail; if(!ast_read_exact(reader,&n->body_is_expression,sizeof(int)))goto fail; break; }
        case AST_RELOCATE: { ASTRelocate* n=(ASTRelocate*)node; n->source=ast_deserialize_recursive(reader); if(reader->failed)goto fail; n->target=ast_deserialize_recursive(reader); break; }
        case AST_UNROLL: { ASTUnroll* n=(ASTUnroll*)node; n->count=ast_deserialize_recursive(reader); if(reader->failed)goto fail; n->loop=ast_deserialize_recursive(reader); break; }
        case AST_ARRAY_LITERAL: ((ASTArrayLiteral*)node)->elements=ast_deserialize_recursive(reader); break;
        case AST_STRUCT_LITERAL: { ASTStructLiteral* n=(ASTStructLiteral*)node; n->type_name=read_string(reader); if(reader->failed)goto fail; n->fields=ast_deserialize_recursive(reader); break; }
        case AST_STRUCT_FIELD_INIT: { ASTStructFieldInit* n=(ASTStructFieldInit*)node; n->name=read_string(reader); if(reader->failed)goto fail; n->value=ast_deserialize_recursive(reader); break; }
        case AST_TYPE_EXPR: { ASTTypeExpr* n=(ASTTypeExpr*)node; if(!ast_read_exact(reader,&n->kind,sizeof(n->kind)) || !ast_read_exact(reader,&n->flags,sizeof(n->flags)))goto fail; n->name=read_string(reader); if(reader->failed)goto fail; n->child=ast_deserialize_recursive(reader); if(reader->failed)goto fail; n->params=ast_deserialize_recursive(reader); if(reader->failed)goto fail; n->dimensions=ast_deserialize_recursive(reader); break; }
        case AST_ARRAY_TYPE: ((ASTArrayType*)node)->dimensions=ast_deserialize_recursive(reader); break;
        case AST_ARRAY_DIMENSION: ((ASTArrayDimension*)node)->length=ast_deserialize_recursive(reader); break;
        case AST_FASTEXEC_BLOCK: ((ASTFastExecBlock*)node)->statements=ast_deserialize_recursive(reader); break;
        case AST_ASM_LABEL: {
            ASTAsmLabel* n=(ASTAsmLabel*)node;
            uint64_t length=0u;
            n->label=read_string(reader);
            if(reader->failed) goto fail;
            n->source_text=read_string(reader);
            if(reader->failed) goto fail;
            if(!ast_read_exact(reader,&length,sizeof(length)) || (uint64_t)(size_t)length!=length ||
                (n->source_text ? strlen(n->source_text) : 0u)!=(size_t)length ||
                (!n->source_text && length!=0u)) goto fail;
            n->source_length=(size_t)length;
            break;
        }
        case AST_NAMED_ARG: { ASTNamedArg* n=(ASTNamedArg*)node; n->name=read_string(reader); if(reader->failed)goto fail; n->value=ast_deserialize_recursive(reader); break; }
        case AST_MUST_DECL: { ASTMustDecl* n=(ASTMustDecl*)node; n->name=read_string(reader); if(reader->failed)goto fail; n->source=ast_deserialize_recursive(reader); break; }
        case AST_ENUM_DECL: {
            ASTEnumDecl* e = (ASTEnumDecl*)node;
            e->name = read_string(reader);
            e->variants = ast_deserialize_recursive(reader);
            break;
        }
        case AST_ENUM_VARIANT: {
            ASTEnumVariant* v = (ASTEnumVariant*)node;
            v->name = read_string(reader);
            v->data = ast_deserialize_recursive(reader);
            break;
        }
        case AST_NEWTYPE_DECL: {
            ASTNewTypeDecl* nt = (ASTNewTypeDecl*)node;
            nt->name = read_string(reader);
            nt->target_type = read_string(reader);
            if(reader->format_version>=4u) nt->target_type_expr=ast_deserialize_recursive(reader);
            break;
        }
        case AST_NEXUS_CAPTURE:
        case AST_LISTEN: {
            ASTNexusCapture* c = (ASTNexusCapture*)node;
            c->handler_name = read_string(reader);
            c->expr = ast_deserialize_recursive(reader);
            break;
        }
        case AST_NEXUS_LABEL: {
            ASTNexusLabel* l = (ASTNexusLabel*)node;
            l->label_name = read_string(reader);
            l->statement = ast_deserialize_recursive(reader);
            break;
        }
        case AST_NEXUS_JUMP: {
            ((ASTNexusJump*)node)->label_name = read_string(reader);
            break;
        }
        case AST_NEXUS_INTENT: {
            ASTIntent* i = (ASTIntent*)node;
            i->intent_name = read_string(reader);
            i->target = ast_deserialize_recursive(reader);
            break;
        }
        case AST_AI_LOGIC: {
            ((ASTAiLogic*)node)->query = read_string(reader);
            break;
        }
        case AST_PANIC: {
            ((ASTPanic*)node)->message = read_string(reader);
            break;
        }
        /* Payload-free nodes have no body after the common header. */
        case AST_BREAK:
        case AST_CONTINUE:
        case AST_BLOCK_HALT:
        case AST_VARIADIC_PARAM:
            break;
        default: break;
    }

    if (reader->failed) goto fail;

    return node;

fail:
    ast_destroy(node);
    return NULL;
}

static ASTNode* ast_deserialize_recursive(ASTReader* reader) {
    ASTNode* head = NULL;
    ASTNode** tail = &head;

    for (;;) {
        int type_val = -1;
        if (!ast_read_exact(reader, &type_val, sizeof(type_val))) {
            ast_destroy(head);
            return NULL;
        }

        if (type_val == -1) return head;

        if (type_val < AST_PROGRAM || type_val > AST_TYPE_EXPR ||
            (type_val == AST_TYPE_EXPR && reader->format_version < 4u) ||
            reader->node_count >= AST_MAX_NODE_COUNT ||
            reader->depth >= AST_MAX_RECURSION_DEPTH) {
            reader->failed = 1;
            ast_destroy(head);
            return NULL;
        }

        reader->node_count++;
        reader->depth++;
        ASTNode* node = ast_deserialize_one(reader, type_val);
        reader->depth--;

        if (reader->failed || !node) {
            ast_destroy(node);
            ast_destroy(head);
            return NULL;
        }

        *tail = node;
        tail = &node->next;
    }
}

ASTNode* ast_deserialize(FILE* in) {
    unsigned char prefix[AST_STREAM_MAGIC_SIZE];
    ASTReader reader;
    ASTNode* root;
    if (!in) return NULL;

    memset(&reader, 0, sizeof(reader));
    reader.stream = in;
    {
        size_t got = fread(prefix, 1u, sizeof(prefix), in);
        if (got == sizeof(prefix) && memcmp(prefix, AST_STREAM_MAGIC, sizeof(prefix)) == 0) {
            uint32_t version = 0u;
            if (fread(&version, sizeof(version), 1u, in) != 1u ||
                (version != 1u && version != 2u && version != 3u && version != AST_STREAM_VERSION)) return NULL;
            reader.format_version = version;
        } else {
            if (ferror(in)) return NULL;
            if (feof(in)) clearerr(in);
            if (got > 0u) {
                memcpy(reader.replay, prefix, got);
                reader.replay_size = got;
            }
            reader.format_version = 0u;
        }
    }
    root = ast_deserialize_recursive(&reader);

    if (reader.failed) {
        ast_destroy(root);
        return NULL;
    }

    if (!root) return NULL;

    /* The serialized tree must consume the complete input stream. */
    if (fgetc(in) != EOF || ferror(in) ||
        !ast_validate(root, NULL)) {
        ast_destroy(root);
        return NULL;
    }

    return root;
}
/* AST extension integration follows the append-only node registry. */

/* ---------------- AST graph validation ---------------- */
typedef struct {
    const ASTNode **slots;
    size_t count, capacity;
    const ASTNode **active;
    const char **active_fields;
    size_t active_count, active_capacity;
    size_t active_fields_capacity;
    unsigned depth;
    ASTValidationError *error;
} ASTValidationState;

static int ast_validation_fail(ASTValidationState *s, ASTValidationCode code,
                               const ASTNode *node, const char *field) {
    if (s->error) { s->error->code=code; s->error->node=node; s->error->field=field; }
    return 0;
}

static int ast_validation_required_text(ASTValidationState *s,
    const ASTNode *node, const char *value, const char *field) {
    if (!value || !*value)
        return ast_validation_fail(s,AST_VALIDATE_MISSING_REQUIRED,node,field);
    return 1;
}
static int ast_validation_required_string(ASTValidationState *s,
    const ASTNode *node, const char *value, const char *field) {
    if (!value)
        return ast_validation_fail(s,AST_VALIDATE_MISSING_REQUIRED,node,field);
    return 1;
}

static size_t ast_validation_hash(const ASTNode *node) {
    size_t key=(size_t)(uintptr_t)node;
    key ^= key >> 17;
    key *= (size_t)0xed5ad4bbu;
    key ^= key >> 11;
    key *= (size_t)0xac4c1b51u;
    key ^= key >> 15;
    return key;
}

static int ast_validation_grow(ASTValidationState *s, const ASTNode *node) {
    size_t capacity=s->capacity ? s->capacity*2u : 128u;
    size_t i;
    const ASTNode **slots;
    if (capacity < s->capacity || capacity > ((size_t)-1)/sizeof(*slots))
        return ast_validation_fail(s,AST_VALIDATE_OUT_OF_MEMORY,node,NULL);
    slots=(const ASTNode**)ast_calloc(capacity,sizeof(*slots));
    if (!slots) return ast_validation_fail(s,AST_VALIDATE_OUT_OF_MEMORY,node,NULL);
    for (i=0;i<s->capacity;i++) if (s->slots[i]) {
        size_t at=ast_validation_hash(s->slots[i]) & (capacity-1u);
        while (slots[at]) at=(at+1u)&(capacity-1u);
        slots[at]=s->slots[i];
    }
    free(s->slots);
    s->slots=slots;
    s->capacity=capacity;
    return 1;
}

static int ast_validation_mark(ASTValidationState *s, const ASTNode *node) {
    size_t at;
    if (!s->capacity || (s->count+1u) >= s->capacity-(s->capacity/3u)) {
        if (!ast_validation_grow(s,node)) return 0;
    }
    at=ast_validation_hash(node) & (s->capacity-1u);
    while (s->slots[at]) {
        if (s->slots[at]==node)
            return ast_validation_fail(s,AST_VALIDATE_SHARED_NODE,node,"ownership");
        at=(at+1u)&(s->capacity-1u);
    }
    s->slots[at]=node;
    s->count++;
    return 1;
}
static int ast_validation_active_push(ASTValidationState *s, const ASTNode *node,
                                      const char *incoming_field) {
    if (s->active_count==s->active_capacity) {
        size_t capacity=s->active_capacity ? s->active_capacity*2u : 32u;
        const ASTNode **active;
        const char **fields;
        if (capacity<s->active_capacity ||
            capacity>((size_t)-1)/sizeof(*active) ||
            capacity>((size_t)-1)/sizeof(*fields))
            return ast_validation_fail(s,AST_VALIDATE_OUT_OF_MEMORY,node,NULL);
        active=(const ASTNode**)ast_realloc(s->active,capacity*sizeof(*active));
        if (!active) return ast_validation_fail(s,AST_VALIDATE_OUT_OF_MEMORY,node,NULL);
        s->active=active;
        s->active_capacity=capacity;
        fields=(const char**)ast_realloc(s->active_fields,capacity*sizeof(*fields));
        if (!fields) return ast_validation_fail(s,AST_VALIDATE_OUT_OF_MEMORY,node,NULL);
        s->active_fields=fields;
        s->active_fields_capacity=capacity;
    }
    s->active[s->active_count]=node;
    s->active_fields[s->active_count]=incoming_field;
    s->active_count++;
    return 1;
}

static int ast_validation_chain(ASTValidationState*,const ASTNode*,const char*);
static int ast_validation_child(ASTValidationState*s,const ASTNode*n,const ASTNode*c,const char*field,int required) {
    int ok;
    if (!c && required) return ast_validation_fail(s,AST_VALIDATE_MISSING_REQUIRED,n,field);
    if (!c) return 1;
    if (s->depth >= 512u) return ast_validation_fail(s,AST_VALIDATE_DEPTH_LIMIT,n,field);
    s->depth++; ok=ast_validation_chain(s,c,field); s->depth--; return ok;
}
static int ast_validation_chain(ASTValidationState*s,const ASTNode*n,const char*incoming_field) {
    const ASTNode *slow=n, *fast=n;
    /* Detect cycles made solely through sibling links before linear traversal. */
    while (fast && fast->next) {
        slow=slow->next;
        fast=fast->next->next;
        if (slow==fast)
            return ast_validation_fail(s,AST_VALIDATE_CYCLE,slow,"next");
    }
    while(n) {
        size_t active_index;
        if ((int)n->type<(int)AST_PROGRAM || (int)n->type>(int)AST_TYPE_EXPR)
            return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"type");
        if (n->source_span.has_location) {
            const OccSourceSpan* span = &n->source_span;
            if (span->has_location != 1u ||
                span->end_offset < span->start_offset ||
                span->start_line == 0u || span->start_column == 0u ||
                span->end_line == 0u || span->end_column == 0u ||
                span->end_line < span->start_line ||
                (span->end_line == span->start_line &&
                    span->end_column < span->start_column) ||
                n->line < 0 || n->col < 0 ||
                (uint32_t)n->line != span->start_line ||
                (uint32_t)n->col != span->start_column) {
                return ast_validation_fail(s,AST_VALIDATE_INVALID_SPAN,n,"source_span");
            }
        }
        for(active_index=0;active_index<s->active_count;active_index++)
            if(s->active[active_index]==n)
                return ast_validation_fail(s,AST_VALIDATE_CYCLE,n,"child");
        if (!ast_validation_mark(s,n) ||
            !ast_validation_active_push(s,n,incoming_field)) return 0;
#define V(child,field) do { if(!ast_validation_child(s,n,(child),(field),0)) return 0; } while(0)
#define R(child,field) do { if(!ast_validation_child(s,n,(child),(field),1)) return 0; } while(0)
        switch(n->type) {
        case AST_PROGRAM: V(((ASTProgram*)n)->declarations,"declarations"); break;
        case AST_BLOCK: case AST_ASM_BLOCK: V(((ASTBlock*)n)->statements,"statements"); break;
        case AST_FASTEXEC_BLOCK: V(((ASTFastExecBlock*)n)->statements,"statements"); break;
        case AST_ASM_LABEL: {
            const ASTAsmLabel* a=(const ASTAsmLabel*)n;
            size_t ancestor;
            int in_fast_exec=0;
            for(ancestor=0u;ancestor+1u<s->active_count;ancestor++) {
                if(s->active[ancestor]->type==AST_FASTEXEC_BLOCK) {
                    in_fast_exec=1;
                    break;
                }
            }
            if(!in_fast_exec ||
               !s->active_fields[s->active_count-1u] ||
               strcmp(s->active_fields[s->active_count-1u],"statements")!=0)
                return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"asm_fast_exec_scope");
            if(!a->label || !*a->label || !a->source_text ||
                a->source_length>16u*1024u*1024u || strlen(a->source_text)!=a->source_length)
                return ast_validation_fail(s,AST_VALIDATE_MISSING_REQUIRED,n,"asm_payload");
            break;
        }
        case AST_VAR_DECL:
            if(!ast_validation_required_text(s,n,((ASTVarDecl*)n)->name,"name")) return 0;
            V(((ASTVarDecl*)n)->initializer,"initializer"); V(((ASTVarDecl*)n)->inline_type,"inline_type"); V(((ASTVarDecl*)n)->type_expr,"type_expr"); break;
        case AST_FUNC_DECL: {
            const ASTFuncDecl* function=(const ASTFuncDecl*)n;
            int group_method_implementation=s->active_count>=2u &&
                s->active[s->active_count-2u]->type==AST_METHOD &&
                s->active_fields[s->active_count-1u] &&
                strcmp(s->active_fields[s->active_count-1u],"implementation")==0;
            if((!function->name || !*function->name) && !group_method_implementation)
                return ast_validation_fail(s,AST_VALIDATE_MISSING_REQUIRED,n,"name");
            if(function->linkname)
                return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"linkname_extern_only");
            V(function->params,"params");
            R(function->body,"body");
            V(function->return_array_type,"return_array_type");
            V(function->return_type_expr,"return_type_expr");
            break;
        }
        case AST_EXTERN_DECL:
            if(!ast_validation_required_text(s,n,((ASTFuncDecl*)n)->name,"name")) return 0;
            V(((ASTFuncDecl*)n)->params,"params"); V(((ASTFuncDecl*)n)->body,"body"); V(((ASTFuncDecl*)n)->return_array_type,"return_array_type"); V(((ASTFuncDecl*)n)->return_type_expr,"return_type_expr"); break;
        case AST_STRUCT_DECL: case AST_UNION_DECL:
            if(!ast_validation_required_text(s,n,((ASTStructDecl*)n)->name,"name")) return 0;
            V(((ASTStructDecl*)n)->fields,"fields"); break;
        case AST_ENUM_DECL:
            if(!ast_validation_required_text(s,n,((ASTEnumDecl*)n)->name,"name")) return 0;
            V(((ASTEnumDecl*)n)->variants,"variants"); break;
        case AST_ENUM_VARIANT:
            if(s->active_count<2u ||
               s->active[s->active_count-2u]->type!=AST_ENUM_DECL ||
               !s->active_fields[s->active_count-1u] ||
               strcmp(s->active_fields[s->active_count-1u],"variants")!=0)
                return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"enum_variant_owner");
            if(!ast_validation_required_text(s,n,((ASTEnumVariant*)n)->name,"name")) return 0;
            V(((ASTEnumVariant*)n)->data,"data"); break;
        case AST_NEWTYPE_DECL:
            if(!ast_validation_required_text(s,n,((ASTNewTypeDecl*)n)->name,"name") ||
               !ast_validation_required_text(s,n,((ASTNewTypeDecl*)n)->target_type,"target_type")) return 0;
            V(((ASTNewTypeDecl*)n)->target_type_expr,"target_type_expr");
            break;
        case AST_GROUP_DECL:
            if(!ast_validation_required_text(s,n,((ASTGroupDecl*)n)->name,"name")) return 0;
            V(((ASTGroupDecl*)n)->members,"members"); break;
        case AST_METHOD: {
            const ASTMethod* method=(const ASTMethod*)n;
            const ASTNode* implementation=method->implementation;
            const char* edge=s->active_count>0u?s->active_fields[s->active_count-1u]:NULL;
            if(s->active_count<2u ||
               s->active[s->active_count-2u]->type!=AST_GROUP_DECL ||
               !edge || strcmp(edge,"members")!=0)
                return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"group_method_owner");
            if(!ast_validation_required_text(s,n,method->public_name,"public_name")) return 0;
            if(!implementation)
                return ast_validation_fail(s,AST_VALIDATE_MISSING_REQUIRED,n,"implementation");
            if(strcmp(method->public_name,"default")==0) {
                if(implementation->type!=AST_BLOCK)
                    return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"default_method_block");
            } else if(implementation->type!=AST_FUNC_DECL) {
                return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"method_function");
            }
            R(implementation,"implementation");
            break;
        }
        case AST_BINARY_EXPR: case AST_ASSIGNMENT: R(((ASTBinaryExpr*)n)->left,"left"); R(((ASTBinaryExpr*)n)->right,"right"); break;
        case AST_UNARY_EXPR: case AST_POSTFIX_EXPR: R(((ASTUnaryExpr*)n)->operand,"operand"); break;
        case AST_RETURN: V(((ASTUnaryExpr*)n)->operand,"operand"); break;
        case AST_CAST: R(((ASTCast*)n)->value,"value"); R(((ASTCast*)n)->target_type,"target_type"); break;
        case AST_FUNC_CALL: case AST_SPAWN: case AST_ECHO: case AST_SEND: case AST_RECEIVE: case AST_DONE: case AST_AREA: case AST_ZONE: case AST_ADDR: case AST_LEN: case AST_PEEK: case AST_POKE: case AST_INB: case AST_OUTB: case AST_IRQ: case AST_INTR: case AST_REG: case AST_PRNT: case AST_PRMT: case AST_WRITE: case AST_FWRITE: case AST_TYPEOF: case AST_SIZEOF: case AST_IS_OK: case AST_IS_ERR: case AST_IS_SOME: case AST_IS_NONE: case AST_IS_NULL: case AST_EXIT: case AST_SWAP: case AST_DATE: case AST_DATENOW: case AST_TIME: case AST_CLOCK: case AST_FREE: case AST_ZONE_SYNC: case AST_ZONE_VIEW: case AST_ZONE_LOCK: {
            const ASTNode *arg_a, *arg_b;
            int named_seen = 0;
            if(n->type==AST_FUNC_CALL) R(((ASTCall*)n)->callee,"callee");
            else V(((ASTCall*)n)->callee,"callee");
            for (arg_a=((ASTCall*)n)->args; arg_a; arg_a=arg_a->next) {
                if (arg_a->type == AST_NAMED_ARG) {
                    const char *label=((const ASTNamedArg*)arg_a)->name;
                    named_seen=1;
                    for (arg_b=arg_a->next; arg_b; arg_b=arg_b->next)
                        if (arg_b->type == AST_NAMED_ARG &&
                            ((const ASTNamedArg*)arg_b)->name && label &&
                            strcmp(((const ASTNamedArg*)arg_b)->name,label)==0)
                            return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,arg_b,"duplicate_named_argument");
                } else if (named_seen) {
                    return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,arg_a,"positional_after_named_argument");
                }
            }
            V(((ASTCall*)n)->args,"args");
            break;
        }
        case AST_MEMBER_ACCESS:
            if(!ast_validation_required_text(s,n,((ASTMemberAccess*)n)->member_name,"member_name")) return 0;
            R(((ASTMemberAccess*)n)->object,"object"); break;
        case AST_INDEX_ACCESS: R(((ASTIndexAccess*)n)->target,"target"); R(((ASTIndexAccess*)n)->index,"index"); break;
        case AST_TUPLE: V(((ASTTuple*)n)->elements,"elements"); break;
        case AST_IF: R(((ASTIf*)n)->condition,"condition"); R(((ASTIf*)n)->then_branch,"then_branch"); V(((ASTIf*)n)->else_branch,"else_branch"); break;
        case AST_LOOP: V(((ASTLoop*)n)->init,"init"); V(((ASTLoop*)n)->condition,"condition"); V(((ASTLoop*)n)->increment,"increment"); V(((ASTLoop*)n)->body,"body"); V(((ASTLoop*)n)->collection,"collection"); V(((ASTLoop*)n)->variable,"variable"); break;
        case AST_NEXUS_CAPTURE:
            if(!ast_validation_required_text(s,n,((ASTNexusCapture*)n)->handler_name,"handler_name")) return 0;
            V(((ASTNexusCapture*)n)->expr,"expr"); break;
        case AST_LISTEN: V(((ASTNexusCapture*)n)->expr,"expr"); break;
        case AST_NEXUS_FLOW: case AST_NEXUS_PIPE: R(((ASTNexusFlow*)n)->left,"left"); R(((ASTNexusFlow*)n)->right,"right"); V(((ASTNexusFlow*)n)->catch_block,"catch_block"); break;
        case AST_NEXUS_LABEL:
            if(!ast_validation_required_text(s,n,((ASTNexusLabel*)n)->label_name,"label_name")) return 0;
            V(((ASTNexusLabel*)n)->statement,"statement"); break;
        case AST_NEXUS_JUMP:
            if(!ast_validation_required_text(s,n,((ASTNexusJump*)n)->label_name,"label_name")) return 0;
            break;
        case AST_NEXUS_INTENT:
            if(!ast_validation_required_text(s,n,((ASTIntent*)n)->intent_name,"intent_name")) return 0;
            V(((ASTIntent*)n)->target,"target"); break;
        case AST_AI_LOGIC:
            if(!ast_validation_required_text(s,n,((ASTAiLogic*)n)->query,"query")) return 0;
            break;
        case AST_PANIC:
            if(!ast_validation_required_string(s,n,((ASTPanic*)n)->message,"message")) return 0;
            break;
        case AST_NEXUS_HALT: V(((ASTHalt*)n)->expr,"expr"); break;
        case AST_UNPACK: R(((ASTUnpack*)n)->expr,"expr"); break;
        case AST_DEFER: R(((ASTDefer*)n)->body,"body"); break;
        case AST_THREAD_BLOCK:
            if(!ast_validation_required_text(s,n,((ASTThreadBlock*)n)->name,"name")) return 0;
            R(((ASTThreadBlock*)n)->body,"body"); V(((ASTThreadBlock*)n)->mode,"mode"); V(((ASTThreadBlock*)n)->timeout,"timeout"); break;
        case AST_SELECT: R(((ASTSelect*)n)->selector,"selector"); R(((ASTSelect*)n)->arms,"arms"); break;
        case AST_SELECT_ARM:
            if(s->active_count<2u ||
               s->active[s->active_count-2u]->type!=AST_SELECT ||
               !s->active_fields[s->active_count-1u] ||
               strcmp(s->active_fields[s->active_count-1u],"arms")!=0)
                return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"select_arm_context");
            if(!((ASTSelectArm*)n)->is_default) R(((ASTSelectArm*)n)->pattern,"pattern");
            else V(((ASTSelectArm*)n)->pattern,"pattern");
            R(((ASTSelectArm*)n)->body,"body"); break;
        case AST_SCAN: R(((ASTScan*)n)->subject,"subject"); R(((ASTScan*)n)->arms,"arms"); break;
        case AST_SCAN_ARM:
            if(s->active_count<2u ||
               s->active[s->active_count-2u]->type!=AST_SCAN ||
               !s->active_fields[s->active_count-1u] ||
               strcmp(s->active_fields[s->active_count-1u],"arms")!=0)
                return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"scan_arm_context");
            if(!((ASTScanArm*)n)->pattern && n->next)
                return ast_validation_fail(s,AST_VALIDATE_MISSING_REQUIRED,n,"pattern");
            V(((ASTScanArm*)n)->pattern,"pattern"); R(((ASTScanArm*)n)->body,"body"); break;
        case AST_PATTERN_BINDING:
            if(!ast_validation_required_text(s,n,((ASTPatternBinding*)n)->name,"name")) return 0;
            break;
        case AST_PATTERN_CONSTRUCTOR:
            if(!ast_validation_required_text(s,n,((ASTPatternConstructor*)n)->name,"name")) return 0;
            V(((ASTPatternConstructor*)n)->bindings,"bindings"); break;
        case AST_RULESET_DECL:
            if(!ast_validation_required_text(s,n,((ASTRulesetDecl*)n)->name,"name")) return 0;
            V(((ASTRulesetDecl*)n)->params,"params"); R(((ASTRulesetDecl*)n)->rules,"rules"); break;
        case AST_RULE_BARRIER: R(((ASTRuleBarrier*)n)->condition,"condition"); V(((ASTRuleBarrier*)n)->message,"message"); break;
        case AST_RULE_TRIGGER: R(((ASTRuleTrigger*)n)->condition,"condition"); R(((ASTRuleTrigger*)n)->action,"action"); break;
        case AST_RULE_ALWAYS: R(((ASTRuleAlways*)n)->action,"action"); break;
        case AST_RULE_DEFAULT: R(((ASTRuleDefault*)n)->action,"action"); break;
        case AST_APPLY_RULES:
            if(!ast_validation_required_text(s,n,((ASTApplyRules*)n)->ruleset_name,"ruleset_name")) return 0;
            V(((ASTApplyRules*)n)->args,"args"); break;
        case AST_ON_BLOCK:
            if(!ast_validation_required_text(s,n,((ASTOnBlock*)n)->ruleset_name,"ruleset_name") ||
               !ast_validation_required_text(s,n,((ASTOnBlock*)n)->event_name,"event_name")) return 0;
            R(((ASTOnBlock*)n)->body,"body"); break;
        case AST_LAMBDA: V(((ASTLambda*)n)->params,"params"); R(((ASTLambda*)n)->body,"body"); break;
        case AST_RELOCATE: R(((ASTRelocate*)n)->source,"source"); R(((ASTRelocate*)n)->target,"target"); break;
        case AST_UNROLL: R(((ASTUnroll*)n)->count,"count"); R(((ASTUnroll*)n)->loop,"loop"); break;
        case AST_ARRAY_LITERAL: V(((ASTArrayLiteral*)n)->elements,"elements"); break;
        case AST_STRUCT_LITERAL:
            if(!ast_validation_required_text(s,n,((ASTStructLiteral*)n)->type_name,"type_name")) return 0;
            V(((ASTStructLiteral*)n)->fields,"fields"); break;
        case AST_TYPE_EXPR: {
            const ASTTypeExpr* t=(const ASTTypeExpr*)n;
            if(t->kind<AST_TYPE_NAMED || t->kind>AST_TYPE_QUALIFIED)
                return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"type_expr_kind");
            if(t->kind==AST_TYPE_NAMED) {
                if(!ast_validation_required_text(s,n,t->name,"name") || t->child || t->params || t->dimensions)
                    return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"named_type_shape");
            } else if(t->kind==AST_TYPE_POINTER || t->kind==AST_TYPE_REFERENCE || t->kind==AST_TYPE_QUALIFIED) {
                if(!t->child || t->name || t->params || t->dimensions)
                    return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"wrapped_type_shape");
            } else if(t->kind==AST_TYPE_ARRAY) {
                if(!t->child || t->name || t->params)
                    return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"array_type_shape");
            } else if(t->kind==AST_TYPE_FUNCTION) {
                if(!t->child || t->name || t->dimensions)
                    return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"function_type_shape");
            }
            V(t->child,"child"); V(t->params,"params"); V(t->dimensions,"dimensions");
            break;
        }
        case AST_STRUCT_FIELD_INIT:
            if(s->active_count<2u ||
               s->active[s->active_count-2u]->type!=AST_STRUCT_LITERAL ||
               !s->active_fields[s->active_count-1u] ||
               strcmp(s->active_fields[s->active_count-1u],"fields")!=0)
                return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"struct_field_context");
            if(!ast_validation_required_text(s,n,((ASTStructFieldInit*)n)->name,"name")) return 0;
            R(((ASTStructFieldInit*)n)->value,"value"); break;
        case AST_ARRAY_TYPE: V(((ASTArrayType*)n)->dimensions,"dimensions"); break;
        case AST_ARRAY_DIMENSION:
            if(s->active_count<2u ||
               (s->active[s->active_count-2u]->type!=AST_ARRAY_TYPE &&
                !(s->active[s->active_count-2u]->type==AST_TYPE_EXPR &&
                  ((const ASTTypeExpr*)s->active[s->active_count-2u])->kind==AST_TYPE_ARRAY)) ||
               !s->active_fields[s->active_count-1u] ||
               strcmp(s->active_fields[s->active_count-1u],"dimensions")!=0)
                return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"array_dimension_context");
            V(((ASTArrayDimension*)n)->length,"length"); break;
        case AST_VARIADIC_PARAM:
            if (s->active_count < 2u ||
                (s->active[s->active_count - 2u]->type != AST_FUNC_DECL &&
                 s->active[s->active_count - 2u]->type != AST_EXTERN_DECL) ||
                !s->active_fields[s->active_count-1u] ||
                strcmp(s->active_fields[s->active_count-1u],"params")!=0 ||
                n->next)
                return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"variadic_parameter_position");
            break;
        case AST_MUST_DECL:
            if (s->active_count < 2u ||
                s->active[s->active_count - 2u]->type != AST_PROGRAM ||
                !s->active_fields[s->active_count-1u] ||
                strcmp(s->active_fields[s->active_count-1u],"declarations")!=0)
                return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"must_dependency_scope");
            R(((ASTMustDecl*)n)->source,"source");
            if (!((ASTMustDecl*)n)->name || !*(((ASTMustDecl*)n)->name))
                return ast_validation_fail(s,AST_VALIDATE_MISSING_REQUIRED,n,"name");
            break;
        case AST_NAMED_ARG:
            if (s->active_count < 2u ||
                !s->active_fields[s->active_count - 1u] ||
                strcmp(s->active_fields[s->active_count - 1u],"args") != 0)
                return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"named_argument_context");
            switch (s->active[s->active_count - 2u]->type) {
                case AST_FUNC_CALL: case AST_SPAWN: case AST_ECHO: case AST_SEND:
                case AST_RECEIVE: case AST_DONE: case AST_AREA: case AST_ZONE:
                case AST_ADDR: case AST_LEN: case AST_PEEK: case AST_POKE:
                case AST_INB: case AST_OUTB: case AST_IRQ: case AST_INTR:
                case AST_REG: case AST_PRNT: case AST_PRMT: case AST_WRITE:
                case AST_FWRITE: case AST_TYPEOF: case AST_SIZEOF: case AST_IS_OK:
                case AST_IS_ERR: case AST_IS_SOME: case AST_IS_NONE: case AST_IS_NULL:
                case AST_EXIT: case AST_SWAP: case AST_DATE: case AST_DATENOW:
                case AST_TIME: case AST_CLOCK: case AST_FREE: case AST_ZONE_SYNC:
                case AST_ZONE_VIEW: case AST_ZONE_LOCK: break;
                default: return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"named_argument_context");
            }
            R(((ASTNamedArg*)n)->value,"value");
            if (!((ASTNamedArg*)n)->name || !*(((ASTNamedArg*)n)->name))
                return ast_validation_fail(s,AST_VALIDATE_MISSING_REQUIRED,n,"name");
            break;

        /* Leaf payloads: their data is scalar/string-owned, not AST children. */
        case AST_LITERAL:
            if(!ast_validation_required_string(s,n,((ASTLiteral*)n)->string_value,"string_value")) return 0;
            break;
        case AST_IDENTIFIER:
            if(!ast_validation_required_text(s,n,((ASTIdentifier*)n)->name,"name")) return 0;
            break;
        case AST_USE:
            if(!ast_validation_required_text(s,n,((ASTUse*)n)->module_name,"module_name")) return 0;
            break;
        case AST_BREAK:
        case AST_CONTINUE:
        case AST_BLOCK_HALT:
            if (n->type == AST_BLOCK_HALT &&
                (s->active_count < 2u ||
                 (s->active[s->active_count - 2u]->type != AST_BLOCK &&
                  s->active[s->active_count - 2u]->type != AST_ASM_BLOCK &&
                  s->active[s->active_count - 2u]->type != AST_FASTEXEC_BLOCK) ||
                 !s->active_fields[s->active_count-1u] ||
                 strcmp(s->active_fields[s->active_count-1u],"statements")!=0 ||
                 n->next != NULL))
                return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"block_cleanup_position");
            break;
        /* Fail closed: a newly appended ASTNodeType must be explicitly
         * assigned a child contract above before it can validate. */
        default:
            return ast_validation_fail(s,AST_VALIDATE_INVALID_KIND,n,"type");
        }
#undef V
#undef R
        s->active_count--;
        n=n->next;
    }
    return 1;
}
int ast_validate(const ASTNode *root, ASTValidationError *error) {
    ASTValidationState state={0};
    if(error){error->code=AST_VALIDATE_OK;error->node=NULL;error->field=NULL;}
    if(!root) { if(error)error->code=AST_VALIDATE_MISSING_REQUIRED; return 0; }
    state.error=error;
    {
        int ok=ast_validation_chain(&state,root,NULL);
        free(state.slots);
        free(state.active);
        free(state.active_fields);
        return ok;
    }
}
