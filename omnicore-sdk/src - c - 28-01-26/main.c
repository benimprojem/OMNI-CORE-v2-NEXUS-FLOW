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
    if (!out_lang || size == 0) return;
    strncpy(out_lang, "en", size - 1);
    out_lang[size - 1] = '\0';

    FILE* f = fopen("occ.ocf", "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "language", 8) != 0) continue;

        p += 8;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '=') continue;
        p++;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '"') continue;

        p++;
        char* end = strchr(p, '"');
        if (!end) continue;

        size_t len = (size_t)(end - p);
        if (len >= size) len = size - 1;
        memcpy(out_lang, p, len);
        out_lang[len] = '\0';
        break;
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

    // CLI argümanlarını sıradan bağımsız ayrıştır.
    const char* target_name = "x86_64_baremetal";
    const char* source_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("%s\n", msg_get(MSG_USAGE));
            list_targets();
            return 0;
        } else if (strcmp(argv[i], "--target") == 0) {
            if (i + 1 >= argc || argv[i + 1][0] == '-') {
                fprintf(stderr, "Hata: --target bir hedef adı gerektirir.\n");
                return 1;
            }
            target_name = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Hata: Bilinmeyen seçenek: %s\n", argv[i]);
            return 1;
        } else if (!source_file) {
            source_file = argv[i];
        } else {
            fprintf(stderr, "Hata: Birden fazla kaynak dosyası belirtildi: %s\n", argv[i]);
            return 1;
        }
    }

    if (!source_file) {
        printf("%s\n", msg_get(MSG_USAGE));
        list_targets();
        return 1;
    }

    // Target path oluştur (kısa isimden tam yol)
    char target_path[256];
    snprintf(target_path, sizeof(target_path), "../targets/%s.target", target_name);

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
            char *last_sep = slash;
            if (backslash && (!last_sep || backslash > last_sep)) {
                last_sep = backslash;
            }
            
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