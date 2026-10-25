# mcla - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(mcla LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MCLA_BUILD_DIAGNOSTIC_TESTS "Expose nonshipping pure diagnostic tests" OFF)
option(MCLA_BUILD_IMAGE_CAPTURE_TESTS "Expose experimental nonshipping image helper/client" OFF)
option(MCLA_EXPERIMENTAL_F10_WGC "Private Windows F10 image-only WGC candidate (unaccepted)" OFF)
option(MCLA_TEST_BUILD_EXPIRY "Require signed 14-day test ticket before Windows host startup" OFF)
if(MCLA_EXPERIMENTAL_F10_WGC AND NOT WIN32)
    message(FATAL_ERROR "Experimental F10 WGC is Windows-only")
endif()
if((MCLA_BUILD_IMAGE_CAPTURE_TESTS OR MCLA_EXPERIMENTAL_F10_WGC) AND WIN32)
    set(MCLA_CPPWINRT_INCLUDE_DIR "" CACHE PATH "Windows SDK cppwinrt include directory")
    if(NOT EXISTS "${MCLA_CPPWINRT_INCLUDE_DIR}/winrt/Windows.Graphics.Capture.h")
        message(FATAL_ERROR "Set MCLA_CPPWINRT_INCLUDE_DIR to the installed Windows SDK cppwinrt directory")
    endif()
    add_executable(mcla_diagnostic_image_helper WIN32 EXCLUDE_FROM_ALL
        src/mcla_diagnostic_image_helper_win.cpp)
    target_include_directories(mcla_diagnostic_image_helper SYSTEM PRIVATE "${MCLA_CPPWINRT_INCLUDE_DIR}")
    target_link_libraries(mcla_diagnostic_image_helper PRIVATE windowsapp d3d11 dxgi dwmapi user32)
    target_link_options(mcla_diagnostic_image_helper PRIVATE "LINKER:/ENTRY:wmainCRTStartup")
    add_library(mcla_diagnostic_image_client STATIC EXCLUDE_FROM_ALL
        src/mcla_diagnostic_image_client_win.cpp)
    target_link_libraries(mcla_diagnostic_image_client PRIVATE dwmapi user32 bcrypt)
    add_executable(mcla_diagnostic_image_fake_helper EXCLUDE_FROM_ALL
        tests/mcla_diagnostic_image_fake_helper.cpp)
    target_include_directories(mcla_diagnostic_image_fake_helper PRIVATE src)
    target_link_options(mcla_diagnostic_image_fake_helper PRIVATE "LINKER:/ENTRY:wmainCRTStartup")
    add_executable(mcla_diagnostic_image_client_test EXCLUDE_FROM_ALL
        tests/mcla_diagnostic_image_client_test.cpp src/mcla_diagnostic_image_client_win.cpp)
    target_include_directories(mcla_diagnostic_image_client_test PRIVATE src)
    target_compile_definitions(mcla_diagnostic_image_client_test PRIVATE MCLA_IMAGE_CLIENT_TEST_SEAM=1)
    target_link_libraries(mcla_diagnostic_image_client_test PRIVATE dwmapi user32 bcrypt)
    add_executable(mcla_diagnostic_image_fixture EXCLUDE_FROM_ALL
        tests/mcla_diagnostic_image_fixture_win.cpp)
    target_include_directories(mcla_diagnostic_image_fixture PRIVATE src)
    target_link_options(mcla_diagnostic_image_fixture PRIVATE "LINKER:/ENTRY:wmainCRTStartup")
    target_link_libraries(mcla_diagnostic_image_fixture PRIVATE
        mcla_diagnostic_image_client dwmapi user32 gdi32)
