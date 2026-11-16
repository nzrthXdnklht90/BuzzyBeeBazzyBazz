# JUCE plug-in target. JUCE is pinned to tag 8.0.9 (commit f72bad64d29715216226685810c5196bd0d79d77).
# Licence: JUCE 8 is dual AGPLv3 / commercial. See docs/dependency_manifest.md before distributing.
set(BASSFORGE_JUCE_DIR "" CACHE PATH "Optional local JUCE checkout (skips download)")
if(BASSFORGE_JUCE_DIR)
  add_subdirectory(${BASSFORGE_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG f72bad64d29715216226685810c5196bd0d79d77  # 8.0.9
    GIT_SHALLOW FALSE)
  FetchContent_MakeAvailable(JUCE)
endif()

set(BF_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND BF_FORMATS AU)
endif()

juce_add_plugin(BassForge
  COMPANY_NAME "Nazareth"
  COMPANY_CODE Nzrt
  PLUGIN_MANUFACTURER_CODE Nzrt
  PLUGIN_CODE Bsfg
  BUNDLE_ID com.nazareth.bassforge
  PRODUCT_NAME "BASSFORGE"
  VERSION ${PROJECT_VERSION}
  FORMATS ${BF_FORMATS}
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Distortion
  AU_MAIN_TYPE kAudioUnitType_Effect)

set(BF_PLUGIN_SOURCES
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/Params.cpp
  Source/Presets.cpp
  Source/Ui/Widgets.cpp
  Source/Ui/Pages.cpp)
target_sources(BassForge PRIVATE ${BF_PLUGIN_SOURCES})
target_include_directories(BassForge PRIVATE Source)
target_compile_definitions(BassForge PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(BassForge
  PRIVATE bassforge_dsp juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

option(BASSFORGE_BUILD_HOST_SMOKE "Build the headless VST3 host smoke-test tool" OFF)
if(BASSFORGE_BUILD_HOST_SMOKE)
  juce_add_console_app(bf_host_smoke PRODUCT_NAME "bf_host_smoke")
  target_sources(bf_host_smoke PRIVATE tools/host_smoke/HostSmoke.cpp)
  target_compile_definitions(bf_host_smoke PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(bf_host_smoke PRIVATE juce::juce_audio_processors juce::juce_gui_basics juce::juce_recommended_config_flags)
endif()

option(BASSFORGE_BUILD_UI_CAPTURE "Build the headless UI screenshot tool" OFF)
if(BASSFORGE_BUILD_UI_CAPTURE)
  juce_add_console_app(bf_ui_capture PRODUCT_NAME "bf_ui_capture")
  target_sources(bf_ui_capture PRIVATE tools/ui_capture/UiCapture.cpp ${BF_PLUGIN_SOURCES})
  target_include_directories(bf_ui_capture PRIVATE Source)
  target_compile_definitions(bf_ui_capture PRIVATE JucePlugin_VersionString="${PROJECT_VERSION}" JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(bf_ui_capture PRIVATE bassforge_dsp juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
