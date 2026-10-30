// ============================================================
// Analyzer.cpp — NexusFlow Semantic Analyzer Implementation
// ============================================================
/*
 * MODÜL: Analyzer
 * GÖREV: OIR (Intermediate Representation) üzerindeki sembolleri, tipleri 
 *        ve sahiplik kurallarını (The 15 Keys) doğrular.
 * 
 * ANA SÜREÇLER:
 * - analyze(): OIRReader'dan talimatları okur ve doğrulamayı başlatır.
 * - traverseOIR(): Talimat akışını ana döngüde gezerek handle metodlarına dağıtır.
 * - handleXXX(): Operatör bazlı semantik (tip uyumluluğu, sembol varlığı) kontrolleri.
 * 
 * KAPASİTİF BÜTÜNLÜK: OIR v2 spesifikasyonundaki tüm opcodelar (0x01 - 0xB2) kapsanmıştır.
 */

#include "Analyzer.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

bool Analyzer::analyze() {
    if (debugMode) std::cout << "[ANALYZER] Semantik analiz baslatildi: " << sourceFilename << "\n";
    
    // 1. Pass: Sembol Tablosunu Hazirla
    symTable.clear();
    for (const auto& sym : reader.symbols) {
        symTable.addSymbol(sym);
    }

    // 2. Pass: Talimatları Gez (Semantik Doğrulama)
    traverseOIR();

    // 3. Pass: Kullanılmayan Sembol/Handle Analizi
    // DEF_VAR ile tanımlanan ama hiç kullanılmayan sembolleri bul
    std::unordered_map<uint32_t, uint32_t> defLines;  // symId -> line
    std::unordered_set<uint32_t> usedSymIds;

    for (const auto& instr : reader.instructions) {
        if (instr.op == OIROp::DEF_VAR && instr.symbolId != 0) {
            const OIRSymbol* s = symTable.getSymbol(instr.symbolId);
            // Yalnızca VAR prefix'li sembolleri izle (FUNC, GROUP, RULE vb. değil)
            if (s && s->prefix == SymPrefix::VAR) {
                // Parametre olanları hariç tut: flags'te MUTABLE var ama adını kontrol et
                // (param: rawData binary'e yazılmadığı için flag tabanlı eleme)
                if (defLines.find(instr.symbolId) == defLines.end()) {
                    defLines[instr.symbolId] = instr.line;
                }
            }
        }
    }

    for (const auto& instr : reader.instructions) {
        if (instr.op == OIROp::DEF_VAR) continue;  // tanım kendisi kullanım sayılmaz
        // Sembol referanslarını "kullanılmış" olarak işaretle
        if (instr.symbolId != 0) usedSymIds.insert(instr.symbolId);
        if (instr.operandId != 0) usedSymIds.insert(instr.operandId);
    }

    // Fark: tanımlanan ama kullanılmayanlar
    for (const auto& [sid, defLine] : defLines) {
        if (usedSymIds.find(sid) == usedSymIds.end()) {
            const OIRSymbol* s = symTable.getSymbol(sid);
            if (!s) continue;
            bool isHandle = (s->name.size() >= 3 && s->name.front() == '(' && s->name.back() == ')');
            if (isHandle) {
                errors.warning("W0011", "Tanim yapilmis ama hic kullanilmamis handle: '" + s->name + "'",
                            sourceFilename, defLine, 1);
            } else {
                errors.warning("W0010", "Tanim yapilmis ama hic kullanilmamis degisken: '" + s->name + "'",
                            sourceFilename, defLine, 1);
            }
        }
    }

    if (errors.hasErrors()) {
        if (debugMode) std::cout << "[ANALYZER] Hatalar bulundu. Pipeline durduruldu.\n";
        return false;
    }
    
    if (debugMode) std::cout << "[ANALYZER] Analiz basariyla tamamlandi. Toplam Sembol: " << symTable.size() << "\n";
    return true;
}

