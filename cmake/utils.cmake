include_guard()

# Download content from URL directly into a string.
# Usage: download_to_string(URL OUTPUT_VAR RESULT_VAR)
#   - URL: The URL to download from.
#   - OUTPUT_VAR: The variable to store the downloaded content.
#   - RESULT_VAR: The variable to store the status code (0 on success).
function(download_to_string URL OUTPUT_VAR RESULT_VAR)
    file(DOWNLOAD
        ${URL}
        ${CMAKE_CURRENT_BINARY_DIR}/temp_download_file
        STATUS DOWNLOAD_STATUS
        TLS_VERIFY ON
        TIMEOUT 10 # Set a reasonable timeout
    )

    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)

    if(NOT STATUS_CODE EQUAL 0)
        list(GET DOWNLOAD_STATUS 1 ERROR_MSG)
        message(WARNING "Failed to download from ${URL}: ${ERROR_MSG}")
        set(${RESULT_VAR} ${STATUS_CODE} PARENT_SCOPE)
        return()
    endif()

    file(READ ${CMAKE_CURRENT_BINARY_DIR}/temp_download_file DOWNLOADED_CONTENT)
    file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/temp_download_file)

    set(${OUTPUT_VAR} ${DOWNLOADED_CONTENT} PARENT_SCOPE)
    set(${RESULT_VAR} 0 PARENT_SCOPE)
endfunction()

# Parse a nested JSON field from a string.
# Usage: parse_json_field_from_string(JSON_STRING PARENT_KEY FIELD_NAME OUTPUT_VAR)
#   - JSON_STRING: A variable containing the JSON content as a string.
#   - PARENT_KEY: A key string to get from the JSON content.
#   - FIELD_NAME: The field name to extract.
#   - OUTPUT_VAR: The variable to store the extracted value.
function(parse_json_field_from_string JSON_CONTENT)
    # Get the last argument as the output variable name
    set(ARGS "${ARGN}")
    list(GET ARGS -1 OUTPUT_VAR)
    list(REMOVE_AT ARGS -1)

    string(JSON FIELD_VALUE GET "${JSON_CONTENT}" ${ARGS})

    if(NOT FIELD_VALUE)
        string(JOIN " " FIELD_PATH_STR ${ARGS})
        message(WARNING "Could not parse field '${FIELD_PATH_STR}' from JSON.")
        set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
        return()
    endif()

    set(${OUTPUT_VAR} ${FIELD_VALUE} PARENT_SCOPE)
endfunction()
