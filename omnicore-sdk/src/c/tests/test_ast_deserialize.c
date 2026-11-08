#include "ast.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int rejects_bytes(const void* bytes, size_t length) {
    FILE* stream = tmpfile();
    if (!stream) return 2;

    if (fwrite(bytes, 1, length, stream) != length) {
        fclose(stream);
        return 2;
    }

    rewind(stream);
    ASTNode* root = ast_deserialize(stream);
    fclose(stream);

    if (root) {
        ast_destroy(root);
        return 1;
    }
    return 0;
}

static char* duplicate_text(const char* text) {
    size_t length = strlen(text);
    char* copy = (char*)malloc(length + 1u);
    if (!copy) return NULL;
    memcpy(copy, text, length + 1u);
    return copy;
}

static int assign_text(char** destination, const char* text) {
    *destination = duplicate_text(text);
    return *destination != NULL;
}

static ASTNode* make_identifier_fixture(const char* name) {
    ASTIdentifier* node = (ASTIdentifier*)ast_create(AST_IDENTIFIER);
    if (!node) return NULL;
    if (!assign_text(&node->name, name)) {
        ast_destroy((ASTNode*)node);
        return NULL;
    }
    return (ASTNode*)node;
}

static ASTNode* make_literal_fixture(void) {
    ASTLiteral* node = (ASTLiteral*)ast_create(AST_LITERAL);
    if (!node) return NULL;
    node->type = TOKEN_NUMBER;
    node->lit_type = 1;
    node->int_value = 321;
    node->float_value = 321.75;
    if (!assign_text(&node->string_value, "321.75")) {
        ast_destroy((ASTNode*)node);
        return NULL;
    }
    return (ASTNode*)node;
}

static ASTNode* make_variable_fixture(void) {
    ASTVarDecl* node = (ASTVarDecl*)ast_create(AST_VAR_DECL);
    if (!node) return NULL;
    if (!assign_text(&node->name, "fixture_variable") ||
        !assign_text(&node->type_name, "i64")) {
        ast_destroy((ASTNode*)node);
        return NULL;
    }
    node->is_const = 1;
    node->is_hard_typed = 1;
    node->is_public = 1;
    node->is_exported = 1;
    node->initializer = make_literal_fixture();
    node->inline_type = make_identifier_fixture("FixtureType");
    if (!node->initializer || !node->inline_type) {
        ast_destroy((ASTNode*)node);
        return NULL;
    }
    return (ASTNode*)node;
}

static ASTNode* make_function_fixture(ASTNodeType type) {
    ASTFuncDecl* node = (ASTFuncDecl*)ast_create(type);
    if (!node) return NULL;
    if (!assign_text(&node->name, "fixture_function") ||
        !assign_text(&node->return_type, "i32")) {
        ast_destroy((ASTNode*)node);
        return NULL;
    }
    node->is_extern = type == AST_EXTERN_DECL;
    node->is_public = 1;
    node->is_exported = 1;
    node->params = make_identifier_fixture("fixture_argument");
    node->body = ast_create(AST_BLOCK);
    if (!node->params || !node->body) {
        ast_destroy((ASTNode*)node);
        return NULL;
    }
    ((ASTBlock*)node->body)->statements = make_identifier_fixture("function_statement");
    if (!((ASTBlock*)node->body)->statements) {
        ast_destroy((ASTNode*)node);
        return NULL;
    }
    return (ASTNode*)node;
}

static ASTNode* make_enum_variant_fixture(void) {
    ASTEnumVariant* node = (ASTEnumVariant*)ast_create(AST_ENUM_VARIANT);
    if (!node) return NULL;
    if (!assign_text(&node->name, "FixtureVariant")) {
        ast_destroy((ASTNode*)node);
        return NULL;
    }
    node->data = make_identifier_fixture("variant_payload");
    if (!node->data) {
        ast_destroy((ASTNode*)node);
        return NULL;
    }
    return (ASTNode*)node;
}

