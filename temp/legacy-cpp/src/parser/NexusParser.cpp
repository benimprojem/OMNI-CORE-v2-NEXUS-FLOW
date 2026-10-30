// ============================================================
// NexusParser.cpp — NexusFlow Syntax Parser & OIR Generator
// ============================================================
/*
 * MODÜL: NexusParser
 * GÖREV: Token listesini (Lexer'dan gelen) anlamlı bir soyut yapıya büründürür 
 *        ve Object Intermediate Representation (OIR) üretir.
 * 
 * ANA SÜREÇLER:
 * - parse(): Dosya sonuna kadar top-level tanımları işler.
 * - parseTopLevel(): v:, f:, nt:, group gibi global yapıları ayıştırır.
 * - parseStatement(): if, loop, select, scan, return ve 15 keys operatörlerini işler.
 * - parseHandleExpression(): Handler tabanlı akış operatörlerini (<- , _> , ?-> , ?=> vb.) yönetir.
 * - parseLoop(): For, While, Foreach, Range ve Sonsuz döngü varyantlarını ayrıştırır.
 * - getExprRaw(): İfadeleri (expressions) ayrıştırırken Nexus akış operatörlerinde (->, !-> vb.) durur.
 * 
 * '15 KEYS' OPERATÖRLERİ:
 * 1. Capture (<-), 2. Pipe (->), 3. Relocate (_>), 4. Ignore (!->), 
 * 5. Rolling Retry (?(n,ms)), 6. Catch (?->), 7. Fallback (?=>), 8. Panic (!!), 
 * 9. Intent (@), 10. Zone Write (<<), 11. Flow Feed (>>), 12. Halt (?;), 
 * 13. Cleanup (}?;), 14. Apply, 15. On Rule.
 */

#ifndef NEXUS_PARSER_CPP
#define NEXUS_PARSER_CPP

#include "Token.hpp"
#include "OIRError.hpp"
#include "OIRWriter.hpp"
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>

class NexusParser {
    std::vector<Token> tokens;
    size_t             current = 0;
    ErrorList&         errors;
    OIRWriter&         oir;
    std::string        srcFile;

public:
    NexusParser(std::vector<Token> toks, ErrorList& errs, OIRWriter& writer, const std::string& file = "<input>")
        : tokens(std::move(toks)), errors(errs), oir(writer), srcFile(file) {}

    // Main entry point
    void parse() {
        while (!isAtEnd()) {
            parseTopLevel();
        }
    }

private:
    // ============================================================
    // TOP-LEVEL DISPATCHER
    // ============================================================
    void parseTopLevel() {
        Token t = peek();

        if (t.type == TokenType::OP_DIRECTIVE)          { parseDirectives(); }
        else if (t.type == TokenType::PREFIX_VAR)       { parseVariable(); consumeSemicolon(); }
        else if (t.type == TokenType::PREFIX_CONST)     { parseConstant(); consumeSemicolon(); }
        else if (t.type == TokenType::PREFIX_FUNC)      { parseFunction(); }
        else if (t.type == TokenType::PREFIX_EXTERN)    { parseExternFunction(); }
        else if (t.type == TokenType::KW_GROUP)         { parseGroup(""); }
        else if (t.type == TokenType::KW_RULES)         { parseRules(); }
        else if (t.type == TokenType::KW_STRUCT || 
                 t.type == TokenType::KW_ENUM || 
                 t.type == TokenType::KW_UNION || 
                 t.type == TokenType::PREFIX_NT)        { parseTypeDefinition(); }
        else if (t.type == TokenType::KW_USE || 
                 t.type == TokenType::KW_IMPORT)        { parseImports(); }
        else if (t.type == TokenType::SEMICOLON)        { advance(); } // empty
        else {
            errors.error("P1000", "Top-level beklenmeyen token: '" + t.value + "'", srcFile, t.line, t.col);
            synchronize();
        }
    }

