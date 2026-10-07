# Development build of the property-browser repository.
#
# The library itself lives in qpb/ (the distributable component folder). This
# file only wires it together with the tests and examples, using qpb exactly as
# a consuming project would.

cmake_minimum_required(VERSION 3.21)

project(property_browser LANGUAGES CXX)

option(QPB_BUILD_TESTS "Build the qpb test suite" ON)
option(QPB_BUILD_EXAMPLES "Build the qpb examples" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 6, or Qt 5.15 when there is no Qt 6 (the minimum versions are checked by qpb).
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

add_subdirectory(qpb)

if(QPB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(QPB_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
