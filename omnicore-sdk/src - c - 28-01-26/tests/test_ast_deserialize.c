#include "ast.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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

static char* duplicate_text(const char* text) {
    size_t length = strlen(text);
    char* copy = (char*)malloc(length + 1u);
    if (!copy) return NULL;
    memcpy(copy, text, length + 1u);
    return copy;
}

static int roundtrips_all_node_type_tags(void) {
    for (int raw_type = AST_PROGRAM; raw_type <= AST_ZONE_LOCK; ++raw_type) {
        ASTNode* original = ast_create((ASTNodeType)raw_type);
        if (!original) return 1;

        original->line = 100 + raw_type;
        original->col = 200 + raw_type;
        ASTIdentifier* sibling = (ASTIdentifier*)ast_create(AST_IDENTIFIER);
        if (!sibling) {
            ast_destroy(original);
            return 1;
        }
        sibling->name = duplicate_text("sibling-node");
        if (!sibling->name) {
            ast_destroy((ASTNode*)sibling);
            ast_destroy(original);
            return 1;
        }
        original->next = (ASTNode*)sibling;

        FILE* stream = tmpfile();
        if (!stream) {
            ast_destroy(original);
            return 1;
        }
        ast_serialize(original, stream);
        rewind(stream);
        ASTNode* decoded = ast_deserialize(stream);
        fclose(stream);

        int valid = decoded &&
            decoded->type == (ASTNodeType)raw_type &&
            decoded->line == 100 + raw_type &&
            decoded->col == 200 + raw_type &&
            decoded->next &&
            decoded->next->type == AST_IDENTIFIER &&
            ((ASTIdentifier*)decoded->next)->name &&
            strcmp(((ASTIdentifier*)decoded->next)->name, "sibling-node") == 0;

        ast_destroy(decoded);
        ast_destroy(original);
        if (!valid) {
            fprintf(stderr, "AST node type round-trip failed for tag %d.\n", raw_type);
            return 1;
        }
    }
    return 0;
}

static int roundtrips_extern_decl(void) {
    ASTFuncDecl* original = (ASTFuncDecl*)ast_create(AST_EXTERN_DECL);
    if (!original) return 1;

    original->name = duplicate_text("external_api");
    original->return_type = duplicate_text("i32");
    original->is_extern = 1;
    original->is_public = 1;
    original->is_exported = 0;
    original->params = ast_create(AST_IDENTIFIER);
    original->body = ast_create(AST_BLOCK);

    if (!original->name || !original->return_type ||
        !original->params || !original->body) {
        ast_destroy((ASTNode*)original);
        return 1;
    }

    ((ASTIdentifier*)original->params)->name = duplicate_text("argument");
    if (!((ASTIdentifier*)original->params)->name) {
        ast_destroy((ASTNode*)original);
        return 1;
    }

    FILE* stream = tmpfile();
    if (!stream) {
        ast_destroy((ASTNode*)original);
        return 1;
    }

    ast_serialize((ASTNode*)original, stream);
    rewind(stream);
    ASTFuncDecl* decoded = (ASTFuncDecl*)ast_deserialize(stream);
    fclose(stream);

    int valid = decoded &&
        decoded->base.type == AST_EXTERN_DECL &&
        decoded->name && strcmp(decoded->name, "external_api") == 0 &&
        decoded->return_type && strcmp(decoded->return_type, "i32") == 0 &&
        decoded->is_extern == 1 &&
        decoded->is_public == 1 &&
        decoded->is_exported == 0 &&
        decoded->params && decoded->params->type == AST_IDENTIFIER &&
        ((ASTIdentifier*)decoded->params)->name &&
        strcmp(((ASTIdentifier*)decoded->params)->name, "argument") == 0 &&
        decoded->body && decoded->body->type == AST_BLOCK;

    ast_destroy((ASTNode*)decoded);
    ast_destroy((ASTNode*)original);
    return valid ? 0 : 1;
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

    unpack_value->type = TOKEN_NUMBER;
    unpack_value->lit_type = 0;
    unpack_value->int_value = 123;
    unpack_value->float_value = 123.5;
    unpack_value->string_value = duplicate_text("123");
    halt_value->type = TOKEN_NUMBER;
    halt_value->lit_type = 0;
    halt_value->int_value = 456;
    halt_value->float_value = 456.5;
    halt_value->string_value = duplicate_text("456");
    if (!unpack_value->string_value || !halt_value->string_value) {
        ast_destroy((ASTNode*)program);
        ast_destroy((ASTNode*)unpack);
        ast_destroy((ASTNode*)halt);
        ast_destroy((ASTNode*)unpack_value);
        ast_destroy((ASTNode*)halt_value);
        return 1;
    }

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
        ((ASTLiteral*)decoded_unpack->expr)->type == TOKEN_NUMBER &&
        ((ASTLiteral*)decoded_unpack->expr)->int_value == 123 &&
        ((ASTLiteral*)decoded_unpack->expr)->float_value == 123.5 &&
        ((ASTLiteral*)decoded_unpack->expr)->string_value &&
        strcmp(((ASTLiteral*)decoded_unpack->expr)->string_value, "123") == 0 &&
        decoded_halt->expr &&
        decoded_halt->expr->type == AST_LITERAL &&
        ((ASTLiteral*)decoded_halt->expr)->type == TOKEN_NUMBER &&
        ((ASTLiteral*)decoded_halt->expr)->int_value == 456 &&
        ((ASTLiteral*)decoded_halt->expr)->float_value == 456.5 &&
        ((ASTLiteral*)decoded_halt->expr)->string_value &&
        strcmp(((ASTLiteral*)decoded_halt->expr)->string_value, "456") == 0;

    ast_destroy((ASTNode*)decoded);
    ast_destroy((ASTNode*)program);
    return valid ? 0 : 1;
}

