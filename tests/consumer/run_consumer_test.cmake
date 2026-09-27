# Script mode (cmake -P). Builds a throwaway consumer project that embeds qpb as
# a copied component folder, then runs the resulting executable.
#
# Required variables: QPB_DIR, PROJECT_TEMPLATE_DIR, WORK_DIR, GENERATOR,
# CXX_COMPILER, QT_PREFIX, CXX_STANDARD, FIND_QT_FIRST, SHARED, UPGRADE.

foreach(var QPB_DIR PROJECT_TEMPLATE_DIR WORK_DIR GENERATOR CXX_COMPILER QT_PREFIX CXX_STANDARD
        FIND_QT_FIRST SHARED UPGRADE)
    if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
        message(FATAL_ERROR "run_consumer_test.cmake: ${var} is not set")
    endif()
endforeach()

set(source_dir "${WORK_DIR}/source")
set(build_dir "${WORK_DIR}/build")
set(component_dir "${source_dir}/components/qpb")

function(run_step description)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE output
        ERROR_VARIABLE output)
    if(NOT result EQUAL 0)
        message("${output}")
        message(FATAL_ERROR "Consumer test failed while ${description} (exit code ${result})")
    endif()
    set(step_output "${output}" PARENT_SCOPE)
endfunction()

# Copies qpb/ into the consumer, optionally with a different VERSION.
function(install_component version)
    file(REMOVE_RECURSE "${component_dir}")
    file(COPY "${QPB_DIR}/" DESTINATION "${component_dir}")
    if(version)
        file(WRITE "${component_dir}/VERSION" "${version}\n")
    endif()
endfunction()

function(build_and_run expected_version)
    run_step("building" "${CMAKE_COMMAND}" --build "${build_dir}" --config Debug)
    run_step("running the consumer application" "${build_dir}/bin/qpb_consumer")
    message("${step_output}")
    if(expected_version AND NOT step_output MATCHES "qpb ${expected_version} ")
        message(FATAL_ERROR "Expected qpb ${expected_version}, got: ${step_output}")
    endif()
endfunction()

# Start from scratch: a clean checkout of the consuming project.
file(REMOVE_RECURSE "${WORK_DIR}")
file(COPY "${PROJECT_TEMPLATE_DIR}/" DESTINATION "${source_dir}")
install_component("")

run_step("configuring"
    "${CMAKE_COMMAND}" -S "${source_dir}" -B "${build_dir}"
        -G "${GENERATOR}"
        "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
        "-DCMAKE_BUILD_TYPE=Debug"
        "-DCMAKE_PREFIX_PATH=${QT_PREFIX}"
        "-DCONSUMER_CXX_STANDARD=${CXX_STANDARD}"
        "-DCONSUMER_FIND_QT_FIRST=${FIND_QT_FIRST}"
        "-DQPB_BUILD_SHARED=${SHARED}"
)
build_and_run("")

if(UPGRADE)
    # "Replace the folder and rebuild": same build tree, no reconfigure by hand,
    # no change to the consumer's CMake or code.
    install_component("9.8.7")
    build_and_run("9.8.7")
endif()