endif()
if(MCLA_BUILD_DIAGNOSTIC_TESTS)
    add_executable(mcla_diagnostic_image_protocol_test EXCLUDE_FROM_ALL
        tests/mcla_diagnostic_image_protocol_test.cpp)
    target_include_directories(mcla_diagnostic_image_protocol_test PRIVATE src)
    add_executable(mcla_diagnostic_hotkey_test EXCLUDE_FROM_ALL
        tests/mcla_diagnostic_hotkey_test.cpp)
    target_include_directories(mcla_diagnostic_hotkey_test PRIVATE src)
    target_compile_features(mcla_diagnostic_hotkey_test PRIVATE cxx_std_23)
    add_executable(mcla_frame_signal_test EXCLUDE_FROM_ALL
        tests/mcla_frame_signal_test.cpp)
    target_include_directories(mcla_frame_signal_test PRIVATE src)
    target_compile_features(mcla_frame_signal_test PRIVATE cxx_std_23)
    add_executable(mcla_portable_path_policy_test EXCLUDE_FROM_ALL
        tests/mcla_portable_path_policy_test.cpp)
    target_include_directories(mcla_portable_path_policy_test PRIVATE src)
    target_compile_features(mcla_portable_path_policy_test PRIVATE cxx_std_23)
    add_executable(mcla_launcher_profile_policy_test EXCLUDE_FROM_ALL
        tests/mcla_launcher_profile_policy_test.cpp)
    target_include_directories(mcla_launcher_profile_policy_test PRIVATE src)
    target_compile_features(mcla_launcher_profile_policy_test PRIVATE cxx_std_23)
    foreach(policy_test IN ITEMS launcher_library_codec launcher_preferences managed_config_policy)
        add_executable(mcla_${policy_test}_test EXCLUDE_FROM_ALL tests/mcla_${policy_test}_test.cpp)
        target_include_directories(mcla_${policy_test}_test PRIVATE src)
    endforeach()
    add_executable(mcla_portable_adapter_policy_test EXCLUDE_FROM_ALL
        tests/mcla_portable_adapter_policy_test.cpp)
    target_include_directories(mcla_portable_adapter_policy_test PRIVATE src)
    target_compile_features(mcla_portable_adapter_policy_test PRIVATE cxx_std_23)
    if(WIN32)
        add_executable(mcla_portable_child_fixture EXCLUDE_FROM_ALL
            tests/mcla_portable_child_fixture_win.cpp)
        target_compile_features(mcla_portable_child_fixture PRIVATE cxx_std_23)
    endif()
    add_executable(mcla_readback_cost_sample_test EXCLUDE_FROM_ALL
        tests/mcla_readback_cost_sample_test.cpp)
    target_include_directories(mcla_readback_cost_sample_test PRIVATE src)
    target_compile_features(mcla_readback_cost_sample_test PRIVATE cxx_std_23)
    add_executable(mcla_cpp_runtime_probe EXCLUDE_FROM_ALL
        tests/mcla_cpp_runtime_probe.cpp)
    target_compile_features(mcla_cpp_runtime_probe PRIVATE cxx_std_23)
    if(WIN32)
        # Match Release consumers: /MD, never a static-CRT false positive.
        set_property(TARGET mcla_cpp_runtime_probe PROPERTY
            MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
endif()

set(MCLA_REXGLUE_VERSION "0.20.0.0")
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

if(MCLA_BUILD_DIAGNOSTIC_TESTS AND WIN32)
    add_executable(mcla_frame_audit_rearm_test EXCLUDE_FROM_ALL
        tests/mcla_frame_audit_rearm_test.cpp)
    target_include_directories(mcla_frame_audit_rearm_test PRIVATE src)
    target_link_libraries(mcla_frame_audit_rearm_test PRIVATE rex::runtime)
    add_custom_command(TARGET mcla_frame_audit_rearm_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:mcla_frame_audit_rearm_test>
            $<TARGET_FILE_DIR:mcla_frame_audit_rearm_test>
        COMMAND_EXPAND_LISTS)
endif()

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
    src/mcla_diagnostics.cpp
    src/mcla_logging.cpp
)

# Measured-time game timer (docs/evidence/M8-001-timebox-20261025.md). It
# overrides sub_822D2478, so it stays out of builds where the diagnostic timer
# configuration probe owns that function.
option(MCLA_RAW_TIMER_DEFAULT_ON "Default the measured-time game timer ON" ON)
if(NOT MCLA_TIMER_CONFIG_PROBE)
    list(APPEND MCLA_SOURCES src/mcla_raw_timer.cpp)
    if(NOT MCLA_RAW_TIMER_DEFAULT_ON)
        set_source_files_properties(src/mcla_raw_timer.cpp PROPERTIES
            COMPILE_DEFINITIONS MCLA_RAW_TIMER_DEFAULT=0)
    endif()
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" MCLA_VERSION)
string(STRIP "${MCLA_VERSION}" MCLA_VERSION)

