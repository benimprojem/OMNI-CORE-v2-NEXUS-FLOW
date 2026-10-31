#include "occ_module_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void set_error(char* output, size_t capacity, const char* message) {
    if (!output || capacity == 0u) return;
    (void)snprintf(output, capacity, "%s", message ? message : "unknown error");
    output[capacity - 1u] = '\0';
}

static void* open_library(const char* path, char* error, size_t error_capacity) {
#if defined(_WIN32)
    HMODULE handle = LoadLibraryA(path);
    if (!handle) {
        DWORD code = GetLastError();
        (void)snprintf(error, error_capacity, "LoadLibraryA failed (error=%lu)",
            (unsigned long)code);
        return NULL;
    }
    return (void*)handle;
#else
    void* handle;
    const char* detail;
    (void)dlerror();
    handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        detail = dlerror();
        set_error(error, error_capacity, detail ? detail : "dlopen failed");
    }
    return handle;
#endif
}

static void* find_symbol(void* handle, char* error, size_t error_capacity) {
#if defined(_WIN32)
    FARPROC symbol = GetProcAddress((HMODULE)handle, OCC_MODULE_API_SYMBOL);
    if (!symbol) {
        DWORD code = GetLastError();
        (void)snprintf(error, error_capacity, "GetProcAddress failed (error=%lu)",
            (unsigned long)code);
    }
    return (void*)symbol;
#else
    void* symbol;
    const char* detail;
    (void)dlerror();
    symbol = dlsym(handle, OCC_MODULE_API_SYMBOL);
    detail = dlerror();
    if (detail) {
        set_error(error, error_capacity, detail);
        return NULL;
    }
    return symbol;
#endif
}

static void close_library(void* handle) {
    if (!handle) return;
#if defined(_WIN32)
    (void)FreeLibrary((HMODULE)handle);
#else
    (void)dlclose(handle);
#endif
}

uint32_t occ_module_loader_open(
    OccLoadedStageModule* loaded,
    const char* library_path,
    char* error_text,
    size_t error_text_capacity
) {
    void* handle;
    void* symbol;
    OccGetStageModuleV1Fn get_module = NULL;
    const OccStageModuleV1* module;

    if (error_text && error_text_capacity) error_text[0] = '\0';
    if (!loaded || !library_path || library_path[0] == '\0') {
        set_error(error_text, error_text_capacity, "invalid loader arguments");
        return OCC_MODULE_LOADER_INVALID_ARGUMENT;
    }
    loaded->library_handle = NULL;
    loaded->module = NULL;

    {
        char detail[512] = {0};
        handle = open_library(library_path, detail, sizeof(detail));
        if (!handle) {
            set_error(error_text, error_text_capacity, detail[0] ? detail : "library open failed");
            return OCC_MODULE_LOADER_OPEN_FAILED;
        }
        symbol = find_symbol(handle, detail, sizeof(detail));
        if (!symbol) {
            set_error(error_text, error_text_capacity,
                detail[0] ? detail : "stage entry symbol is missing");
            close_library(handle);
            return OCC_MODULE_LOADER_SYMBOL_MISSING;
        }
    }

    if (sizeof(symbol) != sizeof(get_module)) {
        close_library(handle);
        set_error(error_text, error_text_capacity, "function pointer representation is unsupported");
        return OCC_MODULE_LOADER_INVALID_MODULE;
    }
    memcpy(&get_module, &symbol, sizeof(get_module));
    module = get_module();
    if (occ_module_validate_descriptor_v1(module) != OCC_MODULE_STATUS_OK) {
        close_library(handle);
        set_error(error_text, error_text_capacity, "module descriptor failed ABI validation");
        return OCC_MODULE_LOADER_INVALID_MODULE;
    }

    loaded->library_handle = handle;
    loaded->module = module;
    return OCC_MODULE_LOADER_OK;
}

void occ_module_loader_close(OccLoadedStageModule* loaded) {
    if (!loaded) return;
    close_library(loaded->library_handle);
    loaded->library_handle = NULL;
    loaded->module = NULL;
}

