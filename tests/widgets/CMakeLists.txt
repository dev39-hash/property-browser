# Widget tests may include the component's private headers (qpb/src) to reach
# test hooks such as PathEdit::setDialogProviderForTesting().

qpb_add_test(tst_editorfactory
    SOURCES tst_editorfactory.cpp
    LIBRARIES qpb::widgets
)
target_include_directories(tst_editorfactory PRIVATE "${PROJECT_SOURCE_DIR}/qpb/src")

qpb_add_test(tst_propertytreeview
    SOURCES tst_propertytreeview.cpp
    LIBRARIES qpb::widgets
)
target_include_directories(tst_propertytreeview PRIVATE "${PROJECT_SOURCE_DIR}/qpb/src")
