#pragma once

#include <memory>
#include <vector>
#include <JuceHeader.h>
#include "PluginProcessor.h"

#if JUCE_WINDOWS
 #include <windows.h>
#endif

class StrobeDisplayComponent : public juce::Component,
                               private juce::Timer
{
public:
    explicit StrobeDisplayComponent(GuitarForgeStrobeTunerAudioProcessor& processor);
    ~StrobeDisplayComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawStrobeRows(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawNeedle(juce::Graphics& g, juce::Rectangle<float> bounds);

    GuitarForgeStrobeTunerAudioProcessor& processor;
#if SLAMMIN_TUNER_USE_OPENGL
    juce::OpenGLContext openGLContext;
#endif
    guitarforge::tuner::TunerSnapshot snapshot;
    std::vector<double> rowPhases;
    double lastFrameMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StrobeDisplayComponent)
};

class GuitarForgeStrobeTunerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                   private juce::Timer
{
public:
    explicit GuitarForgeStrobeTunerAudioProcessorEditor(GuitarForgeStrobeTunerAudioProcessor&);
    ~GuitarForgeStrobeTunerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    enum class ActiveValueField { none, a4, sensitivity, strobeRows };

    void timerCallback() override;
    void configureControls();
    void configureValueEditor(juce::TextEditor& editor);
    void persistStableEditorSize();
    void applyAccentColours();
    void updateValueEditors();
    void commitA4Editor();
    void commitSensitivityEditor();
    void commitStrobeRowsEditor();
    void beginDirectTextEntry(ActiveValueField field);
    void endDirectTextEntry(bool shouldCommit);
    void cancelDirectTextEntry();
    bool handleDirectEntryKeyPress(const juce::KeyPress& key);
    void insertDirectEntryText(const juce::String& text);
    void pasteDirectEntryText();
    void backspaceDirectEntryText();
    void deleteDirectEntryText();
    juce::TextEditor* valueEditorFor(ActiveValueField field);
#if JUCE_WINDOWS
    void installKeyboardCapture();
    void removeKeyboardCapture();
    bool handleDirectEntryVirtualKey(int virtualKey, bool isKeyDown, bool controlDown, bool shiftDown, bool altDown);
    bool showNativeValueEditor(ActiveValueField field);
    void destroyNativeValueEditor();
    void syncNativeValueEditorText();
    static LRESULT CALLBACK nativeValueEditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleNativeValueEditMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
#endif
    void drawReadout(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawNeedleStrip(juce::Graphics& g, juce::Rectangle<int> bounds);
    void launchBrandLinks();

    GuitarForgeStrobeTunerAudioProcessor& processor;
    StrobeDisplayComponent strobeDisplay;

    juce::Image logoImage;
    juce::ImageButton logoButton;
    juce::ComboBox strobeModeBox;
    juce::TextButton strobeRatioButton;
    juce::TextButton barContrastButton;
    juce::ComboBox profileBox;
    juce::ComboBox trackingBox;
    juce::ComboBox inputBox;
    juce::ComboBox colourBox;
    juce::Slider a4Slider;
    juce::Slider sensitivitySlider;
    juce::TextEditor a4ValueEditor;
    juce::TextEditor sensitivityValueEditor;
    juce::TextEditor strobeRowsValueEditor;
    juce::ToggleButton bandResolvedButton;
    juce::ToggleButton muteButton;
    juce::ToggleButton referenceToneButton;
    juce::ToggleButton reducedMotionButton;
    juce::Label versionLabel;
    juce::Label tuningSummaryLabel;

    std::unique_ptr<ComboBoxAttachment> profileAttachment;
    std::unique_ptr<ComboBoxAttachment> trackingAttachment;
    std::unique_ptr<ComboBoxAttachment> inputAttachment;
    std::unique_ptr<ComboBoxAttachment> colourAttachment;
    std::unique_ptr<SliderAttachment> a4Attachment;
    std::unique_ptr<SliderAttachment> sensitivityAttachment;
    std::unique_ptr<ButtonAttachment> bandResolvedAttachment;
    std::unique_ptr<ButtonAttachment> muteAttachment;
    std::unique_ptr<ButtonAttachment> referenceToneAttachment;
    std::unique_ptr<ButtonAttachment> reducedMotionAttachment;
    std::unique_ptr<ComboBoxAttachment> strobeModeAttachment;
    std::unique_ptr<ButtonAttachment> barContrastAttachment;

    guitarforge::tuner::TunerSnapshot snapshot;
    juce::Rectangle<int> noteClickBounds;
    ActiveValueField activeValueField = ActiveValueField::none;
#if JUCE_WINDOWS
    HWND nativeValueEditHandle = nullptr;
    WNDPROC nativeValueEditOriginalProc = nullptr;
#endif
    bool preferSharps = false;
    juce::Point<int> pendingEditorSize;
    double pendingEditorSizeSinceMs = 0.0;
    bool hasPendingEditorSize = false;
    bool shuttingDownEditor = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarForgeStrobeTunerAudioProcessorEditor)
};
