include(${CMAKE_CURRENT_LIST_DIR}/utils.cmake)

# Look up a Git tag's corresponding commit SHA from a GitHub repository
function(github_lookup_tag_commit REPO_OWNER REPO_NAME TAG_NAME OUTPUT_VAR)
    set(GITHUB_API_URL "https://api.github.com/repos/${REPO_OWNER}/${REPO_NAME}/git/ref/tags/${TAG_NAME}")

    message(STATUS "Fetching tag info from: ${GITHUB_API_URL}")

    # Download tag info directly into a string
    download_to_string(${GITHUB_API_URL} TAG_JSON DOWNLOAD_RESULT)
    if(NOT DOWNLOAD_RESULT EQUAL "0")
        message(WARNING "Failed to fetch tag info. Result: ${DOWNLOAD_RESULT}")
        set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
        return()
    endif()

    # Parse the JSON to get object type and SHA
    parse_json_field_from_string("${TAG_JSON}" object "type" OBJECT_TYPE)
    parse_json_field_from_string("${TAG_JSON}" object "sha" OBJECT_SHA)

    if(OBJECT_TYPE STREQUAL "NOTFOUND" OR OBJECT_SHA STREQUAL "NOTFOUND")
        message(WARNING "Could not find object type or SHA in the JSON response.")
        set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
        return()
    endif()

    set(COMMIT_SHA "NOTFOUND")

    if(OBJECT_TYPE STREQUAL "commit")
        # Direct commit reference
        set(COMMIT_SHA ${OBJECT_SHA})
    elseif(OBJECT_TYPE STREQUAL "tag")
        # Annotated tag - need to fetch the actual commit
        set(TAG_API_URL "https://api.github.com/repos/${REPO_OWNER}/${REPO_NAME}/git/tags/${OBJECT_SHA}")

        message(STATUS "Fetching annotated tag info from: ${TAG_API_URL}")

        # Download annotated tag info directly into a string
        download_to_string(${TAG_API_URL} ANNOTATED_TAG_JSON TAG_DOWNLOAD_RESULT)
        if(NOT TAG_DOWNLOAD_RESULT EQUAL "0")
            message(WARNING "Failed to fetch annotated tag info. Result: ${TAG_DOWNLOAD_RESULT}")
            set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
            return()
        endif()

        # Parse the annotated tag JSON to get the commit SHA
        parse_json_field_from_string("${ANNOTATED_TAG_JSON}" object "sha" COMMIT_SHA_FROM_TAG)

        if(NOT COMMIT_SHA_FROM_TAG STREQUAL "NOTFOUND")
            set(COMMIT_SHA ${COMMIT_SHA_FROM_TAG})
        endif()
    else()
        message(WARNING "Unknown object type: ${OBJECT_TYPE}")
    endif()

    set(${OUTPUT_VAR} ${COMMIT_SHA} PARENT_SCOPE)
endfunction()