static int populate_ast_fixture(ASTNode* node) {
    if (!node) return 0;
    node->line = 37;
    node->col = 19;

    switch (node->type) {
        case AST_PROGRAM:
        case AST_BLOCK:
        case AST_ASM_BLOCK:
            ((ASTBlock*)node)->statements = make_identifier_fixture("block_statement");
            return ((ASTBlock*)node)->statements != NULL;

        case AST_VAR_DECL: {
            ASTNode* fixture = make_variable_fixture();
            if (!fixture) return 0;
            ASTVarDecl* source = (ASTVarDecl*)fixture;
            ASTVarDecl* destination = (ASTVarDecl*)node;
            destination->name = source->name; source->name = NULL;
            destination->type_name = source->type_name; source->type_name = NULL;
            destination->is_const = source->is_const;
            destination->is_hard_typed = source->is_hard_typed;
            destination->is_public = source->is_public;
            destination->is_exported = source->is_exported;
            destination->initializer = source->initializer; source->initializer = NULL;
            destination->inline_type = source->inline_type; source->inline_type = NULL;
            ast_destroy(fixture);
            return 1;
        }

        case AST_FUNC_DECL:
        case AST_EXTERN_DECL: {
            ASTNode* fixture = make_function_fixture(node->type);
            if (!fixture) return 0;
            ASTFuncDecl* source = (ASTFuncDecl*)fixture;
            ASTFuncDecl* destination = (ASTFuncDecl*)node;
            destination->name = source->name; source->name = NULL;
            destination->return_type = source->return_type; source->return_type = NULL;
            destination->is_extern = source->is_extern;
            destination->is_public = source->is_public;
            destination->is_exported = source->is_exported;
            destination->params = source->params; source->params = NULL;
            destination->body = source->body; source->body = NULL;
            ast_destroy(fixture);
            return 1;
        }

        case AST_STRUCT_DECL:
        case AST_UNION_DECL: {
            ASTStructDecl* value = (ASTStructDecl*)node;
            if (!assign_text(&value->name, "FixtureAggregate")) return 0;
            value->is_packed = 1;
            value->is_union = node->type == AST_UNION_DECL;
            value->is_public = 1;
            value->is_exported = 1;
            value->fields = make_variable_fixture();
            return value->fields != NULL;
        }

        case AST_LITERAL: {
            ASTLiteral* value = (ASTLiteral*)node;
            value->type = TOKEN_STRING;
            value->lit_type = 2;
            value->int_value = 654;
            value->float_value = 654.125;
            return assign_text(&value->string_value, "654.125");
        }

        case AST_IDENTIFIER:
            return assign_text(&((ASTIdentifier*)node)->name, "fixture_identifier");

        case AST_BINARY_EXPR:
        case AST_ASSIGNMENT: {
            ASTBinaryExpr* value = (ASTBinaryExpr*)node;
            value->op = TOKEN_PLUS;
            value->left = make_identifier_fixture("left_operand");
            value->right = make_identifier_fixture("right_operand");
            return value->left && value->right;
        }

        case AST_INDEX_ACCESS: {
            ASTIndexAccess* value = (ASTIndexAccess*)node;
            value->target = make_identifier_fixture("index_target");
            value->index = make_literal_fixture();
            return value->target && value->index;
        }

        case AST_NEXUS_FLOW:
        case AST_NEXUS_PIPE: {
            ASTNexusFlow* value = (ASTNexusFlow*)node;
            value->op = TOKEN_PLUS;
            value->left = make_identifier_fixture("flow_left");
            value->right = make_identifier_fixture("flow_right");
            value->retry_count = 3;
            value->retry_delay = 75;
            value->catch_block = make_identifier_fixture("flow_catch");
            return value->left && value->right && value->catch_block;
        }

        case AST_UNARY_EXPR:
        case AST_RETURN:
        case AST_POSTFIX_EXPR: {
            ASTUnaryExpr* value = (ASTUnaryExpr*)node;
            value->op = TOKEN_MINUS;
            value->operand = make_identifier_fixture("unary_operand");
            return value->operand != NULL;
        }

        case AST_CAST: {
            ASTCast* value = (ASTCast*)node;
            value->value = make_literal_fixture();
            value->target_type = make_identifier_fixture("i32");
            return value->value != NULL && value->target_type != NULL;
        }

        case AST_TUPLE:
            ((ASTTuple*)node)->elements = make_identifier_fixture("tuple_element");
            return ((ASTTuple*)node)->elements != NULL;

        case AST_FUNC_CALL:
        case AST_SPAWN:
        case AST_ECHO:
        case AST_SEND:
        case AST_RECEIVE:
        case AST_DONE:
        case AST_AREA:
        case AST_ZONE:
        case AST_ADDR:
        case AST_LEN:
        case AST_PEEK:
        case AST_POKE:
        case AST_INB:
        case AST_OUTB:
        case AST_IRQ:
        case AST_INTR:
        case AST_REG:
        case AST_PRNT:
        case AST_PRMT:
        case AST_WRITE:
        case AST_FWRITE:
        case AST_TYPEOF:
        case AST_SIZEOF:
        case AST_IS_OK:
        case AST_IS_ERR:
        case AST_IS_SOME:
        case AST_IS_NONE:
        case AST_IS_NULL:
        case AST_EXIT:
        case AST_SWAP:
        case AST_DATE:
        case AST_DATENOW:
        case AST_TIME:
        case AST_CLOCK:
        case AST_FREE:
        case AST_ZONE_SYNC:
        case AST_ZONE_VIEW:
        case AST_ZONE_LOCK: {
            ASTCall* value = (ASTCall*)node;
            value->callee = make_identifier_fixture("call_target");
            value->args = make_identifier_fixture("call_argument");
            return value->callee && value->args;
        }

        case AST_MEMBER_ACCESS: {
            ASTMemberAccess* value = (ASTMemberAccess*)node;
            if (!assign_text(&value->member_name, "fixture_member")) return 0;
            value->object = make_identifier_fixture("fixture_object");
            return value->object != NULL;
        }

        case AST_IF: {
            ASTIf* value = (ASTIf*)node;
            value->condition = make_identifier_fixture("if_condition");
            value->then_branch = make_identifier_fixture("then_branch");
            value->else_branch = make_identifier_fixture("else_branch");
            return value->condition && value->then_branch && value->else_branch;
        }

        case AST_LOOP: {
            ASTLoop* value = (ASTLoop*)node;
            value->loop_type = 3;
            value->init = make_identifier_fixture("loop_init");
            value->condition = make_identifier_fixture("loop_condition");
            value->increment = make_identifier_fixture("loop_increment");
            value->body = make_identifier_fixture("loop_body");
            value->collection = make_identifier_fixture("loop_collection");
            value->variable = make_identifier_fixture("loop_variable");
            return value->init && value->condition && value->increment &&
                value->body && value->collection && value->variable;
        }

        case AST_GROUP_DECL: {
            ASTGroupDecl* value = (ASTGroupDecl*)node;
            if (!assign_text(&value->name, "FixtureGroup")) return 0;
            value->is_public = 1;
            value->is_exported = 1;
            value->members = make_variable_fixture();
            return value->members != NULL;
        }

        case AST_METHOD: {
            ASTMethod* value = (ASTMethod*)node;
            if (!assign_text(&value->public_name, "fixture_method")) return 0;
            value->implementation = make_function_fixture(AST_FUNC_DECL);
            return value->implementation != NULL;
        }

        case AST_USE: {
            ASTUse* value = (ASTUse*)node;
            return assign_text(&value->module_name, "fixture_module") &&
                assign_text(&value->alias, "fm");
        }

        case AST_DEFER:
            ((ASTDefer*)node)->body = make_identifier_fixture("defer_body");
            return ((ASTDefer*)node)->body != NULL;

        case AST_THREAD_BLOCK: {
            ASTThreadBlock* value = (ASTThreadBlock*)node;
            if (!assign_text(&value->name, "fixture_thread")) return 0;
            value->body = make_identifier_fixture("thread_body");
            value->mode = make_identifier_fixture("thread_mode");
            value->timeout = make_identifier_fixture("thread_timeout");
            return value->body && value->mode && value->timeout;
        }

        case AST_ENUM_DECL: {
            ASTEnumDecl* value = (ASTEnumDecl*)node;
            if (!assign_text(&value->name, "FixtureEnum")) return 0;
            value->variants = make_enum_variant_fixture();
            return value->variants != NULL;
        }

        case AST_ENUM_VARIANT: {
            ASTEnumVariant* value = (ASTEnumVariant*)node;
            if (!assign_text(&value->name, "FixtureVariant")) return 0;
            value->data = make_identifier_fixture("variant_data");
            return value->data != NULL;
        }

        case AST_NEWTYPE_DECL: {
            ASTNewTypeDecl* value = (ASTNewTypeDecl*)node;
            return assign_text(&value->name, "FixtureId") &&
                assign_text(&value->target_type, "u64");
        }

        case AST_NEXUS_CAPTURE:
        case AST_LISTEN: {
            ASTNexusCapture* value = (ASTNexusCapture*)node;
            if (!assign_text(&value->handler_name, "fixture_handler")) return 0;
            value->expr = make_identifier_fixture("capture_expression");
            return value->expr != NULL;
        }

        case AST_NEXUS_LABEL: {
            ASTNexusLabel* value = (ASTNexusLabel*)node;
            if (!assign_text(&value->label_name, "fixture_label")) return 0;
            value->statement = make_identifier_fixture("label_statement");
            return value->statement != NULL;
        }

        case AST_NEXUS_JUMP:
            return assign_text(&((ASTNexusJump*)node)->label_name, "fixture_jump");

        case AST_NEXUS_HALT:
            ((ASTHalt*)node)->expr = make_identifier_fixture("halt_expression");
            return ((ASTHalt*)node)->expr != NULL;

        case AST_NEXUS_INTENT: {
            ASTIntent* value = (ASTIntent*)node;
            if (!assign_text(&value->intent_name, "fixture_intent")) return 0;
            value->target = make_identifier_fixture("intent_target");
            return value->target != NULL;
        }

        case AST_AI_LOGIC:
            return assign_text(&((ASTAiLogic*)node)->query, "inspect this code");

        case AST_UNPACK:
            ((ASTUnpack*)node)->expr = make_identifier_fixture("unpack_expression");
            return ((ASTUnpack*)node)->expr != NULL;

        case AST_PANIC:
            return assign_text(&((ASTPanic*)node)->message, "fixture panic");

        case AST_BREAK:
        case AST_CONTINUE:
            return 1;

        case AST_SELECT: {
            ASTSelect* value = (ASTSelect*)node;
            ASTSelectArm* arm=(ASTSelectArm*)ast_create(AST_SELECT_ARM);
            if(!arm) return 0;
            value->selector = make_identifier_fixture("select_value");
            arm->pattern=make_identifier_fixture("select_pattern");
            arm->body=make_identifier_fixture("select_body");
            arm->is_default=1;
            value->arms=(ASTNode*)arm;
            return value->selector && arm->pattern && arm->body;
        }
        case AST_SELECT_ARM: {
            ASTSelectArm* value = (ASTSelectArm*)node;
            value->pattern = make_identifier_fixture("select_pattern");
            value->body = make_identifier_fixture("select_body");
            value->is_default = 1;
            return value->pattern && value->body;
        }
        case AST_SCAN: {
            ASTScan* value = (ASTScan*)node;
            ASTScanArm* arm=(ASTScanArm*)ast_create(AST_SCAN_ARM);
            if(!arm) return 0;
            value->subject = make_identifier_fixture("scan_subject");
            arm->pattern=make_identifier_fixture("scan_pattern");
            arm->body=make_identifier_fixture("scan_body");
            value->arms=(ASTNode*)arm;
            return value->subject && arm->pattern && arm->body;
        }
        case AST_SCAN_ARM: {
            ASTScanArm* value = (ASTScanArm*)node;
            value->pattern = make_identifier_fixture("scan_pattern");
            value->body = make_identifier_fixture("scan_body");
            return value->pattern && value->body;
        }
        case AST_PATTERN_BINDING:
            return assign_text(&((ASTPatternBinding*)node)->name, "pattern_binding");
        case AST_PATTERN_CONSTRUCTOR: {
            ASTPatternConstructor* value = (ASTPatternConstructor*)node;
            if (!assign_text(&value->name, "PatternCtor")) return 0;
            value->bindings = make_identifier_fixture("pattern_arg");
            return value->bindings != NULL;
        }
        case AST_RULESET_DECL: {
            ASTRulesetDecl* value = (ASTRulesetDecl*)node;
            if (!assign_text(&value->name, "FixtureRules")) return 0;
            value->params = make_identifier_fixture("rule_param");
            value->rules = make_identifier_fixture("rule_body");
            return value->params && value->rules;
        }
        case AST_RULE_BARRIER: {
            ASTRuleBarrier* value = (ASTRuleBarrier*)node;
            value->condition = make_identifier_fixture("barrier_condition");
            value->message = make_literal_fixture();
            return value->condition && value->message;
        }
        case AST_RULE_TRIGGER: {
            ASTRuleTrigger* value = (ASTRuleTrigger*)node;
            value->condition = make_identifier_fixture("trigger_condition");
            value->action = make_identifier_fixture("trigger_action");
            return value->condition && value->action;
        }
        case AST_RULE_ALWAYS:
            ((ASTRuleAlways*)node)->action = make_identifier_fixture("always_action");
            return ((ASTRuleAlways*)node)->action != NULL;
        case AST_RULE_DEFAULT:
            ((ASTRuleDefault*)node)->action = make_identifier_fixture("default_action");
            return ((ASTRuleDefault*)node)->action != NULL;
        case AST_APPLY_RULES: {
            ASTApplyRules* value = (ASTApplyRules*)node;
            if (!assign_text(&value->ruleset_name, "FixtureRules")) return 0;
            value->args = make_identifier_fixture("rule_argument");
            return value->args != NULL;
        }
        case AST_ON_BLOCK: {
            ASTOnBlock* value = (ASTOnBlock*)node;
            if (!assign_text(&value->ruleset_name, "FixtureRules") ||
                !assign_text(&value->event_name, "FixtureEvent")) return 0;
            value->body = make_identifier_fixture("event_body");
            return value->body != NULL;
        }
        case AST_LAMBDA: {
            ASTLambda* value = (ASTLambda*)node;
            value->params = make_identifier_fixture("lambda_param");
            value->body = make_identifier_fixture("lambda_body");
            value->body_is_expression = 1;
            return value->params && value->body;
        }
        case AST_RELOCATE: {
            ASTRelocate* value = (ASTRelocate*)node;
            value->source = make_identifier_fixture("relocate_source");
            value->target = make_identifier_fixture("relocate_target");
            return value->source && value->target;
        }
        case AST_UNROLL: {
            ASTUnroll* value = (ASTUnroll*)node;
            value->count = make_literal_fixture();
            value->loop = make_identifier_fixture("unroll_loop");
            return value->count && value->loop;
        }
        case AST_ARRAY_LITERAL:
            ((ASTArrayLiteral*)node)->elements = make_identifier_fixture("array_element");
            return ((ASTArrayLiteral*)node)->elements != NULL;
        case AST_ARRAY_TYPE:
            ((ASTArrayType*)node)->dimensions = make_identifier_fixture("array_dimension");
            return ((ASTArrayType*)node)->dimensions != NULL;
        case AST_STRUCT_LITERAL: {
            ASTStructLiteral* v=(ASTStructLiteral*)node;
            ASTStructFieldInit* field=(ASTStructFieldInit*)ast_create(AST_STRUCT_FIELD_INIT);
            if(!assign_text(&v->type_name,"FixturePoint") || !field ||
               !assign_text(&field->name,"x")) { ast_destroy((ASTNode*)field); return 0; }
            field->value=make_literal_fixture();
            if(!field->value) { ast_destroy((ASTNode*)field); return 0; }
            v->fields=(ASTNode*)field;
            return 1;
        }
        case AST_STRUCT_FIELD_INIT: {
            ASTStructFieldInit* v=(ASTStructFieldInit*)node;
            if(!assign_text(&v->name,"fixture_field")) return 0;
            v->value=make_literal_fixture();
            return v->value!=NULL;
        }
        case AST_FASTEXEC_BLOCK: ((ASTFastExecBlock*)node)->statements=make_identifier_fixture("fast_statement"); return ((ASTFastExecBlock*)node)->statements!=NULL;
case AST_ASM_LABEL: { ASTAsmLabel* v=(ASTAsmLabel*)node; if(!assign_text(&v->label,"FIXTURE_ASM"))return 0; v->source_text=duplicate_text("nop\\r\\n"); if(!v->source_text)return 0; v->source_length=strlen(v->source_text); return 1; }
case AST_NAMED_ARG: { ASTNamedArg* v=(ASTNamedArg*)node; if(!assign_text(&v->name,"fixture_arg"))return 0; v->value=make_identifier_fixture("named_value"); return v->value!=NULL; }
case AST_MUST_DECL: { ASTMustDecl* v=(ASTMustDecl*)node; if(!assign_text(&v->name,"fixture_dependency"))return 0; v->source=make_identifier_fixture("dependency_source"); return v->source!=NULL; }
case AST_BLOCK_HALT: case AST_VARIADIC_PARAM: return 1;
case AST_ARRAY_DIMENSION:
            ((ASTArrayDimension*)node)->length = make_literal_fixture();
            return ((ASTArrayDimension*)node)->length != NULL;
    }

    return 0;
}

