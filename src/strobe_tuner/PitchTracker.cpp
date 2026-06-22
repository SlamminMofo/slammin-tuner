#include "PitchTracker.h"

#include <cmath>
#include <limits>

namespace guitarforge::tuner
{
    namespace
    {
        constexpr double minimumFrequencyHz = 25.0;
        constexpr double maximumFrequencyHz = 1600.0;
        constexpr double silencePeak = 0.000025;
        constexpr double silenceRms = 0.000012;
        constexpr double centsPerOctave = 1200.0;
        constexpr double twoPi = juce::MathConstants<double>::twoPi;

        double thresholdForMode(TrackingMode mode)
        {
            switch (mode)
            {
                case TrackingMode::fast:      return 0.18;
                case TrackingMode::precision: return 0.08;
                case TrackingMode::balanced:
                default:                      return 0.12;
            }
        }

        double smoothingForMode(TrackingMode mode)
        {
            switch (mode)
            {
                case TrackingMode::fast:      return 0.48;
                case TrackingMode::precision: return 0.18;
                case TrackingMode::balanced:
                default:                      return 0.30;
            }
        }

        int rowCountForMode(TrackingMode mode)
        {
            juce::ignoreUnused(mode);
            return 5;
        }

        double hannWindow(int index, int sampleCount)
        {
            return 0.5 - (0.5 * std::cos(twoPi * static_cast<double>(index) / static_cast<double>(juce::jmax(1, sampleCount - 1))));
        }

        double wrapPositive(double value, double maximum)
        {
            while (value < 0.0)
                value += maximum;
            while (value >= maximum)
                value -= maximum;
            return value;
        }

        double windowedMagnitudeAt(const float* samples, int sampleCount, double frequencyHz, double sampleRate)
        {
            if (samples == nullptr || sampleCount <= 0 || frequencyHz <= 0.0 || sampleRate <= 0.0)
                return 0.0;

            const auto phaseStep = twoPi * frequencyHz / sampleRate;
            const auto stepCos = std::cos(phaseStep);
            const auto stepSin = std::sin(phaseStep);

            double phaseCos = 1.0;
            double phaseSin = 0.0;
            double real = 0.0;
            double imaginary = 0.0;

            for (int index = 0; index < sampleCount; ++index)
            {
                const auto value = static_cast<double>(samples[index]) * hannWindow(index, sampleCount);
                real += value * phaseCos;
                imaginary -= value * phaseSin;

                const auto nextCos = (phaseCos * stepCos) - (phaseSin * stepSin);
                phaseSin = (phaseSin * stepCos) + (phaseCos * stepSin);
                phaseCos = nextCos;
            }

            return std::sqrt((real * real) + (imaginary * imaginary));
        }
    }

    void PitchTracker::prepare(double newSampleRate, int maximumFrameSize)
    {
        sampleRate = juce::jmax(8000.0, newSampleRate);
        const auto maxTau = juce::jlimit(8, juce::jmax(8, maximumFrameSize - 2),
                                         static_cast<int>(std::ceil(sampleRate / minimumFrequencyHz)) + 2);

        if (maxTau + 1 > allocatedTauCount)
        {
            allocatedTauCount = maxTau + 1;
            difference.allocate(static_cast<size_t>(allocatedTauCount), true);
            cumulative.allocate(static_cast<size_t>(allocatedTauCount), true);
        }

        reset();
    }

    void PitchTracker::reset()
    {
        previousStableHz = 0.0;
        previousPeak = 0.0;
        previousResolvedTargetHz = 0.0;
        hasStableFrequency = false;
        hasResolvedRows = false;
        previousResolvedSpeedCents.fill(0.0);
        previousResolvedReliability.fill(0.0);
    }

