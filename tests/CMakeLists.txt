find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

# qpb_add_test(<name> SOURCES <files...> LIBRARIES <targets...>)
#
# Builds a Qt Test executable and registers it with CTest. Widget tests run on
# the offscreen platform so they need no display.
function(qpb_add_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LIBRARIES")
    add_executable(${name} ${arg_SOURCES})
    target_link_libraries(${name} PRIVATE Qt${QT_VERSION_MAJOR}::Test ${arg_LIBRARIES})
    set_target_properties(${name} PROPERTIES AUTOMOC ON)
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Qt 6.5's QTEST_*_MAIN macros trip this pedantic check.
        target_compile_options(${name} PRIVATE -Wno-gnu-zero-variadic-macro-arguments)
    endif()
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

add_subdirectory(api)
add_subdirectory(api_compat)
add_subdirectory(core)
add_subdirectory(widgets)
add_subdirectory(consumer)

# --- repository checks (docs/PLAN.md M4.3, M4.6) --------------------------------

add_test(NAME architecture_rules
    COMMAND "${CMAKE_COMMAND}" "-DQPB_DIR=${PROJECT_SOURCE_DIR}/qpb"
        -P "${PROJECT_SOURCE_DIR}/tools/check_architecture.cmake"
)
add_test(NAME qtcreator_examples_listed
    COMMAND "${CMAKE_COMMAND}" "-DEXAMPLES_DIR=${PROJECT_SOURCE_DIR}/examples"
        -P "${PROJECT_SOURCE_DIR}/tools/check_qtcreator_examples.cmake"
)
add_test(NAME size_quickstart_s1
    COMMAND "${CMAKE_COMMAND}" -DLIMIT=30
        "-DFILES=${PROJECT_SOURCE_DIR}/examples/quickstart/main.cpp"
        -P "${PROJECT_SOURCE_DIR}/tools/count_lines.cmake"
)
add_test(NAME size_custom_type_s2
    COMMAND "${CMAKE_COMMAND}" -DLIMIT=100
        "-DFILES=${PROJECT_SOURCE_DIR}/examples/custom_type/ColorType.h$<SEMICOLON>${PROJECT_SOURCE_DIR}/examples/custom_type/ColorType.cpp"
        -P "${PROJECT_SOURCE_DIR}/tools/count_lines.cmake"
)