static int streams_equal(FILE* left, FILE* right) {
    unsigned char left_buffer[512];
    unsigned char right_buffer[512];

    rewind(left);
    rewind(right);
    for (;;) {
        size_t left_count = fread(left_buffer, 1, sizeof(left_buffer), left);
        size_t right_count = fread(right_buffer, 1, sizeof(right_buffer), right);
        if (left_count != right_count ||
            memcmp(left_buffer, right_buffer, left_count) != 0) return 0;
        if (left_count < sizeof(left_buffer)) {
            return feof(left) && feof(right);
        }
    }
}

static int roundtrips_populated_ast_types(void) {
    for (int raw_type = AST_PROGRAM; raw_type <= AST_ARRAY_DIMENSION; ++raw_type) {
        ASTNode* original = ast_create((ASTNodeType)raw_type);
        ASTNode* root;
        if (!original) { fprintf(stderr, "ast_create failed for populated tag %d.\\n", raw_type); return 1; }
        if (!populate_ast_fixture(original)) {
            fprintf(stderr, "Could not populate AST fixture for tag %d.\n", raw_type);
            ast_destroy(original);
            return 1;
        }
        root=original;
        /* Context-bound records are round-tripped under their legal owning edge. */
        if(raw_type==AST_SELECT_ARM) {
            ASTSelect* owner=(ASTSelect*)ast_create(AST_SELECT);
            if(!owner) { fprintf(stderr,"context owner allocation failed for tag %d.\\n",raw_type); ast_destroy(original); return 1; }
            owner->selector=make_identifier_fixture("select_owner");
            if(!owner->selector) { fprintf(stderr,"select owner selector allocation failed.\\n"); ast_destroy((ASTNode*)owner); ast_destroy(original); return 1; }
            owner->arms=original;
            root=(ASTNode*)owner;
        } else if(raw_type==AST_SCAN_ARM) {
            ASTScan* owner=(ASTScan*)ast_create(AST_SCAN);
            if(!owner) { ast_destroy(original); return 1; }
            owner->subject=make_identifier_fixture("scan_owner");
            if(!owner->subject) { fprintf(stderr,"scan owner subject allocation failed.\\n"); ast_destroy((ASTNode*)owner); ast_destroy(original); return 1; }
            owner->arms=original;
            root=(ASTNode*)owner;
        } else if(raw_type==AST_ARRAY_DIMENSION) {
            ASTArrayType* owner=(ASTArrayType*)ast_create(AST_ARRAY_TYPE);
            if(!owner) { ast_destroy(original); return 1; }
            owner->dimensions=original;
            root=(ASTNode*)owner;
        }

        FILE* first = tmpfile();
        FILE* second = tmpfile();
        if (!first || !second) { fprintf(stderr,"tmpfile failed for populated tag %d.\\n",raw_type);
            if (first) fclose(first);
            if (second) fclose(second);
            ast_destroy(root);
            return 1;
        }

        if (!ast_serialize(root, first)) {
            fprintf(stderr,"AST serializer rejected populated fixture tag %d.\\n",raw_type);
            fclose(first);
            fclose(second);
            ast_destroy(root);
            return 1;
        }
        rewind(first);
        ASTNode* decoded = ast_deserialize(first);
        if (!decoded || !ast_serialize(decoded, second)) {
            fclose(first);
            fclose(second);
            ast_destroy(decoded);
            ast_destroy(root);
            fprintf(stderr, "AST serialize/deserialize failed for tag %d.\n", raw_type);
            return 1;
        }

        int valid = streams_equal(first, second);
        fclose(first);
        fclose(second);
        ast_destroy(decoded);
        ast_destroy(root);
        if (!valid) {
            fprintf(stderr, "AST field round-trip mismatch for tag %d.\n", raw_type);
            return 1;
        }
    }
    return 0;
}

