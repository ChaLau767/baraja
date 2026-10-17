cmake_minimum_required(VERSION 3.22)

project(Baraja VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Versión mínima de macOS")
endif()

include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

FetchContent_Declare(clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG 7adee3a1bd4684d4caa5601100e364abccff4b4d
    GIT_SUBMODULES_RECURSE TRUE)
FetchContent_MakeAvailable(clap-juce-extensions)

set(BARAJA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND BARAJA_FORMATS AU)
endif()

juce_add_plugin(Baraja
    PRODUCT_NAME "Baraja"
    COMPANY_NAME "Lautaro Martin Vieyra"
    BUNDLE_ID "ar.lautarovieyra.baraja"
    PLUGIN_MANUFACTURER_CODE Lmvy
    PLUGIN_CODE Brja
    FORMATS ${BARAJA_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay
    AU_MAIN_TYPE kAudioUnitType_Effect
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Baraja necesita la entrada de audio para procesarla en tiempo real.")

clap_juce_extensions_plugin(TARGET Baraja
    CLAP_ID "ar.lautarovieyra.baraja"
    CLAP_FEATURES audio-effect granular delay stereo)

juce_add_binary_data(BarajaData
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Resources/fonts/BarlowCondensed-SemiBold.ttf
        Resources/fonts/BarlowCondensed-Bold.ttf
        Resources/fonts/BarlowCondensed-ExtraBold.ttf
        Resources/fonts/ShareTechMono-Regular.ttf)

target_sources(Baraja PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Baraja PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Baraja
    PRIVATE
        BarajaData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Prueba del motor sin JUCE (se ejecuta en la compilación automática)
add_executable(baraja_engine_test tests/EngineTest.cpp)
target_include_directories(baraja_engine_test PRIVATE Source)
enable_testing()
add_test(NAME motor COMMAND baraja_engine_test)

# Captura de la interfaz para revisar el diseño (solo desarrollo)
option(BARAJA_SNAPSHOT "Compilar la herramienta de captura de la interfaz" OFF)
if(BARAJA_SNAPSHOT)
    juce_add_console_app(baraja_snapshot PRODUCT_NAME "baraja_snapshot")
    target_sources(baraja_snapshot PRIVATE tests/Snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(baraja_snapshot PRIVATE Source)
    target_compile_definitions(baraja_snapshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_VersionString="0.2.0" JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(baraja_snapshot PRIVATE BarajaData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

    juce_add_console_app(baraja_hosttest PRODUCT_NAME "baraja_hosttest")
    target_sources(baraja_hosttest PRIVATE tests/HostTest.cpp)
    target_compile_definitions(baraja_hosttest PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(baraja_hosttest PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
endif()
