if (APPLE)
    set(default_slammin_tuner_formats AU VST3)
    set(default_slammin_tuner_opengl OFF)
elseif (WIN32)
    set(default_slammin_tuner_formats VST3)
    set(default_slammin_tuner_opengl ON)
else()
    set(default_slammin_tuner_formats VST3)
    set(default_slammin_tuner_opengl OFF)
endif()

set(SLAMMIN_TUNER_FORMATS "${default_slammin_tuner_formats}"
    CACHE STRING "Plugin formats to build, for example AU, VST3, or AU;VST3")
option(SLAMMIN_TUNER_USE_OPENGL "Use JUCE OpenGL-backed component painting where supported" ${default_slammin_tuner_opengl})
option(SLAMMIN_TUNER_STRICT_WARNINGS "Enable stricter compiler diagnostics for CI validation" OFF)

juce_add_plugin(SlamminTunerV1
    VERSION "1.2.1"
    COMPANY_NAME "Slammin Captures"
    COMPANY_WEBSITE "https://slammincaptures.bigcartel.com/"
    BUNDLE_ID com.slammincaptures.slammin-tuner.v121
    PLUGIN_MANUFACTURER_CODE SLMN
    PLUGIN_CODE ST31
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS ${SLAMMIN_TUNER_FORMATS}
    PRODUCT_NAME "Slammin Tuner")

juce_add_binary_data(SlamminTunerV1Assets
    SOURCES
        assets/Slammin_Logo_Main.png)

target_sources(SlamminTunerV1
    PRIVATE
        PitchTracker.cpp
        PitchTracker.h
        PluginEditor.cpp
        PluginEditor.h
        PluginProcessor.cpp
        PluginProcessor.h
        TuningProfile.cpp
        TuningProfile.h)

target_compile_definitions(SlamminTunerV1
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        SLAMMIN_TUNER_USE_OPENGL=$<BOOL:${SLAMMIN_TUNER_USE_OPENGL}>)

if (APPLE)
    target_compile_definitions(SlamminTunerV1
        PRIVATE
            JUCE_DISABLE_COREGRAPHICS_FONT_SMOOTHING=0)
endif()

if (SLAMMIN_TUNER_STRICT_WARNINGS)
    target_compile_options(SlamminTunerV1
        PRIVATE
            $<$<CXX_COMPILER_ID:AppleClang,Clang>:-Wall -Wextra -Wpedantic -Werror=return-type -Werror=unguarded-availability-new>
            $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->)
endif()

target_link_libraries(SlamminTunerV1
    PRIVATE
        SlamminTunerV1Assets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

juce_generate_juce_header(SlamminTunerV1)
