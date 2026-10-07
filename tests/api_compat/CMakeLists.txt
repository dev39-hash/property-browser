# API compatibility tests (docs/SPEC.md §9.5). One file per released minor
# version; files of released versions are never edited. Built with the
# consumer-side strictness, deprecation warnings excepted.
#
# Since 1.7 qpb also builds with Qt 5.15 (PLAN M11, D53), with a slightly
# different API where Qt 5 has no equivalent. v1_0 uses those parts in their
# Qt 6 form, so it is built with Qt 6 only; qt5_v1_7 freezes their Qt 5 form.

set(_qpb_compat_files v1_0 v1_1 v1_2 v1_3 v1_4 v1_5 v1_6)
if(QT_VERSION_MAJOR EQUAL 5)
    list(REMOVE_ITEM _qpb_compat_files v1_0)
    list(APPEND _qpb_compat_files qt5_v1_7)
endif()

foreach(version IN LISTS _qpb_compat_files)
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
# The snapshots check header text, so both Qt configurations run all of them:
# api-1.x for the Qt 6 API, api-qt5-1.x (since 1.7, --qt-major 5) for the Qt 5 one.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    foreach(version IN ITEMS 1.0 1.1 1.2 1.3 1.4 1.5 1.6 1.7)
        add_test(NAME api_snapshot_${version}
            COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tools/api_snapshot.py"
                --include "${PROJECT_SOURCE_DIR}/qpb/include"
                --check "${CMAKE_CURRENT_SOURCE_DIR}/api-${version}.txt"
        )
    endforeach()
    foreach(version IN ITEMS 1.7)
        add_test(NAME api_snapshot_qt5_${version}
            COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tools/api_snapshot.py"
                --include "${PROJECT_SOURCE_DIR}/qpb/include" --qt-major 5
                --check "${CMAKE_CURRENT_SOURCE_DIR}/api-qt5-${version}.txt"
        )
    endforeach()
else()
    message(STATUS "Python 3 not found: the api_snapshot tests are not available")
endif()
