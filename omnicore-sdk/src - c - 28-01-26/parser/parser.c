#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// --- Message System ---
#define MAX_ERROR_CODE 4000
static char* GLOBAL_MSG_TABLE[MAX_ERROR_CODE];
static int msg_table_initialized = 0;

void msg_init(const char* path) {
    if (msg_table_initialized) return;
    
    memset(GLOBAL_MSG_TABLE, 0, sizeof(GLOBAL_MSG_TABLE));
    
    FILE* f = fopen(path, "r");
    if (!f) {
        printf("[WARN] Language file not found: %s. Using defaults/IDs.\n", path);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char* eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            int code = atoi(line);
            char* msg = eq + 1;
            size_t len = strlen(msg);
            if (len > 0 && msg[len-1] == '\n') msg[len-1] = '\0';
            
            if (code > 0 && code < MAX_ERROR_CODE) {
                if (GLOBAL_MSG_TABLE[code]) free(GLOBAL_MSG_TABLE[code]);
                GLOBAL_MSG_TABLE[code] = strdup(msg);
            }
        }
    }
    fclose(f);
    msg_table_initialized = 1;
}

const char* msg_get(int id) {
    if (id > 0 && id < MAX_ERROR_CODE && GLOBAL_MSG_TABLE[id]) {
        return GLOBAL_MSG_TABLE[id];
    }
    return "Unknown Message";
}

// Dosyayı belleğe okuyan yardımcı fonksiyon
static char* read_file_content(const char* filename, size_t* out_len) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buffer = malloc(length + 1);
    if (buffer) {
        fread(buffer, 1, length, f);
        buffer[length] = '\0';
    }
    
    if (out_len) *out_len = length;
    fclose(f);
    return buffer;
}

ParserContext* parser_init(const char* filename) {
    ParserContext* ctx = (ParserContext*)malloc(sizeof(ParserContext));
    if (!ctx) return NULL;

    ctx->filename = filename;
    ctx->source_buffer = read_file_content(filename, &ctx->source_len);
    
    if (!ctx->source_buffer) {
        free(ctx);
        return NULL;
    }

    ctx->pos = 0;
    ctx->current_line = 1;
    ctx->current_col = 1;
    ctx->error_count = 0;
    ctx->error_head = NULL;
    ctx->error_tail = NULL;
    
    return ctx;
}

// --- Parser State & Helper Functions ---

typedef struct {
    ParserContext* ctx;
    Token current;
    Token previous;
    int had_error;
    int panic_mode;
} Parser;

static void parser_add_error(ParserContext* ctx, ErrorCode code, int line, int col, const char* message);

static void error_at(Parser* p, Token* token, ErrorCode code, const char* message) {
    if (p->panic_mode) return;
    p->panic_mode = 1;
    
    char full_msg[256];
    if (token->type == TOKEN_EOF) {
        snprintf(full_msg, sizeof(full_msg), msg_get(MSG_AT_END), message);
    } else if (token->type == TOKEN_ERROR) {
        snprintf(full_msg, sizeof(full_msg), "%s", message);
    } else {
        snprintf(full_msg, sizeof(full_msg), msg_get(MSG_AT_TOKEN), token->length, token->start, message);
    }

    parser_add_error(p->ctx, code, token->line, token->col, full_msg);
    p->had_error = 1;
}

static void advance(Parser* p) {
    p->previous = p->current;
    for (;;) {
        p->current = lexer_next_token(p->ctx);
        if (p->current.type != TOKEN_ERROR) break;
        error_at(p, &p->current, ERR_LEXER_ERROR, p->current.start);
    }
}

static void consume(Parser* p, OmniTokenType type, ErrorCode code, int msg_id) {
    if (p->current.type == type) {
        advance(p);
        return;
    }
    error_at(p, &p->current, code, msg_get(msg_id));
}

static int match(Parser* p, OmniTokenType type) {
    if (p->current.type != type) return 0;
    advance(p);
    return 1;
}

static int check(Parser* p, OmniTokenType type) {
    return p->current.type == type;
}

static Token peek_token(Parser* p) {
    size_t old_pos = p->ctx->pos;
    int old_line = p->ctx->current_line;
    int old_col = p->ctx->current_col;
    
    Token t = lexer_next_token(p->ctx);
    
    p->ctx->pos = old_pos;
    p->ctx->current_line = old_line;
    p->ctx->current_col = old_col;
    
    return t;
}

static Token peek_token_ahead(Parser* p, int n) {
    size_t old_pos = p->ctx->pos;
    int old_line = p->ctx->current_line;
    int old_col = p->ctx->current_col;
    
    Token t;
    for (int i = 0; i < n; i++) {
        t = lexer_next_token(p->ctx);
    }
    
    p->ctx->pos = old_pos;
    p->ctx->current_line = old_line;
    p->ctx->current_col = old_col;
    
    return t;
}

// --- Parsing Logic ---

static ASTNode* parse_expression(Parser* p);
static ASTNode* parse_statement(Parser* p);
static ASTNode* parse_block(Parser* p);
static ASTNode* parse_assignment(Parser* p);
static ASTNode* parse_func_decl(Parser* p);
static ASTNode* parse_nexus_flow(Parser* p);
static ASTNode* parse_return_statement(Parser* p);
static ASTNode* parse_var_decl(Parser* p);
static ASTNode* parse_struct_decl(Parser* p, int is_packed, char* name);
static ASTNode* parse_enum_decl(Parser* p, char* name);
static ASTNode* parse_union_decl(Parser* p, char* name);
static ASTNode* parse_newtype_decl(Parser* p);
static ASTNode* parse_group_decl(Parser* p);
static ASTNode* parse_postfix(Parser* p);
static ASTNode* parse_range(Parser* p);
static ASTNode* parse_equality(Parser* p);
static void parse_struct_body(Parser* p, ASTStructDecl* decl);
static ASTNode* parse_comparison(Parser* p);
static ASTNode* parse_additive(Parser* p);
static ASTNode* parse_unary(Parser* p);
static ASTNode* parse_intrinsic_call(Parser* p, ASTNodeType type);
static ASTNode* parse_defer(Parser* p);
static ASTNode* parse_use(Parser* p);
static ASTNode* parse_thread_block(Parser* p, char* name);

