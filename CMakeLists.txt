cmake_minimum_required(VERSION 3.22)
project(XUM VERSION 1.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(XUM
    COMPANY_NAME "Tapelab"
    PLUGIN_MANUFACTURER_CODE Tape
    PLUGIN_CODE Xum1
    FORMATS VST3
    PRODUCT_NAME "XUM"
    DESCRIPTION "XUM by Tapelab"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(XUM PRIVATE
    Source/Parameters.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(XUM PRIVATE Source)

target_compile_definitions(XUM PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(XUM
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
