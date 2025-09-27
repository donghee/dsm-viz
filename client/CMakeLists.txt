cmake_minimum_required(VERSION 3.10)
project(DSMClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

# Sanitizer options
option(USE_ASAN "Enable Address Sanitizer" ON)
option(USE_UBSAN "Enable Undefined Behavior Sanitizer" ON)
option(USE_TSAN "Enable Thread Sanitizer" OFF)

# Address Sanitizer
if(USE_ASAN)
    message(STATUS "Address Sanitizer enabled")
    set(SANITIZER_FLAGS "-fsanitize=address")
    set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fno-omit-frame-pointer")
    set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -g")
    set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -O1")
    set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fno-optimize-sibling-calls")

    # Enable leak detection (part of AddressSanitizer)
    set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize-address-use-after-scope")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

    # Set runtime options
    set(ENV{ASAN_OPTIONS} "detect_leaks=1:abort_on_error=1:check_initialization_order=1:strict_init_order=1")
endif()

# Undefined Behavior Sanitizer
if(USE_UBSAN)
    message(STATUS "Undefined Behavior Sanitizer enabled")
    set(UBSAN_FLAGS "-fsanitize=undefined")
    set(UBSAN_FLAGS "${UBSAN_FLAGS} -fno-omit-frame-pointer")
    set(UBSAN_FLAGS "${UBSAN_FLAGS} -g")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${UBSAN_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${UBSAN_FLAGS}")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=undefined")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")

    # Set runtime options
    set(ENV{UBSAN_OPTIONS} "print_stacktrace=1:abort_on_error=1")
endif()

# Thread Sanitizer (mutually exclusive with Address Sanitizer)
if(USE_TSAN)
    if(USE_ASAN)
        message(FATAL_ERROR "Thread Sanitizer and Address Sanitizer cannot be used together")
    endif()

    message(STATUS "Thread Sanitizer enabled")
    set(TSAN_FLAGS "-fsanitize=thread")
    set(TSAN_FLAGS "${TSAN_FLAGS} -fno-omit-frame-pointer")
    set(TSAN_FLAGS "${TSAN_FLAGS} -g")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TSAN_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TSAN_FLAGS}")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=thread")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")

    # Set runtime options
    set(ENV{TSAN_OPTIONS} "halt_on_error=1:abort_on_error=1")
endif()

# Warning for Debug builds
if(USE_ASAN OR USE_UBSAN OR USE_TSAN)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(WARNING "Sanitizers are recommended to be used with Debug build type for better debugging experience")
    endif()
endif()

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

target_compile_definitions(client PRIVATE MAVLINK_USE_MESSAGE_INFO)

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
