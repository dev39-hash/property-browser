# Consumer integration tests (docs/SPEC.md §6, §7).
#
# Each test copies qpb/ into components/qpb/ of a fresh standalone project
# (tests/consumer/project) and configures, builds and runs it, exactly as a
# consuming application would.
#
#   consumer_cxx17  host finds Qt first, C++17
#   consumer_cxx20  qpb finds Qt itself, C++20

set(_qpb_consumer_scenarios
    "cxx17|17|ON"
    "cxx20|20|OFF"
)

foreach(scenario IN LISTS _qpb_consumer_scenarios)
    string(REPLACE "|" ";" scenario "${scenario}")
    list(GET scenario 0 name)
    list(GET scenario 1 std)
    list(GET scenario 2 find_qt_first)

    add_test(NAME consumer_${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DQPB_DIR=${PROJECT_SOURCE_DIR}/qpb"
            "-DPROJECT_TEMPLATE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/project"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/${name}"
            "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DQT_PREFIX=${QT6_INSTALL_PREFIX}"
            "-DCXX_STANDARD=${std}"
            "-DFIND_QT_FIRST=${find_qt_first}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_consumer_test.cmake"
    )
    set_tests_properties(consumer_${name} PROPERTIES
        LABELS consumer
        TIMEOUT 600
    )
endforeach()
