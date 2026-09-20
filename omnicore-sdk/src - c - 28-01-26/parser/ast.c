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
        case AST_CAST:
            return sizeof(ASTUnaryExpr);
            
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
            case AST_FUNC_DECL: {
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
    if (!node) return;
    
    // Önce bağlı listeyi temizle (next)
    if (node->next) ast_destroy_recursive(node->next);

    switch (node->type) {
        case AST_VAR_DECL: {
            ASTVarDecl* d = (ASTVarDecl*)node;
            if (d->name) free(d->name);
            if (d->type_name) free(d->type_name);
            if (d->initializer) ast_destroy_recursive(d->initializer);
            if (d->inline_type) ast_destroy_recursive(d->inline_type);
            break;
        }
        case AST_FUNC_DECL: {
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
        case AST_POSTFIX_EXPR:
        case AST_CAST:
        case AST_UNPACK:
        case AST_NEXUS_HALT: {
            ASTUnaryExpr* u = (ASTUnaryExpr*)node;
            if (u->operand) ast_destroy_recursive(u->operand);
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
}

void ast_destroy(ASTNode* root) {
    ast_destroy_recursive(root);
}

static void write_string(FILE* out, const char* s) {
    if (!s) {
        int len = -1;
        fwrite(&len, sizeof(int), 1, out);
        return;
    }
    int len = (int)strlen(s);
    fwrite(&len, sizeof(int), 1, out);
    fwrite(s, 1, len, out);
}

static char* read_string(FILE* in) {
    int len;
    if (fread(&len, sizeof(int), 1, in) != 1) return NULL;
    if (len == -1) return NULL;
    char* s = malloc(len + 1);
    if (!s) return NULL;
    if (fread(s, 1, len, in) != (size_t)len) {
        free(s);
        return NULL;
    }
    s[len] = '\0';
    return s;
}

static void ast_serialize_recursive(ASTNode* node, FILE* out) {
    if (!node) {
        int marker = -1;
        fwrite(&marker, sizeof(int), 1, out);
        return;
    }

    fwrite(&node->type, sizeof(ASTNodeType), 1, out);
    fwrite(&node->line, sizeof(int), 1, out);
    fwrite(&node->col, sizeof(int), 1, out);

    switch(node->type) {
        case AST_VAR_DECL: {
            ASTVarDecl* d = (ASTVarDecl*)node;
            write_string(out, d->name);
            write_string(out, d->type_name);
            fwrite(&d->is_const, sizeof(int), 1, out);
            fwrite(&d->is_hard_typed, sizeof(int), 1, out);
            fwrite(&d->is_public, sizeof(int), 1, out);
            fwrite(&d->is_exported, sizeof(int), 1, out);
            ast_serialize_recursive(d->initializer, out);
            ast_serialize_recursive(d->inline_type, out);
            break;
        }
        case AST_FUNC_DECL: {
            ASTFuncDecl* f = (ASTFuncDecl*)node;
            write_string(out, f->name);
            write_string(out, f->return_type);
            fwrite(&f->is_extern, sizeof(int), 1, out);
            fwrite(&f->is_public, sizeof(int), 1, out);
            fwrite(&f->is_exported, sizeof(int), 1, out);
            ast_serialize_recursive(f->params, out);
            ast_serialize_recursive(f->body, out);
            break;
        }
        case AST_STRUCT_DECL:
        case AST_UNION_DECL: {
            ASTStructDecl* s = (ASTStructDecl*)node;
            write_string(out, s->name);
            fwrite(&s->is_packed, sizeof(int), 1, out);
            fwrite(&s->is_union, sizeof(int), 1, out);
            fwrite(&s->is_public, sizeof(int), 1, out);
            fwrite(&s->is_exported, sizeof(int), 1, out);
            ast_serialize_recursive(s->fields, out);
            break;
        }
        case AST_LITERAL: {
            ASTLiteral* l = (ASTLiteral*)node;
            fwrite(&l->lit_type, sizeof(int), 1, out);
            fwrite(&l->int_value, sizeof(long long), 1, out);
            fwrite(&l->float_value, sizeof(double), 1, out);
            write_string(out, l->string_value);
            break;
        }
        case AST_IDENTIFIER: {
            write_string(out, ((ASTIdentifier*)node)->name);
            break;
        }
        case AST_BINARY_EXPR:
        case AST_ASSIGNMENT:
        case AST_INDEX_ACCESS:
        case AST_NEXUS_FLOW:
        case AST_NEXUS_PIPE: {
            ASTBinaryExpr* b = (ASTBinaryExpr*)node;
            fwrite(&b->op, sizeof(OmniTokenType), 1, out);
            ast_serialize_recursive(b->left, out);
            ast_serialize_recursive(b->right, out);
            if (node->type == AST_NEXUS_FLOW || node->type == AST_NEXUS_PIPE) {
                 ASTNexusFlow* f = (ASTNexusFlow*)node;
                 fwrite(&f->retry_count, sizeof(int), 1, out);
                 fwrite(&f->retry_delay, sizeof(int), 1, out);
                 ast_serialize_recursive(f->catch_block, out);
            }
            break;
        }
        case AST_UNARY_EXPR:
        case AST_RETURN:
        case AST_POSTFIX_EXPR:
        case AST_CAST:
        case AST_UNPACK:
        case AST_NEXUS_HALT: {
            ASTUnaryExpr* u = (ASTUnaryExpr*)node;
            fwrite(&u->op, sizeof(OmniTokenType), 1, out);
            ast_serialize_recursive(u->operand, out);
            break;
        }
        case AST_BLOCK:
        case AST_PROGRAM:
        case AST_ASM_BLOCK: {
            ast_serialize_recursive(((ASTBlock*)node)->statements, out);
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
            ast_serialize_recursive(c->callee, out);
            ast_serialize_recursive(c->args, out);
            break;
        }
        case AST_IF: {
            ASTIf* i = (ASTIf*)node;
            ast_serialize_recursive(i->condition, out);
            ast_serialize_recursive(i->then_branch, out);
            ast_serialize_recursive(i->else_branch, out);
            break;
        }
        case AST_LOOP: {
            ASTLoop* l = (ASTLoop*)node;
            fwrite(&l->loop_type, sizeof(int), 1, out);
            ast_serialize_recursive(l->init, out);
            ast_serialize_recursive(l->condition, out);
            ast_serialize_recursive(l->increment, out);
            ast_serialize_recursive(l->body, out);
            ast_serialize_recursive(l->collection, out);
            ast_serialize_recursive(l->variable, out);
            break;
        }
        case AST_GROUP_DECL: {
            ASTGroupDecl* g = (ASTGroupDecl*)node;
            write_string(out, g->name);
            fwrite(&g->is_public, sizeof(int), 1, out);
            fwrite(&g->is_exported, sizeof(int), 1, out);
            ast_serialize_recursive(g->members, out);
            break;
        }
        case AST_METHOD: {
            ASTMethod* m = (ASTMethod*)node;
            write_string(out, m->public_name);
            ast_serialize_recursive(m->implementation, out);
            break;
        }
        case AST_USE: {
            ASTUse* u = (ASTUse*)node;
            write_string(out, u->module_name);
            write_string(out, u->alias);
            break;
        }
        case AST_DEFER: {
            ast_serialize_recursive(((ASTDefer*)node)->body, out);
            break;
        }
        case AST_THREAD_BLOCK: {
            ASTThreadBlock* t = (ASTThreadBlock*)node;
            write_string(out, t->name);
            ast_serialize_recursive(t->body, out);
            ast_serialize_recursive(t->mode, out);
            ast_serialize_recursive(t->timeout, out);
            break;
        }
        case AST_MEMBER_ACCESS: {
            ASTMemberAccess* m = (ASTMemberAccess*)node;
            write_string(out, m->member_name);
            ast_serialize_recursive(m->object, out);
            break;
        }
        case AST_TUPLE: {
            ast_serialize_recursive(((ASTTuple*)node)->elements, out);
            break;
        }
        case AST_ENUM_DECL: {
            ASTEnumDecl* e = (ASTEnumDecl*)node;
            write_string(out, e->name);
            ast_serialize_recursive(e->variants, out);
            break;
        }
        case AST_ENUM_VARIANT: {
            ASTEnumVariant* v = (ASTEnumVariant*)node;
            write_string(out, v->name);
            ast_serialize_recursive(v->data, out);
            break;
        }
        case AST_NEWTYPE_DECL: {
            ASTNewTypeDecl* nt = (ASTNewTypeDecl*)node;
            write_string(out, nt->name);
            write_string(out, nt->target_type);
            break;
        }
        case AST_NEXUS_CAPTURE:
        case AST_LISTEN: {
            ASTNexusCapture* c = (ASTNexusCapture*)node;
            write_string(out, c->handler_name);
            ast_serialize_recursive(c->expr, out);
            break;
        }
        case AST_NEXUS_LABEL: {
            ASTNexusLabel* l = (ASTNexusLabel*)node;
            write_string(out, l->label_name);
            ast_serialize_recursive(l->statement, out);
            break;
        }
        case AST_NEXUS_JUMP: {
            write_string(out, ((ASTNexusJump*)node)->label_name);
            break;
        }
        case AST_NEXUS_INTENT: {
            ASTIntent* i = (ASTIntent*)node;
            write_string(out, i->intent_name);
            ast_serialize_recursive(i->target, out);
            break;
        }
        case AST_AI_LOGIC: {
            write_string(out, ((ASTAiLogic*)node)->query);
            break;
        }
        case AST_PANIC: {
            write_string(out, ((ASTPanic*)node)->message);
            break;
        }
        default: break;
    }

    ast_serialize_recursive(node->next, out);
}

void ast_serialize(ASTNode* root, FILE* out) {
    if (!out) return;
    ast_serialize_recursive(root, out);
}

static ASTNode* ast_deserialize_recursive(FILE* in) {
    int type_val;
    if (fread(&type_val, sizeof(int), 1, in) != 1) return NULL;
    if (type_val == -1) return NULL;

    ASTNodeType type = (ASTNodeType)type_val;
    ASTNode* node = ast_create(type);
    fread(&node->line, sizeof(int), 1, in);
    fread(&node->col, sizeof(int), 1, in);

    switch(type) {
        case AST_VAR_DECL: {
            ASTVarDecl* d = (ASTVarDecl*)node;
            d->name = read_string(in);
            d->type_name = read_string(in);
            fread(&d->is_const, sizeof(int), 1, in);
            fread(&d->is_hard_typed, sizeof(int), 1, in);
            fread(&d->is_public, sizeof(int), 1, in);
            fread(&d->is_exported, sizeof(int), 1, in);
            d->initializer = ast_deserialize_recursive(in);
            d->inline_type = ast_deserialize_recursive(in);
            break;
        }
        case AST_FUNC_DECL: {
            ASTFuncDecl* f = (ASTFuncDecl*)node;
            f->name = read_string(in);
            f->return_type = read_string(in);
            fread(&f->is_extern, sizeof(int), 1, in);
            fread(&f->is_public, sizeof(int), 1, in);
            fread(&f->is_exported, sizeof(int), 1, in);
            f->params = ast_deserialize_recursive(in);
            f->body = ast_deserialize_recursive(in);
            break;
        }
        case AST_STRUCT_DECL:
        case AST_UNION_DECL: {
            ASTStructDecl* s = (ASTStructDecl*)node;
            s->name = read_string(in);
            fread(&s->is_packed, sizeof(int), 1, in);
            fread(&s->is_union, sizeof(int), 1, in);
            fread(&s->is_public, sizeof(int), 1, in);
            fread(&s->is_exported, sizeof(int), 1, in);
            s->fields = ast_deserialize_recursive(in);
            break;
        }
        case AST_LITERAL: {
            ASTLiteral* l = (ASTLiteral*)node;
            fread(&l->lit_type, sizeof(int), 1, in);
            fread(&l->int_value, sizeof(long long), 1, in);
            fread(&l->float_value, sizeof(double), 1, in);
            l->string_value = read_string(in);
            break;
        }
        case AST_IDENTIFIER: {
            ((ASTIdentifier*)node)->name = read_string(in);
            break;
        }
        case AST_BINARY_EXPR:
        case AST_ASSIGNMENT:
        case AST_INDEX_ACCESS:
        case AST_NEXUS_FLOW:
        case AST_NEXUS_PIPE: {
            ASTBinaryExpr* b = (ASTBinaryExpr*)node;
            fread(&b->op, sizeof(OmniTokenType), 1, in);
            b->left = ast_deserialize_recursive(in);
            b->right = ast_deserialize_recursive(in);
            if (type == AST_NEXUS_FLOW || type == AST_NEXUS_PIPE) {
                 ASTNexusFlow* f = (ASTNexusFlow*)node;
                 fread(&f->retry_count, sizeof(int), 1, in);
                 fread(&f->retry_delay, sizeof(int), 1, in);
                 f->catch_block = ast_deserialize_recursive(in);
            }
            break;
        }
        case AST_UNARY_EXPR:
        case AST_RETURN:
        case AST_POSTFIX_EXPR:
        case AST_CAST:
        case AST_UNPACK:
        case AST_NEXUS_HALT: {
            ASTUnaryExpr* u = (ASTUnaryExpr*)node;
            fread(&u->op, sizeof(OmniTokenType), 1, in);
            u->operand = ast_deserialize_recursive(in);
            break;
        }
        case AST_BLOCK:
        case AST_PROGRAM:
        case AST_ASM_BLOCK: {
            ((ASTBlock*)node)->statements = ast_deserialize_recursive(in);
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
            c->callee = ast_deserialize_recursive(in);
            c->args = ast_deserialize_recursive(in);
            break;
        }
        case AST_IF: {
            ASTIf* i = (ASTIf*)node;
            i->condition = ast_deserialize_recursive(in);
            i->then_branch = ast_deserialize_recursive(in);
            i->else_branch = ast_deserialize_recursive(in);
            break;
        }
        case AST_LOOP: {
            ASTLoop* l = (ASTLoop*)node;
            fread(&l->loop_type, sizeof(int), 1, in);
            l->init = ast_deserialize_recursive(in);
            l->condition = ast_deserialize_recursive(in);
            l->increment = ast_deserialize_recursive(in);
            l->body = ast_deserialize_recursive(in);
            l->collection = ast_deserialize_recursive(in);
            l->variable = ast_deserialize_recursive(in);
            break;
        }
        case AST_GROUP_DECL: {
            ASTGroupDecl* g = (ASTGroupDecl*)node;
            g->name = read_string(in);
            fread(&g->is_public, sizeof(int), 1, in);
            fread(&g->is_exported, sizeof(int), 1, in);
            g->members = ast_deserialize_recursive(in);
            break;
        }
        case AST_METHOD: {
            ASTMethod* m = (ASTMethod*)node;
            m->public_name = read_string(in);
            m->implementation = ast_deserialize_recursive(in);
            break;
        }
        case AST_USE: {
            ASTUse* u = (ASTUse*)node;
            u->module_name = read_string(in);
            u->alias = read_string(in);
            break;
        }
        case AST_DEFER: {
            ((ASTDefer*)node)->body = ast_deserialize_recursive(in);
            break;
        }
        case AST_THREAD_BLOCK: {
            ASTThreadBlock* t = (ASTThreadBlock*)node;
            t->name = read_string(in);
            t->body = ast_deserialize_recursive(in);
            t->mode = ast_deserialize_recursive(in);
            t->timeout = ast_deserialize_recursive(in);
            break;
        }
        case AST_MEMBER_ACCESS: {
            ASTMemberAccess* m = (ASTMemberAccess*)node;
            m->member_name = read_string(in);
            m->object = ast_deserialize_recursive(in);
            break;
        }
        case AST_TUPLE: {
            ((ASTTuple*)node)->elements = ast_deserialize_recursive(in);
            break;
        }
        case AST_ENUM_DECL: {
            ASTEnumDecl* e = (ASTEnumDecl*)node;
            e->name = read_string(in);
            e->variants = ast_deserialize_recursive(in);
            break;
        }
        case AST_ENUM_VARIANT: {
            ASTEnumVariant* v = (ASTEnumVariant*)node;
            v->name = read_string(in);
            v->data = ast_deserialize_recursive(in);
            break;
        }
        case AST_NEWTYPE_DECL: {
            ASTNewTypeDecl* nt = (ASTNewTypeDecl*)node;
            nt->name = read_string(in);
            nt->target_type = read_string(in);
            break;
        }
        case AST_NEXUS_CAPTURE:
        case AST_LISTEN: {
            ASTNexusCapture* c = (ASTNexusCapture*)node;
            c->handler_name = read_string(in);
            c->expr = ast_deserialize_recursive(in);
            break;
        }
        case AST_NEXUS_LABEL: {
            ASTNexusLabel* l = (ASTNexusLabel*)node;
            l->label_name = read_string(in);
            l->statement = ast_deserialize_recursive(in);
            break;
        }
        case AST_NEXUS_JUMP: {
            ((ASTNexusJump*)node)->label_name = read_string(in);
            break;
        }
        case AST_NEXUS_INTENT: {
            ASTIntent* i = (ASTIntent*)node;
            i->intent_name = read_string(in);
            i->target = ast_deserialize_recursive(in);
            break;
        }
        case AST_AI_LOGIC: {
            ((ASTAiLogic*)node)->query = read_string(in);
            break;
        }
        case AST_PANIC: {
            ((ASTPanic*)node)->message = read_string(in);
            break;
        }
        default: break;
    }

    node->next = ast_deserialize_recursive(in);
    return node;
}

ASTNode* ast_deserialize(FILE* in) {
    if (!in) return NULL;
    return ast_deserialize_recursive(in);
}