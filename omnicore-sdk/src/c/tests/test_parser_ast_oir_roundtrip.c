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

static int compare_span_tree(const ASTNode* a,const ASTNode* b,unsigned depth){
    if(depth>512u)return 0;
    while(a&&b){
        const OccSourceSpan* x=&a->source_span,*y=&b->source_span;
        if(x->has_location!=y->has_location)return 0;
        if(x->has_location&&(x->start_offset!=y->start_offset||x->end_offset!=y->end_offset||
            x->start_line!=y->start_line||x->start_column!=y->start_column||
            x->end_line!=y->end_line||x->end_column!=y->end_column))return 0;
        if(oir_ast_frontend_child_field_count(a->type)!=oir_ast_frontend_child_field_count(b->type))return 0;
        for(uint16_t field=1u;field<=oir_ast_frontend_child_field_count(a->type);++field)
            if(!compare_span_tree(oir_ast_frontend_child_at(a,field),
                oir_ast_frontend_child_at(b,field),depth+1u))return 0;
        a=a->next;b=b->next;
    }
    return a==NULL&&b==NULL;
}

/* Every parser-produced AST node must retain its originating source range. */
static const ASTNode* first_missing_source_span(const ASTNode* node,unsigned depth) {
    if(depth>512u)return node;
    while(node) {
        uint16_t field;
        if(!node->source_span.has_location)return node;
        for(field=1u;field<=oir_ast_frontend_child_field_count(node->type);++field) {
            const ASTNode* missing=first_missing_source_span(
                oir_ast_frontend_child_at(node,field),depth+1u);
            if(missing)return missing;
        }
        node=node->next;
    }
    return NULL;
}

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

static int test_struct_literal_oir_roundtrip(void) {
    static const char source[] =
        "f:main() {"
        " struct: Point { v:x!i32, v:y!i32 }"
        " v:p!Point = Point { y = 2, x = 1 };"
        "}";
    ParserContext* parser=parser_init_source("struct-literal-oir.nxf",
        (const uint8_t*)source,sizeof(source)-1u);
    uint8_t *first=NULL,*second=NULL;
    size_t first_length=0u,second_length=0u;
    ASTNode **storage=NULL,*decoded=NULL;
    uint32_t *incoming=NULL;
    uint8_t *visited=NULL;
    OirAstGraphStackEntry *stack=NULL;
    OirAstNodeTableView view;
    OirStatus status;
    int ok=0,stage=0;
    if(!parser)return 0;
    stage=1;
    if(parser_run(parser)!=PARSER_STATUS_OK || parser->error_count ||
        !parser->ast_root || !ast_validate((ASTNode*)parser->ast_root,NULL))goto done;
    if(first_missing_source_span((ASTNode*)parser->ast_root,0u))goto done;
    stage=2;
    status=encode_ast((ASTNode*)parser->ast_root,&first,&first_length);
    if(status!=OIR_STATUS_OK ||
       oir_ast_node_table_open(first,first_length,&view)!=OIR_STATUS_OK)goto done;
    stage=3;
    storage=(ASTNode**)calloc(view.node_count,sizeof(*storage));
    incoming=(uint32_t*)calloc(view.node_count,sizeof(*incoming));
    visited=(uint8_t*)calloc(view.node_count,sizeof(*visited));
    stack=(OirAstGraphStackEntry*)calloc(view.node_count,sizeof(*stack));
    if(!storage || !incoming || !visited || !stack)goto done;
    status=oir_ast_frontend_read_payload(first,first_length,storage,view.node_count,
        incoming,view.node_count,visited,view.node_count,stack,view.node_count,&decoded);
    if(status!=OIR_STATUS_OK || !decoded || !ast_validate(decoded,NULL))goto done;
    stage=4;
    {
        ASTFuncDecl* function=(ASTFuncDecl*)((ASTProgram*)decoded)->declarations;
        ASTBlock* body=function && function->body && function->body->type==AST_BLOCK
            ? (ASTBlock*)function->body : NULL;
        ASTStructDecl* declaration=body && body->statements &&
            body->statements->type==AST_STRUCT_DECL
                ? (ASTStructDecl*)body->statements : NULL;
        ASTVarDecl* variable=declaration && declaration->base.next &&
            declaration->base.next->type==AST_VAR_DECL
                ? (ASTVarDecl*)declaration->base.next : NULL;
        ASTStructLiteral* literal=variable && variable->initializer &&
            variable->initializer->type==AST_STRUCT_LITERAL
                ? (ASTStructLiteral*)variable->initializer : NULL;
        ASTStructFieldInit* y=literal && literal->fields
            ? (ASTStructFieldInit*)literal->fields : NULL;
        ASTStructFieldInit* x=y && y->base.next
            ? (ASTStructFieldInit*)y->base.next : NULL;
        if(!literal || !literal->type_name || strcmp(literal->type_name,"Point")!=0 ||
            !y || !x || strcmp(y->name,"y")!=0 || strcmp(x->name,"x")!=0 ||
            !y->value || y->value->type!=AST_LITERAL ||
            strcmp(((ASTLiteral*)y->value)->string_value,"2")!=0 ||
            !x->value || x->value->type!=AST_LITERAL ||
            strcmp(((ASTLiteral*)x->value)->string_value,"1")!=0 ||
            x->base.next || !y->base.source_span.has_location ||
            !x->base.source_span.has_location)goto done;
    }
    stage=5;
    status=encode_ast(decoded,&second,&second_length);
    if(status!=OIR_STATUS_OK || first_length!=second_length ||
        memcmp(first,second,first_length)!=0)goto done;
    ok=1;
done:
    if(!ok)fprintf(stderr,"Struct literal OIR AST roundtrip stage=%d\\n",stage);
    if(decoded)ast_destroy(decoded);
    free(first);free(second);free(storage);free(incoming);free(visited);free(stack);
    parser_destroy(parser);
    return ok;
}

