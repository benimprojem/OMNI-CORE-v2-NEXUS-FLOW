// ============================================================
// Lexer.hpp — NexusFlow Lexer Implementation
// ============================================================
/*
 * MODÜL: NexusLexer
 * GÖREV: Kaynak kodu token'lara (atomik birimlere) ayırır.
 * 
 * ANA SÜREÇLER:
 * - tokenize(): Tüm kaynak kodu tarar ve vector<Token> döndürür.
 * - nextToken(): Bir sonraki anlamlı token'ı (operatör, kelime, sayı vb.) üretir.
 * - match(std::string): Verilen karakter dizisiyle eşleşme yapar (advance() içerir).
 * - lexWord(): v:, f: gibi prefixleri ve keyword'leri (if, loop vb.) ayırır.
 * 
 * KAPASİTİF BÜTÜNLÜK: !listen, ===, ==!, ?( gibi tüm operatörler kapsanmıştır.
 */

#ifndef NEXUS_LEXER_HPP
#define NEXUS_LEXER_HPP

#include "Token.hpp"
#include "OIRError.hpp"
#include <vector>
#include <string>
#include <cctype>

class NexusLexer {
    std::string source;
    size_t      pos  = 0;
    int         line = 1;
    int         col  = 1;
    std::string filename;
    ErrorList&  errors;

public:
    NexusLexer(const std::string& src, ErrorList& errs, const std::string& file = "<input>")
        : source(src), filename(file), errors(errs) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (pos < source.size()) {
            skipWhitespace();
            if (pos >= source.size()) break;
            if (peek() == '#') {
                if (peek(1) == '#') skipBlockComment();
                else skipLineComment();
                continue;
            }
            Token t = nextToken();
            if (t.type == TokenType::UNKNOWN) {
                errors.error("L1000", std::string("Bilinmeyen karakter: '") + t.value + "'", filename, t.line, t.col);
                continue;
            }
            tokens.push_back(t);
        }
        tokens.push_back(makeToken(TokenType::EOF_TOKEN, "<eof>"));
        return tokens;
    }

