# Minimum CMake version required, we'll just use the latest version.
cmake_minimum_required(VERSION 3.22)
# Project name, version and description
project(ModBot VERSION 1.0 DESCRIPTION "A test bot")
 
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
 
# Create an executable
add_executable(${PROJECT_NAME}
    src/main.cpp
)
 
# Find our pre-installed packages (using cmake/).
find_package(DPP REQUIRED)
find_package(SQLITE REQUIRED)

# Link the pre-installed DPP package.
target_link_libraries(${PROJECT_NAME} 
    ${DPP_LIBRARIES}
    ${SQLITE_LIBRARIES}
)
 
# Include the DPP directories.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${DPP_INCLUDE_DIR}
    ${SQLITE_INCLUDE_DIR}
)
 
# Set C++ version
# set_target_properties(${PROJECT_NAME} PROPERTIES
#     CXX_STANDARD 23
#     CXX_STANDARD_REQUIRED ON
# )

# set(CMAKE_CXX_COMPILER /usr/local/bin/clang++)
set(CMAKE_CXX_FLAGS "-std=c++20 -stdlib=libc++ -fuse-ld=lld")
# set(CMAKE_CXX_LINK_EXECUTABLE "/usr/local/bin/clang++ <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET>")
# set(CMAKE_LINKER_TYPE LLD)
