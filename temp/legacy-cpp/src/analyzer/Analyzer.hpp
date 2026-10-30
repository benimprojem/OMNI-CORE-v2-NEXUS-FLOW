// ============================================================
// Analyzer.hpp — NexusFlow Semantic Analyzer Header
// ============================================================
/*
 * MODÜL: Analyzer
 * GÖREV: OIR talimatlarını ve sembollerini semantik olarak doğrular.
 * 
 * ANA SÜREÇLER:
 * - analyze(): Analiz sürecini başlatan ana metod.
 * - traverseOIR(): OIR akışını gezerek her bir talimatı işler.
 * - handleXXX(): Belirli OIR opcodeları (Var, Assign, Call vb.) için özel kontroller.
 * - checkSymbolExists(): Sembollerin varlığını ve benzerlik önerilerini (Levenshtein) yönetir.
 */

#ifndef NEXUS_ANALYZER_HPP
#define NEXUS_ANALYZER_HPP

#include "OIRReader.hpp"
#include "SymbolTable.hpp"
#include "TypeSystem.hpp"

class Analyzer {
    OIRReader&   reader;
    ErrorList&   errors;
    SymbolTable  symTable;
    TypeSystem&  types;
    std::string  sourceFilename;
    uint32_t     currentInstrIdx = 0;
    bool         debugMode = false;

public:
    Analyzer(OIRReader& r, ErrorList& errs, const std::string& srcFile = "", bool debug = false)
        : reader(r), errors(errs), symTable(), types(TypeSystem::instance()), 
          sourceFilename(srcFile), currentInstrIdx(0), debugMode(debug) {}

    bool analyze();

private:
    void traverseOIR();
    
    // Yapi kontrolleri
    void handleDefVar(const OIRInstruction& instr);
    void handleAssign(const OIRInstruction& instr);
    void handleCapture(const OIRInstruction& instr);
    void handleRelocate(const OIRInstruction& instr);
    void handleCall(const OIRInstruction& instr);
    void handleHardLock(const OIRInstruction& instr);
    void handleArray(const OIRInstruction& instr);
    void handleMap(const OIRInstruction& instr);

    // Yardimci ve Levenshtein
    OIRSymbol* checkSymbolExists(uint32_t symId, uint32_t line, uint16_t col);
    int levenshteinDistance(const std::string& s1, const std::string& s2);
    std::string suggestSimilarName(const std::string& targetName);
};

#endif // NEXUS_ANALYZER_HPP
