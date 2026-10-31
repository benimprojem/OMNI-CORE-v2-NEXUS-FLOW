/*
 * Omni Core v2 Compiler (OCC)
 * Codename: Nexus Flow
 * Phase: DLL Integration
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser/parser.h"
#include "parser/lexer.h"
#include "parser/semantic.h"
#include "module/occ_module_loader.h"
#include "oir/oir_payload.h"
#include "oir/oir_token_stream.h"
#include "oir/oir_diagnostics.h"
#include "oir/oir_ast_artifact.h"
#include "oir/oir_ast_node_table.h"
#include "oir/oir_ast_graph.h"
#include "core/compilation.h"
#include "core/failure_store.h"

#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <dlfcn.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <stdint.h>
#endif

#define OCC_PATH_BUFFER 4096

typedef int (*CodegenRunFn)(const char*, const char*, const char*);

#define OCC_FRONTEND_TOKEN_LIMIT 1000000u
#define OCC_AST_OUTPUT_MAX (256u * 1024u * 1024u)

static int path_join(char* out, size_t out_size, const char* base, const char* relative);
static void occ_add_host_diagnostic(
    OccCompilationContext* job,
    uint32_t stage_id,
    uint32_t code,
    const char* message
);


static void occ_print_job_diagnostics(const OccDiagnosticCollector* diagnostics) {
    size_t i;
    if (!diagnostics) return;
    for (i = 0u; i < occ_diagnostics_count(diagnostics); ++i) {
        OccDiagnosticView view;
        if (!occ_diagnostics_get(diagnostics, i, &view) || !view.record) continue;
        fprintf(stderr, "[OCC stage=%u code=%u] %s",
            view.record->stage_id, view.record->code,
            view.message ? view.message : "Diagnostic");
        if (view.record->span.has_location) {
            fprintf(stderr, " (line %u, column %u; bytes %llu-%llu)",
                view.record->span.start_line,
                view.record->span.start_column,
                (unsigned long long)view.record->span.start_offset,
                (unsigned long long)view.record->span.end_offset);
        }
        fputc('\n', stderr);
        if (view.hint && view.hint[0]) fprintf(stderr, "  hint: %s\n", view.hint);
    }
}

static int occ_build_and_run_parser_module(
    const char* source_path,
    const char* executable_dir,
    OccCompilationContext* job,
    ParserContext** out_context
) {
    ParserContext* source_context = NULL;
    Token* tokens = NULL;
    OirTokenStreamInput* token_inputs = NULL;
    size_t token_count = 0u, token_capacity = 0u;
    uint8_t *source_payload = NULL, *source_artifact = NULL;
    uint8_t *token_payload = NULL, *token_artifact = NULL, *diagnostics_payload = NULL;
    uint8_t* ast_artifact = NULL;
    size_t source_payload_length = 0u, source_artifact_length = 0u;
    size_t token_payload_length = 0u, token_artifact_length = 0u;
    size_t diagnostics_payload_length = 0u, ast_artifact_length = 0u;
    size_t source_payload_capacity = 0u, token_payload_capacity = 0u;
    size_t token_artifact_capacity = 0u, diagnostics_payload_capacity = 0u;
    size_t path_length;
    OirArtifactHeader header;
    OirSectionInput sections[2];
    uint32_t section_count;
    OccModuleArtifactInputV1 module_inputs[2];
    OccLoadedStageModule loaded;
    char module_path[OCC_PATH_BUFFER];
    char loader_error[512];
    const char* module_filename;
    OirArtifactView ast_view;
    OirSectionView ast_payload_section;
    OirAstNodeTableView ast_table;
    ASTNode** node_storage = NULL;
    uint32_t* incoming_counts = NULL;
    uint8_t* visited = NULL;
    OirAstGraphStackEntry* graph_stack = NULL;
    ASTNode* ast_root = NULL;
    OirStatus oir_status;
    size_t i;
    int success = 0;

    if (out_context) *out_context = NULL;
    memset(&loaded, 0, sizeof(loaded));
    if (!source_path || !executable_dir || !job || !out_context) return 0;

    source_context = parser_init(source_path);
    if (!source_context) {
        occ_add_host_diagnostic(job, OCC_STAGE_OCC_HOST, 6001u,
            "The source file could not be opened by the lexer host.");
        goto cleanup;
    }
    /* parser_init() initializes lexer state; the host owns AST attachment later. */
    source_context->ast_root = NULL;
    parser_attach_diagnostics(source_context, &job->diagnostics, OCC_STAGE_LEXER);

    path_length = strlen(source_path);
    if (path_length > UINT32_MAX ||
        path_length > SIZE_MAX - OIR_SOURCE_UNIT_HEADER_SIZE ||
        source_context->source_len >
            SIZE_MAX - OIR_SOURCE_UNIT_HEADER_SIZE - path_length) {
        occ_add_host_diagnostic(job, OCC_STAGE_OCC_HOST, 6002u,
            "The source unit exceeds the supported OIR size.");
        goto cleanup;
    }

    token_capacity = 256u;
    tokens = (Token*)malloc(token_capacity * sizeof(*tokens));
    token_inputs = (OirTokenStreamInput*)malloc(token_capacity * sizeof(*token_inputs));
    if (!tokens || !token_inputs) goto allocation_failure;

    for (;;) {
        Token token = lexer_next_token(source_context);
        if (token_count == token_capacity) {
            size_t next_capacity;
            Token* next_tokens;
            OirTokenStreamInput* next_inputs;
            if (token_capacity >= OCC_FRONTEND_TOKEN_LIMIT) {
                occ_add_host_diagnostic(job, OCC_STAGE_LEXER, 6003u,
                    "The source exceeds the frontend token limit.");
                goto cleanup;
            }
            next_capacity = token_capacity * 2u;
            if (next_capacity > OCC_FRONTEND_TOKEN_LIMIT) next_capacity = OCC_FRONTEND_TOKEN_LIMIT;
            if (next_capacity > SIZE_MAX / sizeof(*tokens) ||
                next_capacity > SIZE_MAX / sizeof(*token_inputs)) goto allocation_failure;
            next_tokens = (Token*)realloc(tokens, next_capacity * sizeof(*tokens));
            if (!next_tokens) goto allocation_failure;
            tokens = next_tokens;
            next_inputs = (OirTokenStreamInput*)realloc(token_inputs,
                next_capacity * sizeof(*token_inputs));
            if (!next_inputs) goto allocation_failure;
            token_inputs = next_inputs;
            token_capacity = next_capacity;
        }
        tokens[token_count] = token;
        oir_status = oir_token_stream_input_from_lexer_token(
            &tokens[token_count], &token_inputs[token_count]);
        if (oir_status != OIR_STATUS_OK) {
            occ_add_host_diagnostic(job, OCC_STAGE_LEXER, 6004u,
                "A frontend token could not be represented in TOKEN_STREAM OIR.");
            goto cleanup;
        }
        ++token_count;
        if (token.type == TOKEN_EOF) break;
        if (token_count >= OCC_FRONTEND_TOKEN_LIMIT) {
            occ_add_host_diagnostic(job, OCC_STAGE_LEXER, 6003u,
                "The source exceeds the frontend token limit.");
            goto cleanup;
        }
    }
    if (token_count == 0u || token_count > UINT32_MAX ||
        job->diagnostics.count > UINT32_MAX ||
        job->diagnostics.allocation_failed) goto cleanup;

    source_payload_capacity = OIR_SOURCE_UNIT_HEADER_SIZE + path_length +
        source_context->source_len;
    source_payload = (uint8_t*)malloc(source_payload_capacity);
    if (!source_payload) goto allocation_failure;
    oir_status = oir_source_unit_write(source_payload, source_payload_capacity,
        0u, (const uint8_t*)source_path, (uint32_t)path_length,
        (const uint8_t*)source_context->source_buffer,
        (uint64_t)source_context->source_len, &source_payload_length);
    if (oir_status != OIR_STATUS_OK) goto oir_failure;

    if (source_payload_length >
        SIZE_MAX - OIR_HEADER_SIZE - OIR_SECTION_HEADER_SIZE) goto allocation_failure;
    source_artifact_length = OIR_HEADER_SIZE + OIR_SECTION_HEADER_SIZE +
        source_payload_length;
    source_artifact = (uint8_t*)malloc(source_artifact_length);
    if (!source_artifact) goto allocation_failure;

    memset(&header, 0, sizeof(header));
    header.container_major = OIR_CONTAINER_MAJOR;
    header.container_minor = OIR_CONTAINER_MINOR;
    header.artifact_kind = OIR_KIND_SOURCE_UNIT;
    header.payload_schema_major = OIR_SOURCE_UNIT_SCHEMA_MAJOR;
    header.payload_schema_minor = OIR_SOURCE_UNIT_SCHEMA_MINOR;
    header.producer_stage_id = OCC_STAGE_OCC_HOST;
    header.validity = OIR_VALIDITY_VALID;
    header.job_id = job->job_id;
    memset(sections, 0, sizeof(sections));
    sections[0].type = OIR_SECTION_PAYLOAD;
    sections[0].version = 1u;
    sections[0].flags = OIR_SECTION_FLAG_REQUIRED;
    sections[0].data = source_payload;
    sections[0].length = source_payload_length;
    oir_status = oir_artifact_write(source_artifact, source_artifact_length,
        &header, sections, 1u, &source_artifact_length);
    if (oir_status != OIR_STATUS_OK) goto oir_failure;

    if (token_count > (SIZE_MAX - OIR_TOKEN_STREAM_HEADER_SIZE) /
        OIR_TOKEN_STREAM_RECORD_SIZE) goto allocation_failure;
    token_payload_capacity = OIR_TOKEN_STREAM_HEADER_SIZE +
        token_count * OIR_TOKEN_STREAM_RECORD_SIZE;
    token_payload = (uint8_t*)malloc(token_payload_capacity);
    if (!token_payload) goto allocation_failure;

    diagnostics_payload_capacity = OIR_DIAGNOSTICS_HEADER_SIZE;
    if (job->diagnostics.count >
        (SIZE_MAX - diagnostics_payload_capacity) / OIR_DIAGNOSTICS_RECORD_SIZE)
        goto allocation_failure;
    diagnostics_payload_capacity += job->diagnostics.count * OIR_DIAGNOSTICS_RECORD_SIZE;
    if (job->diagnostics.text_length > SIZE_MAX - diagnostics_payload_capacity)
        goto allocation_failure;
    diagnostics_payload_capacity += job->diagnostics.text_length;
    if (job->diagnostics.count > 0u) {
        diagnostics_payload = (uint8_t*)malloc(diagnostics_payload_capacity);
        if (!diagnostics_payload) goto allocation_failure;
        oir_status = oir_diagnostics_payload_write(diagnostics_payload,
            diagnostics_payload_capacity, &job->diagnostics,
            &diagnostics_payload_length);
        if (oir_status != OIR_STATUS_OK) goto oir_failure;
    }

    oir_status = oir_token_stream_write(token_payload, token_payload_capacity,
        0u, (uint64_t)source_context->source_len,
        (uint32_t)job->diagnostics.count, token_inputs, (uint32_t)token_count,
        &token_payload_length);
    if (oir_status != OIR_STATUS_OK) goto oir_failure;

    token_artifact_capacity = OIR_HEADER_SIZE + OIR_SECTION_HEADER_SIZE +
        token_payload_length;
    section_count = 1u;
    if (diagnostics_payload) {
        if (diagnostics_payload_length > SIZE_MAX - token_artifact_capacity -
            OIR_SECTION_HEADER_SIZE) goto allocation_failure;
        token_artifact_capacity += OIR_SECTION_HEADER_SIZE + diagnostics_payload_length;
        section_count = 2u;
    }
    token_artifact = (uint8_t*)malloc(token_artifact_capacity);
    if (!token_artifact) goto allocation_failure;

    header.artifact_kind = OIR_KIND_TOKEN_STREAM;
    header.payload_schema_major = OIR_TOKEN_STREAM_SCHEMA_MAJOR;
    header.payload_schema_minor = OIR_TOKEN_STREAM_SCHEMA_MINOR;
    header.producer_stage_id = OCC_STAGE_LEXER;
    sections[0].data = token_payload;
    sections[0].length = token_payload_length;
    if (diagnostics_payload) {
        sections[1].type = OIR_SECTION_DIAGNOSTICS;
        sections[1].version = 1u;
        sections[1].flags = 0u;
        sections[1].data = diagnostics_payload;
        sections[1].length = diagnostics_payload_length;
    }
    oir_status = oir_artifact_write(token_artifact, token_artifact_capacity,
        &header, sections, section_count, &token_artifact_length);
    if (oir_status != OIR_STATUS_OK) goto oir_failure;

