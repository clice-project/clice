if(WIN32)
    set(CMAKE_SYSTEM_NAME Windows)
else()
    set(CMAKE_SYSTEM_NAME Linux)
endif()

set(CMAKE_C_COMPILER clang-19)
set(CMAKE_CXX_COMPILER clang++-19)
