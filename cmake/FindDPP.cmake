# set(DPP_ROOT_DIR "/usr/local/lib")
find_path(DPP_INCLUDE_DIR NAMES dpp.h HINTS ${DPP_ROOT_DIR} PATHS "/usr/local/include/dpp")
 
find_library(DPP_LIBRARIES NAMES libdpp.so dpp "libdpp.a" HINTS ${DPP_ROOT_DIR} PATHS "/usr/local/lib/")
 
include(FindPackageHandleStandardArgs)
 
find_package_handle_standard_args(DPP DEFAULT_MSG DPP_LIBRARIES DPP_INCLUDE_DIR)