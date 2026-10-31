#if !defined(_WIN32)
#define _GNU_SOURCE
#endif
#include "occ_module_api.h"
#include "semantic.h"
#include "ast.h"
#include "oir_ast_artifact.h"
#include "oir_ast_frontend_index.h"
#include "oir_ast_node_table.h"
#include "oir_semantic_ast_artifact.h"
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <string.h>

#define SEMANTIC_INDEX_MAX ((size_t)2097152u)
typedef struct SemanticResolver {
    const OirAstPointerIndexEntry* entries;
    size_t capacity;
} SemanticResolver;
static const OccStageModuleV1 g_semantic_module;

/* Each DLL owns its message catalog; resolve it beside the loaded module. */
static void semantic_stage_init_messages(void) {
    char module_path[4096], config_path[4096], language_relative[64], message_path[4096];
    char language[16] = "en";
    FILE* config;
    char line[256];
    char* separator;
#if defined(_WIN32)
    HMODULE module_handle = NULL;
    DWORD length;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)(uintptr_t)&semantic_stage_init_messages, &module_handle)) return;
    length = GetModuleFileNameA(module_handle, module_path, (DWORD)sizeof(module_path));
    if (length == 0u || length >= sizeof(module_path)) return;
    module_path[length] = '\0';
#else
    Dl_info info;
    if (!dladdr((void*)(uintptr_t)&semantic_stage_init_messages, &info) ||
        !info.dli_fname || strlen(info.dli_fname) >= sizeof(module_path)) return;
    strcpy(module_path, info.dli_fname);
#endif
    separator = strrchr(module_path, '/');
#if defined(_WIN32)
    {
        char* backslash = strrchr(module_path, '\\');
        if (backslash && (!separator || backslash > separator)) separator = backslash;
    }
#endif
    if (!separator) return;
    *separator = '\0';
    {
        int n = snprintf(config_path, sizeof(config_path), "%s/occ.ocf", module_path);
        if (n < 0 || (size_t)n >= sizeof(config_path)) return;
    }
    config = fopen(config_path, "r");
    if (config) {
        while (fgets(line, sizeof(line), config)) {
            char* p = line;
            char* end;
            while (*p == ' ' || *p == '\t') ++p;
            if (strncmp(p, "language", 8u) != 0) continue;
            p += 8u;
            while (*p == ' ' || *p == '\t') ++p;
            if (*p++ != '=') continue;
            while (*p == ' ' || *p == '\t') ++p;
            if (*p++ != '"') continue;
            end = strchr(p, '"');
            if (!end) continue;
            *end = '\0';
            if (p[0] && strlen(p) < sizeof(language)) {
                size_t j; int valid = 1;
                for (j = 0u; p[j] != '\0'; ++j) {
                    if (!((p[j] >= 'a' && p[j] <= 'z') ||
                          (p[j] >= 'A' && p[j] <= 'Z') ||
                          (p[j] >= '0' && p[j] <= '9') ||
                          p[j] == '_' || p[j] == '-')) {
                        valid = 0; break;
                    }
                }
                if (valid) strcpy(language, p);
            }
            break;
        }
        fclose(config);
    }
    {
        int n = snprintf(language_relative, sizeof(language_relative),
            "lang/messages_%s.conf", language);
        if (n < 0 || (size_t)n >= sizeof(language_relative)) return;
        n = snprintf(message_path, sizeof(message_path), "%s/%s",
            module_path, language_relative);
        if (n < 0 || (size_t)n >= sizeof(message_path)) return;
    }
    msg_init(message_path);
}

