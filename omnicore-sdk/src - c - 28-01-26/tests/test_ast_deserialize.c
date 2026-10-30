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

static int roundtrips_special_nodes(void) {
    ASTProgram* program = (ASTProgram*)ast_create(AST_PROGRAM);
    ASTUnpack* unpack = (ASTUnpack*)ast_create(AST_UNPACK);
    ASTHalt* halt = (ASTHalt*)ast_create(AST_NEXUS_HALT);
    ASTLiteral* unpack_value = (ASTLiteral*)ast_create(AST_LITERAL);
    ASTLiteral* halt_value = (ASTLiteral*)ast_create(AST_LITERAL);

    if (!program || !unpack || !halt || !unpack_value || !halt_value) {
        ast_destroy((ASTNode*)program);
        ast_destroy((ASTNode*)unpack);
        ast_destroy((ASTNode*)halt);
        ast_destroy((ASTNode*)unpack_value);
        ast_destroy((ASTNode*)halt_value);
        return 1;
    }

    unpack_value->lit_type = 0;
    unpack_value->int_value = 123;
    halt_value->lit_type = 0;
    halt_value->int_value = 456;

    unpack->expr = (ASTNode*)unpack_value;
    halt->expr = (ASTNode*)halt_value;
    unpack->base.next = (ASTNode*)halt;
    program->declarations = (ASTNode*)unpack;

    FILE* stream = tmpfile();
    if (!stream) {
        ast_destroy((ASTNode*)program);
        return 1;
    }

    ast_serialize((ASTNode*)program, stream);
    rewind(stream);
    ASTProgram* decoded = (ASTProgram*)ast_deserialize(stream);
    fclose(stream);

    if (!decoded || !decoded->declarations ||
        decoded->declarations->type != AST_UNPACK ||
        !decoded->declarations->next ||
        decoded->declarations->next->type != AST_NEXUS_HALT) {
        ast_destroy((ASTNode*)decoded);
        ast_destroy((ASTNode*)program);
        return 1;
    }

    ASTUnpack* decoded_unpack = (ASTUnpack*)decoded->declarations;
    ASTHalt* decoded_halt = (ASTHalt*)decoded->declarations->next;
    int valid = decoded_unpack->expr &&
        decoded_unpack->expr->type == AST_LITERAL &&
        ((ASTLiteral*)decoded_unpack->expr)->int_value == 123 &&
        decoded_halt->expr &&
        decoded_halt->expr->type == AST_LITERAL &&
        ((ASTLiteral*)decoded_halt->expr)->int_value == 456;

    ast_destroy((ASTNode*)decoded);
    ast_destroy((ASTNode*)program);
    return valid ? 0 : 1;
}

int main(void) {
    if (roundtrips_special_nodes() != 0) return 4;

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
