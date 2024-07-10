cmake_minimum_required(VERSION 3.22)
project(clice)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} -g -O0 -fno-rtti")

set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/external/llvm/lib/cmake")
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "LLVM_INCLUDE_DIRS: ${LLVM_INCLUDE_DIRS}")

set(PCH_HEADER "${CMAKE_SOURCE_DIR}/include/Clang/Clang.h")

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory("${CMAKE_SOURCE_DIR}/external/json")
add_subdirectory("${CMAKE_SOURCE_DIR}/external/libuv")
add_subdirectory("${CMAKE_SOURCE_DIR}/external/spdlog")

set(INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/external/json/include"
    "${CMAKE_SOURCE_DIR}/external/libuv/include"
    "${CMAKE_SOURCE_DIR}/external/spdlog/include"
    "${CMAKE_SOURCE_DIR}/external/llvm/include"
    "${CMAKE_SOURCE_DIR}/external/llvm/tools/clang/include"
)

set(LINK_LIBS
    nlohmann_json::nlohmann_json
    uv
    spdlog::spdlog
    LLVMCore
    LLVMSupport
    LLVMIRReader
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

file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")

add_executable(clice src/main.cpp ${SOURCES})
target_precompile_headers(clice PRIVATE ${PCH_HEADER})
target_include_directories(clice PRIVATE ${INCLUDE_DIRS})
target_link_libraries(clice PRIVATE ${LINK_LIBS})

if(CLICE_TEST)
    file(GLOB_RECURSE TEST_SOURCES "${CMAKE_SOURCE_DIR}/tests/*.cpp")
    include_directories(${CMAKE_SOURCE_DIR}/external/googletest/googletest/include)
    add_subdirectory(external/googletest)

    if(WIN32)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    endif()

    add_executable(clice_test ${SOURCES} ${TEST_SOURCES})
    target_precompile_headers(clice_test PRIVATE ${PCH_HEADER})
    target_include_directories(clice_test PRIVATE ${INCLUDE_DIRS})
    target_link_libraries(clice_test PRIVATE ${LINK_LIBS} GTest::gtest_main)
endif()