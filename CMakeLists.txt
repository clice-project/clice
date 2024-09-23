cmake_minimum_required(VERSION 3.22)
project(CLICE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} -fno-rtti -g -O0")

set(LLVM_INSTALL_PATH "${CMAKE_SOURCE_DIR}/deps/llvm/build-install")

set(CMAKE_PREFIX_PATH "${LLVM_INSTALL_PATH}")
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM: ${LLVM_DIR}, Version: ${LLVM_PACKAGE_VERSION}")

# copy clang builtin headers to output directory
if(NOT EXISTS "${CMAKE_BINARY_DIR}/lib/clang")
    set(CLANG_BUILTIN_HEADERS_SOURCE "${LLVM_INSTALL_PATH}/lib/clang")
    set(CLANG_BUILTIN_HEADERS_OUTPUT "${CMAKE_BINARY_DIR}/lib")
    message(STATUS "Copying clang builtin headers to ${CLANG_BUILTIN_HEADERS_OUTPUT}")
    file(MAKE_DIRECTORY ${CLANG_BUILTIN_HEADERS_OUTPUT})
    file(COPY ${CLANG_BUILTIN_HEADERS_SOURCE} DESTINATION ${CLANG_BUILTIN_HEADERS_OUTPUT})
endif()

set(CLICE_CLANG_EXTRA_OUTPUT "${CMAKE_SOURCE_DIR}/deps/clang-extra/include/clang-extra")
if(NOT EXISTS ${CLICE_CLANG_EXTRA_OUTPUT})
    set(CLICE_CLANG_EXTRA_SOURCE "${CMAKE_SOURCE_DIR}/deps/llvm/clang/lib/Sema")
    set(CLICE_CLANG_EXTRA_SOURCE_FILES 
        ${CLICE_CLANG_EXTRA_SOURCE}/CoroutineStmtBuilder.h
        ${CLICE_CLANG_EXTRA_SOURCE}/TypeLocBuilder.h
        ${CLICE_CLANG_EXTRA_SOURCE}/TreeTransform.h
    )
    message(STATUS "Copying clang extra headers to ${CLICE_CLANG_EXTRA_SOURCE_FILES}")
    file(MAKE_DIRECTORY ${CLICE_CLANG_EXTRA_OUTPUT})
    file(COPY ${CLICE_CLANG_EXTRA_SOURCE_FILES} DESTINATION ${CLICE_CLANG_EXTRA_OUTPUT})
endif()

function(target_clang target)
    target_precompile_headers(${target} PRIVATE 
        "${CMAKE_SOURCE_DIR}/include/AST/Diagnostic.h"
    )
    target_include_directories(${target} PRIVATE 
        ${LLVM_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/deps/clang-extra/include
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
    file(GLOB_RECURSE AST_SRC_FILES 
        "${CMAKE_SOURCE_DIR}/src/AST/*.cpp" 
        "${CMAKE_SOURCE_DIR}/src/Index/*.cpp" 
        "${CMAKE_SOURCE_DIR}/src/Feature/*.cpp" 
        "${CMAKE_SOURCE_DIR}/src/Support/*.cpp"
    )
    file(GLOB_RECURSE TEST_SRC_FILES "${CMAKE_SOURCE_DIR}/tests/*/*.cpp")
    target_sources(clice_test PRIVATE ${AST_SRC_FILES} ${TEST_SRC_FILES})
endif()
