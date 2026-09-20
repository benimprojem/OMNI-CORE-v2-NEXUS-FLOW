// ============================================================
// analyzer_main.cpp — Analyzer DLL Entry Point
// ============================================================

#include "OIRReader.hpp"
#include "Analyzer.hpp"
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
    #define EXPORT extern "C" __declspec(dllexport)
#else
    #define EXPORT extern "C" __attribute__((visibility("default")))
#endif

// Global analyzer state for single session
ErrorList g_analyzerErrors;

// Occ.exe calls this first
EXPORT void nxf_init_analyzer() {
    g_analyzerErrors.clear();
}

// Occ.exe calls this to start analysis
EXPORT int nxf_analyze_oir(const char* oirFile, const char* sourceFile, int debugMode) {
    OIRReader reader(oirFile, g_analyzerErrors);
    
    if (!reader.load()) {
        g_analyzerErrors.flush();
        return 1; // Load failed
    }

    std::cout << "[ANALYZER] OK: " << oirFile << " okundu. Hedef: " << reader.targetId << "\n";
    std::cout << "[ANALYZER] Toplam Sembol: " << reader.symbols.size() << ", Komut: " << reader.instructions.size() << "\n";

    Analyzer analyzer(reader, g_analyzerErrors, sourceFile ? sourceFile : "", debugMode != 0);
    bool analysisSuccess = analyzer.analyze();

    if (!analysisSuccess || g_analyzerErrors.hasErrors()) {
        g_analyzerErrors.flush();
        return 1;
    }

    std::cout << "[STAGE 2 OK] Semantik Analiz ve Tip Kontrolleri Basarili.\n";
    return 0; // Success
}
