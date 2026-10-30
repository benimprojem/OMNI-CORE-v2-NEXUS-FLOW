#include "target_parser.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TARGET_LINE_SIZE 512
#define TARGET_SECTION_SIZE 64
#define TARGET_KEY_PATH_SIZE 96
#define TARGET_MAX_KEYS 64

enum {
    REQUIRED_VERSION = 1u << 0,
    REQUIRED_IDENTIFIER = 1u << 1,
    REQUIRED_OS = 1u << 2,
    REQUIRED_ARCH = 1u << 3,
    REQUIRED_BITS = 1u << 4,
    REQUIRED_ENDIAN = 1u << 5,
    REQUIRED_SIMD = 1u << 6,
    REQUIRED_ABI = 1u << 7,
    REQUIRED_FORMAT = 1u << 8,
    REQUIRED_ALL = (1u << 9) - 1u
};

static void set_error(char* out, size_t out_size, const char* format, ...) {
    if (!out || out_size == 0) return;
    va_list args;
    va_start(args, format);
    vsnprintf(out, out_size, format, args);
    va_end(args);
}

static char* trim(char* text) {
    while (*text && isspace((unsigned char)*text)) text++;
    size_t length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1])) {
        text[--length] = '\0';
    }
    return text;
}

static void strip_comment(char* line) {
    int in_quotes = 0;
    int escaped = 0;
    for (char* p = line; *p; ++p) {
        if (escaped) {
            escaped = 0;
            continue;
        }
        if (in_quotes && *p == '\\') {
            escaped = 1;
            continue;
        }
        if (*p == '"') in_quotes = !in_quotes;
        else if (*p == '#' && !in_quotes) {
            *p = '\0';
            return;
        }
    }
}

static int parse_string(const char* value, char* out, size_t out_size) {
    if (!value || !out || out_size == 0 || value[0] != '"') return 0;
    const char* closing = strrchr(value + 1, '"');
    if (!closing) return 0;
    for (const char* p = closing + 1; *p; ++p) {
        if (!isspace((unsigned char)*p)) return 0;
    }
    size_t length = (size_t)(closing - (value + 1));
    if (length >= out_size) return 0;
    memcpy(out, value + 1, length);
    out[length] = '\0';
    return 1;
}

static int parse_unsigned(const char* value, unsigned long long* out) {
    if (!value || !out || !*value || *value == '-') return 0;
    errno = 0;
    char* end = NULL;
    unsigned long long number = strtoull(value, &end, 0);
    if (errno != 0 || end == value) return 0;

    while (*end && isspace((unsigned char)*end)) end++;
    if (*end == 'k' || *end == 'K') {
        if (number > ULLONG_MAX / 1024ULL) return 0;
        number *= 1024ULL;
        end++;
    } else if (*end == 'm' || *end == 'M') {
        if (number > ULLONG_MAX / (1024ULL * 1024ULL)) return 0;
        number *= 1024ULL * 1024ULL;
        end++;
    }

    while (*end && isspace((unsigned char)*end)) end++;
    if (*end) return 0;
    *out = number;
    return 1;
}

static int parse_boolean(const char* value) {
    return value && (strcmp(value, "true") == 0 || strcmp(value, "false") == 0);
}

static int validate_array(const char* value) {
    if (!value) return 0;
    size_t length = strlen(value);
    if (length < 2 || value[0] != '[' || value[length - 1] != ']') return 0;

    int in_quotes = 0;
    int escaped = 0;
    for (size_t i = 1; i + 1 < length; ++i) {
        char ch = value[i];
        if (escaped) {
            escaped = 0;
            continue;
        }
        if (in_quotes && ch == '\\') {
            escaped = 1;
            continue;
        }
        if (ch == '"') in_quotes = !in_quotes;
        else if (ch == '[' || ch == ']') return 0;
    }
    return !in_quotes && !escaped;
}

static int known_section(const char* section) {
    static const char* sections[] = {
        "target_info", "cpu_arch", "instruction_set", "memory_model",
        "abi_convention", "output_format", "nexus_primitives",
        "nexus_primitives.common", "nexus_primitives.arch_specific"
    };
    for (size_t i = 0; i < sizeof(sections) / sizeof(sections[0]); ++i) {
        if (strcmp(section, sections[i]) == 0) return 1;
    }
    return 0;
}

