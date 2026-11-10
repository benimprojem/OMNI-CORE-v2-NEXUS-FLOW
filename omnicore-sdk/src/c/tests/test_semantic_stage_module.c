#include "occ_module_loader.h"
#include "parser.h"
#include "ast.h"
#include "oir_ast_artifact.h"
#include "oir_ast_frontend_index.h"
#include "oir_ast_node_table.h"
#include "oir_semantic_ast_artifact.h"
#include "oir_module_imports_artifact.h"
#include "oir_module_interface_artifact.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

#define SEMANTIC_CONCURRENT_CALLS 4u
#define SEMANTIC_TEST_OUTPUT_CAPACITY (16u * 1024u * 1024u)

typedef struct SemanticWorker {
    const OccLoadedStageModule* loaded;
    const uint8_t* ast_bytes;
    size_t ast_length;
    int succeeded;
} SemanticWorker;

#if defined(_WIN32)
static DWORD WINAPI semantic_worker_run(LPVOID argument)
#else
static void* semantic_worker_run(void* argument)
#endif
{
    SemanticWorker* worker = (SemanticWorker*)argument;
    OccModuleArtifactInputV1 input;
    OccDiagnosticCollector diagnostics;
    uint8_t* output = NULL;
    size_t output_length = 0u;
    uint32_t status;

    input.bytes = worker->ast_bytes;
    input.length = worker->ast_length;
    input.flags = 0u;
    input.reserved = 0u;
    occ_diagnostics_init(&diagnostics);
    status = occ_module_loader_invoke(worker->loaded, &input, 1u, 77u,
        &diagnostics, 64u, SEMANTIC_TEST_OUTPUT_CAPACITY,
        &output, &output_length);
    worker->succeeded = (status == OCC_MODULE_LOADER_OK &&
        output != NULL && output_length > OIR_HEADER_SIZE &&
        occ_diagnostics_count(&diagnostics) == 0u);
    free(output);
    occ_diagnostics_destroy(&diagnostics);
#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); goto cleanup; } } while(0)

int main(int argc,char** argv) {
    static const uint8_t source[]=
        "f:sum(a!i32, b!i32) !i32 { ret a + b; }\n"
        "f:main() { v:x = 10; v:y = 20; v:z = sum(x, y); }\n";
    ParserContext* parser=NULL; ASTNode* root=NULL;
    OirAstPointerIndexEntry* index=NULL; OirAstNodeRecord* records=NULL;
    uint8_t *fields=NULL,*strings=NULL,*ast_payload=NULL,*ast_artifact=NULL,*semantic_artifact=NULL;
    uint8_t *imports_artifact=NULL,*interface_artifact=NULL;
    size_t index_cap=256u,field_cap=0u,string_cap=0u,payload_cap=0u,ast_len=0u,artifact_len=0u;
    size_t fields_len=0u,strings_len=0u,semantic_cap=16u*1024u*1024u,semantic_len=0u;
    size_t imports_len=0u,interface_len=0u;
    uint32_t node_count=0u; OirAstFrontendRequirements req;
    OirArtifactHeader header; OirSectionInput section;
    OccModuleArtifactInputV1 input,import_inputs[3]; OccDiagnosticCollector diagnostics;
    OccModuleImportBinding import_binding;
    OirModuleImports module_imports;
    OirModuleInterface module_interface;
    OccLoadedStageModule loaded; char loader_error[512];
    uint32_t* incoming=NULL,*owner_scope=NULL; uint8_t* visited=NULL;
    OirAstGraphStackEntry* stack=NULL; OirSemanticAstArtifactView view;
    int exit_code=1;

    if(argc!=2) return 2;
    memset(&loaded,0,sizeof(loaded));
    memset(&import_binding,0,sizeof(import_binding));
    memset(&module_imports,0,sizeof(module_imports));
    memset(&module_interface,0,sizeof(module_interface));
    memset(import_inputs,0,sizeof(import_inputs));
    occ_diagnostics_init(&diagnostics);
    parser=parser_init_source("semantic-stage-test.nxf",source,sizeof(source)-1u);
    CHECK(parser!=NULL); CHECK(parser_run(parser)==0); CHECK(parser->ast_root!=NULL);
    root=(ASTNode*)parser->ast_root;
    CHECK(oir_ast_frontend_index_build(root,index=(OirAstPointerIndexEntry*)calloc(index_cap,sizeof(*index)),index_cap,&node_count)==OIR_STATUS_OK);
    CHECK(oir_ast_frontend_measure(root,index,index_cap,node_count,&req)==OIR_STATUS_OK);
    field_cap=req.field_data_length; string_cap=req.string_table_length; payload_cap=req.payload_size;
    records=(OirAstNodeRecord*)calloc(node_count,sizeof(*records));
    fields=field_cap?(uint8_t*)malloc(field_cap):NULL;
    strings=string_cap?(uint8_t*)malloc(string_cap):NULL;
    ast_payload=(uint8_t*)malloc(payload_cap);
    ast_artifact=(uint8_t*)malloc(OIR_HEADER_SIZE+OIR_SECTION_HEADER_SIZE+payload_cap);
    CHECK(records&&ast_payload&&ast_artifact&&(!field_cap||fields)&&(!string_cap||strings));
    CHECK(oir_ast_frontend_write_payload(root,index,index_cap,node_count,records,node_count,
        fields,field_cap,strings,string_cap,ast_payload,payload_cap,&ast_len,&req)==OIR_STATUS_OK);
    memset(&header,0,sizeof(header)); header.container_major=OIR_CONTAINER_MAJOR;
    header.container_minor=OIR_CONTAINER_MINOR; header.artifact_kind=OIR_KIND_AST;
    header.payload_schema_major=1u; header.payload_schema_minor=0u;
    header.producer_stage_id=OCC_STAGE_PARSER; header.validity=OIR_VALIDITY_VALID; header.job_id=77u;
    memset(&section,0,sizeof(section)); section.type=OIR_SECTION_PAYLOAD;
    section.version=1u; section.flags=OIR_SECTION_FLAG_REQUIRED; section.data=ast_payload; section.length=ast_len;
    CHECK(oir_artifact_write(ast_artifact,OIR_HEADER_SIZE+OIR_SECTION_HEADER_SIZE+payload_cap,
        &header,&section,1u,&artifact_len)==OIR_STATUS_OK);
    CHECK(occ_module_loader_open(&loaded,argv[1],loader_error,sizeof(loader_error))==OCC_MODULE_LOADER_OK);
    CHECK(loaded.module->descriptor.stage_id==OCC_STAGE_SEMANTIC);
    CHECK(loaded.module->descriptor.output_artifact_kind==OIR_KIND_SEMANTIC_AST);
    input.bytes=ast_artifact; input.length=artifact_len; input.flags=0u; input.reserved=0u;
    /* The semantic DLL must reject AST artifacts not produced by Parser. */
    ast_artifact[20] ^= 0x01u;
    CHECK(occ_module_loader_invoke(&loaded,&input,1u,77u,&diagnostics,
        64u,semantic_cap,&semantic_artifact,&semantic_len)!=OCC_MODULE_LOADER_OK);
    ast_artifact[20] ^= 0x01u;
    /* The semantic DLL must reject unsupported AST payload schema versions. */
    ast_artifact[18] ^= 0x01u;
    CHECK(occ_module_loader_invoke(&loaded,&input,1u,77u,&diagnostics,
        64u,semantic_cap,&semantic_artifact,&semantic_len)!=OCC_MODULE_LOADER_OK);
    ast_artifact[18] ^= 0x01u;
    CHECK(occ_module_loader_invoke(&loaded,&input,1u,77u,&diagnostics,
        64u,semantic_cap,&semantic_artifact,&semantic_len)==OCC_MODULE_LOADER_OK);
    CHECK(semantic_artifact!=NULL&&semantic_len>OIR_HEADER_SIZE);
    CHECK(occ_diagnostics_count(&diagnostics)==0u);
    {
        OirArtifactView envelope;
        OirAstNodeTableView ast;
        CHECK(oir_artifact_open(semantic_artifact,semantic_len,&envelope)==OIR_STATUS_OK);
        CHECK(envelope.header.artifact_kind==OIR_KIND_SEMANTIC_AST);
        CHECK(envelope.header.validity==OIR_VALIDITY_VALID&&envelope.header.job_id==77u);
        {
            uint32_t i; OirSectionView s; int found=0;
            for(i=0;i<envelope.section_count;i++) {
                CHECK(oir_artifact_get_section(&envelope,i,&s)==OIR_STATUS_OK);
                if(s.type==OIR_SECTION_PAYLOAD) { CHECK(oir_ast_node_table_open(s.data,(size_t)s.length,&ast)==OIR_STATUS_OK); found=1; }
            }
            CHECK(found);
        }
        incoming=(uint32_t*)calloc(ast.node_count,sizeof(*incoming));
        visited=(uint8_t*)calloc(ast.node_count,sizeof(*visited));
        stack=(OirAstGraphStackEntry*)calloc(ast.node_count,sizeof(*stack));
        owner_scope=(uint32_t*)calloc((size_t)ast.node_count+1u,sizeof(*owner_scope));
        CHECK(incoming&&visited&&stack&&owner_scope);
        CHECK(oir_semantic_ast_artifact_open(semantic_artifact,semantic_len,&view,
            incoming,ast.node_count,visited,ast.node_count,stack,ast.node_count,
            owner_scope,(size_t)ast.node_count+1u)==OIR_STATUS_OK);
        CHECK(view.scopes.record_count>=2u);
        CHECK(view.symbols.record_count>=3u);
    }

    /* The Semantic DLL accepts one verified import manifest plus one verified
     * Module Interface per unique dependency, and checks their exact identities. */
    imports_artifact=(uint8_t*)malloc(4096u);
    interface_artifact=(uint8_t*)malloc(4096u);
    CHECK(imports_artifact&&interface_artifact);
    import_binding.use_ordinal=1u;
    import_binding.requested_path="math";
    import_binding.dependency_identity="file:///lib/math.nxf";
    import_binding.alias="m";
    import_binding.namespace_name="core";
    CHECK(oir_module_imports_build("file:///project/main.nxf",
        &import_binding,1u,&module_imports)==OIR_STATUS_OK);
    CHECK(oir_module_imports_artifact_write(&module_imports,77u,
        imports_artifact,4096u,&imports_len)==OIR_STATUS_OK);
    imports_artifact[20]=(uint8_t)OCC_STAGE_OIR_VERIFY;
    imports_artifact[21]=0u; imports_artifact[22]=0u; imports_artifact[23]=0u;

    module_interface.canonical_identity="file:///lib/math.nxf";
    module_interface.schema_major=OCC_MODULE_INTERFACE_SCHEMA_MAJOR;
    module_interface.schema_minor=OCC_MODULE_INTERFACE_SCHEMA_MINOR;
    CHECK(oir_module_interface_artifact_write(&module_interface,77u,
        interface_artifact,4096u,&interface_len)==OIR_STATUS_OK);
    interface_artifact[20]=(uint8_t)OCC_STAGE_OIR_VERIFY;
    interface_artifact[21]=0u; interface_artifact[22]=0u; interface_artifact[23]=0u;

    free(semantic_artifact); semantic_artifact=NULL; semantic_len=0u;
    import_inputs[0]=input;
    import_inputs[1].bytes=imports_artifact; import_inputs[1].length=imports_len;
    import_inputs[2].bytes=interface_artifact; import_inputs[2].length=interface_len;
    CHECK(occ_module_loader_invoke(&loaded,import_inputs,3u,77u,&diagnostics,
        64u,semantic_cap,&semantic_artifact,&semantic_len)==OCC_MODULE_LOADER_OK);
    CHECK(semantic_artifact&&semantic_len>OIR_HEADER_SIZE);
    CHECK(occ_diagnostics_count(&diagnostics)==0u);
    free(semantic_artifact); semantic_artifact=NULL; semantic_len=0u;

    /* Concurrent invocations share only the immutable module descriptor and AST input. */
    {
        SemanticWorker workers[SEMANTIC_CONCURRENT_CALLS];
#if defined(_WIN32)
        HANDLE threads[SEMANTIC_CONCURRENT_CALLS];
#else
        pthread_t threads[SEMANTIC_CONCURRENT_CALLS];
#endif
        uint32_t i, created = 0u;
        memset(workers, 0, sizeof(workers));
#if defined(_WIN32)
        memset(threads, 0, sizeof(threads));
#endif
        for (i = 0u; i < SEMANTIC_CONCURRENT_CALLS; ++i) {
            workers[i].loaded = &loaded;
            workers[i].ast_bytes = ast_artifact;
            workers[i].ast_length = artifact_len;
#if defined(_WIN32)
            threads[i] = CreateThread(NULL, 0u, semantic_worker_run,
                &workers[i], 0u, NULL);
            if (threads[i] == NULL) {
                uint32_t j;
                for (j = 0u; j < created; ++j) {
                    (void)WaitForSingleObject(threads[j], INFINITE);
                    CloseHandle(threads[j]);
                }
                goto cleanup;
            }
#else
            if (pthread_create(&threads[i], NULL, semantic_worker_run,
                &workers[i]) != 0) {
                uint32_t j;
                for (j = 0u; j < created; ++j)
                    (void)pthread_join(threads[j], NULL);
                goto cleanup;
            }
#endif
            ++created;
        }
#if defined(_WIN32)
        if (WaitForMultipleObjects(SEMANTIC_CONCURRENT_CALLS, threads,
            TRUE, INFINITE) != WAIT_OBJECT_0) {
            for (i = 0u; i < created; ++i)
                (void)WaitForSingleObject(threads[i], INFINITE);
        }
        for (i = 0u; i < created; ++i) CloseHandle(threads[i]);
#else
        for (i = 0u; i < created; ++i) {
            if (pthread_join(threads[i], NULL) != 0) goto cleanup;
        }
#endif
        for (i = 0u; i < SEMANTIC_CONCURRENT_CALLS; ++i)
            CHECK(workers[i].succeeded);
    }
    exit_code=0; puts("Real semantic stage shared-module test passed.");

cleanup:
    free(semantic_artifact); free(imports_artifact); free(interface_artifact);
    oir_module_imports_destroy(&module_imports);
    free(index); free(records); free(fields); free(strings);
    free(ast_payload); free(ast_artifact); free(incoming); free(visited); free(stack); free(owner_scope);
    occ_module_loader_close(&loaded); occ_diagnostics_destroy(&diagnostics);
    if(parser) parser_destroy(parser);
    return exit_code;
}
