#include "occ_project_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OCC_PROJECT_CONFIG_LINE_BYTES OCC_PROJECT_CONFIG_MAX_PATH_BYTES

typedef enum OccConfigSection {
    OCC_CONFIG_SECTION_OTHER = 0,
    OCC_CONFIG_SECTION_INCLUDE = 1,
    OCC_CONFIG_SECTION_DEPENDENCIES = 2
} OccConfigSection;

static char* occ_config_duplicate(const char* text) {
    size_t length;
    char* copy;
    if (!text) return NULL;
    length = strlen(text);
    if (length == (size_t)-1) return NULL;
    copy = (char*)malloc(length + 1u);
    if (copy) memcpy(copy, text, length + 1u);
    return copy;
}

static char* occ_config_trim(char* text) {
    char* end;
    if (!text) return text;
    while (*text && isspace((unsigned char)*text)) ++text;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) --end;
    *end = '\0';
    return text;
}

static int occ_config_is_control(const char* text) {
    const unsigned char* p = (const unsigned char*)text;
    while (p && *p) {
        if (*p < 0x20u || *p == 0x7fu) return 1;
        ++p;
    }
    return 0;
}

static char* occ_config_project_root(const char* config_path) {
    size_t length;
    char* root;
    char* slash;
    char* backslash;
    char* separator;
    if (!config_path || !config_path[0]) return NULL;
    length = strlen(config_path);
    if (length >= OCC_PROJECT_CONFIG_MAX_PATH_BYTES) return NULL;
    root = occ_config_duplicate(config_path);
    if (!root) return NULL;
    slash = strrchr(root, '/');
    backslash = strrchr(root, '\\');
    separator = slash;
    if (backslash && (!separator || backslash > separator)) separator = backslash;
    if (!separator) {
        free(root);
        return occ_config_duplicate(".");
    }
    if (separator == root ||
        (separator == root + 2 && root[1] == ':')) {
        separator[1] = '\0';
    } else {
        *separator = '\0';
    }
    return root;
}

static int occ_config_add_include(
    OccProjectConfig* config,
    const char* path
) {
    size_t i;
    size_t length;
    char* copy;
    if (!config || !path || !path[0]) return 0;
    length = strlen(path);
    if (length >= OCC_PROJECT_CONFIG_MAX_PATH_BYTES ||
        occ_config_is_control(path)) return 0;
    for (i = 0u; i < config->include_count; ++i) {
        if (strcmp(config->include_paths[i], path) == 0) return 1;
    }
    if (config->include_count >= OCC_PROJECT_CONFIG_MAX_INCLUDE_PATHS)
        return 0;
    copy = occ_config_duplicate(path);
    if (!copy) return -1;
    config->include_paths[config->include_count++] = copy;
    return 1;
}

static OccProjectConfigStatus occ_config_parse_section(
    char* line,
    OccConfigSection* out_section
) {
    char* close;
    char* name;
    char* tail;
    if (!line || !out_section || line[0] != '[') return OCC_PROJECT_CONFIG_SYNTAX_ERROR;
    close = strchr(line + 1, ']');
    if (!close) return OCC_PROJECT_CONFIG_SYNTAX_ERROR;
    *close = '\0';
    name = occ_config_trim(line + 1);
    tail = occ_config_trim(close + 1);
    if (!name[0] || tail[0]) return OCC_PROJECT_CONFIG_SYNTAX_ERROR;
    if (strcmp(name, "include") == 0)
        *out_section = OCC_CONFIG_SECTION_INCLUDE;
    else if (strcmp(name, "dependencies") == 0)
        *out_section = OCC_CONFIG_SECTION_DEPENDENCIES;
    else
        *out_section = OCC_CONFIG_SECTION_OTHER;
    return OCC_PROJECT_CONFIG_OK;
}

