cmake_minimum_required(VERSION 3.10)
project(DSMClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# Find nlohmann_json
find_package(nlohmann_json REQUIRED)

# Find websocketpp (header-only library)
find_path(WEBSOCKETPP_INCLUDE_DIR websocketpp/config/asio_no_tls_client.hpp)
if(NOT WEBSOCKETPP_INCLUDE_DIR)
    message(FATAL_ERROR "websocketpp not found. Please install websocketpp development package.")
endif()

# Find mavlink from git https://github.com/mavlink/c_library_v2
include(FetchContent)

FetchContent_Declare(
    mavlink
    GIT_REPOSITORY https://github.com/mavlink/c_library_v2.git
    GIT_TAG master
)

FetchContent_MakeAvailable(mavlink)

message(STATUS "mavlink_SOURCE_DIR: ${mavlink_SOURCE_DIR}")

# Add executable
add_executable(client client.cpp)

# Include directories
target_include_directories(client PRIVATE ${WEBSOCKETPP_INCLUDE_DIR} ${mavlink_SOURCE_DIR})

# Link libraries
target_link_libraries(client
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# Compiler definitions for websocketpp
target_compile_definitions(client PRIVATE
    ASIO_STANDALONE
    _WEBSOCKETPP_CPP11_STL_
)
