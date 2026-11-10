#include "module_interface.h"
#include "../oir/oir_semantic_ast_artifact.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char* oir_interface_copy_name(const uint8_t* name,uint32_t length) {
    char* copy;
    if(!name || length==0u) return NULL;
    if(length>SEMANTIC_RESULT_MAX_RECORDS*16u) return NULL;
    if(memchr(name,0,length)) return NULL;
    copy=(char*)malloc((size_t)length+1u);
    if(copy) { memcpy(copy,name,length); copy[length]='\0'; }
    return copy;
}

static OccModuleInterfaceStatus append_oir_type(
    const OirSemanticTypesView* types,OccModuleInterface* out,
    uint8_t* selected,uint32_t initial_id
) {
    uint32_t current=initial_id;
    while(current) {
        OirSemanticType source;
        const uint8_t* name=NULL;
        OccModuleInterfaceType* grown;
        OccModuleInterfaceType* target;
        OirStatus status;
        if(current>types->record_count) return OCC_MODULE_INTERFACE_INVALID_SEMANTIC_RESULT;
        if(selected[current]) break;
        status=oir_semantic_types_get(types,current-1u,&source,&name);
        if(status!=OIR_STATUS_OK || source.type_id!=current)
            return OCC_MODULE_INTERFACE_INVALID_SEMANTIC_RESULT;
        if(out->type_count>=SEMANTIC_RESULT_MAX_RECORDS)
            return OCC_MODULE_INTERFACE_LIMIT_EXCEEDED;
        grown=(OccModuleInterfaceType*)realloc(out->types,
            (out->type_count+1u)*sizeof(*grown));
        if(!grown) return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
        out->types=grown; target=&out->types[out->type_count];
        memset(target,0,sizeof(*target));
        target->source_type_id=source.type_id; target->kind=source.kind;
        target->flags=source.flags; target->referenced_symbol_id=source.referenced_symbol_id;
        target->base_type_id=source.base_type_id;
        if(source.name_length) {
            target->name=oir_interface_copy_name(name,source.name_length);
            if(!target->name) return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
        }
        ++out->type_count; selected[current]=1u;
        current=source.base_type_id;
    }
    return OCC_MODULE_INTERFACE_OK;
}

static OccModuleInterfaceStatus append_oir_symbol(
    const OirSemanticSymbolsView* symbols,uint32_t index,
    OccModuleInterface* out,uint8_t* selected
) {
    OirSemanticSymbol source;
    const uint8_t* name=NULL;
    OccModuleInterfaceSymbol* grown;
    OccModuleInterfaceSymbol* target;
    OirStatus status=oir_semantic_symbols_get(symbols,index,&source,&name);
    if(status!=OIR_STATUS_OK || !source.symbol_id ||
       source.symbol_id>symbols->record_count || source.name_length==0u)
        return OCC_MODULE_INTERFACE_INVALID_SEMANTIC_RESULT;
    if(out->symbol_count>=SEMANTIC_RESULT_MAX_RECORDS)
        return OCC_MODULE_INTERFACE_LIMIT_EXCEEDED;
    grown=(OccModuleInterfaceSymbol*)realloc(out->symbols,
        (out->symbol_count+1u)*sizeof(*grown));
    if(!grown) return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
    out->symbols=grown; target=&out->symbols[out->symbol_count];
    memset(target,0,sizeof(*target));
    target->source_symbol_id=source.symbol_id; target->scope_id=source.scope_id;
    target->declaration_node_id=source.declaration_node_id;
    target->type_id=source.type_id; target->kind=source.kind; target->flags=source.flags;
    target->name=oir_interface_copy_name(name,source.name_length);
    if(!target->name) return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
    ++out->symbol_count; selected[source.symbol_id]=1u;
    return OCC_MODULE_INTERFACE_OK;
}

static OccModuleInterfaceStatus append_oir_relation(
    const OirSemanticRelationsView* relations,uint32_t index,
    OccModuleInterface* out
) {
    OirSemanticRelation source;
    const uint8_t* name=NULL;
    OccModuleInterfaceRelation* grown;
    OccModuleInterfaceRelation* target;
    OirStatus status=oir_semantic_relations_get(relations,index,&source,&name);
    if(status!=OIR_STATUS_OK) return OCC_MODULE_INTERFACE_INVALID_SEMANTIC_RESULT;
    if(out->relation_count>=SEMANTIC_RESULT_MAX_RECORDS)
        return OCC_MODULE_INTERFACE_LIMIT_EXCEEDED;
    grown=(OccModuleInterfaceRelation*)realloc(out->relations,
        (out->relation_count+1u)*sizeof(*grown));
    if(!grown) return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
    out->relations=grown; target=&out->relations[out->relation_count];
    memset(target,0,sizeof(*target));
    target->owner_source_symbol_id=source.owner_symbol_id;
    target->related_source_symbol_id=source.related_symbol_id;
    target->related_node_id=source.related_node_id; target->type_id=source.type_id;
    target->ordinal=source.ordinal; target->kind=source.kind; target->flags=source.flags;
    if(source.name_length) {
        target->name=oir_interface_copy_name(name,source.name_length,0);
        if(!target->name) return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
    }
    ++out->relation_count;
    return OCC_MODULE_INTERFACE_OK;
}

