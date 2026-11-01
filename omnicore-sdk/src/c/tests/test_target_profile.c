#include "target_profile_parser.h"

#include <stdio.h>
#include <string.h>

static const char* valid_descriptor =
    "[target_info]\n"
    "schema_major = 2\n"
    "schema_minor = 0\n"
    "profile_id = \"Test64\"\n"
    "profile_kind = \"family\"\n"
    "os = \"test_os\"\n"
    "runtime = \"test_runtime\"\n"
    "package_format = \"test_pkg\"\n"
    "abi = \"test_abi\"\n"
    "binary_format = \"test_bin\"\n"
    "entry_point = \"main\"\n"
    "stack_alignment_bytes = 16\n"
    "\n"
    "[cpu]\n"
    "id = \"cpu0\"\n"
    "kind = \"cpu\"\n"
    "architecture = \"x86_64\"\n"
    "isa_profile = \"test64\"\n"
    "word_size_bits = 64\n"
    "address_size_bits = 64\n"
    "endian = \"little\"\n"
    "general_register_count = 16\n"
    "atomic_widths = 8,16,32,64\n"
    "fp_formats = fp32,fp64\n"
    "features = atomics,hardware_float\n"
    "isa_extensions = sse,sse2\n";

static int write_text(const char* path, const char* text) {
    FILE* file = fopen(path, "w");
    int ok;
    if (!file) return 0;
    ok = fputs(text, file) >= 0;
    if (fclose(file) != 0) ok = 0;
    return ok;
}

static int expect_rejected(const char* path, const char* text) {
    OccTargetProfile profile;
    char error[256];
    if (!write_text(path, text)) {
        fprintf(stderr, "Could not write test descriptor: %s\n", path);
        return 0;
    }
    if (occ_target_profile_load(path, &profile, error, sizeof(error))) {
        fprintf(stderr, "Malformed descriptor was accepted: %s\n", error);
        return 0;
    }
    return 1;
}

int main(int argc, char** argv) {
    OccTargetProfile profile;
    char error[256];
    char duplicate_list[2048];
    char empty_list[2048];
    char duplicate_section[2048];
    char exact_machine[2048];
    char unsupported_minor[2048];
    char numeric_overflow[2048];
    char malformed_string[2048];
    char unsupported_cpu_kind[2048];

    if (argc != 3) {
        fprintf(stderr, "Usage: test_target_profile <known-good-profile> <temp-file>\n");
        return 2;
    }

    if (!occ_target_profile_load(argv[1], &profile, error, sizeof(error))) {
        fprintf(stderr, "Known-good profile failed: %s\n", error);
        return 1;
    }
    if (profile.schema_major != OCC_TARGET_PROFILE_SCHEMA_MAJOR ||
        profile.device_count != 1u ||
        profile.devices[0].kind != OCC_TARGET_DEVICE_CPU) {
        fprintf(stderr, "Known-good profile produced unexpected normalized data.\n");
        return 1;
    }

    if (!write_text(argv[2], valid_descriptor) ||
        !occ_target_profile_load(argv[2], &profile, error, sizeof(error))) {
        fprintf(stderr, "Explicit schema_minor=0 descriptor failed: %s\n", error);
        return 1;
    }

    (void)snprintf(duplicate_list, sizeof(duplicate_list), "%s",
        valid_descriptor);
    {
        char* p = strstr(duplicate_list, "isa_extensions = sse,sse2");
        if (!p) return 1;
        (void)strcpy(p, "isa_extensions = sse,sse\n");
    }
    if (!expect_rejected(argv[2], duplicate_list)) return 1;

    (void)snprintf(empty_list, sizeof(empty_list), "%s", valid_descriptor);
    {
        char* p = strstr(empty_list, "isa_extensions = sse,sse2");
        if (!p) return 1;
        (void)strcpy(p, "isa_extensions = sse,,sse2\n");
    }
    if (!expect_rejected(argv[2], empty_list)) return 1;

    (void)snprintf(duplicate_section, sizeof(duplicate_section), "%s[cpu]\n",
        valid_descriptor);
    if (!expect_rejected(argv[2], duplicate_section)) return 1;

    (void)snprintf(exact_machine, sizeof(exact_machine), "%s",
        valid_descriptor);
    {
        char* p = strstr(exact_machine, "profile_kind = \"family\"");
        if (!p) return 1;
        (void)strcpy(p, "profile_kind = \"exact_machine\"\n");
    }
    if (!expect_rejected(argv[2], exact_machine)) return 1;

    (void)snprintf(unsupported_minor, sizeof(unsupported_minor), "%s",
        valid_descriptor);
    {
        char* p = strstr(unsupported_minor, "schema_minor = 0");
        if (!p) return 1;
        (void)strcpy(p, "schema_minor = 1\n");
    }
    if (!expect_rejected(argv[2], unsupported_minor)) return 1;

    (void)snprintf(numeric_overflow, sizeof(numeric_overflow), "%s",
        valid_descriptor);
    {
        char* p = strstr(numeric_overflow, "stack_alignment_bytes = 16");
        if (!p) return 1;
        (void)strcpy(p, "stack_alignment_bytes = 4294967296\\n");
    }
    if (!expect_rejected(argv[2], numeric_overflow)) return 1;

    (void)snprintf(malformed_string, sizeof(malformed_string), "%s",
        valid_descriptor);
    {
        char* p = strstr(malformed_string, "profile_id = \\"Test64\\"");
        if (!p) return 1;
        (void)strcpy(p, "profile_id = \\"Test64\\" trailing\\n");
    }
    if (!expect_rejected(argv[2], malformed_string)) return 1;

    (void)snprintf(unsupported_cpu_kind, sizeof(unsupported_cpu_kind), "%s",
        valid_descriptor);
    {
        char* p = strstr(unsupported_cpu_kind, "kind = \\"cpu\\"");
        if (!p) return 1;
        (void)strcpy(p, "kind = \\"gpu\\"\\n");
    }
    if (!expect_rejected(argv[2], unsupported_cpu_kind)) return 1;

    (void)remove(argv[2]);
    puts("Target profile parser checks passed.");
    return 0;
}
