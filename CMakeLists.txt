cmake_minimum_required(VERSION 3.22)
project(STEINEX_TransitFX VERSION 1.0.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(STEINEX_BUILD_TESTS "Build the DSP / UI self-test" OFF)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
elseif(DEFINED ENV{JUCE_DIR} AND EXISTS "$ENV{JUCE_DIR}/CMakeLists.txt")
    add_subdirectory($ENV{JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                              GIT_TAG 8.0.4 GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(TransitFX
    COMPANY_NAME "STEINEX"
    PLUGIN_MANUFACTURER_CODE Stnx
    PLUGIN_CODE Trfx
    FORMATS VST3
    PRODUCT_NAME "STEINEX TRANSIT FX"
    PLUGIN_NAME "TRANSIT FX"
    BUNDLE_ID com.steinex.transitfx
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD FALSE)

set(STEINEX_SOURCES
    Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/CrashLog.cpp)

target_sources(TransitFX PRIVATE ${STEINEX_SOURCES})
target_compile_definitions(TransitFX PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(TransitFX PRIVATE
    juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)

if(MSVC)
    target_link_options(TransitFX PRIVATE /MAP)
endif()

if(MINGW)
    target_link_options(TransitFX PRIVATE -static -static-libgcc -static-libstdc++)
endif()

if(STEINEX_BUILD_TESTS)
    juce_add_console_app(TransitFXTest PRODUCT_NAME "TransitFXTest")
    target_sources(TransitFXTest PRIVATE Tests/SelfTest.cpp ${STEINEX_SOURCES})
    target_compile_definitions(TransitFXTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=0)
    target_link_libraries(TransitFXTest PRIVATE
        juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp juce::juce_gui_basics
        juce::juce_recommended_config_flags)
endif()
