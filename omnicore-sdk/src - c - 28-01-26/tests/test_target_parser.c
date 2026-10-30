#include "target_parser.h"

#include <stdio.h>
#include <string.h>

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

static int expect_invalid_target(const char* path) {
    TargetConfig config;
    char error[256];
    if (target_config_load(path, &config, error, sizeof(error))) {
        fprintf(stderr, "Invalid target was accepted: %s\n", path);
        return 0;
    }
    return 1;
}

int main(int argc, char** argv) {
    if (argc != 15) {
        fprintf(stderr, "Expected 14 target descriptor paths, received %d.\n", argc - 1);
        return 1;
    }

    if (!expect_valid_target(argv[1], "BareMetal64", "x86_64", "none", 64u, ABI_NONE, "raw", 1)) return 2;
    if (!expect_valid_target(argv[2], "Linux64", "x86_64", "linux", 64u, ABI_SYSV, "elf", 0)) return 3;
    if (!expect_valid_target(argv[3], "Win64", "x86_64", "windows", 64u, ABI_WIN64, "pe", 0)) return 4;
    if (!expect_valid_target(argv[4], "macOS_AArch64", "aarch64", "macos", 64u, ABI_AAPCS64, "macho", 0)) return 5;
    if (!expect_valid_target(argv[5], "RV32IMC", "riscv32", "none", 32u, ABI_ILP32, "bin", 0)) return 6;

    for (int i = 6; i < argc; ++i) {
        if (!expect_invalid_target(argv[i])) return 7 + i - 6;
    }

    return 0;
}
