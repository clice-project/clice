cmake_minimum_required(VERSION 3.20)
project(CLICE_PROJECT)

include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT DEFINED LLVM_INSTALL_PATH OR LLVM_INSTALL_PATH STREQUAL "")
    message(FATAL_ERROR "Error: The variable LLVM_INSTALL_PATH is not set. Please specify it with -LLVM_INSTALL_PATH=<value>.")
endif()

set(CMAKE_PREFIX_PATH "${LLVM_INSTALL_PATH}")
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_INCLUDE_DIRS}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions -g -O0 -fsanitize=address -Wno-deprecated-declarations")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fuse-ld=lld -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld -fsanitize=address")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions -O3 -Wno-deprecated-declarations")
    set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld")
endif()

if(WIN32)
    set(CLICE_BUILD_TYPE STATIC)
else ()
    set(CLICE_BUILD_TYPE STATIC)
endif()

# build clice core part as library
file(GLOB_RECURSE CLICE_SOURCES 
    "${CMAKE_SOURCE_DIR}/src/Basic/*.cpp" 
    "${CMAKE_SOURCE_DIR}/src/Compiler/*.cpp" 
    "${CMAKE_SOURCE_DIR}/src/Index/*.cpp" 
    "${CMAKE_SOURCE_DIR}/src/Feature/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Server/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Support/*.cpp"
)
add_library(clice-core "${CLICE_BUILD_TYPE}" "${CLICE_SOURCES}")

target_precompile_headers(clice-core PRIVATE 
    "${CMAKE_SOURCE_DIR}/include/Compiler/Clang.h"
)

# set llvm include and lib path
target_include_directories(clice-core PUBLIC "${LLVM_INSTALL_PATH}/include")
target_link_directories(clice-core PUBLIC "${LLVM_INSTALL_PATH}/lib")
target_link_libraries(clice-core PUBLIC
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

FetchContent_Declare(
    tomlplusplus   
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
)
FetchContent_Declare(
    libuv
    GIT_REPOSITORY https://github.com/libuv/libuv.git
    GIT_TAG v1.x
)
FetchContent_MakeAvailable(tomlplusplus libuv)

target_include_directories(clice-core PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(clice-core PUBLIC uv tomlplusplus::tomlplusplus)

# clice executable
add_executable(clice "${CMAKE_SOURCE_DIR}/src/Driver/clice.cc")
target_link_libraries(clice PRIVATE clice-core)

# integration_tests
add_executable(integration_tests "${CMAKE_SOURCE_DIR}/src/Driver/integration_tests.cc")
target_link_libraries(integration_tests PRIVATE clice-core)

# clice tests
if(CLICE_ENABLE_TEST)
    file(GLOB_RECURSE CLICE_TEST_SOURCES "${CMAKE_SOURCE_DIR}/unittests/*/*.cpp")
    add_executable(unit_tests "${CLICE_TEST_SOURCES}" "${CMAKE_SOURCE_DIR}/src/Driver/unit_tests.cc")

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(googletest)

    target_link_libraries(unit_tests PRIVATE  gtest_main clice-core)
endif()