static int test_select_scan_oir_roundtrip(void) {
    static const char source[] =
        "f:take(a![], b!i32[], third:i32[4]) {}"
        "f:main() {"
        " v:dynamic[]; v:fixed[5]!i32; v:matrix[3][3]!f32;"
        " v:converted = cast(5, i32); v:indexed = array[0];"
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
        "}?;"
        "f:array_return()!i32[n] {}"
        "!!=linkname = \"math.obj\";"
        "exf:add(a!i32, b!i32)!i32;";
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
    int ok=0,stage=0;
    if(!parser)return 0;
    stage=1;
    if(parser_run(parser)!=PARSER_STATUS_OK || parser->error_count ||
        !parser->ast_root || !ast_validate((ASTNode*)parser->ast_root,NULL))goto done;
    {
        const ASTNode* missing=first_missing_source_span((ASTNode*)parser->ast_root,0u);
        if(missing) {
            fprintf(stderr,"parser AST node kind %d is missing a source span\n",(int)missing->type);
            goto done;
        }
    }
    stage=2;
    status=encode_ast((ASTNode*)parser->ast_root,&first,&first_len);
    if(status!=OIR_STATUS_OK || oir_ast_node_table_open(first,first_len,&view)!=OIR_STATUS_OK)goto done;
    stage=3;
    storage=(ASTNode**)calloc(view.node_count,sizeof(*storage));
    incoming=(uint32_t*)calloc(view.node_count,sizeof(*incoming));
    visited=(uint8_t*)calloc(view.node_count,sizeof(*visited));
    stack=(OirAstGraphStackEntry*)calloc(view.node_count,sizeof(*stack));
    if(!storage || !incoming || !visited || !stack)goto done;
    stage=4;
    status=oir_ast_frontend_read_payload(first,first_len,storage,view.node_count,
        incoming,view.node_count,visited,view.node_count,stack,view.node_count,&decoded);
    if(status!=OIR_STATUS_OK || !decoded || !ast_validate(decoded,NULL))goto done;
    stage=5;
    status=encode_ast(decoded,&second,&second_len);
    if(status!=OIR_STATUS_OK || first_len!=second_len ||
        memcmp(first,second,first_len)!=0)goto done;
    /* Root and representative nested production ranges survive OIR decode. */
    stage=6;
    if(!compare_span_tree((ASTNode*)parser->ast_root,decoded,0u))goto done;
    if(!decoded->source_span.has_location ||
       decoded->source_span.start_offset!=0u ||
       decoded->source_span.end_offset!=sizeof(source)-1u ||
       decoded->source_span.start_line!=1u)goto done;
    {
        ASTFuncDecl* main_fn=(ASTFuncDecl*)((ASTProgram*)decoded)->declarations;
        if(!main_fn || !main_fn->base.source_span.has_location ||
           main_fn->base.source_span.end_offset<=main_fn->base.source_span.start_offset)
            goto done;
        {
            stage=7;
            ASTFuncDecl* array_fn=(ASTFuncDecl*)main_fn->base.next;
            ASTArrayDimension* dimension;
            if(!array_fn || array_fn->base.type!=AST_FUNC_DECL) goto done;
            array_fn=(ASTFuncDecl*)array_fn->base.next;
            if(!array_fn || array_fn->base.type!=AST_FUNC_DECL ||
               !array_fn->return_array_type ||
               array_fn->return_array_type->type!=AST_ARRAY_TYPE) goto done;
            dimension=(ASTArrayDimension*)((ASTArrayType*)array_fn->return_array_type)->dimensions;
            if(!dimension || !dimension->length ||
               dimension->length->type!=AST_IDENTIFIER ||
               strcmp(((ASTIdentifier*)dimension->length)->name,"n")!=0 ||
               dimension->base.next) goto done;
            {
                ASTFuncDecl* external=(ASTFuncDecl*)array_fn->base.next;
                if(!external || external->base.type!=AST_EXTERN_DECL ||
                   !external->linkname || strcmp(external->linkname,"math.obj")!=0 ||
                   external->base.next) goto done;
            }
        }
    }
    ok=1;
done:
    if(!ok)fprintf(stderr,"extended AST OIR roundtrip stage=%d\n",stage);
    if(decoded)ast_destroy(decoded);
    free(first); free(second); free(storage); free(incoming); free(visited); free(stack);
    parser_destroy(parser);
    return ok;
}

