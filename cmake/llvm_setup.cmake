include_guard()

function(fetch_private_clang_files)
    # files to fetch:
    # clang/lib/Sema/CoroutineStmtBuilder.h
    # clang/lib/Sema/TypeLocBuilder.h
    # clang/lib/Sema/TreeTransform.h
    # commit hash 7b09d7b446383b71b63d429b21ee45ba389c5134

    # check private clang files' presence
    file(GLOB_RECURSE CLANG_PRIVATE_FILES "${LLVM_INSTALL_PATH}/include/clang/Sema/TreeTransform.h")
    if(CLANG_PRIVATE_FILES)
        message(STATUS "Found private clang files presents, skip downloading.")
        return()
    endif()
    message(WARNING "Private clang files not found, try fetch from llvm-project source...")

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/clang/lib/Sema")
    file(
        DOWNLOAD "https://raw.githubusercontent.com/llvm/llvm-project/7b09d7b446383b71b63d429b21ee45ba389c5134/clang/lib/Sema/CoroutineStmtBuilder.h" "${CMAKE_CURRENT_BINARY_DIR}/include/clang/Sema/CoroutineStmtBuilder.h"
        SHOW_PROGRESS
    )
    file(
        DOWNLOAD "https://raw.githubusercontent.com/llvm/llvm-project/7b09d7b446383b71b63d429b21ee45ba389c5134/clang/lib/Sema/TypeLocBuilder.h" "${CMAKE_CURRENT_BINARY_DIR}/include/clang/Sema/TypeLocBuilder.h"
        SHOW_PROGRESS
    )
    file(
        DOWNLOAD "https://raw.githubusercontent.com/llvm/llvm-project/7b09d7b446383b71b63d429b21ee45ba389c5134/clang/lib/Sema/TreeTransform.h" "${CMAKE_CURRENT_BINARY_DIR}/include/clang/Sema/TreeTransform.h"
        SHOW_PROGRESS
    )

    # add to include directories
    target_include_directories(llvm-libs INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/include")
endfunction()

function(detect_llvm result)
    find_program(LLVM_CONFIG_EXEC llvm-config)
    if(NOT LLVM_CONFIG_EXEC)
        set(${result} FALSE PARENT_SCOPE)
        return()
    endif()
    execute_process(
        COMMAND "${LLVM_CONFIG_EXEC}" --version
        OUTPUT_VARIABLE LLVM_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT LLVM_VERSION VERSION_EQUAL "20.1.5")
        set(${result} FALSE PARENT_SCOPE)
        return()
    endif()
    execute_process(
        COMMAND "${LLVM_CONFIG_EXEC}" --prefix
        OUTPUT_VARIABLE LLVM_INSTALL_PATH_DETECTED
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND "${LLVM_CONFIG_EXEC}" --cmakedir
        OUTPUT_VARIABLE LLVM_CMAKE_DIR_DETECTED
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(LLVM_INSTALL_PATH "${LLVM_INSTALL_PATH_DETECTED}" CACHE PATH "Path to LLVM installation" FORCE)
    set(LLVM_CMAKE_DIR "${LLVM_CMAKE_DIR_DETECTED}" CACHE PATH "Path to LLVM CMake files" FORCE)
    set(${result} TRUE PARENT_SCOPE)
endfunction()

function(install_prebuilt_llvm)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/.llvm")

    if(WIN32)
        set(LLVM_PACKAGE "x64-windows-msvc-release.7z")
    elseif(APPLE)
        set(LLVM_PACKAGE "arm64-macosx-apple-release.tar.xz")
    elseif(UNIX)
        set(LLVM_PACKAGE "x86_64-linux-gnu-release.tar.xz")
    endif()
    message(STATUS "Downloading pre-built LLVM package file ${LLVM_PACKAGE}, please wait...")

    file(
        DOWNLOAD "https://github.com/clice-project/llvm-binary/releases/download/20.1.5/${LLVM_PACKAGE}" "${CMAKE_CURRENT_BINARY_DIR}/${LLVM_PACKAGE}"
        SHOW_PROGRESS
    )

    message(STATUS "Decompressing pre-built LLVM package file ${LLVM_PACKAGE}, please wait...")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E tar xvf "${CMAKE_CURRENT_BINARY_DIR}/${LLVM_PACKAGE}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/.llvm"
    )

    set(LLVM_INSTALL_PATH "${CMAKE_CURRENT_BINARY_DIR}/.llvm" CACHE PATH "Path to LLVM installation" FORCE)
    set(LLVM_CMAKE_DIR "${LLVM_INSTALL_PATH}/lib/cmake/llvm" CACHE PATH "Path to LLVM CMake files" FORCE)
    message(STATUS "LLVM installation path set to: ${LLVM_INSTALL_PATH}")
endfunction()

function(setup_llvm)
    if(DEFINED LLVM_INSTALL_PATH AND NOT LLVM_INSTALL_PATH STREQUAL "")
        message(STATUS "$LLVM_INSTALL_PATH is set, use it directly.")
        return()
    endif()

    detect_llvm(LLVM_FOUND)

    if(NOT LLVM_FOUND)
        message(WARNING "System LLVM not found or version mismatch, downloading prebuilt LLVM...")
        install_prebuilt_llvm()
    endif()
endfunction()