static int roundtrips_struct_literal_nodes(void) {
    ASTStructLiteral* literal=(ASTStructLiteral*)ast_create(AST_STRUCT_LITERAL);
    ASTStructFieldInit* field=(ASTStructFieldInit*)ast_create(AST_STRUCT_FIELD_INIT);
    FILE *first=NULL,*second=NULL;
    ASTNode* decoded=NULL;
    int ok=0;
    if(!literal || !field) { fprintf(stderr,"Struct literal fixture allocation failed.\\n"); goto done; }
    literal->type_name=duplicate_text("Point");
    field->name=duplicate_text("x");
    field->value=make_literal_fixture();
    if(!literal->type_name || !field->name || !field->value) { fprintf(stderr,"Struct literal fixture payload allocation failed.\\n"); goto done; }
    literal->fields=(ASTNode*)field;
    field=NULL;
    if(!ast_validate((ASTNode*)literal,NULL)) { fprintf(stderr,"Struct literal source graph validation failed.\\n"); goto done; }
    first=tmpfile(); second=tmpfile();
    if(!first || !second) { fprintf(stderr,"Struct literal tmpfile failed.\\n"); goto done; }
    if(!ast_serialize((ASTNode*)literal,first)) { fprintf(stderr,"Struct literal serialization failed.\\n"); goto done; }
    rewind(first);
    decoded=ast_deserialize(first);
    if(!decoded) { fprintf(stderr,"Struct literal deserialization failed.\\n"); goto done; }
    if(!ast_validate(decoded,NULL)) { fprintf(stderr,"Struct literal decoded graph validation failed.\\n"); goto done; }
    if(!ast_serialize(decoded,second)) { fprintf(stderr,"Struct literal reserialization failed.\\n"); goto done; }
    if(!streams_equal(first,second)) { fprintf(stderr,"Struct literal stream bytes differ after round-trip.\\n"); goto done; }
    {
        ASTStructLiteral* restored=(ASTStructLiteral*)decoded;
        ASTStructFieldInit* restored_field=restored->fields
            ? (ASTStructFieldInit*)restored->fields : NULL;
        if(restored->base.type!=AST_STRUCT_LITERAL ||
            !restored->type_name || strcmp(restored->type_name,"Point")!=0 ||
            !restored_field || restored_field->base.type!=AST_STRUCT_FIELD_INIT ||
            !restored_field->name || strcmp(restored_field->name,"x")!=0 ||
            !restored_field->value || restored_field->value->type!=AST_LITERAL ||
            strcmp(((ASTLiteral*)restored_field->value)->string_value,"321.75")!=0 ||
            restored_field->base.next) goto done;
    }
    ok=1;
done:
    if(first) fclose(first);
    if(second) fclose(second);
    if(decoded) ast_destroy(decoded);
    if(literal) ast_destroy((ASTNode*)literal);
    if(field) ast_destroy((ASTNode*)field);
    return ok ? 0 : 1;
}

static int roundtrips_all_node_type_tags(void) {
    for (int raw_type = AST_PROGRAM; raw_type <= AST_ARRAY_DIMENSION; ++raw_type) {
        ASTNode* original = ast_create((ASTNodeType)raw_type);
        ASTNode* root;
        ASTNode* sibling;
        if (!original) return 1;
        if (!populate_ast_fixture(original)) {
            fprintf(stderr, "Could not populate AST tag fixture for tag %d.\n", raw_type);
            ast_destroy(original);
            return 1;
        }
        root=original;
        /* Contextual tags are tested as children of their normative owner. */
        if(raw_type==AST_SELECT_ARM) {
            ASTSelect* owner=(ASTSelect*)ast_create(AST_SELECT);
            if(!owner) { ast_destroy(original); return 1; }
            owner->selector=make_identifier_fixture("select_owner");
            if(!owner->selector) { ast_destroy((ASTNode*)owner); ast_destroy(original); return 1; }
            owner->arms=original;
            root=(ASTNode*)owner;
        } else if(raw_type==AST_SCAN_ARM) {
            ASTScan* owner=(ASTScan*)ast_create(AST_SCAN);
            if(!owner) { ast_destroy(original); return 1; }
            owner->subject=make_identifier_fixture("scan_owner");
            if(!owner->subject) { ast_destroy((ASTNode*)owner); ast_destroy(original); return 1; }
            owner->arms=original;
            root=(ASTNode*)owner;
        } else if(raw_type==AST_ARRAY_DIMENSION) {
            ASTArrayType* owner=(ASTArrayType*)ast_create(AST_ARRAY_TYPE);
            if(!owner) { ast_destroy(original); return 1; }
            owner->dimensions=original;
            root=(ASTNode*)owner;
        }
        original->line = 100 + raw_type;
        original->col = 200 + raw_type;
        sibling = ast_create(AST_IDENTIFIER);
        if (!sibling) { ast_destroy(root); return 1; }
        ((ASTIdentifier*)sibling)->name = duplicate_text("sibling-node");
        if (!((ASTIdentifier*)sibling)->name) {
            ast_destroy(sibling); ast_destroy(root); return 1;
        }
        root->next = sibling;

        FILE* stream = tmpfile();
        if (!stream) { ast_destroy(root); return 1; }
        if (!ast_serialize(root, stream)) {
            fclose(stream);
            ast_destroy(root);
            fprintf(stderr, "AST serializer rejected valid tag fixture %d.\n", raw_type);
            return 1;
        }
        rewind(stream);
        ASTNode* decoded = ast_deserialize(stream);
        fclose(stream);

        ASTNode* decoded_target=decoded;
        if(raw_type==AST_SELECT_ARM && decoded)
            decoded_target=((ASTSelect*)decoded)->arms;
        else if(raw_type==AST_SCAN_ARM && decoded)
            decoded_target=((ASTScan*)decoded)->arms;
        else if(raw_type==AST_ARRAY_DIMENSION && decoded)
            decoded_target=((ASTArrayType*)decoded)->dimensions;

        int valid = decoded && decoded_target &&
            decoded_target->type == (ASTNodeType)raw_type &&
            decoded_target->line == 100 + raw_type &&
            decoded_target->col == 200 + raw_type &&
            decoded->next &&
            decoded->next->type == AST_IDENTIFIER &&
            ((ASTIdentifier*)decoded->next)->name &&
            strcmp(((ASTIdentifier*)decoded->next)->name, "sibling-node") == 0;

        ast_destroy(decoded);
        ast_destroy(root);
        if (!valid) {
            fprintf(stderr, "AST node type round-trip failed for tag %d.\n", raw_type);
            return 1;
        }
    }
    return 0;
}

