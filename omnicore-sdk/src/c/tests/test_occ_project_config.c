#include "occ_project_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; \
} } while (0)

static int write_text(const char* path, const char* text) {
    FILE* file = fopen(path, "wb");
    size_t length;
    int ok;
    if (!file) return 0;
    length = strlen(text);
    ok = fwrite(text, 1u, length, file) == length;
    if (fclose(file) != 0) ok = 0;
    return ok;
}

int main(int argc, char** argv) {
    OccProjectConfig config;
    char malformed_path[OCC_PROJECT_CONFIG_MAX_PATH_BYTES];
    char missing_path[OCC_PROJECT_CONFIG_MAX_PATH_BYTES];
    const char* valid =
        "[package]\n"
        "name = \"sample\"\n"
        "[include]\n"
        "./lib\n"
        "\"../shared libraries\"\n"
        "./lib # duplicate is ignored\n"
        "[dependencies]\n"
        "math = \">=1.0\"\n"
        "[flags]\n"
        "-O2\n";

    CHECK(argc == 2);
    memset(&config, 0, sizeof(config));
    CHECK(write_text(argv[1], valid));
    CHECK(occ_project_config_load(argv[1], &config) == OCC_PROJECT_CONFIG_OK);
    CHECK(config.project_root != NULL);
    CHECK(config.include_count == 2u);
    CHECK(strcmp(config.include_paths[0], "./lib") == 0);
    CHECK(strcmp(config.include_paths[1], "../shared libraries") == 0);
    occ_project_config_destroy(&config);

    CHECK(snprintf(malformed_path, sizeof(malformed_path),
        "%s.malformed", argv[1]) > 0);
    CHECK(write_text(malformed_path, "[include]\n\"unterminated\n"));
    CHECK(occ_project_config_load(malformed_path, &config) ==
        OCC_PROJECT_CONFIG_SYNTAX_ERROR);
    CHECK(config.project_root == NULL && config.include_count == 0u);
    (void)remove(malformed_path);

    CHECK(snprintf(missing_path, sizeof(missing_path),
        "%s.missing", argv[1]) > 0);
    CHECK(occ_project_config_load(missing_path, &config) ==
        OCC_PROJECT_CONFIG_IO_ERROR);
    CHECK(config.project_root == NULL && config.include_count == 0u);

    (void)remove(argv[1]);
    puts("OCC project config include parsing tests passed.");
    return 0;
}
