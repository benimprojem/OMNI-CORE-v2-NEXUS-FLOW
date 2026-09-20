/*
 * Omni Core v2 Compiler (OCC)
 * Codename: Nexus Flow
 * Phase: DLL Integration
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser/parser.h"
#include "parser/semantic.h"

#ifdef _WIN32
#include <windows.h>
typedef int (*CodegenRunFn)(const char*, const char*, const char*);
#endif

// Basit Config Okuyucu
static void get_config_language(char* out_lang, size_t size) {
    // Varsayılan: İngilizce
    strncpy(out_lang, "en", size);
    
    FILE* f = fopen("occ.ocf", "r");
    if (!f) return; // Dosya yoksa varsayılanı kullan

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // language = "tr" satırını ara
        char* key = strstr(line, "language");
        if (key) {
            char* val = strchr(key, '"');
            if (val) {
                val++; // İlk tırnağı geç
                char* end = strchr(val, '"');
                if (end) {
                    *end = '\0';
                    strncpy(out_lang, val, size);
                    break;
                }
            }
        }else {break;}
    }
    fclose(f);
}

// Target listeleme (Windows için)
static void list_targets() {
    printf("\nDesteklenen Target Mimarileri (targets/):\n");
    printf("-------------------------------------------\n");
    #ifdef _WIN32
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile("../targets/*.target", &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            printf("  - %s\n", findData.cFileName);
        } while (FindNextFile(hFind, &findData));
        FindClose(hFind);
    } else {
        printf("  (Hiçbir .target dosyası bulunamadı: ../targets/)\n");
    }
    #else
    printf("  (Bu platformda listeleme henüz desteklenmiyor)\n");
    #endif
    printf("-------------------------------------------\n\n");
}

int main(int argc, char **argv) {
    // Config'den dili oku ve mesajları yükle
    char lang[16];
    get_config_language(lang, sizeof(lang));
    char lang_path[256];
    snprintf(lang_path, sizeof(lang_path), "../inc/messages_%s.conf", lang);
    msg_init(lang_path);

    if (argc < 2) {
        printf("%s\n", msg_get(MSG_USAGE));
        list_targets();
        return 1;
    }

    // Flag kontrolü ve target parsing
    const char* target_name = "x86_64_baremetal"; // Default short name
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("%s\n", msg_get(MSG_USAGE));
            list_targets();
            return 0;
        }
        if (strcmp(argv[i], "--target") == 0) {
            if (i + 1 < argc && argv[i+1][0] != '-') {
                target_name = argv[++i];
            } else if (i + 1 < argc && strcmp(argv[i+1], "-h") == 0) {
                list_targets();
                return 0;
            }
        }
    }

    // Target path oluştur (kısa isimden tam yol)
    char target_path[256];
    snprintf(target_path, sizeof(target_path), "../targets/%s.target", target_name);

    const char* source_file = argv[1];
    if (source_file[0] == '-') {
        printf("Hata: Gecersiz kaynak dosyasi veya flag: %s\n", source_file);
        return 1;
    }
    printf(msg_get(MSG_OCC_PROCESSING), source_file);

    ParserContext* ctx = parser_init(source_file);
    if (!ctx) {
        printf("%s\n", msg_get(MSG_OCC_FAIL_INIT));
        printf("[DEBUG] parser_init failed for file: %s\n", source_file);
        return 1;
    }
    printf("[DEBUG] parser_init succeeded, running parser...\n");

    if (parser_run(ctx) == 0 && ctx->ast_root != NULL) {
        printf("%s\n", msg_get(MSG_OCC_SUCCESS_SEMANTIC));
        
        SemanticContext sem_ctx;
        semantic_init(&sem_ctx, source_file, lang_path);
        semantic_check(&sem_ctx, (ASTNode*)ctx->ast_root);
        
        if (sem_ctx.error_count == 0) {
            // Dosya uzantısını temizle ve base name oluştur
            char base_name[256];
            strncpy(base_name, source_file, sizeof(base_name));
            base_name[sizeof(base_name)-1] = '\0';
            
            char *dot = strrchr(base_name, '.');
            char *slash = strrchr(base_name, '/');
            char *backslash = strrchr(base_name, '\\');
            char *last_sep = slash > backslash ? slash : backslash;
            
            if (dot && dot > last_sep) {
                *dot = '\0';
            }

            char ir_path[256];
            snprintf(ir_path, sizeof(ir_path), "%s.oir", base_name);
            parser_save_ir(ctx, ir_path);
            printf("IR kaydedildi: %s\n", ir_path);

            #ifdef _WIN32
            HMODULE hCodegen = LoadLibrary("codegen.dll");
            if (hCodegen) {
                CodegenRunFn run = (CodegenRunFn)GetProcAddress(hCodegen, "codegen_run");
                if (run) {
                    char asm_path[256];
                    snprintf(asm_path, sizeof(asm_path), "%s.s", base_name);
                    // Kullanıcı tarafından belirtilen veya varsayılan target kullan
                    run(ir_path, target_path, asm_path);
                } else {
                    printf("Hata: codegen_run sembolü bulunamadı.\n");
                }
                FreeLibrary(hCodegen);
            } else {
                printf("Uyarı: codegen.dll bulunamadı, sadece IR üretildi.\n");
            }
            #endif
        }

        semantic_print_errors(&sem_ctx);
        semantic_destroy(&sem_ctx);
    } else {
        printf("%s\n", msg_get(MSG_OCC_FAIL_PARSER));
    }
    
    parser_print_errors(ctx);
    parser_destroy(ctx);
    return 0;
}