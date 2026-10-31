#include "oir_ast_tlv.h"

#include <stdio.h>
#include <string.h>

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL: %s (line %d)\n", m, __LINE__); return 1; } } while (0)

static void put_u16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

int main(void) {
    uint8_t bytes[128];
    uint8_t u32_value[4] = { 42u, 0u, 0u, 0u };
    uint8_t bool_value[1] = { 1u };
    uint8_t token_value[2] = { 96u, 0u };
    uint8_t null_value_dummy = 0;
    OirAstTlvWriter writer;
    OirAstTlvReader reader;
    OirAstTlvField field;
    int has_field = 0;
    size_t written;

    (void)null_value_dummy;
    oir_ast_tlv_writer_init(&writer, bytes, sizeof(bytes));
    CHECK(oir_ast_tlv_write(&writer, 1u, OIR_AST_VALUE_U32, 0u,
        u32_value, sizeof(u32_value)) == OIR_STATUS_OK, "write U32 field");
    CHECK(oir_ast_tlv_write(&writer, 2u, OIR_AST_VALUE_BOOL, OIR_AST_TLV_OPTIONAL,
        bool_value, sizeof(bool_value)) == OIR_STATUS_OK, "write optional BOOL field");
    CHECK(oir_ast_tlv_write(&writer, 3u, OIR_AST_VALUE_TOKEN_ID, 0u,
        token_value, sizeof(token_value)) == OIR_STATUS_OK, "write token ID field");
    CHECK(oir_ast_tlv_write(&writer, 4u, OIR_AST_VALUE_NULL, 0u,
        NULL, 0u) == OIR_STATUS_OK, "write explicit NULL field");
    written = writer.used;
    CHECK(oir_ast_tlv_validate(bytes, written) == OIR_STATUS_OK, "validate emitted TLV sequence");

    oir_ast_tlv_reader_init(&reader, bytes, written);
    CHECK(oir_ast_tlv_reader_next(&reader, &field, &has_field) == OIR_STATUS_OK && has_field,
        "read first TLV");
    CHECK(field.field_id == 1u && field.value_type == OIR_AST_VALUE_U32 &&
        field.value_length == 4u && memcmp(field.value, u32_value, 4u) == 0,
        "first field contents");
    CHECK(oir_ast_tlv_reader_next(&reader, &field, &has_field) == OIR_STATUS_OK && has_field &&
        field.field_id == 2u && field.flags == OIR_AST_TLV_OPTIONAL, "read optional field");
    CHECK(oir_ast_tlv_reader_next(&reader, &field, &has_field) == OIR_STATUS_OK && has_field &&
        field.field_id == 3u && field.value_type == OIR_AST_VALUE_TOKEN_ID, "read token field");
    CHECK(oir_ast_tlv_reader_next(&reader, &field, &has_field) == OIR_STATUS_OK && has_field &&
        field.field_id == 4u && field.value_type == OIR_AST_VALUE_NULL &&
        field.value_length == 0u, "read explicit NULL");
    CHECK(oir_ast_tlv_reader_next(&reader, &field, &has_field) == OIR_STATUS_OK && !has_field,
        "reader reports end of sequence");

    CHECK(oir_ast_tlv_write(&writer, 4u, OIR_AST_VALUE_U32, 0u,
        u32_value, 4u) == OIR_STATUS_INVALID_SECTION, "duplicate field ID rejected");
    CHECK(oir_ast_tlv_write(&writer, 3u, OIR_AST_VALUE_U32, 0u,
        u32_value, 4u) == OIR_STATUS_INVALID_SECTION, "out-of-order field ID rejected");

    {
        uint8_t malformed[16] = { 0 };
        put_u16(malformed, 1u);
        malformed[2] = OIR_AST_VALUE_U32;
        put_u32(malformed + 4, 3u);
        CHECK(oir_ast_tlv_validate(malformed, 11u) == OIR_STATUS_INVALID_SECTION,
            "wrong fixed-width U32 length rejected");
        put_u32(malformed + 4, 4u);
        CHECK(oir_ast_tlv_validate(malformed, 11u) == OIR_STATUS_LENGTH_MISMATCH,
            "truncated TLV value rejected");
        malformed[2] = 99u;
        put_u32(malformed + 4, 0u);
        CHECK(oir_ast_tlv_validate(malformed, 8u) == OIR_STATUS_INVALID_SECTION,
            "unknown value type rejected");
        malformed[2] = OIR_AST_VALUE_BOOL;
        put_u32(malformed + 4, 1u);
        malformed[8] = 2u;
        CHECK(oir_ast_tlv_validate(malformed, 9u) == OIR_STATUS_INVALID_SECTION,
            "noncanonical BOOL rejected");
        malformed[2] = OIR_AST_VALUE_TOKEN_ID;
        put_u16(malformed + 8, 161u);
        CHECK(oir_ast_tlv_validate(malformed, 10u) == OIR_STATUS_INVALID_SECTION,
            "unknown stable token ID rejected");
        malformed[2] = OIR_AST_VALUE_NULL;
        put_u32(malformed + 4, 0u);
        malformed[3] = 0x80u;
        CHECK(oir_ast_tlv_validate(malformed, 8u) == OIR_STATUS_INVALID_SECTION,
            "unknown TLV flags rejected");
        malformed[3] = 0u;
        put_u16(malformed, 0u);
        CHECK(oir_ast_tlv_validate(malformed, 8u) == OIR_STATUS_INVALID_SECTION,
            "zero field ID rejected");
    }

    {
        uint8_t tiny[8];
        OirAstTlvWriter small;
        oir_ast_tlv_writer_init(&small, tiny, sizeof(tiny));
        CHECK(oir_ast_tlv_write(&small, 1u, OIR_AST_VALUE_U32, 0u,
            u32_value, 4u) == OIR_STATUS_BUFFER_TOO_SMALL,
            "small writer buffer rejected");
        CHECK(small.used == 0u, "failed write leaves writer unchanged");
    }

    puts("OIR AST TLV primitive tests passed.");
    return 0;
}