    TunerSnapshot PitchTracker::analyse(const float* samples,
                                        int sampleCount,
                                        double a4Hz,
                                        const TuningProfile& profile,
                                        TrackingMode mode,
                                        bool bandResolvedStrobe)
    {
        TunerSnapshot snapshot;
        snapshot.profileName = profile.name;

        if (samples == nullptr || sampleCount <= 0 || sampleRate <= 0.0)
            return snapshot;

        double peak = 0.0;
        double energy = 0.0;
        for (int index = 0; index < sampleCount; ++index)
        {
            const auto sample = static_cast<double>(samples[index]);
            peak = juce::jmax(peak, std::abs(sample));
            energy += sample * sample;
        }

        const auto rms = std::sqrt(energy / static_cast<double>(sampleCount));
        snapshot.peakDbFs = juce::Decibels::gainToDecibels(static_cast<float>(peak), -120.0f);
        snapshot.inputDbFs = juce::Decibels::gainToDecibels(static_cast<float>(rms), -120.0f);

        const auto silent = peak < silencePeak || rms < silenceRms;
        if (silent)
        {
            hasStableFrequency = false;
            hasResolvedRows = false;
            previousPeak = peak;
            snapshot.pitch.voiced = false;
            snapshot.pitch.confidence = 0.0;
            return snapshot;
        }

        auto yin = estimateYin(samples, sampleCount, mode);
        if (! yin.voiced)
        {
            previousPeak = peak;
            snapshot.pitch.confidence = yin.confidence;
            return snapshot;
        }

        const auto correctedHz = selectHarmonicCandidate(samples, sampleCount, yin.frequencyHz, profile, a4Hz);
        const auto stableHz = smoothFrequency(correctedHz, yin.confidence, mode);
        const auto target = TuningProfileLibrary::findNearestTarget(stableHz, a4Hz, profile);
        const auto targetHz = target.getFrequencyHz(a4Hz, profile.noteClassOffsetsCents);
        const auto cents = centsBetween(stableHz, targetHz);
        const auto attackSettling = peak > previousPeak * 1.85 && previousPeak > silencePeak;

        snapshot.pitch.measuredHz = stableHz;
        snapshot.pitch.rawHz = correctedHz;
        snapshot.pitch.targetHz = targetHz;
        snapshot.pitch.cents = cents;
        snapshot.pitch.confidence = juce::jlimit(0.0, 1.0, yin.confidence);
        snapshot.pitch.midi = target.midi;
        snapshot.pitch.stringIndex = target.stringIndex;
        snapshot.pitch.voiced = snapshot.pitch.confidence > 0.34;
        snapshot.pitch.attackSettling = attackSettling;
        snapshot.pitch.displayNote = target.getFlatName();
        snapshot.pitch.displayNoteWithOctave = target.getFlatNameWithOctave();

        const auto rows = rowCountForMode(mode);
        const auto useBandResolvedRows = bandResolvedStrobe && snapshot.pitch.voiced;
        if (useBandResolvedRows
            && (! hasResolvedRows || previousResolvedTargetHz <= 0.0 || std::abs(centsBetween(targetHz, previousResolvedTargetHz)) > 50.0))
        {
            previousResolvedSpeedCents.fill(0.0);
            previousResolvedReliability.fill(0.0);
            hasResolvedRows = false;
        }

        for (int row = 0; row < rows; ++row)
        {
            const auto harmonic = row + 1;
            StrobeRow strobeRow;
            strobeRow.harmonic = harmonic;
            strobeRow.targetHz = targetHz * static_cast<double>(harmonic);
            const auto fallbackSpeedCents = cents * static_cast<double>(harmonic);

            if (useBandResolvedRows)
            {
                double reliability = 0.0;
                const auto bandCents = estimateBandCents(samples,
                                                         sampleCount,
                                                         strobeRow.targetHz,
                                                         cents,
                                                         rms,
                                                         snapshot.pitch.confidence,
                                                         reliability);
                const auto rawSpeedCents = reliability >= 0.10
                    ? bandCents * static_cast<double>(harmonic)
                    : fallbackSpeedCents;
                strobeRow.speedCents = smoothResolvedSpeed(row, rawSpeedCents, reliability);
                strobeRow.opacity = snapshot.pitch.confidence * juce::jlimit(0.28, 1.0, 0.36 + (0.80 * reliability));
            }
            else
            {
                strobeRow.speedCents = fallbackSpeedCents;
                strobeRow.opacity = snapshot.pitch.voiced ? snapshot.pitch.confidence : snapshot.pitch.confidence * 0.35;
            }

            snapshot.rows.add(strobeRow);
        }

        if (useBandResolvedRows)
        {
            hasResolvedRows = true;
            previousResolvedTargetHz = targetHz;
        }
        else
        {
            hasResolvedRows = false;
        }

        previousPeak = peak;
        return snapshot;
    }

