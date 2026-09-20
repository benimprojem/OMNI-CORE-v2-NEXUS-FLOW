#ifndef OCC_PARSER_H
#define OCC_PARSER_H

#include <stdio.h>

#ifdef _WIN32
    #if defined(OCC_STATIC)
        #define OCC_API
    #elif defined(BUILD_PARSER_DLL)
        #define OCC_API __declspec(dllexport)
    #else
        #define OCC_API __declspec(dllimport)
    #endif
#else
    #define OCC_API
#endif

// Omni Core v2 Anayasası: Parser Yapıtaşları

typedef enum {
    LANG_EN,
    LANG_TR
} Language;

typedef enum {
    // --- System & Info Messages (100-999) ---
    MSG_USAGE = 100,
    MSG_OCC_PROCESSING = 101,
    MSG_OCC_FAIL_INIT = 102,
    MSG_OCC_SUCCESS_SEMANTIC = 103,
    MSG_OCC_FAIL_PARSER = 104,
    MSG_PARSER_START = 105,
    MSG_PARSER_SUCCESS = 106,
    MSG_ERRORS_HEADER = 107,
    MSG_SEPARATOR = 108,
    MSG_SEMANTIC_HEADER = 109,
    MSG_SEMANTIC_SEPARATOR = 110,
    MSG_SEMANTIC_SLOT = 111,
    MSG_SEMANTIC_HANDLER = 112,
    MSG_LANG_NOT_FOUND = 113,
    MSG_AT_END = 114,
    MSG_AT_TOKEN = 115,
    MSG_UNTERMINATED_STRING = 116,
    MSG_UNTERMINATED_CHAR = 117,
    MSG_UNEXPECTED_CHAR = 118,
    MSG_UNKNOWN_ERROR = 119,

    // Lexer & Parser Errors (2000+)
    ERR_LEXER_ERROR = 2000,
    ERR_PARSER_EXPECTED_EXPRESSION = 2001,
    ERR_PARSER_EXPECTED_PAREN = 2002,
    ERR_PARSER_EXPECTED_BRACE = 2003,
    ERR_PARSER_EXPECTED_SEMICOLON = 2004,
    ERR_PARSER_INVALID_VAR = 2005,
    ERR_PARSER_UNEXPECTED_TOKEN = 2006,

    // Specific Parser Messages (2100+)
    MSG_EXPECT_RPAREN_ARGS = 2100,
    MSG_EXPECT_RPAREN_EXPR = 2101,
    MSG_EXPECT_FUNC_NAME = 2102,
    MSG_EXPECT_LPAREN_FUNC = 2103,
    MSG_EXPECT_PARAM_NAME = 2104,
    MSG_EXPECT_RPAREN_PARAMS = 2105,
    MSG_EXPECT_RET_TYPE = 2106,
    MSG_EXPECT_TYPE_BANG = 2107,
    MSG_EXPECT_LBRACE_FUNC = 2108,
    MSG_EXPECT_RBRACE_BLOCK = 2109,
    MSG_EXPECT_SEMICOLON_EXPR = 2110,
    MSG_EXPECT_SEMICOLON_VAR = 2111,
    MSG_EXPECT_SEMICOLON_RET = 2112,
    MSG_EXPECT_COLON_VAR = 2113,
    MSG_INVALID_CAPTURE = 2114,
    MSG_EXPECT_COMMA_ROLLING = 2115,
    MSG_EXPECT_RPAREN_ROLLING = 2116,
    MSG_EXPECT_PIPE_ROLLING = 2117,
    MSG_UNEXPECTED_QMARK = 2118,
    MSG_EXPECT_MODULE_NAME = 2119,
    MSG_EXPECT_ALIAS_NAME = 2120,
    
    // Semantic Errors (3000+)
    ERR_SEMANTIC_REDEFINED = 3000,
    ERR_SEMANTIC_UNDEFINED = 3001,
    ERR_SEMANTIC_TYPE_MISMATCH = 3002,
    ERR_SEMANTIC_CONST_ASSIGN = 3003,
    ERR_SEMANTIC_DIV_ZERO = 3004,
    ERR_SEMANTIC_ARG_COUNT = 3005,
    ERR_SEMANTIC_ARG_TYPE = 3006,
    ERR_SEMANTIC_RETURN_MISMATCH = 3007,
    ERR_SEMANTIC_FUNC_REDEFINED = 3008,
    ERR_SEMANTIC_UNKNOWN_TYPE = 3009
} ErrorCode;

typedef struct Error {
    ErrorCode code;
    char* filename;
    int line;
    int col;
    char* message;
    char* suggestion; // "Öneri: ..."
    char* hint;       // "İpucu: ..."
    int similarity;   // Benzerlik skoru (%)
    int is_warning;   // 1 ise Uyarı, 0 ise Hata
    struct Error* next;
} Error;

typedef struct {
    const char* filename;
    FILE* file_handle;
    char* source_buffer;
    size_t source_len;
    size_t pos;
    int current_line;
    int current_col;
    
    // Slot (Masa) Haritası - Semantic Analiz için
    void* ast_root; // (ASTNode*) tipinde, void* olarak tutuyoruz
    // void* slot_map; 
    
    // Hata Durumu
    int error_count;
    Error* error_head;
    Error* error_tail;
} ParserContext;

OCC_API ParserContext* parser_init(const char* filename);
OCC_API int parser_run(ParserContext* ctx);
OCC_API void parser_destroy(ParserContext* ctx);
OCC_API void parser_print_errors(ParserContext* ctx);
OCC_API void parser_save_ir(ParserContext* ctx, const char* path);

// Message System
OCC_API void msg_init(const char* lang_file_path);
OCC_API const char* msg_get(int id);

#endif // OCC_PARSER_H