cmake_minimum_required(VERSION 3.22)
project(CLICE_PROJECT)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

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

# build clice core part as library
file(GLOB_RECURSE CLICE_CORE_SOURCES
    "${CMAKE_SOURCE_DIR}/src/Basic/*.cpp" 
    "${CMAKE_SOURCE_DIR}/src/Compiler/*.cpp" 
    "${CMAKE_SOURCE_DIR}/src/Index/*.cpp" 
    "${CMAKE_SOURCE_DIR}/src/Feature/*.cpp" 
    "${CMAKE_SOURCE_DIR}/src/Support/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Server/*.cpp"
)
add_library(clice-core ${CLICE_LIB_TYPE} ${CLICE_CORE_SOURCES})

target_precompile_headers(clice-core PRIVATE 
    "${CMAKE_SOURCE_DIR}/include/Compiler/Clang.h"
)
target_include_directories(clice-core PUBLIC
    ${LLVM_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(clice-core PUBLIC
    LLVMSupport
    LLVMFrontendOpenMP
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

# clice executable
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/libuv)

add_executable(clice ${SRC_FILES} ${CMAKE_SOURCE_DIR}/src/Driver/clice.cc)
add_executable(integration_tests ${SRC_FILES} ${CMAKE_SOURCE_DIR}/src/Driver/integration_tests.cc)

include_directories(
    "${CMAKE_SOURCE_DIR}/deps/toml/include"
    "${CMAKE_SOURCE_DIR}/deps/libuv/include"
)

target_link_libraries(clice PRIVATE clice-core uv)
target_link_libraries(integration_tests PRIVATE clice-core uv)

# clice tests
if(CLICE_ENABLE_TEST)
    add_subdirectory("${CMAKE_SOURCE_DIR}/deps/googletest")
    set(CLICE_TEST_SOURCES ${CMAKE_SOURCE_DIR}/src/Driver/unit_tests.cc)
    file(GLOB_RECURSE TEST_SRC_FILES "${CMAKE_SOURCE_DIR}/unittests/*/*.cpp")
    list(APPEND CLICE_TEST_SOURCES ${TEST_SRC_FILES})

    add_executable(unit_tests ${CLICE_TEST_SOURCES} ${SRC_FILES})
    target_include_directories(unit_tests PRIVATE
        "${CMAKE_SOURCE_DIR}/deps/googletest/googletest/include"
    )
    target_link_libraries(unit_tests PRIVATE 
        clice-core
        gtest_main
        uv
    )
endif()