if(WIN32)
    list(APPEND MCLA_SOURCES src/mcla_native_crash_win.cpp)
else()
    list(APPEND MCLA_SOURCES src/mcla_native_crash_stub.cpp)
endif()

if(WIN32)
    add_executable(mcla WIN32 ${MCLA_SOURCES})
else()
    add_executable(mcla ${MCLA_SOURCES})
endif()

rexglue_setup_target(mcla GPU_PLUGINS xenos)
target_compile_definitions(mcla PRIVATE MCLA_VERSION="${MCLA_VERSION}")
if(MCLA_EXPERIMENTAL_F10_WGC)
    set(MCLA_IMAGE_PIN_HEADER "${CMAKE_CURRENT_BINARY_DIR}/mcla_image_helper_pin.h")
    add_custom_command(OUTPUT "${MCLA_IMAGE_PIN_HEADER}"
        COMMAND ${CMAKE_COMMAND}
            "-DINPUT=$<TARGET_FILE:mcla_diagnostic_image_helper>"
            "-DOUTPUT=${MCLA_IMAGE_PIN_HEADER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/write-image-helper-pin.cmake"
        DEPENDS mcla_diagnostic_image_helper "${CMAKE_CURRENT_SOURCE_DIR}/scripts/write-image-helper-pin.cmake"
        VERBATIM)
    target_sources(mcla PRIVATE "${MCLA_IMAGE_PIN_HEADER}")
    set_source_files_properties(src/mcla_diagnostics.cpp PROPERTIES
        INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}"
        COMPILE_DEFINITIONS MCLA_EXPERIMENTAL_F10_WGC=1)
    target_link_libraries(mcla PRIVATE mcla_diagnostic_image_client)
endif()