    // ============================================================
    // STATEMENT DISPATCHER
    // ============================================================
    void parseStatement() {
        Token t = peek();

        if (t.type == TokenType::KW_LOOP)               { parseLoop(); }
        else if (t.type == TokenType::KW_IF)            { parseIfStatement(); }
        else if (t.type == TokenType::KW_SELECT)        { parseSelect(); }
        else if (t.type == TokenType::KW_SCAN)          { parseScan(); }
        else if (t.type == TokenType::KW_RETURN)        { parseReturn(); }
        else if (t.type == TokenType::KW_BREAK) {
            advance(); oir.emit(OIROp::BREAK_LOOP, "", 0, t.line, t.col); consumeSemicolon();
        }
        else if (t.type == TokenType::KW_CONTINUE) {
            advance(); oir.emit(OIROp::CONTINUE_LOP, "", 0, t.line, t.col); consumeSemicolon();
        }
        else if (t.type == TokenType::KW_JUMP)          { parseJump(); }
        else if (t.type == TokenType::PREFIX_VAR)       { parseVariable(); consumeSemicolon(); }
        else if (t.type == TokenType::PREFIX_FUNC)      { parseFunction(); }
        else if (t.type == TokenType::KW_GROUP)         { parseGroup(""); }
        else if (t.type == TokenType::KW_APPLY)         { parseApplyRules(); consumeSemicolon(); }
        else if (t.type == TokenType::KW_ON)            { parseOnRule(); }
        else if (t.type == TokenType::OP_PANIC)         { parsePanic(); consumeSemicolon(); }
        else if (t.type == TokenType::OP_INTENT)        { parseIntent(); consumeSemicolon(); }
        else if (t.type == TokenType::KW_UNROLL)        { parseUnrollLoop(); }
        else if (t.type == TokenType::L_PAREN)          { parseHandleExpression(); if (check(TokenType::SEMICOLON)) advance(); }
        else if (t.type == TokenType::KW_FASTEXEC)      { parseFastExec(); }
        else if (t.type == TokenType::SEMICOLON)        { advance(); }
        else if (t.type == TokenType::IDENTIFIER)       { parseIdentifierStatement(); }
        else {
            errors.error("P1001", "İfade başlangıcı tanımsız: '" + t.value + "'", srcFile, t.line, t.col);
            advance(); 
        }
    }

    void parseBlock() {
        consume(TokenType::L_BRACE, "P1010", "'{' bekleniyor.");
        while (!check(TokenType::R_BRACE) && !check(TokenType::OP_TERMINATOR) && !isAtEnd()) {
            parseStatement();
        }
        if (match(TokenType::OP_TERMINATOR)) {
            // Terminator consumed, successfully ends the block and halts flow
        } else {
            consume(TokenType::R_BRACE, "P1011", "'}' bekleniyor.");
        }
    }

    // ============================================================
    // HANDLERS (THE 15 KEYS)
    // ============================================================
    void parseHandleExpression() {
        Token lp = consume(TokenType::L_PAREN, "P1210", "(");
        Token hname = consumeAny("P1211", "Handle ismi");
        consume(TokenType::R_PAREN, "P1212", ")");
        
        std::string hval = "(" + hname.value + ")";
        uint32_t hid = oir.addSymbol(hval, SymPrefix::VAR, SymFlags::MUTABLE);
        oir.emit(OIROp::DEF_VAR, hval, hid, lp.line, lp.col);

        printf("[PARSER] Handle peek type: %d | Val: '%s'\n", (int)peek().type, peek().value.c_str());

        // Dispatches based on Nexus Flow Operator
        if (match(TokenType::OP_CAPTURE)) {
            parseExpression(hid, lp.line, lp.col);
        } else if (match(TokenType::OP_PIPE)) {
            // If right-hand side is an anonymous function (f:(x){...}) parse it properly
            if (check(TokenType::PREFIX_FUNC)) {
                parseFunction();
            } else {
                parseExpression(hid, lp.line, lp.col);
            }
            oir.emit(OIROp::PIPE, hval, hid, lp.line, lp.col);
        } else if (match(TokenType::OP_RELOCATE)) {
            parseRelocate(hid, lp);
        } else if (match(TokenType::OP_IGNORE)) {
            parseExpression(hid, lp.line, lp.col);
            oir.emit(OIROp::IGNORE, hval, hid, lp.line, lp.col);
        } else if (match(TokenType::OP_LSHIFT)) {
            parseExpression(0);
            oir.emit(OIROp::ZONE_WRITE, hval, hid, lp.line, lp.col);
        } else if (match(TokenType::OP_RSHIFT)) {
            parseExpression(0);
            oir.emit(OIROp::FLOW_FEED, hval, hid, lp.line, lp.col);
        }

        // Chainable sequence
        bool more = true;
        while (more) {
            if (check(TokenType::OP_ROLLING)) parseRolling();
            else if (check(TokenType::OP_HALT)) { advance(); oir.emit(OIROp::HALT_FLOW, hval, hid, lp.line, lp.col); }
            else if (check(TokenType::OP_TERMINATOR)) { advance(); oir.emit(OIROp::CLEANUP, hval, hid, lp.line, lp.col); }
            else if (check(TokenType::OP_CATCH)) parseCatch();
            else if (check(TokenType::OP_FALLBACK)) parseFallback();
            else more = false;
        }
    }

