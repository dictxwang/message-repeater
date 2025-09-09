cmake_minimum_required(VERSION 3.10)

project(repeater_samples VERSION 0.1.0 LANGUAGES CXX)

set(PROJECT_NAME "repeater_samples" VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
# set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_TYPE "Debug")


if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CUSTOM_LIB_DIR "/usr/lib/x86_64-linux-gnu")
    set(CUSTOM_INCLUDE_DIR "/usr/lib/x86_64-linux-gnu")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CUSTOM_LIB_DIR "/usr/local/lib")
    set(CUSTOM_INCLUDE_DIR "/usr/local/include")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CUSTOM_LIB_DIR "/usr/local/lib")
    set(CUSTOM_INCLUDE_DIR "/usr/local/include")
else()
    message(WARNING "Unknown platform: ${CMAKE_SYSTEM_NAME}")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    include
    ${CUSTOM_INCLUDE_DIR}
    ../src/
)

link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CUSTOM_LIB_DIR}
    ../build/
)


# json
FILE(GLOB_RECURSE LIB_JSON_SRC RELATIVE ${CMAKE_SOURCE_DIR}
    "../src/json/*")
add_library(json ${LIB_JSON_SRC})

# util
FILE(GLOB_RECURSE LIB_UTIL_SRC RELATIVE ${CMAKE_SOURCE_DIR}
    "../src/util/*")
add_library(util ${LIB_UTIL_SRC})

add_executable("sample_subscriber" "src/sample_subscriber.cpp")
target_link_libraries("sample_subscriber"
    json
    util
)