private:
    char peek(size_t offset = 0) const {
        if (pos + offset >= source.size()) return '\0';
        return source[pos + offset];
    }

    char advance() {
        if (pos >= source.size()) return '\0';
        char c = source[pos++];
        if (c == '\n') { ++line; col = 1; } else { ++col; }
        return c;
    }

    bool match(const std::string& expected) {
        if (pos + expected.size() > source.size()) return false;
        if (source.compare(pos, expected.size(), expected) == 0) {
            for (size_t i = 0; i < expected.size(); ++i) advance();
            return true;
        }
        return false;
    }

    Token makeToken(TokenType type, const std::string& value, int tL = -1, int tC = -1) const {
        return Token{ type, value, tL < 0 ? line : tL, tC < 0 ? col : tC };
    }

    void skipWhitespace() {
        while (pos < source.size() && isspace(peek())) advance();
    }

    void skipLineComment() {
        while (pos < source.size() && peek() != '\n') advance();
    }

    void skipBlockComment() {
        int sl = line, sc = col;
        advance(); advance(); 
        while (pos < source.size()) {
            if (peek() == '#' && peek(1) == '#') { advance(); advance(); return; }
            advance();
        }
        errors.error("L1001", "Blok yorum kapatilmadi", filename, sl, sc);
    }

    Token nextToken() {
        int tl = line, tc = col;

        // Longest Match First (3-char)
        if (match("!listen")) return makeToken(TokenType::KW_NOISY_LISTEN, "!listen", tl, tc);
        if (match(":=>")) return makeToken(TokenType::OP_BARRIER,     ":=>",  tl, tc);
        if (match("}?;"))  return makeToken(TokenType::OP_TERMINATOR,    "}?;",  tl, tc);
        if (match("!!="))  return makeToken(TokenType::OP_DIRECTIVE,     "!!=",  tl, tc);
        if (match("==="))  return makeToken(TokenType::OP_STRICT_EQ,     "===",  tl, tc);
        if (match("==!"))  return makeToken(TokenType::OP_BIT_STRICT_EQ, "==!",  tl, tc);
        if (match("?->"))  return makeToken(TokenType::OP_CATCH,         "?->",  tl, tc);
        if (match("?=>"))  return makeToken(TokenType::OP_FALLBACK,      "?=>",  tl, tc);
        if (match("=?>"))  return makeToken(TokenType::OP_FLOW_IF,       "=?>",  tl, tc);
        if (match("..."))  return makeToken(TokenType::OP_VARIADIC,      "...",  tl, tc);
        if (match("!->"))  return makeToken(TokenType::OP_IGNORE,        "!->",  tl, tc);

        // 2-char
        if (match("^^"))   return makeToken(TokenType::OP_POW,           "^^",   tl, tc);
        if (match("->"))   return makeToken(TokenType::OP_PIPE,          "->",   tl, tc);
        if (match("<-"))   return makeToken(TokenType::OP_CAPTURE,       "<-",   tl, tc);
        if (match("_>"))   return makeToken(TokenType::OP_RELOCATE,      "_>",   tl, tc);
        if (match("=>"))   return makeToken(TokenType::OP_BIND,          "=>",   tl, tc);
        if (match(".."))   return makeToken(TokenType::OP_RANGE,         "..",   tl, tc);
        if (match(":>"))   return makeToken(TokenType::OP_LABEL_DEF,     ":>",   tl, tc);
        if (match("?>"))   return makeToken(TokenType::OP_LABEL_JUMP,    "?>",   tl, tc);
        if (match(".="))   return makeToken(TokenType::OP_STR_CONCAT,     ".=",   tl, tc);
        if (match("=="))   return makeToken(TokenType::OP_EQ,             "==",   tl, tc);
        if (match("!="))   return makeToken(TokenType::OP_NEQ,            "!=",   tl, tc);
        if (match(">="))   return makeToken(TokenType::OP_GTE,            ">=",   tl, tc);
        if (match("<="))   return makeToken(TokenType::OP_LTE,            "<=",   tl, tc);
        if (match("&&"))   return makeToken(TokenType::OP_AND,            "&&",   tl, tc);
        if (match("||"))   return makeToken(TokenType::OP_OR,             "||",   tl, tc);
        if (match("<<"))   return makeToken(TokenType::OP_LSHIFT,         "<<",   tl, tc);
        if (match(">>"))   return makeToken(TokenType::OP_RSHIFT,         ">>",   tl, tc);
        if (match("+="))   return makeToken(TokenType::OP_ADD_ASSIGN,     "+=",   tl, tc);
        if (match("-="))   return makeToken(TokenType::OP_SUB_ASSIGN,     "-=",   tl, tc);
        if (match("*="))   return makeToken(TokenType::OP_MUL_ASSIGN,     "*=",   tl, tc);
        if (match("/="))   return makeToken(TokenType::OP_DIV_ASSIGN,     "/=",   tl, tc);
        if (match("|="))   return makeToken(TokenType::OP_BIT_OR_ASSIGN,  "|=",   tl, tc);
        if (match("&="))   return makeToken(TokenType::OP_BIT_AND_ASSIGN, "&=",   tl, tc);
        if (match("^="))   return makeToken(TokenType::OP_BIT_XOR_ASSIGN, "^=",   tl, tc);
        if (match("++"))   return makeToken(TokenType::OP_INC,            "++",   tl, tc);
        if (match("--"))   return makeToken(TokenType::OP_DEC,            "--",   tl, tc);
        if (match("!!"))   return makeToken(TokenType::OP_PANIC,          "!!",   tl, tc);
        if (match("?("))   return makeToken(TokenType::OP_ROLLING,        "?(",   tl, tc);

        // Word/Prefix/Keyword
        if (isalpha(peek()) || peek() == '_') return lexWord(tl, tc);
        // Numbers
        if (isdigit(peek()) || (peek() == '0' && (toupper(peek(1)) == 'X'))) return lexNumber(tl, tc);
        // Literals
        if (peek() == '"') return lexString(tl, tc);
        if (peek() == '\'') return lexChar(tl, tc);

        // 1-char
        char c = advance();
        switch (c) {
            case '(': return makeToken(TokenType::L_PAREN,   "(", tl, tc);
            case ')': return makeToken(TokenType::R_PAREN,   ")", tl, tc);
            case '{': return makeToken(TokenType::L_BRACE,   "{", tl, tc);
            case '}': return makeToken(TokenType::R_BRACE,   "}", tl, tc);
            case '[': return makeToken(TokenType::L_BRACKET, "[", tl, tc);
            case ']': return makeToken(TokenType::R_BRACKET, "]", tl, tc);
            case ',': return makeToken(TokenType::COMMA,     ",", tl, tc);
            case ':': return makeToken(TokenType::COLON,     ":", tl, tc);
            case ';': return makeToken(TokenType::SEMICOLON, ";", tl, tc);
            case '.': return makeToken(TokenType::OP_DOT,    ".", tl, tc);
            case '@': return makeToken(TokenType::OP_INTENT, "@", tl, tc);
            case '=': return makeToken(TokenType::OP_ASSIGN, "=", tl, tc);
            case '+': return makeToken(TokenType::OP_ADD,    "+", tl, tc);
            case '-': return makeToken(TokenType::OP_SUB,    "-", tl, tc);
            case '*': return makeToken(TokenType::OP_MUL,    "*", tl, tc);
            case '/': return makeToken(TokenType::OP_DIV,    "/", tl, tc);
            case '%': return makeToken(TokenType::OP_MOD,    "%", tl, tc);
            case '&': return makeToken(TokenType::OP_BIT_AND,"&", tl, tc);
            case '|': return makeToken(TokenType::OP_BIT_OR, "|", tl, tc);
            case '^': return makeToken(TokenType::OP_BIT_XOR,"^", tl, tc);
            case '~': return makeToken(TokenType::OP_BIT_NOT,"~", tl, tc);
            case '>': return makeToken(TokenType::OP_GT,     ">",tl, tc);
            case '<': return makeToken(TokenType::OP_LT,     "<", tl, tc);
            case '!': return makeToken(TokenType::OP_HARD_LOCK,"!", tl, tc);
            case '?': return makeToken(TokenType::OP_HALT,   "?", tl, tc);
            default:  return makeToken(TokenType::UNKNOWN, std::string(1, c), tl, tc);
        }
    }

    Token lexWord(int tl, int tc) {
        std::string word;
        while (pos < source.size() && (isalnum(peek()) || peek() == '_' || peek() == ':')) {
            if (peek() == ':') {
                word += advance();
                if (word == "v:")   return makeToken(TokenType::PREFIX_VAR,    "v:", tl, tc);
                if (word == "f:")   return makeToken(TokenType::PREFIX_FUNC,   "f:", tl, tc);
                if (word == "c:")   return makeToken(TokenType::PREFIX_CONST,  "c:", tl, tc);
                if (word == "m:")   return makeToken(TokenType::PREFIX_MUST,   "m:", tl, tc);
                if (word == "r:")   return makeToken(TokenType::PREFIX_RULE,   "r:", tl, tc);
                if (word == "nt:")  return makeToken(TokenType::PREFIX_NT,     "nt:",tl, tc);
                if (word == "exf:") return makeToken(TokenType::PREFIX_EXTERN, "exf:",tl, tc);
                word.pop_back(); --pos; --col; break;
            }
            word += advance();
        }
        const auto& kw = nexusKeywords();
        auto it = kw.find(word);
        if (it != kw.end()) return makeToken(it->second, word, tl, tc);
        return makeToken(TokenType::IDENTIFIER, word, tl, tc);
    }

    Token lexNumber(int tl, int tc) {
        std::string num;
        if (peek() == '0' && toupper(peek(1)) == 'X') {
            num += advance(); num += advance();
            while (pos < source.size() && isxdigit(peek())) num += advance();
        } else {
            while (pos < source.size() && isdigit(peek())) num += advance();
            if (peek() == '.' && peek(1) != '.') {
                num += advance();
                while (pos < source.size() && isdigit(peek())) num += advance();
            }
            if (peek() == 'b' && !isalpha(peek(1))) num += advance();
        }
        return makeToken(TokenType::NUMBER, num, tl, tc);
    }

    Token lexString(int tl, int tc) {
        advance(); std::string val;
        while (pos < source.size() && peek() != '"') {
            if (peek() == '\\') {
                advance();
                switch (advance()) {
                    case 'n': val += '\n'; break;
                    case 't': val += '\t'; break;
                    case 'r': val += '\r'; break;
                    case '\\': val += '\\'; break;
                    case '"': val += '"'; break;
                    default: val += '?'; break;
                }
            } else if (peek() == '\n') break;
            else val += advance();
        }
        if (peek() == '"') advance();
        return makeToken(TokenType::STRING, val, tl, tc);
    }

    Token lexChar(int tl, int tc) {
        advance(); std::string val;
        if (peek() == '\\') { advance(); val = std::string(1, advance()); }
        else val = std::string(1, advance());
        if (peek() == '\'') advance();
        return makeToken(TokenType::T_CHAR_LIT, val, tl, tc);
    }
};

#endif
