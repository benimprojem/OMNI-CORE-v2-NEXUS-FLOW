#include "oir_ast_field_schema.h"

static uint16_t required_field_count(uint16_t kind) {
    switch (kind) {
        case 1: case 2: case 29: case 14: case 20: case 23:
        case 34: case 35: case 37: case 38: case 39: case 69:
            return (kind == 20) ? 2u : 1u;
        case 3: return 8u;
        case 4: case 12: return 7u;
        case 5: case 7: return 6u;
        case 6: case 8: case 9: case 11: case 19: case 33:
        case 30: case 36: case 41: case 71:
            return (kind == 19) ? 2u :
                ((kind == 30 || kind == 36 || kind == 41) ? 2u :
                ((kind == 71) ? 2u : 2u));
        case 10: return 4u;
        case 13: return 5u;
        case 15: case 18: case 31: case 32:
            return (kind == 31 || kind == 32) ? 6u : 3u;
        case 21: return 2u;
        case 16: case 17: case 26: return 2u;
        case 22: return 2u;
        case 24: return 3u;
        case 25: return 7u;
        case 27: case 28: return 0u;
        case 40: case 42: case 43: case 44: case 45: case 46:
        case 47: case 48: case 49: case 50: case 51: case 52:
        case 53: case 54: case 55: case 56: case 57: case 58:
        case 59: case 60: case 61: case 62: case 63: case 64:
        case 65: case 66: case 67: case 68: case 72: case 73:
        case 74: case 75: case 76: case 77: case 78: case 79:
        case 80:
            return 2u;
        case 70: return 4u;
        case 81: case 83: case 84: case 86: return 2u;
        case 82: return 3u;
        case 85: return 1u;
        case 87: case 93: return 3u;
        case 88: case 89: case 92: case 95: case 96: return 2u;
        case 94: return 3u;
        case 97: case 98: case 99: return 1u;
        case 100: return 0u;
        case 90: case 91: return 1u;
        default: return UINT16_MAX;
    }
}

static uint8_t expected_type(uint16_t kind, uint16_t field_id) {
    switch (kind) {
        case 1: case 2: case 29: case 35: case 38: case 69:
            return OIR_AST_VALUE_NODE_REF;
        case 3:
            if (field_id <= 2u) return OIR_AST_VALUE_STRING_REF;
            if (field_id <= 6u) return OIR_AST_VALUE_BOOL;
            return OIR_AST_VALUE_NODE_REF;
        case 4: case 12:
            if (field_id <= 2u) return OIR_AST_VALUE_STRING_REF;
            if (field_id <= 5u) return OIR_AST_VALUE_BOOL;
            return OIR_AST_VALUE_NODE_REF;
        case 5: case 7:
            if (field_id == 1u) return OIR_AST_VALUE_STRING_REF;
            if (field_id <= 5u) return OIR_AST_VALUE_BOOL;
            return OIR_AST_VALUE_NODE_REF;
        case 6: case 9: case 11:
            return field_id == 1u ? OIR_AST_VALUE_STRING_REF : OIR_AST_VALUE_NODE_REF;
        case 8: return OIR_AST_VALUE_STRING_REF;
        case 10:
            if (field_id == 1u) return OIR_AST_VALUE_STRING_REF;
            if (field_id <= 3u) return OIR_AST_VALUE_BOOL;
            return OIR_AST_VALUE_NODE_REF;
        case 13:
            switch (field_id) {
                case 1: return OIR_AST_VALUE_TOKEN_ID;
                case 2: return OIR_AST_VALUE_U32;
                case 3: return OIR_AST_VALUE_I64;
                case 4: return OIR_AST_VALUE_F64;
                default: return OIR_AST_VALUE_STRING_REF;
            }
        case 14: case 37: case 39: case 34:
            return OIR_AST_VALUE_STRING_REF;
        case 15: case 18:
            return field_id == 1u ? OIR_AST_VALUE_TOKEN_ID : OIR_AST_VALUE_NODE_REF;
        case 21: return OIR_AST_VALUE_NODE_REF;
        case 16: case 17: case 26:
            return field_id == 1u ? OIR_AST_VALUE_TOKEN_ID : OIR_AST_VALUE_NODE_REF;
        case 22:
            return OIR_AST_VALUE_NODE_REF;
        case 19: case 40: case 42: case 43: case 44: case 45: case 46:
        case 47: case 48: case 49: case 50: case 51: case 52: case 53:
        case 54: case 55: case 56: case 57: case 58: case 59: case 60:
        case 61: case 62: case 63: case 64: case 65: case 66: case 67:
        case 68: case 72: case 73: case 74: case 75: case 76: case 77:
        case 78: case 79: case 80:
            return OIR_AST_VALUE_NODE_REF;
        case 20:
            return field_id == 1u ? OIR_AST_VALUE_STRING_REF : OIR_AST_VALUE_NODE_REF;
        case 23: case 24:
            return OIR_AST_VALUE_NODE_REF;
        case 25:
            return field_id == 1u ? OIR_AST_VALUE_I32 : OIR_AST_VALUE_NODE_REF;
        case 30: case 41:
            return field_id == 1u ? OIR_AST_VALUE_STRING_REF : OIR_AST_VALUE_NODE_REF;
        case 31: case 32:
            if (field_id == 1u) return OIR_AST_VALUE_TOKEN_ID;
            if (field_id == 4u || field_id == 5u) return OIR_AST_VALUE_I32;
            return OIR_AST_VALUE_NODE_REF;
        case 33: case 36:
            return field_id == 1u ? OIR_AST_VALUE_STRING_REF : OIR_AST_VALUE_NODE_REF;
        case 70:
            return field_id == 1u ? OIR_AST_VALUE_STRING_REF : OIR_AST_VALUE_NODE_REF;
        case 71: return OIR_AST_VALUE_STRING_REF;
        case 81: case 83: case 84: return OIR_AST_VALUE_NODE_REF;
        case 82: if(field_id==3u) return OIR_AST_VALUE_BOOL; return OIR_AST_VALUE_NODE_REF;
        case 85: return OIR_AST_VALUE_STRING_REF;
        case 86: if(field_id==1u) return OIR_AST_VALUE_STRING_REF; return OIR_AST_VALUE_NODE_REF;
        case 87: if(field_id==1u) return OIR_AST_VALUE_STRING_REF; return OIR_AST_VALUE_NODE_REF;
        case 88: case 89: case 90: case 91: case 95: case 96: return OIR_AST_VALUE_NODE_REF;
        case 92: if(field_id==1u) return OIR_AST_VALUE_STRING_REF; return OIR_AST_VALUE_NODE_REF;
        case 93: if(field_id<=2u) return OIR_AST_VALUE_STRING_REF; return OIR_AST_VALUE_NODE_REF;
        case 94: if(field_id==3u) return OIR_AST_VALUE_BOOL; return OIR_AST_VALUE_NODE_REF;
        case 97: case 98: case 99: return OIR_AST_VALUE_NODE_REF;
        case 100: return OIR_AST_VALUE_NODE_REF;
        case 101: return OIR_AST_VALUE_I64;
        default: return UINT8_MAX;
    }
}

