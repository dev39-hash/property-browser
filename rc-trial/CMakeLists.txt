# RC trial application (docs/PLAN.md RC.1).
#
# A standalone application that integrates qpb exactly as a consuming project
# does: the released component folder is extracted to components/qpb/ (see
# run_trial.sh). Nothing from the qpb development repository is used.

cmake_minimum_required(VERSION 3.21)

project(qpb_rc_trial LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Test)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/components/qpb/CMakeLists.txt")
    message(FATAL_ERROR "Extract a qpb release to components/qpb/ first (see run_trial.sh)")
endif()
add_subdirectory(components/qpb)

add_library(trial_app STATIC
    src/ColorType.cpp src/ColorType.h
    src/InspectorPage.cpp src/InspectorPage.h
    src/SettingsPage.cpp src/SettingsPage.h
    src/PluginsPage.cpp src/PluginsPage.h
    src/MainWindow.cpp src/MainWindow.h
)
target_include_directories(trial_app PUBLIC src)
target_link_libraries(trial_app PUBLIC qpb::widgets)

add_executable(qpb_trial src/main.cpp)
target_link_libraries(qpb_trial PRIVATE trial_app)

enable_testing()
add_executable(tst_trial tests/tst_trial.cpp)
target_link_libraries(tst_trial PRIVATE trial_app Qt6::Test)
add_test(NAME tst_trial COMMAND tst_trial)
set_tests_properties(tst_trial PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
