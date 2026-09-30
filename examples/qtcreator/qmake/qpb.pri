# Settings shared by the qpb library and the examples in the qmake build
# (../qpb_examples.pro).

QPB_ROOT = $$clean_path($$PWD/../../../qpb)
QPB_EXAMPLES_DIR = $$clean_path($$PWD/../..)
# The subdirs project builds the library in qmake/qpb, next to the examples.
QPB_BUILD_DIR = $$clean_path($$OUT_PWD/../qpb)

# One build configuration per build directory, as Qt Creator sets it up; the
# examples then find the library directly in QPB_BUILD_DIR.
CONFIG -= debug_and_release debug_and_release_target
CONFIG += c++17 utf8_source warn_on

# Qt's headers are external, as in CMake builds: warnings inside them (e.g. the
# deprecations Qt 6.11 reports in its own headers with MSVC) stay quiet. GCC and
# Clang already get them with -isystem.
msvc: QMAKE_CXXFLAGS += -external:I$$[QT_INSTALL_HEADERS] -external:W0

# Linked statically: the export macros of qpbglobal.h expand to nothing.
DEFINES += QPB_STATIC

INCLUDEPATH += $$QPB_ROOT/include $$QPB_BUILD_DIR/include
