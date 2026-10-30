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
#else
#include <dlfcn.h>
#include <dirent.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <stdint.h>
#endif

#define OCC_PATH_BUFFER 4096

typedef int (*CodegenRunFn)(const char*, const char*, const char*);

static int get_executable_dir(char* out_dir, size_t out_size) {
    if (!out_dir || out_size == 0) return 0;

    char executable_path[OCC_PATH_BUFFER];

#if defined(_WIN32)
    DWORD length = GetModuleFileNameA(NULL, executable_path, (DWORD)sizeof(executable_path));
    if (length == 0 || length >= sizeof(executable_path)) return 0;
    executable_path[length] = '\0';
#elif defined(__APPLE__)
    uint32_t length = (uint32_t)sizeof(executable_path);
    if (_NSGetExecutablePath(executable_path, &length) != 0) return 0;
    executable_path[sizeof(executable_path) - 1] = '\0';
#else
    ssize_t length = readlink("/proc/self/exe", executable_path, sizeof(executable_path) - 1);
    if (length < 0 || (size_t)length >= sizeof(executable_path) - 1) return 0;
    executable_path[length] = '\0';
#endif

    char* slash = strrchr(executable_path, '/');
    char* backslash = strrchr(executable_path, '\\');
    char* last_separator = slash;
    if (backslash && (!last_separator || backslash > last_separator)) {
        last_separator = backslash;
    }
    if (!last_separator) return 0;

    if (last_separator == executable_path ||
        (last_separator == executable_path + 2 && executable_path[1] == ':')) {
        last_separator[1] = '\0';
    } else {
        *last_separator = '\0';
    }

    size_t dir_length = strlen(executable_path);
    if (dir_length + 1 > out_size) return 0;
    memcpy(out_dir, executable_path, dir_length + 1);
    return 1;
}

static int path_join(char* out, size_t out_size, const char* base, const char* relative) {
    if (!out || out_size == 0 || !base || !relative) return 0;

    size_t base_length = strlen(base);
    size_t relative_length = strlen(relative);
    int has_separator = base_length > 0 &&
        (base[base_length - 1] == '/' || base[base_length - 1] == '\\');
    size_t separator_length = (base_length > 0 && !has_separator) ? 1u : 0u;

    if (base_length + separator_length + relative_length + 1u > out_size) return 0;

    memcpy(out, base, base_length);
    if (separator_length) {
#if defined(_WIN32)
        out[base_length++] = '\\';
#else
        out[base_length++] = '/';
#endif
    }
    memcpy(out + base_length, relative, relative_length + 1u);
    return 1;
}

// Basit Config Okuyucu
static void get_config_language(const char* config_path, char* out_lang, size_t size) {
    // Varsayılan: İngilizce
    if (!out_lang || size == 0) return;
    strncpy(out_lang, "en", size - 1);
    out_lang[size - 1] = '\0';

    FILE* f = fopen(config_path, "r");
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

// Target listeleme: platformdan bağımsız olarak executable yanındaki targets dizinini kullanır.
static void list_targets(const char* target_dir) {
    printf("\nDesteklenen Target Mimarileri (targets/):\n");
    printf("-------------------------------------------\n");

#if defined(_WIN32)
    char pattern[OCC_PATH_BUFFER];
    if (!path_join(pattern, sizeof(pattern), target_dir, "*.target")) {
        printf("  (Target dizini yolu cok uzun)\n");
        printf("-------------------------------------------\n\n");
        return;
    }

    WIN32_FIND_DATAA find_data;
    HANDLE search = FindFirstFileA(pattern, &find_data);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                printf("  - %s\n", find_data.cFileName);
            }
        } while (FindNextFileA(search, &find_data));
        FindClose(search);
    } else {
        printf("  (Target dosyasi bulunamadi: %s)\n", target_dir);
    }
#else
    DIR* directory = opendir(target_dir);
    if (!directory) {
        printf("  (Target dizini acilamadi: %s)\n", target_dir);
    } else {
        struct dirent* entry;
        int found = 0;
        while ((entry = readdir(directory)) != NULL) {
            size_t name_length = strlen(entry->d_name);
            if (name_length >= 7 &&
                strcmp(entry->d_name + name_length - 7, ".target") == 0) {
                printf("  - %s\n", entry->d_name);
                found = 1;
            }
        }
        if (!found) printf("  (Hicbir .target dosyasi bulunamadi)\n");
        closedir(directory);
    }
#endif

    printf("-------------------------------------------\n\n");
}

