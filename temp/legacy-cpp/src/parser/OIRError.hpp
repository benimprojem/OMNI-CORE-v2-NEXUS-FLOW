#ifndef NEXUS_OIR_ERROR_HPP
#define NEXUS_OIR_ERROR_HPP

// ============================================================
// OIRError.hpp — NexusFlow Compiler Error Aggregation System
//
// ARCH RULE: Errors are NEVER thrown immediately.
//   - Each module (Lexer, Parser, Analyzer, etc.) appends
//     to an ErrorList.
//   - The pipeline checks hasErrors() before advancing.
//   - All errors shown to user AT ONCE before stopping.
// ============================================================

#include <string>
#include <vector>
#include <sstream>
#include <iostream>

// Severity levels
enum class Severity { ERROR, WARNING, INFO };

// Single diagnostic entry
struct Diagnostic {
    Severity    severity;
    std::string code;       // e.g. "E1001", "W2005"
    std::string message;
    std::string file;
    int         line = 0;
    int         col  = 0;
    std::string hint;       // optional "Did you mean ...?" suggestion

    std::string format() const {
        std::ostringstream oss;
        const char* sev = (severity == Severity::ERROR)   ? "\033[1;31m[ERROR]\033[0m"
                        : (severity == Severity::WARNING)  ? "\033[1;33m[WARN] \033[0m"
                                                           : "\033[1;36m[INFO] \033[0m";
        oss << sev << " " << code << " — " << message << "\n";
        if (!file.empty())
            oss << "       File: " << file << "  Line: " << line << ":" << col << "\n";
        if (!hint.empty())
            oss << "       Hint: " << hint << "\n";
        return oss.str();
    }
};

// Aggregated error list — passed through the pipeline
class ErrorList {
    std::vector<Diagnostic> entries;
    int errorCount = 0;

public:
    void add(Severity sev, const std::string& code,
             const std::string& msg,
             const std::string& file = "",
             int line = 0, int col = 0,
             const std::string& hint = "")
    {
        entries.push_back({sev, code, msg, file, line, col, hint});
        if (sev == Severity::ERROR) ++errorCount;
    }

    void error(const std::string& code, const std::string& msg,
               const std::string& file = "", int line = 0, int col = 0,
               const std::string& hint = "")
    {
        add(Severity::ERROR, code, msg, file, line, col, hint);
    }

    void warning(const std::string& code, const std::string& msg,
                 const std::string& file = "", int line = 0, int col = 0,
                 const std::string& hint = "")
    {
        add(Severity::WARNING, code, msg, file, line, col, hint);
    }

    bool hasErrors()   const { return errorCount > 0; }
    bool empty()       const { return entries.empty(); }
    int  numErrors()   const { return errorCount; }
    int  total()       const { return (int)entries.size(); }

    // Print all diagnostics (call at pipeline boundary)
    void flush(std::ostream& out = std::cerr) const {
        for (const auto& d : entries) out << d.format();
        if (errorCount > 0) {
            out << "\033[1;31m" << errorCount << " error(s) found. "
                << "Pipeline halted — fix all errors before proceeding.\033[0m\n";
        }
    }

    void clear() { entries.clear(); errorCount = 0; }
};

#endif // NEXUS_OIR_ERROR_HPP
