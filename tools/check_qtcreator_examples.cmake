# Script mode (cmake -P): the Qt Creator projects in examples/qtcreator/ cover
# every example.
#
#   cmake -DEXAMPLES_DIR=<path to examples/> -P tools/check_qtcreator_examples.cmake
#
# The CMake project there reuses examples/CMakeLists.txt, so each folder
# examples/<name>/ must appear in that file. The qmake project needs <name> in
# QPB_EXAMPLES of qpb_examples.pro and a file qmake/<name>/<name>.pro; it must
# not list an example that does not exist.

cmake_minimum_required(VERSION 3.21) # policies for script mode (IN_LIST)

if(NOT DEFINED EXAMPLES_DIR)
    message(FATAL_ERROR "check_qtcreator_examples.cmake: EXAMPLES_DIR is required")
endif()

set(qtcreator "${EXAMPLES_DIR}/qtcreator")
set(problems "")

file(GLOB entries LIST_DIRECTORIES true RELATIVE "${EXAMPLES_DIR}" "${EXAMPLES_DIR}/*")
set(examples "")
foreach(entry IN LISTS entries)
    if(IS_DIRECTORY "${EXAMPLES_DIR}/${entry}" AND NOT entry STREQUAL "qtcreator")
        list(APPEND examples "${entry}")
    endif()
endforeach()

file(READ "${EXAMPLES_DIR}/CMakeLists.txt" cmake_list)
file(STRINGS "${qtcreator}/qpb_examples.pro" pro_line REGEX "^QPB_EXAMPLES[ \t]*=")
string(REGEX REPLACE "^QPB_EXAMPLES[ \t]*=[ \t]*" "" pro_examples "${pro_line}")
string(STRIP "${pro_examples}" pro_examples)
string(REGEX REPLACE "[ \t\r]+" ";" pro_examples "${pro_examples}")

foreach(name IN LISTS examples)
    string(FIND "${cmake_list}" "${name}/" found)
    if(found EQUAL -1)
        list(APPEND problems "examples/CMakeLists.txt does not build ${name}")
    endif()
    if(NOT name IN_LIST pro_examples)
        list(APPEND problems "QPB_EXAMPLES in qtcreator/qpb_examples.pro lacks ${name}")
    endif()
    if(NOT EXISTS "${qtcreator}/qmake/${name}/${name}.pro")
        list(APPEND problems "qtcreator/qmake/${name}/${name}.pro is missing")
    endif()
endforeach()

foreach(name IN LISTS pro_examples)
    if(NOT name IN_LIST examples)
        list(APPEND problems "qtcreator/qpb_examples.pro lists ${name}, which is not an example")
    endif()
endforeach()

list(LENGTH examples count)
if(problems)
    list(JOIN problems "\n  " text)
    message(FATAL_ERROR "Qt Creator example projects are out of date:\n  ${text}")
endif()
message(STATUS "Qt Creator example projects cover all ${count} examples")
