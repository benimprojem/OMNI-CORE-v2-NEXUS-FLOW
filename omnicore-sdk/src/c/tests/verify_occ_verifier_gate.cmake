if(NOT DEFINED OCC_EXE OR NOT DEFINED OCC_RUNTIME_DIR OR
   NOT DEFINED REJECTING_VERIFIER OR NOT DEFINED VERIFIER_FILENAME OR
   NOT DEFINED SOURCE_FILE OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "Verifier-gate test arguments are incomplete")
endif()

foreach(required_path IN ITEMS
    "${OCC_EXE}" "${OCC_RUNTIME_DIR}" "${REJECTING_VERIFIER}" "${SOURCE_FILE}")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Required verifier-gate test input does not exist: ${required_path}")
    endif()
endforeach()

get_filename_component(package_dir "${OCC_RUNTIME_DIR}" DIRECTORY)
get_filename_component(occ_exe_name "${OCC_EXE}" NAME)
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/bin")
file(COPY "${OCC_RUNTIME_DIR}/" DESTINATION "${WORK_DIR}/bin")
file(COPY "${package_dir}/occ.ocf" DESTINATION "${WORK_DIR}")
foreach(package_dir_name IN ITEMS targets lang libs dsplibs)
    if(EXISTS "${package_dir}/${package_dir_name}")
        file(COPY "${package_dir}/${package_dir_name}" DESTINATION "${WORK_DIR}")
    endif()
endforeach()
configure_file("${REJECTING_VERIFIER}"
    "${WORK_DIR}/bin/${VERIFIER_FILENAME}" COPYONLY)

get_filename_component(source_name "${SOURCE_FILE}" NAME)
get_filename_component(source_stem "${SOURCE_FILE}" NAME_WE)
configure_file("${SOURCE_FILE}" "${WORK_DIR}/${source_name}" COPYONLY)

execute_process(
    COMMAND "${WORK_DIR}/bin/${occ_exe_name}"
        "${WORK_DIR}/${source_name}" --target BareMetal64
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE occ_result
    OUTPUT_VARIABLE occ_stdout
    ERROR_VARIABLE occ_stderr
)
if(occ_result EQUAL 0)
    message(FATAL_ERROR
        "OCC unexpectedly continued after the rejecting OIR Verifier:
${occ_stdout}
${occ_stderr}")
endif()

set(combined_output "${occ_stdout}
${occ_stderr}")
if(NOT combined_output MATCHES "code=9902")
    message(FATAL_ERROR
        "OCC did not surface the rejecting verifier's diagnostic 9902:
${combined_output}")
endif()

foreach(forbidden_output IN ITEMS
    "${source_stem}.assembly.oir"
    "${source_stem}.stage.s"
    "${source_stem}.s"
    "${source_stem}.module.oir")
    if(EXISTS "${WORK_DIR}/${forbidden_output}")
        message(FATAL_ERROR
            "A downstream artifact was emitted after verifier rejection: ${forbidden_output}")
    endif()
endforeach()

message(STATUS
    "Host verifier rejection blocked OIR_MODULE publication, Codegen stage, and legacy Codegen output")
