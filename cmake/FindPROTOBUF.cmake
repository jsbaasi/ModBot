# /usr/lib/x86_64-linux-gnu/libprotobuf.a
# /usr/include/google/protobuf/

# find_library(PROTOBUF_LIBRARIES NAMES libprotobuf.a PATHS "/usr/lib/x86_64-linux-gnu/")
find_library(PROTOBUF_LIBRARIES NAMES libprotobuf.a)

find_path(PROTOBUF_INCLUDE_DIR NAMES protobuf PATHS "/usr/include/google/protobuf/")

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(PROTOBUF DEFAULT_MSG PROTOBUF_LIBRARIES PROTOBUF_INCLUDE_DIR)