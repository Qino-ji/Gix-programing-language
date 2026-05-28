cmake_minimum_required(VERSION 3.16)
project(Vix LANGUAGES C CXX)

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

file(GLOB_RECURSE VIX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/codegen/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/codegen/*.cpp"
)

set(VIX_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")
list(REMOVE_ITEM VIX_SOURCES "${VIX_MAIN_SOURCE}")

option(VIX_BUILD_CORE "Build core sources under src/" ON)

set(VIX_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vix_src"
    CACHE PATH "Root directory that contains .vix source files to check")

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

    add_custom_target(vix_fbs_gen DEPENDS
        "${GEN_DIR}/project_pack_reader.h"
        "${GEN_DIR}/project_pack_builder.h"
        "${GEN_DIR}/project_pack_verifier.h"
    )
    # ────────────────────────────────────────────────────────────────────

    # ── Define targets FIRST, then configure ────────────────────────────
    add_library(vix_core STATIC ${VIX_SOURCES})
    add_executable(vix ${VIX_MAIN_SOURCE})

    add_dependencies(vix_core vix_fbs_gen)

    # ── Include dirs ─────────────────────────────────────────────────────
    target_include_directories(vix_core
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gen"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/codegen"
            "${LLVM_INCLUDE_DIR}"
            "${FLATCC_INCLUDE_DIR}"
    )

    target_include_directories(vix
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gen"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/codegen"
    )

    # ── Compile definitions ───────────────────────────────────────────────
    target_compile_definitions(vix_core PUBLIC ${LLVM_DEFS})

    # ── Link libraries ────────────────────────────────────────────────────
    target_link_libraries(vix_core PUBLIC
        ${LLVM_LIBS}
        "$<$<CONFIG:Debug>:${FLATCC_LIB_DEBUG}>"
        "$<$<NOT:$<CONFIG:Debug>>:${FLATCC_LIB}>"
    )

    target_link_libraries(vix PRIVATE vix_core)

    # ── Warnings ──────────────────────────────────────────────────────────
    if(MSVC)
        target_compile_options(vix_core PRIVATE /W4)
        target_compile_options(vix      PRIVATE /W4)
    else()
        target_compile_options(vix_core PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(vix      PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    # ── Incremental .vix semantic checks ─────────────────────────────────
    set(VIX_STAMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/vix_stamps")
    file(MAKE_DIRECTORY "${VIX_STAMP_DIR}")

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
        string(REGEX REPLACE "[^a-zA-Z0-9]" "_" _safe_name "${_vix_file}")
        set(_stamp "${VIX_STAMP_DIR}/${_safe_name}.stamp")

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

        string(MD5 _hash "${_vix_file}")
        set(_tgt "vix_check_${_hash}")
        add_custom_target("${_tgt}" ALL DEPENDS "${_stamp}")
        add_dependencies("${_tgt}" vix)
    endforeach()

else()
    message(STATUS "VIX_BUILD_CORE=OFF: configured without compiling src/ (use -DVIX_BUILD_CORE=ON)")
endif()