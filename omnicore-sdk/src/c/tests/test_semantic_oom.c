#include "parser.h"
#include "semantic.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int check_allocation_failures(const char* path) {
    ParserContext* parser = parser_init(path);
    SemanticContext baseline;
    uint32_t allocation_count;
    uint32_t fail_at;

    if (!parser) {
        fprintf(stderr, "Could not initialize parser for %s\n", path);
        return 0;
    }
    if (parser_run(parser) != 0 || !parser->ast_root) {
        fprintf(stderr, "Could not parse semantic OOM fixture %s\n", path);
        parser_destroy(parser);
        return 0;
    }

    semantic_init(&baseline, path, NULL);
    semantic_test_fail_allocation_at(&baseline, UINT32_MAX);
    semantic_check(&baseline, (ASTNode*)parser->ast_root);
    allocation_count = semantic_test_get_allocation_attempts(&baseline);
    semantic_destroy(&baseline);

    if (allocation_count == 0u) {
        fprintf(stderr, "No semantic allocations observed for %s\n", path);
        parser_destroy(parser);
        return 0;
    }

    for (fail_at = 1u; fail_at <= allocation_count; ++fail_at) {
        SemanticContext semantic;
        semantic_init(&semantic, path, NULL);
        semantic_test_fail_allocation_at(&semantic, fail_at);
        semantic_check(&semantic, (ASTNode*)parser->ast_root);
        if (semantic.result_status != SEMANTIC_RESULT_OUT_OF_MEMORY) {
            fprintf(stderr,
                "Allocation failure %u/%u was not propagated for %s (status=%d)\n",
                fail_at, allocation_count, path, (int)semantic.result_status);
            semantic_destroy(&semantic);
            parser_destroy(parser);
            return 0;
        }
        semantic_destroy(&semantic);
    }

    printf("Semantic allocation fault injection passed: %s (%u sites)\n",
        path, allocation_count);
    parser_destroy(parser);
    return 1;
}

static Slot* find_test_slot(SemanticContext* semantic, const char* name) {
    Slot* slot;
    for (slot = semantic ? semantic->head : NULL; slot; slot = slot->next) {
        if (slot->name && strcmp(slot->name, name) == 0) return slot;
    }
    return NULL;
}

static int check_mutable_reborrow_rebind_failure_is_atomic(void) {
    static const char* source =
        "v:owner!i32 = 1; v:spare!i32 = 4; "
        "&mut parent!i32 = &mut owner; "
        "&mut child!i32 = &mut spare; child = &mut *parent;";
    ParserContext* parser = parser_init_source(
        "mutable-reborrow-oom.nxf",
        (const uint8_t*)source, strlen(source));
    SemanticContext baseline;
    uint32_t allocation_count, fail_at;
    uint32_t preserved_failures = 0u;

    if (!parser || parser_run(parser) != 0 || !parser->ast_root) {
        fprintf(stderr, "Could not parse mutable reborrow OOM fixture.\n");
        if (parser) parser_destroy(parser);
        return 0;
    }

    semantic_init(&baseline, "mutable-reborrow-oom.nxf", NULL);
    semantic_test_fail_allocation_at(&baseline, UINT32_MAX);
    semantic_check(&baseline, (ASTNode*)parser->ast_root);
    allocation_count = semantic_test_get_allocation_attempts(&baseline);
    semantic_destroy(&baseline);

    for (fail_at = 1u; fail_at <= allocation_count; ++fail_at) {
        SemanticContext semantic;
        Slot* owner;
        Slot* spare;
        Slot* parent;
        Slot* child;
        Error* error;
        int saw_reborrow_allocation_error = 0;

        semantic_init(&semantic, "mutable-reborrow-oom.nxf", NULL);
        semantic_test_fail_allocation_at(&semantic, fail_at);
        semantic_check(&semantic, (ASTNode*)parser->ast_root);

        for (error = semantic.error_head; error; error = error->next) {
            if (error->message &&
                strstr(error->message,
                    "Unable to create mutable reborrow metadata") != NULL) {
                saw_reborrow_allocation_error = 1;
                break;
            }
        }
        owner = find_test_slot(&semantic, "owner");
        spare = find_test_slot(&semantic, "spare");
        parent = find_test_slot(&semantic, "parent");
        child = find_test_slot(&semantic, "child");

        if (saw_reborrow_allocation_error &&
            semantic.result_status == SEMANTIC_RESULT_OUT_OF_MEMORY &&
            owner && spare && parent && child &&
            parent->borrow_origins &&
            parent->borrow_origins->owner == owner &&
            parent->borrow_suspended_by_identity == 0u &&
            child->borrow_origins &&
            child->borrow_origins->owner == spare &&
            child->borrow_origins->parent_identity == 0u &&
            child->borrow_kind == 2u &&
            owner->mutable_borrow_active &&
            spare->mutable_borrow_active) {
            ++preserved_failures;
        }
        semantic_destroy(&semantic);
    }

    parser_destroy(parser);
    if (preserved_failures == 0u) {
        fprintf(stderr,
            "Mutable reborrow allocation failures did not prove atomic preservation of the previous child binding.\n");
        return 0;
    }
    printf("Mutable reborrow replacement remained atomic at %u injected failure sites.\n",
        preserved_failures);
    return 1;
}

int main(int argc, char** argv) {
    int i;
    if (argc < 2) return 2;
    for (i = 1; i < argc; ++i) {
        if (!check_allocation_failures(argv[i])) return 1;
    }
    if (!check_mutable_reborrow_rebind_failure_is_atomic()) return 1;
    return 0;
}