static int test_fast_exec_asm_oir_roundtrip(void) {
    ASTProgram* program=(ASTProgram*)ast_create(AST_PROGRAM);
    ASTMustDecl* dependency=(ASTMustDecl*)ast_create(AST_MUST_DECL);
    ASTIdentifier* dependency_source=(ASTIdentifier*)ast_create(AST_IDENTIFIER);
    ASTFuncDecl* function=(ASTFuncDecl*)ast_create(AST_FUNC_DECL);
    ASTIdentifier* parameter=(ASTIdentifier*)ast_create(AST_IDENTIFIER);
    ASTVariadicParam* variadic=(ASTVariadicParam*)ast_create(AST_VARIADIC_PARAM);
    ASTBlock* body=(ASTBlock*)ast_create(AST_BLOCK);
    ASTFastExecBlock* fast=(ASTFastExecBlock*)ast_create(AST_FASTEXEC_BLOCK);
    ASTAsmLabel* label=(ASTAsmLabel*)ast_create(AST_ASM_LABEL);
    ASTCall* call=(ASTCall*)ast_create(AST_FUNC_CALL);
    ASTIdentifier* callee=(ASTIdentifier*)ast_create(AST_IDENTIFIER);
    ASTNamedArg* named=(ASTNamedArg*)ast_create(AST_NAMED_ARG);
    ASTIdentifier* named_value=(ASTIdentifier*)ast_create(AST_IDENTIFIER);
    const char* raw="mov rax, %value\r\nadd rax, 1 ; preserve %x:reg";
    uint8_t *first=NULL,*second=NULL;
    size_t first_length=0u,second_length=0u;
    OirAstNodeTableView view;
    ASTNode **storage=NULL,*decoded=NULL;
    uint32_t *incoming=NULL;
    uint8_t *visited=NULL;
    OirAstGraphStackEntry* stack=NULL;
    OirStatus status;
    int ok=0;
    if(!program || !dependency || !dependency_source || !function || !parameter ||
        !variadic || !body || !fast || !label || !call || !callee || !named || !named_value) goto done;

    dependency->name=(char*)malloc(sizeof("runtime"));
    dependency_source->name=(char*)malloc(sizeof("runtime.dll"));
    function->name=(char*)malloc(sizeof("main"));
    function->return_type=(char*)malloc(sizeof("i32"));
    parameter->name=(char*)malloc(sizeof("value"));
    label->label=(char*)malloc(sizeof("ADD"));
    label->source_text=(char*)malloc(strlen(raw)+1u);
    callee->name=(char*)malloc(sizeof("consume"));
    named->name=(char*)malloc(sizeof("input"));
    named_value->name=(char*)malloc(sizeof("value"));
    if(!dependency->name || !dependency_source->name || !function->name ||
        !function->return_type || !parameter->name || !label->label ||
        !label->source_text || !callee->name || !named->name || !named_value->name) goto done;
    strcpy(dependency->name,"runtime");
    strcpy(dependency_source->name,"runtime.dll");
    strcpy(function->name,"main");
    strcpy(function->return_type,"i32");
    strcpy(parameter->name,"value");
    strcpy(label->label,"ADD");
    memcpy(label->source_text,raw,strlen(raw)+1u);
    label->source_length=strlen(raw);
    strcpy(callee->name,"consume");
    strcpy(named->name,"input");
    strcpy(named_value->name,"value");

    dependency->source=(ASTNode*)dependency_source; dependency_source=NULL;
    parameter->base.next=(ASTNode*)variadic; variadic=NULL;
    function->params=(ASTNode*)parameter; parameter=NULL;
    named->value=(ASTNode*)named_value; named_value=NULL;
    call->callee=(ASTNode*)callee; callee=NULL;
    call->args=(ASTNode*)named; named=NULL;
    fast->statements=(ASTNode*)label; label=NULL;
    fast->base.next=(ASTNode*)call; call=NULL;
    body->statements=(ASTNode*)fast; fast=NULL;
    function->body=(ASTNode*)body; body=NULL;
    dependency->base.next=(ASTNode*)function; function=NULL;
    program->declarations=(ASTNode*)dependency; dependency=NULL;

    if(!ast_validate((ASTNode*)program,NULL)) goto done;
    status=encode_ast((ASTNode*)program,&first,&first_length);
    if(status!=OIR_STATUS_OK) goto done;
    status=oir_ast_node_table_open(first,first_length,&view);
    if(status!=OIR_STATUS_OK) goto done;
    storage=(ASTNode**)calloc(view.node_count,sizeof(*storage));
    incoming=(uint32_t*)calloc(view.node_count,sizeof(*incoming));
    visited=(uint8_t*)calloc(view.node_count,sizeof(*visited));
    stack=(OirAstGraphStackEntry*)calloc(view.node_count,sizeof(*stack));
    if(!storage || !incoming || !visited || !stack) goto done;
    status=oir_ast_frontend_read_payload(first,first_length,storage,view.node_count,
        incoming,view.node_count,visited,view.node_count,stack,view.node_count,&decoded);
    if(status!=OIR_STATUS_OK || !decoded || !ast_validate(decoded,NULL)) goto done;
    {
        ASTMustDecl* dep=(ASTMustDecl*)((ASTProgram*)decoded)->declarations;
        ASTFuncDecl* fn=(ASTFuncDecl*)dep->base.next;
        ASTVariadicParam* var=(ASTVariadicParam*)((ASTIdentifier*)fn->params)->base.next;
        ASTFastExecBlock* fx=(ASTFastExecBlock*)((ASTBlock*)fn->body)->statements;
        ASTAsmLabel* asm_node=(ASTAsmLabel*)fx->statements;
        ASTCall* decoded_call=(ASTCall*)fx->base.next;
        ASTNamedArg* arg=(ASTNamedArg*)decoded_call->args;
        if(!dep->name || strcmp(dep->name,"runtime")!=0 ||
            !((ASTIdentifier*)dep->source)->name ||
            strcmp(((ASTIdentifier*)dep->source)->name,"runtime.dll")!=0 ||
            !var || var->base.type!=AST_VARIADIC_PARAM ||
            !asm_node || asm_node->base.type!=AST_ASM_LABEL ||
            strcmp(asm_node->label,"ADD")!=0 ||
            asm_node->source_length!=strlen(raw) ||
            memcmp(asm_node->source_text,raw,asm_node->source_length)!=0 ||
            !arg || strcmp(arg->name,"input")!=0 ||
            !((ASTIdentifier*)arg->value)->name ||
            strcmp(((ASTIdentifier*)arg->value)->name,"value")!=0) goto done;
    }
    status=encode_ast(decoded,&second,&second_length);
    if(status!=OIR_STATUS_OK || first_length!=second_length ||
        memcmp(first,second,first_length)!=0) goto done;
    ok=1;
done:
    if(decoded) ast_destroy(decoded);
    if(program) ast_destroy((ASTNode*)program);
    if(dependency) ast_destroy((ASTNode*)dependency);
    if(dependency_source) ast_destroy((ASTNode*)dependency_source);
    if(function) ast_destroy((ASTNode*)function);
    if(parameter) ast_destroy((ASTNode*)parameter);
    if(variadic) ast_destroy((ASTNode*)variadic);
    if(body) ast_destroy((ASTNode*)body);
    if(fast) ast_destroy((ASTNode*)fast);
    if(label) ast_destroy((ASTNode*)label);
    if(call) ast_destroy((ASTNode*)call);
    if(callee) ast_destroy((ASTNode*)callee);
    if(named) ast_destroy((ASTNode*)named);
    if(named_value) ast_destroy((ASTNode*)named_value);
    free(first); free(second); free(storage); free(incoming); free(visited); free(stack);
    return ok;
}

