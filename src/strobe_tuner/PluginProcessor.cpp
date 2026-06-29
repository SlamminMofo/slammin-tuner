#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    constexpr float referenceToneGain = 0.080f;
    constexpr int editorMinWidth = 900;
    constexpr int editorMinHeight = 620;
    constexpr int editorMaxWidth = 1680;
    constexpr int editorMaxHeight = 1040;
    constexpr const char* editorWidthProperty = "editorWidth";
    constexpr const char* editorHeightProperty = "editorHeight";

    std::atomic<float>* raw(juce::AudioProcessorValueTreeState& state, const char* id)
    {
        return state.getRawParameterValue(id);
    }

    float valueFromUnitText(const juce::String& text, float fallback)
    {
        const auto numeric = text.trim()
                                 .replaceCharacter(',', '.')
                                 .retainCharacters("0123456789.-+");

        return numeric.isNotEmpty() ? numeric.getFloatValue() : fallback;
    }

    juce::File userPreferencesDirectory()
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("Slammin Captures")
            .getChildFile("Slammin Tuner");
    }

    juce::File currentUserPreferencesFile()
    {
        return userPreferencesDirectory().getChildFile("Slammin Tuner V125.settings.xml");
    }
}

GuitarForgeStrobeTunerAudioProcessor::AnalysisThread::AnalysisThread(GuitarForgeStrobeTunerAudioProcessor& ownerProcessor)
    : juce::Thread("Slammin Tuner Analysis"),
      owner(ownerProcessor)
{
}

void GuitarForgeStrobeTunerAudioProcessor::AnalysisThread::run()
{
    while (! threadShouldExit())
    {
        owner.runAnalysisTick();
        wait(10);
    }
}

GuitarForgeStrobeTunerAudioProcessor::GuitarForgeStrobeTunerAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "SlamminTunerV1", createParameterLayout()),
      analysisThread(*this)
{
    latestSnapshot.profileName = guitarforge::tuner::TuningProfileLibrary::getDefaultChromaticProfile().name;
    loadUserPreferences();
    analysisThread.startThread();
}

GuitarForgeStrobeTunerAudioProcessor::~GuitarForgeStrobeTunerAudioProcessor()
{
    saveUserPreferences();
    analysisThread.stopThread(1000);
}

juce::AudioProcessorValueTreeState::ParameterLayout GuitarForgeStrobeTunerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIds::a4Hz, 1 },
        "A4 Hz",
        juce::NormalisableRange<float>(390.0f, 490.0f, 0.01f),
        440.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float value, int)
        {
            return juce::String(value, 2) + " Hz";
        }).withValueFromStringFunction([](const juce::String& text)
        {
            return valueFromUnitText(text, 440.0f);
        })));

    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParameterIds::profileIndex, 1 },
        "Tuning",
        guitarforge::tuner::TuningProfileLibrary::getProfileChoiceNames(),
        0));

    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParameterIds::trackingMode, 1 },
        "Tracking",
        juce::StringArray { "Balanced", "Fast", "Precision" },
        0));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIds::strobeSensitivity, 1 },
        "Duration",
        juce::NormalisableRange<float>(0.25f, 3.0f, 0.01f),
        1.25f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float value, int)
        {
            return juce::String(value, 2) + "x";
        }).withValueFromStringFunction([](const juce::String& text)
        {
            return valueFromUnitText(text, 1.25f);
        })));

    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParameterIds::strobeColour, 1 },
        "Color",
        getColourChoiceNames(),
        0));

    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParameterIds::strobeMode, 1 },
        "Strobe Mode",
        getStrobeModeChoiceNames(),
        3));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParameterIds::strobeRows, 1 },
        "Strobe Rows",
        juce::NormalisableRange<float>(1.0f, 4.0f, 1.0f),
        4.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float value, int)
        {
            return juce::String(juce::jlimit(1, 4, juce::roundToInt(value)));
        }).withValueFromStringFunction([](const juce::String& text)
        {
            return valueFromUnitText(text, 4.0f);
        })));

    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParameterIds::strobeRatioMode, 1 },
        "Strobe Width",
        juce::StringArray { "Wide", "Standard", "Compact" },
        1));

    layout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParameterIds::preferSharps, 1 },
        "Sharp Note Names",
        false));

    layout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParameterIds::bandResolvedStrobe, 1 },
        "True Strobe",
        false));

    layout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParameterIds::barContrast, 1 },
        "Bar Contrast",
        false));

    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParameterIds::inputMode, 1 },
        "Input",
        juce::StringArray { "Auto", "Left", "Right", "Mid" },
        0));

    layout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParameterIds::muteOutput, 1 },
        "Mute Output",
        false));

    layout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParameterIds::referenceToneEnabled, 1 },
        "Reference Tone",
        false));

    layout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParameterIds::reducedMotion, 1 },
        "Reduced Motion",
        false));

    return { layout.begin(), layout.end() };
}

void GuitarForgeStrobeTunerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    preparedSampleRate.store(sampleRate, std::memory_order_release);
    const auto frameSeconds = sampleRate > 96000.0 ? 0.095 : 0.105;
    analysisDecimation = sampleRate > 96000.0 ? 4 : 2;
    analysisFrameSize = juce::jlimit(4096, 24576, juce::roundToInt(sampleRate * frameSeconds));
    trackerFrameSize = juce::jmax(1024, analysisFrameSize / analysisDecimation);
    const auto ringSize = nextPowerOfTwo(juce::jmax(analysisFrameSize * 4, juce::roundToInt(sampleRate * 3.0)));

    ringBuffer.assign(static_cast<size_t>(ringSize), 0.0f);
    analysisScratch.assign(static_cast<size_t>(analysisFrameSize), 0.0f);
    trackerScratch.assign(static_cast<size_t>(trackerFrameSize), 0.0f);
    ringMask = ringSize - 1;
    ringWriteIndex.store(0, std::memory_order_release);
    pitchTracker.prepare(sampleRate / static_cast<double>(analysisDecimation), trackerFrameSize);
    referencePhase = 0.0;
}

void GuitarForgeStrobeTunerAudioProcessor::releaseResources()
{
    preparedSampleRate.store(0.0, std::memory_order_release);
}

bool GuitarForgeStrobeTunerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& input = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();
    return input == output
        && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

void GuitarForgeStrobeTunerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    writeInputToRing(buffer, getInputMode());

    const auto sampleRate = preparedSampleRate.load(std::memory_order_acquire);
    const auto shouldMute = isMuteOutputEnabled();
    const auto referenceEnabled = isReferenceToneEnabled();

    if (shouldMute && ! referenceEnabled)
    {
        buffer.clear();
        return;
    }

    if (! referenceEnabled || sampleRate <= 0.0)
        return;

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    const auto frequency = juce::jlimit(20.0, 5000.0, referenceTargetHz.load(std::memory_order_acquire));
    const auto phaseStep = twoPi * frequency / sampleRate;

    for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
    {
        const auto tone = static_cast<float>(std::sin(referencePhase)) * referenceToneGain;
        referencePhase += phaseStep;
        if (referencePhase >= twoPi)
            referencePhase -= twoPi;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            channelData[sampleIndex] = shouldMute
                ? tone
                : clampSample((channelData[sampleIndex] * 0.55f) + tone);
        }
    }
}

juce::AudioProcessorEditor* GuitarForgeStrobeTunerAudioProcessor::createEditor()
{
    return new GuitarForgeStrobeTunerAudioProcessorEditor(*this);
}

bool GuitarForgeStrobeTunerAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String GuitarForgeStrobeTunerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool GuitarForgeStrobeTunerAudioProcessor::acceptsMidi() const
{
    return false;
}

bool GuitarForgeStrobeTunerAudioProcessor::producesMidi() const
{
    return false;
}

bool GuitarForgeStrobeTunerAudioProcessor::isMidiEffect() const
{
    return false;
}

double GuitarForgeStrobeTunerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int GuitarForgeStrobeTunerAudioProcessor::getNumPrograms()
{
    return 1;
}

int GuitarForgeStrobeTunerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void GuitarForgeStrobeTunerAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String GuitarForgeStrobeTunerAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void GuitarForgeStrobeTunerAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void GuitarForgeStrobeTunerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty(editorWidthProperty, savedEditorWidth.load(), nullptr);
    state.setProperty(editorHeightProperty, savedEditorHeight.load(), nullptr);

    {
        const juce::ScopedLock lock(customProfileLock);
        state.setProperty("customProfileJson", customProfileJson, nullptr);
        state.setProperty("customProfilePath", customProfilePath, nullptr);
        state.setProperty("customProfileStatus", customProfileStatus, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void GuitarForgeStrobeTunerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml(*xml);
        if (state.isValid())
            restoreStateTree(state, true);
    }
}

void GuitarForgeStrobeTunerAudioProcessor::loadUserPreferences()
{
    auto file = currentUserPreferencesFile();
    if (! file.existsAsFile())
    {
        const char* const fallbackFiles[] =
        {
            "Slammin Tuner V124.settings.xml",
            "Slammin Tuner V123.settings.xml",
            "Slammin Tuner V122.settings.xml",
            "Slammin Tuner V121.settings.xml",
            "Slammin Tuner V120.settings.xml",
            "Slammin Tuner.settings.xml"
        };

        for (const auto* fallbackFile : fallbackFiles)
        {
            const auto candidate = userPreferencesDirectory().getChildFile(fallbackFile);
            if (candidate.existsAsFile())
            {
                file = candidate;
                break;
            }
        }
    }

    if (! file.existsAsFile())
        return;

    if (auto xml = juce::XmlDocument::parse(file))
        if (const auto state = juce::ValueTree::fromXml(*xml); state.isValid())
            restoreStateTree(state, true);
}

void GuitarForgeStrobeTunerAudioProcessor::saveUserPreferences()
{
    auto state = parameters.copyState();
    state.setProperty(editorWidthProperty, savedEditorWidth.load(), nullptr);
    state.setProperty(editorHeightProperty, savedEditorHeight.load(), nullptr);

    {
        const juce::ScopedLock lock(customProfileLock);
        state.setProperty("customProfileJson", customProfileJson, nullptr);
        state.setProperty("customProfilePath", customProfilePath, nullptr);
        state.setProperty("customProfileStatus", customProfileStatus, nullptr);
    }

    if (auto xml = state.createXml())
    {
        const auto file = currentUserPreferencesFile();
        file.getParentDirectory().createDirectory();
        xml->writeTo(file);
    }
}

void GuitarForgeStrobeTunerAudioProcessor::restoreStateTree(const juce::ValueTree& state, bool allowCustomProfileRestore)
{
    if (! state.isValid())
        return;

    setSavedEditorSize(static_cast<int>(state.getProperty(editorWidthProperty, savedEditorWidth.load())),
                       static_cast<int>(state.getProperty(editorHeightProperty, savedEditorHeight.load())));

    if (allowCustomProfileRestore)
    {
        const auto json = state.getProperty("customProfileJson").toString();
        const auto path = state.getProperty("customProfilePath").toString();
        const auto status = state.getProperty("customProfileStatus").toString();

        if (json.isNotEmpty())
        {
            guitarforge::tuner::TuningProfile parsed;
            juce::String error;
            if (guitarforge::tuner::TuningProfileLibrary::parseProfileJson(json, parsed, error))
            {
                const juce::ScopedLock lock(customProfileLock);
                customProfile = parsed;
                customProfileJson = json;
                customProfilePath = path;
                customProfileStatus = status.isNotEmpty() ? status : ("Loaded " + parsed.name);
            }
        }
    }

    parameters.replaceState(state);
}

juce::Point<int> GuitarForgeStrobeTunerAudioProcessor::getSavedEditorSize() const noexcept
{
    return { juce::jlimit(editorMinWidth, editorMaxWidth, savedEditorWidth.load()),
             juce::jlimit(editorMinHeight, editorMaxHeight, savedEditorHeight.load()) };
}

void GuitarForgeStrobeTunerAudioProcessor::setSavedEditorSize(int width, int height) noexcept
{
    savedEditorWidth.store(juce::jlimit(editorMinWidth, editorMaxWidth, width));
    savedEditorHeight.store(juce::jlimit(editorMinHeight, editorMaxHeight, height));
}

