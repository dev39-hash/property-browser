# Script mode (cmake -P): checks the architecture rules of docs/SPEC.md §3.
#
#   cmake -DQPB_DIR=<path to qpb/> -P tools/check_architecture.cmake
#
# R1  Views never branch on type IDs (PropertyDelegate / PropertyTreeView).
# R2  qpb::core includes nothing from QtWidgets or QtGui.
# R5  Public headers never include private headers or anything under src/.
# R6  Sources in qpb/ are ASCII only: MSVC on a non-UTF-8 code page warns
#     (C4819) on other characters, which breaks consumers building with /WX.

if(NOT DEFINED QPB_DIR)
    message(FATAL_ERROR "check_architecture.cmake: QPB_DIR is required")
endif()

set(violations "")

function(check files pattern rule)
    foreach(file IN LISTS files)
        file(STRINGS "${file}" lines REGEX "${pattern}")
        foreach(line IN LISTS lines)
            file(RELATIVE_PATH relative "${QPB_DIR}" "${file}")
            list(APPEND violations "${rule}: ${relative}: ${line}")
        endforeach()
    endforeach()
    set(violations "${violations}" PARENT_SCOPE)
endfunction()

# R1
check("${QPB_DIR}/src/widgets/PropertyDelegate.cpp;${QPB_DIR}/src/widgets/PropertyTreeView.cpp"
    "Types::|typeId\\(\\)" "R1 (view branches on a type)")

# R2
file(GLOB core_headers "${QPB_DIR}/include/qpb/*.h")
file(GLOB_RECURSE core_sources "${QPB_DIR}/src/core/*")
check("${core_headers};${core_sources}"
    "#include[ \t]*<(QtWidgets|QtGui|QWidget|QPainter|QColor|QFont|QIcon|QStyle|QApplication)"
    "R2 (core depends on QtWidgets/QtGui)")

# R5
file(GLOB_RECURSE public_headers "${QPB_DIR}/include/*.h")
check("${public_headers}" "#include.*(_p\\.h|src/)" "R5 (public header includes internals)")

# R6
file(GLOB_RECURSE component_sources "${QPB_DIR}/*.h" "${QPB_DIR}/*.cpp" "${QPB_DIR}/*.in"
    "${QPB_DIR}/*.cmake" "${QPB_DIR}/CMakeLists.txt")
foreach(file IN LISTS component_sources)
    file(READ "${file}" content HEX)
    if(content MATCHES "^(..)*[89a-f].")
        file(RELATIVE_PATH relative "${QPB_DIR}" "${file}")
        list(APPEND violations "R6 (non-ASCII character): ${relative}")
    endif()
endforeach()

if(violations)
    list(JOIN violations "\n  " text)
    message(FATAL_ERROR "Architecture rule violations:\n  ${text}")
endif()
message("Architecture rules R1, R2, R5, R6: OK")