    void parseRelocate(uint32_t hid, Token lp) {
        uint32_t tid = 0; std::string tname = "";
        if (match(TokenType::L_PAREN)) {
            Token tn = consumeAny("P1213", "Relocate hedefi");
            consume(TokenType::R_PAREN, "P1214", ")");
            tname = "(" + tn.value + ")";
            tid = oir.addSymbol(tname, SymPrefix::VAR, SymFlags::MUTABLE);
        } else {
            Token tn = consume(TokenType::IDENTIFIER, "P1215", "Relocate hedefi");
            tname = tn.value;
            tid = oir.addSymbol(tname, SymPrefix::VAR, SymFlags::MUTABLE);
        }
        oir.emitWithOperand(OIROp::RELOCATE, tid, hid, tname, lp.line, lp.col);
    }

    void parseRolling() {
        Token op = advance(); // ?(
        Token n = consumeAny("P1221", "Tekrar sayisi");
        consume(TokenType::COMMA, "P1222", ",");
        Token ms = consumeAny("P1223", "Gecikme (ms)");
        consume(TokenType::R_PAREN, "P1224", ")");
        oir.emit(OIROp::ROLLING_RETRY, n.value + "," + ms.value, 0, op.line, op.col);
        // Look for catch/fallback immediately after rolling
        if (check(TokenType::OP_FALLBACK)) parseFallback();
        else if (check(TokenType::OP_CATCH)) parseCatch();
    }

    void parseCatch() {
        Token op = advance(); // ?->
        std::string evar = "e";
        if (match(TokenType::L_PAREN)) {
            evar = consumeAny("P1231", "Hata degiskeni").value;
            consume(TokenType::R_PAREN, "P1232", ")");
        }
        uint32_t sid = oir.addSymbol(evar, SymPrefix::VAR, SymFlags::MUTABLE);
        oir.emit(OIROp::CATCH_ERR, evar, sid, op.line, op.col);
        if (check(TokenType::L_BRACE)) parseBlock(); 
        else {
            // Special case: if next is panic or intent, they handled their own semicolon maybe?
            // Actually, we should call parseStatement but NOT expect it to consume the outer semicolon.
            parseStatement();
        }
    }

    void parseFallback() {
        Token op = advance(); // ?=>
        consume(TokenType::L_PAREN, "P1240", "(");
        Token val = consumeAny("P1241", "Fallback degeri");
        consume(TokenType::R_PAREN, "P1242", ")");
        oir.emit(OIROp::FALLBACK, val.value, 0, op.line, op.col);
    }

    // ============================================================
    // LOOPS (6 VARIANTS)
    // ============================================================
    void parseLoop() {
        Token kw = advance(); // loop

        // 1. Infinite: loop { ... }
        if (check(TokenType::L_BRACE)) {
            oir.emit(OIROp::LOOP_INF, "INF", 0, kw.line, kw.col);
            parseBlock();
            oir.emit(OIROp::LOOP_END, "", 0, kw.line, kw.col);
            return;
        }

        consume(TokenType::L_PAREN, "P1090", "(");

        // 2. Range: loop (1..100) { ... }
        if (check(TokenType::NUMBER) && lookAhead(1).type == TokenType::OP_RANGE) {
            std::string start = advance().value;
            advance(); // ..
            std::string end = advance().value;
            oir.emit(OIROp::LOOP_RANGE, start + ".." + end, 0, kw.line, kw.col);
            consume(TokenType::R_PAREN, "P1091", ")");
            parseBlock();
            oir.emit(OIROp::LOOP_END);
            return;
        }

        // 3. Foreach: loop (v:item in list) { ... }
        if (check(TokenType::PREFIX_VAR) && lookAheadFor("in")) {
            advance(); // v:
            Token item = consumeAny("P1092", "Iterasyon degiskeni");
            consumeIdent("in", "P1093", "in");
            Token list = consumeAny("P1094", "Liste");
            uint32_t sid = oir.addSymbol(item.value, SymPrefix::VAR, SymFlags::MUTABLE);
            oir.emit(OIROp::LOOP_FOREACH, item.value + " in " + list.value, sid, kw.line, kw.col);
            consume(TokenType::R_PAREN, "P1095", ")");
            parseBlock();
            oir.emit(OIROp::LOOP_END);
            return;
        }

        // 4, 5, 6: C-style loops
        int commas = countCommasInParen();
        if (commas == 2) { // for (init, cond, step)
            std::string init = getExprRaw(true); consume(TokenType::COMMA, "P1096", ",");
            std::string cond = getExprRaw(true); consume(TokenType::COMMA, "P1097", ",");
            std::string step = getExprRaw(false);
            oir.emit(OIROp::LOOP_FOR, init + "|" + cond + "|" + step, 0, kw.line, kw.col);
        } else if (commas == 1) { // while-step (cond, step)
            std::string cond = getExprRaw(true); consume(TokenType::COMMA, "P1098", ",");
            std::string step = getExprRaw(false);
            oir.emit(OIROp::LOOP_WHILE_S, cond + "|" + step, 0, kw.line, kw.col);
        } else { // while (cond)
            std::string cond = getExprRaw(false);
            oir.emit(OIROp::LOOP_WHILE, cond, 0, kw.line, kw.col);
        }
        consume(TokenType::R_PAREN, "P1099", ")");
        parseBlock();
        oir.emit(OIROp::LOOP_END);
    }

