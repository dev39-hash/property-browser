# qpb::core and qpb::widgets as one static library for the examples. The project
# is the one of the qmake application example (../../qmake_app/qpb_lib), pointed at
# this repository's qpb/ folder, so there is one copy of it.

QPB_COMPONENT = $$clean_path($$PWD/../../../../qpb)
include(../../qmake_app/qpb_lib/qpb_lib.pro)
