#ifdef NDEBUG
#undef NDEBUG
#endif
#include "parser.h"
#include "ast.h"
#include "oir_ast_frontend_writer.h"
#include "oir_ast_frontend_reader.h"
#include "oir_ast_artifact.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static OirStatus encode_ast(ASTNode* root,uint8_t** out_payload,size_t* out_length) {
    size_t index_capacity=1024u;
    OirAstPointerIndexEntry* index=NULL;
    OirAstNodeRecord* records=NULL;
    uint8_t* fields=NULL;
    uint8_t* strings=NULL;
    uint8_t* payload=NULL;
    uint32_t node_count=0u;
    size_t written=0u;
    OirAstFrontendRequirements req;
    OirStatus status;

    *out_payload=NULL; *out_length=0u;
    for(;;) {
        free(index);
        index=(OirAstPointerIndexEntry*)calloc(index_capacity,sizeof(*index));
        if(!index) { status=OIR_STATUS_OUT_OF_MEMORY; goto done; }
        status=oir_ast_frontend_index_build(root,index,index_capacity,&node_count);
        if(status!=OIR_STATUS_BUFFER_TOO_SMALL) break;
        if(index_capacity>=2097152u) goto done;
        index_capacity*=2u;
    }
    if(status!=OIR_STATUS_OK) goto done;
    status=oir_ast_frontend_measure(root,index,index_capacity,node_count,&req);
    if(status!=OIR_STATUS_OK) goto done;

    records=(OirAstNodeRecord*)calloc(node_count,sizeof(*records));
    fields=req.field_data_length?(uint8_t*)malloc(req.field_data_length):NULL;
    strings=req.string_table_length?(uint8_t*)malloc(req.string_table_length):NULL;
    payload=(uint8_t*)malloc(req.payload_size);
    if(!records || (req.field_data_length && !fields) ||
        (req.string_table_length && !strings) || !payload) {
        status=OIR_STATUS_OUT_OF_MEMORY; goto done;
    }
    status=oir_ast_frontend_write_payload(root,index,index_capacity,node_count,
        records,node_count,fields,req.field_data_length,strings,req.string_table_length,
        payload,req.payload_size,&written,&req);
    if(status!=OIR_STATUS_OK) goto done;
    if(written!=req.payload_size) { status=OIR_STATUS_LENGTH_MISMATCH; goto done; }
    *out_payload=payload; *out_length=written; payload=NULL;

done:
    free(index); free(records); free(fields); free(strings); free(payload);
    return status;
}


static OirStatus encode_ast_artifact(
    ASTNode* root,uint64_t job_id,uint8_t** out_artifact,size_t* out_length
) {
    size_t index_capacity=1024u;
    OirAstPointerIndexEntry* index=NULL;
    OirAstNodeRecord* records=NULL;
    uint8_t* fields=NULL;
    uint8_t* strings=NULL;
    uint8_t* payload=NULL;
    uint8_t* artifact=NULL;
    uint32_t node_count=0u;
    size_t written=0u,artifact_capacity;
    OirAstFrontendRequirements req;
    OirStatus status;

    *out_artifact=NULL; *out_length=0u;
    for(;;) {
        free(index);
        index=(OirAstPointerIndexEntry*)calloc(index_capacity,sizeof(*index));
        if(!index) { status=OIR_STATUS_OUT_OF_MEMORY; goto done; }
        status=oir_ast_frontend_index_build(root,index,index_capacity,&node_count);
        if(status!=OIR_STATUS_BUFFER_TOO_SMALL) break;
        if(index_capacity>=2097152u) goto done;
        index_capacity*=2u;
    }
    if(status!=OIR_STATUS_OK) goto done;
    status=oir_ast_frontend_measure(root,index,index_capacity,node_count,&req);
    if(status!=OIR_STATUS_OK) goto done;
    if(req.payload_size>SIZE_MAX-OIR_HEADER_SIZE-OIR_SECTION_HEADER_SIZE) {
        status=OIR_STATUS_OVERFLOW; goto done;
    }
    artifact_capacity=OIR_HEADER_SIZE+OIR_SECTION_HEADER_SIZE+req.payload_size;
    records=(OirAstNodeRecord*)calloc(node_count,sizeof(*records));
    fields=req.field_data_length?(uint8_t*)malloc(req.field_data_length):NULL;
    strings=req.string_table_length?(uint8_t*)malloc(req.string_table_length):NULL;
    payload=(uint8_t*)malloc(req.payload_size);
    artifact=(uint8_t*)malloc(artifact_capacity);
    if(!records || (req.field_data_length && !fields) ||
        (req.string_table_length && !strings) || !payload || !artifact) {
        status=OIR_STATUS_OUT_OF_MEMORY; goto done;
    }
    status=oir_ast_artifact_write_frontend_ast(root,index,index_capacity,node_count,
        records,node_count,fields,req.field_data_length,strings,req.string_table_length,
        payload,req.payload_size,artifact,artifact_capacity,job_id,&written,&req);
    if(status!=OIR_STATUS_OK) goto done;
    if(written!=artifact_capacity) { status=OIR_STATUS_LENGTH_MISMATCH; goto done; }
    *out_artifact=artifact; *out_length=written; artifact=NULL;

done:
    free(index); free(records); free(fields); free(strings); free(payload); free(artifact);
    return status;
}

