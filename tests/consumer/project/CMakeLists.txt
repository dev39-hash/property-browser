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

if(CONSUMER_FIND_QT_FIRST)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
endif()

# --- the two lines a consumer needs -------------------------------------------
add_subdirectory(components/qpb)
add_executable(qpb_consumer main.cpp)
target_link_libraries(qpb_consumer PRIVATE qpb::widgets)
# ------------------------------------------------------------------------------

target_compile_definitions(qpb_consumer PRIVATE
    CONSUMER_CXX_STANDARD=${CONSUMER_CXX_STANDARD}
)

# qpb's public headers must compile warning-free under strict consumer flags.
if(MSVC)
    target_compile_options(qpb_consumer PRIVATE /W4 /WX /permissive- /Zc:__cplusplus)
else()
    target_compile_options(qpb_consumer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Fixed location for the test driver, independent of the generator.
set_target_properties(qpb_consumer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>"
)
