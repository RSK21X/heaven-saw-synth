cmake_minimum_required(VERSION 3.22)
project(HeavenSawSynth VERSION 1.2.1)

set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "Apple Silicon only" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0 CACHE STRING "Minimum macOS version" FORCE)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.12
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(HeavenSawSynth
  COMPANY_NAME "Heaven Saw"
  BUNDLE_ID "audio.heavensaw.synth"
  PLUGIN_MANUFACTURER_CODE Hvsw
  PLUGIN_CODE Hsaw
  FORMATS VST3
  PRODUCT_NAME "Heaven Saw Synth"
  DESCRIPTION "Play. Instant heaven."
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Instrument Synth)
juce_generate_juce_header(HeavenSawSynth)

target_sources(HeavenSawSynth PRIVATE
  Source/PluginProcessor.cpp
  Source/SynthEngine.cpp
  Source/PluginEditor.cpp)

target_compile_features(HeavenSawSynth PRIVATE cxx_std_17)
target_compile_definitions(HeavenSawSynth PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(HeavenSawSynth PRIVATE
  juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags
         juce::juce_recommended_lto_flags
         juce::juce_recommended_warning_flags)

# JUCE generates moduleinfo.json after its initial bundle signing step.
# Seal the complete development bundle after that resource has been written.
if(APPLE)
  add_custom_command(TARGET HeavenSawSynth_VST3 POST_BUILD
    COMMAND /usr/bin/codesign --force --sign - "$<TARGET_BUNDLE_DIR:HeavenSawSynth_VST3>"
    COMMAND /usr/bin/codesign --verify --deep --strict "$<TARGET_BUNDLE_DIR:HeavenSawSynth_VST3>"
    VERBATIM)
endif()

option(HEAVEN_BUILD_AUDIO_SMOKE "Build the local DSP smoke check" OFF)
if(HEAVEN_BUILD_AUDIO_SMOKE)
  juce_add_console_app(HeavenSawSmoke PRODUCT_NAME "Heaven Saw Audio Smoke")
  juce_generate_juce_header(HeavenSawSmoke)
  target_sources(HeavenSawSmoke PRIVATE
    tests/audio_smoke.cpp
    Source/PluginProcessor.cpp
    Source/SynthEngine.cpp
    Source/PluginEditor.cpp)
  target_compile_features(HeavenSawSmoke PRIVATE cxx_std_17)
  target_compile_definitions(HeavenSawSmoke PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JucePlugin_Name="Heaven Saw Synth"
    JucePlugin_VersionString="${PROJECT_VERSION}")
  target_link_libraries(HeavenSawSmoke PRIVATE
    juce::juce_audio_utils juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
endif()