#if defined(_WIN32)
    module_filename = "occ_parser_stage.dll";
#elif defined(__APPLE__)
    module_filename = "libocc_parser_stage.dylib";
#else
    module_filename = "libocc_parser_stage.so";
#endif
    if (!path_join(module_path, sizeof(module_path), executable_dir, module_filename)) {
        occ_add_host_diagnostic(job, OCC_STAGE_OCC_HOST, 6005u,
            "The parser stage module path could not be constructed.");
        goto cleanup;
    }
    if (occ_module_loader_open(&loaded, module_path, loader_error,
        sizeof(loader_error)) != OCC_MODULE_LOADER_OK) {
        occ_add_host_diagnostic(job, OCC_STAGE_PARSER, 6006u,
            loader_error[0] ? loader_error : "The parser stage module could not be loaded.");
        goto cleanup;
    }

    memset(module_inputs, 0, sizeof(module_inputs));
    module_inputs[0].bytes = source_artifact;
    module_inputs[0].length = source_artifact_length;
    module_inputs[1].bytes = token_artifact;
    module_inputs[1].length = token_artifact_length;
    if (occ_module_loader_invoke(&loaded, module_inputs, 2u, job->job_id,
        &job->diagnostics, 4096u, OCC_AST_OUTPUT_MAX,
        &ast_artifact, &ast_artifact_length) != OCC_MODULE_LOADER_OK) {
        if (occ_diagnostics_count(&job->diagnostics) == 0u) {
            occ_add_host_diagnostic(job, OCC_STAGE_PARSER, 6007u,
                "The parser stage module rejected the source or failed to produce AST OIR.");
        }
        goto cleanup;
    }

    if (oir_artifact_open(ast_artifact, ast_artifact_length, &ast_view) != OIR_STATUS_OK ||
        ast_view.header.artifact_kind != OIR_KIND_AST ||
        ast_view.header.job_id != job->job_id) goto oir_failure;
    for (i = 0u; i < ast_view.section_count; ++i) {
        if (oir_artifact_get_section(&ast_view, (uint32_t)i,
            &ast_payload_section) != OIR_STATUS_OK) goto oir_failure;
        if (ast_payload_section.type == OIR_SECTION_PAYLOAD) break;
    }
    if (i == ast_view.section_count ||
        ast_payload_section.length > SIZE_MAX ||
        oir_ast_node_table_open(ast_payload_section.data,
            (size_t)ast_payload_section.length, &ast_table) != OIR_STATUS_OK)
        goto oir_failure;

    node_storage = (ASTNode**)calloc(ast_table.node_count, sizeof(*node_storage));
    incoming_counts = (uint32_t*)calloc(ast_table.node_count, sizeof(*incoming_counts));
    visited = (uint8_t*)calloc(ast_table.node_count, sizeof(*visited));
    graph_stack = (OirAstGraphStackEntry*)calloc(ast_table.node_count, sizeof(*graph_stack));
    if (!node_storage || !incoming_counts || !visited || !graph_stack)
        goto allocation_failure;

    oir_status = oir_ast_artifact_read_frontend_ast(ast_artifact, ast_artifact_length,
        node_storage, ast_table.node_count,
        incoming_counts, ast_table.node_count,
        visited, ast_table.node_count,
        graph_stack, ast_table.node_count, &ast_root);
    if (oir_status != OIR_STATUS_OK || !ast_root) goto oir_failure;

    source_context->ast_root = ast_root;
    ast_root = NULL;
    *out_context = source_context;
    source_context = NULL;
    success = 1;
    goto cleanup;

