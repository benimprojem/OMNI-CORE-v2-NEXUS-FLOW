if(NOT DEFINED OCC_EXE OR NOT DEFINED SOURCE_FILE OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "OCC_EXE, SOURCE_FILE and WORK_DIR are required")
endif()

if(NOT EXISTS "${OCC_EXE}")
    message(FATAL_ERROR "OCC executable does not exist: ${OCC_EXE}")
endif()
if(NOT EXISTS "${SOURCE_FILE}")
    message(FATAL_ERROR "Pipeline fixture does not exist: ${SOURCE_FILE}")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
file(COPY "${SOURCE_FILE}" DESTINATION "${WORK_DIR}")
get_filename_component(source_name "${SOURCE_FILE}" NAME)
get_filename_component(source_stem "${SOURCE_FILE}" NAME_WE)
set(local_source "${WORK_DIR}/${source_name}")
set(module_artifact "${WORK_DIR}/${source_stem}.module.oir")

execute_process(
    COMMAND "${OCC_EXE}" "${local_source}" --target BareMetal64
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE occ_result
    OUTPUT_VARIABLE occ_stdout
    ERROR_VARIABLE occ_stderr
)
if(NOT occ_result EQUAL 0)
    message(FATAL_ERROR
        "OCC failed while exercising the lowering stage (exit=${occ_result}):\n"
        "${occ_stdout}\n${occ_stderr}")
endif()

string(FIND "${occ_stdout}" "OIR_MODULE kaydedildi:" module_message)
if(module_message LESS 0)
    message(FATAL_ERROR
        "OCC did not report the OIR_MODULE artifact path:\n${occ_stdout}\n${occ_stderr}")
endif()
if(NOT EXISTS "${module_artifact}")
    message(FATAL_ERROR "Expected OIR_MODULE artifact was not written: ${module_artifact}")
endif()

file(SIZE "${module_artifact}" module_size)
if(module_size LESS 96)
    message(FATAL_ERROR
        "OIR_MODULE artifact is too small to contain its envelope and payload: ${module_size} bytes")
endif()
file(READ "${module_artifact}" module_magic LIMIT 4 HEX)
if(NOT module_magic STREQUAL "4f495231")
    message(FATAL_ERROR
        "OIR_MODULE artifact has an invalid OIR container signature: ${module_magic}")
endif()

file(GLOB temporary_artifacts "${module_artifact}.tmp.*")
list(LENGTH temporary_artifacts temporary_count)
if(NOT temporary_count EQUAL 0)
    message(FATAL_ERROR "Temporary artifact files were left behind: ${temporary_artifacts}")
endif()

message(STATUS "OCC CLI persisted the validated OIR_MODULE artifact")
