#include "ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// AST Düğüm tiplerine göre bellek boyutunu belirleyen harita
static size_t get_node_size(ASTNodeType type) {
    switch (type) {
        // --- İfadeler ---
        case AST_BINARY_EXPR: 
        case AST_ASSIGNMENT:
        case AST_INDEX_ACCESS:
            return sizeof(ASTBinaryExpr);

        case AST_UNARY_EXPR:
        case AST_RETURN:
        case AST_POSTFIX_EXPR:
            return sizeof(ASTUnaryExpr);
        case AST_CAST:
            return sizeof(ASTCast);
            
        case AST_TUPLE: return sizeof(ASTTuple);

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

        // Diğerleri için temel düğüm boyutu (AST_BREAK, AST_CONTINUE vb.)
        default: return sizeof(ASTNode);
    }
}

ASTNode* ast_create(ASTNodeType type) {
    size_t size = get_node_size(type);
    ASTNode* node = (ASTNode*)malloc(size);
    if (node) {
        memset(node, 0, size); // Belleği sıfırla (tüm pointerlar NULL olur)
        node->type = type;
        node->next = NULL;
    }
    return node;
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
                printf("[BLOCK]\n");
                ast_print_recursive(((ASTBlock*)node)->statements, level + 1);
                break;
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
            case AST_TUPLE: {
                ASTTuple* tuple = (ASTTuple*)node;
                printf("TUPLE\n");
                ast_print_recursive(tuple->elements, level + 1);
                break;
            }
            case AST_NEXUS_CAPTURE: {
                ASTNexusCapture* cap = (ASTNexusCapture*)node;
                printf("NEXUS_CAPTURE: %s\n", cap->handler_name);
                if (cap->expr) ast_print_recursive(cap->expr, level + 1);
                break;
            }
            case AST_NEXUS_FLOW: {
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
            break;
        }
        case AST_FUNC_DECL:
        case AST_EXTERN_DECL: {
            ASTFuncDecl* f = (ASTFuncDecl*)node;
            if (f->name) free(f->name);
            if (f->return_type) free(f->return_type);
            if (f->params) ast_destroy_recursive(f->params);
            if (f->body) ast_destroy_recursive(f->body);
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
        case AST_INDEX_ACCESS:
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
        case AST_TUPLE: {
            ASTTuple* t = (ASTTuple*)node;
            if (t->elements) ast_destroy_recursive(t->elements);
            break;
        }
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
} ASTWriter;

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
} ASTReader;

static int ast_read_exact(ASTReader* reader, void* destination, size_t size) {
    if (!reader || reader->failed || !destination) return 0;
    if (size == 0) return 1;

    if (fread(destination, 1, size, reader->stream) != size) {
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

    char* value = (char*)malloc((size_t)len + 1u);
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
        case AST_INDEX_ACCESS:
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
        case AST_TUPLE: {
            ast_serialize_recursive(((ASTTuple*)node)->elements, writer);
            break;
        }
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
    if (!out) return 0;

    ASTWriter writer = { out, 0u, 0 };
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
        case AST_INDEX_ACCESS:
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
        case AST_TUPLE: {
            ((ASTTuple*)node)->elements = ast_deserialize_recursive(reader);
            break;
        }
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

        if (type_val < AST_PROGRAM || type_val > AST_ZONE_LOCK ||
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
    if (!in) return NULL;

    ASTReader reader = { in, 0u, 0u, 0 };
    ASTNode* root = ast_deserialize_recursive(&reader);

    if (reader.failed) {
        ast_destroy(root);
        return NULL;
    }

    if (!root) return NULL;

    /* The serialized tree must consume the complete input stream. */
    if (fgetc(in) != EOF || ferror(in)) {
        ast_destroy(root);
        return NULL;
    }

    return root;
}
/* AST extension integration follows the append-only node registry. */
