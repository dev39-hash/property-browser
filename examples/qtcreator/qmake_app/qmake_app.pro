# A qmake application that builds the qpb component folder unchanged.
#
# qpb itself is a CMake component; it ships no qmake file and qmake is not a
# supported way to integrate it. This project is an example to copy for an
# application that keeps qpb as a component folder (components/qpb) and replaces
# that folder as a whole on updates:
#
#   qpb_component.pri  the folder of the component, the only setting
#   qpb_lib/           the library: builds the component folder as one static
#                      library, nothing in the folder is listed by hand
#   qpb.pri            what the application includes to use the library
#   app/               a small application
#
# From a terminal, in a build folder (nmake or jom instead of make with MSVC):
#   qmake <path to>/qmake_app.pro && make

TEMPLATE = subdirs

# Qt 6.5 or newer, or Qt 5.15.
!versionAtLeast(QT_VERSION, 5.15.0)|equals(QT_MAJOR_VERSION, 6):!versionAtLeast(QT_VERSION, 6.5.0): \
    error("qpb requires Qt 6.5 or newer, or Qt 5.15; found $$QT_VERSION")

SUBDIRS = qpb_lib app
app.depends = qpb_lib

OTHER_FILES += qpb.pri qpb_component.pri