static int test_recursive_type_expr_oir_roundtrip(void) {
    ASTProgram* program=(ASTProgram*)ast_create(AST_PROGRAM);
    ASTTypeExpr* pointer=(ASTTypeExpr*)ast_create(AST_TYPE_EXPR);
    ASTTypeExpr* named=(ASTTypeExpr*)ast_create(AST_TYPE_EXPR);
    uint8_t *first=NULL,*second=NULL;
    size_t first_len=0u,second_len=0u;
    ASTNode **storage=NULL,*decoded=NULL;
    uint32_t *incoming=NULL;
    uint8_t *visited=NULL;
    OirAstGraphStackEntry *stack=NULL;
    OirAstNodeTableView view;
    OirStatus status;
    int ok=0;
    if(!program||!pointer||!named)goto done;
    pointer->kind=AST_TYPE_POINTER;
    pointer->child=(ASTNode*)named;
    named->kind=AST_TYPE_NAMED;
    named->name=(char*)malloc(4u);
    if(!named->name)goto done;
    memcpy(named->name,"i32",4u);
    program->declarations=(ASTNode*)pointer;
    if(!ast_validate((ASTNode*)program,NULL))goto done;
    status=encode_ast((ASTNode*)program,&first,&first_len);
    if(status!=OIR_STATUS_OK ||
       oir_ast_node_table_open(first,first_len,&view)!=OIR_STATUS_OK)goto done;
    storage=(ASTNode**)calloc(view.node_count,sizeof(*storage));
    incoming=(uint32_t*)calloc(view.node_count,sizeof(*incoming));
    visited=(uint8_t*)calloc(view.node_count,sizeof(*visited));
    stack=(OirAstGraphStackEntry*)calloc(view.node_count,sizeof(*stack));
    if(!storage||!incoming||!visited||!stack)goto done;
    status=oir_ast_frontend_read_payload(first,first_len,storage,view.node_count,
        incoming,view.node_count,visited,view.node_count,stack,view.node_count,&decoded);
    if(status!=OIR_STATUS_OK||!decoded||!ast_validate(decoded,NULL))goto done;
    {
        ASTTypeExpr* restored=(ASTTypeExpr*)((ASTProgram*)decoded)->declarations;
        if(!restored||restored->kind!=AST_TYPE_POINTER||!restored->child||
           ((ASTTypeExpr*)restored->child)->kind!=AST_TYPE_NAMED||
           !((ASTTypeExpr*)restored->child)->name||
           strcmp(((ASTTypeExpr*)restored->child)->name,"i32")!=0)goto done;
    }
    status=encode_ast(decoded,&second,&second_len);
    if(status!=OIR_STATUS_OK||first_len!=second_len||
       memcmp(first,second,first_len)!=0)goto done;
    ok=1;
done:
    if(decoded)ast_destroy(decoded);
    else if(program)ast_destroy((ASTNode*)program);
    free(first);free(second);free(storage);free(incoming);free(visited);free(stack);
    return ok;
}

int main(int argc,char** argv) {
    if(!test_struct_literal_oir_roundtrip()) { fprintf(stderr,"Struct literal OIR AST round-trip failed.\\n"); return 1; }
    if(!test_select_scan_oir_roundtrip()) { fprintf(stderr,"Extended AST OIR round-trip failed.\\n"); return 1; }
    if(!test_fast_exec_asm_oir_roundtrip()) { fprintf(stderr,"FASTEXEC ASM OIR round-trip failed.\\n"); return 1; }
    if(!test_recursive_type_expr_oir_roundtrip()) { fprintf(stderr,"Recursive type-expression OIR round-trip failed.\\n"); return 1; }
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
        ((ASTNode*)parser->ast_root)->type!=AST_PROGRAM ||
        !ast_validate((ASTNode*)parser->ast_root,NULL)) {
        fprintf(stderr,"Could not parse AST round-trip fixture.\n");
        goto cleanup;
    }

    {
        const ASTNode* missing=first_missing_source_span((ASTNode*)parser->ast_root,0u);
        if(missing) {
            fprintf(stderr,"AST round-trip fixture node kind %d is missing a source span\\n",
                (int)missing->type);
            goto cleanup;
        }
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
