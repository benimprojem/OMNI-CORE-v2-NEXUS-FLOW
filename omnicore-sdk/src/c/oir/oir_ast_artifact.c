#include "oir_ast_artifact.h"

#include "diagnostics.h"

#include <string.h>

OirStatus oir_ast_artifact_write_frontend_ast(
    const ASTNode* root,
    const OirAstPointerIndexEntry* index_entries,
    size_t index_capacity,
    uint32_t node_count,
    OirAstNodeRecord* records,
    size_t records_capacity,
    uint8_t* field_data,
    size_t field_capacity,
    uint8_t* string_table,
    size_t string_capacity,
    uint8_t* payload_scratch,
    size_t payload_capacity,
    uint8_t* artifact_output,
    size_t artifact_capacity,
    uint64_t job_id,
    size_t* out_written,
    OirAstFrontendRequirements* out_requirements
) {
    OirAstFrontendRequirements req;
    OirArtifactHeader header;
    OirSectionInput section;
    size_t payload_length=0u;
    size_t artifact_required;
    OirStatus status;

    if(out_written) *out_written=0u;
    if(out_requirements) memset(out_requirements,0,sizeof(*out_requirements));
    if(!out_written || !records || !payload_scratch || !artifact_output) {
        return OIR_STATUS_INVALID_ARGUMENT;
    }
    status=oir_ast_frontend_measure(root,index_entries,index_capacity,node_count,&req);
    if(status!=OIR_STATUS_OK) return status;
    if(out_requirements) *out_requirements=req;
    if(req.payload_size>SIZE_MAX-OIR_HEADER_SIZE-OIR_SECTION_HEADER_SIZE) {
        return OIR_STATUS_OVERFLOW;
    }
    artifact_required=OIR_HEADER_SIZE+OIR_SECTION_HEADER_SIZE+req.payload_size;
    if(records_capacity<node_count || field_capacity<req.field_data_length ||
        string_capacity<req.string_table_length || payload_capacity<req.payload_size ||
        artifact_capacity<artifact_required ||
        (req.field_data_length && !field_data) ||
        (req.string_table_length && !string_table)) {
        return OIR_STATUS_BUFFER_TOO_SMALL;
    }

    status=oir_ast_frontend_write_payload(root,index_entries,index_capacity,node_count,
        records,records_capacity,field_data,field_capacity,string_table,string_capacity,
        payload_scratch,payload_capacity,&payload_length,&req);
    if(status!=OIR_STATUS_OK) return status;

    memset(&header,0,sizeof(header));
    header.container_major=OIR_CONTAINER_MAJOR;
    header.container_minor=OIR_CONTAINER_MINOR;
    header.artifact_kind=OIR_KIND_AST;
    header.payload_schema_major=1u;
    header.payload_schema_minor=0u;
    header.producer_stage_id=OCC_STAGE_PARSER;
    header.validity=OIR_VALIDITY_VALID;
    header.job_id=job_id;

    memset(&section,0,sizeof(section));
    section.type=OIR_SECTION_PAYLOAD;
    section.version=1u;
    section.flags=OIR_SECTION_FLAG_REQUIRED;
    section.data=payload_scratch;
    section.length=(uint64_t)payload_length;

    status=oir_artifact_write(artifact_output,artifact_capacity,&header,&section,1u,out_written);
    if(status!=OIR_STATUS_OK) *out_written=0u;
    return status;
}

OirStatus oir_ast_artifact_read_frontend_ast(
    const uint8_t* artifact_bytes,
    size_t artifact_length,
    ASTNode** node_storage,
    size_t node_capacity,
    uint32_t* incoming_counts,
    size_t incoming_capacity,
    uint8_t* visited,
    size_t visited_capacity,
    OirAstGraphStackEntry* graph_stack,
    size_t graph_stack_capacity,
    ASTNode** out_root
) {
    OirArtifactView artifact;
    OirSectionView section;
    OirStatus status;
    uint32_t i;
    int found_payload=0;

    if(out_root) *out_root=NULL;
    if(!out_root) return OIR_STATUS_INVALID_ARGUMENT;
    status=oir_artifact_open(artifact_bytes,artifact_length,&artifact);
    if(status!=OIR_STATUS_OK) return status;
    if(artifact.header.artifact_kind!=OIR_KIND_AST ||
        artifact.header.payload_schema_major!=1u ||
        artifact.header.payload_schema_minor!=0u ||
        artifact.header.validity!=OIR_VALIDITY_VALID) {
        return OIR_STATUS_INVALID_HEADER;
    }

    for(i=0;i<artifact.section_count;i++) {
        status=oir_artifact_get_section(&artifact,i,&section);
        if(status!=OIR_STATUS_OK) return status;
        if(section.type==OIR_SECTION_PAYLOAD) {
            if(found_payload) return OIR_STATUS_DUPLICATE_PAYLOAD;
            found_payload=1;
            if(section.version!=1u) return OIR_STATUS_UNSUPPORTED_VERSION;
            if((section.flags&OIR_SECTION_FLAG_REQUIRED)==0u) {
                return OIR_STATUS_INVALID_SECTION;
            }
            if(section.length>SIZE_MAX) return OIR_STATUS_OVERFLOW;
            return oir_ast_frontend_read_payload(section.data,(size_t)section.length,
                node_storage,node_capacity,incoming_counts,incoming_capacity,
                visited,visited_capacity,graph_stack,graph_stack_capacity,out_root);
        }
    }
    return OIR_STATUS_MISSING_PAYLOAD;
}
