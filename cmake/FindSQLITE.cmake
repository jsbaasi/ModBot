find_path(SQLITE_INCLUDE_DIR NAMES sqlite3.h HINTS ${SQLITE_ROOT_DIR})
 
find_library(SQLITE_LIBRARIES NAMES "libsqlite3.a" HINTS ${SQLITE_ROOT_DIR})
 
include(FindPackageHandleStandardArgs)
 
find_package_handle_standard_args(SQLITE DEFAULT_MSG SQLITE_LIBRARIES SQLITE_INCLUDE_DIR)