static OccProjectConfigStatus occ_config_parse_include(
    OccProjectConfig* config,
    char* line
) {
    char* value = occ_config_trim(line);
    size_t length;
    int added;
    if (!value[0]) return OCC_PROJECT_CONFIG_OK;
    if (value[0] == '"') {
        char* close = strchr(value + 1, '"');
        char* tail;
        if (!close) return OCC_PROJECT_CONFIG_SYNTAX_ERROR;
        *close = '\0';
        tail = occ_config_trim(close + 1);
        if (tail[0] || !value[1]) return OCC_PROJECT_CONFIG_SYNTAX_ERROR;
        value += 1;
    } else if (value[0] == '\'') {
        char* close = strchr(value + 1, '\'');
        char* tail;
        if (!close) return OCC_PROJECT_CONFIG_SYNTAX_ERROR;
        *close = '\0';
        tail = occ_config_trim(close + 1);
        if (tail[0] || !value[1]) return OCC_PROJECT_CONFIG_SYNTAX_ERROR;
        value += 1;
    }
    length = strlen(value);
    if (length == 0u || length >= OCC_PROJECT_CONFIG_MAX_PATH_BYTES ||
        occ_config_is_control(value)) return OCC_PROJECT_CONFIG_SYNTAX_ERROR;
    added = occ_config_add_include(config, value);
    if (added < 0) return OCC_PROJECT_CONFIG_OUT_OF_MEMORY;
    if (added == 0) return OCC_PROJECT_CONFIG_LIMIT_EXCEEDED;
    return OCC_PROJECT_CONFIG_OK;
}

void occ_project_config_destroy(OccProjectConfig* config) {
    size_t i;
    if (!config) return;
    free(config->project_root);
    for (i = 0u; i < config->include_count; ++i)
        free(config->include_paths[i]);
    memset(config, 0, sizeof(*config));
}

OccProjectConfigStatus occ_project_config_load(
    const char* config_path,
    OccProjectConfig* out_config
) {
    FILE* file;
    OccProjectConfig parsed;
    OccConfigSection section = OCC_CONFIG_SECTION_OTHER;
    char line[OCC_PROJECT_CONFIG_LINE_BYTES];
    size_t total_bytes = 0u;
    OccProjectConfigStatus status = OCC_PROJECT_CONFIG_OK;

    if (!config_path || !config_path[0] || !out_config)
        return OCC_PROJECT_CONFIG_INVALID_ARGUMENT;
    memset(out_config, 0, sizeof(*out_config));
    memset(&parsed, 0, sizeof(parsed));

    parsed.project_root = occ_config_project_root(config_path);
    if (!parsed.project_root) return OCC_PROJECT_CONFIG_OUT_OF_MEMORY;

    file = fopen(config_path, "rb");
    if (!file) {
        occ_project_config_destroy(&parsed);
        return OCC_PROJECT_CONFIG_IO_ERROR;
    }

    while (fgets(line, sizeof(line), file)) {
        size_t length = strlen(line);
        char* value;
        char* comment;
        if (length > OCC_PROJECT_CONFIG_MAX_FILE_BYTES - total_bytes) {
            status = OCC_PROJECT_CONFIG_LIMIT_EXCEEDED;
            break;
        }
        total_bytes += length;
        if (length == sizeof(line) - 1u && line[length - 1u] != '\n' && !feof(file)) {
            status = OCC_PROJECT_CONFIG_LIMIT_EXCEEDED;
            break;
        }

        comment = strchr(line, '#');
        if (comment) *comment = '\0';
        value = occ_config_trim(line);
        if (!value[0]) continue;

        if (value[0] == '[') {
            status = occ_config_parse_section(value, &section);
            if (status != OCC_PROJECT_CONFIG_OK) break;
            continue;
        }

        if (section == OCC_CONFIG_SECTION_INCLUDE) {
            status = occ_config_parse_include(&parsed, value);
            if (status != OCC_PROJECT_CONFIG_OK) break;
        }
        /* [dependencies] is intentionally owned by OCM, not this parser. */
    }

    if (ferror(file) && status == OCC_PROJECT_CONFIG_OK)
        status = OCC_PROJECT_CONFIG_IO_ERROR;
    if (fclose(file) != 0 && status == OCC_PROJECT_CONFIG_OK)
        status = OCC_PROJECT_CONFIG_IO_ERROR;

    if (status != OCC_PROJECT_CONFIG_OK) {
        occ_project_config_destroy(&parsed);
        return status;
    }
    *out_config = parsed;
    return OCC_PROJECT_CONFIG_OK;
}

const char* occ_project_config_status_string(OccProjectConfigStatus status) {
    switch (status) {
        case OCC_PROJECT_CONFIG_OK: return "ok";
        case OCC_PROJECT_CONFIG_INVALID_ARGUMENT: return "invalid argument";
        case OCC_PROJECT_CONFIG_IO_ERROR: return "I/O error";
        case OCC_PROJECT_CONFIG_SYNTAX_ERROR: return "syntax error";
        case OCC_PROJECT_CONFIG_LIMIT_EXCEEDED: return "resource limit exceeded";
        case OCC_PROJECT_CONFIG_OUT_OF_MEMORY: return "out of memory";
        default: return "unknown project config status";
    }
}
