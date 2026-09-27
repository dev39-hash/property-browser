find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

# qpb_add_test(<name> SOURCES <files...> LIBRARIES <targets...>)
#
# Builds a Qt Test executable and registers it with CTest. Widget tests run on
# the offscreen platform so they need no display.
function(qpb_add_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LIBRARIES")
    add_executable(${name} ${arg_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${arg_LIBRARIES})
    set_target_properties(${name} PROPERTIES AUTOMOC ON)
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4)
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

add_subdirectory(core)
add_subdirectory(consumer)