static int roundtrips_large_sibling_list(void) {
    enum { NODE_COUNT = 700 };
    ASTProgram* program = (ASTProgram*)ast_create(AST_PROGRAM);
    if (!program) return 1;

    ASTNode* head = NULL;
    ASTNode* tail = NULL;
    for (int i = 0; i < NODE_COUNT; ++i) {
        ASTNode* node = ast_create(AST_BREAK);
        if (!node) {
            ast_destroy(head);
            ast_destroy((ASTNode*)program);
            return 1;
        }
        if (!head) head = node;
        else tail->next = node;
        tail = node;
    }
    program->declarations = head;

    FILE* stream = tmpfile();
    if (!stream) {
        ast_destroy((ASTNode*)program);
        return 1;
    }

    ast_serialize((ASTNode*)program, stream);
    rewind(stream);
    ASTProgram* decoded = (ASTProgram*)ast_deserialize(stream);
    fclose(stream);

    size_t count = 0;
    if (decoded) {
        ASTNode* node = decoded->declarations;
        while (node && count <= NODE_COUNT) {
            count++;
            node = node->next;
        }
    }

    int valid = decoded && count == NODE_COUNT;
    ast_destroy((ASTNode*)decoded);
    ast_destroy((ASTNode*)program);
    return valid ? 0 : 1;
}

static int parser_save_ir_rejects_invalid_path(const char* path) {
    ParserContext context;
    memset(&context, 0, sizeof(context));
    context.ast_root = ast_create(AST_PROGRAM);
    if (!context.ast_root) return 0;

    int saved = parser_save_ir(&context, path);
    ast_destroy((ASTNode*)context.ast_root);
    return saved == 0;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: test_ast_deserialize <invalid-output-path>\n");
        return 9;
    }
    if (roundtrips_all_node_type_tags() != 0) return 8;
    if (roundtrips_extern_decl() != 0) return 7;
    if (roundtrips_special_nodes() != 0) return 4;
    if (roundtrips_large_sibling_list() != 0) return 5;
    if (!parser_save_ir_rejects_invalid_path(argv[1])) return 10;

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

    struct {
        ASTNodeType type;
        int line;
        int col;
        int oversized_string_length;
    } oversized_string = { AST_IDENTIFIER, 1, 1, (16 * 1024 * 1024) + 1 };

    if (rejects_bytes(&oversized_string, sizeof(oversized_string)) != 0) {
        return 6;
    }

    return 0;
}