// Primary: Literals, Identifiers, Grouping
static ASTNode* parse_primary(Parser* p) {
    if (match(p, TOKEN_NUMBER)) {
        ASTLiteral* node = (ASTLiteral*)ast_create(AST_LITERAL);
        node->base.line = p->previous.line;
        node->base.col = p->previous.col;
        node->type = TOKEN_NUMBER;
        node->string_value = malloc(p->previous.length + 1);
        memcpy(node->string_value, p->previous.start, p->previous.length);
        node->string_value[p->previous.length] = '\0';
        return (ASTNode*)node;
    }
    
    if (match(p, TOKEN_STRING)) {
        ASTLiteral* node = (ASTLiteral*)ast_create(AST_LITERAL);
        node->base.line = p->previous.line;
        node->base.col = p->previous.col;
        node->type = TOKEN_STRING;
        node->string_value = malloc(p->previous.length + 1);
        memcpy(node->string_value, p->previous.start, p->previous.length);
        node->string_value[p->previous.length] = '\0';
        return (ASTNode*)node;
    }

    if (match(p, TOKEN_CHAR)) {
        ASTLiteral* node = (ASTLiteral*)ast_create(AST_LITERAL);
        node->base.line = p->previous.line;
        node->base.col = p->previous.col;
        node->type = TOKEN_CHAR;
        node->string_value = malloc(p->previous.length + 1);
        memcpy(node->string_value, p->previous.start, p->previous.length);
        node->string_value[p->previous.length] = '\0';
        return (ASTNode*)node;
    }

    if (match(p, TOKEN_TRUE)) {
        ASTLiteral* node = (ASTLiteral*)ast_create(AST_LITERAL);
        node->base.line = p->previous.line;
        node->base.col = p->previous.col;
        node->type = TOKEN_TRUE;
        node->string_value = strdup("true");
        return (ASTNode*)node;
    }

    if (match(p, TOKEN_FALSE)) {
        ASTLiteral* node = (ASTLiteral*)ast_create(AST_LITERAL);
        node->base.line = p->previous.line;
        node->base.col = p->previous.col;
        node->type = TOKEN_FALSE;
        node->string_value = strdup("false");
        return (ASTNode*)node;
    }

    if (match(p, TOKEN_IDENTIFIER)) {
        ASTIdentifier* id_node = (ASTIdentifier*)ast_create(AST_IDENTIFIER);
        id_node->base.line = p->previous.line;
        id_node->base.col = p->previous.col;
        id_node->name = malloc(p->previous.length + 1);
        memcpy(id_node->name, p->previous.start, p->previous.length);
        id_node->name[p->previous.length] = '\0';

        return (ASTNode*)id_node;
    }

    if (match(p, TOKEN_KW_FN)) {
        return parse_func_decl(p);
    }

    if (match(p, TOKEN_KW_LOOP)) {
        ASTLoop* loop = (ASTLoop*)ast_create(AST_LOOP);
        loop->base.line = p->previous.line;
        loop->base.col = p->previous.col;
        loop->loop_type = 0;

        consume(p, TOKEN_LPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_LPAREN_FUNC);

        // Argümanları parse et
        ASTNode* first = NULL;
        if (match(p, TOKEN_PRE_VAR)) { // v:
            first = parse_var_decl(p);
        } else if (match(p, TOKEN_PRE_CONST)) { // c:
            first = parse_var_decl(p);
        } else {
            first = parse_expression(p);
        }

        if (match(p, TOKEN_ITERATE)) { // <--
            loop->loop_type = 3; // Foreach
            loop->variable = first;
            loop->collection = parse_expression(p);
        } else if (match(p, TOKEN_COMMA)) {
            loop->loop_type = 2; // For
            loop->init = first;
            loop->condition = parse_expression(p);
            consume(p, TOKEN_COMMA, ERR_PARSER_EXPECTED_EXPRESSION, MSG_EXPECT_COMMA_ROLLING);
            loop->increment = parse_expression(p);
        } else {
            loop->loop_type = 1; // While
            loop->condition = first;
        }
        consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);

        if (match(p, TOKEN_PIPE)) { } // Opsiyonel ->
        
        if (match(p, TOKEN_LBRACE)) loop->body = parse_block(p);
        else error_at(p, &p->current, ERR_PARSER_EXPECTED_BRACE, msg_get(MSG_EXPECT_LBRACE_FUNC));
        return (ASTNode*)loop;
    }

    if (match(p, TOKEN_LPAREN)) {
        // (loop) Kontrolü
        if (match(p, TOKEN_KW_LOOP)) {
            consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);
            
            ASTLoop* loop = (ASTLoop*)ast_create(AST_LOOP);
            loop->base.line = p->previous.line;
            loop->base.col = p->previous.col;
            loop->loop_type = 0; // Infinite default
            // Artık argüman beklemiyoruz, sadece (loop) sonsuz döngüdür.

            // -> { ... }
            if (match(p, TOKEN_PIPE)) {
                // Opsiyonel pipe
            }
            
            if (match(p, TOKEN_LBRACE)) {
                loop->body = parse_block(p);
            } else {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_BRACE, msg_get(MSG_EXPECT_LBRACE_FUNC));
            }
            return (ASTNode*)loop;
        }

        ASTNode* expr = parse_expression(p);
        
        // Tuple Check: (expr, expr, ...)
        if (p->current.type == TOKEN_COMMA) {
            ASTTuple* tuple = (ASTTuple*)ast_create(AST_TUPLE);
            tuple->base.line = expr->line;
            tuple->base.col = expr->col;
            
            ASTNode* head = expr;
            ASTNode* tail = expr;
            
            while (match(p, TOKEN_COMMA)) {
                ASTNode* next_elem = parse_expression(p);
                if (next_elem) {
                    tail->next = next_elem;
                    tail = next_elem;
                }
            }
            tuple->elements = head;
            
            consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);
            return (ASTNode*)tuple;
        }

        consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);
        
        return expr;
    }

    if (match(p, TOKEN_KW_PRNT)) return parse_intrinsic_call(p, AST_PRNT);
    if (match(p, TOKEN_KW_PRMT)) return parse_intrinsic_call(p, AST_PRMT);
    if (match(p, TOKEN_KW_WRITE)) return parse_intrinsic_call(p, AST_WRITE);
    if (match(p, TOKEN_KW_FWRITE)) return parse_intrinsic_call(p, AST_FWRITE);
    if (match(p, TOKEN_KW_TYPEOF)) return parse_intrinsic_call(p, AST_TYPEOF);
    if (match(p, TOKEN_KW_SIZEOF)) return parse_intrinsic_call(p, AST_SIZEOF);
    if (match(p, TOKEN_KW_LEN)) return parse_intrinsic_call(p, AST_LEN);
    if (match(p, TOKEN_KW_IS_OK)) return parse_intrinsic_call(p, AST_IS_OK);
    if (match(p, TOKEN_KW_IS_ERR)) return parse_intrinsic_call(p, AST_IS_ERR);
    if (match(p, TOKEN_KW_IS_SOME)) return parse_intrinsic_call(p, AST_IS_SOME);
    if (match(p, TOKEN_KW_IS_NONE)) return parse_intrinsic_call(p, AST_IS_NONE);
    if (match(p, TOKEN_KW_IS_NULL)) return parse_intrinsic_call(p, AST_IS_NULL);
    if (match(p, TOKEN_KW_PANIC)) return parse_intrinsic_call(p, AST_PANIC);
    if (match(p, TOKEN_KW_EXIT)) return parse_intrinsic_call(p, AST_EXIT);
    if (match(p, TOKEN_KW_CAST)) return parse_intrinsic_call(p, AST_CAST);
    if (match(p, TOKEN_KW_SWAP)) return parse_intrinsic_call(p, AST_SWAP);
    if (match(p, TOKEN_KW_PEEK)) return parse_intrinsic_call(p, AST_PEEK);
    if (match(p, TOKEN_KW_POKE)) return parse_intrinsic_call(p, AST_POKE);
    if (match(p, TOKEN_KW_INB)) return parse_intrinsic_call(p, AST_INB);
    if (match(p, TOKEN_KW_OUTB)) return parse_intrinsic_call(p, AST_OUTB);
    if (match(p, TOKEN_KW_IRQ)) return parse_intrinsic_call(p, AST_IRQ);
    if (match(p, TOKEN_KW_INTR)) return parse_intrinsic_call(p, AST_INTR);
    if (match(p, TOKEN_KW_REG)) return parse_intrinsic_call(p, AST_REG);
    if (match(p, TOKEN_KW_AREA)) return parse_intrinsic_call(p, AST_AREA);
    if (match(p, TOKEN_KW_ZONE)) return parse_intrinsic_call(p, AST_ZONE);
    if (match(p, TOKEN_KW_ADDR)) return parse_intrinsic_call(p, AST_ADDR);
    if (match(p, TOKEN_KW_DATE)) return parse_intrinsic_call(p, AST_DATE);
    if (match(p, TOKEN_KW_DATENOW)) return parse_intrinsic_call(p, AST_DATENOW);
    if (match(p, TOKEN_KW_TIME)) return parse_intrinsic_call(p, AST_TIME);
    if (match(p, TOKEN_KW_CLOCK)) return parse_intrinsic_call(p, AST_CLOCK);
    if (match(p, TOKEN_KW_FREE)) return parse_intrinsic_call(p, AST_FREE);
    if (match(p, TOKEN_KW_SPAWN)) return parse_intrinsic_call(p, AST_SPAWN);
    if (match(p, TOKEN_KW_SEND)) return parse_intrinsic_call(p, AST_SEND);
    if (match(p, TOKEN_KW_RECEIVE)) return parse_intrinsic_call(p, AST_RECEIVE);
    if (match(p, TOKEN_KW_DONE)) return parse_intrinsic_call(p, AST_DONE);
    if (match(p, TOKEN_KW_LEN)) return parse_intrinsic_call(p, AST_LEN);

    // Support for { expr, expr } (Struct Init / Tuple)
    if (match(p, TOKEN_LBRACE)) {
        // { } Empty tuple check
        if (match(p, TOKEN_RBRACE)) {
             ASTTuple* tuple = (ASTTuple*)ast_create(AST_TUPLE);
             tuple->base.line = p->previous.line;
             tuple->base.col = p->previous.col;
             tuple->elements = NULL;
             return (ASTNode*)tuple;
        }

        ASTNode* expr = parse_expression(p);
        if (!expr) return NULL;
        
        ASTTuple* tuple = (ASTTuple*)ast_create(AST_TUPLE);
        tuple->base.line = expr->line;
        tuple->base.col = expr->col;
        
        ASTNode* head = expr;
        ASTNode* tail = expr;
        
        while (match(p, TOKEN_COMMA)) {
            ASTNode* next_elem = parse_expression(p);
            if (next_elem) {
                tail->next = next_elem;
                tail = next_elem;
            }
        }
        tuple->elements = head;
        
        consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
        return (ASTNode*)tuple;
    }

    if (match(p, TOKEN_AI)) {
        ASTAiLogic* ai = (ASTAiLogic*)ast_create(AST_AI_LOGIC);
        ai->base.line = p->previous.line;
        ai->base.col = p->previous.col;
        if (p->current.type == TOKEN_STRING || p->current.type == TOKEN_IDENTIFIER) {
            Token t = p->current;
            ai->query = malloc(t.length + 1);
            memcpy(ai->query, t.start, t.length);
            ai->query[t.length] = '\0';
            advance(p);
        } else {
            ai->query = strdup("auto");
        }
        return (ASTNode*)ai;
    }

    error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(ERR_PARSER_EXPECTED_EXPRESSION));
    return NULL;
}

static ASTNode* parse_intrinsic_call(Parser* p, ASTNodeType type) {
    ASTCall* call = (ASTCall*)ast_create(type);
    call->base.line = p->previous.line;
    call->base.col = p->previous.col;
    call->args = NULL;

    // reg.r(...) case logic
    if (type == AST_REG && match(p, TOKEN_DOT)) {
        if (match(p, TOKEN_IDENTIFIER)) {
            ASTIdentifier* id = (ASTIdentifier*)ast_create(AST_IDENTIFIER);
            id->name = malloc(p->previous.length + 1);
            memcpy(id->name, p->previous.start, p->previous.length);
            id->name[p->previous.length] = '\0';
            call->callee = (ASTNode*)id;
        }
    }

    if (match(p, TOKEN_LPAREN)) {
        if (p->current.type != TOKEN_RPAREN) {
            ASTNode* head = NULL;
            ASTNode* tail = NULL;
            do {
                ASTNode* arg = parse_expression(p);
                if (!head) head = arg; else tail->next = arg;
                tail = arg;
            } while (match(p, TOKEN_COMMA));
            call->args = head;
        }
        consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_ARGS);
    }
    return (ASTNode*)call;
}

