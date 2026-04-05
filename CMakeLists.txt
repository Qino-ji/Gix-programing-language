cmake_minimum_required(VERSION 3.16)

project(Vix LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE VIX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

option(VIX_BUILD_CORE "Build core sources under src/" OFF)

if(VIX_BUILD_CORE)
    if(NOT VIX_SOURCES)
        message(FATAL_ERROR "No source files found under src/")
    endif()

    add_library(vix_core STATIC ${VIX_SOURCES})

    target_include_directories(vix_core
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )

    if(MSVC)
        target_compile_options(vix_core PRIVATE /W4)
    else()
        target_compile_options(vix_core PRIVATE -Wall -Wextra -Wpedantic)
    endif()
else()
    message(STATUS "VIX_BUILD_CORE=OFF: configured CMake project without compiling src/")
endif()
