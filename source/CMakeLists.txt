cmake_minimum_required(VERSION 3.25)
project(Dusk VERSION 0.21.5 LANGUAGES C CXX)

set(JUCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../_Tools/JUCE/_9.0.4")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE 9.0.4 not found at: ${JUCE_DIR}")
endif()
add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")

juce_add_plugin(Dusk
    VERSION 0.21.5
    COMPANY_NAME "sl23"
    BUNDLE_ID "com.sl23.dusk"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_AUTO_MANIFEST FALSE
    PLUGIN_MANUFACTURER_CODE sl23
    PLUGIN_CODE dsk1
    FORMATS VST3
    VST3_CATEGORIES Fx Filter
    PRODUCT_NAME "Dusk")

juce_add_binary_data(DuskResources
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../Resources/logo.svg")

juce_generate_juce_header(Dusk)
target_sources(Dusk PRIVATE
    PluginProcessor.cpp PluginProcessor.h DuskDistortion.cpp DuskComb.cpp
    DuskFormant.cpp DuskPhaser.cpp
    PluginEditor.cpp PluginEditor.h
    DuskArchitectures.h DuskSequencer.h)
target_compile_features(Dusk PRIVATE cxx_std_17)
target_compile_definitions(Dusk PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Dusk PRIVATE juce::juce_audio_utils DuskResources
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags
           juce::juce_recommended_lto_flags)
