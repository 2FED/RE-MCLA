# mcla - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(mcla LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MCLA_REXGLUE_VERSION "0.9.0.7")
if(DEFINED REXSDK_VERSION AND
   NOT REXSDK_VERSION STREQUAL "" AND
   NOT REXSDK_VERSION STREQUAL "${MCLA_REXGLUE_VERSION}")
    message(FATAL_ERROR
        "MCLA-R requires ReXGlue ${MCLA_REXGLUE_VERSION}; "
        "REXSDK_VERSION is '${REXSDK_VERSION}'.")
endif()
set(REXSDK_VERSION "${MCLA_REXGLUE_VERSION}" CACHE STRING
    "Exact ReXGlue SDK version required by MCLA-R")

include(generated/rexglue.cmake)

if(NOT DEFINED REXGLUE_ENTRYPOINT_GENERATED_SOURCES)
    message(STATUS
        "MCLA guest sources are absent; only the mcla_codegen target is available. "
        "Run codegen, then configure again to create the native target.")
    return()
endif()

set(MCLA_GENERATED_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/generated/default")
set(MCLA_EXPECTED_GENERATED_SOURCE_COUNT 65)
file(REAL_PATH "${MCLA_GENERATED_ROOT}" MCLA_GENERATED_ROOT_REAL)

list(LENGTH REXGLUE_ENTRYPOINT_GENERATED_SOURCES MCLA_GENERATED_SOURCE_COUNT)
set(MCLA_GENERATED_SOURCES_UNIQUE ${REXGLUE_ENTRYPOINT_GENERATED_SOURCES})
list(REMOVE_DUPLICATES MCLA_GENERATED_SOURCES_UNIQUE)
list(LENGTH MCLA_GENERATED_SOURCES_UNIQUE MCLA_GENERATED_SOURCE_UNIQUE_COUNT)
if(NOT MCLA_GENERATED_SOURCE_COUNT EQUAL MCLA_EXPECTED_GENERATED_SOURCE_COUNT OR
   NOT MCLA_GENERATED_SOURCE_UNIQUE_COUNT EQUAL MCLA_EXPECTED_GENERATED_SOURCE_COUNT)
    message(FATAL_ERROR
        "Expected ${MCLA_EXPECTED_GENERATED_SOURCE_COUNT} unique generated sources, got "
        "${MCLA_GENERATED_SOURCE_COUNT} entries / ${MCLA_GENERATED_SOURCE_UNIQUE_COUNT} unique.")
endif()

foreach(MCLA_GENERATED_SOURCE IN LISTS REXGLUE_ENTRYPOINT_GENERATED_SOURCES)
    if(NOT EXISTS "${MCLA_GENERATED_SOURCE}")
        message(FATAL_ERROR "Generated source is missing: ${MCLA_GENERATED_SOURCE}")
    endif()
    file(REAL_PATH "${MCLA_GENERATED_SOURCE}" MCLA_GENERATED_SOURCE_REAL)
    cmake_path(IS_PREFIX MCLA_GENERATED_ROOT_REAL "${MCLA_GENERATED_SOURCE_REAL}"
        NORMALIZE MCLA_GENERATED_SOURCE_CONTAINED)
    if(NOT MCLA_GENERATED_SOURCE_CONTAINED OR
       NOT MCLA_GENERATED_SOURCE_REAL MATCHES "[.]cpp$")
        message(FATAL_ERROR
            "Generated source is outside generated/default or is not C++: "
            "${MCLA_GENERATED_SOURCE}")
    endif()
endforeach()

if(NOT EXISTS "${MCLA_GENERATED_ROOT}/mcla_init.h")
    message(FATAL_ERROR "Generated initialization header is missing: generated/default/mcla_init.h")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MCLA_GENERATED_ROOT}/sources.cmake")

# Sources
set(MCLA_SOURCES
    src/main.cpp
    src/mcla_app.cpp
)

if(WIN32)
    add_executable(mcla WIN32 ${MCLA_SOURCES})
else()
    add_executable(mcla ${MCLA_SOURCES})
endif()

rexglue_setup_target(mcla)