static int roundtrips_block_halt_tag(void) {
    ASTNode* block = ast_create(AST_BLOCK);
    ASTNode* marker = ast_create(AST_BLOCK_HALT);
    FILE* stream;
    ASTNode* decoded;
    int valid;
    if (!block || !marker) { ast_destroy(block); ast_destroy(marker); return 1; }
    ((ASTBlock*)block)->statements = marker;
    block->line = 100;
    block->col = 200;
    marker->line = 101;
    marker->col = 201;
    stream = tmpfile();
    if (!stream) { ast_destroy(block); return 1; }
    if (!ast_serialize(block, stream)) { fclose(stream); ast_destroy(block); return 1; }
    rewind(stream);
    decoded = ast_deserialize(stream);
    fclose(stream);
    valid = decoded && decoded->type == AST_BLOCK &&
        ((ASTBlock*)decoded)->statements &&
        ((ASTBlock*)decoded)->statements->type == AST_BLOCK_HALT &&
        ((ASTBlock*)decoded)->statements->line == 101 &&
        ((ASTBlock*)decoded)->statements->col == 201 &&
        ast_validate(decoded, NULL);
    ast_destroy(decoded);
    ast_destroy(block);
    return valid ? 0 : 1;
}

static int roundtrips_extern_decl(void) {
    ASTFuncDecl* original = (ASTFuncDecl*)ast_create(AST_EXTERN_DECL);
    if (!original) return 1;

    original->name = duplicate_text("external_api");
    original->return_type = duplicate_text("i32");
    original->is_extern = 1;
    original->is_public = 1;
    original->is_exported = 0;
    original->params = ast_create(AST_IDENTIFIER);
    original->body = ast_create(AST_BLOCK);

    if (!original->name || !original->return_type ||
        !original->params || !original->body) {
        ast_destroy((ASTNode*)original);
        return 1;
    }

    ((ASTIdentifier*)original->params)->name = duplicate_text("argument");
    if (!((ASTIdentifier*)original->params)->name) {
        ast_destroy((ASTNode*)original);
        return 1;
    }

    FILE* stream = tmpfile();
    if (!stream) {
        ast_destroy((ASTNode*)original);
        return 1;
    }

    ast_serialize((ASTNode*)original, stream);
    rewind(stream);
    ASTFuncDecl* decoded = (ASTFuncDecl*)ast_deserialize(stream);
    fclose(stream);

    int valid = decoded &&
        decoded->base.type == AST_EXTERN_DECL &&
        decoded->name && strcmp(decoded->name, "external_api") == 0 &&
        decoded->return_type && strcmp(decoded->return_type, "i32") == 0 &&
        decoded->is_extern == 1 &&
        decoded->is_public == 1 &&
        decoded->is_exported == 0 &&
        decoded->params && decoded->params->type == AST_IDENTIFIER &&
        ((ASTIdentifier*)decoded->params)->name &&
        strcmp(((ASTIdentifier*)decoded->params)->name, "argument") == 0 &&
        decoded->body && decoded->body->type == AST_BLOCK;

    ast_destroy((ASTNode*)decoded);
    ast_destroy((ASTNode*)original);
    return valid ? 0 : 1;
}

static int roundtrips_special_nodes(void) {
    ASTProgram* program = (ASTProgram*)ast_create(AST_PROGRAM);
    ASTUnpack* unpack = (ASTUnpack*)ast_create(AST_UNPACK);
    ASTHalt* halt = (ASTHalt*)ast_create(AST_NEXUS_HALT);
    ASTLiteral* unpack_value = (ASTLiteral*)ast_create(AST_LITERAL);
    ASTLiteral* halt_value = (ASTLiteral*)ast_create(AST_LITERAL);

    if (!program || !unpack || !halt || !unpack_value || !halt_value) {
        ast_destroy((ASTNode*)program);
        ast_destroy((ASTNode*)unpack);
        ast_destroy((ASTNode*)halt);
        ast_destroy((ASTNode*)unpack_value);
        ast_destroy((ASTNode*)halt_value);
        return 1;
    }

    unpack_value->type = TOKEN_NUMBER;
    unpack_value->lit_type = 0;
    unpack_value->int_value = 123;
    unpack_value->float_value = 123.5;
    unpack_value->string_value = duplicate_text("123");
    halt_value->type = TOKEN_NUMBER;
    halt_value->lit_type = 0;
    halt_value->int_value = 456;
    halt_value->float_value = 456.5;
    halt_value->string_value = duplicate_text("456");
    if (!unpack_value->string_value || !halt_value->string_value) {
        ast_destroy((ASTNode*)program);
        ast_destroy((ASTNode*)unpack);
        ast_destroy((ASTNode*)halt);
        ast_destroy((ASTNode*)unpack_value);
        ast_destroy((ASTNode*)halt_value);
        return 1;
    }

    unpack->expr = (ASTNode*)unpack_value;
    halt->expr = (ASTNode*)halt_value;
    unpack->base.next = (ASTNode*)halt;
    program->declarations = (ASTNode*)unpack;

    FILE* stream = tmpfile();
    if (!stream) {
        ast_destroy((ASTNode*)program);
        return 1;
    }

    ast_serialize((ASTNode*)program, stream);
    rewind(stream);
    ASTProgram* decoded = (ASTProgram*)ast_deserialize(stream);
    fclose(stream);

    if (!decoded || !decoded->declarations ||
        decoded->declarations->type != AST_UNPACK ||
        !decoded->declarations->next ||
        decoded->declarations->next->type != AST_NEXUS_HALT) {
        ast_destroy((ASTNode*)decoded);
        ast_destroy((ASTNode*)program);
        return 1;
    }

    ASTUnpack* decoded_unpack = (ASTUnpack*)decoded->declarations;
    ASTHalt* decoded_halt = (ASTHalt*)decoded->declarations->next;
    int valid = decoded_unpack->expr &&
        decoded_unpack->expr->type == AST_LITERAL &&
        ((ASTLiteral*)decoded_unpack->expr)->type == TOKEN_NUMBER &&
        ((ASTLiteral*)decoded_unpack->expr)->int_value == 123 &&
        ((ASTLiteral*)decoded_unpack->expr)->float_value == 123.5 &&
        ((ASTLiteral*)decoded_unpack->expr)->string_value &&
        strcmp(((ASTLiteral*)decoded_unpack->expr)->string_value, "123") == 0 &&
        decoded_halt->expr &&
        decoded_halt->expr->type == AST_LITERAL &&
        ((ASTLiteral*)decoded_halt->expr)->type == TOKEN_NUMBER &&
        ((ASTLiteral*)decoded_halt->expr)->int_value == 456 &&
        ((ASTLiteral*)decoded_halt->expr)->float_value == 456.5 &&
        ((ASTLiteral*)decoded_halt->expr)->string_value &&
        strcmp(((ASTLiteral*)decoded_halt->expr)->string_value, "456") == 0;

    ast_destroy((ASTNode*)decoded);
    ast_destroy((ASTNode*)program);
    return valid ? 0 : 1;
}

