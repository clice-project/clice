include_guard()

include(${CMAKE_CURRENT_LIST_DIR}/utils.cmake)

# Look up a Git tag's corresponding commit SHA from a GitHub repository
function(github_lookup_tag_commit REPO_OWNER REPO_NAME TAG_NAME OUTPUT_VAR)
    set(GITHUB_API_URL "https://api.github.com/repos/${REPO_OWNER}/${REPO_NAME}/git/ref/tags/${TAG_NAME}")

    message(STATUS "Fetching tag info from: ${GITHUB_API_URL}")
    
    # Download tag info
    download_json(${GITHUB_API_URL} ${CMAKE_CURRENT_BINARY_DIR}/tag_info.json DOWNLOAD_RESULT)
    if(DOWNLOAD_RESULT STREQUAL "NOTFOUND")
        set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
        return()
    endif()
    
    # Parse the JSON to get object type and SHA
    file(READ ${CMAKE_CURRENT_BINARY_DIR}/tag_info.json TAG_JSON)
    parse_json_field("${TAG_JSON}" object "type" OBJECT_TYPE)
    parse_json_field("${TAG_JSON}" object sha OBJECT_SHA)
    
    if(OBJECT_TYPE STREQUAL "NOTFOUND" OR OBJECT_SHA STREQUAL "NOTFOUND")
        set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
        return()
    endif()
    
    if(OBJECT_TYPE STREQUAL "commit")
        # Direct commit reference
        set(${OUTPUT_VAR} ${OBJECT_SHA} PARENT_SCOPE)
    elseif(OBJECT_TYPE STREQUAL "tag")
        # Annotated tag - need to fetch the actual commit
        set(TAG_API_URL "https://api.github.com/repos/${REPO_OWNER}/${REPO_NAME}/git/tags/${OBJECT_SHA}")

        message(STATUS "Fetching annotated tag info from: ${TAG_API_URL}")
        
        # Download annotated tag info
        download_json(${TAG_API_URL} ${CMAKE_CURRENT_BINARY_DIR}/annotated_tag_info.json TAG_DOWNLOAD_RESULT)
        if(TAG_DOWNLOAD_RESULT STREQUAL "NOTFOUND")
            set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
            return()
        endif()
        
        # Parse the annotated tag JSON to get the commit SHA
        file(READ ${CMAKE_CURRENT_BINARY_DIR}/annotated_tag_info.json ANNOTATED_TAG_JSON)
        parse_json_field("${ANNOTATED_TAG_JSON}" object sha COMMIT_SHA)
        
        if(COMMIT_SHA STREQUAL "NOTFOUND")
            set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
            return()
        endif()
        
        set(${OUTPUT_VAR} ${COMMIT_SHA} PARENT_SCOPE)
    else()
        message(WARNING "Unknown object type: ${OBJECT_TYPE}")
        set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
    endif()
endfunction()