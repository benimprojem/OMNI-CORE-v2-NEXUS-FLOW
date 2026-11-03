#include "ast.h"
#include <stdio.h>

static int expect(ASTNode *root, ASTValidationCode code) {
    ASTValidationError error;
    int valid = ast_validate(root, &error);
    return code == AST_VALIDATE_OK
        ? valid && error.code == AST_VALIDATE_OK
        : !valid && error.code == code;
}

int main(void) {
    ASTNode *root = ast_create(AST_PROGRAM);
    ASTNode *block = ast_create(AST_BLOCK);
    ASTNode *id = ast_create(AST_IDENTIFIER);
    if (!root || !block || !id) return 1;
    ((ASTProgram *)root)->declarations = block;
    ((ASTBlock *)block)->statements = id;
    if (!expect(root, AST_VALIDATE_OK)) {
        fprintf(stderr, "valid tree rejected\n"); return 1;
    }

    /* Child-to-ancestor edge and sibling self-loop must both be rejected. */
    ((ASTBlock *)block)->statements = root;
    if (!expect(root, AST_VALIDATE_SHARED_NODE)) {
        fprintf(stderr, "child cycle not detected\n"); return 1;
    }
    ((ASTBlock *)block)->statements = id;
    id->next = id;
    if (!expect(root, AST_VALIDATE_SHARED_NODE)) {
        fprintf(stderr, "sibling cycle not detected\n"); return 1;
    }
    id->next = NULL;
    ast_destroy(root);

    /* Two child slots cannot own the same node. */
    {
        ASTNode *bin = ast_create(AST_BINARY_EXPR);
        ASTNode *shared = ast_create(AST_IDENTIFIER);
        if (!bin || !shared) return 1;
        ((ASTBinaryExpr *)bin)->left = shared;
        ((ASTBinaryExpr *)bin)->right = shared;
        if (!expect(bin, AST_VALIDATE_SHARED_NODE)) {
            fprintf(stderr, "shared ownership not detected\n"); return 1;
        }
        ((ASTBinaryExpr *)bin)->right = NULL; /* safe cleanup of test fixture */
        ast_destroy(bin);
    }

    root = ast_create(AST_BINARY_EXPR);
    if (!root || !expect(root, AST_VALIDATE_MISSING_REQUIRED)) {
        fprintf(stderr, "missing required operand not detected\n"); return 1;
    }
    ast_destroy(root);
    if (ast_validate(NULL, NULL)) {
        fprintf(stderr, "NULL root accepted\n"); return 1;
    }

    /* Parser output is expected to satisfy the AST graph contract. */
    {
        const char source[] = "f:main() { v:x = 1 + 2; ret x; }";
        ParserContext *parser = parser_init_source("validator.nxf",
            (const uint8_t *)source, sizeof(source) - 1u);
        if (!parser || parser_run(parser) != PARSER_STATUS_OK ||
            !ast_validate(parser->ast_root, NULL)) {
            fprintf(stderr, "parser AST failed graph validation\n");
            if (parser) parser_destroy(parser);
            return 1;
        }
        parser_destroy(parser);
    }
    return 0;
}