int main(int argc, char **argv) {
    int exit_code = 0;
    /* Resolve runtime assets from the executable location, not the caller's CWD. */
    char executable_dir[OCC_PATH_BUFFER];
    char config_path[OCC_PATH_BUFFER];
    char target_dir[OCC_PATH_BUFFER];
    char lang_path[OCC_PATH_BUFFER];
    char lang_relative[64];

    if (!get_executable_dir(executable_dir, sizeof(executable_dir)) ||
        !path_join(config_path, sizeof(config_path), executable_dir, "occ.ocf") ||
        !path_join(target_dir, sizeof(target_dir), executable_dir, "../targets")) {
        fprintf(stderr, "Hata: OCC runtime dizinleri cozumlenemedi.\n");
        return 1;
    }

    char lang[16];
    get_config_language(config_path, lang, sizeof(lang));
    int lang_length = snprintf(lang_relative, sizeof(lang_relative), "lang/messages_%s.conf", lang);
    if (lang_length < 0 || (size_t)lang_length >= sizeof(lang_relative) ||
        !path_join(lang_path, sizeof(lang_path), executable_dir, lang_relative)) {
        fprintf(stderr, "Hata: Dil dosyasi yolu olusturulamadi.\n");
        return 1;
    }
    msg_init(lang_path);

    if (argc < 2) {
        printf("%s\n", msg_get(MSG_USAGE));
        list_targets(target_dir);
        return 1;
    }

    // CLI argümanlarını sıradan bağımsız ayrıştır.
    const char* target_name = "x86_64_baremetal";
    const char* source_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("%s\n", msg_get(MSG_USAGE));
            list_targets(target_dir);
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
        list_targets(target_dir);
        return 1;
    }

    // Target descriptor path is anchored to the executable's runtime layout.
    char target_filename[512];
    char target_path[OCC_PATH_BUFFER];
    int target_filename_length = snprintf(target_filename, sizeof(target_filename), "%s.target", target_name);
    if (target_filename_length < 0 ||
        (size_t)target_filename_length >= sizeof(target_filename) ||
        !path_join(target_path, sizeof(target_path), target_dir, target_filename)) {
        fprintf(stderr, "Hata: Target dosyasi yolu olusturulamadi.\n");
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
            char base_name[512];
            strncpy(base_name, source_file, sizeof(base_name) - 1);
            base_name[sizeof(base_name) - 1] = '\0';

            char *dot = strrchr(base_name, '.');
            char *slash = strrchr(base_name, '/');
            char *backslash = strrchr(base_name, '\\');
            char *last_sep = slash;
            if (backslash && (!last_sep || backslash > last_sep)) {
                last_sep = backslash;
            }

            if (dot && (!last_sep || dot > last_sep)) {
                *dot = '\0';
            }

            char ir_path[520];
            snprintf(ir_path, sizeof(ir_path), "%s.oir", base_name);
            parser_save_ir(ctx, ir_path);
            printf("IR kaydedildi: %s\n", ir_path);

            /* Load the platform-specific codegen module beside the executable. */
            char codegen_path[OCC_PATH_BUFFER];
#if defined(_WIN32)
            const char* codegen_filename = "codegen.dll";
#elif defined(__APPLE__)
            const char* codegen_filename = "libcodegen.dylib";
#else
            const char* codegen_filename = "libcodegen.so";
#endif
            if (!path_join(codegen_path, sizeof(codegen_path), executable_dir, codegen_filename)) {
                fprintf(stderr, "Hata: Codegen modul yolu olusturulamadi.\n");
                exit_code = 1;
            }

#if defined(_WIN32)
            HMODULE hCodegen = exit_code == 0 ? LoadLibraryA(codegen_path) : NULL;
            CodegenRunFn run = hCodegen
                ? (CodegenRunFn)GetProcAddress(hCodegen, "codegen_run")
                : NULL;
#else
            void* hCodegen = exit_code == 0 ? dlopen(codegen_path, RTLD_NOW | RTLD_LOCAL) : NULL;
            CodegenRunFn run = hCodegen
                ? (CodegenRunFn)dlsym(hCodegen, "codegen_run")
                : NULL;
#endif

            if (!hCodegen) {
                fprintf(stderr, "Hata: Codegen modulu yuklenemedi.\n");
                exit_code = 1;
            } else if (!run) {
                fprintf(stderr, "Hata: codegen_run sembolu bulunamadi.\n");
                exit_code = 1;
            } else {
                char asm_path[520];
                snprintf(asm_path, sizeof(asm_path), "%s.s", base_name);
                if (run(ir_path, target_path, asm_path) != 0) {
                    fprintf(stderr, "Hata: Codegen islemi basarisiz oldu.\n");
                    exit_code = 1;
                }
            }

#if defined(_WIN32)
            if (hCodegen) FreeLibrary(hCodegen);
#else
            if (hCodegen) dlclose(hCodegen);
#endif
        } else {
            exit_code = 1;
        }

        semantic_print_errors(&sem_ctx);
        semantic_destroy(&sem_ctx);
    } else {
        printf("%s\n", msg_get(MSG_OCC_FAIL_PARSER));
        exit_code = 1;
    }

    parser_print_errors(ctx);
    parser_destroy(ctx);
    return exit_code;
}
