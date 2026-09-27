# API compatibility tests (docs/SPEC.md §9.5). One file per released minor
# version; files of released versions are never edited. Built with the
# consumer-side strictness, deprecation warnings excepted.

foreach(version IN ITEMS v1_0 v1_1)
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

# Public API snapshots (docs/SPEC.md §9.5, PLAN M4.8, M5.5): one per released
# minor version, never edited afterwards. Each check fails when a declaration of
# that snapshot disappears or changes; additions are allowed. A new minor
# release adds its snapshot (docs/RELEASING.md):
#   python3 tools/api_snapshot.py --include qpb/include --write tests/api_compat/api-1.<minor>.txt
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    foreach(version IN ITEMS 1.0 1.1)
        add_test(NAME api_snapshot_${version}
            COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tools/api_snapshot.py"
                --include "${PROJECT_SOURCE_DIR}/qpb/include"
                --check "${CMAKE_CURRENT_SOURCE_DIR}/api-${version}.txt"
        )
    endforeach()
else()
    message(STATUS "Python 3 not found: the api_snapshot tests are not available")
endif()
