juce_add_plugin(RealToneInstrument
    PRODUCT_NAME "RealTone Studio"
    COMPANY_NAME "RealTone"
    BUNDLE_ID "app.realtone.studio"
    PLUGIN_MANUFACTURER_CODE Rtns
    PLUGIN_CODE Rts1
    FORMATS VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth Sampler
    VST3_AUTO_MANIFEST TRUE
    VERSION ${PROJECT_VERSION})

# Everything lives in the shared library; the plug-in target only adds the format wrapper.
target_link_libraries(RealToneInstrument PRIVATE realtone_studio)
