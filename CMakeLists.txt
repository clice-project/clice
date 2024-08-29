cmake_minimum_required(VERSION 3.22)
project(CLICE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} -w")

set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/deps/llvm/build-install")
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM: ${LLVM_DIR}, Version: ${LLVM_PACKAGE_VERSION}")
add_definitions(${LLVM_DEFINITIONS})


add_subdirectory(${CMAKE_SOURCE_DIR}/deps/libuv)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/spdlog)

add_executable(clice)

target_sources(clice PRIVATE
    src/main.cpp
)

target_include_directories(clice PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/deps/libuv/include
    ${CMAKE_SOURCE_DIR}/deps/spdlog/include
)

set(LLVM_LIBS
    LLVMCore
    LLVMSupport
)

set(CLANG_LIBS
    clangAST
    clangBasic
    clangDependencyScanning
    clangDriver
    clangFormat
    clangFrontend
    clangIndex
    clangLex
    clangSema
    clangSerialization
    clangTooling
    clangToolingCore
    clangToolingInclusions
    clangToolingInclusionsStdlib
    clangToolingSyntax
)

target_link_libraries(clice PRIVATE
    ${LLVM_LIBS}
    ${CLANG_LIBS}
    uv
    spdlog::spdlog
)
