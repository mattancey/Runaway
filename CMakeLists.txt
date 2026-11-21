cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS deployment target")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build Universal Binary for macOS")
set(CMAKE_XCODE_ATTRIBUTE_LLVM_LTO "NO")

project(Runaway VERSION 0.5.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.14
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Runaway
    COMPANY_NAME                "RunawayAudio"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    PLUGIN_MANUFACTURER_CODE    Runa
    PLUGIN_CODE                 Rnwy
    FORMATS                     AU VST3 Standalone
    PRODUCT_NAME                "Runaway"
    BUNDLE_ID                   "com.runawayaudio.runaway"
    AU_MAIN_TYPE                "kAudioUnitType_MusicEffect"
)

target_sources(Runaway
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/DSP/WaveshaperEngine.h
)

target_compile_definitions(Runaway
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Runaway
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