static ASTNode* parse_defer(Parser* p) {
    ASTDefer* defer = (ASTDefer*)ast_create(AST_DEFER);
    defer->base.line = p->previous.line;
    defer->base.col = p->previous.col;
    
    if (match(p, TOKEN_LBRACE)) {
        defer->body = parse_block(p);
    } else {
        defer->body = parse_statement(p);
    }
    return (ASTNode*)defer;
}

static ASTNode* parse_use(Parser* p) {
    ASTUse* head = NULL;
    ASTUse* tail = NULL;
    
    do {
        consume(p, TOKEN_IDENTIFIER, ERR_PARSER_EXPECTED_EXPRESSION, MSG_EXPECT_MODULE_NAME);
        ASTUse* use = (ASTUse*)ast_create(AST_USE);
        use->base.line = p->previous.line;
        use->base.col = p->previous.col;
        use->module_name = malloc(p->previous.length + 1);
        memcpy(use->module_name, p->previous.start, p->previous.length);
        use->module_name[p->previous.length] = '\0';
        use->alias = NULL;
        
        if (match(p, TOKEN_KW_AS)) {
            consume(p, TOKEN_IDENTIFIER, ERR_PARSER_EXPECTED_EXPRESSION, MSG_EXPECT_ALIAS_NAME);
            use->alias = malloc(p->previous.length + 1);
            memcpy(use->alias, p->previous.start, p->previous.length);
            use->alias[p->previous.length] = '\0';
        }
        
        if (!head) head = use; else tail->base.next = (ASTNode*)use;
        tail = use;
        
    } while (match(p, TOKEN_COMMA));
    
    consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
    return (ASTNode*)head;
}

static ASTNode* parse_thread_block(Parser* p, char* name) {
    ASTThreadBlock* tb = (ASTThreadBlock*)ast_create(AST_THREAD_BLOCK);
    tb->base.line = p->previous.line;
    tb->base.col = p->previous.col;
    tb->name = name;
    tb->mode = NULL; // Default
    tb->timeout = NULL;

    // The >{ was already matched in parse_statement
    tb->body = parse_block(p);
    
    return (ASTNode*)tb;
}

static ASTNode* parse_postfix(Parser* p) {
    ASTNode* expr = parse_primary(p);
    if (!expr) return NULL;
    
    for (;;) {
        if (match(p, TOKEN_DOT)) {
            // Üye Erişimi: expr.member
            if (match(p, TOKEN_IDENTIFIER)) {
                ASTMemberAccess* node = (ASTMemberAccess*)ast_create(AST_MEMBER_ACCESS);
                node->base.line = p->previous.line;
                node->base.col = p->previous.col;
                node->object = expr;
                
                Token t = p->previous;
                node->member_name = malloc(t.length + 1);
                memcpy(node->member_name, t.start, t.length);
                node->member_name[t.length] = '\0';
                
                expr = (ASTNode*)node;
            } else {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(ERR_PARSER_EXPECTED_EXPRESSION));
            }
        } else if (match(p, TOKEN_LPAREN)) {
            // Fonksiyon Çağrısı: expr(...)
            ASTCall* call = (ASTCall*)ast_create(AST_FUNC_CALL);
            call->base.line = p->previous.line;
            call->base.col = p->previous.col;
            call->callee = expr;
            call->args = NULL;

            if (p->current.type != TOKEN_RPAREN) {
                ASTNode* head = NULL;
                ASTNode* tail = NULL;
                do {
                    ASTNode* arg = parse_expression(p);
                    if (!head) head = arg; else tail->next = arg;
                    tail = arg;
                } while (match(p, TOKEN_COMMA));
                call->args = head;
            }
            consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_ARGS);
            expr = (ASTNode*)call;
        } else if (match(p, TOKEN_INC) || match(p, TOKEN_DEC)) {
            // Postfix Artırma/Azaltma: expr++, expr--
            ASTUnaryExpr* post = (ASTUnaryExpr*)ast_create(AST_POSTFIX_EXPR);
            post->base.line = p->previous.line;
            post->base.col = p->previous.col;
            post->op = p->previous.type;
            post->operand = expr;
            expr = (ASTNode*)post;
        } else {
            break;
        }
    }
    return expr;
}

static ASTNode* parse_unary(Parser* p) {
    // Prefix ++ ve --
    if (match(p, TOKEN_INC) || match(p, TOKEN_DEC)) {
        OmniTokenType op = p->previous.type;
        ASTNode* operand = parse_unary(p);
        if (!operand) return NULL;
        ASTUnaryExpr* unary = (ASTUnaryExpr*)ast_create(AST_UNARY_EXPR);
        unary->base.line = p->previous.line;
        unary->base.col = p->previous.col;
        unary->op = op;
        unary->operand = operand;
        return (ASTNode*)unary;
    }
    if (match(p, TOKEN_NOT) || match(p, TOKEN_MINUS) || match(p, TOKEN_BIT_NOT)) {
        OmniTokenType op = p->previous.type;
        ASTNode* operand = parse_unary(p);
        if (!operand) return NULL;
        ASTUnaryExpr* unary = (ASTUnaryExpr*)ast_create(AST_UNARY_EXPR);
        unary->base.line = p->previous.line;
        unary->base.col = p->previous.col;
        unary->op = op;
        unary->operand = operand;
        return (ASTNode*)unary;
    }
    return parse_postfix(p);
}

// Binary Expressions (Basit öncelik yönetimi: Çarpma/Bölme -> Toplama/Çıkarma)
static ASTNode* parse_term(Parser* p) {
    ASTNode* left = parse_unary(p);
    if (!left) return NULL;
    while (match(p, TOKEN_STAR) || match(p, TOKEN_SLASH)) {
        OmniTokenType op = p->previous.type;
        ASTNode* right = parse_unary(p);
        if (!right) return NULL;
        ASTBinaryExpr* binary = (ASTBinaryExpr*)ast_create(AST_BINARY_EXPR);
        binary->base.line = p->previous.line; // Operatörün satırı
        binary->base.col = p->previous.col;
        binary->left = left;
        binary->right = right;
        binary->op = op;
        left = (ASTNode*)binary;
    }
    return left;
}

static ASTNode* parse_comparison(Parser* p) {
    ASTNode* left = parse_additive(p);
    if (!left) return NULL;
    while (match(p, TOKEN_GT) || match(p, TOKEN_GTE) || 
           match(p, TOKEN_LT) || match(p, TOKEN_LTE)) {
        OmniTokenType op = p->previous.type;
        ASTNode* right = parse_additive(p);
        if (!right) return NULL;
        ASTBinaryExpr* binary = (ASTBinaryExpr*)ast_create(AST_BINARY_EXPR);
        binary->base.line = p->previous.line;
        binary->base.col = p->previous.col;
        binary->left = left;
        binary->right = right;
        binary->op = op;
        left = (ASTNode*)binary;
    }
    return left;
}

static ASTNode* parse_equality(Parser* p) {
    ASTNode* left = parse_comparison(p);
    if (!left) return NULL;
    while (match(p, TOKEN_EQ) || match(p, TOKEN_NEQ) || 
           match(p, TOKEN_STRICT_EQ) || match(p, TOKEN_FORCE_EQ)) {
        OmniTokenType op = p->previous.type;
        ASTNode* right = parse_comparison(p);
        if (!right) return NULL;
        ASTBinaryExpr* binary = (ASTBinaryExpr*)ast_create(AST_BINARY_EXPR);
        binary->base.line = p->previous.line;
        binary->base.col = p->previous.col;
        binary->left = left;
        binary->right = right;
        binary->op = op;
        left = (ASTNode*)binary;
    }
    return left;
}

static ASTNode* parse_additive(Parser* p) {
    ASTNode* left = parse_term(p);
    if (!left) return NULL;
    while (match(p, TOKEN_PLUS) || match(p, TOKEN_MINUS)) {
        OmniTokenType op = p->previous.type;
        ASTNode* right = parse_term(p);
        if (!right) return NULL;
        ASTBinaryExpr* binary = (ASTBinaryExpr*)ast_create(AST_BINARY_EXPR);
        binary->base.line = p->previous.line;
        binary->base.col = p->previous.col;
        binary->left = left;
        binary->right = right;
        binary->op = op;
        left = (ASTNode*)binary;
    }
    return left;
}

static ASTNode* parse_range(Parser* p) {
    ASTNode* expr = parse_equality(p);
    if (!expr) return NULL;

    if (match(p, TOKEN_RANGE)) {
        ASTNode* right = parse_equality(p);
        if (!right) return NULL;
        ASTBinaryExpr* range = (ASTBinaryExpr*)ast_create(AST_BINARY_EXPR);
        range->base.line = p->previous.line;
        range->base.col = p->previous.col;
        range->op = TOKEN_RANGE;
        range->left = expr;
        range->right = right;
        return (ASTNode*)range;
    }
    return expr;
}

