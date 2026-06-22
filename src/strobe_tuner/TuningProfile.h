#pragma once

#include <array>
#include <JuceHeader.h>

namespace guitarforge::tuner
{
    struct TargetNote
    {
        int midi = 69;
        int stringIndex = -1;
        double offsetCents = 0.0;
        juce::String stringLabel;

        double getFrequencyHz(double a4Hz, const std::array<double, 12>& noteClassOffsets) const noexcept;
        juce::String getFlatNameWithOctave() const;
        juce::String getFlatName() const;
    };

    struct TuningProfile
    {
        juce::String id;
        juce::String name;
        juce::String description;
        bool chromatic = false;
        double defaultA4Hz = 440.0;
        std::array<double, 12> noteClassOffsetsCents {};
        juce::Array<TargetNote> targets;

        bool isValid() const noexcept;
    };

    class TuningProfileLibrary
    {
    public:
        static constexpr int customProfileIndex = 32;

        static int getBuiltInProfileCount() noexcept;
        static int getProfileChoiceCount() noexcept;
        static juce::StringArray getProfileChoiceNames();
        static TuningProfile getBuiltInProfile(int index);
        static TuningProfile getDefaultChromaticProfile();

        static TargetNote findNearestTarget(double measuredHz,
                                            double a4Hz,
                                            const TuningProfile& profile);

        static juce::String flatNameForMidi(int midi);
        static juce::String flatNameWithOctaveForMidi(int midi);
        static juce::String sharpNameForMidi(int midi);
        static juce::String sharpNameWithOctaveForMidi(int midi);
        static int nearestMidiForFrequency(double frequencyHz, double a4Hz) noexcept;

        static bool parseProfileJson(const juce::String& jsonText,
                                     TuningProfile& profile,
                                     juce::String& errorMessage);

    private:
        static int noteClassForMidi(int midi) noexcept;
        static int midiFromNoteName(const juce::String& text, bool& ok);
    };
}
