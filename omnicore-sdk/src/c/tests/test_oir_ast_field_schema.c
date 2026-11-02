#include "oir_ast_field_schema.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void put_u16(uint8_t* p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put_u32(uint8_t* p, uint32_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8);
    p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}

static uint16_t count_for_kind(uint16_t k) {
    switch(k) {
        case 1: case 2: case 29: case 14: case 23: case 34: case 35: case 37: case 38: case 39: case 69: return 1;
        case 3: return 8;
        case 4: case 12: return 7;
        case 5: case 7: return 6;
        case 6: case 8: case 9: case 11: case 19: case 20:
        case 30: case 33: case 36: case 41: case 71: return 2;
        case 10: return 4;
        case 13: return 5;
        case 15: case 18: return 3;
        case 21: case 98: case 99: return 2;
        case 16: case 17: case 22: case 26: case 24: return 2 + (k == 24);
        case 25: return 7;
        case 27: case 28: return 0;
        case 31: case 32: return 6;
        case 40: case 42: case 43: case 44: case 45: case 46: case 47:
        case 48: case 49: case 50: case 51: case 52: case 53: case 54:
        case 55: case 56: case 57: case 58: case 59: case 60: case 61:
        case 62: case 63: case 64: case 65: case 66: case 67: case 68:
        case 72: case 73: case 74: case 75: case 76: case 77: case 78:
        case 79: case 80: return 2;
        case 70: return 4;
        case 81: case 83: case 84: case 86: return 2;
        case 82: return 3;
        case 85: return 1;
        case 87: case 93: return 3;
        case 88: case 89: case 92: case 95: case 96: return 2;
        case 94: return 3;
        case 97: return 1;
        case 90: case 91: return 1;
        default: return 0xffffu;
    }
}

static uint8_t type_for(uint16_t k, uint16_t f) {
    switch(k) {
        case 1: case 2: case 29: case 35: case 38: case 69: return OIR_AST_VALUE_NODE_REF;
        case 3: if(f<=2)return OIR_AST_VALUE_NULL; if(f<=6)return OIR_AST_VALUE_BOOL; return OIR_AST_VALUE_NODE_REF;
        case 4: case 12: if(f<=2)return OIR_AST_VALUE_NULL; if(f<=5)return OIR_AST_VALUE_BOOL; return OIR_AST_VALUE_NODE_REF;
        case 5: case 7: if(f==1)return OIR_AST_VALUE_NULL; if(f<=5)return OIR_AST_VALUE_BOOL; return OIR_AST_VALUE_NODE_REF;
        case 6: case 9: case 11: return f==1?OIR_AST_VALUE_NULL:OIR_AST_VALUE_NODE_REF;
        case 8: case 71: return OIR_AST_VALUE_NULL;
        case 10: if(f==1)return OIR_AST_VALUE_NULL; if(f<=3)return OIR_AST_VALUE_BOOL; return OIR_AST_VALUE_NODE_REF;
        case 13: return f==1?OIR_AST_VALUE_TOKEN_ID:(f==2?OIR_AST_VALUE_U32:(f==3?OIR_AST_VALUE_I64:(f==4?OIR_AST_VALUE_F64:OIR_AST_VALUE_NULL)));
        case 14: case 34: case 37: case 39: return OIR_AST_VALUE_NULL;
        case 15: case 18: return f==1?OIR_AST_VALUE_TOKEN_ID:OIR_AST_VALUE_NODE_REF;
        case 21: case 98: case 99: return OIR_AST_VALUE_NODE_REF;
        case 16: case 17: case 26: return f==1?OIR_AST_VALUE_TOKEN_ID:OIR_AST_VALUE_NODE_REF;
        case 22: return OIR_AST_VALUE_NODE_REF;
        case 19: case 40: case 42: case 43: case 44: case 45: case 46: case 47:
        case 48: case 49: case 50: case 51: case 52: case 53: case 54: case 55:
        case 56: case 57: case 58: case 59: case 60: case 61: case 62: case 63:
        case 64: case 65: case 66: case 67: case 68: case 72: case 73: case 74:
        case 75: case 76: case 77: case 78: case 79: case 80: return OIR_AST_VALUE_NODE_REF;
        case 20: case 30: case 33: case 36: case 41: return f==1?OIR_AST_VALUE_NULL:OIR_AST_VALUE_NODE_REF;
        case 23: case 24: return OIR_AST_VALUE_NODE_REF;
        case 25: return f==1?OIR_AST_VALUE_I32:OIR_AST_VALUE_NODE_REF;
        case 31: case 32: return f==1?OIR_AST_VALUE_TOKEN_ID:((f==4||f==5)?OIR_AST_VALUE_I32:OIR_AST_VALUE_NODE_REF);
        case 70: return f==1?OIR_AST_VALUE_NULL:OIR_AST_VALUE_NODE_REF;
        case 81: case 83: case 84: return OIR_AST_VALUE_NODE_REF;
        case 82: return f==3?OIR_AST_VALUE_BOOL:OIR_AST_VALUE_NODE_REF;
        case 85: return OIR_AST_VALUE_NULL;
        case 86: return f==1?OIR_AST_VALUE_NULL:OIR_AST_VALUE_NODE_REF;
        case 87: case 92: return f==1?OIR_AST_VALUE_NULL:OIR_AST_VALUE_NODE_REF;
        case 88: case 89: case 90: case 91: case 95: case 96: return OIR_AST_VALUE_NODE_REF;
        case 93: return f<=2?OIR_AST_VALUE_NULL:OIR_AST_VALUE_NODE_REF;
        case 94: return f==3?OIR_AST_VALUE_BOOL:OIR_AST_VALUE_NODE_REF;
        case 97: return OIR_AST_VALUE_NODE_REF;
        default: return 0xffu;
    }
}

