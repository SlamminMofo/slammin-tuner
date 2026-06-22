#pragma once

#include <array>
#include <JuceHeader.h>
#include "TuningProfile.h"

namespace guitarforge::tuner
{
    enum class TrackingMode
    {
        balanced = 0,
        fast,
        precision
    };

    struct PitchFrame
    {
        double measuredHz = 0.0;
        double rawHz = 0.0;
        double targetHz = 440.0;
        double cents = 0.0;
        double confidence = 0.0;
        int midi = 69;
        int stringIndex = -1;
        bool voiced = false;
        bool attackSettling = false;
        juce::String displayNote = "A";
        juce::String displayNoteWithOctave = "A4";
    };

    struct StrobeRow
    {
        double targetHz = 440.0;
        double speedCents = 0.0;
        double opacity = 0.0;
        int harmonic = 1;
    };

    struct TunerSnapshot
    {
        PitchFrame pitch;
        juce::Array<StrobeRow> rows;
        float inputDbFs = -120.0f;
        float peakDbFs = -120.0f;
        juce::String profileName = "Chromatic Equal";
    };

    class PitchTracker
    {
    public:
        void prepare(double newSampleRate, int maximumFrameSize);
        void reset();

        TunerSnapshot analyse(const float* samples,
                              int sampleCount,
                              double a4Hz,
                              const TuningProfile& profile,
                              TrackingMode mode,
                              bool bandResolvedStrobe);

    private:
        struct YinEstimate
        {
            double frequencyHz = 0.0;
            double confidence = 0.0;
            bool voiced = false;
        };

        YinEstimate estimateYin(const float* samples, int sampleCount, TrackingMode mode);
        double selectHarmonicCandidate(const float* samples,
                                       int sampleCount,
                                       double yinFrequency,
                                       const TuningProfile& profile,
                                       double a4Hz);
        double estimateBandCents(const float* samples,
                                 int sampleCount,
                                 double targetBandHz,
                                 double fallbackCents,
                                 double signalRms,
                                 double baseConfidence,
                                 double& reliability) const;
        double smoothResolvedSpeed(int rowIndex, double rawSpeedCents, double reliability);
        double harmonicScore(const float* samples, int sampleCount, double candidateHz) const;
        double smoothFrequency(double nextFrequencyHz, double confidence, TrackingMode mode);
        static double parabolicOffset(double previous, double current, double next) noexcept;
        static double centsBetween(double measuredHz, double targetHz) noexcept;

        double sampleRate = 48000.0;
        double previousStableHz = 0.0;
        double previousPeak = 0.0;
        double previousResolvedTargetHz = 0.0;
        bool hasStableFrequency = false;
        bool hasResolvedRows = false;
        std::array<double, 8> previousResolvedSpeedCents {};
        std::array<double, 8> previousResolvedReliability {};
        juce::HeapBlock<double> difference;
        juce::HeapBlock<double> cumulative;
        int allocatedTauCount = 0;
    };
}