static OirStatus find_payload(const OirArtifactView* artifact, uint16_t type,
                              OirSectionView* out) {
    uint32_t i; int found=0;
    for(i=0u;i<artifact->section_count;i++) {
        OirSectionView s; OirStatus st=oir_artifact_get_section(artifact,i,&s);
        if(st!=OIR_STATUS_OK) return st;
        if(s.type==type) { if(found) return OIR_STATUS_DUPLICATE_PAYLOAD; *out=s; found=1; }
    }
    return found?OIR_STATUS_OK:OIR_STATUS_MISSING_PAYLOAD;
}
static uint32_t resolve_node(void* user,const ASTNode* node) {
    SemanticResolver* r=(SemanticResolver*)user;
    return (r&&r->entries&&node)?oir_ast_frontend_index_lookup(r->entries,r->capacity,node):0u;
}
static int add_size(size_t* n,size_t v) {
    if(v>SIZE_MAX-*n) return 0; *n+=v; return 1;
}
static int add_name(size_t* n,const char* s) {
    return (!s||!*s)?1:add_size(n,strlen(s));
}
/* Keep this calculation synchronized with the canonical artifact writer. */
static int required_size(const uint8_t* ast,size_t ast_len,const SemanticResult* m,size_t* out) {
    size_t n=OIR_HEADER_SIZE,ts=0u,ss=0u,rs=0u; uint32_t i,sections;
    if(!ast||!m||!out||!m->finalized||!m->scope_count) return 0;
    for(i=0;i<m->type_count;i++) if(!add_name(&ts,m->types[i].name)) return 0;
    for(i=0;i<m->symbol_count;i++) if(!add_name(&ss,m->symbols[i].name)) return 0;
    for(i=0;i<m->relation_count;i++) if(!add_name(&rs,m->relations[i].name)) return 0;
    sections=m->conversion_count?7u:6u;
    if(!add_size(&n,(size_t)sections*OIR_SECTION_HEADER_SIZE)||!add_size(&n,ast_len)||
       !add_size(&n,OIR_SEMANTIC_SCOPES_HEADER_SIZE+(size_t)m->scope_count*OIR_SEMANTIC_SCOPE_RECORD_SIZE)||
       !add_size(&n,OIR_SEMANTIC_TYPES_HEADER_SIZE+(size_t)m->type_count*OIR_SEMANTIC_TYPE_RECORD_SIZE)||!add_size(&n,ts)||
       !add_size(&n,OIR_SEMANTIC_SYMBOLS_HEADER_SIZE+(size_t)m->symbol_count*OIR_SEMANTIC_SYMBOL_RECORD_SIZE)||!add_size(&n,ss)||
       !add_size(&n,OIR_SEMANTIC_ANNOTATIONS_HEADER_SIZE+(size_t)m->annotation_count*OIR_SEMANTIC_ANNOTATION_RECORD_SIZE)||
       !add_size(&n,OIR_SEMANTIC_RELATIONS_HEADER_SIZE+(size_t)m->relation_count*OIR_SEMANTIC_RELATION_RECORD_SIZE)||!add_size(&n,rs)||
       (m->conversion_count&&!add_size(&n,OIR_SEMANTIC_CONVERSIONS_HEADER_SIZE+(size_t)m->conversion_count*OIR_SEMANTIC_CONVERSION_RECORD_SIZE))) return 0;
    *out=n; return 1;
}
static int send_diagnostics(const OccModuleInvocationV1* inv,const OccDiagnosticCollector* c) {
    size_t i;
    if(occ_diagnostics_count(c)&&!inv->diagnostic_sink.report) return 0;
    for(i=0;i<occ_diagnostics_count(c);i++) {
        OccDiagnosticView v; OccModuleDiagnosticV1 d;
        if(!occ_diagnostics_get(c,i,&v)||!v.record) return 0;
        memset(&d,0,sizeof(d)); d.struct_size=sizeof(d); d.code=v.record->code;
        d.message_id=v.record->message_id; d.severity=v.record->severity; d.span=v.record->span;
        d.message=v.message; d.message_length=v.record->message_length;
        d.hint=v.hint; d.hint_length=v.record->hint_length;
        if(!inv->diagnostic_sink.report(inv->diagnostic_sink.user_context,&d)) return 0;
    }
    return 1;
}
static uint32_t OCC_MODULE_CALL semantic_process(const OccModuleInvocationV1* inv,OccModuleResultV1* out) {
    OirArtifactView artifact; OirSectionView payload; OirAstNodeTableView table;
    ASTNode **storage=NULL,*root=NULL; uint32_t *incoming=NULL,count=0u;
    uint8_t *visited=NULL; OirAstGraphStackEntry *stack=NULL;
    OirAstPointerIndexEntry *index=NULL; size_t index_cap=2u,required=0u,written=0u;
    SemanticResolver resolver; SemanticContext sem; OccDiagnosticCollector diags;
    int sem_init=0,diag_init=0; OirStatus st; uint32_t rc=OCC_MODULE_STATUS_FAILED;
    memset(&sem,0,sizeof(sem)); memset(&resolver,0,sizeof(resolver));
    out->bytes_written=0u; out->required_output_capacity=0u; out->output_validity=OIR_VALIDITY_NONE;
    out->failure_code=0u; out->diagnostics_count=0u;
    if(occ_module_validate_invocation_v1(&g_semantic_module.descriptor,inv,out)!=OCC_MODULE_STATUS_OK||inv->input_count!=1u)
        return OCC_MODULE_STATUS_INVALID_INVOCATION;
    if(oir_artifact_open(inv->inputs[0].bytes,(size_t)inv->inputs[0].length,&artifact)!=OIR_STATUS_OK||
       artifact.header.artifact_kind!=OIR_KIND_AST||
       artifact.header.payload_schema_major!=1u||artifact.header.payload_schema_minor!=0u||
       artifact.header.producer_stage_id!=OCC_STAGE_PARSER||
       artifact.header.validity!=OIR_VALIDITY_VALID||
       artifact.header.job_id!=inv->job_id||find_payload(&artifact,OIR_SECTION_PAYLOAD,&payload)!=OIR_STATUS_OK||
       oir_ast_node_table_open(payload.data,(size_t)payload.length,&table)!=OIR_STATUS_OK||!table.node_count) {
        out->failure_code=8301u; return OCC_MODULE_STATUS_INVALID_INVOCATION;
    }
    semantic_stage_init_messages();
    count=table.node_count;
    storage=(ASTNode**)calloc(count,sizeof(*storage)); incoming=(uint32_t*)calloc(count,sizeof(*incoming));
    visited=(uint8_t*)calloc(count,sizeof(*visited)); stack=(OirAstGraphStackEntry*)calloc(count,sizeof(*stack));
    if(!storage||!incoming||!visited||!stack) goto allocation_failure;
    st=oir_ast_artifact_read_frontend_ast(inv->inputs[0].bytes,(size_t)inv->inputs[0].length,
        storage,count,incoming,count,visited,count,stack,count,&root);
    if(st!=OIR_STATUS_OK||!root) { out->failure_code=8302u; rc=OCC_MODULE_STATUS_INVALID_INVOCATION; goto done; }
    while(index_cap<(size_t)count*2u&&index_cap<SEMANTIC_INDEX_MAX) index_cap*=2u;
    if(index_cap<(size_t)count*2u||index_cap>SEMANTIC_INDEX_MAX) { out->failure_code=8303u; goto done; }
    index=(OirAstPointerIndexEntry*)calloc(index_cap,sizeof(*index));
    if(!index) goto allocation_failure;
    st=oir_ast_frontend_index_build(root,index,index_cap,&count);
    if(st!=OIR_STATUS_OK) { out->failure_code=8304u; goto done; }
    resolver.entries=index; resolver.capacity=index_cap;
    semantic_init(&sem,"<oir-ast>",NULL); sem_init=1;
    if(sem.result_status!=SEMANTIC_RESULT_OK) goto allocation_failure;
    occ_diagnostics_init(&diags); diag_init=1;
    semantic_attach_diagnostics(&sem,&diags,OCC_STAGE_SEMANTIC);
    semantic_attach_node_id_resolver(&sem,resolve_node,&resolver);
    semantic_check(&sem,root);
    out->diagnostics_count=(uint32_t)occ_diagnostics_count(&diags);
    if(sem.error_count||diags.allocation_failed) {
        if(!send_diagnostics(inv,&diags)) { out->failure_code=8305u; goto done; }
        out->failure_code=8306u; rc=OCC_MODULE_STATUS_FAILED; goto done;
    }
    if(semantic_finalize_result(&sem,count)!=SEMANTIC_RESULT_OK) { out->failure_code=8307u; goto done; }
    if(!required_size(payload.data,(size_t)payload.length,&sem.result,&required)) { out->failure_code=8308u; goto done; }
    out->required_output_capacity=(uint64_t)required;
    if(inv->output_capacity<(uint64_t)required) { rc=OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER; goto done; }
#if SIZE_MAX < UINT64_MAX
    if(inv->output_capacity>(uint64_t)SIZE_MAX) { out->failure_code=8309u; goto done; }
#endif
    st=oir_semantic_ast_artifact_write(payload.data,(size_t)payload.length,&sem.result,
        inv->job_id,inv->output_buffer,(size_t)inv->output_capacity,&written);
    if(st!=OIR_STATUS_OK||written!=required) { out->failure_code=8310u; goto done; }
    out->bytes_written=(uint64_t)written; out->required_output_capacity=0u;
    out->output_validity=OIR_VALIDITY_VALID; out->diagnostics_count=(uint32_t)occ_diagnostics_count(&diags);
    rc=OCC_MODULE_STATUS_OK; goto done;
allocation_failure:
    out->failure_code=8311u; rc=OCC_MODULE_STATUS_FAILED;
done:
    if(diag_init) {
        if(rc==OCC_MODULE_STATUS_FAILED&&occ_diagnostics_count(&diags)&&out->failure_code!=8305u&&out->failure_code!=8306u)
            (void)send_diagnostics(inv,&diags);
        occ_diagnostics_destroy(&diags);
    }
    if(sem_init) semantic_destroy(&sem);
    msg_destroy();
    if(root) ast_destroy(root);
    free(storage); free(incoming); free(visited); free(stack); free(index);
    return rc;
}
static const OccStageModuleV1 g_semantic_module={
    (uint32_t)sizeof(OccStageModuleV1),0u,
    {(uint32_t)sizeof(OccStageDescriptorV1),OCC_MODULE_ABI_MAJOR,OCC_MODULE_ABI_MINOR,
     0x00010003u,OCC_STAGE_SEMANTIC,1u,0u,0u,0u,
     OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_AST),OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_AST),
     OIR_KIND_SEMANTIC_AST,1u,1u,0u,
     (uint64_t)OCC_MODULE_CAP_DETERMINISTIC|(uint64_t)OCC_MODULE_CAP_DIAGNOSTIC_REPORTING,
     "nexsus-semantic"},semantic_process
};
OCC_MODULE_EXPORT const OccStageModuleV1* OCC_MODULE_CALL occ_get_stage_module_v1(void) {
    return &g_semantic_module;
}