static ASTNode* parse_nexus_flow(Parser* p) {
    ASTNode* expr = parse_range(p);
    if (!expr) return NULL;
    
        if (match(p, TOKEN_ELSE_FLOW) || match(p, TOKEN_PIPE) || 
            match(p, TOKEN_IGNORE) || match(p, TOKEN_IF_FLOW) || 
            match(p, TOKEN_FALLBACK) || match(p, TOKEN_RELOCATE) ||
            match(p, TOKEN_LSHIFT) || match(p, TOKEN_RSHIFT) ||
            p->current.type == TOKEN_QMARK) {
            
        OmniTokenType op;
        int retry_count = 0;
        int retry_delay = 0;

        // Rolling Check: ?(n,ms)->
        if (p->current.type == TOKEN_QMARK) {
            advance(p); // Consume ?
            if (match(p, TOKEN_LPAREN)) {
                if (match(p, TOKEN_NUMBER)) retry_count = atoi(p->previous.start);
                consume(p, TOKEN_COMMA, ERR_PARSER_EXPECTED_EXPRESSION, MSG_EXPECT_COMMA_ROLLING);
                if (match(p, TOKEN_NUMBER)) retry_delay = atoi(p->previous.start);
                consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_ROLLING);
                consume(p, TOKEN_PIPE, ERR_PARSER_UNEXPECTED_TOKEN, MSG_EXPECT_PIPE_ROLLING);
                op = TOKEN_PIPE;
            } else {
                error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN, msg_get(MSG_UNEXPECTED_QMARK));
                return expr;
            }
        } else {
            op = p->previous.type;
        }

        ASTNode* right = parse_range(p);
        if (!right) return NULL;
        
        if (op == TOKEN_PIPE || op == TOKEN_LSHIFT || op == TOKEN_RSHIFT || op == TOKEN_RELOCATE) {
            ASTNexusFlow* pipe = (ASTNexusFlow*)ast_create(AST_NEXUS_PIPE);
            pipe->base.line = p->previous.line;
            pipe->base.col = p->previous.col;
            pipe->op = op;
            pipe->left = expr;
            pipe->right = right;
            expr = (ASTNode*)pipe;
        } else {
            ASTNexusFlow* flow = (ASTNexusFlow*)ast_create(AST_NEXUS_FLOW);
            flow->base.line = p->previous.line;
            flow->base.col = p->previous.col;
            flow->op = op;
            flow->retry_count = retry_count;
            flow->retry_delay = retry_delay;
            flow->left = expr;
            flow->right = right;
            expr = (ASTNode*)flow;
        }
    }

    if (match(p, TOKEN_UNPACK)) {
        ASTUnpack* unpack = (ASTUnpack*)ast_create(AST_UNPACK);
        unpack->base.line = p->previous.line;
        unpack->base.col = p->previous.col;
        unpack->expr = expr;
        expr = (ASTNode*)unpack;
        return parse_nexus_flow(p); 
    }

    if (match(p, TOKEN_ERR_HANDLER)) {
        if (expr->type == AST_NEXUS_FLOW || expr->type == AST_NEXUS_PIPE) {
            ASTNexusFlow* flow = (ASTNexusFlow*)expr;
            flow->catch_block = parse_block(p);
        } else {
             error_at(p, &p->previous, ERR_PARSER_UNEXPECTED_TOKEN, "Hata yakalayıcı (e) sadece akış (flow) operatörlerinden sonra gelebilir.");
        }
    }

    return expr;
}

static ASTNode* parse_assignment(Parser* p) {
    ASTNode* expr = parse_nexus_flow(p);
    if (!expr) return NULL;

    if (match(p, TOKEN_CAPTURE)) {
        // Nexus Capture: left <- right
        if (expr->type != AST_IDENTIFIER) {
             error_at(p, &p->previous, ERR_PARSER_INVALID_VAR, msg_get(MSG_INVALID_CAPTURE));
             return expr;
        }
        
        ASTNexusCapture* capture = (ASTNexusCapture*)ast_create(AST_NEXUS_CAPTURE);
        capture->base.line = p->previous.line;
        capture->base.col = p->previous.col;
        // ASTIdentifier ismini kopyalıyoruz
        capture->handler_name = strdup(((ASTIdentifier*)expr)->name);
        capture->expr = parse_assignment(p); // Sağ taraf tekrar assignment olabilir
        return (ASTNode*)capture;
    }

    if (match(p, TOKEN_ASSIGN)) {
        ASTNode* right = parse_assignment(p);
        if (!right) return NULL;
        ASTBinaryExpr* assign = (ASTBinaryExpr*)ast_create(AST_ASSIGNMENT);
        assign->base.line = p->previous.line;
        assign->base.col = p->previous.col;
        assign->op = TOKEN_ASSIGN;
        assign->left = expr;
        assign->right = right;
        return (ASTNode*)assign;
    }

    if (match(p, TOKEN_FORCE_ASSIGN)) {
        ASTNode* right = parse_assignment(p);
        if (!right) return NULL;
        ASTBinaryExpr* assign = (ASTBinaryExpr*)ast_create(AST_ASSIGNMENT);
        assign->base.line = p->previous.line;
        assign->base.col = p->previous.col;
        assign->op = TOKEN_FORCE_ASSIGN;
        assign->left = expr;
        assign->right = right;
        return (ASTNode*)assign;
    }
    
    return expr;
}

static ASTNode* parse_expression(Parser* p) {
    // İfade hiyerarşisi: Assignment/Capture -> Nexus Flow -> Additive ...
    return parse_assignment(p);
}

static void parse_struct_body(Parser* p, ASTStructDecl* decl) {
    consume(p, TOKEN_LBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_LBRACE_FUNC);
    
    ASTNode* head = NULL;
    ASTNode* tail = NULL;
    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        if (match(p, TOKEN_PRE_VAR)) {
            ASTNode* field = parse_var_decl(p);
            if (!head) head = field; else tail->next = field;
            tail = field;
            if (match(p, TOKEN_COMMA)) {} // Opsiyonel virgül
        } else {
            advance(p); // Hata durumunda ilerle
        }
    }
    decl->fields = head;
    consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
}

// Variable Declaration: v:name = expr;
static ASTNode* parse_var_decl(Parser* p) {
    // v: veya c: tüketildi, p->previous içinde
    int is_const = (p->previous.type == TOKEN_PRE_CONST);
    Token t = p->previous;

    // Lexer "v:isim" şeklinde tek token döndürdüğü için ismi ayıklıyoruz
    int prefix_len = (t.type == TOKEN_PRE_VAR || t.type == TOKEN_PRE_CONST) ? 2 : 0;
    
    ASTVarDecl* decl = (ASTVarDecl*)ast_create(AST_VAR_DECL);
    decl->base.line = t.line;
    decl->base.col = t.col;
    decl->is_const = is_const;
    decl->inline_type = NULL;
    
    if (t.length > prefix_len) {
        decl->name = malloc(t.length - prefix_len + 1);
        memcpy(decl->name, t.start + prefix_len, t.length - prefix_len); 
        decl->name[t.length - prefix_len] = '\0';
    } else if (t.length == prefix_len) {
        // Separated: v: name
        if (match(p, TOKEN_IDENTIFIER)) {
            Token id = p->previous;
            decl->name = malloc(id.length + 1);
            memcpy(decl->name, id.start, id.length);
            decl->name[id.length] = '\0';
        } else {
            error_at(p, &p->current, ERR_PARSER_INVALID_VAR, msg_get(ERR_PARSER_INVALID_VAR));
        }
    } else {
        error_at(p, &p->previous, ERR_PARSER_INVALID_VAR, msg_get(ERR_PARSER_INVALID_VAR));
    }

    // Tip Tanımı: !Type
    if (match(p, TOKEN_NOT)) {
        decl->is_hard_typed = 1;
        
        if (match(p, TOKEN_KW_STRUCT)) {
             decl->type_name = strdup("struct");
             if (p->current.type == TOKEN_LBRACE) {
                 ASTStructDecl* sdecl = (ASTStructDecl*)ast_create(AST_STRUCT_DECL);
                 sdecl->base.line = p->previous.line;
                 sdecl->base.col = p->previous.col;
                 parse_struct_body(p, sdecl);
                 decl->inline_type = (ASTNode*)sdecl;
             }
        } else if (match(p, TOKEN_KW_UNION)) {
             decl->type_name = strdup("union");
             if (p->current.type == TOKEN_LBRACE) {
                 ASTStructDecl* sdecl = (ASTStructDecl*)ast_create(AST_UNION_DECL);
                 sdecl->base.line = p->previous.line;
                 sdecl->base.col = p->previous.col;
                 sdecl->is_union = 1;
                 parse_struct_body(p, sdecl);
                 decl->inline_type = (ASTNode*)sdecl;
             }
        } else if ((p->current.type >= TOKEN_TYPE_U8 && p->current.type <= TOKEN_TYPE_ANY) || p->current.type == TOKEN_IDENTIFIER) {
             Token t = p->current;
             decl->type_name = malloc(t.length + 1);
             memcpy(decl->type_name, t.start, t.length);
             decl->type_name[t.length] = '\0';
             advance(p);
        } else {
             error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_RET_TYPE));
        }
    }

    if (match(p, TOKEN_ASSIGN)) {
        decl->initializer = parse_expression(p);
    }

    return (ASTNode*)decl;
}

static ASTNode* parse_return_statement(Parser* p) {
    ASTUnaryExpr* ret = (ASTUnaryExpr*)ast_create(AST_RETURN);
    ret->base.line = p->previous.line;
    ret->base.col = p->previous.col;
    ret->op = TOKEN_KW_RET;
    
    if (!match(p, TOKEN_SEMICOLON)) {
        ret->operand = parse_expression(p);
        consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_RET);
    } else {
        ret->operand = NULL;
    }
    return (ASTNode*)ret;
}

