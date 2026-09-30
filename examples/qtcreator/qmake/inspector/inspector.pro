EXAMPLE = inspector
include(../example.pri)

# Uses the QColor type of the custom_type example. That folder has a main.cpp
# too, so nmake's per-folder batch rules cannot be used (qmake would say so).
SOURCES += $$QPB_EXAMPLES_DIR/custom_type/ColorType.cpp
HEADERS += $$QPB_EXAMPLES_DIR/custom_type/ColorType.h
CONFIG += no_batch
