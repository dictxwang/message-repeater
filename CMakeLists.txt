cmake_minimum_required(VERSION 3.10)

project(message_repeater VERSION 0.1.0 LANGUAGES CXX)

set(STARTER_TEST "test_starter")
set(STARTER_REPEATER "repeater_starter")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
option(USE_DEBUG_MODE "Use Debug Mode" OFF)
if (USE_DEBUG_MODE)
    # cmake -DUSE_DEBUG_MODE=ON ..
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(CMAKE_BUILD_TYPE "Debug")
    add_compile_definitions(OPEN_STD_DEBUG_LOG)
else()
    set(CMAKE_BUILD_TYPE "Release")
endif()

add_compile_options(-fPIC)
add_compile_options(-O2)

include(dependencies.cmake)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    include
    src
    3rdparty/spdlog/include
    ${CUSTOM_INCLUDE_DIR}
)

# json
FILE(GLOB_RECURSE LIB_JSON_SRC RELATIVE ${CMAKE_SOURCE_DIR}
    "src/json/*")
add_library(json ${LIB_JSON_SRC})

# config
FILE(GLOB_RECURSE LIB_CONFIG_SRC RELATIVE ${CMAKE_SOURCE_DIR}
    "src/config/*")
add_library(config ${LIB_CONFIG_SRC})

# util
FILE(GLOB_RECURSE LIB_UTIL_SRC RELATIVE ${CMAKE_SOURCE_DIR}
    "src/util/*")
add_library(util ${LIB_UTIL_SRC})

# tgbot
FILE(GLOB_RECURSE LIB_TGBOT_SRC RELATIVE ${CMAKE_SOURCE_DIR}
    "src/tgbot/*")
add_library(tgbot ${LIB_TGBOT_SRC})
target_link_libraries(tgbot
    CURL::libcurl
    OpenSSL::SSL OpenSSL::Crypto
)

add_executable(${STARTER_TEST} "src/test_starter.cpp")
target_link_libraries(${STARTER_TEST}

    ${_LIB_SPDLOG}
    json
    config
    CURL::libcurl
    OpenSSL::SSL OpenSSL::Crypto
)

# connection
FILE(GLOB_RECURSE LIB_CONNECTION_SRC RELATIVE ${CMAKE_SOURCE_DIR}
    "src/connection/*")
add_library(connection ${LIB_CONNECTION_SRC})
# publisher
FILE(GLOB_RECURSE LIB_PUBLISHER_SRC RELATIVE ${CMAKE_SOURCE_DIR}
    "src/publisher/*")
add_library(publisher ${LIB_PUBLISHER_SRC})
# subscriber
FILE(GLOB_RECURSE LIB_SUBSCRIBER_SRC RELATIVE ${CMAKE_SOURCE_DIR}
    "src/subscriber/*")
add_library(subscriber ${LIB_SUBSCRIBER_SRC})
# layer
FILE(GLOB_RECURSE LIB_LAYER_SRC RELATIVE ${CMAKE_SOURCE_DIR}
    "src/layer/*")
add_library(layer ${LIB_LAYER_SRC})
# combiner
FILE(GLOB_RECURSE LIB_COMBINER_SRC RELATIVE ${CMAKE_SOURCE_DIR}
    "src/combiner/*")
add_library(combiner ${LIB_COMBINER_SRC})

add_executable(${STARTER_REPEATER} "src/repeater_starter.cpp")
target_link_libraries(${STARTER_REPEATER}
    ${_LIB_SPDLOG}
    json
    config
    connection
    publisher
    subscriber
    layer
    combiner
    util
    tgbot
)
