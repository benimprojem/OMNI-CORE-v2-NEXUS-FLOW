#ifndef NEXUS_PARSER_H
#define NEXUS_PARSER_H

// ============================================================
// nexus_parser.h — NexusFlow Parser DLL C-ABI Public Header
//
// Uses C linkage so it can be loaded from any language.
// ============================================================

#ifdef _WIN32
  #ifdef NEXUS_PARSER_EXPORTS
    #define NXF_API __declspec(dllexport)
  #else
    #define NXF_API __declspec(dllimport)
  #endif
#else
  #define NXF_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to a parse session
typedef void* NxfSession;

// ------------------------------------------------------------
// nxf_create_session — create a new parser session
//   targetId   : e.g. "x86_64-windows"  (null = "unknown")
//   debugMode  : 1 = OIR debug mode, 0 = release
//   outOirPath : output .oir file path
// Returns session handle. Must be freed with nxf_destroy_session.
// ------------------------------------------------------------
NXF_API NxfSession nxf_create_session(const char* targetId,
                                       int         debugMode,
                                       const char* outOirPath);

// ------------------------------------------------------------
// nxf_parse_source — parse NexusFlow source string
//   session  : handle from nxf_create_session
//   source   : UTF-8 NexusFlow source code
//   filename : source file name (for error messages)
// Returns 0 on success, 1 if errors (call nxf_get_errors).
// ------------------------------------------------------------
NXF_API int nxf_parse_source(NxfSession  session,
                              const char* source,
                              const char* filename);

// ------------------------------------------------------------
// nxf_parse_file — convenience: read file and parse
// Returns 0 on success, 1 on file error or parse errors.
// ------------------------------------------------------------
NXF_API int nxf_parse_file(NxfSession session, const char* filepath);

// ------------------------------------------------------------
// nxf_save_oir — write the OIR binary to disk
// Returns 0 on success, 1 on failure.
// Will REFUSE to write if there are parse errors.
// ------------------------------------------------------------
NXF_API int nxf_save_oir(NxfSession session);

// ------------------------------------------------------------
// nxf_has_errors — check if any errors were collected
// Returns 1 if errors exist, 0 if clean.
// ------------------------------------------------------------
NXF_API int nxf_has_errors(NxfSession session);

// ------------------------------------------------------------
// nxf_get_errors — get all diagnostics as newline-separated string
// Caller must free with nxf_free_string.
// ------------------------------------------------------------
NXF_API const char* nxf_get_errors(NxfSession session);

// ------------------------------------------------------------
// nxf_get_error_count — number of ERROR-level diagnostics
// ------------------------------------------------------------
NXF_API int nxf_get_error_count(NxfSession session);

// ------------------------------------------------------------
// nxf_dump_oir — print OIR summary to stdout (debug)
// ------------------------------------------------------------
NXF_API void nxf_dump_oir(NxfSession session);

// ------------------------------------------------------------
// nxf_free_string — free a string returned by the API
// ------------------------------------------------------------
NXF_API void nxf_free_string(const char* str);

// ------------------------------------------------------------
// nxf_destroy_session — free all resources
// ------------------------------------------------------------
NXF_API void nxf_destroy_session(NxfSession session);

#ifdef __cplusplus
}
#endif

#endif // NEXUS_PARSER_H