static size_t emit_field(uint8_t* out, uint16_t id, uint8_t type, uint8_t flags) {
    uint32_t length = type == OIR_AST_VALUE_NULL ? 0u :
        (type == OIR_AST_VALUE_BOOL ? 1u :
        (type == OIR_AST_VALUE_U32 || type == OIR_AST_VALUE_I32 ||
         type == OIR_AST_VALUE_NODE_REF ? 4u :
        (type == OIR_AST_VALUE_U64 || type == OIR_AST_VALUE_I64 ||
         type == OIR_AST_VALUE_F64 ? 8u :
        (type == OIR_AST_VALUE_TOKEN_ID ? 2u : 8u))));
    memset(out, 0, 8u + length);
    put_u16(out, id); out[2] = type; out[3] = flags; put_u32(out + 4, length);
    if (type == OIR_AST_VALUE_TOKEN_ID) put_u16(out + 8, 1u);
    return 8u + length;
}

int main(void) {
    uint8_t fields[256], payload[512];
    OirAstNodeRecord records[2];
    OirAstNodeTableView view;
    size_t root_len, child_len, written;
    uint16_t kind, n, f;
    uint8_t type;

    for (kind = 1; kind <= 99; ++kind) {
        root_len = emit_field(fields, 1, OIR_AST_VALUE_NODE_REF, 0);
        child_len = 0;
        n = count_for_kind(kind);
        assert(n != 0xffffu);
        for (f = 1; f <= n; ++f) {
            type = type_for(kind, f);
            child_len += emit_field(fields + root_len + child_len, f, type, 0);
        }
        memset(records, 0, sizeof(records));
        records[0].node_id=1; records[0].node_kind_id=1;
        records[0].field_data_offset=0; records[0].field_data_length=(uint32_t)root_len;
        records[1].node_id=2; records[1].node_kind_id=kind;
        records[1].field_data_offset=(uint32_t)root_len;
        records[1].field_data_length=(uint32_t)child_len;
        assert(oir_ast_node_table_write(payload,sizeof(payload),records,2,1,
            fields, (uint32_t)(root_len+child_len), NULL, 0, &written)==OIR_STATUS_OK);
        assert(oir_ast_node_table_open(payload,written,&view)==OIR_STATUS_OK);
        assert(oir_ast_field_schema_validate(&view)==OIR_STATUS_OK);
    }

    /* Missing required field, wrong known type, unknown required and optional fields. */
    root_len = emit_field(fields,1,OIR_AST_VALUE_NODE_REF,0);
    child_len = emit_field(fields+root_len,1,OIR_AST_VALUE_NULL,0);
    memset(records,0,sizeof(records));
    records[0].node_id=1; records[0].node_kind_id=1;
    records[0].field_data_offset=0; records[0].field_data_length=(uint32_t)root_len;
    records[1].node_id=2; records[1].node_kind_id=14;
    records[1].field_data_offset=(uint32_t)root_len;
    records[1].field_data_length=(uint32_t)child_len;
    assert(oir_ast_node_table_write(payload,sizeof(payload),records,2,1,fields,
        (uint32_t)(root_len+child_len),NULL,0,&written)==OIR_STATUS_OK);
    assert(oir_ast_node_table_open(payload,written,&view)==OIR_STATUS_OK);
    assert(oir_ast_field_schema_validate(&view)==OIR_STATUS_OK);

    child_len = 0; /* Missing required identifier.name. */
    records[1].field_data_length=0;
    assert(oir_ast_node_table_write(payload,sizeof(payload),records,2,1,fields,
        (uint32_t)root_len,NULL,0,&written)==OIR_STATUS_OK);
    assert(oir_ast_node_table_open(payload,written,&view)==OIR_STATUS_OK);
    assert(oir_ast_field_schema_validate(&view)==OIR_STATUS_INVALID_SECTION);

    child_len = emit_field(fields+root_len,1,OIR_AST_VALUE_NODE_REF,0);
    records[1].field_data_length=(uint32_t)child_len;
    assert(oir_ast_node_table_write(payload,sizeof(payload),records,2,1,fields,
        (uint32_t)(root_len+child_len),NULL,0,&written)==OIR_STATUS_OK);
    assert(oir_ast_node_table_open(payload,written,&view)==OIR_STATUS_OK);
    assert(oir_ast_field_schema_validate(&view)==OIR_STATUS_INVALID_SECTION);

    child_len = emit_field(fields+root_len,2,OIR_AST_VALUE_NULL,0);
    records[1].field_data_length=(uint32_t)child_len;
    assert(oir_ast_node_table_write(payload,sizeof(payload),records,2,1,fields,
        (uint32_t)(root_len+child_len),NULL,0,&written)==OIR_STATUS_OK);
    assert(oir_ast_node_table_open(payload,written,&view)==OIR_STATUS_OK);
    assert(oir_ast_field_schema_validate(&view)==OIR_STATUS_INVALID_SECTION);

    child_len = emit_field(fields+root_len,1,OIR_AST_VALUE_NULL,0);
    child_len += emit_field(fields+root_len+child_len,2,OIR_AST_VALUE_NULL,0);
    records[1].field_data_length=(uint32_t)child_len;
    assert(oir_ast_node_table_write(payload,sizeof(payload),records,2,1,fields,
        (uint32_t)(root_len+child_len),NULL,0,&written)==OIR_STATUS_OK);
    assert(oir_ast_node_table_open(payload,written,&view)==OIR_STATUS_OK);
    assert(oir_ast_field_schema_validate(&view)==OIR_STATUS_INVALID_SECTION);

    child_len = emit_field(fields+root_len,1,OIR_AST_VALUE_NULL,0);
    child_len += emit_field(fields+root_len+child_len,2,OIR_AST_VALUE_NULL,OIR_AST_TLV_OPTIONAL);
    records[1].field_data_length=(uint32_t)child_len;
    assert(oir_ast_node_table_write(payload,sizeof(payload),records,2,1,fields,
        (uint32_t)(root_len+child_len),NULL,0,&written)==OIR_STATUS_OK);
    assert(oir_ast_node_table_open(payload,written,&view)==OIR_STATUS_OK);
    assert(oir_ast_field_schema_validate(&view)==OIR_STATUS_OK);

    puts("OIR AST field schema tests passed.");
    return 0;
}
