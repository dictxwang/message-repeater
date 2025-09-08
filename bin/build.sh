#!/usr/bin/env bash

MAKE_JOB_NUM=2
BUILD_SHARED_LIBS=OFF
OS_UNAME=$(uname)

PROJECT_ROOT=$(cd `dirname $0`; cd ..; pwd;)
cd ${PROJECT_ROOT}

function prepare() {

    if [ ! -d build ];
    then
        mkdir build
        mkdir build/lib
    fi
}

function compile_one_third_library() {
    name=$1
    source_path=$2
    build_path=$3
    other_args=$4

    prefix_path=${build_path}/${name}
    build_path=${prefix_path}/src/${name}-build
    cmake ${source_path} -B ${build_path} -DCMAKE_INSTALL_PREFIX=${prefix_path} -DCMAKE_INSTALL_LIBDIR=${prefix_path}/lib -DCMAKE_CXX_FLAGS_RELEASE=-O2 ${other_args}
    cd ${build_path}; make -j ${MAKE_JOB_NUM}; make install; cd -
}

function build_all_third_libraries() {

    ORIGINAL_BUILD_PATH=$1
    # compile_one_third_library binancecpp ../3rdparty/binancecpp ${ORIGINAL_BUILD_PATH}
    compile_one_third_library spdlog ../3rdparty/spdlog ${ORIGINAL_BUILD_PATH}
    # compile_one_third_library zeromq ../3rdparty/zeromq ${ORIGINAL_BUILD_PATH} "-D WITH_PERF_TOOL=OFF -D ZMQ_BUILD_TESTS=OFF -D ENABLE_CPACK=OFF -D CMAKE_BUILD_TYPE=Release"
}

function link_process_files() {
    cd ${PROJECT_ROOT}
    rm -f test_starter
    rm -f repeater_starter

    if [[ -e build/test_starter ]]; then
        ln -s build/test_starter test_starter
    fi
    if [[ -e build/repeater_starter ]]; then
        ln -s build/repeater_starter repeater_starter
    fi
}

BUILD_TYPE="all"
BUILD_DEBUG=""

if [ $# -ge 1 ];
then
    BUILD_TYPE=$1
else
    echo "Usage: ./build.sh [all|third|main]"
    exit 0
fi

if [ $# -gt 1 ];
then
    BUILD_DEBUG="ON"
fi

prepare
cd build
build_path=$(pwd)

BUILD_SHARED_LIBS=OFF # maybe link error if no this setting

if [[ ${BUILD_TYPE} == "third" || ${BUILD_TYPE} == "all" ]]; then
    build_all_third_libraries ${build_path}
fi

if [[ ${BUILD_TYPE} == "all" || ${BUILD_TYPE} == "main" ]]; then
    if [[ ${BUILD_DEBUG} == "ON" ]]; then
        cmake -DUSE_DEBUG_MODE=ON ..
    else
        cmake ..
    fi
    make

    link_process_files
fi
