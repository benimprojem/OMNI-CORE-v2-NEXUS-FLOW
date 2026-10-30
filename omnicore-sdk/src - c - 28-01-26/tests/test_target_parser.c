#include "target_parser.h"

#include <stdio.h>
#include <string.h>

static const char* valid_minimal_descriptor =
    "[target_info]\n"
    "spec_version = 1.0\n"
    "identifier = \"TestBareMetal64\"\n"
    "os = \"none\"\n"
    "[cpu_arch]\n"
    "family = \"x86_64\"\n"
    "bits = 64\n"
    "endian = \"little\"\n"
    "[instruction_set]\n"
    "simd = \"sse4_2\"\n"
    "[abi_convention]\n"
    "name = \"none\"\n"
    "[output_format]\n"
    "binary_type = \"raw\"\n";

static int write_text(const char* path, const char* text) {
    FILE* file = fopen(path, "wb");
    if (!file) return 0;
    size_t length = strlen(text);
    int ok = fwrite(text, 1, length, file) == length;
    if (fclose(file) != 0) ok = 0;
    return ok;
}

static int expect_invalid_text(const char* path, const char* text, const char* label) {
    TargetConfig config;
    char error[256];

    if (!write_text(path, text)) {
        fprintf(stderr, "Could not write test fixture for %s.\n", label);
        return 0;
    }
    if (target_config_load(path, &config, error, sizeof(error))) {
        fprintf(stderr, "Invalid target was accepted (%s).\n", label);
        return 0;
    }
    return 1;
}

static int expect_valid_target(
    const char* path,
    const char* identifier,
    const char* arch,
    const char* os,
    unsigned int bits,
    TargetABI abi,
    const char* format,
    int backend_supported
) {
    TargetConfig config;
    char error[256];

    if (!target_config_load(path, &config, error, sizeof(error))) {
        fprintf(stderr, "Expected valid target %s: %s\n", path, error);
        return 0;
    }

    if (strcmp(config.identifier, identifier) != 0 ||
        strcmp(config.arch, arch) != 0 ||
        strcmp(config.os, os) != 0 ||
        config.bits != bits ||
        config.abi != abi ||
        strcmp(config.format, format) != 0) {
        fprintf(stderr, "Parsed target fields do not match expectations: %s\n", path);
        return 0;
    }

    int supported = target_backend_check(&config, error, sizeof(error));
    if (supported != backend_supported) {
        fprintf(stderr, "Unexpected backend support result for %s: %s\n", path, error);
        return 0;
    }
    return 1;
}

static int test_invalid_descriptors(const char* path) {
    static const char* unknown_section =
        "[unknown_section]\nkey = \"value\"\n";
    static const char* unknown_key =
        "[target_info]\nunknown_key = \"value\"\n";
    static const char* duplicate_key =
        "[target_info]\n"
        "spec_version = 1.0\n"
        "spec_version = 1.0\n";
    static const char* overlong_identifier =
        "[target_info]\n"
        "spec_version = 1.0\n"
        "identifier = \"TooLongIdentifierThatExceedsTheFixedSixtyFourByteFieldCapacityAndMustFail\"\n";
    static const char* unknown_arch =
        "[target_info]\n"
        "spec_version = 1.0\n"
        "identifier = \"BadArch\"\n"
        "os = \"none\"\n"
        "[cpu_arch]\n"
        "family = \"unknown_arch\"\n";
    static const char* unknown_feature =
        "[target_info]\n"
        "spec_version = 1.0\n"
        "identifier = \"BadFeature\"\n"
        "os = \"none\"\n"
        "[cpu_arch]\n"
        "family = \"x86_64\"\n"
        "bits = 64\n"
        "endian = \"little\"\n"
        "[instruction_set]\n"
        "simd = \"made_up_feature\"\n";
    static const char* missing_required =
        "[target_info]\n"
        "spec_version = 1.0\n"
        "identifier = \"MissingRequired\"\n"
        "os = \"none\"\n";

    if (!expect_invalid_text(path, unknown_section, "unknown section")) return 0;
    if (!expect_invalid_text(path, unknown_key, "unknown key")) return 0;
    if (!expect_invalid_text(path, duplicate_key, "duplicate key")) return 0;
    if (!expect_invalid_text(path, overlong_identifier, "overlong identifier")) return 0;
    if (!expect_invalid_text(path, unknown_arch, "unknown architecture")) return 0;
    if (!expect_invalid_text(path, unknown_feature, "unknown SIMD profile")) return 0;
    if (!expect_invalid_text(path, missing_required, "missing required fields")) return 0;

    if (!write_text(path, valid_minimal_descriptor)) return 0;
    FILE* file = fopen(path, "ab");
    if (!file) return 0;
    char oversized_line[600];
    memset(oversized_line, 'X', sizeof(oversized_line));
    int write_ok = fwrite(oversized_line, 1, sizeof(oversized_line), file) == sizeof(oversized_line);
    if (fclose(file) != 0) write_ok = 0;
    if (!write_ok) return 0;

    TargetConfig config;
    char error[256];
    if (target_config_load(path, &config, error, sizeof(error))) {
        fprintf(stderr, "Overlong target line was accepted.\n");
        return 0;
    }

    return 1;
}

static int test_missing_file(const char* path) {
    TargetConfig config;
    char error[256];
    (void)remove(path);
    if (target_config_load(path, &config, error, sizeof(error))) {
        fprintf(stderr, "Missing target file was accepted.\n");
        return 0;
    }
    return 1;
}

int main(int argc, char** argv) {
    if (argc != 7) {
        fprintf(stderr, "Usage: test_target_parser <baremetal> <linux> <windows> <macos-aarch64> <riscv> <temporary-target>\n");
        return 1;
    }

    if (!expect_valid_target(argv[1], "BareMetal64", "x86_64", "none", 64u, ABI_NONE, "raw", 1)) return 2;
    if (!expect_valid_target(argv[2], "Linux64", "x86_64", "linux", 64u, ABI_SYSV, "elf", 0)) return 3;
    if (!expect_valid_target(argv[3], "Win64", "x86_64", "windows", 64u, ABI_WIN64, "pe", 0)) return 4;
    if (!expect_valid_target(argv[4], "macOS_AArch64", "aarch64", "macos", 64u, ABI_AAPCS64, "macho", 0)) return 5;
    if (!expect_valid_target(argv[5], "RV32IMC", "riscv32", "none", 32u, ABI_ILP32, "bin", 0)) return 6;

    if (!test_invalid_descriptors(argv[6])) return 7;

    char missing_path[1024];
    int length = snprintf(missing_path, sizeof(missing_path), "%s.missing", argv[6]);
    if (length < 0 || (size_t)length >= sizeof(missing_path) ||
        !test_missing_file(missing_path)) return 8;

    return 0;
}