static int roundtrips_large_sibling_list(void) {
    enum { NODE_COUNT = 32768 };
    ASTProgram* program = (ASTProgram*)ast_create(AST_PROGRAM);
    if (!program) return 1;

    ASTNode* head = NULL;
    ASTNode* tail = NULL;
    for (int i = 0; i < NODE_COUNT; ++i) {
        ASTNode* node = ast_create(AST_BREAK);
        if (!node) {
            ast_destroy(head);
            ast_destroy((ASTNode*)program);
            return 1;
        }
        if (!head) head = node;
        else tail->next = node;
        tail = node;
    }
    program->declarations = head;

    FILE* stream = tmpfile();
    if (!stream) {
        ast_destroy((ASTNode*)program);
        return 1;
    }

    ast_serialize((ASTNode*)program, stream);
    rewind(stream);
    ASTProgram* decoded = (ASTProgram*)ast_deserialize(stream);
    fclose(stream);

    size_t count = 0;
    if (decoded) {
        ASTNode* node = decoded->declarations;
        while (node && count <= NODE_COUNT) {
            count++;
            node = node->next;
        }
    }

    int valid = decoded && count == NODE_COUNT;
    ast_destroy((ASTNode*)decoded);
    ast_destroy((ASTNode*)program);
    return valid ? 0 : 1;
}

static int destroys_very_large_sibling_list(void) {
    enum { NODE_COUNT = 32768 };
    ASTNode* head = NULL;
    ASTNode* tail = NULL;
    int i;

    for (i = 0; i < NODE_COUNT; ++i) {
        ASTNode* node = ast_create(AST_BREAK);
        if (!node) {
            ast_destroy(head);
            return 0;
        }
        if (!head) head = node;
        else tail->next = node;
        tail = node;
    }

    /* Must not recurse once per sibling while releasing a flat AST list. */
    ast_destroy(head);
    return 1;
}

static int rejects_excessive_serialization_depth(void) {
    enum { NESTING = 600 };
    ASTNode* root = ast_create(AST_UNARY_EXPR);
    ASTNode* current = root;
    FILE* stream;
    int serialized;

    if (!root) return 0;
    for (int i = 1; i < NESTING; ++i) {
        ASTNode* child = ast_create(AST_UNARY_EXPR);
        if (!child) {
            ast_destroy(root);
            return 0;
        }
        ((ASTUnaryExpr*)current)->operand = child;
        current = child;
    }

    stream = tmpfile();
    if (!stream) {
        ast_destroy(root);
        return 0;
    }
    serialized = ast_serialize(root, stream);
    fclose(stream);
    ast_destroy(root);
    return serialized == 0;
}

static int parser_save_ir_rejects_invalid_path(const char* path) {
    ParserContext context;
    memset(&context, 0, sizeof(context));
    context.ast_root = ast_create(AST_PROGRAM);
    if (!context.ast_root) return 0;

    int saved = parser_save_ir(&context, path);
    ast_destroy((ASTNode*)context.ast_root);
    return saved == 0;
}


static int roundtrips_versioned_source_spans(void) {
    ASTIdentifier* original = (ASTIdentifier*)ast_create(AST_IDENTIFIER);
    OccSourceSpan span;
    FILE* stream;
    unsigned char magic[8];
    uint32_t version = 0u;
    ASTIdentifier* decoded;
    int valid;
    if (!original) return 1;
    original->name = duplicate_text("span_fixture");
    if (!original->name) { ast_destroy((ASTNode*)original); return 1; }
    memset(&span, 0, sizeof(span));
    span.source_id = 7u;
    span.start_offset = 12u;
    span.end_offset = 24u;
    span.start_line = 3u;
    span.start_column = 5u;
    span.end_line = 3u;
    span.end_column = 17u;
    span.has_location = 1u;
    if (!ast_set_source_span((ASTNode*)original, &span)) {
        ast_destroy((ASTNode*)original);
        return 1;
    }
    stream = tmpfile();
    if (!stream) { ast_destroy((ASTNode*)original); return 1; }
    if (!ast_serialize((ASTNode*)original, stream) ||
        fseek(stream, 0L, SEEK_SET) != 0 ||
        fread(magic, 1u, sizeof(magic), stream) != sizeof(magic) ||
        fread(&version, sizeof(version), 1u, stream) != 1u ||
        memcmp(magic, "OCCAST\r\n", sizeof(magic)) != 0 ||
        version != 2u) {
        fclose(stream);
        ast_destroy((ASTNode*)original);
        return 1;
    }
    rewind(stream);
    decoded = (ASTIdentifier*)ast_deserialize(stream);
    fclose(stream);
    valid = decoded && decoded->name &&
        strcmp(decoded->name, "span_fixture") == 0 &&
        decoded->base.source_span.has_location == 1u &&
        decoded->base.source_span.source_id == span.source_id &&
        decoded->base.source_span.start_offset == span.start_offset &&
        decoded->base.source_span.end_offset == span.end_offset &&
        decoded->base.source_span.start_line == span.start_line &&
        decoded->base.source_span.start_column == span.start_column &&
        decoded->base.source_span.end_line == span.end_line &&
        decoded->base.source_span.end_column == span.end_column &&
        ast_validate((ASTNode*)decoded, NULL);
    ast_destroy((ASTNode*)decoded);
    ast_destroy((ASTNode*)original);
    return valid ? 0 : 1;
}

static int reads_v1_function_stream_without_v2_return_child(void) {
    ASTNode* original=make_function_fixture(AST_FUNC_DECL);
    FILE* stream=NULL;
    unsigned char* bytes=NULL;
    long end;
    uint32_t version=1u;
    ASTFuncDecl* decoded=NULL;
    int valid=0;
    if(!original) return 1;
    stream=tmpfile();
    if(!stream || !ast_serialize(original,stream) || fflush(stream)!=0) goto done;
    end=ftell(stream);
    if(end<=4L || (unsigned long)end>SIZE_MAX) goto done;
    bytes=(unsigned char*)malloc((size_t)end);
    if(!bytes || fseek(stream,0L,SEEK_SET)!=0 ||
       fread(bytes,1u,(size_t)end,stream)!=(size_t)end) goto done;
    /* Version 1 function payloads end after body; the v2-only optional
     * return-array NULL marker is removed while preserving the list marker. */
    memcpy(bytes+8u,&version,sizeof(version));
    fclose(stream);
    stream=tmpfile();
    if(!stream || fwrite(bytes,1u,(size_t)end-sizeof(int),stream)!=(size_t)end-sizeof(int))
        goto done;
    rewind(stream);
    decoded=(ASTFuncDecl*)ast_deserialize(stream);
    valid=decoded && decoded->base.type==AST_FUNC_DECL &&
        decoded->name && strcmp(decoded->name,"fixture_function")==0 &&
        decoded->params && decoded->body && !decoded->return_array_type &&
        ast_validate((ASTNode*)decoded,NULL);
done:
    if(stream) fclose(stream);
    free(bytes);
    ast_destroy(decoded?(ASTNode*)decoded:NULL);
    ast_destroy(original);
    return valid?0:1;
}