    // ============================================================
    // VARIABLES & TYPES
    // ============================================================
    void parseVariable() {
        Token prefix = advance(); // v:
        Token name   = consume(TokenType::IDENTIFIER, "P1040", "Degisken ismi");
        uint32_t sid = oir.addSymbol(name.value, SymPrefix::VAR, SymFlags::MUTABLE | SymFlags::PUBLIC);
        oir.emit(OIROp::DEF_VAR, name.value, sid, prefix.line, prefix.col);

        // Type Lock: v:name!type
        if (match(TokenType::OP_HARD_LOCK)) {
            Token type = consumeAny("P1044", "Tip");
            oir.emit(OIROp::HARD_LOCK, name.value + "!" + type.value, sid, prefix.line, prefix.col);
        }

        // Array: v:name[10]
        if (match(TokenType::L_BRACKET)) {
            if (match(TokenType::R_BRACKET)) {
                oir.emit(OIROp::EXPR, "ARR_DYN:" + name.value, sid);
            } else {
                std::string sz = getExprRaw(false);
                consume(TokenType::R_BRACKET, "P1045", "]");
                oir.emit(OIROp::EXPR, "ARR_STATIC:" + sz, sid);
            }
        }

        // Assignment: v:name = 10;
        if (match(TokenType::OP_ASSIGN)) {
            parseExpression(sid, prefix.line, prefix.col);
        }
    }

    void parseConstant() {
        Token prefix = advance(); // c:
        Token name = consume(TokenType::IDENTIFIER, "P1050", "Sabit ismi");
        uint32_t sid = oir.addSymbol(name.value, SymPrefix::CONST, SymFlags::PUBLIC);
        consume(TokenType::OP_ASSIGN, "P1051", "=");
        parseExpression(sid, prefix.line, prefix.col);
        oir.emit(OIROp::DEF_VAR, "const:" + name.value, sid, prefix.line, prefix.col);
    }

    // ============================================================
    // FUNCTIONS
    // ============================================================
    void parseFunction() {
        Token prefix = advance(); // f:
        
        // Anonymous / Lambda: f:(x) > x+1
        if (check(TokenType::L_PAREN)) {
            advance(); parseFunctionParams(); consume(TokenType::R_PAREN, "P1061", ")");
            if (match(TokenType::OP_SINGLE_LINE_RET) || match(TokenType::OP_GT)) {
                parseExpression(0);
                oir.emit(OIROp::DEF_LAMBDA, "<anon>", 0, prefix.line, prefix.col);
            } else {
                parseBlock();
                oir.emit(OIROp::DEF_LAMBDA, "<anon>", 0, prefix.line, prefix.col);
            }
            return;
        }

        Token name = consume(TokenType::IDENTIFIER, "P1062", "Fonksiyon ismi");
        uint32_t sid = oir.addSymbol(name.value, SymPrefix::FUNC, SymFlags::PUBLIC);
        oir.emit(OIROp::DEF_FUNC, name.value, sid, prefix.line, prefix.col);

        consume(TokenType::L_PAREN, "P1063", "(");
        parseFunctionParams();
        consume(TokenType::R_PAREN, "P1064", ")");

        if (match(TokenType::OP_HARD_LOCK)) {
            Token rtype = consumeAny("P1072", "Donus tipi");
            oir.emit(OIROp::EXPR, "ret_type:" + rtype.value);
        }

        if (match(TokenType::OP_SINGLE_LINE_RET) || match(TokenType::OP_GT)) {
            parseExpression(sid);
            oir.emit(OIROp::RETURN_VAL, name.value, sid);
            consumeSemicolon();
        } else {
            parseBlock();
        }
        oir.emit(OIROp::END_FUNC, name.value, sid);
    }

    void parseFunctionParams() {
        while (!check(TokenType::R_PAREN) && !isAtEnd()) {
            Token pn = consumeAny("P1070", "Parametre");
            std::string pstr = pn.value;
            if (match(TokenType::OP_HARD_LOCK)) pstr += "!" + consumeAny("P1071", "tip").value;
            uint32_t sid = oir.addSymbol(pn.value, SymPrefix::VAR, SymFlags::MUTABLE);
            oir.emit(OIROp::DEF_VAR, "param:" + pstr, sid, pn.line, pn.col);
            if (!match(TokenType::COMMA)) break;
        }
    }

