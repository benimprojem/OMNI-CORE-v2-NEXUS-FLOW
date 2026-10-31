#include "oir_ast_graph.h"

#include <string.h>

static uint32_t read_u32_le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static OirStatus add_incoming(uint32_t* incoming, uint32_t count, uint32_t target) {
    if (target == 0u) return OIR_STATUS_OK;
    if (target > count || incoming[target - 1u] == UINT32_MAX) {
        return OIR_STATUS_INVALID_SECTION;
    }
    ++incoming[target - 1u];
    return OIR_STATUS_OK;
}

OirStatus oir_ast_graph_validate(
    const OirAstNodeTableView* view,
    uint32_t* incoming_counts,
    size_t incoming_capacity,
    uint8_t* visited,
    size_t visited_capacity,
    OirAstGraphStackEntry* stack,
    size_t stack_capacity
) {
    uint32_t i, visited_count = 0;
    size_t stack_size = 0;
    OirStatus status;

    if (!view || !view->node_records || view->node_count == 0u ||
        !incoming_counts || !visited || !stack ||
        incoming_capacity < view->node_count ||
        visited_capacity < view->node_count ||
        stack_capacity < view->node_count ||
        view->root_node_id != 1u) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }

    memset(incoming_counts, 0, (size_t)view->node_count * sizeof(*incoming_counts));
    memset(visited, 0, (size_t)view->node_count * sizeof(*visited));

    /* Count all owning child and sibling edges before traversing anything. */
    for (i = 0; i < view->node_count; ++i) {
        OirAstNodeRecord record;
        OirAstTlvReader reader;
        OirAstTlvField field;
        int has_field = 0;

        status = oir_ast_node_table_get(view, i, &record);
        if (status != OIR_STATUS_OK) return status;
        status = add_incoming(incoming_counts, view->node_count, record.next_node_id);
        if (status != OIR_STATUS_OK) return status;

        oir_ast_tlv_reader_init(&reader,
            view->field_data + record.field_data_offset,
            record.field_data_length);
        do {
            status = oir_ast_tlv_reader_next(&reader, &field, &has_field);
            if (status != OIR_STATUS_OK) return status;
            if (has_field && field.value_type == OIR_AST_VALUE_NODE_REF) {
                status = add_incoming(incoming_counts, view->node_count,
                    read_u32_le(field.value));
                if (status != OIR_STATUS_OK) return status;
            }
        } while (has_field);
    }

    if (incoming_counts[0] != 0u) return OIR_STATUS_INVALID_SECTION;
    for (i = 1; i < view->node_count; ++i) {
        if (incoming_counts[i] != 1u) return OIR_STATUS_INVALID_SECTION;
    }

    stack[stack_size].node_id = view->root_node_id;
    stack[stack_size].depth = 1u;
    ++stack_size;

    while (stack_size != 0u) {
        OirAstGraphStackEntry current = stack[--stack_size];
        OirAstNodeRecord record;
        OirAstTlvReader reader;
        OirAstTlvField field;
        int has_field = 0;

        if (current.node_id == 0u || current.node_id > view->node_count ||
            current.depth > OIR_AST_NODE_DEPTH_MAX) {
            return OIR_STATUS_INVALID_SECTION;
        }
        if (visited[current.node_id - 1u]) return OIR_STATUS_INVALID_SECTION;
        visited[current.node_id - 1u] = 1u;
        ++visited_count;

        status = oir_ast_node_table_get(view, current.node_id - 1u, &record);
        if (status != OIR_STATUS_OK) return status;

        if (record.next_node_id != 0u) {
            if (stack_size >= stack_capacity) return OIR_STATUS_INVALID_SECTION;
            stack[stack_size].node_id = record.next_node_id;
            stack[stack_size].depth = current.depth;
            ++stack_size;
        }

        oir_ast_tlv_reader_init(&reader,
            view->field_data + record.field_data_offset,
            record.field_data_length);
        do {
            status = oir_ast_tlv_reader_next(&reader, &field, &has_field);
            if (status != OIR_STATUS_OK) return status;
            if (has_field && field.value_type == OIR_AST_VALUE_NODE_REF) {
                uint32_t child = read_u32_le(field.value);
                if (child != 0u) {
                    if (current.depth >= OIR_AST_NODE_DEPTH_MAX ||
                        stack_size >= stack_capacity) {
                        return OIR_STATUS_INVALID_SECTION;
                    }
                    stack[stack_size].node_id = child;
                    stack[stack_size].depth = current.depth + 1u;
                    ++stack_size;
                }
            }
        } while (has_field);
    }

    return visited_count == view->node_count ? OIR_STATUS_OK : OIR_STATUS_INVALID_SECTION;
}
