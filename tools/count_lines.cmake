# Script mode (cmake -P): checks the size criteria S1 and S2 (docs/SPEC.md §1.3).
#
#   cmake -DLIMIT=30 -DFILES="a.cpp;b.h" -P tools/count_lines.cmake
#
# Counts lines that are not blank, not // comments and not #include lines.

if(NOT DEFINED LIMIT OR NOT DEFINED FILES)
    message(FATAL_ERROR "count_lines.cmake: LIMIT and FILES are required")
endif()

set(total 0)
foreach(file IN LISTS FILES)
    file(STRINGS "${file}" lines ENCODING UTF-8)
    foreach(line IN LISTS lines)
        string(STRIP "${line}" line)
        if(line STREQUAL "" OR line MATCHES "^//" OR line MATCHES "^#include")
            continue()
        endif()
        math(EXPR total "${total} + 1")
    endforeach()
endforeach()

message("${total} counted lines (limit ${LIMIT})")
if(total GREATER LIMIT)
    message(FATAL_ERROR "Size criterion exceeded: ${total} > ${LIMIT}")
endif()