    void parseExternFunction() {
        Token prefix = advance(); // exf:
        Token name = consume(TokenType::IDENTIFIER, "P1080", "Name");
        uint32_t sid = oir.addSymbol(name.value, SymPrefix::EXTERN, SymFlags::PUBLIC | SymFlags::EXTERN);
        oir.emit(OIROp::DEF_EXTERN, name.value, sid, prefix.line, prefix.col);
        consume(TokenType::L_PAREN, "P1081", "("); parseFunctionParams(); consume(TokenType::R_PAREN, "P1082", ")");
        if (match(TokenType::OP_HARD_LOCK)) consumeAny("P1072", "type");
        consumeSemicolon();
        oir.emit(OIROp::END_FUNC, name.value, sid);
    }

    // ============================================================
    // GROUPS & TYPES (nt:)
    // ============================================================
    void parseGroup(const std::string& parent) {
        Token kw = peek(); if (kw.type == TokenType::KW_GROUP) advance();
        std::string gn = parent;
        if (!check(TokenType::L_BRACE)) gn = (parent.empty() ? "" : parent + ".") + consumeAny("P1130", "Grup adi").value;
        uint32_t sid = oir.addSymbol(gn, SymPrefix::GROUP, SymFlags::PUBLIC);
        oir.emit(OIROp::DEF_GROUP, gn, sid, kw.line, kw.col);
        consume(TokenType::L_BRACE, "P1131", "{");
        while (!check(TokenType::R_BRACE) && !isAtEnd()) {
            if (match(TokenType::KW_DEFAULT)) parseDefault(gn);
            else if (check(TokenType::PREFIX_NT) || check(TokenType::KW_STRUCT)) parseTypeInGroup(gn);
            else parseGroupMember(gn);
        }
        consume(TokenType::R_BRACE, "P1132", "}");
        oir.emit(OIROp::END_TYPEDEF, gn, sid);
    }

    void parseGroupMember(const std::string& parent) {
        Token name = consume(TokenType::IDENTIFIER, "P1140", "Uye adi");
        std::string path = parent + "." + name.value;
        consume(TokenType::OP_BIND, "P1141", "=>");
        if (match(TokenType::PREFIX_FUNC)) {
            uint32_t sid = oir.addSymbol(path, SymPrefix::FUNC, SymFlags::PUBLIC);
            oir.emit(OIROp::DEF_FUNC, path, sid);
            consume(TokenType::L_PAREN, "P1150", "("); parseFunctionParams(); consume(TokenType::R_PAREN, "P1151", ")");
            parseBlock();
            oir.emit(OIROp::END_FUNC, path, sid);
        } else if (check(TokenType::L_BRACE)) {
            parseBlock();
        } else {
            parseExpression(); consumeSemicolon();
        }
        oir.emit(OIROp::GROUP_MEMBER, path, 0, name.line, name.col);
    }

    void parseDefault(const std::string& parent) {
        oir.emit(OIROp::EXPR, "DEFAULT_MEMBER:" + parent);
        parseBlock();
    }

    void parseTypeInGroup(const std::string& parent) {
        Token prefix = advance();
        Token name = consumeAny("P1172", "Type name");
        std::string path = parent + "." + name.value;
        uint32_t sid = oir.addSymbol(path, SymPrefix::NT, SymFlags::PUBLIC);
        oir.emit(OIROp::DEF_STRUCT, path, sid, prefix.line, prefix.col);
        parseTypeBlock();
        oir.emit(OIROp::END_TYPEDEF, path, sid);
    }

    void parseTypeDefinition() {
        Token kw = advance();
        bool isNT = (kw.type == TokenType::PREFIX_NT);
        std::string bType = "";
        if (isNT) {
            bType = consumeAny("P1170", "Base type").value;
            if (match(TokenType::COLON)) bType += ":" + consumeAny("P1171", "Type name").value;
            else { /* bType is the name */ }
        }
        Token name = (isNT && bType.find(':') == std::string::npos) ? Token{TokenType::IDENTIFIER, bType} : consumeAny("P1183", "Name");
        uint32_t sid = oir.addSymbol(name.value, SymPrefix::NT, SymFlags::PUBLIC);
        oir.emit(isNT ? OIROp::DEF_NEWTYPE : OIROp::DEF_STRUCT, name.value + (isNT ? "<-"+bType : ""), sid, kw.line, kw.col);
        parseTypeBlock();
        if (isNT) consumeSemicolon();
        oir.emit(OIROp::END_TYPEDEF, name.value, sid);
    }

