# Development build of the property-browser repository.
#
# The library itself lives in qpb/ (the distributable component folder). This
# file only wires it together with the tests and examples, using qpb exactly as
# a consuming project would.

cmake_minimum_required(VERSION 3.21)

project(property_browser LANGUAGES CXX)

option(QPB_BUILD_TESTS "Build the qpb test suite" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

add_subdirectory(qpb)

if(QPB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
