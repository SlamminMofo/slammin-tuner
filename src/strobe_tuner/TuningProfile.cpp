#include "TuningProfile.h"

#include <cmath>
#include <initializer_list>
#include <limits>

namespace guitarforge::tuner
{
    namespace
    {
        constexpr double centsPerOctave = 1200.0;
        constexpr double semitoneRatio = 12.0;
        constexpr int midiA4 = 69;

        const char* const flatNames[] =
        {
            "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
        };

        const char* const sharpNames[] =
        {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };

        struct StringSpec
        {
            int midi = 69;
            const char* label = "";
            double offsetCents = 0.0;
        };

        TargetNote makeTarget(int midi, int stringIndex, const char* stringLabel, double offsetCents = 0.0)
        {
            TargetNote target;
            target.midi = midi;
            target.stringIndex = stringIndex;
            target.stringLabel = stringLabel;
            target.offsetCents = offsetCents;
            return target;
        }

        TuningProfile makeStringProfile(const char* id,
                                        const char* name,
                                        const char* description,
                                        std::initializer_list<StringSpec> strings,
                                        double defaultA4Hz = 440.0,
                                        double globalOffsetCents = 0.0)
        {
            TuningProfile profile;
            profile.id = id;
            profile.name = name;
            profile.description = description;
            profile.defaultA4Hz = defaultA4Hz;

            int stringIndex = 0;
            for (const auto& string : strings)
            {
                profile.targets.add(makeTarget(string.midi,
                                               stringIndex,
                                               string.label,
                                               string.offsetCents + globalOffsetCents));
                ++stringIndex;
            }

            return profile;
        }

        TuningProfile makeChromatic()
        {
            TuningProfile profile;
            profile.id = "chromatic-equal";
            profile.name = "Chromatic Equal";
            profile.description = "Flat-spelled chromatic equal temperament.";
            profile.chromatic = true;
            return profile;
        }

        TuningProfile makeSevenStringStandard()
        {
            return makeStringProfile("guitar-7-standard",
                                     "7 String Guitar",
                                     "Standard 7-string electric guitar: B E A D G B E.",
                                     { { 35, "7" }, { 40, "6" }, { 45, "5" }, { 50, "4" },
                                       { 55, "3" }, { 59, "2" }, { 64, "1" } });
        }

        TuningProfile makeSixStringBass()
        {
            return makeStringProfile("bass-6-standard",
                                     "6 String Bass",
                                     "Standard 6-string bass: B E A D G C.",
                                     { { 23, "6" }, { 28, "5" }, { 33, "4" },
                                       { 38, "3" }, { 43, "2" }, { 48, "1" } });
        }

        TuningProfile makeDropASevenString()
        {
            return makeStringProfile("guitar-7-drop-a",
                                     "7 String Drop A",
                                     "7-string drop A: A E A D G B E.",
                                     { { 33, "7" }, { 40, "6" }, { 45, "5" }, { 50, "4" },
                                       { 55, "3" }, { 59, "2" }, { 64, "1" } });
        }

        TuningProfile makeSevenStringHalfStepDown()
        {
            return makeStringProfile("guitar-7-half-step-down",
                                     "7 String Half Down",
                                     "7-string guitar tuned down one semitone: Bb Eb Ab Db Gb Bb Eb.",
                                     { { 34, "7" }, { 39, "6" }, { 44, "5" }, { 49, "4" },
                                       { 54, "3" }, { 58, "2" }, { 63, "1" } });
        }

        TuningProfile makeGuitarStandard()
        {
            return makeStringProfile("guitar-standard",
                                     "Guitar Standard",
                                     "Standard 6-string guitar: E A D G B E.",
                                     { { 40, "6" }, { 45, "5" }, { 50, "4" },
                                       { 55, "3" }, { 59, "2" }, { 64, "1" } });
        }

        TuningProfile makeHalfStepDown()
        {
            return makeStringProfile("guitar-half-step-down",
                                     "Half-Step Down",
                                     "6-string guitar tuned down one semitone: Eb Ab Db Gb Bb Eb.",
                                     { { 39, "6" }, { 44, "5" }, { 49, "4" },
                                       { 54, "3" }, { 58, "2" }, { 63, "1" } });
        }

        TuningProfile makeFullStepDown()
        {
            return makeStringProfile("guitar-full-step-down",
                                     "Full-Step Down",
                                     "6-string guitar tuned down one whole tone: D G C F A D.",
                                     { { 38, "6" }, { 43, "5" }, { 48, "4" },
                                       { 53, "3" }, { 57, "2" }, { 62, "1" } });
        }

