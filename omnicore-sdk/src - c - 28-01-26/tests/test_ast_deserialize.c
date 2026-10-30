#include "ast.h"

#include <stdio.h>
#include <stddef.h>

static int rejects_bytes(const void* bytes, size_t length) {
    FILE* stream = tmpfile();
    if (!stream) return 2;

    if (fwrite(bytes, 1, length, stream) != length) {
        fclose(stream);
        return 2;
    }

    rewind(stream);
    ASTNode* root = ast_deserialize(stream);
    fclose(stream);

    if (root) {
        ast_destroy(root);
        return 1;
    }
    return 0;
}

int main(void) {
    struct {
        ASTNodeType type;
        int line;
    } truncated_header = { AST_PROGRAM, 1 };

    if (rejects_bytes(&truncated_header, sizeof(truncated_header)) != 0) {
        return 1;
    }

    struct {
        ASTNodeType type;
        int line;
        int col;
    } invalid_node = { (ASTNodeType)(AST_ZONE_LOCK + 1), 1, 1 };

    if (rejects_bytes(&invalid_node, sizeof(invalid_node)) != 0) {
        return 2;
    }

    struct {
        ASTNodeType type;
        int line;
        int col;
        int invalid_string_length;
    } invalid_string = { AST_IDENTIFIER, 1, 1, -2 };

    if (rejects_bytes(&invalid_string, sizeof(invalid_string)) != 0) {
        return 3;
    }

    return 0;
}
