#include "oir_token_map.h"

#include <stdio.h>

int main(void) {
    unsigned int id;
    unsigned int count = 0;

    for (id = 1; id <= OIR_TOKEN_REGISTRY_COUNT; ++id) {
        OmniTokenType token;
        if (!oir_token_id_to_omni((uint16_t)id, &token)) {
            fprintf(stderr, "Missing reverse token mapping for OIR ID %u\n", id);
            return 1;
        }
        if (oir_token_id_from_omni(token) != id) {
            fprintf(stderr, "Token mapping round-trip failed for OIR ID %u\n", id);
            return 1;
        }
        ++count;
    }

    if (oir_token_id_from_omni(TOKEN_KW_IF) != 161u ||
        oir_token_id_from_omni(TOKEN_KW_ELS) != 162u ||
        oir_token_id_from_omni(TOKEN_KW_ELSIF) != 163u ||
        oir_token_id_from_omni(TOKEN_ASM_BODY) != 164u ||
        oir_token_id_from_omni(TOKEN_BIT_SHL) != 165u ||
        oir_token_id_from_omni(TOKEN_BIT_SHR) != 166u) {
        fprintf(stderr, "if/els/elsif token IDs are not stable\n");
        return 1;
    }

    if (oir_token_id_from_omni((OmniTokenType)-1) != 0 ||
        oir_token_id_from_omni((OmniTokenType)65535) != 0) {
        fprintf(stderr, "Unknown frontend token enum must map to zero\n");
        return 1;
    }

    {
        OmniTokenType ignored;
        if (oir_token_id_to_omni(0, &ignored) ||
            oir_token_id_to_omni((uint16_t)(OIR_TOKEN_REGISTRY_COUNT + 1u), &ignored) ||
            oir_token_id_to_omni(1, NULL)) {
            fprintf(stderr, "Invalid OIR token ID or output pointer was accepted\n");
            return 1;
        }
    }

    if (count != OIR_TOKEN_REGISTRY_COUNT) return 1;
    printf("OIR token registry mapping passed for %u token kinds.\n", count);
    return 0;
}