        TuningProfile makeCSharpStandard()
        {
            return makeStringProfile("guitar-csharp-standard",
                                     "C# Standard",
                                     "6-string guitar tuned down one and a half steps: C# F# B E G# C#.",
                                     { { 37, "6" }, { 42, "5" }, { 47, "4" },
                                       { 52, "3" }, { 56, "2" }, { 61, "1" } });
        }

        TuningProfile makeCStandard()
        {
            return makeStringProfile("guitar-c-standard",
                                     "C Standard",
                                     "6-string guitar tuned down two whole tones: C F Bb Eb G C.",
                                     { { 36, "6" }, { 41, "5" }, { 46, "4" },
                                       { 51, "3" }, { 55, "2" }, { 60, "1" } });
        }

        TuningProfile makeDropD()
        {
            return makeStringProfile("drop-d",
                                     "Drop D",
                                     "Standard guitar with the low E dropped to D: D A D G B E.",
                                     { { 38, "6" }, { 45, "5" }, { 50, "4" },
                                       { 55, "3" }, { 59, "2" }, { 64, "1" } });
        }

        TuningProfile makeDropDb()
        {
            return makeStringProfile("drop-db",
                                     "Drop Db",
                                     "Drop D lowered one semitone: Db Ab Db Gb Bb Eb.",
                                     { { 37, "6" }, { 44, "5" }, { 49, "4" },
                                       { 54, "3" }, { 58, "2" }, { 63, "1" } });
        }

        TuningProfile makeDropC()
        {
            return makeStringProfile("drop-c",
                                     "Drop C",
                                     "Drop tuning one whole step below Drop D: C G C F A D.",
                                     { { 36, "6" }, { 43, "5" }, { 48, "4" },
                                       { 53, "3" }, { 57, "2" }, { 62, "1" } });
        }

        TuningProfile makeDropB()
        {
            return makeStringProfile("drop-b",
                                     "Drop B",
                                     "Modern heavy drop tuning: B F# B E G# C#.",
                                     { { 35, "6" }, { 42, "5" }, { 47, "4" },
                                       { 52, "3" }, { 56, "2" }, { 61, "1" } });
        }

        TuningProfile makeDropA()
        {
            return makeStringProfile("drop-a",
                                     "Drop A",
                                     "Low drop tuning for baritone-range guitar: A E A D F# B.",
                                     { { 33, "6" }, { 40, "5" }, { 45, "4" },
                                       { 50, "3" }, { 54, "2" }, { 59, "1" } });
        }

        TuningProfile makeDadgad()
        {
            return makeStringProfile("dadgad",
                                     "DADGAD",
                                     "Modal D suspended tuning: D A D G A D.",
                                     { { 38, "6" }, { 45, "5" }, { 50, "4" },
                                       { 55, "3" }, { 57, "2" }, { 62, "1" } });
        }

        TuningProfile makeOpenG()
        {
            return makeStringProfile("open-g",
                                     "Open G",
                                     "Open G major tuning: D G D G B D.",
                                     { { 38, "6" }, { 43, "5" }, { 50, "4" },
                                       { 55, "3" }, { 59, "2" }, { 62, "1" } });
        }

        TuningProfile makeOpenD()
        {
            return makeStringProfile("open-d",
                                     "Open D",
                                     "Open D major tuning: D A D F# A D.",
                                     { { 38, "6" }, { 45, "5" }, { 50, "4" },
                                       { 54, "3" }, { 57, "2" }, { 62, "1" } });
        }

        TuningProfile makeOpenE()
        {
            return makeStringProfile("open-e",
                                     "Open E",
                                     "Open E major tuning: E B E G# B E.",
                                     { { 40, "6" }, { 47, "5" }, { 52, "4" },
                                       { 56, "3" }, { 59, "2" }, { 64, "1" } });
        }

        TuningProfile makeOpenC()
        {
            return makeStringProfile("open-c",
                                     "Open C",
                                     "Open C major tuning: C G C G C E.",
                                     { { 36, "6" }, { 43, "5" }, { 48, "4" },
                                       { 55, "3" }, { 60, "2" }, { 64, "1" } });
        }

        TuningProfile makeOpenA()
        {
            return makeStringProfile("open-a",
                                     "Open A",
                                     "Open A major tuning: E A E A C# E.",
                                     { { 40, "6" }, { 45, "5" }, { 52, "4" },
                                       { 57, "3" }, { 61, "2" }, { 64, "1" } });
        }

