cmake_minimum_required(VERSION 3.12)
project(linkex VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)

# Find libcurl
find_package(CURL REQUIRED)

# Find gumbo-parser using pkg-config
pkg_check_modules(GUMBO REQUIRED gumbo)

# Create executable
add_executable(linkex linkex.cpp)

# Link libraries
target_link_libraries(linkex 
    PRIVATE 
    ${CURL_LIBRARIES}
    ${GUMBO_LIBRARIES}
)

# Include directories
target_include_directories(linkex 
    PRIVATE 
    ${CURL_INCLUDE_DIRS}
    ${GUMBO_INCLUDE_DIRS}
)

# Compiler flags
target_compile_options(linkex 
    PRIVATE 
    ${GUMBO_CFLAGS_OTHER}
)

# Set compiler flags for release and debug
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(linkex PRIVATE -O3 -DNDEBUG -march=native)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(linkex PRIVATE -g -O0 -DDEBUG)
endif()

# Enable all warnings and threading support
target_compile_options(linkex PRIVATE -Wall -Wextra -Wpedantic)

# Link threading library for std::thread
find_package(Threads REQUIRED)
target_link_libraries(linkex PRIVATE Threads::Threads)

# Install the executable
install(TARGETS linkex
    RUNTIME DESTINATION bin
)

# Print configuration info
message(STATUS "linkex configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")

# Create uninstall target
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY)

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()