cmake_minimum_required(VERSION 3.20)
project(CLICE_PROJECT LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# set llvm-libs install path
if(NOT DEFINED LLVM_INSTALL_PATH OR LLVM_INSTALL_PATH STREQUAL "")
    message(FATAL_ERROR "Error: The variable LLVM_INSTALL_PATH is not set. Please specify it with -DLLVM_INSTALL_PATH=<value>.")
endif()
get_filename_component(LLVM_INSTALL_PATH "${LLVM_INSTALL_PATH}" ABSOLUTE)

if(NOT EXISTS "${LLVM_INSTALL_PATH}")
    message(FATAL_ERROR "Error: The specified LLVM_INSTALL_PATH does not exist: ${LLVM_INSTALL_PATH}")
endif()
message(STATUS "Found llvm-libs ${LLVM_INSTALL_PATH}")

set(CMAKE_PREFIX_PATH "${LLVM_INSTALL_PATH}")

# install dependencies
include(FetchContent)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    # libuv option
    set(ASAN ON CACHE BOOL "" FORCE)
endif()

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

if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
endif()

set(CLICE_CXX_FLAGS "-fno-rtti;-fno-exceptions;-Wno-deprecated-declarations;-Wno-undefined-inline;")
set(CLICE_LINKER_FLAGS "")

if(CLICE_USE_LIBCXX)
    list(APPEND CLICE_CXX_FLAGS "-stdlib=libc++")
    list(APPEND CLICE_LINKER_FLAGS "-stdlib=libc++")
endif()

# Build-type specific flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug build: Enabling -g -O0 -fsanitize=address")
    list(APPEND CLICE_CXX_FLAGS "-g;-O0;-fsanitize=address")
    list(APPEND CLICE_LINKER_FLAGS "-fsanitize=address")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    message(STATUS "RelWithDebInfo build: Enabling -fsanitize=thread")
    list(APPEND CLICE_CXX_FLAGS "-fsanitize=thread")
    list(APPEND CLICE_LINKER_FLAGS "-fsanitize=thread")
else()
    message(STATUS "Release/Default build: Enabling -O3")
    list(APPEND CLICE_CXX_FLAGS "-O3")
endif()

# Linker flags for LLD
if(WIN32)
    list(APPEND CLICE_LINKER_FLAGS "-fuse-ld=lld-link")
else()
    list(APPEND CLICE_LINKER_FLAGS "-fuse-ld=lld")
endif()

# build clice core part as library
file(GLOB_RECURSE CLICE_SOURCES
    "${CMAKE_SOURCE_DIR}/src/AST/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Async/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Basic/*.cpp" 
    "${CMAKE_SOURCE_DIR}/src/Compiler/*.cpp" 
    "${CMAKE_SOURCE_DIR}/src/Index/*.cpp" 
    "${CMAKE_SOURCE_DIR}/src/Feature/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Server/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Support/*.cpp"
)
add_library(clice-core STATIC "${CLICE_SOURCES}")

# set llvm include and lib path
add_library(llvm-libs INTERFACE IMPORTED)
target_include_directories(llvm-libs INTERFACE "${LLVM_INSTALL_PATH}/include")
if(WIN32)
    target_compile_definitions(llvm-libs INTERFACE "CLANG_BUILD_STATIC")
    target_link_libraries(llvm-libs INTERFACE version ntdll)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_directories(llvm-libs INTERFACE "${LLVM_INSTALL_PATH}/lib")
    target_link_libraries(llvm-libs INTERFACE
        LLVMSupport
        LLVMFrontendOpenMP
        LLVMOption
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
else()
    file(GLOB LLVM_LIBRARIES CONFIGURE_DEPENDS "${LLVM_INSTALL_PATH}/lib/*${CMAKE_STATIC_LIBRARY_SUFFIX}")
    target_link_libraries(llvm-libs INTERFACE ${LLVM_LIBRARIES})
endif()

target_compile_options(clice-core PUBLIC ${CLICE_CXX_FLAGS})
target_link_options(clice-core PUBLIC ${CLICE_LINKER_FLAGS})

target_include_directories(clice-core PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(clice-core PUBLIC uv_a tomlplusplus::tomlplusplus llvm-libs)

# clice executable
add_executable(clice "${CMAKE_SOURCE_DIR}/src/Driver/clice.cc")
target_link_libraries(clice PRIVATE clice-core)
target_compile_options(clice PUBLIC ${CLICE_CXX_FLAGS})
target_link_options(clice PUBLIC ${CLICE_LINKER_FLAGS})

# clice tests
if(CLICE_ENABLE_TEST)
    file(GLOB_RECURSE CLICE_TEST_SOURCES "${CMAKE_SOURCE_DIR}/tests/unit/*/*.cpp")
    add_executable(unit_tests "${CLICE_TEST_SOURCES}" "${CMAKE_SOURCE_DIR}/src/Driver/unit_tests.cc")
    target_include_directories(unit_tests PUBLIC "${CMAKE_SOURCE_DIR}")

    target_link_libraries(unit_tests PRIVATE clice-core)
    target_compile_options(unit_tests PUBLIC ${CLICE_CXX_FLAGS})
    target_link_options(unit_tests PUBLIC ${CLICE_LINKER_FLAGS})
endif()
