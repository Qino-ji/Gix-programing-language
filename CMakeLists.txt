cmake_minimum_required(VERSION 3.16)

project(Vix LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

file(GLOB_RECURSE VIX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

set(VIX_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")
list(REMOVE_ITEM VIX_SOURCES "${VIX_MAIN_SOURCE}")

option(VIX_BUILD_CORE "Build core sources under src/" ON)

if(VIX_BUILD_CORE)
    if(NOT EXISTS "${VIX_MAIN_SOURCE}")
        message(FATAL_ERROR "Entry point not found: ${VIX_MAIN_SOURCE}")
    endif()

    if(NOT VIX_SOURCES)
        message(FATAL_ERROR "No core source files found under src/")
    endif()

    list(LENGTH VIX_SOURCES VIX_SOURCE_COUNT)
    message(STATUS "VIX_BUILD_CORE=ON: building ${VIX_SOURCE_COUNT} source files from src/")

    # ── flatcc ──────────────────────────────────────────────────────────────
    set(FLATCC_INCLUDE_DIR "C:/Users/Client/Desktop/vcpkg/installed/x64-windows/include")
    set(FLATCC_LIB         "C:/Users/Client/Desktop/vcpkg/installed/x64-windows/lib/flatccrt.lib")
    set(FLATCC_LIB_DEBUG   "C:/Users/Client/Desktop/vcpkg/installed/x64-windows/debug/lib/flatccrt_d.lib")

    # Generate flatbuffers headers from project_pack.fbs into src/gen/
    find_program(FLATCC_EXE flatcc HINTS "C:/Users/Client/Desktop/flatcc-src/bin/Release" REQUIRED)
    set(FBS_FILE   "${CMAKE_CURRENT_SOURCE_DIR}/src/project_pack.fbs")
    set(GEN_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/src/gen")

    file(MAKE_DIRECTORY "${GEN_DIR}")

    add_custom_command(
        OUTPUT
            "${GEN_DIR}/project_pack_reader.h"
            "${GEN_DIR}/project_pack_builder.h"
            "${GEN_DIR}/project_pack_verifier.h"
        COMMAND "${FLATCC_EXE}" --reader --builder --verifier -o "${GEN_DIR}" "${FBS_FILE}"
        DEPENDS "${FBS_FILE}"
        COMMENT "Generating flatbuffers headers from project_pack.fbs"
    )

    add_custom_target(vix_fbs_gen DEPENDS
        "${GEN_DIR}/project_pack_reader.h"
        "${GEN_DIR}/project_pack_builder.h"
        "${GEN_DIR}/project_pack_verifier.h"
    )
    # ────────────────────────────────────────────────────────────────────────

    add_library(vix_core STATIC ${VIX_SOURCES})

    # Make sure headers are generated before vix_core tries to compile pack.c
    add_dependencies(vix_core vix_fbs_gen)

    add_executable(vix ${VIX_MAIN_SOURCE})
    target_link_libraries(vix PRIVATE vix_core)

    target_include_directories(vix_core
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gen"   # for generated headers
    )

    target_include_directories(vix
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gen"
    )

    # Link flatcc runtime into vix_core (pack.c needs flatccrt)
    target_include_directories(vix_core PUBLIC "${FLATCC_INCLUDE_DIR}")
    target_link_libraries(vix_core PUBLIC
        "$<$<CONFIG:Debug>:${FLATCC_LIB_DEBUG}>"
        "$<$<NOT:$<CONFIG:Debug>>:${FLATCC_LIB}>"
    )

    if(MSVC)
        target_compile_options(vix_core PRIVATE /W4)
        target_compile_options(vix PRIVATE /W4)
    else()
        target_compile_options(vix_core PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(vix PRIVATE -Wall -Wextra -Wpedantic)
    endif()
else()
    message(STATUS "VIX_BUILD_CORE=OFF: configured without compiling src/ (use -DVIX_BUILD_CORE=ON)")
endif()