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

    /* Duplicate/noncanonical IDs are rejected. */
    memcpy(damaged, bytes, written);
    write_u32(damaged + OIR_SEMANTIC_SCOPES_HEADER_SIZE +
        OIR_SEMANTIC_SCOPE_RECORD_SIZE, 1u);
    assert(oir_semantic_scopes_open(damaged, written, &view) ==
        OIR_STATUS_INVALID_SECTION);

    /* A non-root scope must have an owning AST node. */
    memcpy(damaged, bytes, written);
    write_u32(damaged + OIR_SEMANTIC_SCOPES_HEADER_SIZE +
        OIR_SEMANTIC_SCOPE_RECORD_SIZE + 16u, 0u);
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


    {
        const uint8_t names[] = { 'i', '3', '2', 'W', 'i', 'd', 'g', 'e', 't' };
        const OirSemanticType types[] = {
            { 1u, OIR_SEMANTIC_TYPE_PRIMITIVE, 0u, 0u, 3u, 0u, 0u },
            { 2u, OIR_SEMANTIC_TYPE_USER_DEFINED, OIR_SEMANTIC_TYPE_FLAG_HARD,
                3u, 6u, 7u, 1u }
        };
        uint8_t type_bytes[OIR_SEMANTIC_TYPES_HEADER_SIZE +
            2u * OIR_SEMANTIC_TYPE_RECORD_SIZE + sizeof(names)];
        uint8_t type_second[sizeof(type_bytes)];
        uint8_t type_bad[sizeof(type_bytes)];
        size_t type_written = 0u;
        size_t type_second_written = 0u;
        OirSemanticTypesView types_view;
        OirSemanticType decoded_type;
        const uint8_t* decoded_name = NULL;

        assert(oir_semantic_types_write(type_bytes, sizeof(type_bytes), types, 2u,
            names, (uint32_t)sizeof(names), &type_written) == OIR_STATUS_OK);
        assert(type_written == sizeof(type_bytes));
        assert(oir_semantic_types_open(type_bytes, type_written, &types_view) ==
            OIR_STATUS_OK);
        assert(types_view.record_count == 2u);
        assert(oir_semantic_types_get(&types_view, 0u, &decoded_type,
            &decoded_name) == OIR_STATUS_OK);
        assert(decoded_type.type_id == 1u &&
            decoded_type.kind == OIR_SEMANTIC_TYPE_PRIMITIVE &&
            decoded_type.name_length == 3u &&
            decoded_name && memcmp(decoded_name, "i32", 3u) == 0);
        assert(oir_semantic_types_get(&types_view, 1u, &decoded_type,
            &decoded_name) == OIR_STATUS_OK);
        assert(decoded_type.type_id == 2u &&
            decoded_type.kind == OIR_SEMANTIC_TYPE_USER_DEFINED &&
            decoded_type.flags == OIR_SEMANTIC_TYPE_FLAG_HARD &&
            decoded_type.referenced_symbol_id == 7u &&
            decoded_type.base_type_id == 1u &&
            decoded_name && memcmp(decoded_name, "Widget", 6u) == 0);

        assert(oir_semantic_types_write(type_second, sizeof(type_second), types, 2u,
            names, (uint32_t)sizeof(names), &type_second_written) == OIR_STATUS_OK);
        assert(type_second_written == type_written &&
            memcmp(type_bytes, type_second, type_written) == 0);

        memcpy(type_bad, type_bytes, type_written);
        write_u32(type_bad + 4u, 28u);
        assert(oir_semantic_types_open(type_bad, type_written, &types_view) ==
            OIR_STATUS_INVALID_SECTION);

        memcpy(type_bad, type_bytes, type_written);
        write_u32(type_bad, UINT32_MAX);
        assert(oir_semantic_types_open(type_bad, type_written, &types_view) ==
            OIR_STATUS_INVALID_SECTION);

        memcpy(type_bad, type_bytes, type_written);
        write_u32(type_bad + OIR_SEMANTIC_TYPES_HEADER_SIZE +
            OIR_SEMANTIC_TYPE_RECORD_SIZE, 1u);
        assert(oir_semantic_types_open(type_bad, type_written, &types_view) ==
            OIR_STATUS_INVALID_SECTION);

        memcpy(type_bad, type_bytes, type_written);
        write_u32(type_bad + OIR_SEMANTIC_TYPES_HEADER_SIZE + 8u, UINT32_MAX);
        assert(oir_semantic_types_open(type_bad, type_written, &types_view) ==
            OIR_STATUS_INVALID_SECTION);

        memcpy(type_bad, type_bytes, type_written);
        type_bad[OIR_SEMANTIC_TYPES_HEADER_SIZE +
            2u * OIR_SEMANTIC_TYPE_RECORD_SIZE] = 0xc0u;
        assert(oir_semantic_types_open(type_bad, type_written, &types_view) ==
            OIR_STATUS_INVALID_SECTION);

        memcpy(type_bad, type_bytes, type_written);
        type_bad[OIR_SEMANTIC_TYPES_HEADER_SIZE + 4u] = 0xffu;
        type_bad[OIR_SEMANTIC_TYPES_HEADER_SIZE + 5u] = 0u;
        assert(oir_semantic_types_open(type_bad, type_written, &types_view) ==
            OIR_STATUS_INVALID_SECTION);

        memcpy(type_bad, type_bytes, type_written);
        write_u32(type_bad + OIR_SEMANTIC_TYPES_HEADER_SIZE +
            OIR_SEMANTIC_TYPE_RECORD_SIZE + 16u, 0u);
        assert(oir_semantic_types_open(type_bad, type_written, &types_view) ==
            OIR_STATUS_INVALID_SECTION);

        memcpy(type_bad, type_bytes, type_written);
        write_u32(type_bad + OIR_SEMANTIC_TYPES_HEADER_SIZE +
            OIR_SEMANTIC_TYPE_RECORD_SIZE + 20u, 2u);
        assert(oir_semantic_types_open(type_bad, type_written, &types_view) ==
            OIR_STATUS_INVALID_SECTION);

        memset(type_second, 0xa5, sizeof(type_second));
        assert(oir_semantic_types_write(type_second, sizeof(type_second) - 1u,
            types, 2u, names, (uint32_t)sizeof(names), &type_second_written) ==
            OIR_STATUS_BUFFER_TOO_SMALL);
        assert(type_second_written == 0u && type_second[0] == 0xa5u);
    }

    puts("OIR SEMANTIC_AST scope/type codec tests passed.");
    return 0;
}
