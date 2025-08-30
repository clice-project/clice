include_guard()

# Download JSON from URL with error handling
function(download_json URL OUTPUT_FILE OUTPUT_VAR)
    file(DOWNLOAD 
        ${URL}
        ${OUTPUT_FILE}
        STATUS DOWNLOAD_STATUS
        TLS_VERIFY ON
    )
    
    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
    if(NOT STATUS_CODE EQUAL 0)
        list(GET DOWNLOAD_STATUS 1 ERROR_MSG)
        message(WARNING "Failed to download JSON from ${URL}: ${ERROR_MSG}")
        set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
        return()
    endif()
    
    set(${OUTPUT_VAR} "SUCCESS" PARENT_SCOPE)
endfunction()

# Parse nested JSON field with error checking
# Usage: parse_json_field("${JSON}" object "type" OUTPUT_VAR) 
function(parse_json_field JSON_CONTENT)
    set(ARGS "${ARGN}")
    list(GET ARGS -1 OUTPUT_VAR)
    list(REMOVE_AT ARGS -1)
    
    string(JSON FIELD_VALUE GET ${JSON_CONTENT} ${ARGS})
    if(NOT FIELD_VALUE)
        string(JOIN " " FIELD_PATH_STR ${ARGS})
        message(WARNING "Could not parse field '${FIELD_PATH_STR}' from JSON.")
        set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
        return()
    endif()
    
    set(${OUTPUT_VAR} ${FIELD_VALUE} PARENT_SCOPE)
endfunction()