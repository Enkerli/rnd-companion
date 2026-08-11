cmake_minimum_required(VERSION 3.22)
project(RndCompanion VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

option(RND_BUILD_APP   "Build the standalone app (needs JUCE)" ON)
option(RND_BUILD_TESTS "Build the protocol tests (no JUCE needed)" ON)
option(RND_BUILD_PROBE "Build the SysEx passthrough probe plugin (needs enkerli-juce)" ON)

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

# ── JUCE ─────────────────────────────────────────────────────────────────────
if(RND_BUILD_APP OR RND_BUILD_PROBE)
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
endif()

# ── enkerli-juce: the suite's plugin archetypes ──────────────────────────────
if(RND_BUILD_APP OR RND_BUILD_PROBE)
    set(ENKERLI_JUCE_PATH "${CMAKE_SOURCE_DIR}/enkerli-juce" CACHE PATH "Path to enkerli-juce")
    if(NOT EXISTS "${ENKERLI_JUCE_PATH}/cmake/EnkerliPlugin.cmake" AND EXISTS "$ENV{ENKERLI_JUCE_PATH}/cmake/EnkerliPlugin.cmake")
        set(ENKERLI_JUCE_PATH "$ENV{ENKERLI_JUCE_PATH}")
    endif()
    if(NOT EXISTS "${ENKERLI_JUCE_PATH}/cmake/EnkerliPlugin.cmake" AND EXISTS "${CMAKE_SOURCE_DIR}/../enkerli-juce/cmake/EnkerliPlugin.cmake")
        set(ENKERLI_JUCE_PATH "${CMAKE_SOURCE_DIR}/../enkerli-juce")
    endif()

    if(EXISTS "${ENKERLI_JUCE_PATH}/cmake/EnkerliPlugin.cmake")
        message(STATUS "Using enkerli-juce at ${ENKERLI_JUCE_PATH}")

        # clap-juce-extensions lives beside the suite repos, not inside them.
        if(NOT EXISTS "${CMAKE_SOURCE_DIR}/clap-juce-extensions/CMakeLists.txt"
           AND EXISTS "${CMAKE_SOURCE_DIR}/../clap-juce-extensions/CMakeLists.txt")
            set(CLAP_JUCE_PATH "${CMAKE_SOURCE_DIR}/../clap-juce-extensions" CACHE PATH "Path to clap-juce-extensions")
        endif()

        include("${ENKERLI_JUCE_PATH}/cmake/EnkerliPlugin.cmake")
    endif()
endif()

# ── RND Companion ────────────────────────────────────────────────────────────
# A plugin, not a plain app. The host tests decided this: AUM passes SysEx to an
# AUv3 in both directions, so the companion belongs inside the host on iPadOS.
# The archetype's Standalone build replaces the old gui_app on desktop.
if(RND_BUILD_APP)
    if(NOT EXISTS "${ENKERLI_JUCE_PATH}/cmake/EnkerliPlugin.cmake")
        message(WARNING "enkerli-juce not found - skipping RND Companion.")
    else()
        enkerli_add_midi_effect_plugin(RndCompanion
            PRODUCT_NAME "RND Companion"
            PLUGIN_CODE  Rndc
            BUNDLE_ID    "com.enkerli.RndCompanion"
            VERSION      "0.1.0"
            ICON_BIG     "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.png"
            DESCRIPTION  "Capture, send and curate patch seeds on a Cymaforma RND Synth")

        juce_generate_juce_header(RndCompanion)

        # Build stamp, regenerated every build so the UI can say which build is
        # running (F2 of the 2026-08-11 design audit).
        set(_stamp "${CMAKE_CURRENT_BINARY_DIR}/BuildStamp.h")
        add_custom_command(OUTPUT "${_stamp}"
            COMMAND ${CMAKE_COMMAND} -DOUTPUT=${_stamp} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenBuildStamp.cmake"
            COMMENT "Stamping build" VERBATIM)
        add_custom_target(RndCompanionBuildStamp DEPENDS "${_stamp}")
        add_dependencies(RndCompanion RndCompanionBuildStamp)
        target_include_directories(RndCompanion PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

        target_sources(RndCompanion PRIVATE
            Source/Plugin/CompanionProcessor.cpp
            Source/Plugin/CompanionEditor.cpp
            Source/App/CompanionModel.cpp
            Source/App/CompanionView.cpp
            Source/App/DeviceLink.cpp
            Source/App/RndCommands.cpp
            Source/App/SeedLibrary.cpp
            Source/App/SharedFrame.cpp
            Source/App/SuiteTheme.cpp)

        # enkerli-juce's file helpers: a plain FileChooser does nothing inside an
        # AUv3 app extension. On iOS these become a share sheet and a document
        # picker presented from the responder chain (device-verified in AUM).
        target_include_directories(RndCompanion PRIVATE "${ENKERLI_JUCE_PATH}/src")

        if(APPLE)
            target_sources(RndCompanion PRIVATE
                "${ENKERLI_JUCE_PATH}/src/FileExport.mm"
                "${ENKERLI_JUCE_PATH}/src/FileImport.mm")
        endif()

        target_link_libraries(RndCompanion PRIVATE
            rnd_protocol
            juce::juce_audio_utils
            juce::juce_gui_extra
            PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
    endif()
endif()


# ── SysEx passthrough probe ──────────────────────────────────────────────────
# A throwaway-shaped but permanent diagnostic: the plugin that answers "does
# this host pass SysEx?" for each format. Built on the suite archetype so the
# answer is about SysEx and not about our project settings.
if(RND_BUILD_PROBE)
    if(NOT EXISTS "${ENKERLI_JUCE_PATH}/cmake/EnkerliPlugin.cmake")
        message(WARNING "enkerli-juce not found - skipping the probe. Set -DENKERLI_JUCE_PATH=...")
    else()
        enkerli_add_midi_effect_plugin(RndSysExProbe
            PRODUCT_NAME "RND SysEx Probe"
            PLUGIN_CODE  Rsxp
            BUNDLE_ID    "com.enkerli.RndSysExProbe"
            VERSION      "0.1.0"
            DESCRIPTION  "Checks whether a host passes RND SysEx in and out of a plugin")

        juce_generate_juce_header(RndSysExProbe)

        target_sources(RndSysExProbe PRIVATE
            Source/Probe/ProbeProcessor.cpp
            Source/Probe/ProbeEditor.cpp)

        target_link_libraries(RndSysExProbe PRIVATE
            rnd_protocol
            juce::juce_audio_utils
            juce::juce_gui_extra
            PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
    endif()
endif()
