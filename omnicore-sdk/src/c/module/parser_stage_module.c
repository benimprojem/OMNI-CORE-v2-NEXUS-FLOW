#include "occ_module_api.h"
#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include "oir_payload.h"
#include "oir_token_stream.h"
#include "oir_diagnostics.h"
#include "oir_ast_artifact.h"
#include "oir_ast_frontend_index.h"
#include "oir_ast_node_table.h"

#include <stdlib.h>
#include <string.h>

static const OccStageModuleV1 g_parser_module;

#define PARSER_STAGE_MAX_INDEX_CAPACITY ((size_t)2097152u)
#define PARSER_STAGE_MAX_TOKEN_COUNT 1000000u

static OirStatus find_payload(
    const OirArtifactView* artifact, uint16_t type, OirSectionView* out
) {
    uint32_t i;
    int found = 0;
    for(i=0u;i<artifact->section_count;i++) {
        OirSectionView section;
        OirStatus status=oir_artifact_get_section(artifact,i,&section);
        if(status!=OIR_STATUS_OK) return status;
        if(section.type==type) {
            if(found) return OIR_STATUS_DUPLICATE_PAYLOAD;
            *out=section;
            found=1;
        }
    }
    return found ? OIR_STATUS_OK : OIR_STATUS_MISSING_PAYLOAD;
}

static int report_parser_diagnostics(
    const OccModuleInvocationV1* invocation,
    const OccDiagnosticCollector* diagnostics
) {
    size_t diagnostic_index;
    for(diagnostic_index=0u;
        diagnostic_index<occ_diagnostics_count(diagnostics);
        ++diagnostic_index) {
        OccDiagnosticView view;
        OccModuleDiagnosticV1 diagnostic;
        if(!occ_diagnostics_get(diagnostics,diagnostic_index,&view)) return 0;
        memset(&diagnostic,0,sizeof(diagnostic));
        diagnostic.struct_size=(uint32_t)sizeof(diagnostic);
        diagnostic.code=view.record->code;
        diagnostic.message_id=view.record->message_id;
        diagnostic.severity=view.record->severity;
        diagnostic.span=view.record->span;
        diagnostic.message=view.message;
        diagnostic.message_length=view.record->message_length;
        diagnostic.hint=view.hint;
        diagnostic.hint_length=view.record->hint_length;
        if(!invocation->diagnostic_sink.report(
            invocation->diagnostic_sink.user_context,&diagnostic)) return 0;
    }
    return 1;
}

