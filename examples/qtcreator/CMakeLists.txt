# All qpb examples as one CMake project for Qt Creator.
#
# Open this file in Qt Creator (File > Open File or Project...), pick a Qt 6.5+
# kit, build, and choose the example to start in the run target selector. It
# builds qpb from ../../qpb exactly as a consuming project does and reuses
# ../CMakeLists.txt, so a new example added there shows up here as well. Only
# the library and the examples are built, not the tests.
#
# From a terminal:
#   cmake -S examples/qtcreator -B build-examples -DCMAKE_PREFIX_PATH=<Qt prefix>
#   cmake --build build-examples

cmake_minimum_required(VERSION 3.21)

project(qpb_examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

# Both directories are outside this one, so each needs a binary directory.
add_subdirectory(../../qpb qpb)
add_subdirectory(.. examples)
