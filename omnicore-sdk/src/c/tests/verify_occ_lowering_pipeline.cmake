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
set(assembly_artifact "${WORK_DIR}/${source_stem}.assembly.oir")
set(stage_assembly "${WORK_DIR}/${source_stem}.stage.s")
set(legacy_assembly "${WORK_DIR}/${source_stem}.s")

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
file(READ "${module_artifact}" module_producer_stage OFFSET 20 LIMIT 4 HEX)
if(NOT module_producer_stage STREQUAL "08000000")
    message(FATAL_ERROR
        "OIR_MODULE artifact was not passed through the OIR Verifier stage (producer=${module_producer_stage})")
endif()

file(GLOB temporary_artifacts "${module_artifact}.tmp.*")
list(LENGTH temporary_artifacts temporary_count)
if(NOT temporary_count EQUAL 0)
    message(FATAL_ERROR "Temporary artifact files were left behind: ${temporary_artifacts}")
endif()

if(NOT EXISTS "${assembly_artifact}")
    message(FATAL_ERROR "Expected OIR_ASSEMBLY artifact was not written: ${assembly_artifact}")
endif()
if(NOT EXISTS "${stage_assembly}")
    message(FATAL_ERROR "Codegen stage assembly text was not written: ${stage_assembly}")
endif()
if(NOT EXISTS "${legacy_assembly}")
    message(FATAL_ERROR "Legacy Codegen parity output was not written: ${legacy_assembly}")
endif()

file(SIZE "${assembly_artifact}" assembly_size)
if(assembly_size LESS 96)
    message(FATAL_ERROR "OIR_ASSEMBLY artifact is too small: ${assembly_size} bytes")
endif()
file(READ "${assembly_artifact}" assembly_magic LIMIT 4 HEX)
if(NOT assembly_magic STREQUAL "4f495231")
    message(FATAL_ERROR "OIR_ASSEMBLY has an invalid OIR container signature: ${assembly_magic}")
endif()

file(READ "${stage_assembly}" stage_text)
foreach(required_text IN ITEMS "global _kernel_main" "call main" "imul eax, ebx" "add eax, ebx" "cmp eax, ebx" "setl al" "jne .Lbb_2" "jmp .Lbb_3")
    string(FIND "${stage_text}" "${required_text}" found_at)
    if(found_at LESS 0)
        message(FATAL_ERROR "Codegen stage output is missing '${required_text}':\n${stage_text}")
    endif()
endforeach()

message(STATUS "OCC CLI persisted verified OIR_MODULE and OIR_ASSEMBLY; legacy Codegen output is retained for parity checks")