static ASTNode* parse_func_decl(Parser* p) {
    ASTFuncDecl* func = (ASTFuncDecl*)ast_create(AST_FUNC_DECL);
    func->base.line = p->previous.line;
    func->base.col = p->previous.col;

    if (p->previous.type == TOKEN_PRE_FUNC) {
        // f:isim formatı
        Token t = p->previous;
        func->name = malloc(t.length - 2 + 1);
        memcpy(func->name, t.start + 2, t.length - 2);
        func->name[t.length - 2] = '\0';
    } else if (match(p, TOKEN_IDENTIFIER)) {
        func->name = malloc(p->previous.length + 1);
        memcpy(func->name, p->previous.start, p->previous.length);
        func->name[p->previous.length] = '\0';
    } else {
        func->name = NULL; // Anonim/Lambda fonksiyon
    }

    consume(p, TOKEN_LPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_LPAREN_FUNC);
    
    // Parametreleri parse et
    ASTNode* param_head = NULL;
    ASTNode* param_tail = NULL;

    if (p->current.type != TOKEN_RPAREN) {
        do {
            if (match(p, TOKEN_IDENTIFIER)) {
                ASTVarDecl* param = (ASTVarDecl*)ast_create(AST_VAR_DECL);
                param->base.line = p->previous.line;
                param->base.col = p->previous.col;
                param->is_const = 0;
                
                param->name = malloc(p->previous.length + 1);
                memcpy(param->name, p->previous.start, p->previous.length);
                param->name[p->previous.length] = '\0';
                
                if (param_head == NULL) {
                    param_head = (ASTNode*)param;
                    param_tail = (ASTNode*)param;
                } else {
                    param_tail->next = (ASTNode*)param;
                    param_tail = (ASTNode*)param;
                }

                // Parametre Tipi: !type veya type
                int has_bang = 0;
                if (match(p, TOKEN_NOT)) has_bang = 1;

                if ((p->current.type >= TOKEN_TYPE_U8 && p->current.type <= TOKEN_TYPE_ANY) || p->current.type == TOKEN_IDENTIFIER) {
                     Token t = p->current;
                     int len = t.length + (has_bang ? 1 : 0);
                     param->type_name = malloc(len + 1);
                     if (has_bang) {
                         param->type_name[0] = '!';
                         memcpy(param->type_name + 1, t.start, t.length);
                     } else {
                         memcpy(param->type_name, t.start, t.length);
                     }
                     param->type_name[len] = '\0';
                     param->is_hard_typed = has_bang;
                     advance(p);
                }
            } else {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_PARAM_NAME));
            }
        } while (match(p, TOKEN_COMMA));
    }
    func->params = param_head;

    consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_PARAMS);

    // Lambda Gövdesi: > expr
    if (match(p, TOKEN_GT)) {
        ASTNode* expr = parse_expression(p);
        if (!expr) return NULL;
        
        // İfadeyi bir blok ve return içine sarıyoruz
        ASTBlock* block = (ASTBlock*)ast_create(AST_BLOCK);
        block->base.line = func->base.line;
        block->base.col = func->base.col;
        
        ASTUnaryExpr* ret = (ASTUnaryExpr*)ast_create(AST_RETURN);
        ret->base.line = func->base.line;
        ret->base.col = func->base.col;
        ret->op = TOKEN_KW_RET;
        ret->operand = expr;
        
        block->statements = (ASTNode*)ret;
        func->body = (ASTNode*)block;
        return (ASTNode*)func;
    }

    // Dönüş Tipi: type (veya !type)
    int has_bang = 0;
    if (match(p, TOKEN_NOT)) {
        has_bang = 1;
    }

    if ((p->current.type >= TOKEN_TYPE_U8 && p->current.type <= TOKEN_TYPE_ANY) || p->current.type == TOKEN_IDENTIFIER) {
         Token t = p->current;
         int len = t.length + (has_bang ? 1 : 0);
         func->return_type = malloc(len + 1);
         if (has_bang) {
             func->return_type[0] = '!';
             memcpy(func->return_type + 1, t.start, t.length);
         } else {
             memcpy(func->return_type, t.start, t.length);
         }
         func->return_type[len] = '\0';
         advance(p);
    } else if (has_bang) {
         error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_TYPE_BANG));
    }

    consume(p, TOKEN_LBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_LBRACE_FUNC);
    func->body = parse_block(p);
    return (ASTNode*)func;
}

static ASTNode* parse_block(Parser* p) {
    ASTBlock* block = (ASTBlock*)ast_create(AST_BLOCK);
    block->base.line = p->current.line; // { karakterinin konumu
    block->base.col = p->current.col;
    ASTNode* head = NULL;
    ASTNode* tail = NULL;

    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        ASTNode* stmt = parse_statement(p);
        if (stmt) {
            if (!head) head = stmt;
            else tail->next = stmt;
            tail = stmt;
        } else {
            advance(p);
        }
    }
    consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
    block->statements = head;
    return (ASTNode*)block;
}

static ASTNode* parse_struct_decl(Parser* p, int is_packed, char* name) {
    ASTStructDecl* decl = (ASTStructDecl*)ast_create(AST_STRUCT_DECL);
    decl->base.line = p->previous.line;
    decl->base.col = p->previous.col;
    decl->is_packed = is_packed;
    decl->is_union = 0;

    // struct:Name
    if (name && name[0] != '\0') {
        decl->name = name;
    } else {
        if (name) free(name);

        if (p->previous.type == TOKEN_PRE_STRUCT) {
             Token t = p->previous;
             if (t.length > 7) {
                 decl->name = malloc(t.length - 7 + 1);
                 memcpy(decl->name, t.start + 7, t.length - 7);
                 decl->name[t.length - 7] = '\0';
             } else if (match(p, TOKEN_IDENTIFIER)) {
                 Token t = p->previous;
                 decl->name = malloc(t.length + 1);
                 memcpy(decl->name, t.start, t.length);
                 decl->name[t.length] = '\0';
             } else {
                 error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_FUNC_NAME));
             }
        } else {
            if (p->current.type == TOKEN_COLON) {
                advance(p);
            }

            if (match(p, TOKEN_IDENTIFIER)) {
                Token t = p->previous;
                decl->name = malloc(t.length + 1);
                memcpy(decl->name, t.start, t.length);
                decl->name[t.length] = '\0';
            } else {
                error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_FUNC_NAME));
            }
        }
    }

    parse_struct_body(p, decl);
    return (ASTNode*)decl;
}

static ASTNode* parse_union_decl(Parser* p, char* name) {
    ASTStructDecl* decl = (ASTStructDecl*)ast_create(AST_UNION_DECL);
    decl->base.line = p->previous.line;
    decl->base.col = p->previous.col;
    decl->is_packed = 0;
    decl->is_union = 1;

    if (name && name[0] != '\0') {
        decl->name = name;
    } else {
        if (name) free(name);

        if (p->previous.type == TOKEN_PRE_UNION) {
             Token t = p->previous;
             if (t.length > 6) {
                 decl->name = malloc(t.length - 6 + 1);
                 memcpy(decl->name, t.start + 6, t.length - 6);
                 decl->name[t.length - 6] = '\0';
             } else if (match(p, TOKEN_IDENTIFIER)) {
                 Token t = p->previous;
                 decl->name = malloc(t.length + 1);
                 memcpy(decl->name, t.start, t.length);
                 decl->name[t.length] = '\0';
             } else {
                 error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_FUNC_NAME));
             }
        } else {
            if (p->current.type == TOKEN_COLON) {
                advance(p);
            }

            if (match(p, TOKEN_IDENTIFIER)) {
                Token t = p->previous;
                decl->name = malloc(t.length + 1);
                memcpy(decl->name, t.start, t.length);
                decl->name[t.length] = '\0';
            }
        }
    }

    parse_struct_body(p, decl);
    return (ASTNode*)decl;
}