static int test_select_scan_oir_roundtrip(void) {
    static const char source[] =
        "f:take(a![], b!i32[], third:i32[4]) {}"
        "f:main() {"
        " v:dynamic[]; v:fixed[5]!i32; v:matrix[3][3]!f32;"
        " select(x) { 1 when x > 0 -> { } default -> { } }"
        " scan(x) { Range(s,e) when s == e && e == s -> { } item -> { } else -> { } }"
        " rules PrimeBorders(n) { n < 2 :=> \"out of range\"; n == 2 -> true; always -> true; default -> false; }"
        " apply rules(PrimeBorders(5));"
        " on PrimeBorders.Success { ret true; }"
        " v:callback = f:(x) > x;"
        " v:callback_block = f:(x) { ret x; };"
        " unroll(3) -> loop(item in data) { v:copy = item; }"
        " v:moved = source _> target;"
        " v:retry = source?(5, 500ms)->target;"
        "}";
    ParserContext* parser=parser_init_source("pattern-oir.nxf",
        (const uint8_t*)source,sizeof(source)-1u);
    uint8_t *first=NULL,*second=NULL;
    size_t first_len=0u,second_len=0u;
    ASTNode **storage=NULL,*decoded=NULL;
    uint32_t *incoming=NULL;
    uint8_t *visited=NULL;
    OirAstGraphStackEntry *stack=NULL;
    OirAstNodeTableView view;
    OirStatus status;
    int ok=0;
    if(!parser)return 0;
    if(parser_run(parser)!=PARSER_STATUS_OK || parser->error_count ||
        !parser->ast_root)goto done;
    status=encode_ast((ASTNode*)parser->ast_root,&first,&first_len);
    if(status!=OIR_STATUS_OK || oir_ast_node_table_open(first,first_len,&view)!=OIR_STATUS_OK)goto done;
    storage=(ASTNode**)calloc(view.node_count,sizeof(*storage));
    incoming=(uint32_t*)calloc(view.node_count,sizeof(*incoming));
    visited=(uint8_t*)calloc(view.node_count,sizeof(*visited));
    stack=(OirAstGraphStackEntry*)calloc(view.node_count,sizeof(*stack));
    if(!storage || !incoming || !visited || !stack)goto done;
    status=oir_ast_frontend_read_payload(first,first_len,storage,view.node_count,
        incoming,view.node_count,visited,view.node_count,stack,view.node_count,&decoded);
    if(status!=OIR_STATUS_OK || !decoded)goto done;
    status=encode_ast(decoded,&second,&second_len);
    if(status!=OIR_STATUS_OK || first_len!=second_len ||
        memcmp(first,second,first_len)!=0)goto done;
    ok=1;
done:
    if(decoded)ast_destroy(decoded);
    free(first); free(second); free(storage); free(incoming); free(visited); free(stack);
    parser_destroy(parser);
    return ok;
}