    void parseTypeBlock() {
        consume(TokenType::L_BRACE, "P1012", "{");
        while (!check(TokenType::R_BRACE) && !isAtEnd()) {
            if (check(TokenType::PREFIX_VAR)) {
                parseVariable();
                if (match(TokenType::COMMA)) {} 
                else if (check(TokenType::SEMICOLON)) advance();
            } else { advance(); }
        }
        consume(TokenType::R_BRACE, "P1014", "}");
    }

    // ============================================================
    // MISC
    // ============================================================
    void parseIfStatement() {
        Token ifTok = advance();
        consume(TokenType::L_PAREN, "P1120", "(");
        std::string cond = getExprRaw(true);
        consume(TokenType::R_PAREN, "P1122", ")");
        oir.emit(OIROp::IF_BEGIN, cond, 0, ifTok.line, ifTok.col);
        if (check(TokenType::L_BRACE)) parseBlock(); else parseStatement();
        oir.emit(OIROp::IF_END);

        while (match(TokenType::KW_ELSIF)) {
            consume(TokenType::L_PAREN, "P1121", "(");
            std::string c = getExprRaw(true);
            consume(TokenType::R_PAREN, "P1123", ")");
            oir.emit(OIROp::ELSIF_BEGIN, c);
            if (check(TokenType::L_BRACE)) parseBlock(); else parseStatement();
            oir.emit(OIROp::IF_END);
        }
        if (match(TokenType::KW_ELS) || match(TokenType::KW_ELSE)) {
            oir.emit(OIROp::ELS_BEGIN);
            if (check(TokenType::L_BRACE)) parseBlock(); else parseStatement();
            oir.emit(OIROp::IF_END);
        }
    }

    void parseSelect() {
        Token kw = advance(); Token target = consumeAny("P1100", "Target");
        oir.emit(OIROp::SEL_BEGIN, target.value, 0, kw.line, kw.col);
        consume(TokenType::L_BRACE, "P1101", "{");
        while (!check(TokenType::R_BRACE) && !isAtEnd()) {
            Token cv = advance(); consume(TokenType::OP_PIPE, "P1102", "->");
            oir.emit(OIROp::SEL_BRANCH, cv.value);
            parseBlock();
        }
        consume(TokenType::R_BRACE, "P1103", "}");
        oir.emit(OIROp::SEL_END);
    }

    void parseScan() {
        Token kw = advance(); Token handle = consumeAny("P1110", "Handle");
        oir.emit(OIROp::SCAN_BEGIN, handle.value);
        consume(TokenType::L_BRACE, "P1111", "{");
        while (!check(TokenType::R_BRACE) && !isAtEnd()) {
            std::string pat = getExprRaw(false); consume(TokenType::OP_PIPE, "P1112", "->");
            oir.emit(OIROp::SCAN_PATTERN, pat);
            parseBlock();
        }
        consume(TokenType::R_BRACE, "P1113", "}");
        oir.emit(OIROp::SCAN_END);
    }

    void parseRules() {
        Token kw = advance(); // rules
        std::string rn = "ruleset";
        if (check(TokenType::IDENTIFIER)) rn = advance().value;
        
        // Params: rules name(p1, p2)
        if (match(TokenType::L_PAREN)) {
            while (!check(TokenType::R_PAREN) && !isAtEnd()) {
                advance(); // consume param
                if (check(TokenType::COMMA)) advance();
            }
            consume(TokenType::R_PAREN, "P1122", ")");
        }

        oir.emit(OIROp::EXPR, "RULES_BEGIN:" + rn);
        consume(TokenType::L_BRACE, "P1120", "{");
        while (!check(TokenType::R_BRACE) && !isAtEnd()) {
            if (match(TokenType::PREFIX_RULE)) {
                Token rname = consumeAny("P1121", "Rule name");
                uint32_t sid = oir.addSymbol(rname.value, SymPrefix::RULE, 0);
                
                // Trigger (->) or Barrier (:=>)
                if (match(TokenType::OP_PIPE)) { // -> Trigger
                    std::string trigger = consumeAny("P1124", "Trigger").value;
                    oir.emit(OIROp::EXPR, "TRIGGER:" + trigger, sid);
                } else if (match(TokenType::OP_BARRIER)) { // :=> Barrier
                    std::string msg = getExprRaw(false);
                    oir.emit(OIROp::EXPR, "BARRIER:" + msg, sid);
                } else if (check(TokenType::L_BRACE)) {
                    parseBlock();
                } else {
                    parseExpression(sid);
                }
                consumeSemicolon();
            } 
            else if (match(TokenType::KW_ALWAYS) || match(TokenType::KW_DEFAULT)) {
                Token type = previous();
                consume(TokenType::OP_PIPE, "P1125", "->");
                parseExpression();
                consumeSemicolon();
            }
            else advance();
        }
        consume(TokenType::R_BRACE, "P1123", "}");
        oir.emit(OIROp::EXPR, "RULES_END");
    }

