# API compatibility tests (docs/SPEC.md §9.5). One file per released minor
# version; files of released versions are never edited. Built with the
# consumer-side strictness, deprecation warnings excepted.

foreach(version IN ITEMS v1_0)
    qpb_add_test(tst_api_compat_${version}
        SOURCES ${version}.cpp
        LIBRARIES qpb::widgets
    )
    if(MSVC)
        target_compile_options(tst_api_compat_${version} PRIVATE /WX /wd4996 /utf-8)
    else()
        target_compile_options(tst_api_compat_${version} PRIVATE
            -Werror -Wno-error=deprecated-declarations)
    endif()
endforeach()

# Public API snapshot (docs/SPEC.md §9.5, PLAN M4.8): fails when a declaration of
# the baseline disappears or changes. Additions are allowed. Before 1.0 the
# baseline is regenerated on purpose with:
#   python3 tools/api_snapshot.py --include qpb/include --write tests/api_compat/api-1.0.txt
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME api_snapshot
        COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tools/api_snapshot.py"
            --include "${PROJECT_SOURCE_DIR}/qpb/include"
            --check "${CMAKE_CURRENT_SOURCE_DIR}/api-1.0.txt"
    )
else()
    message(STATUS "Python 3 not found: the api_snapshot test is not available")
endif()
