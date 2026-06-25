cmake_minimum_required(VERSION 3.16)
project(Gix LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# ── LLVM 18 ──────────────────────────────────────────────────────────────────
set(LLVM_DIR "C:/LLVM/18.1.8")
set(LLVM_INCLUDE_DIR "${LLVM_DIR}/include")

set(LLVM_LIBS
    "${LLVM_DIR}/lib/LLVMCore.lib"
    "${LLVM_DIR}/lib/LLVMRemarks.lib"
    "${LLVM_DIR}/lib/LLVMBitstreamReader.lib"
    "${LLVM_DIR}/lib/LLVMBinaryFormat.lib"
    "${LLVM_DIR}/lib/LLVMTargetParser.lib"
    "${LLVM_DIR}/lib/LLVMSupport.lib"
    "${LLVM_DIR}/lib/LLVMDemangle.lib"
)

set(LLVM_DEFS
    _CRT_SECURE_NO_DEPRECATE
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_DEPRECATE
    _CRT_NONSTDC_NO_WARNINGS
    __STDC_CONSTANT_MACROS
    __STDC_FORMAT_MACROS
    __STDC_LIMIT_MACROS
    UNICODE
    _UNICODE
)
# ─────────────────────────────────────────────────────────────────────────────

file(GLOB_RECURSE GIX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/codegen/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/codegen/*.cpp"
)

set(GIX_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")
list(REMOVE_ITEM GIX_SOURCES "${GIX_MAIN_SOURCE}")

option(GIX_BUILD_CORE "Build core sources under src/" ON)

set(GIX_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/gix_src"
    CACHE PATH "Root directory that contains .gix source files to check")

set(GIX_PROJECT_NAME "my_project"
    CACHE STRING "Gix project name (used by the pack/snapshot system)")

if(GIX_BUILD_CORE)
    if(NOT EXISTS "${GIX_MAIN_SOURCE}")
        message(FATAL_ERROR "Entry point not found: ${GIX_MAIN_SOURCE}")
    endif()

    if(NOT GIX_SOURCES)
        message(FATAL_ERROR "No core source files found under src/")
    endif()

    list(LENGTH GIX_SOURCES GIX_SOURCE_COUNT)
    message(STATUS "GIX_BUILD_CORE=ON: building ${GIX_SOURCE_COUNT} source files from src/")

    # ── flatcc ───────────────────────────────────────────────────────────
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

    add_custom_target(gix_fbs_gen DEPENDS
        "${GEN_DIR}/project_pack_reader.h"
        "${GEN_DIR}/project_pack_builder.h"
        "${GEN_DIR}/project_pack_verifier.h"
    )
    # ────────────────────────────────────────────────────────────────────

    # ── Define targets FIRST, then configure ────────────────────────────
    add_library(gix_core STATIC ${GIX_SOURCES})
    add_executable(gix ${GIX_MAIN_SOURCE})

    add_dependencies(gix_core gix_fbs_gen)

    # ── Include dirs ─────────────────────────────────────────────────────
    target_include_directories(gix_core
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gen"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/codegen"
            "${LLVM_INCLUDE_DIR}"
            "${FLATCC_INCLUDE_DIR}"
    )

    target_include_directories(gix
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gen"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/codegen"
    )

    # ── Compile definitions ───────────────────────────────────────────────
    target_compile_definitions(gix_core PUBLIC ${LLVM_DEFS})

    # ── Link libraries ────────────────────────────────────────────────────
    target_link_libraries(gix_core PUBLIC
        ${LLVM_LIBS}
        "$<$<CONFIG:Debug>:${FLATCC_LIB_DEBUG}>"
        "$<$<NOT:$<CONFIG:Debug>>:${FLATCC_LIB}>"
    )

    target_link_libraries(gix PRIVATE gix_core)

    # ── Warnings ──────────────────────────────────────────────────────────
    if(MSVC)
        target_compile_options(gix_core PRIVATE /W4)
        target_compile_options(gix      PRIVATE /W4)
    else()
        target_compile_options(gix_core PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(gix      PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    # ── Incremental .gix semantic checks ─────────────────────────────────
    set(GIX_STAMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/gix_stamps")
    file(MAKE_DIRECTORY "${GIX_STAMP_DIR}")

    file(GLOB_RECURSE GIX_PROJECT_SOURCES CONFIGURE_DEPENDS
        "${GIX_PROJECT_ROOT}/*.gix"
    )

    if(NOT GIX_PROJECT_SOURCES)
        message(STATUS
            "No .gix files found under ${GIX_PROJECT_ROOT}. "
            "Set -DGIX_PROJECT_ROOT=<path> if your sources live elsewhere.")
    else()
        list(LENGTH GIX_PROJECT_SOURCES _ngix)
        message(STATUS "Found ${_ngix} .gix source file(s) to check incrementally")
    endif()

    foreach(_gix_file IN LISTS GIX_PROJECT_SOURCES)
        string(REGEX REPLACE "[^a-zA-Z0-9]" "_" _safe_name "${_gix_file}")
        set(_stamp "${GIX_STAMP_DIR}/${_safe_name}.stamp")

        add_custom_command(
            OUTPUT  "${_stamp}"
            COMMAND "$<TARGET_FILE:gix>"
                        check
                        "${_gix_file}"
                        "${GIX_PROJECT_NAME}"
            COMMAND "${CMAKE_COMMAND}" -E touch "${_stamp}"
            DEPENDS gix "${_gix_file}"
            COMMENT "gix check: ${_gix_file}"
            VERBATIM
        )

        string(MD5 _hash "${_gix_file}")
        set(_tgt "gix_check_${_hash}")
        add_custom_target("${_tgt}" ALL DEPENDS "${_stamp}")
        add_dependencies("${_tgt}" gix)
    endforeach()

else()
    message(STATUS "GIX_BUILD_CORE=OFF: configured without compiling src/ (use -DGIX_BUILD_CORE=ON)")
endif()