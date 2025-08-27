include_guard()

# look up specific version's corresponding commit
function(lookup_commit llvm_ver OUTPUT_VAR)
    # version should matches: "\d{2}\.\d+.\d+", 11.4.5, e.g.
    set(LLVM_TAG "llvmorg-${llvm_ver}")
    # fetch tag info
    set(GITHUB_API_URL "https://api.github.com/repos/llvm/llvm-project/git/ref/tags/${LLVM_TAG}")

    message(STATUS "Fetching tag info from: ${GITHUB_API_URL}")
    
    # download tag info
    file(DOWNLOAD 
        ${GITHUB_API_URL}
        ${CMAKE_CURRENT_BINARY_DIR}/tag_info.json
        STATUS DOWNLOAD_STATUS
        TLS_VERIFY ON
    )
    
    # check download info
    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
    if(NOT STATUS_CODE EQUAL 0)
        list(GET DOWNLOAD_STATUS 1 ERROR_MSG)
        message(WARNING "Failed to download tag info: ${ERROR_MSG}")
        set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
        return()
    endif()
    
    # read json
    file(READ ${CMAKE_CURRENT_BINARY_DIR}/tag_info.json TAG_INFO_JSON)

    # check object type
    string(JSON OBJECT_TYPE GET ${TAG_INFO_JSON} object type)
    string(JSON OBJECT_SHA GET ${TAG_INFO_JSON} object sha)
    
    if(OBJECT_TYPE STREQUAL "commit")
        # is commit
        set(${OUTPUT_VAR} ${OBJECT_SHA} PARENT_SCOPE)
    elseif(OBJECT_TYPE STREQUAL "tag")
        # is annotated
        set(TAG_API_URL "https://api.github.com/repos/llvm/llvm-project/git/tags/${OBJECT_SHA}")
        
        file(DOWNLOAD 
            ${TAG_API_URL}
            ${CMAKE_CURRENT_BINARY_DIR}/annotated_tag_info.json
            STATUS TAG_DOWNLOAD_STATUS
            TLS_VERIFY ON
        )
        
        list(GET TAG_DOWNLOAD_STATUS 0 TAG_STATUS_CODE)
        if(NOT TAG_STATUS_CODE EQUAL 0)
            list(GET TAG_DOWNLOAD_STATUS 1 TAG_ERROR_MSG)
            message(WARNING "Failed to download annotated tag info: ${TAG_ERROR_MSG}")
            set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
            return()
        endif()
        
        file(READ ${CMAKE_CURRENT_BINARY_DIR}/annotated_tag_info.json ANNOTATED_TAG_JSON)
        string(JSON COMMIT_SHA GET ${ANNOTATED_TAG_JSON} object sha)
        set(${OUTPUT_VAR} ${COMMIT_SHA} PARENT_SCOPE)
    else()
        message(WARNING "Unknown object type: ${OBJECT_TYPE}")
        set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
    endif()
endfunction()

function(fetch_private_clang_files llvm_ver)

    set(PRIVATE_CLANG_FILE_LIST
        "Sema/CoroutineStmtBuilder.h"
        "Sema/TypeLocBuilder.h"
        "Sema/TreeTransform.h"
    )
    
    set(PRIVATE_FILE_EXISTS TRUE)
    foreach(FILE ${PRIVATE_CLANG_FILE_LIST})
        if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/include/clang/${FILE}")
            message(STATUS "Private clang file already exists: ${FILE}")
        else()
            set(PRIVATE_FILE_EXISTS FALSE)
            break()
        endif()
    endforeach()

    if(PRIVATE_FILE_EXISTS)
        message(STATUS "All private clang files already exist.")
        return()
    endif()

    message(WARNING "Private clang files not found, try fetch from llvm-project source...")

    set(LLVM_COMMIT "")
    lookup_commit(${llvm_ver} LLVM_COMMIT)

    set(PRIVATE_FILE_COMMIT "${LLVM_COMMIT}")

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/clang")
    foreach(FILE ${PRIVATE_CLANG_FILE_LIST})
        if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/include/clang/${FILE}")
            file(
                DOWNLOAD "https://raw.githubusercontent.com/llvm/llvm-project/${PRIVATE_FILE_COMMIT}/clang/lib/${FILE}" "${CMAKE_CURRENT_BINARY_DIR}/include/clang/${FILE}"
                SHOW_PROGRESS
            ) 
        endif()
    endforeach()

    # add to include directories
    target_include_directories(llvm-libs INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/include")
endfunction()

function(detect_llvm result)
    find_program(LLVM_CONFIG_EXEC llvm-config)
    if(NOT LLVM_CONFIG_EXEC)
        set(${result} "" PARENT_SCOPE)
        return()
    endif()
    execute_process(
        COMMAND "${LLVM_CONFIG_EXEC}" --version
        OUTPUT_VARIABLE LLVM_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT (LLVM_VERSION VERSION_GREATER_EQUAL "20.1" AND LLVM_VERSION VERSION_LESS "20.2"))
        set(${result} "" PARENT_SCOPE)
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
    set(${result} ${LLVM_VERSION} PARENT_SCOPE)
endfunction()

function(install_prebuilt_llvm llvm_ver)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/.llvm")
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(LLVM_BUILD_TYPE "debug")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(LLVM_BUILD_TYPE "release")
    else()
        set(LLVM_BUILD_TYPE "release-lto")
    endif()

    if(WIN32)
        set(LLVM_PACKAGE "x64-windows-msvc-${LLVM_BUILD_TYPE}.7z")
    elseif(APPLE)
        set(LLVM_PACKAGE "arm64-macosx-apple-${LLVM_BUILD_TYPE}.tar.xz")
    elseif(UNIX)
        set(LLVM_PACKAGE "x86_64-linux-gnu-${LLVM_BUILD_TYPE}.tar.xz")
    endif()

    message(STATUS "Downloading pre-built LLVM package file ${LLVM_PACKAGE}, please wait...")

    file(
        DOWNLOAD "https://github.com/clice-project/llvm-binary/releases/download/${llvm_ver}/${LLVM_PACKAGE}" "${CMAKE_CURRENT_BINARY_DIR}/${LLVM_PACKAGE}"
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
        message(STATUS "LLVM_INSTALL_PATH is set to ${LLVM_INSTALL_PATH}, using it directly.")
        return()
    endif()

    detect_llvm(LLVM_VERSION)

    if(LLVM_VERSION STREQUAL "")
        set(LLVM_VERSION "20.1.5")
        message(WARNING "System LLVM not found or version mismatch, downloading prebuilt LLVM...")
        install_prebuilt_llvm(LLVM_VERSION)
    endif()

    fetch_private_clang_files(LLVM_VERSION)
endfunction()
