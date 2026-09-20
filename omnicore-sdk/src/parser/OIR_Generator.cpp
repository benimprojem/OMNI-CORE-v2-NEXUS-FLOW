// ============================================================
// OIR_Generator.cpp — NexusFlow Parser DLL Implementation
//
// Implements the C-ABI exported in nexus_parser.h.
// Ties together: NexusLexer + NexusParser + OIRWriter + ErrorList
//
// PIPELINE CONTRACT:
//   1. nxf_create_session() → allocate Session
//   2. nxf_parse_source()   → Lex + Parse → OIR instructions buffered
//   3. nxf_has_errors()     → check before saving
//   4. nxf_save_oir()       → writes .oir ONLY if no errors
//   5. Next stage reads .oir → Analyzer etc.
// ============================================================

#include "nexus_parser.h"
#include "Lexer.hpp"
#include "NexusParser.cpp"   // header-included implementation
#include "OIRWriter.hpp"
#include "OIRError.hpp"

#include <string>
#include <sstream>
#include <fstream>
#include <cstring>

// Internal session state
struct NxfSessionState {
    ErrorList  errors;
    OIRWriter* writer  = nullptr;
    std::string errorCache;   // cached for nxf_get_errors()
    bool        parsed = false;

    NxfSessionState(const std::string& targetId, bool debugMode,
                    const std::string& outPath)
    {
        writer = new OIRWriter(targetId, errors, debugMode, outPath);
    }

    ~NxfSessionState() { delete writer; }
};

// ============================================================
// C-ABI implementations
// ============================================================

extern "C" {

NXF_API NxfSession nxf_create_session(const char* targetId,
                                       int         debugMode,
                                       const char* outOirPath)
{
    const std::string tid = targetId  ? targetId  : "unknown";
    const std::string out = outOirPath ? outOirPath : "output.oir";
    return new NxfSessionState(tid, debugMode != 0, out);
}

NXF_API int nxf_parse_source(NxfSession  sessionPtr,
                              const char* source,
                              const char* filename)
{
    if (!sessionPtr || !source) return 1;
    auto* S = static_cast<NxfSessionState*>(sessionPtr);
    const std::string srcStr  = source;
    const std::string fileStr = filename ? filename : "<input>";

    // --- STAGE 1: Lex ---
    NexusLexer lexer(srcStr, S->errors, fileStr);
    std::vector<Token> tokens = lexer.tokenize();

    // Pipeline check: lexer errors → don't proceed to parse
    if (S->errors.hasErrors()) {
        S->parsed = false;
        return 1;
    }

    // --- STAGE 2: Parse ---
    NexusParser parser(std::move(tokens), S->errors, *(S->writer), fileStr);
    parser.parse();

    S->parsed = true;
    return S->errors.hasErrors() ? 1 : 0;
}

NXF_API int nxf_parse_file(NxfSession session, const char* filepath)
{
    if (!session || !filepath) return 1;

    std::ifstream f(filepath, std::ios::in);
    if (!f) {
        auto* S = static_cast<NxfSessionState*>(session);
        S->errors.error("L0001",
            std::string("Dosya açılamadı: ") + filepath, filepath, 0, 0);
        return 1;
    }

    std::string src((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
    return nxf_parse_source(session, src.c_str(), filepath);
}

NXF_API int nxf_save_oir(NxfSession session)
{
    if (!session) return 1;
    auto* S = static_cast<NxfSessionState*>(session);

    // Hard rule: don't write if errors present
    if (S->errors.hasErrors()) {
        S->errors.flush(std::cerr);
        return 1;
    }

    return S->writer->save() ? 0 : 1;
}

NXF_API int nxf_has_errors(NxfSession session)
{
    if (!session) return 1;
    return static_cast<NxfSessionState*>(session)->errors.hasErrors() ? 1 : 0;
}

NXF_API const char* nxf_get_errors(NxfSession session)
{
    if (!session) return "";
    auto* S = static_cast<NxfSessionState*>(session);

    std::ostringstream oss;
    S->errors.flush(oss);
    S->errorCache = oss.str();
    return S->errorCache.c_str();
}

NXF_API int nxf_get_error_count(NxfSession session)
{
    if (!session) return 0;
    return static_cast<NxfSessionState*>(session)->errors.numErrors();
}

NXF_API void nxf_dump_oir(NxfSession session)
{
    if (!session) return;
    static_cast<NxfSessionState*>(session)->writer->dump();
}

NXF_API void nxf_free_string(const char* /*str*/)
{
    // Strings are owned by the session — no separate free needed
}

NXF_API void nxf_destroy_session(NxfSession session)
{
    if (session) delete static_cast<NxfSessionState*>(session);
}

} // extern "C"