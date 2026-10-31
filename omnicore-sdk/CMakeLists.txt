cmake_minimum_required(VERSION 3.16)

project(OmniCoreC VERSION 0.2.0 LANGUAGES C)

include(CTest)

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(OCC_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(OCC_ENABLE_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "OCC_ENABLE_SANITIZERS currently requires GCC or Clang.")
    elseif(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        message(FATAL_ERROR "OCC_ENABLE_SANITIZERS is unsupported by this C compiler.")
    endif()

    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

set(C_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/c")
set(C_PARSER_DIR "${C_SOURCE_DIR}/parser")
set(C_CODEGEN_DIR "${C_SOURCE_DIR}/codegen")
set(C_RUNTIME_DIR "${CMAKE_BINARY_DIR}/bin")

# Keep all generators (including Visual Studio multi-config) on one runtime path.
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} "${C_RUNTIME_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config} "${C_RUNTIME_DIR}")
endforeach()

file(MAKE_DIRECTORY "${C_RUNTIME_DIR}")
file(COPY "${C_SOURCE_DIR}/occ.ocf" DESTINATION "${C_RUNTIME_DIR}")
file(COPY "${C_SOURCE_DIR}/lang" DESTINATION "${C_RUNTIME_DIR}")
file(COPY "${C_SOURCE_DIR}/tests" DESTINATION "${C_RUNTIME_DIR}")
configure_file("${C_SOURCE_DIR}/tests/test1.nxf" "${C_RUNTIME_DIR}/tests/test1_alt.nxf" COPYONLY)
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/targets" DESTINATION "${CMAKE_BINARY_DIR}")

# OIR is the shared, platform-independent artifact container implementation.
# It is linked by stage modules and tools; it does not expose host struct layouts.
add_library(occ_oir STATIC
    "${C_SOURCE_DIR}/oir/oir.c"
    "${C_SOURCE_DIR}/oir/oir_payload.c"
)

target_include_directories(occ_oir PUBLIC "${C_SOURCE_DIR}/oir")

# Job-owned diagnostics collector; modules report through a host-owned context.
add_library(occ_diagnostics STATIC
    "${C_SOURCE_DIR}/core/diagnostics.c"
)
target_include_directories(occ_diagnostics PUBLIC "${C_SOURCE_DIR}/core")
set_target_properties(occ_diagnostics PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(occ_compilation STATIC
    "${C_SOURCE_DIR}/core/compilation.c"
)
target_include_directories(occ_compilation PUBLIC "${C_SOURCE_DIR}/core")
target_link_libraries(occ_compilation PUBLIC occ_diagnostics)

# OIR DIAGNOSTICS payload codec bridges the stable container to job-owned records.
add_library(occ_oir_diagnostics STATIC
    "${C_SOURCE_DIR}/oir/oir_diagnostics.c"
)
target_include_directories(occ_oir_diagnostics PUBLIC
    "${C_SOURCE_DIR}/oir"
    "${C_SOURCE_DIR}/core"
)
target_link_libraries(occ_oir_diagnostics PUBLIC occ_oir occ_diagnostics)
set_target_properties(occ_oir_diagnostics PROPERTIES POSITION_INDEPENDENT_CODE ON)




# Frontend token enum <-> stable OIR token ID adapter. Kept separate from the
# container library so the canonical OIR layer does not depend on parser types.
add_library(occ_oir_lexer_adapter STATIC
    "${C_SOURCE_DIR}/oir/oir_token_map.c"
)
target_include_directories(occ_oir_lexer_adapter PUBLIC
    "${C_SOURCE_DIR}/oir"
    "${C_PARSER_DIR}"
)
set_target_properties(occ_oir_lexer_adapter PROPERTIES POSITION_INDEPENDENT_CODE ON)

# TOKEN_STREAM codec validates stable token IDs, source spans and diagnostic references.
add_library(occ_oir_token_stream STATIC
    "${C_SOURCE_DIR}/oir/oir_token_stream.c"
)
target_include_directories(occ_oir_token_stream PUBLIC
    "${C_SOURCE_DIR}/oir"
    "${C_PARSER_DIR}"
)
target_link_libraries(occ_oir_token_stream PUBLIC occ_oir_lexer_adapter)
set_target_properties(occ_oir_token_stream PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Stable frontend AST tag <-> OIR node-kind ID mapping. This adapter is kept
# separate so wire identifiers never depend on C enum ordinals.
add_library(occ_oir_ast_map STATIC
    "${C_SOURCE_DIR}/oir/oir_ast_map.c"
)
target_include_directories(occ_oir_ast_map PUBLIC
    "${C_SOURCE_DIR}/oir"
    "${C_PARSER_DIR}"
)
set_target_properties(occ_oir_ast_map PROPERTIES POSITION_INDEPENDENT_CODE ON)

# AST field TLV primitives are independent of frontend structs and graph traversal.
add_library(occ_oir_ast_tlv STATIC
    "${C_SOURCE_DIR}/oir/oir_ast_tlv.c"
)
target_include_directories(occ_oir_ast_tlv PUBLIC "${C_SOURCE_DIR}/oir")
target_link_libraries(occ_oir_ast_tlv PUBLIC occ_oir)
set_target_properties(occ_oir_ast_tlv PROPERTIES POSITION_INDEPENDENT_CODE ON)

# AST node table codec validates fixed records, TLV ranges and string/node references.
add_library(occ_oir_ast_node_table STATIC
    "${C_SOURCE_DIR}/oir/oir_ast_node_table.c"
)
target_include_directories(occ_oir_ast_node_table PUBLIC "${C_SOURCE_DIR}/oir")
target_link_libraries(occ_oir_ast_node_table PUBLIC occ_oir_ast_tlv)
set_target_properties(occ_oir_ast_node_table PROPERTIES POSITION_INDEPENDENT_CODE ON)

# AST graph validator checks single-owner edges, reachability, cycles and depth.
add_library(occ_oir_ast_graph STATIC
    "${C_SOURCE_DIR}/oir/oir_ast_graph.c"
)
target_include_directories(occ_oir_ast_graph PUBLIC "${C_SOURCE_DIR}/oir")
target_link_libraries(occ_oir_ast_graph PUBLIC occ_oir_ast_node_table)
set_target_properties(occ_oir_ast_graph PROPERTIES POSITION_INDEPENDENT_CODE ON)

# AST field schema validator enforces per-kind required fields and wire types.
add_library(occ_oir_ast_field_schema STATIC
    "${C_SOURCE_DIR}/oir/oir_ast_field_schema.c"
)
target_include_directories(occ_oir_ast_field_schema PUBLIC "${C_SOURCE_DIR}/oir")
target_link_libraries(occ_oir_ast_field_schema PUBLIC occ_oir_ast_node_table)
set_target_properties(occ_oir_ast_field_schema PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Frontend AST pointer index assigns deterministic OIR IDs without allocation.
add_library(occ_oir_ast_frontend_index STATIC
    "${C_SOURCE_DIR}/oir/oir_ast_frontend_index.c"
)
target_include_directories(occ_oir_ast_frontend_index PUBLIC
    "${C_SOURCE_DIR}/oir"
    "${C_PARSER_DIR}"
)
target_link_libraries(occ_oir_ast_frontend_index PUBLIC occ_oir_ast_map)
set_target_properties(occ_oir_ast_frontend_index PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The C frontend is the canonical compiler frontend.
# parser/main.c is intentionally excluded: it is an obsolete standalone entry point.
add_library(occ_frontend STATIC
    "${C_PARSER_DIR}/ast.c"
    "${C_PARSER_DIR}/lexer.c"
    "${C_PARSER_DIR}/parser.c"
    "${C_PARSER_DIR}/semantic.c"
)

set_target_properties(occ_frontend PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(occ_frontend PUBLIC
    "${C_SOURCE_DIR}"
    "${C_PARSER_DIR}"
)

target_compile_definitions(occ_frontend PUBLIC OCC_STATIC)
target_link_libraries(occ_frontend PUBLIC occ_diagnostics)

# strdup is POSIX rather than ISO C11. Expose its declaration on POSIX
# toolchains and map it to the Microsoft CRT spelling on MSVC.
if(MSVC)
    target_compile_definitions(occ_frontend PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
        strdup=_strdup
    )
else()
    target_compile_definitions(occ_frontend PRIVATE _POSIX_C_SOURCE=200809L)
endif()

# Code generation remains a separately loadable module.
# Output name is codegen.dll on Windows, libcodegen.so on Linux,
# and libcodegen.dylib on macOS.
add_library(occ_codegen SHARED
    "${C_CODEGEN_DIR}/codegen.c"
    "${C_CODEGEN_DIR}/target_parser.c"
)

set_target_properties(occ_codegen PROPERTIES
    OUTPUT_NAME codegen
    RUNTIME_OUTPUT_DIRECTORY "${C_RUNTIME_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${C_RUNTIME_DIR}"
)

target_include_directories(occ_codegen PRIVATE
    "${C_SOURCE_DIR}"
    "${C_PARSER_DIR}"
    "${C_CODEGEN_DIR}"
)

target_compile_definitions(occ_codegen PRIVATE
    OCC_STATIC
    BUILD_CODEGEN_DLL
)

target_link_libraries(occ_codegen PRIVATE occ_frontend)

add_executable(occ "${C_SOURCE_DIR}/main.c")

set_target_properties(occ PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${C_RUNTIME_DIR}"
)

target_include_directories(occ PRIVATE
    "${C_SOURCE_DIR}"
    "${C_PARSER_DIR}"
)

target_compile_definitions(occ PRIVATE OCC_STATIC)
if(NOT MSVC)
    target_compile_definitions(occ PRIVATE _POSIX_C_SOURCE=200809L)
endif()
target_link_libraries(occ PRIVATE occ_frontend ${CMAKE_DL_LIBS})

if(MSVC)
    target_compile_options(occ_oir PRIVATE /W4)
    target_compile_options(occ_diagnostics PRIVATE /W4)
    target_compile_options(occ_compilation PRIVATE /W4)
    target_compile_options(occ_oir_lexer_adapter PRIVATE /W4)
    target_compile_options(occ_oir_token_stream PRIVATE /W4)
    target_compile_options(occ_oir_ast_map PRIVATE /W4)
    target_compile_options(occ_oir_ast_tlv PRIVATE /W4)
    target_compile_options(occ_oir_ast_node_table PRIVATE /W4)
    target_compile_options(occ_oir_ast_graph PRIVATE /W4)
    target_compile_options(occ_oir_ast_field_schema PRIVATE /W4)
    target_compile_options(occ_oir_ast_frontend_index PRIVATE /W4)
    target_compile_options(occ_frontend PRIVATE /W4)
    target_compile_options(occ_codegen PRIVATE /W4)
    target_compile_options(occ PRIVATE /W4)
else()
    target_compile_options(occ_oir PRIVATE -Wall -Wextra)
    target_compile_options(occ_diagnostics PRIVATE -Wall -Wextra)
    target_compile_options(occ_compilation PRIVATE -Wall -Wextra)
    target_compile_options(occ_oir_lexer_adapter PRIVATE -Wall -Wextra)
    target_compile_options(occ_oir_token_stream PRIVATE -Wall -Wextra)
    target_compile_options(occ_oir_ast_map PRIVATE -Wall -Wextra)
    target_compile_options(occ_oir_ast_tlv PRIVATE -Wall -Wextra)
    target_compile_options(occ_oir_ast_node_table PRIVATE -Wall -Wextra)
    target_compile_options(occ_oir_ast_graph PRIVATE -Wall -Wextra)
    target_compile_options(occ_oir_ast_field_schema PRIVATE -Wall -Wextra)
    target_compile_options(occ_oir_ast_frontend_index PRIVATE -Wall -Wextra)
    target_compile_options(occ_frontend PRIVATE -Wall -Wextra)
    target_compile_options(occ_codegen PRIVATE -Wall -Wextra)
    target_compile_options(occ PRIVATE -Wall -Wextra)
endif()

if(BUILD_TESTING)
    # A second executable copy proves that config and language assets are
    # resolved beside the executable and that language is read after sections.
    set(C_LOCALE_TEST_DIR "${CMAKE_BINARY_DIR}/locale-tr")
    file(MAKE_DIRECTORY "${C_LOCALE_TEST_DIR}")
    file(WRITE "${C_LOCALE_TEST_DIR}/occ.ocf"
        "[package]\n[package.metadata]\nlanguage = \"tr\"\n"
    )
    file(COPY "${C_SOURCE_DIR}/lang" DESTINATION "${C_LOCALE_TEST_DIR}")
    add_custom_command(TARGET occ POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:occ>"
            "${C_LOCALE_TEST_DIR}/occ${CMAKE_EXECUTABLE_SUFFIX}"
        VERBATIM
    )

    add_executable(test_ast_deserialize
        "${C_SOURCE_DIR}/tests/test_ast_deserialize.c"
    )
    target_include_directories(test_ast_deserialize PRIVATE "${C_PARSER_DIR}")
    target_link_libraries(test_ast_deserialize PRIVATE occ_frontend)
    set(AST_INVALID_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/missing_ast_write_dir")
    file(REMOVE_RECURSE "${AST_INVALID_OUTPUT_DIR}")
    add_test(NAME ast_rejects_malformed_streams
        COMMAND test_ast_deserialize "${AST_INVALID_OUTPUT_DIR}/output.oir"
    )

    # Validate known target descriptors and reject malformed or unsupported profiles.
    add_executable(test_target_parser
        "${C_CODEGEN_DIR}/target_parser.c"
        "${C_SOURCE_DIR}/tests/test_target_parser.c"
    )
    target_include_directories(test_target_parser PRIVATE
        "${C_PARSER_DIR}"
        "${C_CODEGEN_DIR}"
    )
    if(MSVC)
        target_compile_options(test_target_parser PRIVATE /W4)
    else()
        target_compile_options(test_target_parser PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME target_parser_validates_descriptors
        COMMAND test_target_parser
            "${CMAKE_CURRENT_SOURCE_DIR}/targets/BareMetal64.target"
            "${CMAKE_CURRENT_SOURCE_DIR}/targets/Linux64.target"
            "${CMAKE_CURRENT_SOURCE_DIR}/targets/Win64.target"
            "${CMAKE_CURRENT_SOURCE_DIR}/targets/macOS_AArch64.target"
            "${CMAKE_CURRENT_SOURCE_DIR}/targets/RV32IMC.target"
            "${CMAKE_CURRENT_BINARY_DIR}/target_parser_case.target"
    )

    add_test(NAME occ_help COMMAND occ --help)
    set_tests_properties(occ_help PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        PASS_REGULAR_EXPRESSION "BareMetal64[.]target"
    )

    add_test(NAME occ_rejects_unknown_option COMMAND occ --not-an-option)
    set_tests_properties(occ_rejects_unknown_option PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        WILL_FAIL TRUE
    )

    add_test(NAME occ_rejects_missing_source
        COMMAND occ "${C_RUNTIME_DIR}/tests/does_not_exist.nxf"
    )
    set_tests_properties(occ_rejects_missing_source PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        WILL_FAIL TRUE
    )

    add_test(NAME occ_accepts_target_before_source
        COMMAND occ --target BareMetal64 "${C_RUNTIME_DIR}/tests/test1_alt.nxf"
    )
    set_tests_properties(occ_accepts_target_before_source PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )

    add_test(NAME occ_rejects_missing_target_value COMMAND occ --target)
    set_tests_properties(occ_rejects_missing_target_value PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        WILL_FAIL TRUE
    )

    add_test(NAME occ_rejects_multiple_sources
        COMMAND occ "${C_RUNTIME_DIR}/tests/test1.nxf" "${C_RUNTIME_DIR}/tests/test1_alt.nxf"
    )
    set_tests_properties(occ_rejects_multiple_sources PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        WILL_FAIL TRUE
    )

    # Exercises the complete C path: lexer, parser, semantic analysis,
    # AST serialization/deserialization, shared-library loading and codegen.
    add_test(NAME occ_compiles_basic_source
        COMMAND occ "${C_RUNTIME_DIR}/tests/test1.nxf" --target BareMetal64
    )
    set_tests_properties(occ_compiles_basic_source PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )

    add_test(NAME occ_uses_configured_language
        COMMAND "${C_LOCALE_TEST_DIR}/occ${CMAKE_EXECUTABLE_SUFFIX}"
            "${C_SOURCE_DIR}/tests/test_undefined_variable.nxf"
    )
    set_tests_properties(occ_uses_configured_language PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        PASS_REGULAR_EXPRESSION "Tanımsız değişken"
    )

    add_executable(test_compilation "${C_SOURCE_DIR}/tests/test_compilation.c")
    target_link_libraries(test_compilation PRIVATE occ_compilation)
    if(MSVC)
        target_compile_options(test_compilation PRIVATE /W4)
    else()
        target_compile_options(test_compilation PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME compilation_context_state_and_output_gate COMMAND test_compilation)

    add_executable(test_oir_diagnostics "${C_SOURCE_DIR}/tests/test_oir_diagnostics.c")
    target_link_libraries(test_oir_diagnostics PRIVATE occ_oir_diagnostics)
    if(MSVC)
        target_compile_options(test_oir_diagnostics PRIVATE /W4)
    else()
        target_compile_options(test_oir_diagnostics PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME oir_diagnostics_payload_roundtrip_and_validation
        COMMAND test_oir_diagnostics)

    add_executable(test_diagnostics "${C_SOURCE_DIR}/tests/test_diagnostics.c")
    target_link_libraries(test_diagnostics PRIVATE occ_diagnostics)
    if(MSVC)
        target_compile_options(test_diagnostics PRIVATE /W4)
    else()
        target_compile_options(test_diagnostics PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME diagnostic_collector_and_strict_output_gate COMMAND test_diagnostics)

    add_executable(test_oir_token_stream "${C_SOURCE_DIR}/tests/test_oir_token_stream.c")
    target_link_libraries(test_oir_token_stream PRIVATE occ_oir_token_stream)
    if(MSVC)
        target_compile_options(test_oir_token_stream PRIVATE /W4)
    else()
        target_compile_options(test_oir_token_stream PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME oir_token_stream_payload_roundtrip_and_validation
        COMMAND test_oir_token_stream)

    add_executable(test_oir_ast_map "${C_SOURCE_DIR}/tests/test_oir_ast_map.c")
    target_link_libraries(test_oir_ast_map PRIVATE occ_oir_ast_map)
    if(MSVC)
        target_compile_options(test_oir_ast_map PRIVATE /W4)
    else()
        target_compile_options(test_oir_ast_map PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME oir_ast_node_registry_mapping_roundtrip COMMAND test_oir_ast_map)

    add_executable(test_oir_ast_tlv "${C_SOURCE_DIR}/tests/test_oir_ast_tlv.c")
    target_link_libraries(test_oir_ast_tlv PRIVATE occ_oir_ast_tlv)
    if(MSVC)
        target_compile_options(test_oir_ast_tlv PRIVATE /W4)
    else()
        target_compile_options(test_oir_ast_tlv PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME oir_ast_tlv_primitives_roundtrip_and_validation COMMAND test_oir_ast_tlv)

    add_executable(test_oir_ast_node_table "${C_SOURCE_DIR}/tests/test_oir_ast_node_table.c")
    target_link_libraries(test_oir_ast_node_table PRIVATE occ_oir_ast_node_table)
    if(MSVC)
        target_compile_options(test_oir_ast_node_table PRIVATE /W4)
    else()
        target_compile_options(test_oir_ast_node_table PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME oir_ast_node_table_roundtrip_and_validation COMMAND test_oir_ast_node_table)

    add_executable(test_oir_ast_graph "${C_SOURCE_DIR}/tests/test_oir_ast_graph.c")
    target_link_libraries(test_oir_ast_graph PRIVATE occ_oir_ast_graph)
    if(MSVC)
        target_compile_options(test_oir_ast_graph PRIVATE /W4)
    else()
        target_compile_options(test_oir_ast_graph PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME oir_ast_graph_single_owner_reachability_and_depth COMMAND test_oir_ast_graph)

    add_executable(test_oir_ast_field_schema "${C_SOURCE_DIR}/tests/test_oir_ast_field_schema.c")
    target_link_libraries(test_oir_ast_field_schema PRIVATE occ_oir_ast_field_schema)
    if(MSVC)
        target_compile_options(test_oir_ast_field_schema PRIVATE /W4)
    else()
        target_compile_options(test_oir_ast_field_schema PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME oir_ast_field_schema_all_node_kinds_and_compatibility COMMAND test_oir_ast_field_schema)

    add_executable(test_oir_ast_frontend_index "${C_SOURCE_DIR}/tests/test_oir_ast_frontend_index.c")
    target_link_libraries(test_oir_ast_frontend_index PRIVATE occ_oir_ast_frontend_index)
    if(MSVC)
        target_compile_options(test_oir_ast_frontend_index PRIVATE /W4)
    else()
        target_compile_options(test_oir_ast_frontend_index PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME oir_ast_frontend_index_deterministic_preorder_and_graph_rejection
        COMMAND test_oir_ast_frontend_index)

    add_executable(test_oir_token_map "${C_SOURCE_DIR}/tests/test_oir_token_map.c")
    target_link_libraries(test_oir_token_map PRIVATE occ_oir_lexer_adapter)
    if(MSVC)
        target_compile_options(test_oir_token_map PRIVATE /W4)
    else()
        target_compile_options(test_oir_token_map PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME oir_token_registry_mapping_roundtrip COMMAND test_oir_token_map)

    add_executable(test_oir_source_unit "${C_SOURCE_DIR}/tests/test_oir_source_unit.c")
    target_link_libraries(test_oir_source_unit PRIVATE occ_oir)
    if(MSVC)
        target_compile_options(test_oir_source_unit PRIVATE /W4)
    else()
        target_compile_options(test_oir_source_unit PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME oir_source_unit_payload_roundtrip_and_validation
        COMMAND test_oir_source_unit)

    add_executable(test_oir "${C_SOURCE_DIR}/tests/test_oir.c")
    target_link_libraries(test_oir PRIVATE occ_oir)
    if(MSVC)
        target_compile_options(test_oir PRIVATE /W4)
    else()
        target_compile_options(test_oir PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME oir_container_roundtrip_and_validation COMMAND test_oir)

    add_executable(test_lexer "${C_SOURCE_DIR}/tests/test_lexer.c")
    target_include_directories(test_lexer PRIVATE "${C_PARSER_DIR}")
    target_link_libraries(test_lexer PRIVATE occ_frontend)
    add_test(NAME lexer_tokenizes_core_and_flow_tokens COMMAND test_lexer)

    add_executable(test_parser_oir_handoff
        "${C_SOURCE_DIR}/tests/test_parser_oir_handoff.c"
    )
    target_link_libraries(test_parser_oir_handoff PRIVATE
        occ_frontend
        occ_oir_token_stream
    )
    if(MSVC)
        target_compile_options(test_parser_oir_handoff PRIVATE /W4)
    else()
        target_compile_options(test_parser_oir_handoff PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME parser_consumes_oir_token_stream
        COMMAND test_parser_oir_handoff "${C_SOURCE_DIR}/tests/test1.nxf")

    add_executable(test_parser "${C_SOURCE_DIR}/tests/test_parser.c")
    target_include_directories(test_parser PRIVATE "${C_PARSER_DIR}")
    target_link_libraries(test_parser PRIVATE occ_frontend)
    add_test(NAME parser_accepts_and_rejects_sources
        COMMAND test_parser
            "${C_SOURCE_DIR}/tests/test1.nxf"
            "${CMAKE_CURRENT_BINARY_DIR}/invalid_parser_input.nxf"
            "${CMAKE_CURRENT_BINARY_DIR}/invalid_lexer_input.nxf"
    )

    add_executable(test_semantic "${C_SOURCE_DIR}/tests/test_semantic.c")
    target_include_directories(test_semantic PRIVATE "${C_PARSER_DIR}")
    target_link_libraries(test_semantic PRIVATE occ_frontend)
    add_test(NAME semantic_checks_valid_and_undefined_symbols
        COMMAND test_semantic
            "${C_SOURCE_DIR}/tests/test1.nxf"
            "${C_SOURCE_DIR}/tests/test_undefined_variable.nxf"
    )
endif()