    PitchTracker::YinEstimate PitchTracker::estimateYin(const float* samples, int sampleCount, TrackingMode mode)
    {
        YinEstimate estimate;
        const auto minTau = juce::jmax(2, static_cast<int>(std::floor(sampleRate / maximumFrequencyHz)));
        auto maxTau = juce::jmin(sampleCount - 2, static_cast<int>(std::ceil(sampleRate / minimumFrequencyHz)));
        maxTau = juce::jmin(maxTau, allocatedTauCount - 1);

        if (maxTau <= minTau + 2)
            return estimate;

        const auto windowLength = juce::jmax(32, sampleCount - maxTau);
        double mean = 0.0;
        for (int index = 0; index < sampleCount; ++index)
            mean += samples[index];
        mean /= static_cast<double>(sampleCount);

        difference[0] = 0.0;
        cumulative[0] = 1.0;

        for (int tau = 1; tau <= maxTau; ++tau)
        {
            double sum = 0.0;
            for (int index = 0; index < windowLength; ++index)
            {
                const auto delta = (static_cast<double>(samples[index]) - mean)
                    - (static_cast<double>(samples[index + tau]) - mean);
                sum += delta * delta;
            }

            difference[tau] = sum;
        }

        double runningSum = 0.0;
        for (int tau = 1; tau <= maxTau; ++tau)
        {
            runningSum += difference[tau];
            cumulative[tau] = runningSum > std::numeric_limits<double>::epsilon()
                ? difference[tau] * static_cast<double>(tau) / runningSum
                : 1.0;
        }

        const auto threshold = thresholdForMode(mode);
        int bestTau = -1;
        for (int tau = minTau; tau <= maxTau - 1; ++tau)
        {
            if (cumulative[tau] < threshold)
            {
                while (tau + 1 <= maxTau && cumulative[tau + 1] < cumulative[tau])
                    ++tau;

                bestTau = tau;
                break;
            }
        }

        if (bestTau < 0)
        {
            auto bestValue = std::numeric_limits<double>::max();
            for (int tau = minTau; tau <= maxTau; ++tau)
            {
                if (cumulative[tau] < bestValue)
                {
                    bestValue = cumulative[tau];
                    bestTau = tau;
                }
            }
        }

        if (bestTau < minTau || bestTau > maxTau)
            return estimate;

        const auto localPrevious = cumulative[juce::jmax(1, bestTau - 1)];
        const auto localCurrent = cumulative[bestTau];
        const auto localNext = cumulative[juce::jmin(maxTau, bestTau + 1)];
        const auto refinedTau = static_cast<double>(bestTau) + parabolicOffset(localPrevious, localCurrent, localNext);
        const auto confidence = 1.0 - juce::jlimit(0.0, 1.0, localCurrent);

        estimate.frequencyHz = sampleRate / juce::jmax(1.0, refinedTau);
        estimate.confidence = confidence;
        estimate.voiced = confidence > 0.24 && estimate.frequencyHz >= minimumFrequencyHz && estimate.frequencyHz <= maximumFrequencyHz;
        return estimate;
    }

