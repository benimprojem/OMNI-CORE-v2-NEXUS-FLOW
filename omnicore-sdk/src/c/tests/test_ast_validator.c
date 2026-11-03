#include "ast.h"
#include <stdio.h>

static int expect(ASTNode *root, ASTValidationCode code) {
    ASTValidationError error;
    int valid=ast_validate(root,&error);
    return (code==AST_VALIDATE_OK) ? valid && error.code==AST_VALIDATE_OK
        : !valid && error.code==code;
}
int main(void) {
    ASTNode *root=ast_create(AST_PROGRAM),*block=ast_create(AST_BLOCK);
    ASTNode *id=ast_create(AST_IDENTIFIER);
    if(!root||!block||!id)return 1;
    ((ASTProgram*)root)->declarations=block;
    ((ASTBlock*)block)->statements=id;
    if(!expect(root,AST_VALIDATE_OK)){fprintf(stderr,"valid tree rejected\n");return 1;}
    /* A child points back to its ancestor: graph cycle. */
    ((ASTBlock*)block)->statements=root;
    if(!expect(root,AST_VALIDATE_SHARED_NODE)){fprintf(stderr,"cycle not detected\n");return 1;}
    ((ASTBlock*)block)->statements=id;
    /* Same node attached through two child fields (shared ownership). */
    ((ASTVarDecl*)id)->initializer=id; /* intentionally malformed node layout avoided below */
    /* duplicate sibling pointer via self-next */
    id->next=id;
    if(!expect(root,AST_VALIDATE_SHARED_NODE)){fprintf(stderr,"sibling cycle not detected\n");return 1;}
    id->next=NULL;
    ast_destroy(root);
    /* Required child field is checked independently of ownership. */
    root=ast_create(AST_BINARY_EXPR);
    if(!root||!expect(root,AST_VALIDATE_MISSING_REQUIRED)){fprintf(stderr,"missing operand not detected\n");return 1;}
    ast_destroy(root);
    if(ast_validate(NULL,NULL)){fprintf(stderr,"NULL root accepted\n");return 1;}
    return 0;
}
