cmake_minimum_required(VERSION 3.16)

project(OmniCoreC VERSION 0.2.0 LANGUAGES C)

include(CTest)

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(C_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src - c - 28-01-26")
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
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/targets" DESTINATION "${CMAKE_BINARY_DIR}")

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
target_link_libraries(occ PRIVATE occ_frontend ${CMAKE_DL_LIBS})

if(MSVC)
    target_compile_options(occ_frontend PRIVATE /W4)
    target_compile_options(occ_codegen PRIVATE /W4)
    target_compile_options(occ PRIVATE /W4)
else()
    target_compile_options(occ_frontend PRIVATE -Wall -Wextra)
    target_compile_options(occ_codegen PRIVATE -Wall -Wextra)
    target_compile_options(occ PRIVATE -Wall -Wextra)
endif()

if(BUILD_TESTING)
    add_executable(test_ast_deserialize
        "${C_SOURCE_DIR}/tests/test_ast_deserialize.c"
    )
    target_include_directories(test_ast_deserialize PRIVATE "${C_PARSER_DIR}")
    target_link_libraries(test_ast_deserialize PRIVATE occ_frontend)
    add_test(NAME ast_rejects_malformed_streams COMMAND test_ast_deserialize)

    add_test(NAME occ_help COMMAND occ --help)
    set_tests_properties(occ_help PROPERTIES
        WORKING_DIRECTORY "${C_RUNTIME_DIR}"
    )

    add_test(NAME occ_rejects_unknown_option COMMAND occ --not-an-option)
    set_tests_properties(occ_rejects_unknown_option PROPERTIES
        WORKING_DIRECTORY "${C_RUNTIME_DIR}"
        WILL_FAIL TRUE
    )

    # Exercises the complete C path: lexer, parser, semantic analysis,
    # AST serialization/deserialization, shared-library loading and codegen.
    add_test(NAME occ_compiles_basic_source
        COMMAND occ tests/test1.nxf --target BareMetal64
    )
    set_tests_properties(occ_compiles_basic_source PROPERTIES
        WORKING_DIRECTORY "${C_RUNTIME_DIR}"
    )
endif()
