#ifndef OCC_TARGET_PROFILE_PARSER_H
#define OCC_TARGET_PROFILE_PARSER_H

#include "target_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Loads the bounded initial v2 text format: [target_info] and one [cpu] section.
 * Heterogeneous device/resource tables and exact-machine binding are deferred. */
int occ_target_profile_load(
    const char* path,
    OccTargetProfile* profile,
    char* error_message,
    size_t error_message_size
);

#ifdef __cplusplus
}
#endif

#endif
