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

static int rejects_block_statement_context(ASTNodeType type) {
    ASTNode* block=ast_create(AST_BLOCK);
    ASTNode* child=ast_create(type);
    int rejected;
    if(!block || !child) { ast_destroy(block); ast_destroy(child); return 0; }
    ((ASTBlock*)block)->statements=child;
    rejected=expect(block,AST_VALIDATE_INVALID_KIND);
    ast_destroy(block);
    return rejected;
}

static int test_ast_allocation_failure_injection(void) {
    ASTNode* node;
    ASTBinaryExpr* binary;
    ASTIdentifier* left;
    ASTIdentifier* right;
    size_t fail_at;

    ast_test_fail_allocation_at(1u);
    node=ast_create(AST_IDENTIFIER);
    if(node || ast_test_get_allocation_count()!=1u) {
        ast_test_reset_allocation_failure();
        ast_destroy(node);
        return 0;
    }
    ast_test_reset_allocation_failure();

    binary=(ASTBinaryExpr*)ast_create(AST_BINARY_EXPR);
    left=(ASTIdentifier*)ast_create(AST_IDENTIFIER);
    right=(ASTIdentifier*)ast_create(AST_IDENTIFIER);
    if(!binary || !left || !right) {
        ast_destroy((ASTNode*)binary); ast_destroy((ASTNode*)left);
        ast_destroy((ASTNode*)right); return 0;
    }
    left->name=(char*)malloc(sizeof("left"));
    right->name=(char*)malloc(sizeof("right"));
    if(!left->name || !right->name) {
        ast_destroy((ASTNode*)binary); ast_destroy((ASTNode*)left);
        ast_destroy((ASTNode*)right); return 0;
    }
    strcpy(left->name,"left"); strcpy(right->name,"right");
    binary->left=(ASTNode*)left; binary->right=(ASTNode*)right;

    for(fail_at=1u;fail_at<=3u;++fail_at) {
        ASTValidationError error;
        int valid;
        ast_test_fail_allocation_at(fail_at);
        valid=ast_validate((ASTNode*)binary,&error);
        if(valid || error.code!=AST_VALIDATE_OUT_OF_MEMORY ||
           ast_test_get_allocation_count()!=fail_at) {
            ast_test_reset_allocation_failure();
            ast_destroy((ASTNode*)binary);
            return 0;
        }
        ast_test_reset_allocation_failure();
    }
    ast_destroy((ASTNode*)binary);
    return 1;
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
            fprintf(stderr, "invalid source span accepted\n");
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
    ((ASTIdentifier*)id)->name=(char*)malloc(3u);
    if(!((ASTIdentifier*)id)->name) return 1;
    strcpy(((ASTIdentifier*)id)->name,"id");
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
        ((ASTIdentifier*)shared)->name=(char*)malloc(7u);
        if(!((ASTIdentifier*)shared)->name) return 1;
        strcpy(((ASTIdentifier*)shared)->name,"shared");
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
        const ASTNodeType leaves[] = { AST_BREAK, AST_CONTINUE };
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

    /* Scalar payloads that identify source constructs are required. */
    {
        ASTIdentifier* identifier=(ASTIdentifier*)ast_create(AST_IDENTIFIER);
        ASTLiteral* literal=(ASTLiteral*)ast_create(AST_LITERAL);
        ASTNewTypeDecl* newtype=(ASTNewTypeDecl*)ast_create(AST_NEWTYPE_DECL);
        if(!identifier || !literal || !newtype) return 1;
        if(!expect((ASTNode*)identifier,AST_VALIDATE_MISSING_REQUIRED) ||
           !expect((ASTNode*)literal,AST_VALIDATE_MISSING_REQUIRED) ||
           !expect((ASTNode*)newtype,AST_VALIDATE_MISSING_REQUIRED)) {
            fprintf(stderr,"missing scalar AST payload was accepted\\n");
            ast_destroy((ASTNode*)identifier);
            ast_destroy((ASTNode*)literal);
            ast_destroy((ASTNode*)newtype);
            return 1;
        }
        identifier->name=(char*)malloc(2u);
        literal->string_value=(char*)malloc(1u);
        newtype->name=(char*)malloc(3u);
        newtype->target_type=(char*)malloc(4u);
        if(!identifier->name || !literal->string_value ||
           !newtype->name || !newtype->target_type) return 1;
        strcpy(identifier->name,"x");
        literal->string_value[0]='\0'; /* Empty string literal is valid. */
        strcpy(newtype->name,"Id");
        strcpy(newtype->target_type,"u32");
        if(!expect((ASTNode*)identifier,AST_VALIDATE_OK) ||
           !expect((ASTNode*)literal,AST_VALIDATE_OK) ||
           !expect((ASTNode*)newtype,AST_VALIDATE_OK)) {
            fprintf(stderr,"valid scalar AST payload was rejected\n");
            ast_destroy((ASTNode*)identifier);
            ast_destroy((ASTNode*)literal);
            ast_destroy((ASTNode*)newtype);
            return 1;
        }
        ast_destroy((ASTNode*)identifier);
        ast_destroy((ASTNode*)literal);
        ast_destroy((ASTNode*)newtype);
    }

    /* Struct literal graph ownership and required payload contracts. */
    {
        ASTStructLiteral* literal=(ASTStructLiteral*)ast_create(AST_STRUCT_LITERAL);
        ASTStructFieldInit* field=(ASTStructFieldInit*)ast_create(AST_STRUCT_FIELD_INIT);
        ASTLiteral* value=(ASTLiteral*)ast_create(AST_LITERAL);
        if(!literal || !field || !value) return 1;
        literal->type_name=(char*)malloc(sizeof("Point"));
        field->name=(char*)malloc(sizeof("x"));
        value->string_value=(char*)malloc(sizeof("1"));
        if(!literal->type_name || !field->name || !value->string_value) return 1;
        strcpy(literal->type_name,"Point");
        strcpy(field->name,"x");
        strcpy(value->string_value,"1");
        field->value=(ASTNode*)value;
        literal->fields=(ASTNode*)field;
        {
            ASTValidationError validation_error;
            if(!ast_validate((ASTNode*)literal,&validation_error)) {
                fprintf(stderr,"valid Struct literal graph rejected: code=%d field=%s node=%d\\n",
                    (int)validation_error.code,
                    validation_error.field ? validation_error.field : "(null)",
                    validation_error.node ? (int)validation_error.node->type : -1);
                ast_destroy((ASTNode*)literal); return 1;
            }
        }

        free(literal->type_name); literal->type_name=NULL;
        if(!expect((ASTNode*)literal,AST_VALIDATE_MISSING_REQUIRED)) {
            fprintf(stderr,"Struct literal without type name accepted\\n");
            ast_destroy((ASTNode*)literal); return 1;
        }
        literal->type_name=(char*)malloc(sizeof("Point"));
        if(!literal->type_name) return 1;
        strcpy(literal->type_name,"Point");

        free(field->name); field->name=NULL;
        if(!expect((ASTNode*)literal,AST_VALIDATE_MISSING_REQUIRED)) {
            fprintf(stderr,"Struct field without name accepted\\n");
            ast_destroy((ASTNode*)literal); return 1;
        }
        field->name=(char*)malloc(sizeof("x"));
        if(!field->name) return 1;
        strcpy(field->name,"x");

        field->value=NULL;
        if(!expect((ASTNode*)literal,AST_VALIDATE_MISSING_REQUIRED)) {
            fprintf(stderr,"Struct field without value accepted\\n");
            ast_destroy((ASTNode*)literal); return 1;
        }
        field->value=(ASTNode*)value;

        /* Empty initializer list is structurally legal; Semantic checks completeness. */
        literal->fields=NULL;
        if(!expect((ASTNode*)literal,AST_VALIDATE_OK)) {
            fprintf(stderr,"empty Struct initializer list rejected by AST validator\\n");
            ast_destroy((ASTNode*)literal); return 1;
        }
        literal->fields=(ASTNode*)field;
        ast_destroy((ASTNode*)literal);
    }

    /* Struct field initializers participate in graph cycle/ownership checks. */
    {
        ASTStructLiteral* literal=(ASTStructLiteral*)ast_create(AST_STRUCT_LITERAL);
        ASTStructFieldInit* field=(ASTStructFieldInit*)ast_create(AST_STRUCT_FIELD_INIT);
        if(!literal || !field) return 1;
        literal->type_name=(char*)malloc(sizeof("Node"));
        field->name=(char*)malloc(sizeof("next"));
        if(!literal->type_name || !field->name) return 1;
        strcpy(literal->type_name,"Node");
        strcpy(field->name,"next");
        literal->fields=(ASTNode*)field;
        field->value=(ASTNode*)literal;
        if(!expect((ASTNode*)literal,AST_VALIDATE_CYCLE)) {
            fprintf(stderr,"Struct literal child cycle not detected\\n");
            field->value=NULL; ast_destroy((ASTNode*)literal); return 1;
        }
        field->value=NULL;
        ast_destroy((ASTNode*)literal);
    }

    /* Named arguments are valid only in the argument-list edge of a call. */
    {
        ASTCall *call=(ASTCall*)ast_create(AST_FUNC_CALL);
        ASTIdentifier *callee=(ASTIdentifier*)ast_create(AST_IDENTIFIER);
        ASTNamedArg *named=(ASTNamedArg*)ast_create(AST_NAMED_ARG);
        ASTIdentifier *value=(ASTIdentifier*)ast_create(AST_IDENTIFIER);
        if(!call || !callee || !named || !value) return 1;
        callee->name=(char*)malloc(sizeof("target"));
        named->name=(char*)malloc(sizeof("item"));
        value->name=(char*)malloc(sizeof("payload"));
        if(!callee->name || !named->name || !value->name) return 1;
        strcpy(callee->name,"target");
        strcpy(named->name,"item");
        strcpy(value->name,"payload");
        named->value=(ASTNode*)value;
        call->callee=(ASTNode*)callee;
        call->args=(ASTNode*)named;
        if(!expect((ASTNode*)call,AST_VALIDATE_OK)) {
            fprintf(stderr,"named argument in call args was rejected\\n");
            ast_destroy((ASTNode*)call); return 1;
        }

        /* Same node shape under the callee edge is structurally illegal. */
        call->callee=(ASTNode*)named;
        call->args=NULL;
        if(!expect((ASTNode*)call,AST_VALIDATE_INVALID_KIND)) {
            fprintf(stderr,"named argument under call callee was accepted\\n");
            call->callee=(ASTNode*)callee;
            call->args=(ASTNode*)named;
            ast_destroy((ASTNode*)call); return 1;
        }
        call->callee=(ASTNode*)callee;
        call->args=(ASTNode*)named;
        ast_destroy((ASTNode*)call);
    }

    /* Context-bound tags cannot be moved into an unrelated statement list. */
    {
        const ASTNodeType invalid_statement_tags[] = {
            AST_ENUM_VARIANT, AST_SELECT_ARM, AST_SCAN_ARM, AST_STRUCT_FIELD_INIT, AST_METHOD,
            AST_ARRAY_DIMENSION, AST_VARIADIC_PARAM, AST_MUST_DECL,
            AST_NAMED_ARG, AST_ASM_LABEL
        };
        size_t i;
        for(i=0u;i<sizeof(invalid_statement_tags)/sizeof(invalid_statement_tags[0]);++i) {
            if(!rejects_block_statement_context(invalid_statement_tags[i])) {
                fprintf(stderr,"context-bound AST tag accepted in AST_BLOCK.statements: %d\\n",
                    (int)invalid_statement_tags[i]);
                return 1;
            }
        }
    }

    /* Variadic marker must occupy the params edge, not another function edge. */
    {
        ASTFuncDecl* fn=(ASTFuncDecl*)ast_create(AST_FUNC_DECL);
        ASTVariadicParam* variadic=(ASTVariadicParam*)ast_create(AST_VARIADIC_PARAM);
        if(!fn || !variadic) return 1;
        fn->name=(char*)malloc(sizeof("wrong_edge"));
        if(!fn->name) return 1;
        strcpy(fn->name,"wrong_edge");
        fn->body=(ASTNode*)variadic;
        if(!expect((ASTNode*)fn,AST_VALIDATE_INVALID_KIND)) {
            fprintf(stderr,"variadic marker accepted outside function params\\n");
            ast_destroy((ASTNode*)fn); return 1;
        }
        ast_destroy((ASTNode*)fn);
    }

    /* Block cleanup is not a generic expression/branch child. */
    {
        ASTIf* conditional=(ASTIf*)ast_create(AST_IF);
        ASTNode* condition=ast_create(AST_LITERAL);
        ASTNode* halt=ast_create(AST_BLOCK_HALT);
        if(!conditional || !condition || !halt) return 1;
        ((ASTLiteral*)condition)->string_value=(char*)malloc(2u);
        if(!((ASTLiteral*)condition)->string_value) return 1;
        strcpy(((ASTLiteral*)condition)->string_value,"1");
        conditional->condition=condition;
        conditional->then_branch=halt;
        if(!expect((ASTNode*)conditional,AST_VALIDATE_INVALID_KIND)) {
            fprintf(stderr,"block cleanup accepted as AST_IF.then_branch\\n");
            ast_destroy((ASTNode*)conditional); return 1;
        }
        ast_destroy((ASTNode*)conditional);
    }

    if(!test_ast_allocation_failure_injection()) {
        fprintf(stderr,"AST allocation failure injection checks failed\\n");
        return 1;
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
        ASTSelect *select = (ASTSelect*)ast_create(AST_SELECT);
        ASTSelectArm *arm = (ASTSelectArm*)ast_create(AST_SELECT_ARM);
        ASTNode *selector = ast_create(AST_IDENTIFIER);
        ASTNode *arm_body = ast_create(AST_BREAK);
        if (!ret || !fn || !ext || !select || !arm || !selector || !arm_body) return 1;
        if (!expect(ret, AST_VALIDATE_OK)) return 1;
        if (!expect(fn, AST_VALIDATE_MISSING_REQUIRED)) return 1;
        ((ASTFuncDecl*)ext)->name=(char*)malloc(4u);
        ((ASTIdentifier*)selector)->name=(char*)malloc(sizeof("choice"));
        if(!((ASTFuncDecl*)ext)->name || !((ASTIdentifier*)selector)->name) return 1;
        strcpy(((ASTFuncDecl*)ext)->name,"ext");
        strcpy(((ASTIdentifier*)selector)->name,"choice");
        if (!expect(ext, AST_VALIDATE_OK)) return 1;
        arm->is_default=1;
        arm->body=arm_body;
        select->selector=selector;
        select->arms=(ASTNode*)arm;
        if (!expect((ASTNode*)select, AST_VALIDATE_OK)) {
            fprintf(stderr, "select default arm rejected in legal arms context\n");
            ast_destroy(ret); ast_destroy(fn); ast_destroy(ext);
            ast_destroy((ASTNode*)select); return 1;
        }
        ast_destroy(ret); ast_destroy(fn); ast_destroy(ext);
        ast_destroy((ASTNode*)select);
    }

    /* A scan else arm is legal only as the final AST_SCAN.arms entry. */
    {
        ASTScan *scan=(ASTScan*)ast_create(AST_SCAN);
        ASTScanArm *scan_else=(ASTScanArm*)ast_create(AST_SCAN_ARM);
        ASTScanArm *scan_next=(ASTScanArm*)ast_create(AST_SCAN_ARM);
        ASTNode *subject=ast_create(AST_IDENTIFIER);
        ASTNode *scan_body=ast_create(AST_BREAK);
        ASTPatternBinding *next_pattern=(ASTPatternBinding*)ast_create(AST_PATTERN_BINDING);
        ASTNode *next_body=ast_create(AST_BREAK);
        if(!scan || !scan_else || !scan_next || !subject ||
           !scan_body || !next_pattern || !next_body) return 1;
        ((ASTIdentifier*)subject)->name=(char*)malloc(sizeof("value"));
        next_pattern->name=(char*)malloc(sizeof("item"));
        if(!((ASTIdentifier*)subject)->name || !next_pattern->name) return 1;
        strcpy(((ASTIdentifier*)subject)->name,"value");
        strcpy(next_pattern->name,"item");
        scan->subject=subject;
        scan->arms=(ASTNode*)scan_else;
        scan_else->body=scan_body;
        if(!expect((ASTNode*)scan,AST_VALIDATE_OK)) {
            fprintf(stderr,"final scan else arm rejected in legal arms context\n");
            ast_destroy((ASTNode*)scan); ast_destroy((ASTNode*)scan_next);
            ast_destroy((ASTNode*)next_pattern); ast_destroy(next_body); return 1;
        }
        scan_next->pattern=(ASTNode*)next_pattern;
        scan_next->body=next_body;
        scan_else->base.next=(ASTNode*)scan_next;
        if(!expect((ASTNode*)scan,AST_VALIDATE_MISSING_REQUIRED)) {
            fprintf(stderr,"non-final scan else arm accepted\n");
            scan_else->base.next=NULL;
            ast_destroy((ASTNode*)scan); ast_destroy((ASTNode*)scan_next); return 1;
        }
        scan_else->base.next=NULL;
        ast_destroy((ASTNode*)scan);
        ast_destroy((ASTNode*)scan_next);
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
        ((ASTFuncDecl *)fn)->name=(char*)malloc(3u);
        if(!((ASTFuncDecl *)fn)->name) return 1;
        strcpy(((ASTFuncDecl *)fn)->name,"fn");
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
        fn->name=(char*)malloc(3u);
        if(!fn->name) return 1;
        strcpy(fn->name,"fn");
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
        ((ASTLiteral*)first->value)->string_value=(char*)malloc(2u);
        ((ASTLiteral*)second->value)->string_value=(char*)malloc(2u);
        if(!((ASTLiteral*)first->value)->string_value ||
           !((ASTLiteral*)second->value)->string_value) return 1;
        strcpy(((ASTLiteral*)first->value)->string_value,"1");
        strcpy(((ASTLiteral*)second->value)->string_value,"2");
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
        ((ASTLiteral*)literal)->string_value=(char*)malloc(2u);
        if(!((ASTLiteral*)literal)->string_value) return 1;
        strcpy(((ASTLiteral*)literal)->string_value,"x");
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
        fn->name=(char*)malloc(3u);
        if(!fn->name) return 1;
        strcpy(fn->name,"fn");
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
        if(!fast || !nested || !label) { ast_destroy((ASTNode*)fast); ast_destroy((ASTNode*)nested); ast_destroy((ASTNode*)label); return 1; }
        label->label=(char*)malloc(4u);
        label->source_text=(char*)malloc(strlen("mov rax, %value\\r\\nadd rax, 1")+1u);
        if(!label->label || !label->source_text) { ast_destroy((ASTNode*)fast); ast_destroy((ASTNode*)nested); ast_destroy((ASTNode*)label); return 1; }
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
        if(!label->label || !label->source_text) { ast_destroy((ASTNode*)label); return 1; }
        strcpy(label->label,"ADD");
        label->source_text[0]='\0';
        label->source_length=0u;
        if(!expect((ASTNode*)label,AST_VALIDATE_INVALID_KIND)) {
            fprintf(stderr,"ASM label outside FASTEXEC was accepted\\n");
            ast_destroy((ASTNode*)label); return 1;
        }
        ast_destroy((ASTNode*)label);
    }

    return 0;
}
