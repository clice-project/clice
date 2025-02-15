if(WIN32)
    set(CMAKE_SYSTEM_NAME Windows)

    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)
else()
    set(CMAKE_SYSTEM_NAME Linux)

    set(CMAKE_C_COMPILER clang-20)
    set(CMAKE_CXX_COMPILER clang++-20)
endif()
