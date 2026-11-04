#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expect(ASTNode *root, ASTValidationCode code) {
    ASTValidationError error;
    int valid = ast_validate(root, &error);
    return code == AST_VALIDATE_OK
        ? valid && error.code == AST_VALIDATE_OK
        : !valid && error.code == code;
}

int main(void) {
    /* Source-span API preserves the start-position compatibility mirrors. */
    {
        ASTNode *span_node = ast_create(AST_IDENTIFIER);
        OccSourceSpan span = {0};
        if (!span_node) return 1;
        span.source_id = 7;
        span.start_offset = 12;
        span.end_offset = 19;
        span.start_line = 3;
        span.start_column = 5;
        span.end_line = 3;
        span.end_column = 12;
        span.has_location = 1;
        if (!ast_set_source_span(span_node, &span) ||
            span_node->line != 3 || span_node->col != 5 ||
            memcmp(&span_node->source_span, &span, sizeof(span)) != 0) {
            fprintf(stderr, "source span assignment failed\\n");
            ast_destroy(span_node); return 1;
        }
        span.end_offset = 11;
        if (ast_set_source_span(span_node, &span)) {
            fprintf(stderr, "invalid source span accepted\\n");
            ast_destroy(span_node); return 1;
        }
        if (!ast_set_source_span(span_node, NULL) ||
            span_node->source_span.has_location || span_node->line || span_node->col) {
            fprintf(stderr, "source span clear failed\\n");
            ast_destroy(span_node); return 1;
        }
        ast_destroy(span_node);
    }
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
        ast_destroy(second);
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


    /* Complete source spans are validated as part of the public AST contract. */
    {
        ASTIdentifier* identifier = (ASTIdentifier*)ast_create(AST_IDENTIFIER);
        OccSourceSpan span;
        if (!identifier) return 1;
        memset(&span, 0, sizeof(span));
        span.source_id = 7u;
        span.start_offset = 4u;
        span.end_offset = 9u;
        span.start_line = 2u;
        span.start_column = 3u;
        span.end_line = 2u;
        span.end_column = 8u;
        span.has_location = 1u;
        identifier->name = (char*)malloc(2u);
        if (!identifier->name) { ast_destroy((ASTNode*)identifier); return 1; }
        strcpy(identifier->name, "x");
        if (!ast_set_source_span((ASTNode*)identifier, &span) ||
            !expect((ASTNode*)identifier, AST_VALIDATE_OK)) {
            fprintf(stderr, "valid source span rejected\\n");
            ast_destroy((ASTNode*)identifier);
            return 1;
        }
        identifier->base.source_span.end_offset = 3u;
        if (!expect((ASTNode*)identifier, AST_VALIDATE_INVALID_SPAN)) {
            fprintf(stderr, "reversed source span accepted\\n");
            ast_destroy((ASTNode*)identifier);
            return 1;
        }
        identifier->base.source_span = span;
        identifier->base.line = 99;
        if (!expect((ASTNode*)identifier, AST_VALIDATE_INVALID_SPAN)) {
            fprintf(stderr, "source span line mirror mismatch accepted\\n");
            ast_destroy((ASTNode*)identifier);
            return 1;
        }
        identifier->base.line = 2;
        identifier->base.source_span.end_line = 1u;
        if (!expect((ASTNode*)identifier, AST_VALIDATE_INVALID_SPAN)) {
            fprintf(stderr, "backward source span line accepted\\n");
            ast_destroy((ASTNode*)identifier);
            return 1;
        }
        identifier->base.source_span = span;
        identifier->base.source_span.end_column = 2u;
        if (!expect((ASTNode*)identifier, AST_VALIDATE_INVALID_SPAN)) {
            fprintf(stderr, "backward same-line source span column accepted\\n");
            ast_destroy((ASTNode*)identifier);
            return 1;
        }
        identifier->base.source_span = span;
        identifier->base.line = 2;
        identifier->base.source_span.end_column = 0u;
        if (!expect((ASTNode*)identifier, AST_VALIDATE_INVALID_SPAN)) {
            fprintf(stderr, "zero source span end column accepted\\n");
            ast_destroy((ASTNode*)identifier);
            return 1;
        }
        ast_destroy((ASTNode*)identifier);
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
    /* Appended variadic and named-argument nodes enforce their context. */
    {
        ASTFuncDecl* fn = (ASTFuncDecl*)ast_create(AST_FUNC_DECL);
        ASTNode* marker = ast_create(AST_VARIADIC_PARAM);
        ASTNode* body = ast_create(AST_BLOCK);
        if (!fn || !marker || !body) return 1;
        fn->params = marker;
        fn->body = body;
        if (!expect((ASTNode*)fn, AST_VALIDATE_OK)) {
            fprintf(stderr, "final variadic parameter marker rejected\\n");
            ast_destroy((ASTNode*)fn);
            return 1;
        }
        marker->next = ast_create(AST_VARIADIC_PARAM);
        if (!marker->next || !expect((ASTNode*)fn, AST_VALIDATE_INVALID_KIND)) {
            fprintf(stderr, "non-final variadic parameter marker accepted\\n");
            ast_destroy((ASTNode*)fn);
            return 1;
        }
        ast_destroy((ASTNode*)fn);
    }
    {
        ASTCall* call = (ASTCall*)ast_create(AST_FUNC_CALL);
        ASTIdentifier* callee = (ASTIdentifier*)ast_create(AST_IDENTIFIER);
        ASTNamedArg* first = (ASTNamedArg*)ast_create(AST_NAMED_ARG);
        ASTNamedArg* second = (ASTNamedArg*)ast_create(AST_NAMED_ARG);
        ASTNode* positional = ast_create(AST_LITERAL);
        if (!call || !callee || !first || !second || !positional) return 1;
        callee->name = (char*)malloc(2u);
        first->name = (char*)malloc(2u);
        second->name = (char*)malloc(2u);
        first->value = ast_create(AST_LITERAL);
        second->value = ast_create(AST_LITERAL);
        if (!callee->name || !first->name || !second->name ||
            !first->value || !second->value) return 1;
        strcpy(callee->name, "f");
        strcpy(first->name, "a");
        strcpy(second->name, "b");
        call->callee = (ASTNode*)callee;
        call->args = (ASTNode*)first;
        first->base.next = (ASTNode*)second;
        if (!expect((ASTNode*)call, AST_VALIDATE_OK)) {
            fprintf(stderr, "unique named arguments rejected\\n");
            ast_destroy((ASTNode*)call);
            ast_destroy(positional);
            return 1;
        }
        free(second->name);
        second->name = (char*)malloc(2u);
        if (!second->name) return 1;
        strcpy(second->name, "a");
        if (!expect((ASTNode*)call, AST_VALIDATE_INVALID_KIND)) {
            fprintf(stderr, "duplicate named argument accepted\\n");
            ast_destroy((ASTNode*)call);
            ast_destroy(positional);
            return 1;
        }
        free(second->name);
        second->name = (char*)malloc(2u);
        if (!second->name) return 1;
        strcpy(second->name, "b");
        second->base.next = positional;
        if (!expect((ASTNode*)call, AST_VALIDATE_INVALID_KIND)) {
            fprintf(stderr, "positional argument after named argument accepted\\n");
            ast_destroy((ASTNode*)call);
            return 1;
        }
        ast_destroy((ASTNode*)call);
    }

    /* MUST declarations are legal at program scope, not inside executable blocks. */
    {
        ASTProgram* program = (ASTProgram*)ast_create(AST_PROGRAM);
        ASTMustDecl* dependency = (ASTMustDecl*)ast_create(AST_MUST_DECL);
        ASTNode* literal = ast_create(AST_LITERAL);
        if (!program || !dependency || !literal) return 1;
        dependency->name = (char*)malloc(2u);
        if (!dependency->name) return 1;
        strcpy(dependency->name, "M");
        dependency->source = literal;
        program->declarations = (ASTNode*)dependency;
        if (!expect((ASTNode*)program, AST_VALIDATE_OK)) {
            fprintf(stderr, "top-level MUST declaration rejected\\n");
            ast_destroy((ASTNode*)program);
            return 1;
        }
        ast_destroy((ASTNode*)program);
    }
    {
        ASTFuncDecl* fn = (ASTFuncDecl*)ast_create(AST_FUNC_DECL);
        ASTBlock* body = (ASTBlock*)ast_create(AST_BLOCK);
        ASTMustDecl* dependency = (ASTMustDecl*)ast_create(AST_MUST_DECL);
        if (!fn || !body || !dependency) return 1;
        dependency->name = (char*)malloc(2u);
        dependency->source = ast_create(AST_LITERAL);
        if (!dependency->name || !dependency->source) return 1;
        strcpy(dependency->name, "M");
        body->statements = (ASTNode*)dependency;
        fn->body = (ASTNode*)body;
        if (!expect((ASTNode*)fn, AST_VALIDATE_INVALID_KIND)) {
            fprintf(stderr, "nested MUST declaration accepted\\n");
            ast_destroy((ASTNode*)fn);
            return 1;
        }
        ast_destroy((ASTNode*)fn);
    }

    /* Raw ASM labels are owned by FASTEXEC and must never float outside it. */
    {
        ASTFastExecBlock* fast=(ASTFastExecBlock*)ast_create(AST_FASTEXEC_BLOCK);
        ASTBlock* nested=(ASTBlock*)ast_create(AST_BLOCK);
        ASTAsmLabel* label=(ASTAsmLabel*)ast_create(AST_ASM_LABEL);
        if(!fast || !nested || !label) return 1;
        label->label=(char*)malloc(4u);
        label->source_text=(char*)malloc(strlen("mov rax, %value\\r\\nadd rax, 1")+1u);
        if(!label->label || !label->source_text) return 1;
        strcpy(label->label,"ADD");
        strcpy(label->source_text,"mov rax, %value\r\nadd rax, 1");
        label->source_length=strlen(label->source_text);
        nested->statements=(ASTNode*)label;
        fast->statements=(ASTNode*)nested;
        if(!expect((ASTNode*)fast,AST_VALIDATE_OK)) {
            fprintf(stderr,"FASTEXEC with raw ASM label rejected\\n");
            ast_destroy((ASTNode*)fast); return 1;
        }
        ast_destroy((ASTNode*)fast);
    }
    {
        ASTAsmLabel* label=(ASTAsmLabel*)ast_create(AST_ASM_LABEL);
        if(!label) return 1;
        label->label=(char*)malloc(4u);
        label->source_text=(char*)malloc(1u);
        if(!label->label || !label->source_text) return 1;
        strcpy(label->label,"ADD");
        label->source_text[0]='\\0';
        label->source_length=0u;
        if(!expect((ASTNode*)label,AST_VALIDATE_INVALID_KIND)) {
            fprintf(stderr,"ASM label outside FASTEXEC was accepted\\n");
            ast_destroy((ASTNode*)label); return 1;
        }
        ast_destroy((ASTNode*)label);
    }

    return 0;
}