static int is_nullable_string(uint16_t kind, uint16_t field_id) {
    (void)field_id;
    switch (kind) {
        case 3: case 4: case 5: case 6: case 7: case 8: case 9:
        case 10: case 11: case 12: case 13: case 14: case 20:
        case 30: case 33: case 34: case 36: case 37: case 39:
        case 41: case 70: case 71: case 85: case 86: case 87: case 92: case 93:
            return 1;
        default:
            return 0;
    }
}

OirStatus oir_ast_field_schema_validate(const OirAstNodeTableView* view) {
    uint32_t i;
    if (!view || !view->node_records || view->node_count == 0u) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    for (i = 0; i < view->node_count; ++i) {
        OirAstNodeRecord record;
        OirAstTlvReader reader;
        OirAstTlvField field;
        uint16_t field_count, next_required = 1u;
        int has_field = 0;
        OirStatus status = oir_ast_node_table_get(view, i, &record);
        if (status != OIR_STATUS_OK) return status;
        field_count = required_field_count(record.node_kind_id);
        if (field_count == UINT16_MAX) return OIR_STATUS_INVALID_SECTION;
        oir_ast_tlv_reader_init(&reader,
            view->field_data + record.field_data_offset,
            record.field_data_length);
        do {
            status = oir_ast_tlv_reader_next(&reader, &field, &has_field);
            if (status != OIR_STATUS_OK) return status;
            if (!has_field) break;
            if (field.field_id <= field_count) {
                uint8_t expected;
                if (field.field_id != next_required) return OIR_STATUS_INVALID_SECTION;
                expected = expected_type(record.node_kind_id, field.field_id);
                if (expected == UINT8_MAX) return OIR_STATUS_INVALID_SECTION;
                if (field.value_type != expected &&
                    !(field.value_type == OIR_AST_VALUE_NULL &&
                      expected == OIR_AST_VALUE_STRING_REF &&
                      is_nullable_string(record.node_kind_id, field.field_id))) {
                    return OIR_STATUS_INVALID_SECTION;
                }
                ++next_required;
            } else if ((field.flags & OIR_AST_TLV_OPTIONAL) == 0u) {
                return OIR_STATUS_INVALID_SECTION;
            }
        } while (has_field);
        if (next_required != (uint16_t)(field_count + 1u)) {
            return OIR_STATUS_INVALID_SECTION;
        }
    }
    return OIR_STATUS_OK;
}