void GuitarForgeStrobeTunerAudioProcessor::flushUserPreferences()
{
    saveUserPreferences();
}

guitarforge::tuner::TunerSnapshot GuitarForgeStrobeTunerAudioProcessor::getTunerSnapshot() const
{
    const juce::ScopedLock lock(snapshotLock);
    return latestSnapshot;
}

bool GuitarForgeStrobeTunerAudioProcessor::loadCustomProfileFromFile(const juce::File& file, juce::String& errorMessage)
{
    if (! file.existsAsFile())
    {
        errorMessage = "File does not exist.";
        return false;
    }

    const auto json = file.loadFileAsString();
    guitarforge::tuner::TuningProfile parsed;
    if (! guitarforge::tuner::TuningProfileLibrary::parseProfileJson(json, parsed, errorMessage))
        return false;

    {
        const juce::ScopedLock lock(customProfileLock);
        customProfile = parsed;
        customProfileJson = json;
        customProfilePath = file.getFullPathName();
        customProfileStatus = "Loaded " + parsed.name;
    }

    if (auto* parameter = parameters.getParameter(ParameterIds::profileIndex))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(guitarforge::tuner::TuningProfileLibrary::customProfileIndex)));

    return true;
}

juce::String GuitarForgeStrobeTunerAudioProcessor::getCustomProfileStatus() const
{
    const juce::ScopedLock lock(customProfileLock);
    return customProfileStatus;
}

juce::String GuitarForgeStrobeTunerAudioProcessor::getActiveProfileName() const
{
    return getActiveProfile().name;
}

juce::String GuitarForgeStrobeTunerAudioProcessor::getActiveTuningSummary(bool preferSharps) const
{
    const auto profile = getActiveProfile();
    if (profile.chromatic || profile.targets.isEmpty())
        return preferSharps
            ? "Chromatic: C  C#  D  D#  E  F  F#  G  G#  A  A#  B"
            : "Chromatic: C  Db  D  Eb  E  F  Gb  G  Ab  A  Bb  B";

    juce::StringArray strings;
    for (const auto& target : profile.targets)
    {
        const auto note = preferSharps
            ? guitarforge::tuner::TuningProfileLibrary::sharpNameWithOctaveForMidi(target.midi)
            : guitarforge::tuner::TuningProfileLibrary::flatNameWithOctaveForMidi(target.midi);

        auto text = target.stringLabel.isNotEmpty()
            ? (target.stringLabel + ":" + note)
            : note;

        if (std::abs(target.offsetCents) >= 0.01)
            text << " " << (target.offsetCents > 0.0 ? "+" : "") << juce::String(target.offsetCents, 1) << "c";

        strings.add(text);
    }

    auto summary = strings.joinIntoString("  ");
    if (std::abs(profile.defaultA4Hz - 440.0) >= 0.01)
        summary << "  A4 " << juce::String(profile.defaultA4Hz, 2);

    return summary;
}

double GuitarForgeStrobeTunerAudioProcessor::getStrobeSensitivity() const noexcept
{
    if (auto* value = raw(const_cast<AudioProcessorValueTreeState&>(parameters), ParameterIds::strobeSensitivity))
        return juce::jlimit(0.25, 3.0, static_cast<double>(value->load()));

    return 1.25;
}

int GuitarForgeStrobeTunerAudioProcessor::getStrobeMode() const noexcept
{
    if (auto* value = raw(const_cast<AudioProcessorValueTreeState&>(parameters), ParameterIds::strobeMode))
        return juce::jlimit(0, 4, juce::roundToInt(value->load()));

    return 3;
}

int GuitarForgeStrobeTunerAudioProcessor::getStrobeRowCount() const noexcept
{
    if (auto* value = raw(const_cast<AudioProcessorValueTreeState&>(parameters), ParameterIds::strobeRows))
        return juce::jlimit(1, 4, juce::roundToInt(value->load()));

    return 4;
}

