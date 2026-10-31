#ifdef NDEBUG
#undef NDEBUG
#endif
#include "occ_module_loader.h"
#include "diagnostics.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t make_ast_artifact(uint8_t* output, size_t capacity) {
    static const uint8_t payload[] = {0x4fu,0x49u,0x52u,0x2du,0x41u,0x53u,0x54u};
    OirArtifactHeader header;
    OirSectionInput section;
    size_t written = 0u;
    memset(&header, 0, sizeof(header));
    header.container_major = OIR_CONTAINER_MAJOR;
    header.container_minor = OIR_CONTAINER_MINOR;
    header.artifact_kind = OIR_KIND_AST;
    header.payload_schema_major = 1u;
    header.producer_stage_id = OCC_STAGE_PARSER;
    header.validity = OIR_VALIDITY_VALID;
    header.job_id = 99u;
    memset(&section, 0, sizeof(section));
    section.type = OIR_SECTION_PAYLOAD;
    section.version = 1u;
    section.flags = OIR_SECTION_FLAG_REQUIRED;
    section.data = payload;
    section.length = sizeof(payload);
    assert(oir_artifact_write(output, capacity, &header, &section, 1u, &written) == OIR_STATUS_OK);
    return written;
}

int main(int argc, char** argv) {
    uint8_t input_bytes[128];
    size_t input_length;
    OccModuleArtifactInputV1 input;
    OccLoadedStageModule loaded;
    OccDiagnosticCollector diagnostics;
    uint8_t* output = NULL;
    size_t output_length = 0u;
    char error_text[512];

    if (argc != 3) return 2;
    input_length = make_ast_artifact(input_bytes, sizeof(input_bytes));
    memset(&input, 0, sizeof(input));
    input.bytes = input_bytes;
    input.length = (uint64_t)input_length;
    occ_diagnostics_init(&diagnostics);

    memset(&loaded, 0, sizeof(loaded));
    assert(occ_module_loader_open(&loaded, argv[1], error_text, sizeof(error_text)) ==
        OCC_MODULE_LOADER_OK);
    assert(loaded.module != NULL);
    assert(loaded.module->descriptor.output_artifact_kind == OIR_KIND_AST);

    assert(occ_module_loader_invoke(&loaded, &input, 1u, 99u, &diagnostics,
        8u, 4096u, &output, &output_length) == OCC_MODULE_LOADER_OK);
    assert(output != NULL && output_length == input_length);
    assert(memcmp(output, input_bytes, input_length) == 0);
    assert(occ_diagnostics_count(&diagnostics) == 1u);
    {
        OccDiagnosticView view;
        assert(occ_diagnostics_get(&diagnostics, 0u, &view));
        assert(view.record->stage_id == OCC_STAGE_PARSER);
        assert(strcmp(view.message, "passthrough stage completed") == 0);
    }
    free(output);
    output = NULL;
    occ_module_loader_close(&loaded);
    assert(loaded.library_handle == NULL && loaded.module == NULL);

    assert(occ_module_loader_open(&loaded, argv[2], error_text, sizeof(error_text)) ==
        OCC_MODULE_LOADER_SYMBOL_MISSING);
    assert(loaded.library_handle == NULL && loaded.module == NULL);
    assert(error_text[0] != '\0');

    assert(occ_module_loader_open(&loaded, "missing-stage-module-file", error_text,
        sizeof(error_text)) == OCC_MODULE_LOADER_OPEN_FAILED);
    occ_diagnostics_destroy(&diagnostics);
    puts("OCC module loader open/invoke/validate/unload passed.");
    return 0;
}