        TuningProfile makeOpenF()
        {
            return makeStringProfile("open-f",
                                     "Open F",
                                     "Open F major tuning: C F C F A C.",
                                     { { 36, "6" }, { 41, "5" }, { 48, "4" },
                                       { 53, "3" }, { 57, "2" }, { 60, "1" } });
        }

        TuningProfile makeOpenB()
        {
            return makeStringProfile("open-b",
                                     "Open B",
                                     "Open B major tuning: B F# B F# B D#.",
                                     { { 35, "6" }, { 42, "5" }, { 47, "4" },
                                       { 54, "3" }, { 59, "2" }, { 63, "1" } });
        }

        TuningProfile makeOpenDMinor()
        {
            return makeStringProfile("open-d-minor",
                                     "Open D Minor",
                                     "Open D minor tuning: D A D F A D.",
                                     { { 38, "6" }, { 45, "5" }, { 50, "4" },
                                       { 53, "3" }, { 57, "2" }, { 62, "1" } });
        }

        TuningProfile makeOpenGMinor()
        {
            return makeStringProfile("open-g-minor",
                                     "Open G Minor",
                                     "Open G minor tuning: D G D G Bb D.",
                                     { { 38, "6" }, { 43, "5" }, { 50, "4" },
                                       { 55, "3" }, { 58, "2" }, { 62, "1" } });
        }

        TuningProfile makeAllFourths()
        {
            return makeStringProfile("all-fourths",
                                     "All Fourths",
                                     "Symmetrical fourths tuning: E A D G C F.",
                                     { { 40, "6" }, { 45, "5" }, { 50, "4" },
                                       { 55, "3" }, { 60, "2" }, { 65, "1" } });
        }

        TuningProfile makeNashvilleHigh()
        {
            return makeStringProfile("nashville-high",
                                     "Nashville High",
                                     "High-strung Nashville tuning with the lower four courses raised an octave.",
                                     { { 52, "6" }, { 57, "5" }, { 62, "4" },
                                       { 67, "3" }, { 59, "2" }, { 64, "1" } });
        }

        TuningProfile makeBaritoneB()
        {
            return makeStringProfile("baritone-b",
                                     "Baritone B",
                                     "Baritone standard tuning: B E A D F# B.",
                                     { { 35, "6" }, { 40, "5" }, { 45, "4" },
                                       { 50, "3" }, { 54, "2" }, { 59, "1" } });
        }

        TuningProfile makeShelfSweetener()
        {
            return makeStringProfile("shelf-sweetener",
                                     "Shelf Sweetener",
                                     "A mild non-branded sweetened standard profile inspired by compensated-nut systems.",
                                     { { 40, "6", -2.0 }, { 45, "5", -1.5 }, { 50, "4", -1.0 },
                                       { 55, "3", -1.0 }, { 59, "2", -2.0 }, { 64, "1", -2.0 } });
        }

        TuningProfile makeVHHalfFlat()
        {
            return makeStringProfile("vh-half-flat",
                                     "VH Half-Flat",
                                     "Half-step down with the B-string course sweetened slightly flat.",
                                     { { 39, "6" }, { 44, "5" }, { 49, "4" },
                                       { 54, "3" }, { 58, "2", -12.0 }, { 63, "1" } });
        }

        TuningProfile makeBassDropD()
        {
            return makeStringProfile("bass-drop-d",
                                     "Bass Drop D",
                                     "4-string bass drop D: D A D G.",
                                     { { 26, "4" }, { 33, "3" }, { 38, "2" }, { 43, "1" } });
        }

        TuningProfile makeFiveStringBass()
        {
            return makeStringProfile("bass-5-standard",
                                     "5 String Bass",
                                     "Standard 5-string bass: B E A D G.",
                                     { { 23, "5" }, { 28, "4" }, { 33, "3" },
                                       { 38, "2" }, { 43, "1" } });
        }

        double propertyAsDouble(juce::DynamicObject* object, const juce::Identifier& name, double fallback)
        {
            if (object == nullptr || ! object->hasProperty(name))
                return fallback;

            return static_cast<double>(object->getProperty(name));
        }

        juce::String propertyAsString(juce::DynamicObject* object,
                                      const juce::Identifier& name,
                                      const juce::String& fallback = {})
        {
            if (object == nullptr || ! object->hasProperty(name))
                return fallback;

            return object->getProperty(name).toString();
        }
    }

