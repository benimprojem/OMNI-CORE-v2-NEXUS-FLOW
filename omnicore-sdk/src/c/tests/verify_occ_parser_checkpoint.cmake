if(NOT DEFINED OCC_EXE OR NOT DEFINED SOURCE_FILE OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "OCC_EXE, SOURCE_FILE and WORK_DIR are required")
endif()

file(REMOVE_RECURSE "${WORK_DIR}/occ-errors")
file(MAKE_DIRECTORY "${WORK_DIR}")
execute_process(
    COMMAND "${OCC_EXE}" "${SOURCE_FILE}"
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE occ_result
    OUTPUT_VARIABLE occ_stdout
    ERROR_VARIABLE occ_stderr
)
if(occ_result EQUAL 0)
    message(FATAL_ERROR "Invalid source unexpectedly compiled successfully")
endif()

set(combined_output "${occ_stdout}\n${occ_stderr}")
if(NOT combined_output MATCHES "Hata kaydi saklandi:")
    message(FATAL_ERROR "OCC did not report a persisted failure checkpoint:\n${combined_output}")
endif()

file(GLOB checkpoint_files "${WORK_DIR}/occ-errors/job-*.oir")
list(LENGTH checkpoint_files checkpoint_count)
if(NOT checkpoint_count EQUAL 1)
    message(FATAL_ERROR "Expected exactly one checkpoint, found ${checkpoint_count}")
endif()

list(GET checkpoint_files 0 checkpoint_file)
file(READ "${checkpoint_file}" checkpoint_hex HEX)
file(READ "${SOURCE_FILE}" expected_source_hex HEX)
string(FIND "${checkpoint_hex}" "${expected_source_hex}" source_offset)
if(source_offset LESS 0)
    message(FATAL_ERROR "Checkpoint does not preserve the original invalid source bytes")
endif()

message(STATUS "OCC parser DLL failure checkpoint preserves source and diagnostics")
