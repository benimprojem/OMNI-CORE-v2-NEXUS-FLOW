#ifdef NDEBUG
#undef NDEBUG
#endif
#include "oir_ast_artifact.h"
#include "diagnostics.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    ASTProgram root;
    ASTIdentifier identifier;
    OirAstPointerIndexEntry index[8];
    OirAstNodeRecord records[2];
    uint8_t fields[128],strings[32],payload[256],artifact[512],bad[512];
    ASTNode* decoded[2]={NULL,NULL};
    ASTNode* decoded_root=NULL;
    uint32_t incoming[2],count=0u;
    uint8_t visited[2];
    OirAstGraphStackEntry stack[2];
    OirAstFrontendRequirements req;
    OirArtifactView artifact_view;
    OirStatus status;
    size_t written=0u;
    size_t required;

    memset(&root,0,sizeof(root));
    memset(&identifier,0,sizeof(identifier));
    root.base.type=AST_PROGRAM; root.base.line=1; root.base.col=1;
    identifier.base.type=AST_IDENTIFIER; identifier.base.line=2; identifier.base.col=3;
    identifier.name="checkpoint";
    root.declarations=(ASTNode*)&identifier;

    assert(oir_ast_frontend_index_build((ASTNode*)&root,index,8u,&count)==OIR_STATUS_OK);
    assert(count==2u);
    assert(oir_ast_frontend_measure((ASTNode*)&root,index,8u,count,&req)==OIR_STATUS_OK);
    required=OIR_HEADER_SIZE+OIR_SECTION_HEADER_SIZE+req.payload_size;

    memset(records,0xa5,sizeof(records));
    memset(fields,0xa5,sizeof(fields));
    memset(strings,0xa5,sizeof(strings));
    memset(payload,0xa5,sizeof(payload));
    memset(artifact,0xa5,sizeof(artifact));
    assert(oir_ast_artifact_write_frontend_ast((ASTNode*)&root,index,8u,count,
        records,2u,fields,sizeof(fields),strings,sizeof(strings),payload,req.payload_size,
        artifact,required-1u,42u,&written,&req)==OIR_STATUS_BUFFER_TOO_SMALL);
    assert(written==0u && artifact[0]==0xa5u && payload[0]==0xa5u);
    assert(((const uint8_t*)records)[0]==0xa5u);

    status=oir_ast_artifact_write_frontend_ast((ASTNode*)&root,index,8u,count,
        records,2u,fields,sizeof(fields),strings,sizeof(strings),payload,sizeof(payload),
        artifact,sizeof(artifact),42u,&written,&req);
    assert(status==OIR_STATUS_OK);
    assert(written==required);
    assert(oir_artifact_open(artifact,written,&artifact_view)==OIR_STATUS_OK);
    assert(artifact_view.header.artifact_kind==OIR_KIND_AST);
    assert(artifact_view.header.payload_schema_major==1u);
    assert(artifact_view.header.payload_schema_minor==0u);
    assert(artifact_view.header.producer_stage_id==OCC_STAGE_PARSER);
    assert(artifact_view.header.validity==OIR_VALIDITY_VALID);
    assert(artifact_view.header.job_id==42u);

    assert(oir_ast_artifact_read_frontend_ast(artifact,written,decoded,2u,
        incoming,2u,visited,2u,stack,2u,&decoded_root)==OIR_STATUS_OK);
    assert(decoded_root && decoded_root->type==AST_PROGRAM);
    assert(strcmp(((ASTIdentifier*)((ASTProgram*)decoded_root)->declarations)->name,
        "checkpoint")==0);
    ast_destroy(decoded_root); decoded_root=NULL;

    /* Wrong artifact kind is rejected by the envelope adapter. */
    memcpy(bad,artifact,written);
    bad[12]=OIR_KIND_TOKEN_STREAM;
    assert(oir_ast_artifact_read_frontend_ast(bad,written,decoded,2u,
        incoming,2u,visited,2u,stack,2u,&decoded_root)==OIR_STATUS_INVALID_HEADER);
    assert(decoded_root==NULL);

    /* Corrupt inner root ownership while leaving the outer OIR envelope valid. */
    memcpy(bad,artifact,written);
    bad[OIR_HEADER_SIZE+OIR_SECTION_HEADER_SIZE+
        OIR_AST_PAYLOAD_HEADER_SIZE+16u]=2u;
    assert(oir_ast_artifact_read_frontend_ast(bad,written,decoded,2u,
        incoming,2u,visited,2u,stack,2u,&decoded_root)==OIR_STATUS_INVALID_SECTION);
    assert(decoded_root==NULL);

    puts("OIR AST artifact envelope tests passed.");
    return 0;
}
