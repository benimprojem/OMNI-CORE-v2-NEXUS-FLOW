#ifndef OMNICORE_OIR_AST_TLV_H
#define OMNICORE_OIR_AST_TLV_H

#include "oir.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OIR_AST_TLV_HEADER_SIZE 8u
#define OIR_AST_TLV_OPTIONAL 0x01u
#define OIR_AST_TLV_KNOWN_FLAGS OIR_AST_TLV_OPTIONAL
#define OIR_AST_TOKEN_ID_MAX 166u

typedef enum OirAstValueType {
    OIR_AST_VALUE_NULL = 0,
    OIR_AST_VALUE_BOOL = 1,
    OIR_AST_VALUE_U32 = 2,
    OIR_AST_VALUE_I32 = 3,
    OIR_AST_VALUE_U64 = 4,
    OIR_AST_VALUE_I64 = 5,
    OIR_AST_VALUE_F64 = 6,
    OIR_AST_VALUE_TOKEN_ID = 7,
    OIR_AST_VALUE_STRING_REF = 8,
    OIR_AST_VALUE_NODE_REF = 9
} OirAstValueType;

typedef struct OirAstTlvWriter {
    uint8_t* buffer;
    size_t capacity;
    size_t used;
    uint16_t last_field_id;
} OirAstTlvWriter;

typedef struct OirAstTlvField {
    uint16_t field_id;
    uint8_t value_type;
    uint8_t flags;
    const uint8_t* value;
    uint32_t value_length;
} OirAstTlvField;

typedef struct OirAstTlvReader {
    const uint8_t* buffer;
    size_t length;
    size_t offset;
    uint16_t last_field_id;
} OirAstTlvReader;

void oir_ast_tlv_writer_init(OirAstTlvWriter* writer, uint8_t* buffer, size_t capacity);
OirStatus oir_ast_tlv_write(
    OirAstTlvWriter* writer,
    uint16_t field_id,
    uint8_t value_type,
    uint8_t flags,
    const uint8_t* value,
    uint32_t value_length
);
void oir_ast_tlv_reader_init(OirAstTlvReader* reader, const uint8_t* buffer, size_t length);
OirStatus oir_ast_tlv_reader_next(
    OirAstTlvReader* reader,
    OirAstTlvField* out_field,
    int* out_has_field
);
OirStatus oir_ast_tlv_validate(const uint8_t* buffer, size_t length);

#ifdef __cplusplus
}
#endif

#endif
