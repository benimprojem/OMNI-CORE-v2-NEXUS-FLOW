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
    /* duplicate sibling pointer via self-next */
    id->next=id;
    if(!expect(root,AST_VALIDATE_SHARED_NODE)){fprintf(stderr,"sibling cycle not detected\n");return 1;}
    id->next=NULL;
    ast_destroy(root);
    { ASTNode *bin=ast_create(AST_BINARY_EXPR), *shared=ast_create(AST_IDENTIFIER);\n      if(!bin||!shared)return 1; ((ASTBinaryExpr*)bin)->left=shared; ((ASTBinaryExpr*)bin)->right=shared;\n      if(!expect(bin,AST_VALIDATE_SHARED_NODE)){fprintf(stderr,"shared ownership not detected\\n");return 1;}\n      /* Detach before destructor to avoid malformed-graph double free. */\n      ((ASTBinaryExpr*)bin)->right=NULL; ast_destroy(bin); }
    /* Required child field is checked independently of ownership. */
    root=ast_create(AST_BINARY_EXPR);
    if(!root||!expect(root,AST_VALIDATE_MISSING_REQUIRED)){fprintf(stderr,"missing operand not detected\n");return 1;}
    ast_destroy(root);
    if(ast_validate(NULL,NULL)){fprintf(stderr,"NULL root accepted\n");return 1;}
    return 0;
}