uint32_t occ_module_loader_invoke(
    const OccLoadedStageModule* loaded,
    const OccModuleArtifactInputV1* inputs,
    uint32_t input_count,
    uint64_t job_id,
    OccDiagnosticCollector* diagnostics,
    size_t initial_output_capacity,
    size_t max_output_capacity,
    uint8_t** out_artifact,
    size_t* out_artifact_length
) {
    OccModuleInvocationV1 invocation;
    OccModuleResultV1 result;
    OccModuleDiagnosticSinkV1 sink;
    OccModuleDiagnosticHostContextV1 host_context;
    uint8_t* output = NULL;
    size_t capacity;
    unsigned int attempts = 0u;
    uint32_t status;

    if (out_artifact) *out_artifact = NULL;
    if (out_artifact_length) *out_artifact_length = 0u;
    if (!loaded || !loaded->library_handle || !loaded->module ||
        !out_artifact || !out_artifact_length || !diagnostics ||
        (input_count && !inputs) || initial_output_capacity == 0u ||
        max_output_capacity < initial_output_capacity) {
        return OCC_MODULE_LOADER_INVALID_ARGUMENT;
    }

    capacity = initial_output_capacity;
    output = (uint8_t*)malloc(capacity);
    if (!output) return OCC_MODULE_LOADER_OUT_OF_MEMORY;

    memset(&invocation, 0, sizeof(invocation));
    invocation.struct_size = (uint32_t)sizeof(invocation);
    invocation.input_count = input_count;
    invocation.inputs = inputs;
    invocation.job_id = job_id;

    memset(&sink, 0, sizeof(sink));
    memset(&host_context, 0, sizeof(host_context));
    if ((loaded->module->descriptor.capability_flags &
        OCC_MODULE_CAP_DIAGNOSTIC_REPORTING) != 0u) {
        if (occ_module_diagnostic_sink_init_v1(&sink, &host_context,
            diagnostics, loaded->module->descriptor.stage_id) != OCC_MODULE_STATUS_OK) {
            free(output);
            return OCC_MODULE_LOADER_INVALID_ARGUMENT;
        }
        invocation.diagnostic_sink = sink;
    }

    for (;;) {
        memset(output, 0, capacity);
        invocation.output_buffer = output;
        invocation.output_capacity = (uint64_t)capacity;
        memset(&result, 0, sizeof(result));
        result.struct_size = (uint32_t)sizeof(result);

        status = occ_module_validate_invocation_v1(
            &loaded->module->descriptor, &invocation, &result);
        if (status != OCC_MODULE_STATUS_OK) {
            free(output);
            return OCC_MODULE_LOADER_INVALID_ARGUMENT;
        }

        status = loaded->module->process(&invocation, &result);
        if (status == OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER) {
            uint64_t requested = result.required_output_capacity;
            uint8_t* resized;
            if (result.bytes_written != 0u ||
                result.output_validity != OIR_VALIDITY_NONE ||
                result.failure_code != 0u ||
                requested <= (uint64_t)capacity ||
                requested > (uint64_t)max_output_capacity ||
                requested > (uint64_t)SIZE_MAX) {
                free(output);
                return OCC_MODULE_LOADER_INVALID_OUTPUT;
            }
            if (++attempts > 2u) {
                free(output);
                return OCC_MODULE_LOADER_RETRY_LIMIT;
            }
            resized = (uint8_t*)realloc(output, (size_t)requested);
            if (!resized) {
                free(output);
                return OCC_MODULE_LOADER_OUT_OF_MEMORY;
            }
            output = resized;
            capacity = (size_t)requested;
            continue;
        }

        if (status != OCC_MODULE_STATUS_OK &&
            status != OCC_MODULE_STATUS_PARTIAL) {
            if (result.bytes_written != 0u ||
                result.output_validity != OIR_VALIDITY_NONE) {
                free(output);
                return OCC_MODULE_LOADER_INVALID_OUTPUT;
            }
            free(output);
            return OCC_MODULE_LOADER_INVOKE_FAILED;
        }

        if (result.bytes_written == 0u ||
            result.required_output_capacity != 0u ||
            result.failure_code != 0u ||
            result.bytes_written > (uint64_t)capacity ||
            result.bytes_written > (uint64_t)SIZE_MAX ||
            (status == OCC_MODULE_STATUS_OK &&
             result.output_validity != OIR_VALIDITY_VALID) ||
            (status == OCC_MODULE_STATUS_PARTIAL &&
             result.output_validity != OIR_VALIDITY_PARTIAL)) {
            free(output);
            return OCC_MODULE_LOADER_INVALID_OUTPUT;
        }
        {
            OirArtifactView view;
            if (oir_artifact_open(output, (size_t)result.bytes_written, &view) != OIR_STATUS_OK ||
                view.header.artifact_kind != loaded->module->descriptor.output_artifact_kind ||
                view.header.producer_stage_id != loaded->module->descriptor.stage_id ||
                view.header.job_id != job_id ||
                view.header.validity != result.output_validity) {
                free(output);
                return OCC_MODULE_LOADER_INVALID_OUTPUT;
            }
        }
        *out_artifact = output;
        *out_artifact_length = (size_t)result.bytes_written;
        return OCC_MODULE_LOADER_OK;
    }
}