static ASTNode* parse_enum_decl(Parser* p, char* name) {
    ASTEnumDecl* decl = (ASTEnumDecl*)ast_create(AST_ENUM_DECL);
    decl->base.line = p->previous.line;
    
    if (name && name[0] != '\0') {
        decl->name = name;
    } else {
        if (name) free(name);

        if (p->previous.type == TOKEN_PRE_ENUM) {
             Token t = p->previous;
             if (t.length > 5) {
                 decl->name = malloc(t.length - 5 + 1);
                 memcpy(decl->name, t.start + 5, t.length - 5);
                 decl->name[t.length - 5] = '\0';
             } else if (match(p, TOKEN_IDENTIFIER)) {
                 Token t = p->previous;
                 decl->name = malloc(t.length + 1);
                 memcpy(decl->name, t.start, t.length);
                 decl->name[t.length] = '\0';
             } else {
                 error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_FUNC_NAME));
             }
        } else {
            if (p->current.type == TOKEN_COLON) {
                advance(p);
            }

            if (match(p, TOKEN_IDENTIFIER)) {
                Token t = p->previous;
                decl->name = malloc(t.length + 1);
                memcpy(decl->name, t.start, t.length);
                decl->name[t.length] = '\0';
            }
        }
    }

    consume(p, TOKEN_LBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_LBRACE_FUNC);
    ASTNode* head = NULL;
    ASTNode* tail = NULL;
    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        if (match(p, TOKEN_IDENTIFIER)) {
            ASTEnumVariant* var = (ASTEnumVariant*)ast_create(AST_ENUM_VARIANT);
            Token t = p->previous;
            var->name = malloc(t.length + 1);
            memcpy(var->name, t.start, t.length);
            var->name[t.length] = '\0';
            var->data = NULL;
            
            // Veri taşıyan enum: Variant(type)
            if (match(p, TOKEN_LPAREN)) {
                ASTNode* data_head = NULL;
                ASTNode* data_tail = NULL;

                if (p->current.type != TOKEN_RPAREN) {
                    do {
                        if (match(p, TOKEN_IDENTIFIER)) {
                            ASTVarDecl* field = (ASTVarDecl*)ast_create(AST_VAR_DECL);
                            field->base.line = p->previous.line;
                            field->base.col = p->previous.col;
                            field->is_const = 1; // Enum fields are conceptually const
                            
                            field->name = malloc(p->previous.length + 1);
                            memcpy(field->name, p->previous.start, p->previous.length);
                            field->name[p->previous.length] = '\0';
                            
                            // Tip: !type
                            consume(p, TOKEN_NOT, ERR_PARSER_UNEXPECTED_TOKEN, MSG_EXPECT_TYPE_BANG);
                            field->is_hard_typed = 1;

                            if ((p->current.type >= TOKEN_TYPE_U8 && p->current.type <= TOKEN_TYPE_ANY) || p->current.type == TOKEN_IDENTIFIER) {
                                Token t_type = p->current;
                                field->type_name = malloc(t_type.length + 1);
                                memcpy(field->type_name, t_type.start, t_type.length);
                                field->type_name[t_type.length] = '\0';
                                advance(p);
                            } else {
                                error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_RET_TYPE));
                            }

                            if (!data_head) data_head = (ASTNode*)field;
                            else data_tail->next = (ASTNode*)field;
                            data_tail = (ASTNode*)field;
                        } else {
                             error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, "Expect field name in enum variant.");
                        }
                    } while (match(p, TOKEN_COMMA));
                }
                var->data = data_head;
                consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_PARAMS);
            }

            if (!head) head = (ASTNode*)var; else tail->next = (ASTNode*)var;
            tail = (ASTNode*)var;
            if (match(p, TOKEN_COMMA)) {}
        } else {
            advance(p);
        }
    }
    decl->variants = head;
    consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
    return (ASTNode*)decl;
}

static ASTNode* parse_newtype_decl(Parser* p) {
    // nt:Name = Type; veya nt:struct:Name
    Token t = p->previous;
    // nt: prefix'ini atla
    char* raw = malloc(t.length + 1);
    memcpy(raw, t.start, t.length);
    raw[t.length] = '\0';
    char* name_part = raw + 3; // "nt:"
    char* decl_name = NULL;

    // 1. Contiguous cases: nt:struct:Name, nt:enum:Name, nt:union:Name
    if (strncmp(name_part, "struct:", 7) == 0) {
        decl_name = malloc(strlen(name_part + 7) + 1);
        strcpy(decl_name, name_part + 7);
        free(raw);
        return parse_struct_decl(p, 0, decl_name);
    }
    if (strncmp(name_part, "enum:", 5) == 0) {
        decl_name = malloc(strlen(name_part + 5) + 1);
        strcpy(decl_name, name_part + 5);
        free(raw);
        return parse_enum_decl(p, decl_name);
    }
    if (strncmp(name_part, "union:", 6) == 0) {
        decl_name = malloc(strlen(name_part + 6) + 1);
        strcpy(decl_name, name_part + 6);
        free(raw);
        return parse_union_decl(p, decl_name);
    }

    // 2. Check for separated keywords or contiguous keyword tokens (nt:struct)
    int is_struct = (strcmp(name_part, "struct") == 0);
    int is_enum = (strcmp(name_part, "enum") == 0);
    int is_union = (strcmp(name_part, "union") == 0);
    int is_separated = (*name_part == '\0');

    if (is_struct || is_enum || is_union || is_separated) {
        free(raw);
        
        OmniTokenType current_type = p->current.type;
        int type_kind = 0; // 1: struct, 2: enum, 3: union

        if (is_struct) type_kind = 1;
        else if (is_enum) type_kind = 2;
        else if (is_union) type_kind = 3;
        else if (is_separated) {
            if (current_type == TOKEN_KW_STRUCT || current_type == TOKEN_PRE_STRUCT) type_kind = 1;
            else if (current_type == TOKEN_KW_ENUM || current_type == TOKEN_PRE_ENUM) type_kind = 2;
            else if (current_type == TOKEN_KW_UNION || current_type == TOKEN_PRE_UNION) type_kind = 3;
        }

        if (type_kind > 0) {
            // If separated, consume the keyword
            if (is_separated) {
                Token t_cur = p->current;
                int prefix_len = 0;
                if (type_kind == 1 && t_cur.type == TOKEN_PRE_STRUCT) prefix_len = 7;
                if (type_kind == 2 && t_cur.type == TOKEN_PRE_ENUM) prefix_len = 5;
                if (type_kind == 3 && t_cur.type == TOKEN_PRE_UNION) prefix_len = 6;

                advance(p); // Consume keyword/prefix

                if (prefix_len > 0 && t_cur.length > prefix_len) {
                    // Name is attached (e.g. struct:Name)
                    decl_name = malloc(t_cur.length - prefix_len + 1);
                    memcpy(decl_name, t_cur.start + prefix_len, t_cur.length - prefix_len);
                    decl_name[t_cur.length - prefix_len] = '\0';
                    
                    if (type_kind == 1) return parse_struct_decl(p, 0, decl_name);
                    if (type_kind == 2) return parse_enum_decl(p, decl_name);
                    if (type_kind == 3) return parse_union_decl(p, decl_name);
                }
            }

            // Check for C-style Definition: { ... } Name;
            if (p->current.type == TOKEN_LBRACE) {
                if (type_kind == 1 || type_kind == 3) {
                     ASTStructDecl* decl = (ASTStructDecl*)ast_create(type_kind == 1 ? AST_STRUCT_DECL : AST_UNION_DECL);
                     decl->base.line = p->previous.line;
                     decl->base.col = p->previous.col;
                     decl->is_packed = 0;
                     decl->is_union = (type_kind == 3);
                     
                     consume(p, TOKEN_LBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_LBRACE_FUNC);
                     
                     ASTNode* head = NULL;
                     ASTNode* tail = NULL;
                     while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
                        if (match(p, TOKEN_PRE_VAR)) {
                            ASTNode* field = parse_var_decl(p);
                            if (!head) head = field; else tail->next = field;
                            tail = field;
                            if (match(p, TOKEN_COMMA)) {} 
                        } else {
                            advance(p);
                        }
                     }
                     decl->fields = head;
                     consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
                     
                     if (match(p, TOKEN_IDENTIFIER)) {
                         Token t_id = p->previous;
                         decl->name = malloc(t_id.length + 1);
                         memcpy(decl->name, t_id.start, t_id.length);
                         decl->name[t_id.length] = '\0';
                     } else {
                         error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_FUNC_NAME));
                     }
                     consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
                     return (ASTNode*)decl;
                } else {
                    // Enum definition: nt: enum { ... } Name;
                    ASTEnumDecl* decl = (ASTEnumDecl*)ast_create(AST_ENUM_DECL);
                    decl->base.line = p->previous.line;
                    
                    consume(p, TOKEN_LBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_LBRACE_FUNC);
                    ASTNode* head = NULL;
                    ASTNode* tail = NULL;
                    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
                        if (match(p, TOKEN_IDENTIFIER)) {
                            ASTEnumVariant* var = (ASTEnumVariant*)ast_create(AST_ENUM_VARIANT);
                            Token t_var = p->previous;
                            var->name = malloc(t_var.length + 1);
                            memcpy(var->name, t_var.start, t_var.length);
                            var->name[t_var.length] = '\0';
                            var->data = NULL;
                            
                            if (match(p, TOKEN_LPAREN)) {
                                while(p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) advance(p);
                                consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_PARAMS);
                            }

                            if (!head) head = (ASTNode*)var; else tail->next = (ASTNode*)var;
                            tail = (ASTNode*)var;
                            if (match(p, TOKEN_COMMA)) {}
                        } else {
                            advance(p);
                        }
                    }
                    decl->variants = head;
                    consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
                    
                    if (match(p, TOKEN_IDENTIFIER)) {
                         Token t_id = p->previous;
                         decl->name = malloc(t_id.length + 1);
                         memcpy(decl->name, t_id.start, t_id.length);
                         decl->name[t_id.length] = '\0';
                    } else {
                         error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_FUNC_NAME));
                    }
                    consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
                    return (ASTNode*)decl;
                }
            } else {
                // Declaration: nt: struct : Name or nt: struct Name
                if (p->current.type == TOKEN_COLON) {
                    advance(p);
                }
                if (match(p, TOKEN_IDENTIFIER)) {
                    Token id = p->previous;
                    decl_name = malloc(id.length + 1);
                    memcpy(decl_name, id.start, id.length);
                    decl_name[id.length] = '\0';
                }
                
                if (type_kind == 1) return parse_struct_decl(p, 0, decl_name);
                if (type_kind == 2) return parse_enum_decl(p, decl_name);
                if (type_kind == 3) return parse_union_decl(p, decl_name);
            }
        }
    }

    ASTNewTypeDecl* decl = (ASTNewTypeDecl*)ast_create(AST_NEWTYPE_DECL);
    decl->name = malloc(strlen(name_part) + 1);
    strcpy(decl->name, name_part);
    free(raw);

    consume(p, TOKEN_ASSIGN, ERR_PARSER_UNEXPECTED_TOKEN, MSG_UNEXPECTED_CHAR);
    // Tip ismini al (Identifier veya primitive type)
    if (p->current.type >= TOKEN_TYPE_U8 && p->current.type <= TOKEN_TYPE_ANY) advance(p);
    else if (p->current.type == TOKEN_IDENTIFIER) advance(p);
    
    Token type_tok = p->previous;
    decl->target_type = malloc(type_tok.length + 1);
    memcpy(decl->target_type, type_tok.start, type_tok.length);
    decl->target_type[type_tok.length] = '\0';
    
    consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
    return (ASTNode*)decl;
}

