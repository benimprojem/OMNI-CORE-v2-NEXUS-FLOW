#ifndef OMNICORE_OIR_TOKEN_MAP_H
#define OMNICORE_OIR_TOKEN_MAP_H

#include <stdint.h>
#include "lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OIR_TOKEN_REGISTRY_VERSION 1u
#define OIR_TOKEN_REGISTRY_COUNT 160u

/* Stable OIR IDs are independent from the C enum's numeric ordinals. */
uint16_t oir_token_id_from_omni(OmniTokenType token);
int oir_token_id_to_omni(uint16_t token_id, OmniTokenType* out_token);

#ifdef __cplusplus
}
#endif

#endif
