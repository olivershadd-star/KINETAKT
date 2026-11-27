cmake_minimum_required(VERSION 3.16)
project(ELEKTAKT VERSION 0.6.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Threads REQUIRED)

# Audio engine inherited from KICK (AudIO + Airwindows) with the ELEKTAKT (TRIG layer) sequencer.
add_library(trig_engine STATIC src/engine.cpp)
target_include_directories(trig_engine PUBLIC src PRIVATE vendor/AudIO)
target_link_libraries(trig_engine PUBLIC Threads::Threads)
if(MSVC)
    target_compile_options(trig_engine PRIVATE /utf-8 /bigobj)
    target_compile_definitions(trig_engine PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(MINGW)
    target_compile_options(trig_engine PRIVATE -Wa,-mbig-obj)
endif()

if(WIN32)
    add_executable(ELEKTAKT WIN32 src/trig_windows.cpp src/ui.cpp)
    target_link_libraries(ELEKTAKT PRIVATE trig_engine winmm avrt comdlg32 shell32 gdi32 gdiplus)
    target_compile_definitions(ELEKTAKT PRIVATE UNICODE _UNICODE)
    if(MSVC)
        target_compile_options(ELEKTAKT PRIVATE /utf-8)
        # Snapshots hold every pattern; keep a comfortable GUI stack.
        target_link_options(ELEKTAKT PRIVATE /STACK:16777216)
    elseif(MINGW)
        target_link_options(ELEKTAKT PRIVATE -static -municode -Wl,--stack,16777216)
    endif()
    add_custom_command(TARGET ELEKTAKT POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/demo-kit $<TARGET_FILE_DIR:ELEKTAKT>/demo-kit)
endif()

add_executable(TRIG-Test tests/trig_test.cpp)
target_link_libraries(TRIG-Test PRIVATE trig_engine)
if(MSVC)
    target_link_options(TRIG-Test PRIVATE /STACK:16777216)
elseif(MINGW)
    target_link_options(TRIG-Test PRIVATE -static -Wl,--stack,16777216)
endif()
enable_testing()
add_test(NAME TrigSequencer COMMAND TRIG-Test ${CMAKE_CURRENT_BINARY_DIR}/trig-test-output)

# Optional Linux harness: draws the interface with Cairo into PNG files.
option(TRIG_UI_HARNESS "Build the Cairo interface renderer (Linux)" OFF)
if(TRIG_UI_HARNESS)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CAIRO REQUIRED cairo)
    add_executable(TRIG-UI-Render tests/ui_render.cpp src/ui.cpp)
    target_include_directories(TRIG-UI-Render PRIVATE src ${CAIRO_INCLUDE_DIRS})
    target_link_libraries(TRIG-UI-Render PRIVATE trig_engine ${CAIRO_LIBRARIES})
endif()

# ----- VST3 plugin (JUCE) -----
option(ELEKTAKT_PLUGIN "Build the ELEKTAKT VST3 plugin with JUCE" OFF)
if(ELEKTAKT_PLUGIN)
    set_target_properties(trig_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(JUCE_SOURCE_DIR)
        add_subdirectory(${JUCE_SOURCE_DIR} JUCE)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.17-security GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()
    juce_add_plugin(ELEKTAKT_VST3
        PRODUCT_NAME "ELEKTAKT"
        COMPANY_NAME "ANKS Labs"
        COMPANY_WEBSITE "https://github.com/olivershadd-star"
        PLUGIN_MANUFACTURER_CODE AnkL
        PLUGIN_CODE Ektk
        VERSION ${PROJECT_VERSION}
        FORMATS VST3
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        VST3_CATEGORIES Instrument Sampler Drum
        COPY_PLUGIN_AFTER_BUILD FALSE)
    file(GLOB ELEKTAKT_DEMO_WAVS ${CMAKE_CURRENT_SOURCE_DIR}/demo-kit/*.wav)
    juce_add_binary_data(ElektaktDemoKit HEADER_NAME DemoKit.h NAMESPACE DemoKit SOURCES ${ELEKTAKT_DEMO_WAVS})
    set_target_properties(ElektaktDemoKit PROPERTIES POSITION_INDEPENDENT_CODE ON)
    juce_generate_juce_header(ELEKTAKT_VST3)
    target_sources(ELEKTAKT_VST3 PRIVATE plugin/ElektaktPlugin.cpp src/ui.cpp)
    target_include_directories(ELEKTAKT_VST3 PRIVATE src plugin)
    target_compile_definitions(ELEKTAKT_VST3 PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_DISPLAY_SPLASH_SCREEN=0)
    if(MSVC)
        target_compile_options(ELEKTAKT_VST3 PRIVATE /utf-8)
    endif()
    target_link_libraries(ELEKTAKT_VST3 PRIVATE trig_engine ElektaktDemoKit
        juce::juce_audio_utils juce::juce_audio_processors
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
if(ELEKTAKT_PLUGIN AND ELEKTAKT_PLUGIN_TEST)
    # Minimal VST3 host used by the tests: loads the built plugin like a DAW.
    juce_add_console_app(ELEKTAKT_PluginHostTest PRODUCT_NAME "ELEKTAKT-PluginHostTest")
    juce_generate_juce_header(ELEKTAKT_PluginHostTest)
    target_sources(ELEKTAKT_PluginHostTest PRIVATE tests/plugin_host_test.cpp src/ui.cpp)
    target_include_directories(ELEKTAKT_PluginHostTest PRIVATE src plugin)
    target_compile_definitions(ELEKTAKT_PluginHostTest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(ELEKTAKT_PluginHostTest PRIVATE trig_engine juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_extra PUBLIC juce::juce_recommended_config_flags)
endif()