    double TargetNote::getFrequencyHz(double a4Hz, const std::array<double, 12>& noteClassOffsets) const noexcept
    {
        const auto pitchClass = ((midi % 12) + 12) % 12;
        const auto totalOffsetCents = offsetCents + noteClassOffsets[static_cast<size_t>(pitchClass)];
        return a4Hz * std::pow(2.0, (static_cast<double>(midi - midiA4) / semitoneRatio) + (totalOffsetCents / centsPerOctave));
    }

    juce::String TargetNote::getFlatNameWithOctave() const
    {
        return TuningProfileLibrary::flatNameWithOctaveForMidi(midi);
    }

    juce::String TargetNote::getFlatName() const
    {
        return TuningProfileLibrary::flatNameForMidi(midi);
    }

    bool TuningProfile::isValid() const noexcept
    {
        return chromatic || ! targets.isEmpty();
    }

    int TuningProfileLibrary::getBuiltInProfileCount() noexcept
    {
        return customProfileIndex;
    }

    int TuningProfileLibrary::getProfileChoiceCount() noexcept
    {
        return getBuiltInProfileCount() + 1;
    }

    juce::StringArray TuningProfileLibrary::getProfileChoiceNames()
    {
        juce::StringArray names;
        for (int index = 0; index < getBuiltInProfileCount(); ++index)
            names.add(getBuiltInProfile(index).name);

        names.add("Custom");
        return names;
    }

    TuningProfile TuningProfileLibrary::getBuiltInProfile(int index)
    {
        switch (index)
        {
            case 0:  return makeChromatic();
            case 1:  return makeGuitarStandard();
            case 2:  return makeHalfStepDown();
            case 3:  return makeFullStepDown();
            case 4:  return makeCSharpStandard();
            case 5:  return makeCStandard();
            case 6:  return makeDropD();
            case 7:  return makeDropDb();
            case 8:  return makeDropC();
            case 9:  return makeDropB();
            case 10: return makeDropA();
            case 11: return makeDadgad();
            case 12: return makeOpenG();
            case 13: return makeOpenD();
            case 14: return makeOpenE();
            case 15: return makeOpenC();
            case 16: return makeOpenA();
            case 17: return makeOpenF();
            case 18: return makeOpenB();
            case 19: return makeOpenDMinor();
            case 20: return makeOpenGMinor();
            case 21: return makeAllFourths();
            case 22: return makeNashvilleHigh();
            case 23: return makeBaritoneB();
            case 24: return makeShelfSweetener();
            case 25: return makeVHHalfFlat();
            case 26: return makeSevenStringStandard();
            case 27: return makeDropASevenString();
            case 28: return makeSevenStringHalfStepDown();
            case 29: return makeSixStringBass();
            case 30: return makeBassDropD();
            case 31: return makeFiveStringBass();
            default: return makeChromatic();
        }
    }

    TuningProfile TuningProfileLibrary::getDefaultChromaticProfile()
    {
        return makeChromatic();
    }

    TargetNote TuningProfileLibrary::findNearestTarget(double measuredHz,
                                                       double a4Hz,
                                                       const TuningProfile& profile)
    {
        if (! std::isfinite(measuredHz) || measuredHz <= 0.0)
            return makeTarget(midiA4, -1, "");

        if (profile.chromatic || profile.targets.isEmpty())
        {
            TargetNote target;
            target.midi = nearestMidiForFrequency(measuredHz, a4Hz);
            target.stringIndex = -1;
            target.offsetCents = 0.0;
            return target;
        }

        auto best = profile.targets.getFirst();
        auto bestDistance = std::numeric_limits<double>::max();

        for (const auto& candidate : profile.targets)
        {
            const auto targetHz = candidate.getFrequencyHz(a4Hz, profile.noteClassOffsetsCents);
            const auto cents = std::abs(centsPerOctave * std::log2(measuredHz / targetHz));
            if (cents < bestDistance)
            {
                bestDistance = cents;
                best = candidate;
            }
        }

        return best;
    }

    juce::String TuningProfileLibrary::flatNameForMidi(int midi)
    {
        return flatNames[noteClassForMidi(midi)];
    }

    juce::String TuningProfileLibrary::flatNameWithOctaveForMidi(int midi)
    {
        const auto octave = (midi / 12) - 1;
        return flatNameForMidi(midi) + juce::String(octave);
    }

    juce::String TuningProfileLibrary::sharpNameForMidi(int midi)
    {
        return sharpNames[noteClassForMidi(midi)];
    }

    juce::String TuningProfileLibrary::sharpNameWithOctaveForMidi(int midi)
    {
        const auto octave = (midi / 12) - 1;
        return sharpNameForMidi(midi) + juce::String(octave);
    }

