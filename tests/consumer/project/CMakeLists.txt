# A minimal application that uses qpb the way consuming projects are told to:
# a copied component folder plus two lines of CMake (docs/SPEC.md §6.2).
# Its own settings are deliberately strict so problems in qpb's public headers
# or CMake surface here.

cmake_minimum_required(VERSION 3.21)

project(qpb_consumer LANGUAGES CXX)

set(CONSUMER_CXX_STANDARD 17 CACHE STRING "C++ standard used by the consumer")
option(CONSUMER_FIND_QT_FIRST "Find Qt before adding qpb" ON)

set(CMAKE_CXX_STANDARD ${CONSUMER_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# One output directory for executables and DLLs, independent of the generator,
# so a shared qpb is found at run time on Windows too.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")

if(CONSUMER_FIND_QT_FIRST)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
endif()

# Snapshot of the host's settings that qpb must not change (docs/SPEC.md §6.3).
set(_watched_variables
    CMAKE_CXX_STANDARD CMAKE_CXX_STANDARD_REQUIRED CMAKE_CXX_EXTENSIONS CMAKE_CXX_FLAGS
    CMAKE_CXX_FLAGS_DEBUG CMAKE_AUTOMOC CMAKE_AUTOUIC CMAKE_AUTORCC BUILD_SHARED_LIBS
    CMAKE_POSITION_INDEPENDENT_CODE CMAKE_CXX_VISIBILITY_PRESET CMAKE_RUNTIME_OUTPUT_DIRECTORY
    CMAKE_LIBRARY_OUTPUT_DIRECTORY CMAKE_ARCHIVE_OUTPUT_DIRECTORY CMAKE_MODULE_PATH)
foreach(variable IN LISTS _watched_variables)
    set(_before_${variable} "${${variable}}")
endforeach()
get_cmake_property(_cache_before CACHE_VARIABLES)

# --- the two lines a consumer needs -------------------------------------------
add_subdirectory(components/qpb)
add_executable(qpb_consumer main.cpp)
target_link_libraries(qpb_consumer PRIVATE qpb::widgets)
# ------------------------------------------------------------------------------

foreach(variable IN LISTS _watched_variables)
    if(NOT "${${variable}}" STREQUAL "${_before_${variable}}")
        message(FATAL_ERROR "qpb changed the host's ${variable}")
    endif()
endforeach()
# New cache entries must belong to qpb (or to finding Qt when the host
# left finding Qt to qpb).
get_cmake_property(_cache_after CACHE_VARIABLES)
list(REMOVE_ITEM _cache_after ${_cache_before})
foreach(variable IN LISTS _cache_after)
    if(NOT variable MATCHES "^(QPB_|qpb_)" AND CONSUMER_FIND_QT_FIRST)
        message(FATAL_ERROR "qpb added the cache entry ${variable}")
    endif()
endforeach()

target_compile_definitions(qpb_consumer PRIVATE
    CONSUMER_CXX_STANDARD=${CONSUMER_CXX_STANDARD}
)

# qpb's public headers must compile warning-free under strict consumer flags.
if(MSVC)
    target_compile_options(qpb_consumer PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
else()
    target_compile_options(qpb_consumer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
