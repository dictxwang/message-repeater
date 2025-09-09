SET(BUILD_PATH ${CMAKE_CURRENT_LIST_DIR}/build)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CUSTOM_LIB_DIR "/usr/lib/x86_64-linux-gnu")
    set(CUSTOM_INCLUDE_DIR "/usr/include/x86_64-linux-gnu")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CUSTOM_LIB_DIR "/usr/local/lib")
    set(CUSTOM_INCLUDE_DIR "/usr/local/include")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CUSTOM_LIB_DIR "/usr/local/lib")
    set(CUSTOM_INCLUDE_DIR "/usr/local/include")
else()
    message(WARNING "Unknown platform: ${CMAKE_SYSTEM_NAME}")
endif()

link_directories(
    # ${CMAKE_CURRENT_SOURCE_DIR}
    ${CUSTOM_LIB_DIR}
)

# # Find Curl
# find_package(CURL REQUIRED)
# # Find OpenSSL
# find_package(OpenSSL REQUIRED)

FIND_LIBRARY(_LIB_SPDLOG
    NAMES spdlog
    HINTS ${BUILD_PATH}/spdlog/lib/
    NO_DEFAULT_PATH
    NO_SYSTEM_ENVIRONMENT_PATH
    NO_CMAKE_SYSTEM_PATH
)
