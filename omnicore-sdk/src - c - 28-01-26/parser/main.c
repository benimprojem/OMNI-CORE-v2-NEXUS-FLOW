/*
 * Omni Core v2 Compiler (OCC)
 * Codename: Nexus Flow
 * Phase: Bootstrap & Parser (C Implementation)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Henüz oluşturulmamış modüller için placeholder
// #include "targets/target_manager.h"
// #include "ncb/bell_system.h"
#include "parser/parser.h"
#include "parser/semantic.h"

#define OCC_VERSION "2.0.0-alpha (Nexus Flow)"

void init_nexus_flow_sequence() {
    printf("==============================================\n");
    printf(" Omni Core Compiler (OCC) - v%s\n", OCC_VERSION);
    printf("==============================================\n");
    printf("[SYS] Constitution v2.0   : LOADED\n");
    printf("[SYS] Target Architecture : (Multi-Target Ready)\n");
    printf("[SYS] NCB System          : (Event-Driven)\n");
    printf("[SYS] Parser Engine       : ACTIVE (LALR-1)\n");
    printf("----------------------------------------------\n");
}

int main(int argc, char **argv) {
    init_nexus_flow_sequence();

    if (argc < 2) {
        printf("[ERR] Usage: occ <source_file.nxf> [options]\n");
        return 1;
    }

    const char* source_file = argv[1];
    printf("[INFO] Processing Source: %s\n", source_file);

    // 1. Aşama: Parser Başlatma
    printf("[STEP 1] Initializing Parser...\n");
    ParserContext* ctx = parser_init(source_file);
    
    if (!ctx) {
        printf("[FATAL] Parser initialization failed.\n");
        return 1;
    }

    // Parser çalıştır (Tokenizasyon ve AST Üretimi)
    int parser_result = parser_run(ctx);
    
    if (parser_result == 0 && ctx->ast_root != NULL) {
        printf("[STEP 2] Initializing Semantic Analysis...\n");
        
        SemanticContext sem_ctx;
        semantic_init(&sem_ctx, source_file, LANG_TR);
        semantic_check(&sem_ctx, (ASTNode*)ctx->ast_root);
        
        semantic_print_errors(&sem_ctx);
        semantic_destroy(&sem_ctx);
    } else {
        printf("[INFO] Semantic analysis skipped due to parser errors.\n");
    }
    
    // Hataları en sonda bas
    parser_print_errors(ctx);
    parser_destroy(ctx);
    return 0;
}