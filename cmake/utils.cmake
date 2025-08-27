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
function(parse_json_field JSON_CONTENT FIELD_PATH OUTPUT_VAR)
    string(JSON FIELD_VALUE GET ${JSON_CONTENT} ${FIELD_PATH})
    if(NOT FIELD_VALUE)
        message(WARNING "Could not parse field '${FIELD_PATH}' from JSON.")
        set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
        return()
    endif()
    
    set(${OUTPUT_VAR} ${FIELD_VALUE} PARENT_SCOPE)
endfunction()

# Download JSON from URL and parse specified fields
function(download_and_parse_json URL OUTPUT_FILE FIELD_SPEC OUTPUT_VAR)
    # Download JSON
    download_json(${URL} ${OUTPUT_FILE} DOWNLOAD_RESULT)
    if(DOWNLOAD_RESULT STREQUAL "NOTFOUND")
        set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
        return()
    endif()
    
    # Read and parse JSON
    file(READ ${OUTPUT_FILE} JSON_CONTENT)
    parse_json_fields("${JSON_CONTENT}" "${FIELD_SPEC}" PARSED_VALUES)
    
    if(PARSED_VALUES STREQUAL "NOTFOUND")
        set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
    else()
        set(${OUTPUT_VAR} ${PARSED_VALUES} PARENT_SCOPE)
    endif()
endfunction()

# Parse multiple fields from JSON content
# FIELD_SPEC format: "field1;field2;..." or single field
function(parse_json_fields JSON_CONTENT FIELD_SPEC OUTPUT_VAR)
    string(REPLACE ";" " " FIELDS "${FIELD_SPEC}")
    separate_arguments(FIELD_LIST NATIVE_COMMAND ${FIELDS})
    
    set(PARSED_VALUES "")
    foreach(FIELD ${FIELD_LIST})
        string(JSON FIELD_VALUE GET ${JSON_CONTENT} ${FIELD})
        if(NOT FIELD_VALUE)
            message(WARNING "Could not parse field '${FIELD}' from JSON.")
            set(${OUTPUT_VAR} "NOTFOUND" PARENT_SCOPE)
            return()
        endif()
        list(APPEND PARSED_VALUES ${FIELD_VALUE})
    endforeach()
    
    set(${OUTPUT_VAR} ${PARSED_VALUES} PARENT_SCOPE)
endfunction()