static int known_key(const char* section, const char* key) {
    if (strcmp(section, "target_info") == 0) {
        return strcmp(key, "spec_version") == 0 ||
            strcmp(key, "identifier") == 0 ||
            strcmp(key, "os") == 0 ||
            strcmp(key, "author") == 0;
    }
    if (strcmp(section, "cpu_arch") == 0) {
        return strcmp(key, "family") == 0 ||
            strcmp(key, "bits") == 0 ||
            strcmp(key, "endian") == 0 ||
            strcmp(key, "registers") == 0 ||
            strcmp(key, "scratch_regs") == 0 ||
            strcmp(key, "reserved_regs") == 0;
    }
    if (strcmp(section, "instruction_set") == 0) {
        return strcmp(key, "simd") == 0 ||
            strcmp(key, "atomic") == 0 ||
            strcmp(key, "fastexec_support") == 0;
    }
    if (strcmp(section, "memory_model") == 0) {
        return strcmp(key, "page_size") == 0 ||
            strcmp(key, "stack_alignment") == 0 ||
            strcmp(key, "default_area_size") == 0 ||
            strcmp(key, "zone_sync_mode") == 0 ||
            strcmp(key, "scratchpad_addr") == 0 ||
            strcmp(key, "dma_support") == 0;
    }
    if (strcmp(section, "abi_convention") == 0) {
        return strcmp(key, "name") == 0 ||
            strcmp(key, "shadow_space") == 0 ||
            strcmp(key, "return_reg") == 0 ||
            strcmp(key, "param_regs") == 0;
    }
    if (strcmp(section, "output_format") == 0) {
        return strcmp(key, "binary_type") == 0 ||
            strcmp(key, "object_ext") == 0 ||
            strcmp(key, "shared_lib_ext") == 0 ||
            strcmp(key, "entry_point") == 0;
    }
    if (strncmp(section, "nexus_primitives", 16) == 0) {
        return strcmp(key, "handler_init") == 0 ||
            strcmp(key, "relocate_op") == 0 ||
            strcmp(key, "trigger_compute") == 0 ||
            strcmp(key, "sync_barrier") == 0;
    }
    return 0;
}

static int parse_abi(const char* name, TargetABI* abi) {
    if (strcmp(name, "none") == 0) *abi = ABI_NONE;
    else if (strcmp(name, "baremetal") == 0) *abi = ABI_BAREMETAL;
    else if (strcmp(name, "ms_x64") == 0) *abi = ABI_WIN64;
    else if (strcmp(name, "sysv_amd64") == 0) *abi = ABI_SYSV;
    else if (strcmp(name, "ilp32") == 0) *abi = ABI_ILP32;
    else if (strcmp(name, "aapcs64") == 0) *abi = ABI_AAPCS64;
    else return 0;
    return 1;
}

static int parse_simd(const char* name, unsigned int* features) {
    if (strcmp(name, "none") == 0) *features = FEAT_NONE;
    else if (strcmp(name, "sse4_2") == 0) *features = FEAT_SSE | FEAT_SSE2 | FEAT_SSE3 | FEAT_SSE4;
    else if (strcmp(name, "avx2") == 0) *features = FEAT_SSE | FEAT_SSE2 | FEAT_SSE3 | FEAT_SSE4 | FEAT_AVX | FEAT_AVX2;
    else if (strcmp(name, "avx512") == 0) *features = FEAT_SSE | FEAT_SSE2 | FEAT_SSE3 | FEAT_SSE4 | FEAT_AVX | FEAT_AVX2 | FEAT_AVX512;
    else if (strcmp(name, "neon") == 0) *features = FEAT_NEON;
    else return 0;
    return 1;
}

static int parse_spec_version(const char* value) {
    errno = 0;
    char* end = NULL;
    double version = strtod(value, &end);
    if (errno != 0 || end == value || version != 1.0) return 0;
    while (*end && isspace((unsigned char)*end)) end++;
    return *end == '\0';
}