bool GuitarForgeStrobeTunerAudioProcessor::isReducedMotionEnabled() const noexcept
{
    if (auto* value = raw(const_cast<AudioProcessorValueTreeState&>(parameters), ParameterIds::reducedMotion))
        return value->load() >= 0.5f;

    return false;
}

bool GuitarForgeStrobeTunerAudioProcessor::isAlternateStrobeDirectionEnabled() const noexcept
{
    const auto mode = getStrobeMode();
    return mode == 2 || mode == 3;
}

bool GuitarForgeStrobeTunerAudioProcessor::isSolidStrobeStyleEnabled() const noexcept
{
    const auto mode = getStrobeMode();
    return mode == 1 || mode == 3 || mode == 4;
}

bool GuitarForgeStrobeTunerAudioProcessor::isClassicStrobeModeEnabled() const noexcept
{
    return getStrobeMode() == 4;
}

int GuitarForgeStrobeTunerAudioProcessor::getStrobeRatioMode() const noexcept
{
    if (auto* value = raw(const_cast<AudioProcessorValueTreeState&>(parameters), ParameterIds::strobeRatioMode))
        return juce::jlimit(0, 2, juce::roundToInt(value->load()));

    return 1;
}

bool GuitarForgeStrobeTunerAudioProcessor::isSharpNotationEnabled() const noexcept
{
    if (auto* value = raw(const_cast<AudioProcessorValueTreeState&>(parameters), ParameterIds::preferSharps))
        return value->load() >= 0.5f;

    return false;
}

bool GuitarForgeStrobeTunerAudioProcessor::isBandResolvedStrobeEnabled() const noexcept
{
    if (auto* value = raw(const_cast<AudioProcessorValueTreeState&>(parameters), ParameterIds::bandResolvedStrobe))
        return value->load() >= 0.5f;

    return false;
}

bool GuitarForgeStrobeTunerAudioProcessor::isBarContrastEnabled() const noexcept
{
    if (auto* value = raw(const_cast<AudioProcessorValueTreeState&>(parameters), ParameterIds::barContrast))
        return value->load() >= 0.5f;

    return false;
}

juce::StringArray GuitarForgeStrobeTunerAudioProcessor::getColourChoiceNames()
{
    return {
        "Neon Cyan",
        "Neon Pink",
        "Acid Green",
        "Laser Blue",
        "Hot Amber",
        "Violet",
        "Signal Red",
        "Mint",
        "Ice White",
        "Gold",
        "Orange",
        "Teal",
        "Deep Purple",
        "Steel Blue",
        "Warm Cream"
    };
}

juce::StringArray GuitarForgeStrobeTunerAudioProcessor::getStrobeModeChoiceNames()
{
    return {
        "Same Flow 1",
        "Same Flow 2",
        "Alt Flow 1",
        "Alt Flow 2",
        "Classic"
    };
}

void GuitarForgeStrobeTunerAudioProcessor::runAnalysisTick()
{
    const auto sampleRate = preparedSampleRate.load(std::memory_order_acquire);
    if (sampleRate <= 0.0 || ringBuffer.empty() || trackerScratch.empty() || ringMask <= 0)
        return;

    const auto writeIndex = ringWriteIndex.load(std::memory_order_acquire);
    if (writeIndex < static_cast<size_t>(analysisFrameSize))
        return;

    const auto start = writeIndex - static_cast<size_t>(analysisFrameSize);
    for (int index = 0; index < trackerFrameSize; ++index)
    {
        double sum = 0.0;
        const auto sourceBase = start + (static_cast<size_t>(index) * static_cast<size_t>(analysisDecimation));
        for (int offset = 0; offset < analysisDecimation; ++offset)
            sum += ringBuffer[(sourceBase + static_cast<size_t>(offset)) & static_cast<size_t>(ringMask)];

        trackerScratch[static_cast<size_t>(index)] = static_cast<float>(sum / static_cast<double>(analysisDecimation));
    }

    const auto profile = getActiveProfile();
    auto snapshot = pitchTracker.analyse(trackerScratch.data(),
                                         trackerFrameSize,
                                         getA4Hz(),
                                         profile,
                                         getTrackingMode(),
                                         isBandResolvedStrobeEnabled());

    referenceTargetHz.store(snapshot.pitch.targetHz, std::memory_order_release);

    const juce::ScopedLock lock(snapshotLock);
    latestSnapshot = snapshot;
}

