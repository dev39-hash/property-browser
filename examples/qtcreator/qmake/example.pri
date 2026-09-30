# One example application of the qmake build (../qpb_examples.pro). The .pro
# file including this sets EXAMPLE to the example's folder under examples/.

isEmpty(EXAMPLE): error("Set EXAMPLE before including example.pri")

include(qpb.pri)

TEMPLATE = app
TARGET = $$EXAMPLE
QT += widgets

QPB_EXAMPLE_DIR = $$QPB_EXAMPLES_DIR/$$EXAMPLE
SOURCES += $$files($$QPB_EXAMPLE_DIR/*.cpp)
HEADERS += $$files($$QPB_EXAMPLE_DIR/*.h)
RESOURCES += $$files($$QPB_EXAMPLE_DIR/*.qrc)
OTHER_FILES += $$files($$QPB_EXAMPLE_DIR/*.qss)

LIBS += -L$$QPB_BUILD_DIR -lqpb
msvc: PRE_TARGETDEPS += $$QPB_BUILD_DIR/qpb.lib
else: PRE_TARGETDEPS += $$QPB_BUILD_DIR/libqpb.a
