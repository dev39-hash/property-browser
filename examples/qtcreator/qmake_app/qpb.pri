# Include this from the .pro file of an application that uses qpb:
#
#   include(../qpb.pri)
#
# It gives the application the include paths, the QPB_STATIC define, the link
# line and the build dependency on the library. The application's project must
# be a sibling of qpb_lib in the build tree (both are SUBDIRS of one project).

include(qpb_component.pri)

QT += widgets

# One build configuration per build directory, as Qt Creator sets it up (the
# library is then found directly in QPB_LIB_DIR). The library project does the same.
CONFIG -= debug_and_release debug_and_release_target
CONFIG += c++17 utf8_source warn_on

# Qt's headers are external, so warnings inside them (some Qt 6 versions report
# deprecations in their own headers with MSVC) stay quiet.
msvc: QMAKE_CXXFLAGS += -external:I$$[QT_INSTALL_HEADERS] -external:W0

QPB_LIB_DIR = $$clean_path($$OUT_PWD/../qpb_lib)

# Linked statically: the export macros of qpbglobal.h expand to nothing.
DEFINES += QPB_STATIC
# The second path holds the generated qpb/qpbversion.h.
INCLUDEPATH += $$QPB_COMPONENT/include $$QPB_LIB_DIR/include

LIBS += -L$$QPB_LIB_DIR -lqpb
msvc: PRE_TARGETDEPS += $$QPB_LIB_DIR/qpb.lib
else: PRE_TARGETDEPS += $$QPB_LIB_DIR/libqpb.a