void GuitarForgeStrobeTunerAudioProcessor::writeInputToRing(const juce::AudioBuffer<float>& buffer, int inputMode)
{
    if (ringBuffer.empty() || ringMask <= 0 || buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
        return;

    const auto numSamples = buffer.getNumSamples();
    const auto* left = buffer.getReadPointer(0);
    const auto* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : left;

    if (inputMode == 0 && buffer.getNumChannels() > 1)
    {
        double leftEnergy = 0.0;
        double rightEnergy = 0.0;
        for (int index = 0; index < numSamples; ++index)
        {
            leftEnergy += static_cast<double>(left[index]) * static_cast<double>(left[index]);
            rightEnergy += static_cast<double>(right[index]) * static_cast<double>(right[index]);
        }

        inputMode = rightEnergy > leftEnergy * 1.2 ? 2 : 1;
    }

    auto write = ringWriteIndex.load(std::memory_order_relaxed);
    for (int index = 0; index < numSamples; ++index)
    {
        float sample = 0.0f;
        switch (inputMode)
        {
            case 2:  sample = right[index]; break;
            case 3:  sample = (left[index] + right[index]) * 0.5f; break;
            case 1:
            default: sample = left[index]; break;
        }

        ringBuffer[write & static_cast<size_t>(ringMask)] = sample;
        ++write;
    }

    ringWriteIndex.store(write, std::memory_order_release);
}

guitarforge::tuner::TuningProfile GuitarForgeStrobeTunerAudioProcessor::getActiveProfile() const
{
    const auto profileIndex = juce::roundToInt(raw(const_cast<AudioProcessorValueTreeState&>(parameters), ParameterIds::profileIndex)->load());

    if (profileIndex == guitarforge::tuner::TuningProfileLibrary::customProfileIndex)
    {
        const juce::ScopedLock lock(customProfileLock);
        if (customProfile.isValid())
            return customProfile;
    }

    return guitarforge::tuner::TuningProfileLibrary::getBuiltInProfile(profileIndex);
}

guitarforge::tuner::TrackingMode GuitarForgeStrobeTunerAudioProcessor::getTrackingMode() const noexcept
{
    const auto value = juce::roundToInt(raw(const_cast<AudioProcessorValueTreeState&>(parameters), ParameterIds::trackingMode)->load());
    switch (value)
    {
        case 1:  return guitarforge::tuner::TrackingMode::fast;
        case 2:  return guitarforge::tuner::TrackingMode::precision;
        case 0:
        default: return guitarforge::tuner::TrackingMode::balanced;
    }
}

double GuitarForgeStrobeTunerAudioProcessor::getA4Hz() const noexcept
{
    return juce::jlimit(390.0, 490.0, static_cast<double>(raw(const_cast<AudioProcessorValueTreeState&>(parameters), ParameterIds::a4Hz)->load()));
}

bool GuitarForgeStrobeTunerAudioProcessor::isReferenceToneEnabled() const noexcept
{
    return raw(const_cast<AudioProcessorValueTreeState&>(parameters), ParameterIds::referenceToneEnabled)->load() >= 0.5f;
}

bool GuitarForgeStrobeTunerAudioProcessor::isMuteOutputEnabled() const noexcept
{
    return raw(const_cast<AudioProcessorValueTreeState&>(parameters), ParameterIds::muteOutput)->load() >= 0.5f;
}

int GuitarForgeStrobeTunerAudioProcessor::getInputMode() const noexcept
{
    return juce::roundToInt(raw(const_cast<AudioProcessorValueTreeState&>(parameters), ParameterIds::inputMode)->load());
}

int GuitarForgeStrobeTunerAudioProcessor::nextPowerOfTwo(int value) noexcept
{
    auto result = 1;
    while (result < value)
        result <<= 1;
    return result;
}

float GuitarForgeStrobeTunerAudioProcessor::clampSample(float value) noexcept
{
    return juce::jlimit(-1.0f, 1.0f, value);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GuitarForgeStrobeTunerAudioProcessor();
}
