cmake_minimum_required(VERSION 3.22)
project(clice)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} -g -O0 -fno-rtti ")

set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/external/llvm/lib/cmake")
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

set(PCH_HEADER "${CMAKE_SOURCE_DIR}/include/Clang/Clang.h")

add_subdirectory(external/libuv)
add_subdirectory(external/simdjson)

file(GLOB_RECURSE SOURCES src/*.cpp)
add_executable(clice ${SOURCES})

target_include_directories(clice PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_include_directories(clice PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(clice PRIVATE ${CLANG_INCLUDE_DIRS})

llvm_map_components_to_libnames(llvm_libs support core irreader)

message(STATUS "LLVM_INCLUDE_DIRS: ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM_LIBS: ${llvm_libs}")

target_precompile_headers(clice PRIVATE ${PCH_HEADER})

target_link_libraries(clice PRIVATE uv)
target_link_libraries(clice PRIVATE simdjson)
target_link_libraries(clice PRIVATE ${llvm_libs})

target_link_libraries(clice PRIVATE
    LLVMTargetParser
    clangAST
    clangASTMatchers
    clangBasic
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