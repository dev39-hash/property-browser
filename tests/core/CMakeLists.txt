qpb_add_test(tst_version
    SOURCES tst_version.cpp
    LIBRARIES qpb::core
)
target_compile_definitions(tst_version PRIVATE
    QPB_TEST_VERSION_FILE="${PROJECT_SOURCE_DIR}/qpb/VERSION"
)
