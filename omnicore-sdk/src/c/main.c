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
#include "module/occ_module_loader.h"
#include "oir/oir_payload.h"
#include "oir/oir_ast_artifact.h"
#include "oir/oir_ast_node_table.h"
#include "oir/oir_ast_graph.h"
#include "oir/oir_module.h"
#include "target_parser.h"
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

#define OCC_TOKEN_OUTPUT_MAX (64u * 1024u * 1024u)
#define OCC_AST_OUTPUT_MAX (256u * 1024u * 1024u)
#define OCC_SEMANTIC_OUTPUT_MAX (256u * 1024u * 1024u)
#define OCC_MODULE_OUTPUT_MAX (256u * 1024u * 1024u)
#define OCC_ASSEMBLY_OUTPUT_MAX (64u * 1024u * 1024u)

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

static int occ_validate_lowering_artifact(
    const uint8_t* bytes,
    size_t length,
    uint64_t job_id
) {
    OirArtifactView artifact;
    OirSectionView payload;
    OirModuleView module;
    uint32_t i;
    int found_payload = 0;

    if (!bytes || length == 0u ||
        oir_artifact_open(bytes, length, &artifact) != OIR_STATUS_OK ||
        artifact.header.artifact_kind != OIR_KIND_MODULE ||
        artifact.header.payload_schema_major != OIR_MODULE_SCHEMA_MAJOR ||
        artifact.header.payload_schema_minor != OIR_MODULE_SCHEMA_MINOR ||
        artifact.header.producer_stage_id != OCC_STAGE_OIR_LOWERING ||
        artifact.header.validity != OIR_VALIDITY_VALID ||
        artifact.header.job_id != job_id) {
        return 0;
    }

    memset(&payload, 0, sizeof(payload));
    for (i = 0u; i < artifact.section_count; ++i) {
        OirSectionView section;
        if (oir_artifact_get_section(&artifact, i, &section) != OIR_STATUS_OK)
            return 0;
        if (section.type == OIR_SECTION_PAYLOAD) {
            if (found_payload || section.version != 1u ||
                section.length > (uint64_t)SIZE_MAX) {
                return 0;
            }
            payload = section;
            found_payload = 1;
        }
    }

    if (!found_payload || !payload.data || payload.length == 0u)
        return 0;
    return oir_module_open(payload.data, (size_t)payload.length, &module) ==
        OIR_STATUS_OK;
}

/* Persist a fully validated stage artifact through a same-directory temporary
 * file. The final path is never exposed with partially written contents. */
