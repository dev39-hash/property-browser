# All qpb examples as one qmake project for Qt Creator.
#
# Open this file in Qt Creator (File > Open File or Project...), pick a Qt 6.5+
# kit, build, and choose the example to start in the run target selector.
#
# qpb itself is a CMake component (docs/SPEC.md section 6); CMakeLists.txt next
# to this file is the reference way to build the examples. This project builds
# the same sources from ../../qpb as one static library (qmake/qpb) for those
# who work with qmake. qmake builds one executable per .pro file, so every
# example has a small .pro file of its own under qmake/<example>/; the test
# qtcreator_examples_listed checks that none is missing.
#
# From a terminal (nmake or jom instead of make with MSVC):
#   qmake examples/qtcreator/qpb_examples.pro && make

TEMPLATE = subdirs

# Qt 6.5 or newer, or Qt 5.15 (since 1.7).
!versionAtLeast(QT_VERSION, 5.15.0)|equals(QT_MAJOR_VERSION, 6):!versionAtLeast(QT_VERSION, 6.5.0): \
    error("qpb requires Qt 6.5 or newer, or Qt 5.15; found $$QT_VERSION")

QPB_EXAMPLES = quickstart custom_type inspector settings_dialog plugin_config form_view object_editor custom_theme

SUBDIRS = qpb
qpb.subdir = qmake/qpb

for(example, QPB_EXAMPLES) {
    SUBDIRS += $$example
    $${example}.subdir = qmake/$$example
    $${example}.depends = qpb
}

OTHER_FILES += CMakeLists.txt