    void parseApplyRules() {
        Token kw = advance(); if (!match(TokenType::KW_RULES)) match(TokenType::IDENTIFIER);
        consume(TokenType::L_PAREN, "P1271", "(");
        Token rname = consumeAny("P1272", "Ruleset");
        consume(TokenType::R_PAREN, "P1273", ")");
        oir.emit(OIROp::APPLY_RULE, rname.value);
    }

    void parseOnRule() {
        Token kw = advance(); Token rname = consumeAny("P1280", "Rule");
        std::string path = rname.value;
        if (match(TokenType::OP_DOT)) path += "." + consumeAny("P1281", "Action").value;
        uint32_t sid = oir.addSymbol(path, SymPrefix::RULE, 0);
        oir.emit(OIROp::ON_RULE, path, sid, kw.line, kw.col);
        if (check(TokenType::L_BRACE)) parseBlock(); else parseStatement();
    }

    void parsePanic() {
        Token kw = advance(); std::string msg = "";
        if (match(TokenType::L_PAREN)) { msg = consumeAny("P1250", "Msg").value; consume(TokenType::R_PAREN, "P1251", ")"); }
        oir.emit(OIROp::PANIC, msg, 0, kw.line, kw.col);
    }

    void parseIntent() {
        Token kw = advance(); Token name = consumeAny("P1260", "Intent");
        oir.emit(OIROp::INTENT, name.value, 0, kw.line, kw.col);
        if (check(TokenType::L_BRACE)) parseBlock(); else parseExpression();
    }

    void parseUnrollLoop() {
        Token kw = advance(); consume(TokenType::L_PAREN, "P1260", "(");
        Token count = consumeAny("P1261", "Count"); consume(TokenType::R_PAREN, "P1262", ")");
        consume(TokenType::OP_PIPE, "P1263", "->");
        oir.emit(OIROp::EXPR, "UNROLL:" + count.value);
        parseLoop();
    }

    void parseFastExec() {
        Token kw = advance(); oir.emit(OIROp::ASM_BLOCK, "begin", 0, kw.line, kw.col);
        consume(TokenType::L_BRACE, "P1190", "{");
        while (!check(TokenType::R_BRACE) && !isAtEnd()) {
            if (check(TokenType::KW_ASM_LABEL)) { advance(); oir.emit(OIROp::ASM_LABEL, consumeAny("P1191", "lbl").value); }
            else { std::string al = getExprRaw(false); oir.emit(OIROp::ASM_BLOCK, al); if (check(TokenType::SEMICOLON)) advance(); }
        }
        consume(TokenType::R_BRACE, "P1192", "}");
        oir.emit(OIROp::ASM_BLOCK, "end");
    }

    void parseReturn() {
        Token kw = advance(); if (check(TokenType::SEMICOLON)) oir.emit(OIROp::RETURN_VOID, "", 0, kw.line, kw.col);
        else { parseExpression(); oir.emit(OIROp::RETURN_VAL); }
        consumeSemicolon();
    }

    void parseJump() {
        Token kw = advance(); Token lbl = consumeAny("P1290", "Label");
        oir.emit(OIROp::UNCOND_JUMP, lbl.value, 0, kw.line, kw.col);
        consumeSemicolon();
    }

    void parseIdentifierStatement() {
        Token id = peek();
        if (lookAhead(1).type == TokenType::OP_LABEL_DEF) { advance(); advance(); oir.emit(OIROp::LABEL_DEF, id.value); return; }
        if (lookAhead(1).type == TokenType::OP_LABEL_JUMP) { advance(); advance(); oir.emit(OIROp::COND_JUMP, id.value); consumeSemicolon(); return; }
        parseExpression(); consumeSemicolon();
    }

    void parseDirectives() {
        Token op = advance(); // !!=
        std::string d = getExprRaw(false);
        oir.emit(OIROp::EXPR, "DIR:" + d, 0, op.line, op.col);
        if (check(TokenType::SEMICOLON)) advance(); 
    }

    void parseImports() {
        Token kw = advance(); Token path = consumeAny("P1030", "path"); std::string al = path.value;
        if (match(TokenType::KW_AS)) al = consumeAny("P1031", "alias").value;
        oir.emit(OIROp::USE_MOD, path.value + ":" + al); consumeSemicolon();
    }

