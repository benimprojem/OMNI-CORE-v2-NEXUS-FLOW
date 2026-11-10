#ifdef NDEBUG
#undef NDEBUG
#endif
#include "occ_module_loader.h"
#include "oir_module_interface_artifact.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    OccModuleInterface interface;
    OirModuleInterfaceArtifactView verified_view;
    OccLoadedStageModule loaded;
    OccDiagnosticCollector diagnostics;
    uint8_t input_bytes[1024];
    uint8_t* output_bytes = NULL;
    size_t input_length = 0u, output_length = 0u;
    OirStatus status;

    if (argc != 2) return 2;
    memset(&interface, 0, sizeof(interface));
    memset(&verified_view, 0, sizeof(verified_view));
    memset(&loaded, 0, sizeof(loaded));

    interface.canonical_identity = (char*)"module://verify-fixture";
    interface.schema_major = OCC_MODULE_INTERFACE_SCHEMA_MAJOR;
    interface.schema_minor = OCC_MODULE_INTERFACE_SCHEMA_MINOR;
    status = oir_module_interface_artifact_write(&interface, 9001u,
        input_bytes, sizeof(input_bytes), &input_length);
    assert(status == OIR_STATUS_OK);

    occ_diagnostics_init(&diagnostics);
    assert(occ_module_loader_open(&loaded, argv[1], NULL, 0u) ==
        OCC_MODULE_LOADER_OK);
    assert(loaded.module->descriptor.output_artifact_kind ==
        OIR_KIND_MODULE_INTERFACE);

    {
        OccModuleArtifactInputV1 input;
        memset(&input, 0, sizeof(input));
        input.bytes = input_bytes;
        input.length = input_length;
        assert(occ_module_loader_invoke(&loaded, &input, 1u, 9001u,
            &diagnostics, 32u, 4096u, &output_bytes, &output_length) ==
            OCC_MODULE_LOADER_OK);
    }
    assert(output_bytes != NULL && output_length == input_length);
    assert(oir_module_interface_artifact_open(output_bytes, output_length,
        &verified_view) == OIR_STATUS_OK);
    assert(verified_view.envelope.header.producer_stage_id == OCC_STAGE_OIR_VERIFY);
    assert(verified_view.envelope.header.job_id == 9001u);
    assert(occ_diagnostics_count(&diagnostics) == 0u);

    oir_module_interface_artifact_destroy(&verified_view);
    free(output_bytes);
    output_bytes = NULL;

    /* A structurally corrupted payload must be rejected with a verifier diagnostic. */
    input_bytes[OIR_HEADER_SIZE + OIR_SECTION_HEADER_SIZE] ^= 0x01u;
    {
        OccModuleArtifactInputV1 input;
        OccDiagnosticView diagnostic;
        memset(&input, 0, sizeof(input));
        input.bytes = input_bytes;
        input.length = input_length;
        assert(occ_module_loader_invoke(&loaded, &input, 1u, 9001u,
            &diagnostics, 32u, 4096u, &output_bytes, &output_length) ==
            OCC_MODULE_LOADER_INVOKE_FAILED);
        assert(output_bytes == NULL);
        assert(occ_diagnostics_count(&diagnostics) == 1u);
        assert(occ_diagnostics_get(&diagnostics, 0u, &diagnostic));
        assert(diagnostic.record->stage_id == OCC_STAGE_OIR_VERIFY);
        assert(diagnostic.record->code == 8701u);
        assert(diagnostic.record->severity == OCC_DIAG_ERROR);
    }

    occ_module_loader_close(&loaded);
    occ_diagnostics_destroy(&diagnostics);
    return 0;
}