static int occ_write_artifact_file(
    const char* path,
    const uint8_t* bytes,
    size_t length,
    uint64_t job_id
) {
    char temporary_path[OCC_PATH_BUFFER + 64u];
    FILE* file;
    int written;
    int failed = 0;

    if (!path || !bytes || length == 0u) return 0;
    written = snprintf(temporary_path, sizeof(temporary_path),
        "%s.tmp.%llu", path, (unsigned long long)job_id);
    if (written < 0 || (size_t)written >= sizeof(temporary_path)) return 0;

    file = fopen(temporary_path, "wb");
    if (!file) return 0;
    if (fwrite(bytes, 1u, length, file) != length) failed = 1;
    if (!failed && fflush(file) != 0) failed = 1;
    if (fclose(file) != 0) failed = 1;
    if (failed) {
        (void)remove(temporary_path);
        return 0;
    }

#if defined(_WIN32)
    if (!MoveFileExA(temporary_path, path,
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        (void)remove(temporary_path);
        return 0;
    }
#else
    if (rename(temporary_path, path) != 0) {
        (void)remove(temporary_path);
        return 0;
    }
#endif
    return 1;
}

static int occ_validate_assembly_artifact(
    const uint8_t* bytes,
    size_t length,
    uint64_t job_id,
    OirSectionView* out_payload
) {
    OirArtifactView artifact;
    OirSectionView payload;
    uint32_t i;
    int found_payload = 0;
    if (!bytes || length == 0u || !out_payload ||
        oir_artifact_open(bytes, length, &artifact) != OIR_STATUS_OK ||
        artifact.header.artifact_kind != OIR_KIND_ASSEMBLY ||
        artifact.header.payload_schema_major != 1u ||
        artifact.header.payload_schema_minor != 0u ||
        artifact.header.producer_stage_id != OCC_STAGE_CODEGEN ||
        artifact.header.validity != OIR_VALIDITY_VALID ||
        artifact.header.job_id != job_id) return 0;

    memset(&payload, 0, sizeof(payload));
    for (i = 0u; i < artifact.section_count; ++i) {
        OirSectionView section;
        uint64_t j;
        if (oir_artifact_get_section(&artifact, i, &section) != OIR_STATUS_OK)
            return 0;
        if (section.type != OIR_SECTION_PAYLOAD) continue;
        if (found_payload || section.version != 1u ||
            section.length == 0u || section.length > (uint64_t)SIZE_MAX ||
            !section.data) return 0;
        for (j = 0u; j < section.length; ++j) {
            uint8_t ch = section.data[j];
            if (ch == 0u || (ch < 0x20u && ch != '\n' && ch != '\r' && ch != '\t') ||
                ch > 0x7eu) return 0;
        }
        payload = section;
        found_payload = 1;
    }
    if (!found_payload) return 0;
    *out_payload = payload;
    return 1;
}

static int occ_run_codegen_stage(
    const char* executable_dir,
    const char* target_path,
    const char* base_name,
    const uint8_t* module_artifact,
    size_t module_artifact_length,
    OccCompilationContext* job
) {
    OccLoadedStageModule loaded;
    OccModuleArtifactInputV1 input;
    OirSectionView assembly_payload;
    TargetConfig target_config;
    uint8_t* assembly_artifact = NULL;
    size_t assembly_artifact_length = 0u;
    char module_path[OCC_PATH_BUFFER], module_error[512];
    char target_error[512];
    char artifact_path[640], assembly_path[640];
    const char* filename;
    int artifact_path_length, assembly_path_length;
    int ok = 0;

    memset(&loaded, 0, sizeof(loaded));
    if (!executable_dir || !target_path || !base_name || !module_artifact ||
        module_artifact_length == 0u || !job) return 0;

    if (!target_config_load(target_path, &target_config,
        target_error, sizeof(target_error)) ||
        !target_backend_check(&target_config, target_error, sizeof(target_error))) {
        occ_add_host_diagnostic(job, OCC_STAGE_CODEGEN, 5020u,
            target_error[0] ? target_error : "The selected target is not supported by the current backend.");
        return 0;
    }

#if defined(_WIN32)
    filename = "occ_codegen_stage.dll";
#elif defined(__APPLE__)
    filename = "libocc_codegen_stage.dylib";
#else
    filename = "libocc_codegen_stage.so";
#endif
    if (!path_join(module_path, sizeof(module_path), executable_dir, filename)) {
        occ_add_host_diagnostic(job, OCC_STAGE_CODEGEN, 5021u,
            "The Codegen stage module path could not be constructed.");
        return 0;
    }
    if (occ_module_loader_open(&loaded, module_path,
        module_error, sizeof(module_error)) != OCC_MODULE_LOADER_OK) {
        occ_add_host_diagnostic(job, OCC_STAGE_CODEGEN, 5022u,
            module_error[0] ? module_error : "The Codegen stage module could not be loaded.");
        goto cleanup;
    }
    if (!loaded.module ||
        loaded.module->descriptor.stage_id != OCC_STAGE_CODEGEN ||
        loaded.module->descriptor.output_artifact_kind != OIR_KIND_ASSEMBLY) {
        occ_add_host_diagnostic(job, OCC_STAGE_CODEGEN, 5023u,
            "The loaded Codegen module does not match the OIR_ASSEMBLY 1.0 stage contract.");
        goto cleanup;
    }

    memset(&input, 0, sizeof(input));
    input.bytes = module_artifact;
    input.length = (uint64_t)module_artifact_length;
    if (occ_module_loader_invoke(&loaded, &input, 1u, job->job_id,
        &job->diagnostics, 4096u, OCC_ASSEMBLY_OUTPUT_MAX,
        &assembly_artifact, &assembly_artifact_length) != OCC_MODULE_LOADER_OK) {
        if (occ_diagnostics_count(&job->diagnostics) == 0u) {
            occ_add_host_diagnostic(job, OCC_STAGE_CODEGEN, 5024u,
                "The Codegen stage failed to produce OIR_ASSEMBLY.");
        }
        goto cleanup;
    }
    if (!occ_validate_assembly_artifact(assembly_artifact,
        assembly_artifact_length, job->job_id, &assembly_payload)) {
        occ_add_host_diagnostic(job, OCC_STAGE_CODEGEN, 5025u,
            "The Codegen output failed OIR_ASSEMBLY envelope or text validation.");
        goto cleanup;
    }

    artifact_path_length = snprintf(artifact_path, sizeof(artifact_path),
        "%s.assembly.oir", base_name);
    assembly_path_length = snprintf(assembly_path, sizeof(assembly_path),
        "%s.stage.s", base_name);
    if (artifact_path_length < 0 ||
        (size_t)artifact_path_length >= sizeof(artifact_path) ||
        assembly_path_length < 0 ||
        (size_t)assembly_path_length >= sizeof(assembly_path)) {
        occ_add_host_diagnostic(job, OCC_STAGE_CODEGEN, 5026u,
            "The Codegen output path exceeds the supported path buffer.");
        goto cleanup;
    }
    if (!occ_write_artifact_file(artifact_path, assembly_artifact,
            assembly_artifact_length, job->job_id) ||
        !occ_write_artifact_file(assembly_path, assembly_payload.data,
            (size_t)assembly_payload.length, job->job_id)) {
        occ_add_host_diagnostic(job, OCC_STAGE_CODEGEN, 5027u,
            "The validated OIR_ASSEMBLY artifact or assembly text could not be persisted.");
        goto cleanup;
    }

    printf("OIR_ASSEMBLY kaydedildi: %s\n", artifact_path);
    printf("Codegen stage assembly kaydedildi: %s\n", assembly_path);
    ok = 1;

cleanup:
    free(assembly_artifact);
    occ_module_loader_close(&loaded);
    return ok;
}

static int occ_build_and_run_parser_module(
    const char* source_path,
    const char* executable_dir,
    OccCompilationContext* job,
    ParserContext** out_context,
    uint8_t** out_module_artifact,
    size_t* out_module_artifact_length
) {
    ParserContext* source_context = NULL;
    uint8_t *source_payload = NULL, *source_artifact = NULL;
    uint8_t *token_artifact = NULL, *ast_artifact = NULL, *semantic_artifact = NULL;
    uint8_t *lowered_artifact = NULL;
    size_t source_payload_length = 0u, source_artifact_length = 0u;
    size_t token_artifact_length = 0u, ast_artifact_length = 0u, semantic_artifact_length = 0u;
    size_t lowered_artifact_length = 0u;
    size_t source_payload_capacity = 0u, path_length;
    OirArtifactHeader header;
    OirSectionInput section;
    OccModuleArtifactInputV1 module_inputs[2];
    OccLoadedStageModule lexer_loaded, parser_loaded, semantic_loaded, lowering_loaded;
    char lexer_module_path[OCC_PATH_BUFFER], parser_module_path[OCC_PATH_BUFFER];
    char semantic_module_path[OCC_PATH_BUFFER], lowering_module_path[OCC_PATH_BUFFER];
    char lexer_error[512], parser_error[512], semantic_error[512], lowering_error[512];
    const char *lexer_filename, *parser_filename, *semantic_filename, *lowering_filename;
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
    memset(&lexer_loaded, 0, sizeof(lexer_loaded));
    memset(&parser_loaded, 0, sizeof(parser_loaded));
    memset(&semantic_loaded, 0, sizeof(semantic_loaded));
    memset(&lowering_loaded, 0, sizeof(lowering_loaded));
    if (out_module_artifact) *out_module_artifact = NULL;
    if (out_module_artifact_length) *out_module_artifact_length = 0u;
    if (!source_path || !executable_dir || !job || !out_context ||
        !out_module_artifact || !out_module_artifact_length) return 0;

    source_context = parser_init(source_path);
    if (!source_context) {
        occ_add_host_diagnostic(job, OCC_STAGE_OCC_HOST, 6001u,
            "The source file could not be opened by the OCC host.");
        goto cleanup;
    }
    /* The host owns this context only for source bytes and later AST attachment. */
    source_context->ast_root = NULL;

    path_length = strlen(source_path);
    if (path_length > UINT32_MAX ||
        path_length > SIZE_MAX - OIR_SOURCE_UNIT_HEADER_SIZE ||
        source_context->source_len >
            SIZE_MAX - OIR_SOURCE_UNIT_HEADER_SIZE - path_length) {
        occ_add_host_diagnostic(job, OCC_STAGE_OCC_HOST, 6002u,
            "The source unit exceeds the supported OIR size.");
        goto cleanup;
    }

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
    memset(&section, 0, sizeof(section));
    section.type = OIR_SECTION_PAYLOAD;
    section.version = 1u;
    section.flags = OIR_SECTION_FLAG_REQUIRED;
    section.data = source_payload;
    section.length = source_payload_length;
    oir_status = oir_artifact_write(source_artifact, source_artifact_length,
        &header, &section, 1u, &source_artifact_length);
    if (oir_status != OIR_STATUS_OK) goto oir_failure;

#if defined(_WIN32)
    lexer_filename = "occ_lexer_stage.dll";
    parser_filename = "occ_parser_stage.dll";
    semantic_filename = "occ_semantic_stage.dll";
    lowering_filename = "occ_oir_lowering_stage.dll";
#elif defined(__APPLE__)
    lexer_filename = "libocc_lexer_stage.dylib";
    parser_filename = "libocc_parser_stage.dylib";
    semantic_filename = "libocc_semantic_stage.dylib";
    lowering_filename = "libocc_oir_lowering_stage.dylib";
#else
    lexer_filename = "libocc_lexer_stage.so";
    parser_filename = "libocc_parser_stage.so";
    semantic_filename = "libocc_semantic_stage.so";
    lowering_filename = "libocc_oir_lowering_stage.so";
#endif
    if (!path_join(lexer_module_path, sizeof(lexer_module_path),
        executable_dir, lexer_filename) ||
        !path_join(parser_module_path, sizeof(parser_module_path),
        executable_dir, parser_filename) ||
        !path_join(semantic_module_path, sizeof(semantic_module_path),
        executable_dir, semantic_filename) ||
        !path_join(lowering_module_path, sizeof(lowering_module_path),
        executable_dir, lowering_filename)) {
        occ_add_host_diagnostic(job, OCC_STAGE_OCC_HOST, 6005u,
            "A compiler stage module path could not be constructed.");
        goto cleanup;
    }

    if (occ_module_loader_open(&lexer_loaded, lexer_module_path,
        lexer_error, sizeof(lexer_error)) != OCC_MODULE_LOADER_OK) {
        occ_add_host_diagnostic(job, OCC_STAGE_LEXER, 6006u,
            lexer_error[0] ? lexer_error : "The lexer stage module could not be loaded.");
        goto cleanup;
    }

    memset(module_inputs, 0, sizeof(module_inputs));
    module_inputs[0].bytes = source_artifact;
    module_inputs[0].length = source_artifact_length;
    if (occ_module_loader_invoke(&lexer_loaded, module_inputs, 1u, job->job_id,
        &job->diagnostics, 256u, OCC_TOKEN_OUTPUT_MAX,
        &token_artifact, &token_artifact_length) != OCC_MODULE_LOADER_OK) {
        if (occ_diagnostics_count(&job->diagnostics) == 0u) {
            occ_add_host_diagnostic(job, OCC_STAGE_LEXER, 6007u,
                "The lexer stage module failed to produce TOKEN_STREAM OIR.");
        }
        goto cleanup;
    }
    occ_module_loader_close(&lexer_loaded);

    if (occ_module_loader_open(&parser_loaded, parser_module_path,
        parser_error, sizeof(parser_error)) != OCC_MODULE_LOADER_OK) {
        occ_add_host_diagnostic(job, OCC_STAGE_PARSER, 6008u,
            parser_error[0] ? parser_error : "The parser stage module could not be loaded.");
        goto cleanup;
    }

    module_inputs[1].bytes = token_artifact;
    module_inputs[1].length = token_artifact_length;
    if (occ_module_loader_invoke(&parser_loaded, module_inputs, 2u, job->job_id,
        &job->diagnostics, 4096u, OCC_AST_OUTPUT_MAX,
        &ast_artifact, &ast_artifact_length) != OCC_MODULE_LOADER_OK) {
        if (occ_diagnostics_count(&job->diagnostics) == 0u) {
            occ_add_host_diagnostic(job, OCC_STAGE_PARSER, 6009u,
                "The parser stage module rejected the source or failed to produce AST OIR.");
        }
        goto cleanup;
    }

    /* Semantic analysis is a separate DLL stage: AST OIR -> SEMANTIC_AST OIR. */
    if (occ_module_loader_open(&semantic_loaded, semantic_module_path,
        semantic_error, sizeof(semantic_error)) != OCC_MODULE_LOADER_OK) {
        occ_add_host_diagnostic(job, OCC_STAGE_SEMANTIC, 6012u,
            semantic_error[0] ? semantic_error : "The semantic stage module could not be loaded.");
        goto cleanup;
    }
    memset(module_inputs, 0, sizeof(module_inputs));
    module_inputs[0].bytes = ast_artifact;
    module_inputs[0].length = ast_artifact_length;
    if (occ_module_loader_invoke(&semantic_loaded, module_inputs, 1u, job->job_id,
        &job->diagnostics, 4096u, OCC_SEMANTIC_OUTPUT_MAX,
        &semantic_artifact, &semantic_artifact_length) != OCC_MODULE_LOADER_OK) {
        if (occ_diagnostics_count(&job->diagnostics) == 0u) {
            occ_add_host_diagnostic(job, OCC_STAGE_SEMANTIC, 6013u,
                "The semantic stage module failed to produce SEMANTIC_AST OIR.");
        }
        goto cleanup;
    }
    occ_module_loader_close(&semantic_loaded);

    /* Lowering is now a first-class DLL stage: SEMANTIC_AST -> OIR_MODULE. */
    if (occ_module_loader_open(&lowering_loaded, lowering_module_path,
        lowering_error, sizeof(lowering_error)) != OCC_MODULE_LOADER_OK) {
        occ_add_host_diagnostic(job, OCC_STAGE_OIR_LOWERING, 6014u,
            lowering_error[0] ? lowering_error : "The OIR lowering stage module could not be loaded.");
        goto cleanup;
    }
    if (!lowering_loaded.module ||
        lowering_loaded.module->descriptor.stage_id != OCC_STAGE_OIR_LOWERING ||
        lowering_loaded.module->descriptor.output_artifact_kind != OIR_KIND_MODULE) {
        occ_add_host_diagnostic(job, OCC_STAGE_OIR_LOWERING, 6015u,
            "The loaded lowering module does not match the required stage contract.");
        goto cleanup;
    }

    memset(module_inputs, 0, sizeof(module_inputs));
    module_inputs[0].bytes = semantic_artifact;
    module_inputs[0].length = semantic_artifact_length;
    if (occ_module_loader_invoke(&lowering_loaded, module_inputs, 1u,
        job->job_id, &job->diagnostics, 4096u, OCC_MODULE_OUTPUT_MAX,
        &lowered_artifact, &lowered_artifact_length) != OCC_MODULE_LOADER_OK) {
        if (occ_diagnostics_count(&job->diagnostics) == 0u) {
            occ_add_host_diagnostic(job, OCC_STAGE_OIR_LOWERING, 6016u,
                "The OIR lowering stage failed to produce OIR_MODULE.");
        }
        goto cleanup;
    }

    if (!occ_validate_lowering_artifact(
        lowered_artifact, lowered_artifact_length, job->job_id)) {
        occ_add_host_diagnostic(job, OCC_STAGE_OIR_LOWERING, 6017u,
            "The OIR lowering output failed host-side artifact or payload validation.");
        goto cleanup;
    }
    occ_module_loader_close(&lowering_loaded);

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
    *out_module_artifact = lowered_artifact;
    *out_module_artifact_length = lowered_artifact_length;
    lowered_artifact = NULL;
    lowered_artifact_length = 0u;
    success = 1;
    goto cleanup;

allocation_failure:
    occ_add_host_diagnostic(job, OCC_STAGE_OCC_HOST, 6010u,
        "Memory allocation failed while preparing the frontend stage handoff.");
    goto cleanup;
oir_failure:
    occ_add_host_diagnostic(job, OCC_STAGE_OIR, 6011u,
        "The OCC host could not construct or validate a frontend OIR artifact.");
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
    free(lowered_artifact);
    free(semantic_artifact);
    free(ast_artifact);
    free(token_artifact);
    occ_module_loader_close(&lowering_loaded);
    occ_module_loader_close(&semantic_loaded);
    occ_module_loader_close(&parser_loaded);
    occ_module_loader_close(&lexer_loaded);
    free(source_payload);
    free(source_artifact);
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

/* Resolve the default target from the compiler host, never from a hard-coded
 * cross-target fallback. Unsupported host combinations require an explicit
 * --target selection. */
static const char* occ_host_default_target(void) {
#if defined(_WIN32)
#  if defined(_M_X64) || defined(__x86_64__)
    return "Win64";
#  else
    return NULL;
#  endif
#elif defined(__APPLE__)
#  if defined(__aarch64__) || defined(__arm64__)
    return "macOS_AArch64";
#  else
    return NULL;
#  endif
#elif defined(__linux__)
#  if defined(__x86_64__)
    return "Linux64";
#  else
    return NULL;
#  endif
#else
    return NULL;
#endif
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
    const char* target_name = NULL;
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

    if (!target_name) {
        target_name = occ_host_default_target();
        if (!target_name) {
            fprintf(stderr,
                "Hata: Bu host OS/mimari icin varsayilan target tanimli degil. "
                "Lutfen --target ile bir hedef belirtin.\n");
            list_targets(target_dir);
            return 1;
        }
        printf("[OCC] Varsayilan host target secildi: %s\n", target_name);
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
    uint8_t* module_artifact = NULL;
    size_t module_artifact_length = 0u;
    if (!occ_build_and_run_parser_module(source_file, executable_dir, &job, &ctx,
            &module_artifact, &module_artifact_length) ||
        !ctx || !ctx->ast_root || !module_artifact || module_artifact_length == 0u) {
        printf("%s\n", msg_get(MSG_OCC_FAIL_PARSER));
        exit_code = 1;
    }

    if (exit_code == 0 && ctx && ctx->ast_root != NULL) {
        printf("%s\n", msg_get(MSG_OCC_SUCCESS_SEMANTIC));

        /* Semantic validation has already completed in occ_semantic_stage. */
        if (!occ_diagnostics_blocks_output(&job.diagnostics)) {
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
            char module_path[640];
            int module_path_length;
            snprintf(ir_path, sizeof(ir_path), "%s.oir", base_name);
            module_path_length = snprintf(module_path, sizeof(module_path),
                "%s.module.oir", base_name);
            if (module_path_length < 0 ||
                (size_t)module_path_length >= sizeof(module_path)) {
                fprintf(stderr, "Hata: OIR_MODULE cikti yolu olusturulamadi.\n");
                occ_add_host_diagnostic(&job, OCC_STAGE_OIR_LOWERING, 6018u,
                    "The OIR_MODULE output path exceeds the supported path buffer.");
                exit_code = 1;
            } else if (!parser_save_ir(ctx, ir_path)) {
                fprintf(stderr, "Hata: IR dosyasi kaydedilemedi: %s\n", ir_path);
                occ_add_host_diagnostic(&job, OCC_STAGE_OIR, 4001u,
                    "Failed to write the OIR intermediate artifact.");
                exit_code = 1;
            } else if (!occ_write_artifact_file(module_path, module_artifact,
                module_artifact_length, job.job_id)) {
                fprintf(stderr, "Hata: OIR_MODULE artifact kaydedilemedi: %s\n", module_path);
                occ_add_host_diagnostic(&job, OCC_STAGE_OIR_LOWERING, 6019u,
                    "The validated OIR_MODULE artifact could not be persisted.");
                exit_code = 1;
            } else if (!occ_run_codegen_stage(executable_dir, target_path,
                base_name, module_artifact, module_artifact_length, &job)) {
                exit_code = 1;
            } else {
                printf("IR kaydedildi: %s\n", ir_path);
                printf("OIR_MODULE kaydedildi: %s\n", module_path);

                /* The legacy Codegen bridge remains until OIR_MODULE parity is proven. */
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

    free(module_artifact);
    if (ctx) parser_destroy(ctx);
    occ_compilation_destroy(&job);
    return exit_code;
}
