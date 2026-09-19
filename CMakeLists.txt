cmake_minimum_required(VERSION 3.22)

project(RIPPLES VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# JUCE
# Prefer a local checkout (fast, offline); fall back to fetching 9.0.2.
# ---------------------------------------------------------------------------
set(RIPPLES_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout")

if(RIPPLES_JUCE_DIR AND EXISTS "${RIPPLES_JUCE_DIR}/CMakeLists.txt")
    message(STATUS "RIPPLES: using local JUCE at ${RIPPLES_JUCE_DIR}")
    add_subdirectory(${RIPPLES_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
    message(STATUS "RIPPLES: using sibling JUCE checkout")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    message(STATUS "RIPPLES: fetching JUCE 9.0.2")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.2
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Plug-in target
# ---------------------------------------------------------------------------
set(RIPPLES_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND RIPPLES_FORMATS AU)
endif()

juce_add_plugin(RIPPLES
    PRODUCT_NAME                "RIPPLES"
    COMPANY_NAME                "Classa Music"
    BUNDLE_ID                   "com.classamusic.ripples"
    PLUGIN_MANUFACTURER_CODE    Clsa
    PLUGIN_CODE                 Rppl
    FORMATS                     ${RIPPLES_FORMATS}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             "Instrument" "Synth"
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    DESCRIPTION                 "RIPPLES — dive into sound")

# Sources are globbed so that modules can be added without editing this file.
file(GLOB_RECURSE RIPPLES_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h")

target_sources(RIPPLES PRIVATE ${RIPPLES_SOURCES})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" FILES ${RIPPLES_SOURCES})

target_include_directories(RIPPLES PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_compile_definitions(RIPPLES PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0)

# Linux CI containers commonly lack ALSA/JACK headers; the plug-in formats do
# not need them, and the standalone falls back to whatever backend is present.
if(UNIX AND NOT APPLE)
    find_package(PkgConfig QUIET)
    set(RIPPLES_HAVE_ALSA 0)
    if(PkgConfig_FOUND)
        pkg_check_modules(ALSA QUIET alsa)
        if(ALSA_FOUND)
            set(RIPPLES_HAVE_ALSA 1)
        endif()
    endif()
    if(NOT RIPPLES_HAVE_ALSA)
        message(STATUS "RIPPLES: ALSA not found — building without ALSA support")
        target_compile_definitions(RIPPLES PUBLIC JUCE_ALSA=0 JUCE_JACK=0)
    endif()
endif()

juce_generate_juce_header(RIPPLES)

target_link_libraries(RIPPLES PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Denormals are a real cost in the reverb / resonator feedback paths.
if(MSVC)
    target_compile_options(RIPPLES PRIVATE /fp:fast)
else()
    target_compile_options(RIPPLES PRIVATE -ffast-math -fno-finite-math-only)
endif()

# ---------------------------------------------------------------------------
# Offline render harness — lets the synth be measured, not just compiled.
# ---------------------------------------------------------------------------
option(RIPPLES_BUILD_TESTS "Build the offline render harness" ON)

if(RIPPLES_BUILD_TESTS)
    juce_add_console_app(RIPPLES_Render PRODUCT_NAME "RIPPLES_Render")

    target_sources(RIPPLES_Render PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tests/RenderHarness.cpp")

    target_include_directories(RIPPLES_Render PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

    target_compile_definitions(RIPPLES_Render PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_MODAL_LOOPS_PERMITTED=0)

    target_link_libraries(RIPPLES_Render PRIVATE
        RIPPLES
        juce::juce_recommended_config_flags)
endif()