static uint32_t OCC_MODULE_CALL parser_stage_process(
    const OccModuleInvocationV1* invocation,
    OccModuleResultV1* result
) {
    OirArtifactView source_artifact, token_artifact;
    OirSectionView source_section, token_section, diag_section;
    OirSourceUnitView source;
    OirTokenStreamPayloadView tokens_view;
    OirDiagnosticsPayloadView diagnostics_view;
    OccDiagnosticCollector lexer_diagnostics;
    ParserContext* parser=NULL;
    Token* tokens=NULL;
    char* filename=NULL;
    OirAstPointerIndexEntry* index=NULL;
    OirAstNodeRecord* records=NULL;
    uint8_t *field_data=NULL,*string_table=NULL,*payload=NULL;
    size_t index_capacity=1024u, payload_capacity=0u, artifact_required=0u;
    size_t field_capacity=0u,string_capacity=0u, artifact_written=0u;
    uint32_t node_count=0u, diag_count=0u;
    OirAstFrontendRequirements requirements;
    ASTNode* root;
    OirStatus status;
    uint32_t module_status;
    size_t i;
    int diagnostics_found=0;

    occ_diagnostics_init(&lexer_diagnostics);

    result->bytes_written=0u;
    result->required_output_capacity=0u;
    result->output_validity=OIR_VALIDITY_NONE;
    result->failure_code=0u;
    result->diagnostics_count=0u;

    module_status=occ_module_validate_invocation_v1(
        &g_parser_module.descriptor,invocation,result);
    if(module_status!=OCC_MODULE_STATUS_OK) return module_status;
    if(invocation->input_count!=2u) return OCC_MODULE_STATUS_INVALID_INVOCATION;

    if(oir_artifact_open(invocation->inputs[0].bytes,
        (size_t)invocation->inputs[0].length,&source_artifact)!=OIR_STATUS_OK ||
       source_artifact.header.artifact_kind!=OIR_KIND_SOURCE_UNIT ||
       oir_artifact_open(invocation->inputs[1].bytes,
        (size_t)invocation->inputs[1].length,&token_artifact)!=OIR_STATUS_OK ||
       token_artifact.header.artifact_kind!=OIR_KIND_TOKEN_STREAM) {
        return OCC_MODULE_STATUS_INVALID_INVOCATION;
    }
    if(source_artifact.header.payload_schema_major!=OIR_SOURCE_UNIT_SCHEMA_MAJOR ||
       source_artifact.header.payload_schema_minor!=OIR_SOURCE_UNIT_SCHEMA_MINOR ||
       token_artifact.header.payload_schema_major!=OIR_TOKEN_STREAM_SCHEMA_MAJOR ||
       token_artifact.header.payload_schema_minor!=OIR_TOKEN_STREAM_SCHEMA_MINOR ||
       token_artifact.header.producer_stage_id!=OCC_STAGE_LEXER ||
       source_artifact.header.job_id!=token_artifact.header.job_id ||
       source_artifact.header.job_id!=invocation->job_id) {
        return OCC_MODULE_STATUS_INVALID_INVOCATION;
    }
    if(find_payload(&source_artifact,OIR_SECTION_PAYLOAD,&source_section)!=OIR_STATUS_OK ||
       oir_source_unit_open(source_section.data,(size_t)source_section.length,&source)!=OIR_STATUS_OK ||
       find_payload(&token_artifact,OIR_SECTION_PAYLOAD,&token_section)!=OIR_STATUS_OK) {
        return OCC_MODULE_STATUS_INVALID_INVOCATION;
    }
    status=find_payload(&token_artifact,OIR_SECTION_DIAGNOSTICS,&diag_section);
    if(status==OIR_STATUS_OK) {
        diagnostics_found=1;
        if(oir_diagnostics_payload_open(diag_section.data,
            (size_t)diag_section.length,&diagnostics_view)!=OIR_STATUS_OK) {
            return OCC_MODULE_STATUS_INVALID_INVOCATION;
        }
        diag_count=diagnostics_view.record_count;
    } else if(status!=OIR_STATUS_MISSING_PAYLOAD) {
        return OCC_MODULE_STATUS_INVALID_INVOCATION;
    }
#if SIZE_MAX < UINT64_MAX
    if(source.source_length>(uint64_t)SIZE_MAX) return OCC_MODULE_STATUS_INVALID_INVOCATION;
#endif
    if((uint64_t)source.path_length>=(uint64_t)SIZE_MAX) return OCC_MODULE_STATUS_INVALID_INVOCATION;
    if(oir_token_stream_open(token_section.data,(size_t)token_section.length,
        source.source_id,source.source_length,diag_count,&tokens_view)!=OIR_STATUS_OK ||
       tokens_view.token_count > PARSER_STAGE_MAX_TOKEN_COUNT) {
        return OCC_MODULE_STATUS_INVALID_INVOCATION;
    }

    filename=(char*)malloc((size_t)source.path_length+1u);
    tokens=(Token*)calloc(tokens_view.token_count,sizeof(*tokens));
    if(!filename || !tokens) goto allocation_failure;
    if(source.path_length) memcpy(filename,source.path,source.path_length);
    filename[source.path_length]='\0';

    for(i=0u;i<tokens_view.token_count;i++) {
        OirTokenStreamRecordView record;
        const char* message=NULL;
        uint32_t message_length=0u;
        if(oir_token_stream_get(&tokens_view,(uint32_t)i,&record)!=OIR_STATUS_OK)
            goto invalid_input;
        if(record.diagnostic_id) {
            OirDiagnosticEntryView entry;
            if(!diagnostics_found ||
               oir_diagnostics_payload_get(&diagnostics_view,
                    record.diagnostic_id-1u,&entry)!=OIR_STATUS_OK) goto invalid_input;
            message=entry.message;
            message_length=entry.message_length;
        }
        if(oir_token_stream_record_to_lexer_token(&record,source.source,
            source.source_length,message,message_length,&tokens[i])!=OIR_STATUS_OK)
            goto invalid_input;
    }

    parser=parser_init_source(filename,source.source,(size_t)source.source_length);
    if(!parser) goto allocation_failure;
    parser_attach_diagnostics(parser,&lexer_diagnostics,OCC_STAGE_PARSER);
    if(!parser_attach_token_stream(parser,tokens,tokens_view.token_count))
        goto invalid_input;
    {
        int parse_ok=parser_run(parser)==0 && parser->ast_root!=NULL;
        if(occ_diagnostics_count(&lexer_diagnostics)>UINT32_MAX-diag_count)
            goto allocation_failure;
        result->diagnostics_count=diag_count+(uint32_t)occ_diagnostics_count(&lexer_diagnostics);
        if(!parse_ok) {
            /* A failed parse will not request an output-buffer retry. */
            if(!report_parser_diagnostics(invocation,&lexer_diagnostics))
                goto report_failure;
            result->output_validity=OIR_VALIDITY_NONE;
            result->failure_code=2006u;
            goto parse_failure;
        }
    }
    root=(ASTNode*)parser->ast_root;

    for(;;) {
        index=(OirAstPointerIndexEntry*)calloc(index_capacity,sizeof(*index));
        if(!index) goto allocation_failure;
        status=oir_ast_frontend_index_build(root,index,index_capacity,&node_count);
        if(status==OIR_STATUS_OK) break;
        free(index); index=NULL;
        if(status!=OIR_STATUS_BUFFER_TOO_SMALL ||
            index_capacity>=PARSER_STAGE_MAX_INDEX_CAPACITY) goto serialization_failure;
        index_capacity*=2u;
    }
    status=oir_ast_frontend_measure(root,index,index_capacity,node_count,&requirements);
    if(status!=OIR_STATUS_OK) goto serialization_failure;
    field_capacity=requirements.field_data_length;
    string_capacity=requirements.string_table_length;
    payload_capacity=requirements.payload_size;
    if(payload_capacity>SIZE_MAX-OIR_HEADER_SIZE-OIR_SECTION_HEADER_SIZE)
        goto serialization_failure;
    artifact_required=OIR_HEADER_SIZE+OIR_SECTION_HEADER_SIZE+payload_capacity;
    result->required_output_capacity=(uint64_t)artifact_required;
    if(invocation->output_capacity<(uint64_t)artifact_required) {
        result->output_validity=OIR_VALIDITY_NONE;
        result->failure_code=0u;
        module_status=OCC_MODULE_STATUS_NEED_OUTPUT_BUFFER;
        goto cleanup;
    }

    records=(OirAstNodeRecord*)calloc(node_count,sizeof(*records));
    field_data=field_capacity?(uint8_t*)malloc(field_capacity):NULL;
    string_table=string_capacity?(uint8_t*)malloc(string_capacity):NULL;
    payload=(uint8_t*)malloc(payload_capacity);
    if(!records ||
       (field_capacity&&!field_data) || (string_capacity&&!string_table) || !payload)
        goto allocation_failure;
    status=oir_ast_artifact_write_frontend_ast(root,index,index_capacity,node_count,
        records,node_count,field_data,field_capacity,string_table,string_capacity,
        payload,payload_capacity,invocation->output_buffer,
        (size_t)invocation->output_capacity,invocation->job_id,
        &artifact_written,&requirements);
    if(status!=OIR_STATUS_OK) goto serialization_failure;
    /*
     * Report only after output capacity has been confirmed and the artifact
     * has been written. NEED_OUTPUT_BUFFER retries must not duplicate reports.
     */
    if(!report_parser_diagnostics(invocation,&lexer_diagnostics)) {
        result->failure_code=6002u;
        result->bytes_written=0u;
        result->output_validity=OIR_VALIDITY_NONE;
        module_status=OCC_MODULE_STATUS_FAILED;
        goto cleanup;
    }
    result->bytes_written=(uint64_t)artifact_written;
    result->required_output_capacity=0u;
    result->output_validity=OIR_VALIDITY_VALID;
    result->failure_code=0u;
    result->diagnostics_count=diag_count+(uint32_t)occ_diagnostics_count(&lexer_diagnostics);
    module_status=OCC_MODULE_STATUS_OK;
    goto cleanup;

invalid_input:
    result->failure_code=6001u;
    module_status=OCC_MODULE_STATUS_INVALID_INVOCATION;
    goto cleanup;
report_failure:
    result->failure_code=6002u;
    module_status=OCC_MODULE_STATUS_FAILED;
    goto cleanup;
allocation_failure:
    result->failure_code=6003u;
    module_status=OCC_MODULE_STATUS_FAILED;
    goto cleanup;
serialization_failure:
    result->failure_code=6004u;
    module_status=OCC_MODULE_STATUS_FAILED;
    goto cleanup;
parse_failure:
    module_status=OCC_MODULE_STATUS_FAILED;
cleanup:
    if(parser) parser_destroy(parser);
    if(filename) free(filename);
    if(tokens) free(tokens);
    if(index) free(index);
    if(records) free(records);
    if(field_data) free(field_data);
    if(string_table) free(string_table);
    if(payload) free(payload);
    occ_diagnostics_destroy(&lexer_diagnostics);
    return module_status;
}

static const OccStageModuleV1 g_parser_module = {
    (uint32_t)sizeof(OccStageModuleV1), 0u,
    {
        (uint32_t)sizeof(OccStageDescriptorV1),
        OCC_MODULE_ABI_MAJOR,OCC_MODULE_ABI_MINOR,
        0x00010001u,OCC_STAGE_PARSER,
        1u,0u,0u,0u,
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_SOURCE_UNIT) |
            OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_TOKEN_STREAM),
        OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_SOURCE_UNIT) |
            OCC_MODULE_ARTIFACT_KIND_MASK(OIR_KIND_TOKEN_STREAM),
        OIR_KIND_AST,2u,2u,0u,
        (uint64_t)OCC_MODULE_CAP_DETERMINISTIC |
            (uint64_t)OCC_MODULE_CAP_DIAGNOSTIC_REPORTING,
        "nexsus-parser"
    },
    parser_stage_process
};

OCC_MODULE_EXPORT const OccStageModuleV1* OCC_MODULE_CALL
occ_get_stage_module_v1(void) {
    return &g_parser_module;
}
