cmake_minimum_required(VERSION 3.22)
project(CLICE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} -w -fno-rtti -O0")

set(LLVM_INSTALL_PATH "${CMAKE_SOURCE_DIR}/deps/llvm/build-install")

set(CMAKE_PREFIX_PATH "${LLVM_INSTALL_PATH}")
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM: ${LLVM_DIR}, Version: ${LLVM_PACKAGE_VERSION}")

# copy clang builtin headers to output directory
if(NOT EXISTS "${CMAKE_BINARY_DIR}/lib/clang")
    set(SOURCE_CLANG_BUILTIN_HEADERS "${LLVM_INSTALL_PATH}/lib/clang")
    set(DEST_CLANG_BUILTIN_HEADERS "${CMAKE_BINARY_DIR}/lib")
    message(STATUS "Copying clang builtin headers to ${DEST_CLANG_BUILTIN_HEADERS}")
    file(MAKE_DIRECTORY ${DEST_CLANG_BUILTIN_HEADERS})
    file(COPY ${SOURCE_CLANG_BUILTIN_HEADERS} DESTINATION ${DEST_CLANG_BUILTIN_HEADERS})
endif()

function(target_clang target)
    target_precompile_headers(${target} PRIVATE 
        "${CMAKE_SOURCE_DIR}/include/AST/Diagnostic.h"
    )
    target_include_directories(${target} PRIVATE 
        ${LLVM_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_libraries(${target} PRIVATE
        LLVMSupport
        clangAST
        clangASTMatchers
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
endfunction()

add_subdirectory(${CMAKE_SOURCE_DIR}/deps/libuv)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/spdlog)

add_executable(clice)

target_clang(clice)

file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*/*.cpp")
target_sources(clice PRIVATE ${SRC_FILES} "${CMAKE_SOURCE_DIR}/src/main.cpp")

target_include_directories(clice PRIVATE
    "${CMAKE_SOURCE_DIR}/deps/libuv/include"
    "${CMAKE_SOURCE_DIR}/deps/spdlog/include"
    "${CMAKE_SOURCE_DIR}/deps/toml/include"
)

target_link_libraries(clice PRIVATE uv spdlog::spdlog)

if(CLICE_ENABLE_TEST)
    add_executable(clice_test)

    # add googletest
    add_subdirectory("${CMAKE_SOURCE_DIR}/deps/googletest")
    target_include_directories(clice_test PRIVATE "${CMAKE_SOURCE_DIR}/deps/googletest/googletest/include")
    target_link_libraries(clice_test PRIVATE gtest_main)

    target_clang(clice_test)
    file(GLOB_RECURSE AST_SRC_FILES "${CMAKE_SOURCE_DIR}/src/AST/*.cpp")
    target_sources(clice_test PRIVATE ${AST_SRC_FILES} "${CMAKE_SOURCE_DIR}/tests/Resolver.cpp")
endif()
