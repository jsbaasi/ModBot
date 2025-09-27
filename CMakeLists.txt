# Minimum CMake version required, we'll just use the latest version.
cmake_minimum_required(VERSION 3.22)
# Project name, version and description
project(ModBot VERSION 1.0 DESCRIPTION "A test bot")
 
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Create an executable
add_executable(${PROJECT_NAME}
    src/progress.pb.cc
    src/main.cpp
)

# Find our pre-installed packages (using cmake/).
find_package(DPP REQUIRED)
find_package(SQLITE REQUIRED)
find_package(Protobuf REQUIRED)
include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
                         GIT_TAG 1.11.x) # The commit hash for 1.11.x. Replace with the latest from: https://github.com/libcpr/cpr/releases
FetchContent_MakeAvailable(cpr)

# Link the pre-installed DPP package.
target_link_libraries(${PROJECT_NAME}
    PRIVATE cpr::cpr
    protobuf::libprotobuf
    ${DPP_LIBRARIES}
    ${SQLITE_LIBRARIES}
)
 
# Include the DPP directories.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${DPP_INCLUDE_DIR}
    ${SQLITE_INCLUDE_DIR}
)
 
# Set C++ version
set(CMAKE_CXX_STANDARD 20)
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# set(CMAKE_CXX_COMPILER /usr/local/bin/clang++)
# set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_FLAGS "-stdlib=libstdc++ --gcc-install-dir=/usr/local/lib/gcc/x86_64-pc-linux-gnu/15.2.0/ --std=c++20 -rtlib=libgcc -unwindlib=libgcc")
# set(CMAKE_CXX_LINK_EXECUTABLE "/usr/local/bin/clang++ <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET>")
# set(CMAKE_LINKER_TYPE LLD)
# CC=/usr/bin/gcc CXX=/home/jsbaasi/Coding/llvm/llvm-project/build/bin/clang++ cmake -B build set it when configuring instead of setting it in this cmakelists file