    double PitchTracker::selectHarmonicCandidate(const float* samples,
                                                 int sampleCount,
                                                 double yinFrequency,
                                                 const TuningProfile& profile,
                                                 double a4Hz)
    {
        if (! std::isfinite(yinFrequency) || yinFrequency <= 0.0)
            return yinFrequency;

        double candidates[] = { yinFrequency * 0.5, yinFrequency, yinFrequency * 2.0 };
        auto bestFrequency = yinFrequency;
        auto bestScore = -std::numeric_limits<double>::max();

        for (auto candidate : candidates)
        {
            if (candidate < minimumFrequencyHz || candidate > maximumFrequencyHz)
                continue;

            const auto target = TuningProfileLibrary::findNearestTarget(candidate, a4Hz, profile);
            const auto targetHz = target.getFrequencyHz(a4Hz, profile.noteClassOffsetsCents);
            const auto targetError = std::abs(centsBetween(candidate, targetHz));
            const auto targetBonus = 1.0 + (0.10 * juce::jlimit(0.0, 1.0, (35.0 - targetError) / 35.0));
            const auto score = harmonicScore(samples, sampleCount, candidate) * targetBonus;

            if (score > bestScore)
            {
                bestScore = score;
                bestFrequency = candidate;
            }
        }

        return bestFrequency;
    }

    double PitchTracker::harmonicScore(const float* samples, int sampleCount, double candidateHz) const
    {
        if (candidateHz <= 0.0 || sampleCount <= 0)
            return 0.0;

        double score = 0.0;
        const auto maxHarmonic = 8;
        for (int harmonic = 1; harmonic <= maxHarmonic; ++harmonic)
        {
            const auto frequency = candidateHz * static_cast<double>(harmonic);
            if (frequency >= sampleRate * 0.48)
                break;

            const auto phaseStep = twoPi * frequency / sampleRate;
            double phase = 0.0;
            double real = 0.0;
            double imaginary = 0.0;

            for (int index = 0; index < sampleCount; ++index)
            {
                const auto window = hannWindow(index, sampleCount);
                const auto value = static_cast<double>(samples[index]) * window;
                real += value * std::cos(phase);
                imaginary -= value * std::sin(phase);
                phase = wrapPositive(phase + phaseStep, twoPi);
            }

            const auto magnitude = std::sqrt((real * real) + (imaginary * imaginary));
            score += magnitude / std::pow(static_cast<double>(harmonic), 0.72);
        }

        return score;
    }

    double PitchTracker::estimateBandCents(const float* samples,
                                           int sampleCount,
                                           double targetBandHz,
                                           double fallbackCents,
                                           double signalRms,
                                           double baseConfidence,
                                           double& reliability) const
    {
        reliability = 0.0;

        if (samples == nullptr || sampleCount <= 0 || targetBandHz <= minimumFrequencyHz || targetBandHz >= sampleRate * 0.46)
            return fallbackCents;

        constexpr int probeCount = 9;
        constexpr int middleProbe = probeCount / 2;
        constexpr double probeStepCents = 8.0;
        double magnitudes[probeCount] {};

        auto bestIndex = middleProbe;
        auto bestMagnitude = 0.0;
        auto magnitudeSum = 0.0;

        for (int probe = 0; probe < probeCount; ++probe)
        {
            const auto offsetCents = (static_cast<double>(probe - middleProbe) * probeStepCents);
            const auto probeHz = targetBandHz * std::pow(2.0, offsetCents / centsPerOctave);
            magnitudes[probe] = windowedMagnitudeAt(samples, sampleCount, probeHz, sampleRate);
            magnitudeSum += magnitudes[probe];

            if (magnitudes[probe] > bestMagnitude)
            {
                bestMagnitude = magnitudes[probe];
                bestIndex = probe;
            }
        }

        if (bestMagnitude <= std::numeric_limits<double>::epsilon())
            return fallbackCents;

        auto estimatedCents = static_cast<double>(bestIndex - middleProbe) * probeStepCents;
        if (bestIndex > 0 && bestIndex < probeCount - 1)
        {
            const auto previous = std::log(magnitudes[bestIndex - 1] + 1.0e-12);
            const auto current = std::log(magnitudes[bestIndex] + 1.0e-12);
            const auto next = std::log(magnitudes[bestIndex + 1] + 1.0e-12);
            estimatedCents += parabolicOffset(previous, current, next) * probeStepCents;
        }

        const auto localMean = (magnitudeSum - bestMagnitude) / static_cast<double>(probeCount - 1);
        const auto contrast = (bestMagnitude - localMean) / juce::jmax(bestMagnitude, 1.0e-12);
        const auto strength = bestMagnitude / juce::jmax(signalRms * static_cast<double>(sampleCount) * 0.25, 1.0e-9);
        const auto strengthReliability = juce::jlimit(0.0, 1.0, (strength - 0.035) / 0.62);
        const auto contrastReliability = juce::jlimit(0.35, 1.0, 0.62 + (contrast * 1.85));
        reliability = juce::jlimit(0.0, 1.0, strengthReliability * contrastReliability * baseConfidence);

        if (reliability < 0.055)
            return fallbackCents;

        return juce::jlimit(-42.0, 42.0, estimatedCents);
    }

