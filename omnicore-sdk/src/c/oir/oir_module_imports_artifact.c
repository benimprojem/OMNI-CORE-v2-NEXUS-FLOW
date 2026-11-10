#include "oir_module_imports_artifact.h"
#include "module_scheduler.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define OMI_MAGIC_0 ((uint8_t)'O')
#define OMI_MAGIC_1 ((uint8_t)'I')
#define OMI_MAGIC_2 ((uint8_t)'M')
#define OMI_MAGIC_3 ((uint8_t)'1')
#define OMI_HEADER_SIZE 16u
#define OMI_RECORD_SIZE 60u

static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t rd64(const uint8_t* p) { return (uint64_t)rd32(p) | ((uint64_t)rd32(p+4) << 32); }
static void wr16(uint8_t* p,uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void wr32(uint8_t* p,uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
static void wr64(uint8_t* p,uint64_t v) { wr32(p,(uint32_t)v); wr32(p+4,(uint32_t)(v>>32)); }

static char* dup_text(const char* s) {
    size_t n; char* p;
    if (!s) return NULL;
    n=strlen(s);
    if (n==SIZE_MAX || n>OIR_MODULE_IMPORTS_MAX_STRING) return NULL;
    p=(char*)malloc(n+1u);
    if(p) memcpy(p,s,n+1u);
    return p;
}
static int valid_text(const char* s,int optional) {
    size_t n;
    if(!s) return optional;
    n=strlen(s);
    return (optional || n!=0u) && n<=OIR_MODULE_IMPORTS_MAX_STRING;
}
static int add_size(size_t* n,size_t x) {
    if(x>SIZE_MAX-*n) return 0;
    *n+=x; return 1;
}
static void free_record(OirModuleImportRecord* r) {
    if(!r) return;
    free(r->requested_path); free(r->alias); free(r->namespace_name);
    free(r->dependency_identity); memset(r,0,sizeof(*r));
}
void oir_module_imports_destroy(OirModuleImports* imports) {
    size_t i;
    if(!imports) return;
    for(i=0;i<imports->record_count;i++) free_record(&imports->records[i]);
    free(imports->records); free(imports->importer_identity);
    memset(imports,0,sizeof(*imports));
}
OirStatus oir_module_imports_build(const char* importer_identity,
    const OccModuleImportBinding* bindings,size_t binding_count,
    OirModuleImports* out_imports) {
    size_t i;
    uint8_t* seen=NULL;
    if(!out_imports || !valid_text(importer_identity,0) ||
       (binding_count && !bindings) || binding_count>OIR_MODULE_IMPORTS_MAX_RECORDS)
        return OIR_STATUS_INVALID_ARGUMENT;
    memset(out_imports,0,sizeof(*out_imports));
    seen=(uint8_t*)calloc((size_t)OIR_MODULE_IMPORTS_MAX_RECORDS+1u,1u);
    if(!seen) return OIR_STATUS_OUT_OF_MEMORY;
    out_imports->importer_identity=dup_text(importer_identity);
    if(!out_imports->importer_identity) { free(seen); return OIR_STATUS_OUT_OF_MEMORY; }
    out_imports->schema_major=OIR_MODULE_IMPORTS_SCHEMA_MAJOR;
    out_imports->schema_minor=OIR_MODULE_IMPORTS_SCHEMA_MINOR;
    if(binding_count) {
        if(binding_count>SIZE_MAX/sizeof(*out_imports->records)) {
            free(seen); oir_module_imports_destroy(out_imports); return OIR_STATUS_OVERFLOW;
        }
        out_imports->records=(OirModuleImportRecord*)calloc(binding_count,sizeof(*out_imports->records));
        if(!out_imports->records) { free(seen); oir_module_imports_destroy(out_imports); return OIR_STATUS_OUT_OF_MEMORY; }
    }
    for(i=0;i<binding_count;i++) {
        const OccModuleImportBinding* b=&bindings[i];
        OirModuleImportRecord* r=&out_imports->records[i];
        if(!b->use_ordinal || b->use_ordinal>OIR_MODULE_IMPORTS_MAX_RECORDS ||
           seen[b->use_ordinal] || !valid_text(b->requested_path,0) ||
           !valid_text(b->dependency_identity,0) ||
           !valid_text(b->alias,1) || !valid_text(b->namespace_name,1)) {
            free(seen); oir_module_imports_destroy(out_imports); return OIR_STATUS_INVALID_ARGUMENT;
        }
        seen[b->use_ordinal]=1u;
        r->use_ordinal=b->use_ordinal; r->span=b->span;
        r->requested_path=dup_text(b->requested_path);
        r->alias=b->alias?dup_text(b->alias):NULL;
        r->namespace_name=b->namespace_name?dup_text(b->namespace_name):NULL;
        r->dependency_identity=dup_text(b->dependency_identity);
        if(!r->requested_path || (b->alias&&!r->alias) ||
           (b->namespace_name&&!r->namespace_name) || !r->dependency_identity) {
            out_imports->record_count=i+1u;
            free(seen); oir_module_imports_destroy(out_imports); return OIR_STATUS_OUT_OF_MEMORY;
        }
        out_imports->record_count=i+1u;
    }
    free(seen);
    return OIR_STATUS_OK;
}

OirStatus oir_module_imports_build_from_scheduler(
    const OccModuleScheduler* scheduler,
    uint32_t importer_module_id,
    OirModuleImports* out_imports
) {
    size_t count=0u,i;
    OccModuleImportBinding* bindings=NULL;
    OccModuleSchedulerStatus scheduler_status;
    OirStatus status;
    const OccModuleResolverNode* importer;
    if(!scheduler || !out_imports) return OIR_STATUS_INVALID_ARGUMENT;
    importer=occ_module_resolver_get_module(&scheduler->resolver,importer_module_id);
    if(!importer || !importer->canonical_identity) return OIR_STATUS_INVALID_ARGUMENT;
    scheduler_status=occ_module_scheduler_get_import_binding_count(
        scheduler,importer_module_id,&count);
    if(scheduler_status!=OCC_MODULE_SCHEDULER_OK) return OIR_STATUS_INVALID_ARGUMENT;
    if(count) {
        if(count>SIZE_MAX/sizeof(*bindings)) return OIR_STATUS_OVERFLOW;
        bindings=(OccModuleImportBinding*)calloc(count,sizeof(*bindings));
        if(!bindings) return OIR_STATUS_OUT_OF_MEMORY;
    }
    for(i=0u;i<count;i++) {
        scheduler_status=occ_module_scheduler_get_import_binding(
            scheduler,importer_module_id,(uint32_t)(i+1u),&bindings[i]);
        if(scheduler_status!=OCC_MODULE_SCHEDULER_OK) {
            free(bindings); return OIR_STATUS_INVALID_SECTION;
        }
    }
    status=oir_module_imports_build(importer->canonical_identity,bindings,count,out_imports);
    free(bindings);
    return status;
}

static int payload_size(const OirModuleImports* m,size_t* out) {
    size_t n=OMI_HEADER_SIZE+strlen(m->importer_identity);
    size_t i;
    if(m->record_count>OIR_MODULE_IMPORTS_MAX_RECORDS) return 0;
    for(i=0;i<m->record_count;i++) {
        const OirModuleImportRecord* r=&m->records[i];
        const char* s[4]={r->requested_path,r->alias,r->namespace_name,r->dependency_identity};
        size_t k;
        if(!r->use_ordinal || !valid_text(s[0],0)||!valid_text(s[1],1)||
           !valid_text(s[2],1)||!valid_text(s[3],0)||!add_size(&n,OMI_RECORD_SIZE)) return 0;
        for(k=0;k<4;k++) if(s[k]&&!add_size(&n,strlen(s[k]))) return 0;
    }
    *out=n; return 1;
}
OirStatus oir_module_imports_artifact_write(const OirModuleImports* m,
    uint64_t job_id,uint8_t* output,size_t capacity,size_t* out_written) {
    size_t psize,total,written=0,off,i;
    uint8_t* payload;
    OirArtifactHeader h;
    OirSectionInput section;
    OirStatus st;
    if(out_written) *out_written=0;
    if(!m||!out_written||!output||!valid_text(m->importer_identity,0)||
       m->schema_major!=OIR_MODULE_IMPORTS_SCHEMA_MAJOR||
       m->schema_minor>OIR_MODULE_IMPORTS_SCHEMA_MINOR||
       (m->record_count&&!m->records)) return OIR_STATUS_INVALID_ARGUMENT;
    if(!payload_size(m,&psize)) return OIR_STATUS_INVALID_SECTION;
    if(psize>SIZE_MAX-OIR_HEADER_SIZE-OIR_SECTION_HEADER_SIZE) return OIR_STATUS_OVERFLOW;
    total=OIR_HEADER_SIZE+OIR_SECTION_HEADER_SIZE+psize;
    if(total>capacity) return OIR_STATUS_BUFFER_TOO_SMALL;
    payload=(uint8_t*)calloc(psize?psize:1u,1u);
    if(!payload) return OIR_STATUS_OUT_OF_MEMORY;
    payload[0]=OMI_MAGIC_0; payload[1]=OMI_MAGIC_1; payload[2]=OMI_MAGIC_2; payload[3]=OMI_MAGIC_3;
    wr16(payload+4,(uint16_t)m->schema_major); wr16(payload+6,(uint16_t)m->schema_minor);
    wr32(payload+8,(uint32_t)m->record_count);
    wr32(payload+12,(uint32_t)strlen(m->importer_identity));
    off=OMI_HEADER_SIZE; memcpy(payload+off,m->importer_identity,strlen(m->importer_identity)); off+=strlen(m->importer_identity);
    for(i=0;i<m->record_count;i++) {
        const OirModuleImportRecord* r=&m->records[i];
        const char* s[4]={r->requested_path,r->alias,r->namespace_name,r->dependency_identity};
        uint32_t lens[4]; size_t k;
        lens[0]=(uint32_t)strlen(s[0]); lens[1]=s[1]?(uint32_t)strlen(s[1]):0u;
        lens[2]=s[2]?(uint32_t)strlen(s[2]):0u; lens[3]=(uint32_t)strlen(s[3]);
        wr32(payload+off,r->use_ordinal); wr32(payload+off+4,r->span.source_id);
        wr64(payload+off+8,r->span.start_offset); wr64(payload+off+16,r->span.end_offset);
        wr32(payload+off+24,r->span.start_line); wr32(payload+off+28,r->span.start_column);
        wr32(payload+off+32,r->span.end_line); wr32(payload+off+36,r->span.end_column);
        payload[off+40]=r->span.has_location;
        for(k=0;k<4;k++) wr32(payload+off+44u+k*4u,lens[k]);
        off+=OMI_RECORD_SIZE;
        for(k=0;k<4;k++) if(lens[k]) { memcpy(payload+off,s[k],lens[k]); off+=lens[k]; }
    }
    memset(&h,0,sizeof(h)); h.container_major=OIR_CONTAINER_MAJOR;
    h.container_minor=OIR_CONTAINER_MINOR; h.artifact_kind=OIR_KIND_MODULE_IMPORTS;
    h.payload_schema_major=1u; h.payload_schema_minor=0u;
    h.producer_stage_id=OCC_STAGE_OCC_HOST; h.validity=OIR_VALIDITY_VALID; h.job_id=job_id;
    section.type=OIR_SECTION_PAYLOAD; section.version=1u; section.flags=OIR_SECTION_FLAG_REQUIRED;
    section.data=payload; section.length=psize;
    st=oir_artifact_write(output,capacity,&h,&section,1u,&written);
    free(payload);
    if(st==OIR_STATUS_OK) *out_written=written;
    return st;
}

static char* read_text(const uint8_t* p,size_t n,size_t* off,uint32_t len,int optional) {
    char* s;
    if(len==0u) return optional?NULL:NULL;
    if(len>OIR_MODULE_IMPORTS_MAX_STRING || *off>n || len>n-*off ||
       memchr(p+*off,0,len)) return NULL;
    s=(char*)malloc((size_t)len+1u);
    if(!s) return NULL;
    memcpy(s,p+*off,len); s[len]='\0'; *off+=len;
    return s;
}
OirStatus oir_module_imports_artifact_open(const uint8_t* bytes,size_t length,
    OirModuleImportsArtifactView* out) {
    OirSectionView sec; OirStatus st; size_t off=OMI_HEADER_SIZE,i;
    uint32_t count,idlen; const uint8_t* p; size_t n;
    uint8_t* seen=NULL;
    if(!bytes||!out) return OIR_STATUS_INVALID_ARGUMENT;
    memset(out,0,sizeof(*out));
    st=oir_artifact_open(bytes,length,&out->envelope);
    if(st!=OIR_STATUS_OK) return st;
    if(out->envelope.section_count!=1u ||
       out->envelope.header.artifact_kind!=OIR_KIND_MODULE_IMPORTS ||
       out->envelope.header.producer_stage_id!=OCC_STAGE_OCC_HOST ||
       out->envelope.header.validity!=OIR_VALIDITY_VALID ||
       out->envelope.header.payload_schema_major!=1u ||
       out->envelope.header.payload_schema_minor!=0u) return OIR_STATUS_INVALID_HEADER;
    st=oir_artifact_get_section(&out->envelope,0u,&sec);
    if(st!=OIR_STATUS_OK || sec.type!=OIR_SECTION_PAYLOAD || sec.version!=1u ||
       sec.length>SIZE_MAX) return OIR_STATUS_INVALID_SECTION;
    p=sec.data; n=(size_t)sec.length;
    if(n<OMI_HEADER_SIZE || p[0]!=OMI_MAGIC_0 || p[1]!=OMI_MAGIC_1 ||
       p[2]!=OMI_MAGIC_2 || p[3]!=OMI_MAGIC_3 ||
       rd16(p+4)!=OIR_MODULE_IMPORTS_SCHEMA_MAJOR ||
       rd16(p+6)>OIR_MODULE_IMPORTS_SCHEMA_MINOR) return OIR_STATUS_INVALID_SECTION;
    count=rd32(p+8); idlen=rd32(p+12);
    if(count>OIR_MODULE_IMPORTS_MAX_RECORDS || idlen==0u ||
       idlen>OIR_MODULE_IMPORTS_MAX_STRING || idlen>n-off ||
       memchr(p+off,0,idlen)) return OIR_STATUS_INVALID_SECTION;
    out->imports.importer_identity=read_text(p,n,&off,idlen,0);
    if(!out->imports.importer_identity) return OIR_STATUS_OUT_OF_MEMORY;
    out->imports.schema_major=rd16(p+4); out->imports.schema_minor=rd16(p+6);
    seen=(uint8_t*)calloc((size_t)OIR_MODULE_IMPORTS_MAX_RECORDS+1u,1u);
    if(!seen) { st=OIR_STATUS_OUT_OF_MEMORY; goto fail; }
    if(count) {
        if((size_t)count>SIZE_MAX/sizeof(*out->imports.records)) { st=OIR_STATUS_OVERFLOW; goto fail; }
        out->imports.records=(OirModuleImportRecord*)calloc(count,sizeof(*out->imports.records));
        if(!out->imports.records) { st=OIR_STATUS_OUT_OF_MEMORY; goto fail; }
    }
    for(i=0;i<count;i++) {
        OirModuleImportRecord* r=&out->imports.records[i];
        uint32_t lens[4]; size_t k;
        if(off>n || n-off<OMI_RECORD_SIZE) { st=OIR_STATUS_INVALID_SECTION; goto fail; }
        r->use_ordinal=rd32(p+off); r->span.source_id=rd32(p+off+4);
        r->span.start_offset=rd64(p+off+8); r->span.end_offset=rd64(p+off+16);
        r->span.start_line=rd32(p+off+24); r->span.start_column=rd32(p+off+28);
        r->span.end_line=rd32(p+off+32); r->span.end_column=rd32(p+off+36);
        r->span.has_location=p[off+40];
        if(!r->use_ordinal || r->use_ordinal>OIR_MODULE_IMPORTS_MAX_RECORDS ||
           seen[r->use_ordinal] || r->span.has_location>1u ||
           (r->span.has_location && r->span.end_offset<r->span.start_offset) ||
           p[off+41]||p[off+42]||p[off+43]) { st=OIR_STATUS_INVALID_SECTION; goto fail; }
        seen[r->use_ordinal]=1u;
        for(k=0;k<4;k++) lens[k]=rd32(p+off+44u+k*4u);
        off+=OMI_RECORD_SIZE;
        out->imports.record_count=i+1u;
        r->requested_path=read_text(p,n,&off,lens[0],0);
        r->alias=read_text(p,n,&off,lens[1],1);
        r->namespace_name=read_text(p,n,&off,lens[2],1);
        r->dependency_identity=read_text(p,n,&off,lens[3],0);
        if(!r->requested_path||!r->dependency_identity ||
           (lens[1]&&!r->alias)||(lens[2]&&!r->namespace_name)) { st=OIR_STATUS_INVALID_SECTION; goto fail; }
        if(r->span.has_location && r->span.end_offset<r->span.start_offset) {
            st=OIR_STATUS_INVALID_SECTION; goto fail;
        }
    }
    if(off!=n) { st=OIR_STATUS_INVALID_SECTION; goto fail; }
    free(seen);
    return OIR_STATUS_OK;
fail:
    free(seen);
    oir_module_imports_artifact_destroy(out);
    return st;
}
void oir_module_imports_artifact_destroy(OirModuleImportsArtifactView* view) {
    if(!view) return;
    oir_module_imports_destroy(&view->imports);
    memset(view,0,sizeof(*view));
}
