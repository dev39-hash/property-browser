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

qpb_add_library(qpb_core
    EXPORT_DEFINE QPB_BUILD_CORE_LIB
    OUTPUT_NAME qpbcore
    SOURCES
        include/qpb/Attributes.h
        include/qpb/Property.h
        include/qpb/PropertyBuilders.h
        include/qpb/PropertyGroup.h
        include/qpb/PropertyModel.h
        include/qpb/TypeRegistry.h
        include/qpb/Types.h
        include/qpb/ValidationResult.h
        include/qpb/qpbcore.h
        include/qpb/qpbglobal.h
        "${CMAKE_CURRENT_BINARY_DIR}/include/qpb/qpbversion.h"
        src/core/Property.cpp
        src/core/PropertyBuilders.cpp
        src/core/PropertyGroup.cpp
        src/core/PropertyModel.cpp
        src/core/Property_p.h
        src/core/TypeRegistry.cpp
        src/core/qpbglobal.cpp
)
target_link_libraries(qpb_core PUBLIC Qt6::Core)
add_library(qpb::core ALIAS qpb_core)

# --- qpb::widgets (Qt6::Widgets) ---------------------------------------------
#
# The widgets headers are declarations only until docs/PLAN.md M3.

qpb_add_library(qpb_widgets
    EXPORT_DEFINE QPB_BUILD_WIDGETS_LIB
    OUTPUT_NAME qpbwidgets
    SOURCES
        src/widgets/qpbwidgets.cpp
)
target_link_libraries(qpb_widgets PUBLIC qpb_core Qt6::Widgets)
add_library(qpb::widgets ALIAS qpb_widgets)
