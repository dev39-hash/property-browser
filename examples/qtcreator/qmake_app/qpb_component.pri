# The only setting of this project: where the qpb component folder is, the
# folder that holds qpb's CMakeLists.txt, VERSION, include/ and src/.
#
# In an application that keeps qpb as a component and replaces the folder as a
# whole on updates, this is a path inside the application, for example
#   QPB_COMPONENT = $$clean_path($$PWD/components/qpb)
# Here it is the qpb/ folder of this repository.

QPB_COMPONENT = $$clean_path($$PWD/../../../qpb)
