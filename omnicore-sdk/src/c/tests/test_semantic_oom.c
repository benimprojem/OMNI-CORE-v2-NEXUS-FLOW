#include "parser.h"
#include "semantic.h"

#include <stdio.h>
#include <stdint.h>

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

int main(int argc, char** argv) {
    int i;
    if (argc < 2) return 2;
    for (i = 1; i < argc; ++i) {
        if (!check_allocation_failures(argv[i])) return 1;
    }
    return 0;
}