allocation_failure:
    occ_add_host_diagnostic(job, OCC_STAGE_OCC_HOST, 6008u,
        "Memory allocation failed while preparing the parser stage handoff.");
    goto cleanup;
oir_failure:
    occ_add_host_diagnostic(job, OCC_STAGE_OIR, 6009u,
        "The OCC host could not construct or validate a parser OIR artifact.");
    goto cleanup;
cleanup:
    if (!success && source_context && out_context && !*out_context) {
        *out_context = source_context;
        source_context = NULL;
    }
    if (ast_root) ast_destroy(ast_root);
    free(node_storage);
    free(incoming_counts);
    free(visited);
    free(graph_stack);
    free(ast_artifact);
    occ_module_loader_close(&loaded);
    free(source_payload);
    free(source_artifact);
    free(token_payload);
    free(token_artifact);
    free(diagnostics_payload);
    free(tokens);
    free(token_inputs);
    if (source_context) parser_destroy(source_context);
    return success;
}

static void occ_save_failure_checkpoint(
    OccCompilationContext* job,
    const char* source_path,
    const uint8_t* source_bytes,
    size_t source_length
);

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


static uint64_t occ_make_job_id(void) {
    struct timespec now;
    static uint32_t sequence = 0u;
    uint64_t value;
    if (timespec_get(&now, TIME_UTC) == TIME_UTC && now.tv_sec >= 0) {
        value = (uint64_t)now.tv_sec * UINT64_C(1000000000) +
            (uint64_t)now.tv_nsec;
    } else {
        value = (uint64_t)time(NULL) * UINT64_C(1000000);
    }
    return value + (uint64_t)(++sequence);
}

