# Minimum CMake version required, we'll just use the latest version.
cmake_minimum_required(VERSION 3.22)
# Project name, version and description
project(ModBot VERSION 1.0 DESCRIPTION "A test bot")
include(FetchContent)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Create an executable
add_executable(${PROJECT_NAME}
    src/commands/command_listener.cpp
    src/league/league.cpp
    src/pubg/pubg.cpp
    src/sql.cpp
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
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/commands
    ${CMAKE_SOURCE_DIR}/src/league
    ${CMAKE_SOURCE_DIR}/src/pubg
    ${DPP_INCLUDE_DIR}
    ${SQLITE_INCLUDE_DIR}
)
 
# Set C++ version
set(CMAKE_CXX_STANDARD 20)
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# set(CMAKE_CXX_FLAGS "-stdlib=libstdc++ --gcc-install-dir=/usr/local/lib/gcc/x86_64-pc-linux-gnu/15.2.0/ --std=c++20 -rtlib=libgcc -unwindlib=libgcc")
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
