#ifdef NDEBUG
#undef NDEBUG
#endif
#include "oir_ast_frontend_index.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define INDEX_CAPACITY 2048u

int main(void) {
    OirAstPointerIndexEntry entries[INDEX_CAPACITY];
    uint32_t count = 0u;
    ASTProgram root;
    ASTVarDecl first, second;
    ASTIdentifier init;
    ASTProgram cycle_root;
    ASTVarDecl shared;
    ASTIdentifier shared_child;
    ASTUnpack chain[OIR_AST_NODE_DEPTH_MAX + 1u];
    uint32_t i;

    memset(&root,0,sizeof(root));
    memset(&first,0,sizeof(first));
    memset(&second,0,sizeof(second));
    memset(&init,0,sizeof(init));
    root.base.type=AST_PROGRAM;
    first.base.type=AST_VAR_DECL;
    second.base.type=AST_VAR_DECL;
    init.base.type=AST_IDENTIFIER;
    root.declarations=(ASTNode*)&first;
    first.initializer=(ASTNode*)&init;
    first.base.next=(ASTNode*)&second;

    assert(oir_ast_frontend_index_build((ASTNode*)&root,entries,INDEX_CAPACITY,&count)==OIR_STATUS_OK);
    assert(count==4u);
    assert(oir_ast_frontend_index_lookup(entries,INDEX_CAPACITY,(ASTNode*)&root)==1u);
    assert(oir_ast_frontend_index_lookup(entries,INDEX_CAPACITY,(ASTNode*)&first)==2u);
    assert(oir_ast_frontend_index_lookup(entries,INDEX_CAPACITY,(ASTNode*)&init)==3u);
    assert(oir_ast_frontend_index_lookup(entries,INDEX_CAPACITY,(ASTNode*)&second)==4u);

    memset(&cycle_root,0,sizeof(cycle_root));
    cycle_root.base.type=AST_PROGRAM;
    cycle_root.declarations=(ASTNode*)&cycle_root;
    assert(oir_ast_frontend_index_build((ASTNode*)&cycle_root,entries,INDEX_CAPACITY,&count)==OIR_STATUS_INVALID_SECTION);
    assert(count==0u);

    memset(&shared,0,sizeof(shared));
    memset(&shared_child,0,sizeof(shared_child));
    shared.base.type=AST_VAR_DECL;
    shared_child.base.type=AST_IDENTIFIER;
    shared.initializer=(ASTNode*)&shared_child;
    shared.inline_type=(ASTNode*)&shared_child;
    memset(&root,0,sizeof(root));
    root.base.type=AST_PROGRAM;
    root.declarations=(ASTNode*)&shared;
    assert(oir_ast_frontend_index_build((ASTNode*)&root,entries,INDEX_CAPACITY,&count)==OIR_STATUS_INVALID_SECTION);

    memset(chain,0,sizeof(chain));
    memset(&root,0,sizeof(root));
    root.base.type=AST_PROGRAM;
    root.declarations=(ASTNode*)&chain[0];
    for(i=0;i<OIR_AST_NODE_DEPTH_MAX+1u;i++) {
        chain[i].base.type=AST_UNPACK;
        if(i+1u<OIR_AST_NODE_DEPTH_MAX+1u) chain[i].expr=(ASTNode*)&chain[i+1u];
    }
    assert(oir_ast_frontend_index_build((ASTNode*)&root,entries,INDEX_CAPACITY,&count)==OIR_STATUS_INVALID_SECTION);

    chain[OIR_AST_NODE_DEPTH_MAX-2u].expr=NULL;
    assert(oir_ast_frontend_index_build((ASTNode*)&root,entries,INDEX_CAPACITY,&count)==OIR_STATUS_OK);
    assert(count==OIR_AST_NODE_DEPTH_MAX);

    puts("OIR AST frontend pointer index tests passed.");
    return 0;
}