static void occ_add_host_diagnostic(
    OccCompilationContext* job,
    uint32_t stage_id,
    uint32_t code,
    const char* message
) {
    OccDiagnosticInput input = {0};
    if (!job || !message) return;
    input.code = code;
    input.stage_id = stage_id;
    input.message_id = code;
    input.severity = OCC_DIAG_ERROR;
    input.message = message;
    input.message_length = strlen(message);
    (void)occ_diagnostics_add(&job->diagnostics, &input);
}

static void occ_save_failure_checkpoint(
    OccCompilationContext* job,
    const char* source_path,
    const uint8_t* source_bytes,
    size_t source_length
) {
    OccDiagnosticView last;
    uint32_t stage_id = OCC_STAGE_PARSER;
    char artifact_path[4096];

    if (!job || !source_path || (!source_bytes && source_length != 0u) ||
        occ_diagnostics_count(&job->diagnostics) == 0u) return;

    if (occ_diagnostics_get(&job->diagnostics,
        occ_diagnostics_count(&job->diagnostics) - 1u, &last) &&
        last.record && last.record->stage_id != 0u) {
        stage_id = last.record->stage_id;
    }

    if (occ_failure_store_save(
        "occ-errors", job->job_id, stage_id, source_path,
        source_bytes, source_length,
        &job->diagnostics, artifact_path, sizeof(artifact_path))) {
        fprintf(stderr, "Hata kaydi saklandi: %s\n", artifact_path);
    } else {
        fprintf(stderr, "Hata: OIR hata kaydi yazilamadi (job=%llu).\n",
            (unsigned long long)job->job_id);
    }
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
    const char* target_name = "BareMetal64";
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

    OccCompilationOptions compilation_options = {0};
    OccCompilationContext job;
    uint64_t job_id = occ_make_job_id();
    compilation_options.target_name = target_name;
    if (!occ_compilation_init(&job, job_id, &compilation_options) ||
        !occ_compilation_begin(&job)) {
        fprintf(stderr, "Hata: Derleme isi baslatilamadi.\n");
        occ_compilation_destroy(&job);
        return 1;
    }

    ParserContext* ctx = NULL;
    if (!occ_build_and_run_parser_module(source_file, executable_dir, &job, &ctx) ||
        !ctx || !ctx->ast_root) {
        printf("%s\n", msg_get(MSG_OCC_FAIL_PARSER));
        exit_code = 1;
    }

    if (exit_code == 0 && ctx && ctx->ast_root != NULL) {
        printf("%s\n", msg_get(MSG_OCC_SUCCESS_SEMANTIC));

        SemanticContext sem_ctx;
        semantic_init(&sem_ctx, source_file, lang_path);
        semantic_attach_diagnostics(&sem_ctx, &job.diagnostics, OCC_STAGE_SEMANTIC);
        semantic_check(&sem_ctx, (ASTNode*)ctx->ast_root);

        if (sem_ctx.error_count == 0 &&
            !occ_diagnostics_blocks_output(&job.diagnostics)) {
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
            if (!parser_save_ir(ctx, ir_path)) {
                fprintf(stderr, "Hata: IR dosyasi kaydedilemedi: %s\n", ir_path);
                occ_add_host_diagnostic(&job, OCC_STAGE_OIR, 4001u,
                    "Failed to write the OIR intermediate artifact.");
                exit_code = 1;
            } else {
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
                    occ_add_host_diagnostic(&job, OCC_STAGE_CODEGEN, 5001u,
                        "The code generation module could not be loaded.");
                    exit_code = 1;
                } else if (!run) {
                    fprintf(stderr, "Hata: codegen_run sembolu bulunamadi.\n");
                    occ_add_host_diagnostic(&job, OCC_STAGE_CODEGEN, 5002u,
                        "The codegen_run entry point is missing.");
                    exit_code = 1;
                } else {
                    char asm_path[520];
                    snprintf(asm_path, sizeof(asm_path), "%s.s", base_name);
                    if (run(ir_path, target_path, asm_path) != 0) {
                        fprintf(stderr, "Hata: Codegen islemi basarisiz oldu.\n");
                        occ_add_host_diagnostic(&job, OCC_STAGE_CODEGEN, 5003u,
                            "The code generation stage returned failure.");
                        exit_code = 1;
                    }
                }

    #if defined(_WIN32)
                if (hCodegen) FreeLibrary(hCodegen);
    #else
                if (hCodegen) dlclose(hCodegen);
    #endif
            }
        } else {
            exit_code = 1;
        }

        semantic_destroy(&sem_ctx);
    } else {
        printf("%s\n", msg_get(MSG_OCC_FAIL_PARSER));
        exit_code = 1;
    }

    if (exit_code == 0) {
        if (!occ_compilation_complete(&job) ||
            !occ_compilation_can_publish_output(&job)) {
            exit_code = 1;
        }
    }

    if (exit_code != 0) {
        occ_print_job_diagnostics(&job.diagnostics);
        (void)occ_compilation_fail(&job);
        occ_save_failure_checkpoint(&job, source_file,
            ctx ? (const uint8_t*)ctx->source_buffer : NULL,
            ctx ? ctx->source_len : 0u);
    }

    if (ctx) parser_destroy(ctx);
    occ_compilation_destroy(&job);
    return exit_code;
}