    // ============================================================
    // EXPRESSION PARSER (RAW)
    // ============================================================
    void parseExpression(uint32_t sid = 0, int l = 0, int c = 0) {
        oir.emit(OIROp::EXPR, getExprRaw(false), sid, l, c);
    }

    std::string getExprRaw(bool stopAtComma) {
        std::string res; int p = 0, b = 0, br = 0;
        while (!isAtEnd()) {
            TokenType t = peek().type;
            if (p == 0 && b == 0 && br == 0) {
                if (t == TokenType::SEMICOLON) break;
                if (stopAtComma && t == TokenType::COMMA) break;
                
                // Stop at Nexus Flow Operators if we are at depth 0
                if (t == TokenType::OP_CAPTURE || t == TokenType::OP_PIPE || 
                    t == TokenType::OP_RELOCATE || t == TokenType::OP_IGNORE ||
                    t == TokenType::OP_ROLLING || t == TokenType::OP_CATCH || 
                    t == TokenType::OP_FALLBACK || t == TokenType::OP_FLOW_IF ||
                    t == TokenType::OP_TERMINATOR || t == TokenType::OP_HALT) break;
            }
            
            if (t == TokenType::L_PAREN) p++; 
            if (t == TokenType::R_PAREN) { if (p == 0) break; p--; }
            if (t == TokenType::L_BRACKET) b++;
            if (t == TokenType::R_BRACKET) { if (b == 0) break; b--; }
            if (t == TokenType::L_BRACE) br++;
            if (t == TokenType::R_BRACE || t == TokenType::OP_TERMINATOR) { if (br == 0) break; br--; }

            res += advance().value + " ";
        }
        if (!res.empty() && res.back() == ' ') res.pop_back();
        return res;
    }

    // ============================================================
    // CORE UTILS
    // ============================================================
    Token peek(size_t o = 0) const { size_t i = current + o; return i < tokens.size() ? tokens[i] : tokens.back(); }
    Token lookAhead(size_t d) const { return peek(d); }
    Token advance() { 
        if (!isAtEnd()) current++;
        return tokens[current - 1]; 
    }
    Token previous() const { return tokens[current - 1]; }
    bool check(TokenType t) const { return tokens[current].type == t; }
    bool match(TokenType t) { if (check(t)) { advance(); return true; } return false; }
    bool isAtEnd() const { return tokens[current].type == TokenType::EOF_TOKEN; }
    
    Token consume(TokenType t, const std::string& c, const std::string& m) {
        if (check(t)) return advance();
        errors.error(c, m + " (Mevcut: '" + peek().value + "')", srcFile, peek().line, peek().col);
        return peek();
    }
    
    Token consumeAny(const std::string& c, const std::string& m) {
        if (!isAtEnd()) return advance();
        errors.error(c, m, srcFile, peek().line, peek().col);
        return peek();
    }

    void consumeSemicolon() {
        if (check(TokenType::SEMICOLON)) advance();
        // R_BRACE veya OP_TERMINATOR ile biten bloklarda ; zorunlu değil
        // Aynı şekilde EOF'ta da hata verme
        else if (!check(TokenType::R_BRACE) && !check(TokenType::OP_TERMINATOR) && !isAtEnd()) {
            errors.error("P1300", "; bekleniyor", srcFile, peek().line, peek().col);
        }
    }

    void consumeIdent(const std::string& v, const std::string& c, const std::string& m) {
        if (peek().value == v) advance();
        else errors.error(c, m + " ('" + v + "' bekleniyor)", srcFile, peek().line, peek().col);
    }

    bool lookAheadFor(const std::string& v) {
        for (size_t i = current; i < tokens.size() && tokens[i].line == peek().line; i++) {
            if (tokens[i].value == v) return true;
        }
        return false;
    }

    int countCommasInParen() {
        int c = 0, d = 1;
        for (size_t i = current; i < tokens.size() && d > 0; i++) {
            if (tokens[i].type == TokenType::L_PAREN) d++;
            if (tokens[i].type == TokenType::R_PAREN) { d--; if (d == 0) break; }
            if (tokens[i].type == TokenType::COMMA && d == 1) c++;
        }
        return c;
    }

    void synchronize() {
        // Stop skipping if we hit a major top-level boundary
        while (!isAtEnd()) {
            Token t = peek();
            if (t.type == TokenType::KW_RULES || 
                t.type == TokenType::PREFIX_FUNC || 
                t.type == TokenType::PREFIX_VAR ||
                t.type == TokenType::KW_STRUCT ||
                t.type == TokenType::L_BRACE ||
                t.type == TokenType::SEMICOLON) return;
            
            advance();
        }
    }
};

#endif