static int reads_unprefixed_legacy_stream(void) {
    FILE* stream = tmpfile();
    int type = (int)AST_IDENTIFIER, line = 9, col = 4;
    int name_length = 6, terminator = -1;
    ASTIdentifier* decoded;
    int valid;
    if (!stream) return 1;
    if (fwrite(&type, sizeof(type), 1u, stream) != 1u ||
        fwrite(&line, sizeof(line), 1u, stream) != 1u ||
        fwrite(&col, sizeof(col), 1u, stream) != 1u ||
        fwrite(&name_length, sizeof(name_length), 1u, stream) != 1u ||
        fwrite("legacy", 1u, 6u, stream) != 6u ||
        fwrite(&terminator, sizeof(terminator), 1u, stream) != 1u) {
        fclose(stream);
        return 1;
    }
    rewind(stream);
    decoded = (ASTIdentifier*)ast_deserialize(stream);
    fclose(stream);
    valid = decoded && decoded->base.type == AST_IDENTIFIER &&
        decoded->base.line == line && decoded->base.col == col &&
        decoded->base.source_span.has_location == 0u &&
        decoded->name && strcmp(decoded->name, "legacy") == 0;
    ast_destroy((ASTNode*)decoded);
    return valid ? 0 : 1;
}

static int rejects_unknown_ast_stream_version(void) {
    FILE* stream = tmpfile();
    const unsigned char magic[8] = "OCCAST\r\n";
    uint32_t version = 99u;
    ASTNode* decoded;
    if (!stream) return 1;
    if (fwrite(magic, 1u, sizeof(magic), stream) != sizeof(magic) ||
        fwrite(&version, sizeof(version), 1u, stream) != 1u) {
        fclose(stream);
        return 1;
    }
    rewind(stream);
    decoded = ast_deserialize(stream);
    fclose(stream);
    if (decoded) { ast_destroy(decoded); return 1; }
    return 0;
}


static int rejects_truncated_ast_stream_prefixes(void) {
    const unsigned char magic[8] = "OCCAST\r\n";
    uint32_t version = 1u;
    FILE* stream = tmpfile();
    ASTNode* decoded;
    if (!stream) return 1;
    if (fwrite(magic, 1u, 4u, stream) != 4u) { fclose(stream); return 1; }
    rewind(stream);
    decoded = ast_deserialize(stream);
    fclose(stream);
    if (decoded) { ast_destroy(decoded); return 1; }

    stream = tmpfile();
    if (!stream) return 1;
    if (fwrite(magic, 1u, sizeof(magic), stream) != sizeof(magic) ||
        fwrite(&version, 1u, 2u, stream) != 2u) {
        fclose(stream);
        return 1;
    }
    rewind(stream);
    decoded = ast_deserialize(stream);
    fclose(stream);
    if (decoded) { ast_destroy(decoded); return 1; }
    return 0;
}


static int rejects_all_truncated_ast_payload_prefixes(void) {
    ASTNode* fixture = make_variable_fixture();
    FILE* stream = NULL;
    unsigned char* bytes = NULL;
    long end;
    size_t cut;
    int ok = 0;

    if (!fixture) return 1;
    stream = tmpfile();
    if (!stream) goto done;
    if (!ast_serialize(fixture, stream) || fflush(stream) != 0) {
        fprintf(stderr, "could not serialize nested truncation fixture\\n");
        goto done;
    }
    end = ftell(stream);
    if (end <= 0) {
        fprintf(stderr, "invalid truncation fixture size\\n");
        goto done;
    }
    bytes = (unsigned char*)malloc((size_t)end);
    if (!bytes || fseek(stream, 0L, SEEK_SET) != 0 ||
        fread(bytes, 1u, (size_t)end, stream) != (size_t)end) {
        fprintf(stderr, "could not read truncation fixture\\n");
        goto done;
    }

    for (cut = 0u; cut < (size_t)end; ++cut) {
        int result = rejects_bytes(bytes, cut);
        if (result != 0) {
            fprintf(stderr, "truncated AST payload prefix=%zu/%ld result=%d\\n",
                    cut, end, result);
            goto done;
        }
    }
    ok = 1;

done:
    free(bytes);
    if (stream) fclose(stream);
    ast_destroy(fixture);
    return ok ? 0 : 1;
}

static int rejects_ast_decode_allocation_failures(void) {
    ASTBinaryExpr* root=(ASTBinaryExpr*)ast_create(AST_BINARY_EXPR);
    ASTIdentifier* left=(ASTIdentifier*)ast_create(AST_IDENTIFIER);
    ASTIdentifier* right=(ASTIdentifier*)ast_create(AST_IDENTIFIER);
    FILE* stream=NULL;
    size_t fail_at;
    size_t failed_sites=0u;
    int reached_success=0;
    if(!root || !left || !right) goto done;
    left->name=duplicate_text("left_payload");
    right->name=duplicate_text("right_payload");
    if(!left->name || !right->name) goto done;
    root->left=(ASTNode*)left; root->right=(ASTNode*)right;
    stream=tmpfile();
    if(!stream || !ast_serialize((ASTNode*)root,stream)) goto done;

    /* Root/child nodes, owned strings, validator hash table and both stacks. */
    for(fail_at=1u;fail_at<=9u;++fail_at) {
        ASTNode* decoded;
        size_t allocations;
        rewind(stream);
        ast_test_fail_allocation_at(fail_at);
        decoded=ast_deserialize(stream);
        allocations=ast_test_get_allocation_count();
        ast_test_reset_allocation_failure();
        if(decoded) {
            if(allocations>=fail_at) {
                ast_destroy(decoded);
                goto done;
            }
            ast_destroy(decoded);
            reached_success=1;
            break;
        }
        if(allocations!=fail_at) goto done;
        ++failed_sites;
    }
done:
    ast_test_reset_allocation_failure();
    if(stream) fclose(stream);
    ast_destroy((ASTNode*)root);
    return reached_success && failed_sites==8u ? 0 : 1;
}

static int roundtrips_fast_exec_asm_payload(void) {
    ASTFastExecBlock* fast=(ASTFastExecBlock*)ast_create(AST_FASTEXEC_BLOCK);
    ASTAsmLabel* label=(ASTAsmLabel*)ast_create(AST_ASM_LABEL);
    FILE* stream=NULL;
    ASTNode* decoded=NULL;
    const char* raw="mov rax, %value\r\nadd rax, 1 ; keep %x:reg";
    int valid=0;
    if(!fast || !label) goto done;
    label->label=duplicate_text("ADD");
    label->source_text=duplicate_text(raw);
    if(!label->label || !label->source_text) goto done;
    label->source_length=strlen(raw);
    fast->statements=(ASTNode*)label;
    label=NULL; /* ownership transferred to FASTEXEC */
    if(!ast_validate((ASTNode*)fast,NULL)) goto done;
    stream=tmpfile();
    if(!stream || !ast_serialize((ASTNode*)fast,stream)) goto done;
    rewind(stream);
    decoded=ast_deserialize(stream);
    if(!decoded || !ast_validate(decoded,NULL)) goto done;
    {
        ASTAsmLabel* got=(ASTAsmLabel*)((ASTFastExecBlock*)decoded)->statements;
        valid=got && got->base.type==AST_ASM_LABEL &&
            strcmp(got->label,"ADD")==0 &&
            got->source_length==strlen(raw) &&
            memcmp(got->source_text,raw,got->source_length)==0;
    }
done:
    if(stream) fclose(stream);
    if(decoded) ast_destroy(decoded);
    if(label) ast_destroy((ASTNode*)label);
    if(fast) ast_destroy((ASTNode*)fast);
    return valid ? 0 : 1;
}


