cmake_minimum_required(VERSION 3.22)
project(RndCompanion VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

option(RND_BUILD_APP   "Build the standalone app (needs JUCE)" ON)
option(RND_BUILD_TESTS "Build the protocol tests (no JUCE needed)" ON)

# ── rnd_protocol ─────────────────────────────────────────────────────────────
# The codec, deliberately free of JUCE and of I/O so the plugin shells and a
# future WASM build can share it unchanged. Nothing platform-specific belongs
# in here.
add_library(rnd_protocol STATIC
    Source/Protocol/RndProtocol.cpp
    Source/Protocol/RndProtocol.h)

target_include_directories(rnd_protocol PUBLIC Source/Protocol)
set_target_properties(rnd_protocol PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(rnd_protocol PRIVATE /W4)
else()
    target_compile_options(rnd_protocol PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Tests ────────────────────────────────────────────────────────────────────
if(RND_BUILD_TESTS)
    enable_testing()

    add_executable(rnd_protocol_tests Tests/ProtocolTests.cpp Tests/SmfScanner.h)
    target_link_libraries(rnd_protocol_tests PRIVATE rnd_protocol)
    target_compile_definitions(rnd_protocol_tests PRIVATE
        RND_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Tests/fixtures")

    if(NOT MSVC)
        target_compile_options(rnd_protocol_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME protocol COMMAND rnd_protocol_tests)
endif()

# ── Standalone app ───────────────────────────────────────────────────────────
if(RND_BUILD_APP)
    # Same JUCE resolution as the rest of the suite: installed package, then a
    # local checkout (JUCE_PATH env or /Applications/JUCE), then FetchContent.
    find_package(JUCE CONFIG QUIET)

    if(NOT JUCE_FOUND)
        set(JUCE_PATH "${CMAKE_SOURCE_DIR}/JUCE" CACHE PATH "Path to a JUCE checkout")

        if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt" AND EXISTS "$ENV{JUCE_PATH}/CMakeLists.txt")
            set(JUCE_PATH "$ENV{JUCE_PATH}")
        endif()
        if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt" AND EXISTS "/Applications/JUCE/CMakeLists.txt")
            set(JUCE_PATH "/Applications/JUCE")
        endif()

        if(EXISTS "${JUCE_PATH}/CMakeLists.txt")
            message(STATUS "Using JUCE at ${JUCE_PATH}")
            add_subdirectory("${JUCE_PATH}" JUCE)
        else()
            message(STATUS "JUCE not found locally - fetching juce-framework/JUCE 8.0.13")
            include(FetchContent)
            FetchContent_Declare(JUCE
                GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                GIT_TAG 8.0.13
                GIT_SHALLOW TRUE)
            FetchContent_MakeAvailable(JUCE)
        endif()
    endif()

    juce_add_gui_app(RndCompanion
        PRODUCT_NAME     "RND Companion"
        COMPANY_NAME     "Enkerli"
        BUNDLE_ID        "com.enkerli.RndCompanion"
        # CoreMIDI on iOS needs the background mode; harmless elsewhere.
        MICROPHONE_PERMISSION_ENABLED FALSE)

    target_sources(RndCompanion PRIVATE
        Source/Standalone/Main.cpp
        Source/Standalone/MainComponent.cpp
        Source/Standalone/DeviceLink.cpp
        Source/Standalone/SeedLibrary.cpp)

    target_compile_definitions(RndCompanion PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:RndCompanion,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:RndCompanion,JUCE_VERSION>")

    target_link_libraries(RndCompanion PRIVATE
        rnd_protocol
        juce::juce_audio_devices
        juce::juce_data_structures
        juce::juce_gui_extra
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
endif()