OccModuleInterfaceStatus occ_module_interface_build_from_oir_view(
    const char* canonical_identity,
    const struct OirSemanticAstArtifactView* artifact,
    OccModuleInterface* out
) {
    uint8_t *selected_symbols=NULL,*selected_types=NULL;
    uint32_t i;
    OccModuleInterfaceStatus result=OCC_MODULE_INTERFACE_INVALID_SEMANTIC_RESULT;
    if(!out || !canonical_identity || !canonical_identity[0] || !artifact)
        return OCC_MODULE_INTERFACE_INVALID_ARGUMENT;
    memset(out,0,sizeof(*out));
    if(artifact->envelope.header.artifact_kind!=OIR_KIND_SEMANTIC_AST ||
       artifact->envelope.header.producer_stage_id!=OCC_STAGE_SEMANTIC ||
       artifact->envelope.header.validity!=OIR_VALIDITY_VALID ||
       artifact->envelope.header.payload_schema_major!=OIR_SEMANTIC_AST_SCHEMA_MAJOR ||
       artifact->envelope.header.payload_schema_minor>OIR_SEMANTIC_AST_SCHEMA_MINOR ||
       !artifact->symbols.bytes || !artifact->types.bytes || !artifact->relations.bytes)
        return OCC_MODULE_INTERFACE_INVALID_SEMANTIC_RESULT;
    out->canonical_identity=interface_strdup(canonical_identity);
    if(!out->canonical_identity) return OCC_MODULE_INTERFACE_OUT_OF_MEMORY;
    out->schema_major=OCC_MODULE_INTERFACE_SCHEMA_MAJOR;
    out->schema_minor=OCC_MODULE_INTERFACE_SCHEMA_MINOR;
    selected_symbols=(uint8_t*)calloc((size_t)artifact->symbols.record_count+1u,1u);
    selected_types=(uint8_t*)calloc((size_t)artifact->types.record_count+1u,1u);
    if(!selected_symbols || !selected_types) {
        result=OCC_MODULE_INTERFACE_OUT_OF_MEMORY; goto done;
    }
    for(i=0u;i<artifact->symbols.record_count;i++) {
        OirSemanticSymbol symbol;
        OirStatus status=oir_semantic_symbols_get(&artifact->symbols,i,&symbol,NULL);
        if(status!=OIR_STATUS_OK || !symbol.symbol_id ||
           symbol.symbol_id>artifact->symbols.record_count) goto done;
        if((symbol.flags&(OIR_SEMANTIC_SYMBOL_FLAG_PUBLIC|
            OIR_SEMANTIC_SYMBOL_FLAG_EXPORTED))==0u) continue;
        result=append_oir_symbol(&artifact->symbols,i,out,selected_symbols);
        if(result!=OCC_MODULE_INTERFACE_OK) goto done;
        result=append_oir_type(&artifact->types,out,selected_types,symbol.type_id);
        if(result!=OCC_MODULE_INTERFACE_OK) goto done;
    }
    for(i=0u;i<artifact->relations.record_count;i++) {
        OirSemanticRelation relation;
        OirStatus status=oir_semantic_relations_get(&artifact->relations,i,&relation,NULL);
        if(status!=OIR_STATUS_OK || !relation.owner_symbol_id ||
           relation.owner_symbol_id>artifact->symbols.record_count) goto done;
        if(!selected_symbols[relation.owner_symbol_id]) continue;
        if(relation.related_symbol_id &&
           (relation.related_symbol_id>artifact->symbols.record_count ||
            !selected_symbols[relation.related_symbol_id])) continue;
        result=append_oir_relation(&artifact->relations,i,out);
        if(result!=OCC_MODULE_INTERFACE_OK) goto done;
        result=append_oir_type(&artifact->types,out,selected_types,relation.type_id);
        if(result!=OCC_MODULE_INTERFACE_OK) goto done;
    }
    result=OCC_MODULE_INTERFACE_OK;
done:
    free(selected_symbols); free(selected_types);
    if(result!=OCC_MODULE_INTERFACE_OK) occ_module_interface_destroy(out);
    return result;
}