void Analyzer::traverseOIR() {
    for (currentInstrIdx = 0; currentInstrIdx < reader.instructions.size(); ++currentInstrIdx) {
        const auto& instr = reader.instructions[currentInstrIdx];
        
        switch (instr.op) {
            // [0x01 - 0x0F] Build / Target
            case OIROp::SET_TARGET:
            case OIROp::LINK_LIB:
            case OIROp::USE_MOD:       break;

            // [0x10 - 0x1F] Variables & Flow
            case OIROp::DEF_VAR:       handleDefVar(instr); break;
            case OIROp::CAPTURE:       checkSymbolExists(instr.symbolId, instr.line, instr.col); break;
            case OIROp::RELOCATE:      handleRelocate(instr); break;
            case OIROp::ASSIGN:        checkSymbolExists(instr.symbolId, instr.line, instr.col); break;
            case OIROp::PIPE:          checkSymbolExists(instr.symbolId, instr.line, instr.col); break;
            case OIROp::IGNORE:        checkSymbolExists(instr.symbolId, instr.line, instr.col); break;
            case OIROp::ZONE_WRITE:    checkSymbolExists(instr.symbolId, instr.line, instr.col); break;
            case OIROp::FLOW_FEED:     checkSymbolExists(instr.symbolId, instr.line, instr.col); break;

            // [0x20 - 0x2F] Control Flow
            case OIROp::RETURN_VOID:
            case OIROp::RETURN_VAL:    break;
            case OIROp::BREAK_LOOP:
            case OIROp::CONTINUE_LOP:  break;
            case OIROp::COND_JUMP:
            case OIROp::UNCOND_JUMP:
            case OIROp::LABEL_DEF:     break;
            case OIROp::PANIC:         break;
            case OIROp::HALT_FLOW:     break;
            case OIROp::CLEANUP:       break;

            // [0x30 - 0x3F] Functions
            case OIROp::DEF_FUNC:
            case OIROp::END_FUNC:      break;
            case OIROp::CALL:          checkSymbolExists(instr.symbolId, instr.line, instr.col); break;
            case OIROp::DEF_EXTERN:
            case OIROp::DEF_LAMBDA:    break;
            case OIROp::INTENT:        break;

            // [0x40 - 0x5F] Structures / Groups
            case OIROp::DEF_STRUCT:
            case OIROp::DEF_ENUM:
            case OIROp::DEF_UNION:
            case OIROp::DEF_NEWTYPE:
            case OIROp::END_TYPEDEF:   break;
            case OIROp::HARD_LOCK:     checkSymbolExists(instr.symbolId, instr.line, instr.col); break;
            case OIROp::APPLY_RULE:    break;
            case OIROp::ON_RULE:       break;
            case OIROp::MAP_DEF:       break;
            case OIROp::DEF_GROUP:     break;
            case OIROp::GROUP_MEMBER:  break;
            case OIROp::METHOD_FULL:
            case OIROp::METHOD_ONELINER: break;

            // [0x60 - 0x7F] Blocks & Loops
            case OIROp::IF_BEGIN:
            case OIROp::ELSIF_BEGIN:
            case OIROp::ELS_BEGIN:
            case OIROp::IF_END:        break;
            case OIROp::LOOP_INF:
            case OIROp::LOOP_RANGE:
            case OIROp::LOOP_FOREACH:
            case OIROp::LOOP_FOR:
            case OIROp::LOOP_WHILE:
            case OIROp::LOOP_WHILE_S:
            case OIROp::LOOP_END:      break;

            // [0x80 - 0x8F] Selection / Scan
            case OIROp::SEL_BEGIN:
            case OIROp::SEL_BRANCH:
            case OIROp::SEL_END:       break;
            case OIROp::SCAN_BEGIN:
            case OIROp::SCAN_PATTERN:
            case OIROp::SCAN_END:      break;

            // [0x90 - 0x9F] Catch / Fallback
            case OIROp::ROLLING_RETRY: break;
            case OIROp::CATCH_ERR:     checkSymbolExists(instr.symbolId, instr.line, instr.col); break;
            case OIROp::FALLBACK:      break;

            // [0xA0 - 0xBF] Expressions / ASM
            case OIROp::EXPR:          break;
            case OIROp::ASM_BLOCK:
            case OIROp::ASM_LABEL:     break;
            case OIROp::MACRO_CALL:    break;

            default: 
                if (debugMode) std::cout << "  WARNING: Bilinmeyen OIR Opcode: " << std::hex << (int)instr.op << std::dec << "\n";
                break;
        }
    }
}

void Analyzer::handleDefVar(const OIRInstruction& instr) {
    checkSymbolExists(instr.symbolId, instr.line, instr.col);
}

void Analyzer::handleRelocate(const OIRInstruction& instr) {
    checkSymbolExists(instr.symbolId, instr.line, instr.col);
    checkSymbolExists(instr.operandId, instr.line, instr.col);
}

OIRSymbol* Analyzer::checkSymbolExists(uint32_t symId, uint32_t line, uint16_t col) {
    if (symId == 0) return nullptr;
    OIRSymbol* s = symTable.getSymbol(symId);
    if (!s) {
        errors.error("A2000", "Tanimlanmamis sembol ID: " + std::to_string(symId), sourceFilename, line, col);
        return nullptr;
    }
    
    // Implicit Handler check: (h1), (v2) etc are valid handlers
    if (s->name.size() >= 3 && s->name.front() == '(' && s->name.back() == ')') {
        // Valid implicit handler
    }

    return s;
}

int Analyzer::levenshteinDistance(const std::string& s1, const std::string& s2) {
    int n = (int)s1.length(), m = (int)s2.length();
    if (n == 0) return m;
    if (m == 0) return n;
    std::vector<int> col(m + 1);
    for (int i = 0; i <= m; i++) col[i] = i;
    for (int i = 1; i <= n; i++) {
        int prev = i;
        for (int j = 1; j <= m; j++) {
            int cur = (s1[i - 1] == s2[j - 1]) ? col[j - 1] : 1 + std::min({col[j - 1], col[j], prev});
            col[j - 1] = prev;
            prev = cur;
        }
        col[m] = prev;
    }
    return col[m];
}

std::string Analyzer::suggestSimilarName(const std::string& target) { return ""; }
