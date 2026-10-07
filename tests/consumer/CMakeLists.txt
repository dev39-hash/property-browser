# Consumer integration tests (docs/SPEC.md §6, §7, PLAN M0.5 / M4.5).
#
# Each test copies qpb/ into components/qpb/ of a fresh standalone project
# (tests/consumer/project) and configures, builds and runs it, exactly as a
# consuming application would. The project also fails to configure if qpb
# changes any of the host's global CMake settings.
#
#   consumer_cxx17    static, C++17, host finds Qt first
#   consumer_cxx20    static, C++20, qpb finds Qt itself
#   consumer_shared   QPB_BUILD_SHARED=ON, C++17
#   consumer_upgrade  build, replace components/qpb with a newer copy,
#                     rebuild in the same build tree, check the new version

set(_qpb_consumer_scenarios
    # name    | std | find Qt first | shared | upgrade
    "cxx17    | 17  | ON            | OFF    | OFF"
    "cxx20    | 20  | OFF           | OFF    | OFF"
    "shared   | 17  | ON            | ON     | OFF"
    "upgrade  | 17  | ON            | OFF    | ON"
)

# Installation prefix of the Qt in use (QT_DIR is <prefix>/lib/cmake/Qt6 or Qt5).
get_filename_component(_qpb_qt_prefix "${QT_DIR}/../../.." ABSOLUTE)

foreach(scenario IN LISTS _qpb_consumer_scenarios)
    string(REPLACE " " "" scenario "${scenario}")
    string(REPLACE "|" ";" scenario "${scenario}")
    list(GET scenario 0 name)
    list(GET scenario 1 std)
    list(GET scenario 2 find_qt_first)
    list(GET scenario 3 shared)
    list(GET scenario 4 upgrade)

    add_test(NAME consumer_${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DQPB_DIR=${PROJECT_SOURCE_DIR}/qpb"
            "-DPROJECT_TEMPLATE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/project"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/${name}"
            "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DQT_PREFIX=${_qpb_qt_prefix}"
            "-DCXX_STANDARD=${std}"
            "-DFIND_QT_FIRST=${find_qt_first}"
            "-DSHARED=${shared}"
            "-DUPGRADE=${upgrade}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_consumer_test.cmake"
    )
    set_tests_properties(consumer_${name} PROPERTIES
        LABELS consumer
        TIMEOUT 900
    )
endforeach()
