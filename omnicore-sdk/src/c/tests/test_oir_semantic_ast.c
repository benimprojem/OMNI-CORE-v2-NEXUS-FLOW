#ifdef NDEBUG
#undef NDEBUG
#endif
#include "oir_semantic_ast.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void write_u32(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)(value & 0xffu);
    p[1] = (uint8_t)((value >> 8) & 0xffu);
    p[2] = (uint8_t)((value >> 16) & 0xffu);
    p[3] = (uint8_t)((value >> 24) & 0xffu);
}

int main(void) {
    const OirSemanticScope scopes[] = {
        { 1u, 0u, 0u, 0u, 0u },
        { 2u, 1u, 1u, 0u, 10u },
        { 3u, 2u, 2u, 0u, 14u }
    };
    uint8_t bytes[OIR_SEMANTIC_SCOPES_HEADER_SIZE +
        3u * OIR_SEMANTIC_SCOPE_RECORD_SIZE];
    uint8_t second[sizeof(bytes)];
    uint8_t damaged[sizeof(bytes)];
    size_t written = 0u;
    size_t second_written = 0u;
    OirSemanticScopesView view;
    OirSemanticScope decoded;

    memset(bytes, 0xa5, sizeof(bytes));
    assert(oir_semantic_scopes_write(bytes, sizeof(bytes), scopes, 3u,
        &written) == OIR_STATUS_OK);
    assert(written == sizeof(bytes));
    assert(oir_semantic_scopes_open(bytes, written, &view) == OIR_STATUS_OK);
    assert(view.record_count == 3u);

    assert(oir_semantic_scopes_get(&view, 0u, &decoded) == OIR_STATUS_OK);
    assert(memcmp(&decoded, &scopes[0], sizeof(decoded)) == 0);
    assert(oir_semantic_scopes_get(&view, 1u, &decoded) == OIR_STATUS_OK);
    assert(memcmp(&decoded, &scopes[1], sizeof(decoded)) == 0);
    assert(oir_semantic_scopes_get(&view, 2u, &decoded) == OIR_STATUS_OK);
    assert(memcmp(&decoded, &scopes[2], sizeof(decoded)) == 0);
    assert(oir_semantic_scopes_get(&view, 3u, &decoded) ==
        OIR_STATUS_INVALID_ARGUMENT);

    assert(oir_semantic_scopes_write(second, sizeof(second), scopes, 3u,
        &second_written) == OIR_STATUS_OK);
    assert(second_written == written && memcmp(bytes, second, written) == 0);

    /* Truncated record array. */
    assert(oir_semantic_scopes_open(bytes, written - 1u, &view) ==
        OIR_STATUS_LENGTH_MISMATCH);

    /* Unsupported record width. */
    memcpy(damaged, bytes, written);
    write_u32(damaged + 4, 24u);
    assert(oir_semantic_scopes_open(damaged, written, &view) ==
        OIR_STATUS_INVALID_SECTION);

    /* Count multiplication / payload bound must be rejected before record access. */
    memcpy(damaged, bytes, written);
    write_u32(damaged, UINT32_MAX);
    assert(oir_semantic_scopes_open(damaged, written, &view) ==
        OIR_STATUS_INVALID_SECTION);

    /* Root must be unique and have no parent/owner. */
    memcpy(damaged, bytes, written);
    write_u32(damaged + OIR_SEMANTIC_SCOPES_HEADER_SIZE + 4u, 1u);
    assert(oir_semantic_scopes_open(damaged, written, &view) ==
        OIR_STATUS_INVALID_SECTION);

    /* Parent must precede child; this also rules out cycles. */
    memcpy(damaged, bytes, written);
    write_u32(damaged + OIR_SEMANTIC_SCOPES_HEADER_SIZE +
        OIR_SEMANTIC_SCOPE_RECORD_SIZE + 4u, 3u);
    assert(oir_semantic_scopes_open(damaged, written, &view) ==
        OIR_STATUS_INVALID_SECTION);

    /* Depth must be exactly parent depth + 1. */
    memcpy(damaged, bytes, written);
    write_u32(damaged + OIR_SEMANTIC_SCOPES_HEADER_SIZE +
        OIR_SEMANTIC_SCOPE_RECORD_SIZE + 8u, 2u);
    assert(oir_semantic_scopes_open(damaged, written, &view) ==
        OIR_STATUS_INVALID_SECTION);

    /* Reserved flags are required to be zero in schema 1.0. */
    memcpy(damaged, bytes, written);
    write_u32(damaged + OIR_SEMANTIC_SCOPES_HEADER_SIZE + 12u, 1u);
    assert(oir_semantic_scopes_open(damaged, written, &view) ==
        OIR_STATUS_INVALID_SECTION);

    /* Writer failure must not touch output. */
    memset(second, 0xa5, sizeof(second));
    assert(oir_semantic_scopes_write(second, sizeof(second) - 1u, scopes, 3u,
        &second_written) == OIR_STATUS_BUFFER_TOO_SMALL);
    assert(second_written == 0u && second[0] == 0xa5u);

    puts("OIR SEMANTIC_AST scope codec tests passed.");
    return 0;
}