    int TuningProfileLibrary::nearestMidiForFrequency(double frequencyHz, double a4Hz) noexcept
    {
        if (! std::isfinite(frequencyHz) || frequencyHz <= 0.0 || ! std::isfinite(a4Hz) || a4Hz <= 0.0)
            return midiA4;

        return juce::roundToInt(static_cast<float>(midiA4 + (12.0 * std::log2(frequencyHz / a4Hz))));
    }

    bool TuningProfileLibrary::parseProfileJson(const juce::String& jsonText,
                                                TuningProfile& profile,
                                                juce::String& errorMessage)
    {
        auto parsed = juce::JSON::parse(jsonText);
        auto* object = parsed.getDynamicObject();
        if (object == nullptr)
        {
            errorMessage = "Profile JSON must contain an object.";
            return false;
        }

        TuningProfile result;
        result.id = propertyAsString(object, "id", "custom");
        result.name = propertyAsString(object, "name", "Custom");
        result.description = propertyAsString(object, "description");
        result.defaultA4Hz = propertyAsDouble(object, "a4Hz", 440.0);
        result.chromatic = static_cast<bool>(object->getProperty("chromatic"));

        if (auto* offsets = object->getProperty("noteClassOffsetsCents").getDynamicObject())
        {
            for (int index = 0; index < 12; ++index)
            {
                const auto name = juce::Identifier(flatNames[index]);
                if (offsets->hasProperty(name))
                    result.noteClassOffsetsCents[static_cast<size_t>(index)] = static_cast<double>(offsets->getProperty(name));
            }
        }

        auto targetsVar = object->getProperty("stringTargets");
        if (targetsVar.isArray())
        {
            auto* array = targetsVar.getArray();
            for (int index = 0; array != nullptr && index < array->size(); ++index)
            {
                auto* targetObject = array->getReference(index).getDynamicObject();
                if (targetObject == nullptr)
                    continue;

                TargetNote target;
                target.stringIndex = juce::roundToInt(static_cast<float>(propertyAsDouble(targetObject, "stringIndex", index)));
                target.stringLabel = propertyAsString(targetObject, "stringLabel", juce::String(index + 1));
                target.offsetCents = propertyAsDouble(targetObject, "offsetCents", 0.0);

                if (targetObject->hasProperty("midi"))
                {
                    target.midi = juce::roundToInt(static_cast<float>(targetObject->getProperty("midi")));
                }
                else
                {
                    bool ok = false;
                    target.midi = midiFromNoteName(propertyAsString(targetObject, "note"), ok);
                    if (! ok)
                        continue;
                }

                result.targets.add(target);
            }
        }

        if (! result.isValid())
        {
            errorMessage = "Profile needs chromatic=true or at least one string target.";
            return false;
        }

        profile = result;
        errorMessage.clear();
        return true;
    }

    int TuningProfileLibrary::noteClassForMidi(int midi) noexcept
    {
        return ((midi % 12) + 12) % 12;
    }

    int TuningProfileLibrary::midiFromNoteName(const juce::String& text, bool& ok)
    {
        ok = false;
        auto trimmed = text.trim();
        if (trimmed.isEmpty())
            return midiA4;

        auto notePart = trimmed.initialSectionContainingOnly("ABCDEFGabcdefg#b");
        auto octavePart = trimmed.substring(notePart.length()).trim();
        if (notePart.isEmpty() || octavePart.isEmpty())
            return midiA4;

        notePart = notePart.toUpperCase().replace("#", "S");
        int pitchClass = -1;
        if (notePart == "C") pitchClass = 0;
        else if (notePart == "CS" || notePart == "DB") pitchClass = 1;
        else if (notePart == "D") pitchClass = 2;
        else if (notePart == "DS" || notePart == "EB") pitchClass = 3;
        else if (notePart == "E") pitchClass = 4;
        else if (notePart == "F") pitchClass = 5;
        else if (notePart == "FS" || notePart == "GB") pitchClass = 6;
        else if (notePart == "G") pitchClass = 7;
        else if (notePart == "GS" || notePart == "AB") pitchClass = 8;
        else if (notePart == "A") pitchClass = 9;
        else if (notePart == "AS" || notePart == "BB") pitchClass = 10;
        else if (notePart == "B") pitchClass = 11;

        if (pitchClass < 0)
            return midiA4;

        const auto octave = octavePart.getIntValue();
        ok = true;
        return ((octave + 1) * 12) + pitchClass;
    }
}
