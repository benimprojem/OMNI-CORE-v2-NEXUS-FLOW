#include "codegen.h"
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

// Basit .target dosyası parser'ı
static void parse_target(const char* path, TargetConfig* config) {
    // Varsayılanlar
    strcpy(config->arch, "x86_64");
    strcpy(config->os, "none");
    strcpy(config->format, "bin");
    config->base_addr = 0;
    config->stack_size = 4096;
    config->region_count = 0;
    config->port_count = 0;
    config->abi = ABI_NONE;
    config->features = FEAT_NONE;

    if (!path) return;
    FILE* f = fopen(path, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        
        char key[64], val[128];
        if (sscanf(line, "%63[^:]: %127[^\r\n]", key, val) == 2) {
            if (strcmp(key, "arch") == 0) strcpy(config->arch, val);
            else if (strcmp(key, "os") == 0) strcpy(config->os, val);
            else if (strcmp(key, "format") == 0) strcpy(config->format, val);
            else if (strcmp(key, "base") == 0) config->base_addr = strtoull(val, NULL, 0);
            else if (strcmp(key, "stack") == 0) config->stack_size = strtoull(val, NULL, 0);
            else if (strcmp(key, "abi") == 0) {
                if (strcmp(val, "win64") == 0) config->abi = ABI_WIN64;
                else if (strcmp(val, "sysv") == 0) config->abi = ABI_SYSV;
                else if (strcmp(val, "baremetal") == 0) config->abi = ABI_BAREMETAL;
            }
            else if (strcmp(key, "features") == 0) {
                char* token = strtok(val, ",");
                while (token) {
                    if (strcmp(token, "mmx") == 0) config->features |= FEAT_MMX;
                    else if (strcmp(token, "sse") == 0) config->features |= FEAT_SSE;
                    else if (strcmp(token, "sse2") == 0) config->features |= FEAT_SSE2;
                    else if (strcmp(token, "sse3") == 0) config->features |= FEAT_SSE3;
                    else if (strcmp(token, "sse4") == 0) config->features |= FEAT_SSE4;
                    else if (strcmp(token, "avx") == 0) config->features |= FEAT_AVX;
                    else if (strcmp(token, "avx2") == 0) config->features |= FEAT_AVX2;
                    else if (strcmp(token, "avx512") == 0) config->features |= FEAT_AVX512;
                    token = strtok(NULL, ",");
                }
            }
            else if (strcmp(key, "memory") == 0) {
                if (config->region_count < 16) {
                    MemoryRegion* r = &config->regions[config->region_count++];
                    char type_str[16];
                    if (sscanf(val, "%31s %15s %llx %llx", r->name, type_str, &r->start, &r->size) >= 4) {
                        if (strcmp(type_str, "flash") == 0) r->type = MEM_FLASH;
                        else if (strcmp(type_str, "ram") == 0) r->type = MEM_RAM;
                        else if (strcmp(type_str, "io") == 0) r->type = MEM_IO;
                    }
                }
            }
            else if (strcmp(key, "port") == 0) {
                if (config->port_count < 64) {
                    Peripheral* p = &config->ports[config->port_count++];
                    sscanf(val, "%31s %llx", p->name, &p->address);
                }
            }
        }
    }
    fclose(f);
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

    // 2. Target Konfigürasyonu Oku
    TargetConfig config;
    parse_target(target_path, &config);
    printf("[CODEGEN] Hedef: %s (%s, %s)\n", config.arch, config.os, config.format);

    // 3. Kod Üretimi (Başlangıç Şablonu)
    FILE* out_f = fopen(out_path, "w");
    if (!out_f) {
        printf("[CODEGEN] Hata: Cikti dosyasi acilamadi: %s\n", out_path);
        ast_destroy(root);
        return 1;
    }

    fprintf(out_f, "; OmniCore v2 Backend Output\n");
    fprintf(out_f, "; Target: %s, OS: %s, Format: %s\n", config.arch, config.os, config.format);
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
