# Examples use qpb exactly like a consuming application.

function(qpb_add_example name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES")
    add_executable(${name} ${arg_SOURCES})
    target_link_libraries(${name} PRIVATE qpb::widgets)
    set_target_properties(${name} PROPERTIES AUTOMOC ON)
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

set(_qpb_color_type custom_type/ColorType.h custom_type/ColorType.cpp)

qpb_add_example(qpb_example_quickstart SOURCES quickstart/main.cpp)
qpb_add_example(qpb_example_custom_type SOURCES custom_type/main.cpp ${_qpb_color_type})
qpb_add_example(qpb_example_inspector SOURCES inspector/main.cpp ${_qpb_color_type})
qpb_add_example(qpb_example_settings_dialog SOURCES settings_dialog/main.cpp)
qpb_add_example(qpb_example_plugin_config SOURCES plugin_config/main.cpp)