static ASTNode* parse_group_decl(Parser* p) {
    ASTGroupDecl* group = (ASTGroupDecl*)ast_create(AST_GROUP_DECL);
    group->base.line = p->previous.line;
    group->base.col = p->previous.col;

    if (match(p, TOKEN_IDENTIFIER)) {
        Token t = p->previous;
        group->name = malloc(t.length + 1);
        memcpy(group->name, t.start, t.length);
        group->name[t.length] = '\0';
    }

    consume(p, TOKEN_LBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_LBRACE_FUNC);

    ASTNode* head = NULL;
    ASTNode* tail = NULL;

    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        ASTNode* member = NULL;
        
        // Method Mapping: identifier => f:name...
        if (p->current.type == TOKEN_IDENTIFIER) {
            Token next = peek_token(p);
            if (next.type == TOKEN_FAT_ARROW) {
                ASTMethod* method = (ASTMethod*)ast_create(AST_METHOD);
                method->base.line = p->current.line;
                method->base.col = p->current.col;
                method->public_name = malloc(p->current.length + 1);
                memcpy(method->public_name, p->current.start, p->current.length);
                method->public_name[p->current.length] = '\0';
                advance(p); // identifier
                advance(p); // =>
                
                if (match(p, TOKEN_KW_FN) || match(p, TOKEN_PRE_FUNC)) {
                    method->implementation = parse_func_decl(p);
                } else {
                    error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, "Expected function after '=>'");
                }
                member = (ASTNode*)method;
                // No semicolon after method in some cases, but let's check
                if (match(p, TOKEN_SEMICOLON)) {} 
            }
        }
        
        if (!member) {
            if (match(p, TOKEN_KW_STRUCT) || match(p, TOKEN_PRE_STRUCT)) {
                member = parse_struct_decl(p, 0, NULL);
            } else if (match(p, TOKEN_NOT)) {
                 if (match(p, TOKEN_KW_STRUCT) || match(p, TOKEN_PRE_STRUCT)) {
                     member = parse_struct_decl(p, 1, NULL);
                 }
            } else if (match(p, TOKEN_KW_ENUM) || match(p, TOKEN_PRE_ENUM)) {
                member = parse_enum_decl(p, NULL);
            } else if (match(p, TOKEN_KW_UNION) || match(p, TOKEN_PRE_UNION)) {
                member = parse_union_decl(p, NULL);
            } else if (match(p, TOKEN_PRE_VAR) || match(p, TOKEN_PRE_CONST)) {
                member = parse_var_decl(p);
                consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
            } else if (match(p, TOKEN_KW_FN) || match(p, TOKEN_PRE_FUNC)) {
                member = parse_func_decl(p);
            } else {
                advance(p);
                continue;
            }
        }

        if (member) {
            if (!head) head = member; else tail->next = member;
            tail = member;
        }
    }

    consume(p, TOKEN_RBRACE, ERR_PARSER_EXPECTED_BRACE, MSG_EXPECT_RBRACE_BLOCK);
    group->members = head;
    return (ASTNode*)group;
}

static ASTNode* parse_statement(Parser* p) {
    if (p->panic_mode) p->panic_mode = 0; // Her yeni ifadede hata modunu sıfırla (Basit senkronizasyon)

    if (match(p, TOKEN_SEMICOLON)) {
        ASTBlock* nop = (ASTBlock*)ast_create(AST_BLOCK);
        nop->base.line = p->previous.line;
        nop->base.col = p->previous.col;
        nop->statements = NULL;
        return (ASTNode*)nop;
    }

    int is_pub = 0;
    int is_exp = 0;

    if (match(p, TOKEN_KW_EXP)) is_exp = 1;
    if (match(p, TOKEN_KW_PUP)) is_pub = 1;

    if (match(p, TOKEN_KW_USE)) return parse_use(p);
    if (match(p, TOKEN_KW_DEFER)) return parse_defer(p);
    if (match(p, TOKEN_KW_RET)) return parse_return_statement(p);

    if (match(p, TOKEN_PRE_VAR) || match(p, TOKEN_PRE_CONST)) {
        ASTVarDecl* decl = (ASTVarDecl*)parse_var_decl(p);
        decl->is_public = is_pub;
        decl->is_exported = is_exp;
        consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_VAR);
        return (ASTNode*)decl;
    }
    
    if (match(p, TOKEN_KW_FN) || match(p, TOKEN_PRE_FUNC)) {
        ASTFuncDecl* decl = (ASTFuncDecl*)parse_func_decl(p);
        decl->is_public = is_pub;
        decl->is_exported = is_exp;
        return (ASTNode*)decl;
    }

    if (match(p, TOKEN_KW_STRUCT) || match(p, TOKEN_PRE_STRUCT)) {
        ASTStructDecl* decl = (ASTStructDecl*)parse_struct_decl(p, 0, NULL);
        decl->is_public = is_pub;
        decl->is_exported = is_exp;
        return (ASTNode*)decl;
    }

    if (match(p, TOKEN_KW_GROUP)) {
        ASTGroupDecl* decl = (ASTGroupDecl*)parse_group_decl(p);
        decl->is_public = is_pub;
        decl->is_exported = is_exp;
        return (ASTNode*)decl;
    }

    // Special Thread Block: (name)>{...}
    if (p->current.type == TOKEN_LPAREN) {
        Token lookahead = peek_token(p);
        if (lookahead.type == TOKEN_IDENTIFIER) {
            Token next2 = peek_token_ahead(p, 2);
            if (next2.type == TOKEN_RPAREN) {
                Token next3 = peek_token_ahead(p, 3);
                if (next3.type == TOKEN_THREAD_BLOCK) {
                    advance(p); // (
                    Token name_tok = p->current;
                    advance(p); // name
                    advance(p); // )
                    advance(p); // >{
                    char* name = malloc(name_tok.length + 1);
                    memcpy(name, name_tok.start, name_tok.length);
                    name[name_tok.length] = '\0';
                    return parse_thread_block(p, name);
                }
            }
        }
    }
    if (match(p, TOKEN_NOT)) {
        if (match(p, TOKEN_KW_STRUCT) || match(p, TOKEN_PRE_STRUCT)) {
            return parse_struct_decl(p, 1, NULL); // !struct (Packed)
        }
        
        // Check for !(listen)
        if (p->current.type == TOKEN_LPAREN) {
            Token lookahead = peek_token(p);
            if (lookahead.type == TOKEN_IDENTIFIER && strncmp(lookahead.start, "listen", 6) == 0) {
                advance(p); // (
                advance(p); // listen
                consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);
                consume(p, TOKEN_COLON, ERR_PARSER_UNEXPECTED_TOKEN, MSG_EXPECT_COLON_VAR);
                
                ASTNexusCapture* listen = (ASTNexusCapture*)ast_create(AST_LISTEN);
                listen->base.line = p->previous.line;
                listen->base.col = p->previous.col;
                listen->handler_name = strdup("global_listener");
                
                if (match(p, TOKEN_LBRACE)) listen->expr = parse_block(p);
                else listen->expr = parse_statement(p);
                
                return (ASTNode*)listen;
            }
        }
    }
    if (match(p, TOKEN_KW_ENUM) || match(p, TOKEN_PRE_ENUM)) return parse_enum_decl(p, NULL);
    if (match(p, TOKEN_KW_UNION) || match(p, TOKEN_PRE_UNION)) return parse_union_decl(p, NULL);
    if (match(p, TOKEN_PRE_NEWTYPE)) return parse_newtype_decl(p);
    if (match(p, TOKEN_KW_GROUP)) return parse_group_decl(p);

    if (match(p, TOKEN_LBRACE)) {
        return parse_block(p);
    }
    if (match(p, TOKEN_JUMP)) {
        // ?>(label);
        consume(p, TOKEN_LPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_LPAREN_FUNC);
        if (match(p, TOKEN_IDENTIFIER)) {
            ASTNexusJump* jmp = (ASTNexusJump*)ast_create(AST_NEXUS_JUMP);
            jmp->base.line = p->previous.line;
            jmp->base.col = p->previous.col;
            Token t = p->previous;
            jmp->label_name = malloc(t.length + 1);
            memcpy(jmp->label_name, t.start, t.length);
            jmp->label_name[t.length] = '\0';
            consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);
            consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_EXPR);
            return (ASTNode*)jmp;
        }
        error_at(p, &p->current, ERR_PARSER_EXPECTED_EXPRESSION, msg_get(MSG_EXPECT_FUNC_NAME));
    }

    if (match(p, TOKEN_HALT)) {
        ASTHalt* halt = (ASTHalt*)ast_create(AST_NEXUS_HALT);
        halt->base.line = p->previous.line;
        halt->base.col = p->previous.col;
        if (!match(p, TOKEN_SEMICOLON)) {
            halt->expr = parse_expression(p);
            consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_EXPR);
        }
        return (ASTNode*)halt;
    }

    if (match(p, TOKEN_INTENT)) {
        ASTIntent* in = (ASTIntent*)ast_create(AST_NEXUS_INTENT);
        in->base.line = p->previous.line;
        in->base.col = p->previous.col;
        if (match(p, TOKEN_IDENTIFIER)) {
            Token t = p->previous;
            in->intent_name = malloc(t.length + 1);
            memcpy(in->intent_name, t.start, t.length);
            in->intent_name[t.length] = '\0';
        }
        if (match(p, TOKEN_LPAREN)) {
            in->target = parse_expression(p);
            consume(p, TOKEN_RPAREN, ERR_PARSER_EXPECTED_PAREN, MSG_EXPECT_RPAREN_EXPR);
        }
        if (match(p, TOKEN_SEMICOLON)) {} // Opsiyonel ;
        return (ASTNode*)in;
    }

    if (match(p, TOKEN_FORCE)) {
        ASTPanic* panic = (ASTPanic*)ast_create(AST_PANIC);
        panic->base.line = p->previous.line;
        panic->base.col = p->previous.col;
        if (match(p, TOKEN_STRING)) {
            Token t = p->previous;
            panic->message = malloc(t.length + 1);
            memcpy(panic->message, t.start, t.length);
            panic->message[t.length] = '\0';
        } else {
            panic->message = strdup("Critical Panic");
        }
        consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_EXPR);
        return (ASTNode*)panic;
    }
    
    // Expression Statement
    ASTNode* expr = parse_expression(p);
    if (!expr) return NULL;
    
    // Check for Label Syntax: expr :> statement
    if (match(p, TOKEN_NEXUS_LABEL)) {
        char* label_name = NULL;
        
        // Unwrap tuple if single element (defensive)
        ASTNode* target = expr;
        while (target->type == AST_TUPLE) {
            ASTTuple* tuple = (ASTTuple*)target;
            if (tuple->elements && !tuple->elements->next) {
                target = tuple->elements;
            }
        }

        if (target->type == AST_IDENTIFIER) {
            ASTIdentifier* id = (ASTIdentifier*)target;
            label_name = malloc(strlen(id->name) + 1);
            strcpy(label_name, id->name);
        } else if (target->type == AST_LITERAL) {
            ASTLiteral* lit = (ASTLiteral*)target;
            label_name = malloc(strlen(lit->string_value) + 1);
            strcpy(label_name, lit->string_value);
        } else {
             error_at(p, &p->previous, ERR_PARSER_INVALID_VAR, msg_get(MSG_INVALID_CAPTURE));
             return expr;
        }
        ASTNexusLabel* lbl = (ASTNexusLabel*)ast_create(AST_NEXUS_LABEL);
        lbl->base.line = expr->line;
        lbl->base.col = expr->col;
        lbl->label_name = label_name;
        
        if (match(p, TOKEN_LBRACE)) lbl->statement = parse_block(p);
        else if (p->current.type != TOKEN_EOF && p->current.type != TOKEN_RBRACE) {
             lbl->statement = parse_statement(p);
        } else {
             lbl->statement = NULL;
        }
        return (ASTNode*)lbl;
    }

    // Loop, If (Nexus Flow), Label gibi yapılar noktalı virgül gerektirmez
    if (expr->type != AST_LOOP && expr->type != AST_NEXUS_FLOW && expr->type != AST_NEXUS_LABEL) {
        consume(p, TOKEN_SEMICOLON, ERR_PARSER_EXPECTED_SEMICOLON, MSG_EXPECT_SEMICOLON_EXPR);
    }
    return expr;
}

