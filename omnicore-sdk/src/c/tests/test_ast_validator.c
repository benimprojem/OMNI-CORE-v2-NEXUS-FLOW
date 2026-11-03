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
    if (!expect(root, AST_VALIDATE_CYCLE)) {
        fprintf(stderr, "child cycle not detected\n"); return 1;
    }
    ((ASTBlock *)block)->statements = id;
    id->next = id;
    if (!expect(root, AST_VALIDATE_CYCLE)) {
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

    /* Payload-free and scalar-only leaf nodes are valid without child edges. */
    {
        const ASTNodeType leaves[] = { AST_LITERAL, AST_IDENTIFIER, AST_BREAK,
            AST_CONTINUE, AST_NEXUS_JUMP, AST_AI_LOGIC, AST_PANIC, AST_USE,
            AST_PATTERN_BINDING, AST_NEWTYPE_DECL };
        size_t i;
        for (i = 0; i < sizeof(leaves) / sizeof(leaves[0]); ++i) {
            ASTNode *leaf = ast_create(leaves[i]);
            if (!leaf || !expect(leaf, AST_VALIDATE_OK)) {
                fprintf(stderr, "leaf node rejected: %d\n", (int)leaves[i]);
                if (leaf) ast_destroy(leaf);
                return 1;
            }
            ast_destroy(leaf);
        }
    }

    /* Invalid enum values must fail before payload dispatch. */
    {
        ASTNode invalid = { (ASTNodeType)-1, 0, 0, NULL };
        ASTValidationError error;
        if (ast_validate(&invalid, &error) ||
            error.code != AST_VALIDATE_INVALID_KIND) {
            fprintf(stderr, "invalid node kind accepted\n"); return 1;
        }
    }

    /* Depth guard prevents an adversarially deep child chain exhausting C stack. */
    {
        ASTNode *deep = ast_create(AST_UNARY_EXPR), *cursor = deep;
        unsigned i;
        if (!deep) return 1;
        for (i = 0; i < 513u; ++i) {
            ASTNode *child = ast_create(AST_UNARY_EXPR);
            if (!child) { ast_destroy(deep); return 1; }
            ((ASTUnaryExpr *)cursor)->operand = child;
            cursor = child;
        }
        {
            ASTValidationError error;
            if (ast_validate(deep, &error) ||
                error.code != AST_VALIDATE_DEPTH_LIMIT) {
                fprintf(stderr, "depth limit not enforced\n");
                ast_destroy(deep); return 1;
            }
        }
        ast_destroy(deep);
    }

    /* Syntax-level optional children and required function bodies. */
    {
        ASTNode *ret = ast_create(AST_RETURN);
        ASTNode *fn = ast_create(AST_FUNC_DECL);
        ASTNode *ext = ast_create(AST_EXTERN_DECL);
        ASTNode *arm = ast_create(AST_SELECT_ARM);
        ASTNode *arm_body = ast_create(AST_BREAK);
        if (!ret || !fn || !ext || !arm || !arm_body) return 1;
        if (!expect(ret, AST_VALIDATE_OK)) {
            fprintf(stderr, "void return rejected\n"); return 1;
        }
        if (!expect(fn, AST_VALIDATE_MISSING_REQUIRED)) {
            fprintf(stderr, "function without body accepted\n"); return 1;
        }
        if (!expect(ext, AST_VALIDATE_OK)) {
            fprintf(stderr, "bodyless extern rejected\n"); return 1;
        }
        ((ASTSelectArm *)arm)->is_default = 1;
        ((ASTSelectArm *)arm)->body = arm_body;
        if (!expect(arm, AST_VALIDATE_OK)) {
            fprintf(stderr, "select default arm rejected\n"); return 1;
        }
        ast_destroy(ret); ast_destroy(fn); ast_destroy(ext); ast_destroy(arm);
    }

    {
        ASTNode *scan_else = ast_create(AST_SCAN_ARM);
        ASTNode *scan_body = ast_create(AST_BREAK);
        ASTNode *scan_next = ast_create(AST_SCAN_ARM);
        if (!scan_else || !scan_body || !scan_next) return 1;
        ((ASTScanArm *)scan_else)->body = scan_body;
        if (!expect(scan_else, AST_VALIDATE_OK)) {
            fprintf(stderr, "final scan else arm rejected\n"); return 1;
        }
        scan_else->next = scan_next;
        if (!expect(scan_else, AST_VALIDATE_MISSING_REQUIRED)) {
            fprintf(stderr, "non-final scan else arm accepted\n"); return 1;
        }
        scan_else->next = NULL;
        ast_destroy(scan_else);
        ast_destroy(scan_next);
    }

    /* A parsed void-return function must retain a NULL return operand. */
    {
        const char source[] = "f:empty() { ret; }";
        ParserContext *parser = parser_init_source("validator-void-return.nxf",
            (const uint8_t *)source, sizeof(source) - 1u);
        if (!parser || parser_run(parser) != PARSER_STATUS_OK ||
            !ast_validate(parser->ast_root, NULL)) {
            fprintf(stderr, "void-return parser AST failed validation\n");
            if (parser) parser_destroy(parser);
            return 1;
        }
        parser_destroy(parser);
    }

    /* A block-cleanup marker cannot escape its owning block or precede siblings. */
    {
        ASTNode* invalid_root = ast_create(AST_BLOCK_HALT);
        ASTNode* block = ast_create(AST_BLOCK);
        ASTNode* first = ast_create(AST_BLOCK_HALT);
        ASTNode* second = ast_create(AST_BREAK);
        if (!invalid_root || !block || !first || !second) return 1;
        if (ast_validate(invalid_root, NULL)) {
            fprintf(stderr, "root-level block cleanup marker was accepted\\n");
            ast_destroy(invalid_root); ast_destroy(block); return 1;
        }
        ast_destroy(invalid_root);
        first->next = second;
        ((ASTBlock*)block)->statements = first;
        if (ast_validate(block, NULL)) {
            fprintf(stderr, "non-final block cleanup marker was accepted\\n");
            ast_destroy(block); return 1;
        }
        first->next = NULL;
        ast_destroy(block);
    }

    /* The block cleanup leaf survives the legacy AST stream codec. */
    {
        ASTNode* block = ast_create(AST_BLOCK);
        ASTNode* cleanup = ast_create(AST_BLOCK_HALT);
        FILE* stream = tmpfile();
        ASTNode* decoded;
        if (!block || !cleanup || !stream) return 1;
        ((ASTBlock*)block)->statements = cleanup;
        if (!ast_validate(block, NULL) || !ast_serialize(block, stream) ||
            fseek(stream, 0L, SEEK_SET) != 0) {
            ast_destroy(block); fclose(stream); return 1;
        }
        decoded = ast_deserialize(stream);
        if (!decoded || !ast_validate(decoded, NULL) ||
            !((ASTBlock*)decoded)->statements ||
            ((ASTBlock*)decoded)->statements->type != AST_BLOCK_HALT) {
            fprintf(stderr, "block cleanup marker legacy AST round-trip failed\\n");
            if (decoded) ast_destroy(decoded);
            ast_destroy(block); fclose(stream); return 1;
        }
        ast_destroy(decoded);
        ast_destroy(block);
        fclose(stream);
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

    /* Legacy stream serialization must enforce the same ownership contract. */
    {
        ASTNode *bin = ast_create(AST_BINARY_EXPR);
        ASTNode *shared = ast_create(AST_IDENTIFIER);
        FILE *stream = tmpfile();
        if (!bin || !shared || !stream) return 1;
        ((ASTBinaryExpr *)bin)->left = shared;
        ((ASTBinaryExpr *)bin)->right = shared;
        if (ast_serialize(bin, stream) || ftell(stream) != 0L) {
            fprintf(stderr, "serializer accepted shared ownership or wrote partial bytes\\n");
            ((ASTBinaryExpr *)bin)->right = NULL;
            ast_destroy(bin);
            fclose(stream);
            return 1;
        }
        ((ASTBinaryExpr *)bin)->right = NULL;
        ast_destroy(bin);
        fclose(stream);
    }

    /* Cyclic graphs are rejected before the serializer touches the stream. */
    {
        ASTNode *unary = ast_create(AST_UNARY_EXPR);
        FILE *stream = tmpfile();
        if (!unary || !stream) return 1;
        ((ASTUnaryExpr *)unary)->operand = unary;
        if (ast_serialize(unary, stream) || ftell(stream) != 0L) {
            fprintf(stderr, "serializer accepted a cyclic graph or wrote partial bytes\\n");
            ((ASTUnaryExpr *)unary)->operand = NULL;
            ast_destroy(unary);
            fclose(stream);
            return 1;
        }
        ((ASTUnaryExpr *)unary)->operand = NULL;
        ast_destroy(unary);
        fclose(stream);
    }

    /* A structurally invalid stream must not materialize as a public AST. */
    {
        FILE *stream = tmpfile();
        int type = (int)AST_FUNC_DECL, line = 1, col = 1;
        int name_length = 1, absent = -1, zero = 0;
        ASTNode *decoded;
        if (!stream) return 1;
        if (fwrite(&type, sizeof(type), 1u, stream) != 1u ||
            fwrite(&line, sizeof(line), 1u, stream) != 1u ||
            fwrite(&col, sizeof(col), 1u, stream) != 1u ||
            fwrite(&name_length, sizeof(name_length), 1u, stream) != 1u ||
            fwrite("f", 1u, 1u, stream) != 1u ||
            fwrite(&absent, sizeof(absent), 1u, stream) != 1u ||
            fwrite(&zero, sizeof(zero), 1u, stream) != 1u ||
            fwrite(&zero, sizeof(zero), 1u, stream) != 1u ||
            fwrite(&zero, sizeof(zero), 1u, stream) != 1u ||
            fwrite(&absent, sizeof(absent), 1u, stream) != 1u ||
            fwrite(&absent, sizeof(absent), 1u, stream) != 1u ||
            fwrite(&absent, sizeof(absent), 1u, stream) != 1u ||
            fflush(stream) != 0 || fseek(stream, 0L, SEEK_SET) != 0) {
            fclose(stream);
            return 1;
        }
        decoded = ast_deserialize(stream);
        if (decoded) {
            fprintf(stderr, "deserializer accepted function declaration without body\\n");
            ast_destroy(decoded);
            fclose(stream);
            return 1;
        }
        fclose(stream);
    }

    /* Valid ASTs survive the legacy stream codec and validate after decode. */
    {
        ASTNode *fn = ast_create(AST_FUNC_DECL);
        ASTNode *body = ast_create(AST_BLOCK);
        FILE *stream = tmpfile();
        ASTNode *decoded;
        if (!fn || !body || !stream) return 1;
        ((ASTFuncDecl *)fn)->body = body;
        if (!ast_serialize(fn, stream) || fseek(stream, 0L, SEEK_SET) != 0) {
            ast_destroy(fn); fclose(stream); return 1;
        }
        decoded = ast_deserialize(stream);
        if (!decoded || !ast_validate(decoded, NULL)) {
            fprintf(stderr, "valid AST legacy round-trip failed\\n");
            if (decoded) ast_destroy(decoded);
            ast_destroy(fn); fclose(stream); return 1;
        }
        ast_destroy(decoded);
        ast_destroy(fn);
        fclose(stream);
    }
    return 0;
}
