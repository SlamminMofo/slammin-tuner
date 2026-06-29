#pragma once

#include <atomic>
#include <memory>
#include <vector>
#include <JuceHeader.h>
#include "PitchTracker.h"

class GuitarForgeStrobeTunerAudioProcessor : public juce::AudioProcessor
{
public:
    using AudioProcessorValueTreeState = juce::AudioProcessorValueTreeState;

    GuitarForgeStrobeTunerAudioProcessor();
    ~GuitarForgeStrobeTunerAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    guitarforge::tuner::TunerSnapshot getTunerSnapshot() const;
    bool loadCustomProfileFromFile(const juce::File& file, juce::String& errorMessage);
    juce::String getCustomProfileStatus() const;
    juce::String getActiveProfileName() const;
    juce::String getActiveTuningSummary(bool preferSharps) const;
    juce::Point<int> getSavedEditorSize() const noexcept;
    void setSavedEditorSize(int width, int height) noexcept;
    void flushUserPreferences();
    double getStrobeSensitivity() const noexcept;
    int getStrobeMode() const noexcept;
    int getStrobeRowCount() const noexcept;
    bool isReducedMotionEnabled() const noexcept;
    bool isAlternateStrobeDirectionEnabled() const noexcept;
    bool isSolidStrobeStyleEnabled() const noexcept;
    bool isClassicStrobeModeEnabled() const noexcept;
    int getStrobeRatioMode() const noexcept;
    bool isSharpNotationEnabled() const noexcept;
    bool isBandResolvedStrobeEnabled() const noexcept;
    bool isBarContrastEnabled() const noexcept;
    static juce::StringArray getColourChoiceNames();
    static juce::StringArray getStrobeModeChoiceNames();

    AudioProcessorValueTreeState parameters;

    static AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct ParameterIds
    {
        static constexpr const char* a4Hz = "a4Hz";
        static constexpr const char* profileIndex = "profileIndex";
        static constexpr const char* trackingMode = "trackingMode";
        static constexpr const char* strobeSensitivity = "strobeSensitivity";
        static constexpr const char* strobeColour = "strobeColour";
        static constexpr const char* strobeMode = "strobeMode";
        static constexpr const char* strobeRows = "strobeRows";
        static constexpr const char* strobeRatioMode = "strobeRatioMode";
        static constexpr const char* preferSharps = "preferSharps";
        static constexpr const char* bandResolvedStrobe = "bandResolvedStrobe";
        static constexpr const char* barContrast = "barContrast";
        static constexpr const char* inputMode = "inputMode";
        static constexpr const char* muteOutput = "muteOutput";
        static constexpr const char* referenceToneEnabled = "referenceToneEnabled";
        static constexpr const char* reducedMotion = "reducedMotion";
    };

    class AnalysisThread : public juce::Thread
    {
    public:
        explicit AnalysisThread(GuitarForgeStrobeTunerAudioProcessor& ownerProcessor);
        void run() override;

    private:
        GuitarForgeStrobeTunerAudioProcessor& owner;
    };

    void runAnalysisTick();
    void loadUserPreferences();
    void saveUserPreferences();
    void restoreStateTree(const juce::ValueTree& state, bool allowCustomProfileRestore);
    void writeInputToRing(const juce::AudioBuffer<float>& buffer, int inputMode);
    guitarforge::tuner::TuningProfile getActiveProfile() const;
    guitarforge::tuner::TrackingMode getTrackingMode() const noexcept;
    double getA4Hz() const noexcept;
    bool isReferenceToneEnabled() const noexcept;
    bool isMuteOutputEnabled() const noexcept;
    int getInputMode() const noexcept;
    static int nextPowerOfTwo(int value) noexcept;
    static float clampSample(float value) noexcept;

    AnalysisThread analysisThread;
    guitarforge::tuner::PitchTracker pitchTracker;

    mutable juce::CriticalSection snapshotLock;
    guitarforge::tuner::TunerSnapshot latestSnapshot;

    mutable juce::CriticalSection customProfileLock;
    guitarforge::tuner::TuningProfile customProfile;
    juce::String customProfileJson;
    juce::String customProfilePath;
    juce::String customProfileStatus = "No custom profile loaded.";

    std::vector<float> ringBuffer;
    std::vector<float> analysisScratch;
    std::vector<float> trackerScratch;
    std::atomic<size_t> ringWriteIndex { 0 };
    int ringMask = 0;
    int analysisFrameSize = 4096;
    int trackerFrameSize = 2048;
    int analysisDecimation = 2;
    std::atomic<double> preparedSampleRate { 0.0 };
    std::atomic<double> referenceTargetHz { 440.0 };
    std::atomic<int> savedEditorWidth { 1120 };
    std::atomic<int> savedEditorHeight { 720 };
    double referencePhase = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarForgeStrobeTunerAudioProcessor)
};
