#ifndef OCC_TARGET_PARSER_H
#define OCC_TARGET_PARSER_H

#include "codegen.h"
#include <stddef.h>

int target_config_load(
    const char* path,
    TargetConfig* config,
    char* error_message,
    size_t error_message_size
);

int target_backend_check(
    const TargetConfig* config,
    char* error_message,
    size_t error_message_size
);

#endif
