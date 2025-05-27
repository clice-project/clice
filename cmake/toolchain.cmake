if(WIN32)
    set(CMAKE_SYSTEM_NAME Windows)

    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)
elseif(LINUX)
    set(CMAKE_SYSTEM_NAME Linux)

    set(CMAKE_C_COMPILER clang-20)
    set(CMAKE_CXX_COMPILER clang++-20)
elseif(APPLE)
    set(CMAKE_SYSTEM_NAME Darwin)

    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)
else()
    message(FALTAL_ERROR, "Unsupported system")
endif()
