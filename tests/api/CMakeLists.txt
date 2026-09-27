# Compile-only checks of the public API (docs/PLAN.md M1.3, M1.5). Nothing here
# is linked or run: a check fails by failing the build.

# Every public header must compile on its own and be include-guarded. One
# translation unit per header, compiled with strict warnings.
file(GLOB_RECURSE _qpb_public_headers
    RELATIVE "${PROJECT_SOURCE_DIR}/qpb/include"
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/qpb/include/qpb/*.h"
)
set(_qpb_header_check_sources "")
foreach(header IN LISTS _qpb_public_headers)
    string(MAKE_C_IDENTIFIER "${header}" name)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/self_contained/${name}.cpp")
    file(CONFIGURE OUTPUT "${source}"
        CONTENT "#include <${header}>\n#include <${header}>\n"
        @ONLY
    )
    list(APPEND _qpb_header_check_sources "${source}")
endforeach()

add_library(qpb_api_check OBJECT
    ${_qpb_header_check_sources}
    future_sketches.cpp
)
target_link_libraries(qpb_api_check PRIVATE qpb::widgets)
if(MSVC)
    target_compile_options(qpb_api_check PRIVATE /W4 /WX /utf-8)
else()
    target_compile_options(qpb_api_check PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