int main(int argc,char** argv) {
    if(!test_select_scan_oir_roundtrip()) { fprintf(stderr,"Extended AST OIR round-trip failed.\\n"); return 1; }
    ParserContext* parser;
    uint8_t* first_payload=NULL;
    uint8_t* second_payload=NULL;
    uint8_t* ast_artifact=NULL;
    size_t first_length=0u,second_length=0u,artifact_length=0u;
    ASTNode* decoded_root=NULL;
    ASTNode* artifact_root=NULL;
    ASTNode** node_storage=NULL;
    uint32_t* incoming=NULL;
    uint8_t* visited=NULL;
    OirAstGraphStackEntry* stack=NULL;
    OirStatus status;
    int result=1;
    OirAstNodeTableView view;

    if(argc!=2) return 2;
    parser=parser_init(argv[1]);
    if(!parser) return 1;
    if(parser_run(parser)!=0 || !parser->ast_root ||
        ((ASTNode*)parser->ast_root)->type!=AST_PROGRAM) {
        fprintf(stderr,"Could not parse AST round-trip fixture.\n");
        goto cleanup;
    }

    status=encode_ast((ASTNode*)parser->ast_root,&first_payload,&first_length);
    if(status!=OIR_STATUS_OK) {
        fprintf(stderr,"Frontend AST writer failed: %s\n",oir_status_string(status));
        goto cleanup;
    }
    status=oir_ast_node_table_open(first_payload,first_length,&view);
    if(status!=OIR_STATUS_OK) goto cleanup;

    node_storage=(ASTNode**)calloc(view.node_count,sizeof(*node_storage));
    incoming=(uint32_t*)calloc(view.node_count,sizeof(*incoming));
    visited=(uint8_t*)calloc(view.node_count,sizeof(*visited));
    stack=(OirAstGraphStackEntry*)calloc(view.node_count,sizeof(*stack));
    if(!node_storage || !incoming || !visited || !stack) goto cleanup;

    status=oir_ast_frontend_read_payload(first_payload,first_length,node_storage,
        view.node_count,incoming,view.node_count,visited,view.node_count,
        stack,view.node_count,&decoded_root);
    if(status!=OIR_STATUS_OK || !decoded_root) {
        fprintf(stderr,"OIR AST reader failed: %s\n",oir_status_string(status));
        goto cleanup;
    }

    status=encode_ast(decoded_root,&second_payload,&second_length);
    if(status!=OIR_STATUS_OK) {
        fprintf(stderr,"Re-encoding materialized AST failed: %s\n",oir_status_string(status));
        goto cleanup;
    }
    if(first_length!=second_length || memcmp(first_payload,second_payload,first_length)!=0) {
        fprintf(stderr,"AST → OIR → AST → OIR was not byte-for-byte canonical.\n");
        goto cleanup;
    }

    /* Deterministic one-bit mutation sweep over every payload byte. A mutation
     * may remain valid (for example a UTF-8 character change); successful
     * materializations are destroyed, while failures must leave root NULL. */
    ast_destroy(decoded_root);
    decoded_root=NULL;
    {
        size_t accepted=0u,rejected=0u,offset;
        for(offset=0u;offset<first_length;offset++) {
            first_payload[offset]^=0x01u;
            status=oir_ast_frontend_read_payload(first_payload,first_length,node_storage,
                view.node_count,incoming,view.node_count,visited,view.node_count,
                stack,view.node_count,&decoded_root);
            if(status==OIR_STATUS_OK) {
                assert(decoded_root!=NULL);
                ast_destroy(decoded_root);
                decoded_root=NULL;
                ++accepted;
            } else {
                assert(decoded_root==NULL);
                if(status==OIR_STATUS_OUT_OF_MEMORY) {
                    first_payload[offset]^=0x01u;
                    goto cleanup;
                }
                ++rejected;
            }
            first_payload[offset]^=0x01u;
        }
        assert(accepted>0u);
        assert(rejected>0u);
    }

    /* Exercise the same real parser tree through the canonical OIR envelope. */
    status=encode_ast_artifact((ASTNode*)parser->ast_root,77u,&ast_artifact,&artifact_length);
    if(status!=OIR_STATUS_OK) {
        fprintf(stderr,"AST artifact writer failed: %s\\n",oir_status_string(status));
        goto cleanup;
    }
    status=oir_ast_artifact_read_frontend_ast(ast_artifact,artifact_length,node_storage,
        view.node_count,incoming,view.node_count,visited,view.node_count,
        stack,view.node_count,&artifact_root);
    if(status!=OIR_STATUS_OK || !artifact_root) {
        fprintf(stderr,"AST artifact reader failed: %s\\n",oir_status_string(status));
        goto cleanup;
    }
    free(second_payload); second_payload=NULL; second_length=0u;
    status=encode_ast(artifact_root,&second_payload,&second_length);
    if(status!=OIR_STATUS_OK) goto cleanup;
    if(first_length!=second_length || memcmp(first_payload,second_payload,first_length)!=0) {
        fprintf(stderr,"Parser AST artifact handoff changed canonical payload bytes.\\n");
        goto cleanup;
    }
    ast_destroy(artifact_root);
    artifact_root=NULL;

    /* Sweep every byte of the complete OIR container, including its envelope. */
    {
        size_t accepted=0u,rejected=0u,offset;
        for(offset=0u;offset<artifact_length;offset++) {
            ast_artifact[offset]^=0x01u;
            status=oir_ast_artifact_read_frontend_ast(ast_artifact,artifact_length,node_storage,
                view.node_count,incoming,view.node_count,visited,view.node_count,
                stack,view.node_count,&artifact_root);
            if(status==OIR_STATUS_OK) {
                assert(artifact_root!=NULL);
                ast_destroy(artifact_root);
                artifact_root=NULL;
                ++accepted;
            } else {
                assert(artifact_root==NULL);
                if(status==OIR_STATUS_OUT_OF_MEMORY) {
                    ast_artifact[offset]^=0x01u;
                    goto cleanup;
                }
                ++rejected;
            }
            ast_artifact[offset]^=0x01u;
        }
        assert(accepted>0u);
        assert(rejected>0u);
    }

    result=0;
cleanup:
    if(decoded_root) ast_destroy(decoded_root);
    if(artifact_root) ast_destroy(artifact_root);
    free(node_storage); free(incoming); free(visited); free(stack);
    free(first_payload); free(second_payload); free(ast_artifact);
    parser_destroy(parser);
    return result;
}
