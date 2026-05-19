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

# ── Path where .vix project sources live ────────────────────────────────
# Override with -DVIX_PROJECT_ROOT=<path> if your .vix files live elsewhere.
# Defaults to a "vix_src" folder next to CMakeLists.txt.
set(VIX_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vix_src"
    CACHE PATH "Root directory that contains .vix source files to check")

# The project name passed to `vix check`.  Matches what config.toml says.
set(VIX_PROJECT_NAME "my_project"
    CACHE STRING "Vix project name (used by the pack/snapshot system)")

if(VIX_BUILD_CORE)
    if(NOT EXISTS "${VIX_MAIN_SOURCE}")
        message(FATAL_ERROR "Entry point not found: ${VIX_MAIN_SOURCE}")
    endif()

    if(NOT VIX_SOURCES)
        message(FATAL_ERROR "No core source files found under src/")
    endif()

    list(LENGTH VIX_SOURCES VIX_SOURCE_COUNT)
    message(STATUS "VIX_BUILD_CORE=ON: building ${VIX_SOURCE_COUNT} source files from src/")

    # ── flatcc ──────────────────────────────────────────────────────────
    set(FLATCC_INCLUDE_DIR "C:/Users/Client/Desktop/vcpkg/installed/x64-windows/include")
    set(FLATCC_LIB         "C:/Users/Client/Desktop/vcpkg/installed/x64-windows/lib/flatccrt.lib")
    set(FLATCC_LIB_DEBUG   "C:/Users/Client/Desktop/vcpkg/installed/x64-windows/debug/lib/flatccrt_d.lib")

    find_program(FLATCC_EXE flatcc
        HINTS "C:/Users/Client/Desktop/flatcc-src/bin/Release"
        REQUIRED)

    set(FBS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/project_pack.fbs")
    set(GEN_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/src/gen")
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
    # ────────────────────────────────────────────────────────────────────

    add_library(vix_core STATIC ${VIX_SOURCES})
    add_dependencies(vix_core vix_fbs_gen)

    add_executable(vix ${VIX_MAIN_SOURCE})
    target_link_libraries(vix PRIVATE vix_core)

    target_include_directories(vix_core
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gen"
    )

    target_include_directories(vix
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gen"
    )

    target_include_directories(vix_core PUBLIC "${FLATCC_INCLUDE_DIR}")
    target_link_libraries(vix_core PUBLIC
        "$<$<CONFIG:Debug>:${FLATCC_LIB_DEBUG}>"
        "$<$<NOT:$<CONFIG:Debug>>:${FLATCC_LIB}>"
    )

    if(MSVC)
        target_compile_options(vix_core PRIVATE /W4)
        target_compile_options(vix     PRIVATE /W4)
    else()
        target_compile_options(vix_core PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(vix     PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    # ── Incremental semantic check for every .vix source file ───────────
    #
    # At the end of every build CMake:
    #   1. Collects all .vix files under VIX_PROJECT_ROOT (configure-time glob).
    #   2. For each file adds a POST_BUILD custom command that calls
    #        vix check <file> <project_name>
    #      CMake only re-runs the command when the .vix file's timestamp is
    #      newer than the sentinel stamp file we touch on success.
    #
    # The stamp files live in <build_dir>/vix_stamps/ so they survive
    # incremental builds but are wiped by a clean.
    # ────────────────────────────────────────────────────────────────────

    set(VIX_STAMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/vix_stamps")
    file(MAKE_DIRECTORY "${VIX_STAMP_DIR}")

    # Glob .vix files at configure time.
    # CONFIGURE_DEPENDS re-runs CMake automatically when the set changes.
    file(GLOB_RECURSE VIX_PROJECT_SOURCES CONFIGURE_DEPENDS
        "${VIX_PROJECT_ROOT}/*.vix"
    )

    if(NOT VIX_PROJECT_SOURCES)
        message(STATUS
            "No .vix files found under ${VIX_PROJECT_ROOT}. "
            "Set -DVIX_PROJECT_ROOT=<path> if your sources live elsewhere.")
    else()
        list(LENGTH VIX_PROJECT_SOURCES _nvix)
        message(STATUS "Found ${_nvix} .vix source file(s) to check incrementally")
    endif()

    foreach(_vix_file IN LISTS VIX_PROJECT_SOURCES)
        # Build a safe filename for the stamp: replace non-alphanum with '_'
        string(REGEX REPLACE "[^a-zA-Z0-9]" "_" _safe_name "${_vix_file}")
        set(_stamp "${VIX_STAMP_DIR}/${_safe_name}.stamp")

        #
        # add_custom_command(OUTPUT ...) — CMake only reruns this when
        # the .vix file is newer than the stamp.
        #
        add_custom_command(
            OUTPUT  "${_stamp}"
            COMMAND "$<TARGET_FILE:vix>"
                        check
                        "${_vix_file}"
                        "${VIX_PROJECT_NAME}"
            COMMAND "${CMAKE_COMMAND}" -E touch "${_stamp}"
            DEPENDS vix "${_vix_file}"
            COMMENT "vix check: ${_vix_file}"
            VERBATIM
        )

        # Give the stamp a target name so it participates in the build graph.
        string(MD5 _hash "${_vix_file}")
        set(_tgt "vix_check_${_hash}")
        add_custom_target("${_tgt}" ALL DEPENDS "${_stamp}")

        # The check must happen after the vix binary is fully linked.
        add_dependencies("${_tgt}" vix)
    endforeach()

else()
    message(STATUS "VIX_BUILD_CORE=OFF: configured without compiling src/ (use -DVIX_BUILD_CORE=ON)")
endif()