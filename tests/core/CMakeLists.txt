qpb_add_test(tst_version
    SOURCES tst_version.cpp
    LIBRARIES qpb::core
)
target_compile_definitions(tst_version PRIVATE
    QPB_TEST_VERSION_FILE="${PROJECT_SOURCE_DIR}/qpb/VERSION"
)

qpb_add_test(tst_property
    SOURCES tst_property.cpp
    LIBRARIES qpb::core
)

qpb_add_test(tst_typeregistry
    SOURCES tst_typeregistry.cpp
    LIBRARIES qpb::core
)

qpb_add_test(tst_propertymodel
    SOURCES tst_propertymodel.cpp
    LIBRARIES qpb::core
)

qpb_add_test(tst_propertyfilterproxymodel
    SOURCES tst_propertyfilterproxymodel.cpp
    LIBRARIES qpb::core
)

qpb_add_test(tst_serialization
    SOURCES tst_serialization.cpp
    LIBRARIES qpb::core
)

qpb_add_test(tst_qobjectpropertysource
    SOURCES tst_qobjectpropertysource.cpp
    LIBRARIES qpb::core
)

qpb_add_test(tst_conditions
    SOURCES tst_conditions.cpp
    LIBRARIES qpb::core
)
