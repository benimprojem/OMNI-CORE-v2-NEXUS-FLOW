#include "codegen.h"
#include "target_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration
static void codegen_node(ASTNode* node, FILE* out, TargetConfig* config);

static void codegen_binary(ASTBinaryExpr* b, FILE* out, TargetConfig* config) {
    codegen_node(b->left, out, config);
    fprintf(out, "    push rax\n");
    codegen_node(b->right, out, config);
    fprintf(out, "    mov rbx, rax\n");
    fprintf(out, "    pop rax\n");
    
    switch (b->op) {
        case TOKEN_PLUS:  fprintf(out, "    add rax, rbx\n"); break;
        case TOKEN_MINUS: fprintf(out, "    sub rax, rbx\n"); break;
        case TOKEN_STAR:  fprintf(out, "    imul rax, rbx\n"); break;
        case TOKEN_SLASH: fprintf(out, "    idiv rbx\n");     break;
        default: break;
    }
}

static void codegen_func_decl(ASTFuncDecl* fn, FILE* out, TargetConfig* config) {
    // ULTRA: Her fonksiyonu kendi section'ına koy (Function-Level Linking desteği)
    fprintf(out, "\nsection .text_%s\n", fn->name);
    fprintf(out, "global %s\n", fn->name);
    fprintf(out, "%s:\n", fn->name);
    
    // Prologue
    fprintf(out, "    push rbp\n");
    fprintf(out, "    mov rbp, rsp\n");
    
    // Body
    codegen_node(fn->body, out, config);
    
    // Epilogue (Varsayılan return eğer sonda yoksa)
    fprintf(out, "    mov rsp, rbp\n");
    fprintf(out, "    pop rbp\n");
    fprintf(out, "    ret\n");
}

static void codegen_node(ASTNode* node, FILE *out, TargetConfig* config) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM: {
            ASTProgram* p = (ASTProgram*)node;
            ASTNode* decl = p->declarations;
            while (decl) {
                codegen_node(decl, out, config);
                decl = decl->next;
            }
            break;
        }
        case AST_FUNC_DECL: {
            codegen_func_decl((ASTFuncDecl*)node, out, config);
            break;
        }
        case AST_LITERAL: {
            ASTLiteral* lit = (ASTLiteral*)node;
            if (lit->lit_type == 0) { // Int
                fprintf(out, "    mov rax, %lld\n", lit->int_value);
            }
            break;
        }
        case AST_BINARY_EXPR: {
            codegen_binary((ASTBinaryExpr*)node, out, config);
            break;
        }
        case AST_BLOCK: {
            ASTBlock* b = (ASTBlock*)node;
            ASTNode* stmt = b->statements;
            while (stmt) {
                codegen_node(stmt, out, config);
                stmt = stmt->next;
            }
            break;
        }
        case AST_RETURN: {
            ASTUnaryExpr* r = (ASTUnaryExpr*)node;
            if (r->operand) codegen_node(r->operand, out, config);
            fprintf(out, "    mov rsp, rbp\n");
            fprintf(out, "    pop rbp\n");
            fprintf(out, "    ret\n");
            break;
        }
        default:
            fprintf(out, "    ; TODO: Implement codegen for node type %d\n", node->type);
            break;
    }
}

int codegen_run(const char* ir_path, const char* target_path, const char* out_path) {
    printf("[CODEGEN] Baslatiliyor: %s -> %s\n", ir_path, out_path);

    // 1. IR Yükle (Deserialization)
    FILE* ir_f = fopen(ir_path, "rb");
    if (!ir_f) {
        printf("[CODEGEN] Hata: IR dosyasi acilamadi: %s\n", ir_path);
        return 1;
    }
    ASTNode* root = ast_deserialize(ir_f);
    fclose(ir_f);

    if (!root) {
        printf("[CODEGEN] Hata: IR deserialize edilemedi.\n");
        return 1;
    }

    // 2. Parse and validate the target descriptor before emitting any code.
    TargetConfig config;
    char target_error[256];
    if (!target_config_load(target_path, &config, target_error, sizeof(target_error))) {
        fprintf(stderr, "[CODEGEN] Target descriptor error: %s\n", target_error);
        ast_destroy(root);
        return 1;
    }
    if (!target_backend_check(&config, target_error, sizeof(target_error))) {
        fprintf(stderr, "[CODEGEN] Unsupported target: %s\n", target_error);
        ast_destroy(root);
        return 1;
    }
    printf("[CODEGEN] Hedef: %s (%s/%u, %s, %s)\n",
        config.identifier, config.arch, config.bits, config.os, config.format);

    // 3. Kod Üretimi (Başlangıç Şablonu)
    FILE* out_f = fopen(out_path, "w");
    if (!out_f) {
        printf("[CODEGEN] Hata: Cikti dosyasi acilamadi: %s\n", out_path);
        ast_destroy(root);
        return 1;
    }

    fprintf(out_f, "; OmniCore v2 Backend Output\n");
    fprintf(out_f, "; Target: %s (%s/%u), OS: %s, Format: %s\n", config.identifier, config.arch, config.bits, config.os, config.format);
    fprintf(out_f, "; Base Address: 0x%llX\n\n", config.base_addr);
    
    // Basit Nasm şablonu
    fprintf(out_f, "[BITS 64]\n");
    if (config.base_addr > 0) {
        fprintf(out_f, "org 0x%llX\n", config.base_addr);
    }
    
    fprintf(out_f, "section .text\n");
    fprintf(out_f, "global _start\n\n");
    fprintf(out_f, "_start:\n");
    fprintf(out_f, "    ; --- NexusFlow IR Generated Code ---\n");
    codegen_node(root, out_f, &config);
    fprintf(out_f, "    hlt ; Halt for baremetal safety\n");

    fclose(out_f);
    ast_destroy(root);

    printf("[CODEGEN] Basariyla tamamlandi.\n");
    return 0;
}