    double PitchTracker::smoothResolvedSpeed(int rowIndex, double rawSpeedCents, double reliability)
    {
        const auto index = juce::jlimit(0, static_cast<int>(previousResolvedSpeedCents.size()) - 1, rowIndex);

        if (! hasResolvedRows)
        {
            previousResolvedSpeedCents[static_cast<size_t>(index)] = rawSpeedCents;
            previousResolvedReliability[static_cast<size_t>(index)] = reliability;
            return rawSpeedCents;
        }

        const auto previous = previousResolvedSpeedCents[static_cast<size_t>(index)];
        auto alpha = juce::jmap(juce::jlimit(0.0, 1.0, reliability), 0.0, 1.0, 0.10, 0.58);
        if (std::abs(rawSpeedCents - previous) > 80.0)
            alpha = juce::jmax(alpha, 0.74);

        const auto smoothed = previous + ((rawSpeedCents - previous) * alpha);
        previousResolvedSpeedCents[static_cast<size_t>(index)] = smoothed;
        previousResolvedReliability[static_cast<size_t>(index)] = reliability;
        return smoothed;
    }

    double PitchTracker::smoothFrequency(double nextFrequencyHz, double confidence, TrackingMode mode)
    {
        if (! std::isfinite(nextFrequencyHz) || nextFrequencyHz <= 0.0)
            return previousStableHz;

        if (! hasStableFrequency || previousStableHz <= 0.0 || confidence < 0.28)
        {
            previousStableHz = nextFrequencyHz;
            hasStableFrequency = true;
            return previousStableHz;
        }

        const auto deltaCents = centsBetween(nextFrequencyHz, previousStableHz);
        auto alpha = smoothingForMode(mode);

        if (std::abs(deltaCents) > 85.0)
            alpha = 0.82;
        else if (std::abs(deltaCents) > 25.0)
            alpha = juce::jmax(alpha, 0.56);

        alpha *= juce::jmap(juce::jlimit(0.30, 1.0, confidence), 0.30, 1.0, 0.55, 1.0);
        previousStableHz *= std::pow(2.0, (deltaCents * alpha) / centsPerOctave);
        return previousStableHz;
    }

    double PitchTracker::parabolicOffset(double previous, double current, double next) noexcept
    {
        const auto denominator = previous - (2.0 * current) + next;
        if (std::abs(denominator) < std::numeric_limits<double>::epsilon())
            return 0.0;

        return juce::jlimit(-0.5, 0.5, 0.5 * (previous - next) / denominator);
    }

    double PitchTracker::centsBetween(double measuredHz, double targetHz) noexcept
    {
        if (measuredHz <= 0.0 || targetHz <= 0.0 || ! std::isfinite(measuredHz) || ! std::isfinite(targetHz))
            return 0.0;

        return centsPerOctave * std::log2(measuredHz / targetHz);
    }
}