if(WIN32)
    target_link_libraries(mcla PRIVATE psapi bcrypt gdi32 user32)
    add_library(mcla_build_lease STATIC
        src/mcla_build_lease_policy.cpp src/mcla_build_lease_network_win.cpp
        src/mcla_build_lease_runtime_win.cpp src/mcla_launcher_fs_win.cpp)
    target_include_directories(mcla_build_lease PUBLIC src)
    target_link_libraries(mcla_build_lease PUBLIC bcrypt winhttp shell32 ole32 uuid)
    if(MCLA_TEST_BUILD_EXPIRY)
        target_compile_definitions(mcla PRIVATE MCLA_TEST_BUILD_EXPIRY=1)
        target_link_libraries(mcla PRIVATE mcla_build_lease)
    endif()
    add_executable(mcla_crash_handler WIN32
        src/mcla_crash_handler_win.cpp
    )
    target_compile_features(mcla_crash_handler PRIVATE cxx_std_23)
    target_link_libraries(mcla_crash_handler PRIVATE dbghelp bcrypt shell32 user32)
    add_dependencies(mcla mcla_crash_handler)

    add_executable(mcla_portable_launcher
        src/mcla_portable_launcher_win.cpp
    )
    set_target_properties(mcla_portable_launcher PROPERTIES
        OUTPUT_NAME "Launch-MCLA")
    target_compile_features(mcla_portable_launcher PRIVATE cxx_std_23)
    target_link_libraries(mcla_portable_launcher PRIVATE bcrypt user32 dxgi d3d12)
    add_dependencies(mcla mcla_portable_launcher)

    set(MCLA_LAUNCHER_SOURCES
        src/mcla_launcher_controller_win.cpp src/mcla_launcher_ui_win.cpp
        src/mcla_launcher_store_win.cpp src/mcla_launcher_fs_win.cpp
        src/mcla_launcher_writable_root_win.cpp src/mcla_launcher_process_win.cpp
        src/mcla_launcher_snapshot_win.cpp src/mcla_launcher_build_win.cpp
        src/mcla_launcher_game_win.cpp)
    add_executable(mcla_internal_launcher WIN32 src/mcla_launcher_main_win.cpp ${MCLA_LAUNCHER_SOURCES})
    set_target_properties(mcla_internal_launcher PROPERTIES OUTPUT_NAME "MCLA-Launcher")
    target_include_directories(mcla_internal_launcher PRIVATE src
        third_party/rexglue-sdk/thirdparty/inja/third_party/include)
    target_link_libraries(mcla_internal_launcher PRIVATE bcrypt user32 gdi32 shell32 ole32 uuid dxgi d3d12)
    target_link_libraries(mcla_internal_launcher PRIVATE mcla_build_lease)

    if(MCLA_BUILD_DIAGNOSTIC_TESTS)
        add_executable(mcla_build_lease_policy_test EXCLUDE_FROM_ALL tests/mcla_build_lease_policy_test.cpp)
        target_link_libraries(mcla_build_lease_policy_test PRIVATE mcla_build_lease)
        add_executable(mcla_build_lease_network_win_test EXCLUDE_FROM_ALL tests/mcla_build_lease_network_win_test.cpp)
        target_link_libraries(mcla_build_lease_network_win_test PRIVATE mcla_build_lease)
        add_executable(mcla_build_lease_runtime_win_test EXCLUDE_FROM_ALL
            tests/mcla_build_lease_runtime_win_test.cpp src/mcla_build_lease_runtime_win.cpp)
        target_compile_definitions(mcla_build_lease_runtime_win_test PRIVATE MCLA_BUILD_LEASE_RUNTIME_TESTING)
        target_link_libraries(mcla_build_lease_runtime_win_test PRIVATE mcla_build_lease)
        add_executable(mcla_build_lease_probe_win EXCLUDE_FROM_ALL tests/mcla_build_lease_probe_win.cpp)
        target_link_libraries(mcla_build_lease_probe_win PRIVATE mcla_build_lease)
        add_executable(mcla_launcher_fs_win_test EXCLUDE_FROM_ALL
            tests/mcla_launcher_fs_win_test.cpp src/mcla_launcher_fs_win.cpp)
        target_include_directories(mcla_launcher_fs_win_test PRIVATE src)
        target_link_libraries(mcla_launcher_fs_win_test PRIVATE bcrypt)
        add_executable(mcla_launcher_ui_win_test EXCLUDE_FROM_ALL
            tests/mcla_launcher_ui_win_test.cpp src/mcla_launcher_ui_win.cpp)
        target_include_directories(mcla_launcher_ui_win_test PRIVATE src)
        target_link_libraries(mcla_launcher_ui_win_test PRIVATE user32 gdi32 ole32 uuid)
        add_executable(mcla_launcher_process_win_test EXCLUDE_FROM_ALL
            tests/mcla_launcher_process_win_test.cpp src/mcla_launcher_process_win.cpp)
        target_include_directories(mcla_launcher_process_win_test PRIVATE src)
        target_compile_definitions(mcla_launcher_process_win_test PRIVATE MCLA_LAUNCHER_PROCESS_TEST_SEAM)
        add_executable(mcla_launcher_store_win_test EXCLUDE_FROM_ALL
            tests/mcla_launcher_store_win_test.cpp src/mcla_launcher_store_win.cpp src/mcla_launcher_fs_win.cpp src/mcla_launcher_writable_root_win.cpp)
        target_include_directories(mcla_launcher_store_win_test PRIVATE src)
        target_link_libraries(mcla_launcher_store_win_test PRIVATE bcrypt shell32 ole32 uuid)
        add_executable(mcla_launcher_build_win_test EXCLUDE_FROM_ALL
            tests/mcla_launcher_build_win_test.cpp src/mcla_launcher_build_win.cpp src/mcla_launcher_fs_win.cpp)
        target_include_directories(mcla_launcher_build_win_test PRIVATE src)
        target_link_libraries(mcla_launcher_build_win_test PRIVATE bcrypt)
        add_executable(mcla_launcher_snapshot_win_test EXCLUDE_FROM_ALL
            tests/mcla_launcher_snapshot_win_test.cpp src/mcla_launcher_snapshot_win.cpp src/mcla_launcher_fs_win.cpp)
        target_include_directories(mcla_launcher_snapshot_win_test PRIVATE src)
        target_compile_definitions(mcla_launcher_snapshot_win_test PRIVATE MCLA_LAUNCHER_SNAPSHOT_TEST_SEAM)
        target_link_libraries(mcla_launcher_snapshot_win_test PRIVATE bcrypt)
        add_executable(mcla_launcher_game_win_test EXCLUDE_FROM_ALL
            tests/mcla_launcher_game_win_test.cpp src/mcla_launcher_game_win.cpp src/mcla_launcher_fs_win.cpp)
        target_include_directories(mcla_launcher_game_win_test PRIVATE src)
        target_compile_definitions(mcla_launcher_game_win_test PRIVATE MCLA_LAUNCHER_GAME_TESTING)
        target_link_libraries(mcla_launcher_game_win_test PRIVATE bcrypt)
        add_executable(mcla_launcher_import_policy_test EXCLUDE_FROM_ALL tests/mcla_launcher_import_policy_test.cpp)
        add_executable(mcla_launcher_capabilities_test EXCLUDE_FROM_ALL tests/mcla_launcher_capabilities_test.cpp)
        target_include_directories(mcla_launcher_capabilities_test PRIVATE src)
        target_include_directories(mcla_launcher_import_policy_test PRIVATE src
            third_party/rexglue-sdk/thirdparty/inja/third_party/include)
        add_executable(mcla_launcher_controller_win_test EXCLUDE_FROM_ALL
            tests/mcla_launcher_controller_win_test.cpp ${MCLA_LAUNCHER_SOURCES})
        target_include_directories(mcla_launcher_controller_win_test PRIVATE src
            third_party/rexglue-sdk/thirdparty/inja/third_party/include)
        target_compile_definitions(mcla_launcher_controller_win_test PRIVATE
            MCLA_LAUNCHER_CONTROLLER_TESTING MCLA_LAUNCHER_GAME_TESTING)
        target_link_libraries(mcla_launcher_controller_win_test PRIVATE bcrypt user32 gdi32 shell32 ole32 uuid dxgi d3d12)
        target_link_libraries(mcla_launcher_controller_win_test PRIVATE mcla_build_lease)
        add_executable(mcla_launcher_host_probe_win EXCLUDE_FROM_ALL
            tests/mcla_launcher_host_probe_win.cpp ${MCLA_LAUNCHER_SOURCES})
        target_include_directories(mcla_launcher_host_probe_win PRIVATE src
            third_party/rexglue-sdk/thirdparty/inja/third_party/include)
        target_compile_definitions(mcla_launcher_host_probe_win PRIVATE MCLA_LAUNCHER_HOST_LIFECYCLE_TEST)
        target_link_libraries(mcla_launcher_host_probe_win PRIVATE bcrypt user32 gdi32 shell32 ole32 uuid dxgi d3d12)
        target_link_libraries(mcla_launcher_host_probe_win PRIVATE mcla_build_lease)
        add_executable(mcla_render_target_probe_win EXCLUDE_FROM_ALL
            tests/mcla_render_target_probe_win.cpp
            src/mcla_launcher_build_win.cpp src/mcla_launcher_game_win.cpp
            src/mcla_launcher_process_win.cpp src/mcla_launcher_fs_win.cpp)
        target_include_directories(mcla_render_target_probe_win PRIVATE src)
        target_link_libraries(mcla_render_target_probe_win PRIVATE
            mcla_build_lease bcrypt user32 dxgi d3d12)
        add_executable(mcla_launcher_import_win_test EXCLUDE_FROM_ALL
            tests/mcla_launcher_import_win_test.cpp ${MCLA_LAUNCHER_SOURCES})
        target_include_directories(mcla_launcher_import_win_test PRIVATE src
            third_party/rexglue-sdk/thirdparty/inja/third_party/include)
        target_link_libraries(mcla_launcher_import_win_test PRIVATE bcrypt user32 gdi32 shell32 ole32 uuid dxgi d3d12)
        target_link_libraries(mcla_launcher_import_win_test PRIVATE mcla_build_lease)
        add_executable(mcla_launcher_ui_render_win EXCLUDE_FROM_ALL
            tests/mcla_launcher_ui_render_win.cpp src/mcla_launcher_ui_win.cpp)
        target_include_directories(mcla_launcher_ui_render_win PRIVATE src)
        target_link_libraries(mcla_launcher_ui_render_win PRIVATE user32 gdi32 ole32 uuid gdiplus)
    endif()
endif()

# Ship a discoverable host-configuration template without overwriting the
# user's mcla.toml or changing command-line-driven test roots.
add_custom_command(TARGET mcla POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/mcla.toml.example"
        "$<TARGET_FILE_DIR:mcla>/mcla.toml.example"
    VERBATIM)