int target_config_load(
    const char* path,
    TargetConfig* config,
    char* error_message,
    size_t error_message_size
) {
    if (error_message && error_message_size) error_message[0] = '\0';
    if (!path || !config) {
        set_error(error_message, error_message_size, "Target path or output config is null.");
        return 0;
    }

    FILE* file = fopen(path, "r");
    if (!file) {
        set_error(error_message, error_message_size, "Target file could not be opened: %s", path);
        return 0;
    }

    memset(config, 0, sizeof(*config));
    config->stack_size = 4096;
    config->abi = ABI_NONE;

    char section[TARGET_SECTION_SIZE] = "";
    char line[TARGET_LINE_SIZE];
    char seen[TARGET_MAX_KEYS][TARGET_KEY_PATH_SIZE];
    size_t seen_count = 0;
    unsigned int required = 0;
    unsigned long long numeric_value = 0;
    unsigned long line_number = 0;
    int ok = 1;

    while (fgets(line, sizeof(line), file)) {
        line_number++;
        if (!strchr(line, '\n') && !feof(file)) {
            set_error(error_message, error_message_size, "Target line %lu exceeds %u bytes.", line_number, TARGET_LINE_SIZE - 1);
            ok = 0;
            break;
        }

        strip_comment(line);
        char* text = trim(line);
        if (!*text) continue;

        if (*text == '[') {
            char* closing = strchr(text + 1, ']');
            if (!closing) {
                set_error(error_message, error_message_size, "Target line %lu has an invalid section header.", line_number);
                ok = 0;
                break;
            }
            *closing = '\0';
            char* trailing = trim(closing + 1);
            char* name = trim(text + 1);
            if (*trailing || strlen(name) >= sizeof(section) || !known_section(name)) {
                set_error(error_message, error_message_size, "Target line %lu has an unknown or invalid section.", line_number);
                ok = 0;
                break;
            }
            strcpy(section, name);
            continue;
        }

        char* equals = strchr(text, '=');
        if (!equals || !*section) {
            set_error(error_message, error_message_size, "Target line %lu is not a valid key = value assignment.", line_number);
            ok = 0;
            break;
        }

        *equals = '\0';
        char* key = trim(text);
        char* value = trim(equals + 1);
        if (!*key || !*value || !known_key(section, key)) {
            set_error(error_message, error_message_size, "Target line %lu has an unknown key or empty value.", line_number);
            ok = 0;
            break;
        }

        char key_path[TARGET_KEY_PATH_SIZE];
        int key_path_length = snprintf(key_path, sizeof(key_path), "%s.%s", section, key);
        if (key_path_length < 0 || (size_t)key_path_length >= sizeof(key_path)) {
            set_error(error_message, error_message_size, "Target line %lu has an overlong key.", line_number);
            ok = 0;
            break;
        }
        for (size_t i = 0; i < seen_count; ++i) {
            if (strcmp(seen[i], key_path) == 0) {
                set_error(error_message, error_message_size, "Target line %lu duplicates key %s.", line_number, key_path);
                ok = 0;
                break;
            }
        }
        if (!ok) break;
        if (seen_count >= TARGET_MAX_KEYS) {
            set_error(error_message, error_message_size, "Target contains too many keys.");
            ok = 0;
            break;
        }
        strcpy(seen[seen_count++], key_path);

        char string_value[256];
        if (strcmp(key, "spec_version") == 0) {
            if (!parse_spec_version(value)) {
                set_error(error_message, error_message_size, "Target line %lu has an unsupported spec_version.", line_number);
                ok = 0;
                break;
            }
            required |= REQUIRED_VERSION;
        } else if (strcmp(key, "identifier") == 0) {
            if (!parse_string(value, config->identifier, sizeof(config->identifier))) {
                set_error(error_message, error_message_size, "Target line %lu has an invalid identifier.", line_number);
                ok = 0;
                break;
            }
            required |= REQUIRED_IDENTIFIER;
        } else if (strcmp(key, "os") == 0) {
            if (!parse_string(value, config->os, sizeof(config->os)) ||
                (strcmp(config->os, "none") != 0 && strcmp(config->os, "windows") != 0 &&
                 strcmp(config->os, "linux") != 0 && strcmp(config->os, "macos") != 0)) {
                set_error(error_message, error_message_size, "Target line %lu has an unsupported OS.", line_number);
                ok = 0;
                break;
            }
            required |= REQUIRED_OS;
        } else if (strcmp(key, "family") == 0) {
            if (!parse_string(value, config->arch, sizeof(config->arch)) ||
                (strcmp(config->arch, "x86_64") != 0 && strcmp(config->arch, "aarch64") != 0 &&
                 strcmp(config->arch, "riscv32") != 0)) {
                set_error(error_message, error_message_size, "Target line %lu has an unsupported architecture.", line_number);
                ok = 0;
                break;
            }
            required |= REQUIRED_ARCH;
        } else if (strcmp(key, "bits") == 0) {
            if (!parse_unsigned(value, &numeric_value) || numeric_value > 128u || numeric_value == 0u) {
                set_error(error_message, error_message_size, "Target line %lu has invalid architecture bits.", line_number);
                ok = 0;
                break;
            }
            config->bits = (unsigned int)numeric_value;
            required |= REQUIRED_BITS;
        } else if (strcmp(key, "endian") == 0) {
            if (!parse_string(value, config->endian, sizeof(config->endian)) ||
                (strcmp(config->endian, "little") != 0 && strcmp(config->endian, "big") != 0)) {
                set_error(error_message, error_message_size, "Target line %lu has invalid endianness.", line_number);
                ok = 0;
                break;
            }
            required |= REQUIRED_ENDIAN;
        } else if (strcmp(key, "simd") == 0) {
            if (!parse_string(value, string_value, sizeof(string_value)) ||
                !parse_simd(string_value, &config->features)) {
                set_error(error_message, error_message_size, "Target line %lu has an unknown SIMD feature profile.", line_number);
                ok = 0;
                break;
            }
            required |= REQUIRED_SIMD;
        } else if (strcmp(key, "name") == 0 && strcmp(section, "abi_convention") == 0) {
            if (!parse_string(value, config->abi_name, sizeof(config->abi_name)) ||
                !parse_abi(config->abi_name, &config->abi)) {
                set_error(error_message, error_message_size, "Target line %lu has an unknown ABI.", line_number);
                ok = 0;
                break;
            }
            required |= REQUIRED_ABI;
        } else if (strcmp(key, "binary_type") == 0) {
            if (!parse_string(value, config->format, sizeof(config->format)) ||
                (strcmp(config->format, "raw") != 0 && strcmp(config->format, "bin") != 0 &&
                 strcmp(config->format, "pe") != 0 && strcmp(config->format, "elf") != 0 &&
                 strcmp(config->format, "macho") != 0)) {
                set_error(error_message, error_message_size, "Target line %lu has an unknown output format.", line_number);
                ok = 0;
                break;
            }
            required |= REQUIRED_FORMAT;
        } else if (strcmp(key, "registers") == 0 || strcmp(key, "scratch_regs") == 0 ||
                   strcmp(key, "reserved_regs") == 0 || strcmp(key, "param_regs") == 0) {
            if (!validate_array(value)) {
                set_error(error_message, error_message_size, "Target line %lu has an invalid array.", line_number);
                ok = 0;
                break;
            }
        } else if (strcmp(key, "atomic") == 0 || strcmp(key, "fastexec_support") == 0 ||
                   strcmp(key, "dma_support") == 0) {
            if (!parse_boolean(value)) {
                set_error(error_message, error_message_size, "Target line %lu has an invalid boolean.", line_number);
                ok = 0;
                break;
            }
        } else if (strcmp(key, "page_size") == 0 || strcmp(key, "stack_alignment") == 0 ||
                   strcmp(key, "default_area_size") == 0 || strcmp(key, "scratchpad_addr") == 0 ||
                   strcmp(key, "shadow_space") == 0) {
            if (!parse_unsigned(value, &numeric_value)) {
                set_error(error_message, error_message_size, "Target line %lu has an invalid numeric value.", line_number);
                ok = 0;
                break;
            }
        } else {
            if (!parse_string(value, string_value, sizeof(string_value))) {
                set_error(error_message, error_message_size, "Target line %lu has an invalid string value.", line_number);
                ok = 0;
                break;
            }
        }
    }

    if (ferror(file)) {
        set_error(error_message, error_message_size, "Error while reading target file: %s", path);
        ok = 0;
    }
    fclose(file);

    if (ok && required != REQUIRED_ALL) {
        set_error(error_message, error_message_size, "Target descriptor is missing required fields (mask 0x%X).", REQUIRED_ALL & ~required);
        ok = 0;
    }
    if (ok && strcmp(config->identifier, "") == 0) {
        set_error(error_message, error_message_size, "Target identifier is empty.");
        ok = 0;
    }
    return ok;
}

int target_backend_check(
    const TargetConfig* config,
    char* error_message,
    size_t error_message_size
) {
    if (!config) {
        set_error(error_message, error_message_size, "Target config is null.");
        return 0;
    }

    if (strcmp(config->arch, "x86_64") != 0 || config->bits != 64u) {
        set_error(error_message, error_message_size,
            "The current C backend supports only x86_64/64-bit targets (requested %s/%u).",
            config->arch, config->bits);
        return 0;
    }
    if (strcmp(config->os, "none") != 0 ||
        (config->abi != ABI_NONE && config->abi != ABI_BAREMETAL) ||
        strcmp(config->format, "raw") != 0) {
        set_error(error_message, error_message_size,
            "The current C backend supports only the bare-metal raw profile (requested OS=%s ABI=%s format=%s).",
            config->os, config->abi_name, config->format);
        return 0;
    }
    if (strcmp(config->endian, "little") != 0) {
        set_error(error_message, error_message_size, "The current C backend supports little-endian targets only.");
        return 0;
    }
    return 1;
}