/* Context-sensitive AST tags must be encoded under their legal owner. */
static int roundtrips_contextual_ast_tags(void) {
    ASTProgram* program=(ASTProgram*)ast_create(AST_PROGRAM);
    ASTMustDecl* must=(ASTMustDecl*)ast_create(AST_MUST_DECL);
    ASTFuncDecl* fn=(ASTFuncDecl*)ast_create(AST_FUNC_DECL);
    ASTVariadicParam* variadic=(ASTVariadicParam*)ast_create(AST_VARIADIC_PARAM);
    ASTBlock* block=(ASTBlock*)ast_create(AST_BLOCK);
    ASTNode* halt=ast_create(AST_BLOCK_HALT);
    ASTFastExecBlock* fast=(ASTFastExecBlock*)ast_create(AST_FASTEXEC_BLOCK);
    ASTAsmLabel* label=(ASTAsmLabel*)ast_create(AST_ASM_LABEL);
    FILE *a=NULL,*b=NULL; ASTNode *decoded=NULL; int ok=0;
    if(!program||!must||!fn||!variadic||!block||!halt||!fast||!label) goto done;
    must->name=duplicate_text("context_dependency"); must->source=make_identifier_fixture("source_name");
    fn->name=duplicate_text("context_function"); fn->params=(ASTNode*)variadic; variadic=NULL;
    fn->body=ast_create(AST_BLOCK);
    block->statements=halt; halt=NULL;
    label->label=duplicate_text("CTX_LABEL"); label->source_text=duplicate_text("nop");
    label->source_length=3; fast->statements=(ASTNode*)label; label=NULL;
    program->declarations=(ASTNode*)must; must=NULL;
    program->declarations->next=(ASTNode*)fn; fn=NULL;
    program->declarations->next->next=(ASTNode*)block; block=NULL;
    program->declarations->next->next->next=(ASTNode*)fast; fast=NULL;
    if(!ast_validate((ASTNode*)program,NULL)) goto done;
    a=tmpfile(); b=tmpfile(); if(!a||!b) goto done;
    if(!ast_serialize((ASTNode*)program,a)) goto done;
    rewind(a); decoded=ast_deserialize(a); if(!decoded||!ast_validate(decoded,NULL)) goto done;
    if(!ast_serialize(decoded,b)) goto done;
    ok=streams_equal(a,b);
done:
    if(a)fclose(a); if(b)fclose(b); ast_destroy(decoded); ast_destroy((ASTNode*)program);
    ast_destroy((ASTNode*)must); ast_destroy((ASTNode*)fn); ast_destroy((ASTNode*)variadic);
    ast_destroy((ASTNode*)block); ast_destroy(halt); ast_destroy((ASTNode*)fast); ast_destroy((ASTNode*)label);
    return ok?0:1;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: test_ast_deserialize <invalid-output-path>\n");
        return 9;
    }
    if (roundtrips_versioned_source_spans() != 0) { fprintf(stderr, "roundtrips_versioned_source_spans failed.\\n"); return 15; }
    if (reads_v1_function_stream_without_v2_return_child() != 0) { fprintf(stderr, "reads_v1_function_stream_without_v2_return_child failed.\\n"); return 22; }
    if (rejects_ast_decode_allocation_failures() != 0) { fprintf(stderr, "rejects_ast_decode_allocation_failures failed.\\n"); return 23; }
    if (roundtrips_fast_exec_asm_payload() != 0) { fprintf(stderr, "roundtrips_fast_exec_asm_payload failed.\\n"); return 19; }
    if (roundtrips_contextual_ast_tags() != 0) { fprintf(stderr, "roundtrips_contextual_ast_tags failed.\\n"); return 21; }
    if (reads_unprefixed_legacy_stream() != 0) { fprintf(stderr, "reads_unprefixed_legacy_stream failed.\\n"); return 16; }
    if (rejects_unknown_ast_stream_version() != 0) { fprintf(stderr, "rejects_unknown_ast_stream_version failed.\\n"); return 17; }
    if (rejects_truncated_ast_stream_prefixes() != 0) { fprintf(stderr, "rejects_truncated_ast_stream_prefixes failed.\\n"); return 18; }
    if (rejects_all_truncated_ast_payload_prefixes() != 0) { fprintf(stderr, "rejects_all_truncated_ast_payload_prefixes failed.\\n"); return 20; }
    if (roundtrips_populated_ast_types() != 0) { fprintf(stderr, "roundtrips_populated_ast_types failed.\\n"); return 11; }
    if (roundtrips_all_node_type_tags() != 0) { fprintf(stderr, "roundtrips_all_node_type_tags failed.\\n"); return 8; }
    if (roundtrips_struct_literal_nodes() != 0) { fprintf(stderr, "roundtrips_struct_literal_nodes failed.\\n"); return 19; }
    if (roundtrips_block_halt_tag() != 0) { fprintf(stderr, "roundtrips_block_halt_tag failed.\\n"); return 14; }
    if (roundtrips_extern_decl() != 0) { fprintf(stderr, "roundtrips_extern_decl failed.\\n"); return 7; }
    if (roundtrips_special_nodes() != 0) { fprintf(stderr, "roundtrips_special_nodes failed.\\n"); return 4; }
    if (roundtrips_large_sibling_list() != 0) { fprintf(stderr, "roundtrips_large_sibling_list failed.\\n"); return 5; }
    if (!destroys_very_large_sibling_list()) { fprintf(stderr, "destroys_very_large_sibling_list failed.\\n"); return 12; }
    if (!rejects_excessive_serialization_depth()) { fprintf(stderr, "rejects_excessive_serialization_depth failed.\\n"); return 13; }
    if (!parser_save_ir_rejects_invalid_path(argv[1])) return 10;

    struct {
        ASTNodeType type;
        int line;
    } truncated_header = { AST_PROGRAM, 1 };

    if (rejects_bytes(&truncated_header, sizeof(truncated_header)) != 0) {
        return 1;
    }

    struct {
        ASTNodeType type;
        int line;
        int col;
    } invalid_node = { (ASTNodeType)(AST_STRUCT_FIELD_INIT + 1), 1, 1 };

    if (rejects_bytes(&invalid_node, sizeof(invalid_node)) != 0) {
        return 2;
    }

    struct {
        ASTNodeType type;
        int line;
        int col;
        int invalid_string_length;
    } invalid_string = { AST_IDENTIFIER, 1, 1, -2 };

    if (rejects_bytes(&invalid_string, sizeof(invalid_string)) != 0) {
        return 3;
    }

    struct {
        ASTNodeType type;
        int line;
        int col;
        int oversized_string_length;
    } oversized_string = { AST_IDENTIFIER, 1, 1, (16 * 1024 * 1024) + 1 };

    if (rejects_bytes(&oversized_string, sizeof(oversized_string)) != 0) {
        return 6;
    }

    return 0;
}
