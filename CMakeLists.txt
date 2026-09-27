# qpb — property browser for Qt 6 Widgets.
#
# This folder is a self-contained component. Consuming projects copy it (e.g. to
# components/qpb/) and use it with:
#
#   add_subdirectory(components/qpb)
#   target_link_libraries(my_app PRIVATE qpb::widgets)
#
# Rules for this file (docs/SPEC.md §6.3): never touch global variables or flags,
# configure only qpb's own targets, and never refer to CMAKE_SOURCE_DIR.

cmake_minimum_required(VERSION 3.21)

file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/VERSION" _qpb_version LIMIT_COUNT 1)
string(STRIP "${_qpb_version}" _qpb_version)
project(qpb VERSION ${_qpb_version} LANGUAGES CXX)
unset(_qpb_version)

option(QPB_BUILD_SHARED "Build qpb as shared libraries instead of static ones" OFF)

set(QPB_MIN_QT_VERSION 6.5)

# Reuse the host project's Qt if it has already been found; otherwise find it here.
if(TARGET Qt6::Widgets)
    if(DEFINED Qt6_VERSION AND Qt6_VERSION VERSION_LESS QPB_MIN_QT_VERSION)
        message(FATAL_ERROR "qpb requires Qt >= ${QPB_MIN_QT_VERSION}, found ${Qt6_VERSION}")
    endif()
else()
    find_package(Qt6 ${QPB_MIN_QT_VERSION} REQUIRED COMPONENTS Core Widgets)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cmake/qpbTargetHelpers.cmake")

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/qpbversion.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/qpb/qpbversion.h"
    @ONLY
)

# --- qpb::core (Qt6::Core only) ----------------------------------------------
#
# Headers are listed here together with their implementation (docs/PLAN.md
# M2/M3); the remaining public headers are declarations only for now.

qpb_add_library(qpb_core
    EXPORT_DEFINE QPB_BUILD_CORE_LIB
    OUTPUT_NAME qpbcore
    SOURCES
        include/qpb/qpbglobal.h
        "${CMAKE_CURRENT_BINARY_DIR}/include/qpb/qpbversion.h"
        src/core/qpbglobal.cpp
)
target_link_libraries(qpb_core PUBLIC Qt6::Core)
add_library(qpb::core ALIAS qpb_core)

# --- qpb::widgets (Qt6::Widgets) ---------------------------------------------

qpb_add_library(qpb_widgets
    EXPORT_DEFINE QPB_BUILD_WIDGETS_LIB
    OUTPUT_NAME qpbwidgets
    SOURCES
        src/widgets/qpbwidgets.cpp
)
target_link_libraries(qpb_widgets PUBLIC qpb_core Qt6::Widgets)
add_library(qpb::widgets ALIAS qpb_widgets)