int parser_run(ParserContext* ctx) {
    printf("%s\n", msg_get(MSG_PARSER_START));
    
    Parser parser;
    parser.ctx = ctx;
    parser.had_error = 0;
    parser.panic_mode = 0;
    
    advance(&parser); // İlk tokeni yükle

    ASTProgram* program = (ASTProgram*)ast_create(AST_PROGRAM);
    program->base.line = 1;
    program->base.col = 1;
    ASTNode* current_stmt = NULL;

    while (!match(&parser, TOKEN_EOF)) {
        ASTNode* stmt = parse_statement(&parser);
        if (stmt) {
            // AST'ye ekle (Bağlı liste mantığı)
            if (program->declarations == NULL) {
                program->declarations = stmt;
                current_stmt = stmt;
            } else {
                current_stmt->next = stmt;
                current_stmt = stmt;
            }
        } else {
            // Hata durumunda sonsuz döngüyü önlemek için ilerle
            advance(&parser);
        }
    }

    if (parser.had_error) {
        return 1;
    }

    printf("%s\n", msg_get(MSG_PARSER_SUCCESS));
    
    // AST Görselleştirme
    if (program) {
        ctx->ast_root = (void*)program; // Semantic analiz için kaydet
        ast_print((ASTNode*)program);
    }
    
    return 0;
}

// --- Levenshtein Similarity Helper (ULTRA) ---
static int levenshtein(const char* s1, const char* s2) {
    int len1 = strlen(s1), len2 = strlen(s2);
    int matrix[256][256]; // Basit statik matrix (Küçük isimler için yeterli)
    
    if (len1 > 255) len1 = 255;
    if (len2 > 255) len2 = 255;

    for (int i = 0; i <= len1; i++) matrix[i][0] = i;
    for (int j = 0; j <= len2; j++) matrix[0][j] = j;

    for (int i = 1; i <= len1; i++) {
        for (int j = 1; j <= len2; j++) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            int a = matrix[i - 1][j] + 1;
            int b = matrix[i][j - 1] + 1;
            int c = matrix[i - 1][j - 1] + cost;
            if (a < b) matrix[i][j] = (a < c) ? a : c;
            else matrix[i][j] = (b < c) ? b : c;
        }
    }
    return matrix[len1][len2];
}

static int get_similarity(const char* s1, const char* s2) {
    int dist = levenshtein(s1, s2);
    int max_len = strlen(s1) > strlen(s2) ? strlen(s1) : strlen(s2);
    if (max_len == 0) return 100;
    return (int)((1.0 - (double)dist / max_len) * 100);
}

static void parser_add_issue(ParserContext* ctx, ErrorCode code, int line, int col, const char* message, const char* suggestion, const char* hint, int similarity, int is_warning) {
    Error* err = (Error*)malloc(sizeof(Error));
    err->code = code;
    err->filename = strdup(ctx->filename ? ctx->filename : "<unknown>");
    err->line = line;
    err->col = col;
    err->message = strdup(message);
    err->suggestion = suggestion ? strdup(suggestion) : NULL;
    err->hint = hint ? strdup(hint) : NULL;
    err->similarity = similarity;
    err->is_warning = is_warning;
    err->next = NULL;

    if (ctx->error_tail) {
        ctx->error_tail->next = err;
        ctx->error_tail = err;
    } else {
        ctx->error_head = err;
        ctx->error_tail = err;
    }
    ctx->error_count++;
}

// Eski fonksiyonu yönlendir
static void parser_add_error(ParserContext* ctx, ErrorCode code, int line, int col, const char* message) {
    parser_add_issue(ctx, code, line, col, message, NULL, NULL, 0, 0);
}

void parser_print_errors(ParserContext* ctx) {
    if (ctx->error_count == 0) return;

    printf("\n%s (%d) ---\n", msg_get(MSG_ERRORS_HEADER), ctx->error_count);
    Error* current = ctx->error_head;
    while (current) {
        // [E042] Hata: ... veya [W012] Uyarı: ...
        const char* type_label = current->is_warning ? "Uyarı" : "Hata";
        char type_char = current->is_warning ? 'W' : 'E';
        
        printf("[%c%03d] %s: %s\n", type_char, current->code, type_label, current->message);
        printf("       Dosya: %s Satır: %d:%d\n", current->filename, current->line, current->col);
        
        if (current->suggestion) {
            if (current->similarity > 0) {
                printf("       Öneri: %s (Benzerlik: %%%d)\n", current->suggestion, current->similarity);
            } else {
                printf("       Öneri: %s\n", current->suggestion);
            }
        }
        
        if (current->hint) {
            printf("       İpucu: %s\n", current->hint);
        }

        printf("\n");
        current = current->next;
    }
    printf("%s\n", msg_get(MSG_SEPARATOR));
}

void parser_destroy(ParserContext* ctx) {
    if (ctx) {
        if (ctx->ast_root) ast_destroy((ASTNode*)ctx->ast_root);
        
        Error* current = ctx->error_head;
        while (current) {
            Error* next = current->next;
            if (current->filename) free(current->filename);
            if (current->message) free(current->message);
            if (current->suggestion) free(current->suggestion);
            if (current->hint) free(current->hint);
            free(current);
            current = next;
        }
        if (ctx->source_buffer) free(ctx->source_buffer);
        free(ctx);
    }
}

void parser_save_ir(ParserContext* ctx, const char* path) {
    if (!ctx || !ctx->ast_root) return;
    FILE* f = fopen(path, "wb");
    if (!f) {
        printf("Error: Could not open IR file for writing: %s\n", path);
        return;
    }
    ast_serialize((ASTNode*)ctx->ast_root, f);
    fclose(f);
}