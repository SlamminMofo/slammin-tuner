#include "PluginEditor.h"

#include "BinaryData.h"

#include <cmath>
#include <functional>
#include <iterator>
#include <string>

namespace
{
    constexpr int initialWidth = 1120;
    constexpr int initialHeight = 720;
    constexpr int minStrobeRows = 1;
    constexpr int maxStrobeRows = 4;
    constexpr float panelRadius = 8.0f;
    constexpr double a4MinHz = 390.0;
    constexpr double a4MaxHz = 490.0;
    constexpr double durationMinX = 0.25;
    constexpr double durationMaxX = 3.0;
    constexpr double editorSizeSettleMs = 550.0;

    juce::Colour backgroundTop()      { return juce::Colour::fromRGB(7, 10, 12); }
    juce::Colour backgroundBottom()   { return juce::Colour::fromRGB(15, 18, 20); }
    juce::Colour panelDark()          { return juce::Colour::fromRGB(14, 18, 21); }
    juce::Colour panelBright()        { return juce::Colour::fromRGB(25, 30, 33); }
    juce::Colour border()             { return juce::Colour::fromRGB(62, 71, 76); }
    juce::Colour textMain()           { return juce::Colour::fromRGB(238, 243, 244); }
    juce::Colour textMuted()          { return juce::Colour::fromRGB(142, 153, 158); }
    juce::Colour lockGreen()          { return juce::Colour::fromRGB(80, 246, 162); }
    juce::Colour flatBlue()           { return juce::Colour::fromRGB(74, 172, 255); }
    juce::Colour sharpAmber()         { return juce::Colour::fromRGB(255, 181, 82); }
    juce::Colour dangerRed()          { return juce::Colour::fromRGB(255, 68, 74); }

    juce::Colour accentColourForIndex(int index)
    {
        static const juce::Colour colours[] =
        {
            juce::Colour::fromRGB(0, 236, 255),
            juce::Colour::fromRGB(255, 51, 188),
            juce::Colour::fromRGB(117, 255, 64),
            juce::Colour::fromRGB(54, 126, 255),
            juce::Colour::fromRGB(255, 170, 58),
            juce::Colour::fromRGB(166, 93, 255),
            juce::Colour::fromRGB(255, 58, 76),
            juce::Colour::fromRGB(111, 255, 192),
            juce::Colour::fromRGB(238, 249, 255),
            juce::Colour::fromRGB(255, 215, 88),
            juce::Colour::fromRGB(255, 118, 47),
            juce::Colour::fromRGB(27, 202, 184),
            juce::Colour::fromRGB(116, 75, 210),
            juce::Colour::fromRGB(96, 154, 204),
            juce::Colour::fromRGB(255, 239, 202)
        };

        return colours[static_cast<size_t>(juce::jlimit(0, static_cast<int>(std::size(colours)) - 1, index))];
    }

    juce::Colour spectralColour(float normalised)
    {
        const auto x = juce::jlimit(0.0f, 1.0f, normalised);

        if (x < 0.24f)
            return dangerRed().interpolatedWith(juce::Colour::fromRGB(255, 63, 186), x / 0.24f);
        if (x < 0.50f)
            return juce::Colour::fromRGB(255, 63, 186).interpolatedWith(juce::Colour::fromRGB(74, 108, 255), (x - 0.24f) / 0.26f);
        if (x < 0.76f)
            return juce::Colour::fromRGB(74, 108, 255).interpolatedWith(juce::Colour::fromRGB(255, 63, 186), (x - 0.50f) / 0.26f);

        return juce::Colour::fromRGB(255, 63, 186).interpolatedWith(dangerRed(), (x - 0.76f) / 0.24f);
    }

    juce::Colour counterColourFor(juce::Colour colour)
    {
        return juce::Colour::fromRGB(255 - colour.getRed(),
                                     255 - colour.getGreen(),
                                     255 - colour.getBlue()).brighter(0.22f);
    }

    juce::String noteNameForMidi(int midi, bool preferSharps, bool withOctave)
    {
        if (withOctave)
            return preferSharps
                ? guitarforge::tuner::TuningProfileLibrary::sharpNameWithOctaveForMidi(midi)
                : guitarforge::tuner::TuningProfileLibrary::flatNameWithOctaveForMidi(midi);

        return preferSharps
            ? guitarforge::tuner::TuningProfileLibrary::sharpNameForMidi(midi)
            : guitarforge::tuner::TuningProfileLibrary::flatNameForMidi(midi);
    }

    juce::Colour colourForCents(double cents)
    {
        const auto magnitude = std::abs(cents);
        if (magnitude <= 0.6)
            return lockGreen();

        const auto edge = cents < 0.0 ? flatBlue() : sharpAmber();
        const auto amount = juce::jlimit(0.0, 1.0, magnitude / 18.0);
        return lockGreen().interpolatedWith(edge, static_cast<float>(amount));
    }

    juce::String centsText(double cents)
    {
        const auto prefix = cents > 0.0 ? "+" : "";
        return prefix + juce::String(cents, 2) + " cents";
    }

    juce::String hzText(double hz)
    {
        if (hz <= 0.0)
            return "--";

        return juce::String(hz, hz < 100.0 ? 3 : 2) + " Hz";
    }

    void drawSurface(juce::Graphics& g, juce::Rectangle<float> bounds, bool active = false)
    {
        juce::ColourGradient gradient(active ? panelBright().brighter(0.14f) : panelBright(),
                                      bounds.getX(), bounds.getY(),
                                      panelDark(), bounds.getX(), bounds.getBottom(),
                                      false);
        gradient.addColour(0.62, panelDark());
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(bounds, panelRadius);
        g.setColour(active ? border().brighter(0.25f) : border());
        g.drawRoundedRectangle(bounds, panelRadius, active ? 1.4f : 1.0f);
    }

    void styleCombo(juce::ComboBox& box)
    {
        box.setColour(juce::ComboBox::backgroundColourId, panelDark());
        box.setColour(juce::ComboBox::outlineColourId, border());
        box.setColour(juce::ComboBox::focusedOutlineColourId, lockGreen());
        box.setColour(juce::ComboBox::textColourId, textMain());
        box.setColour(juce::ComboBox::arrowColourId, lockGreen());
    }

    void styleSlider(juce::Slider& slider)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        slider.setColour(juce::Slider::backgroundColourId, panelDark());
        slider.setColour(juce::Slider::trackColourId, lockGreen());
        slider.setColour(juce::Slider::thumbColourId, textMain());
        slider.setColour(juce::Slider::textBoxTextColourId, textMain());
        slider.setColour(juce::Slider::textBoxBackgroundColourId, panelDark());
        slider.setColour(juce::Slider::textBoxOutlineColourId, border());
    }

    void styleButton(juce::Button& button)
    {
        button.setColour(juce::TextButton::buttonColourId, panelDark());
        button.setColour(juce::TextButton::buttonOnColourId, lockGreen().withAlpha(0.35f));
        button.setColour(juce::TextButton::textColourOffId, textMain());
        button.setColour(juce::TextButton::textColourOnId, textMain());
        button.setColour(juce::ToggleButton::textColourId, textMain());
        button.setColour(juce::ToggleButton::tickColourId, lockGreen());
        button.setColour(juce::ToggleButton::tickDisabledColourId, textMuted());
    }

    int currentChoice(juce::AudioProcessorValueTreeState& state, const char* id)
    {
        if (auto* value = state.getRawParameterValue(id))
            return juce::roundToInt(value->load());

        return 0;
    }

    juce::Font uiFont(float height, bool bold = false)
    {
        return juce::Font(juce::FontOptions(height, bold ? juce::Font::bold : juce::Font::plain));
    }

    double numericValueFromText(const juce::String& text, double fallback)
    {
        const auto cleaned = text.trim()
                                 .replaceCharacter(',', '.')
                                 .retainCharacters("0123456789.-+");

        return cleaned.isNotEmpty() ? cleaned.getDoubleValue() : fallback;
    }

    juce::String a4EditorText(double value)
    {
        return juce::String(value, 2) + " Hz";
    }

    juce::String durationEditorText(double value)
    {
        return juce::String(value, 2) + "x";
    }

    juce::String strobeRowsEditorText(int value)
    {
        return juce::String(juce::jlimit(minStrobeRows, maxStrobeRows, value));
    }

    juce::String arrowText(int ratioMode)
    {
        const auto arrow = ratioMode <= 0 ? 0x2191 : (ratioMode == 1 ? 0x2192 : 0x2193);
        return juce::String::charToString(static_cast<juce::juce_wchar>(arrow));
    }

#if JUCE_WINDOWS
    HHOOK directEntryKeyboardHook = nullptr;
    std::function<bool(int, bool, bool, bool, bool)> directEntryKeyboardHandler;

    LRESULT CALLBACK directEntryKeyboardProc(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code == HC_ACTION && directEntryKeyboardHandler != nullptr)
        {
            const auto message = static_cast<unsigned int>(wParam);
            const auto isKeyDown = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
            const auto isKeyUp = message == WM_KEYUP || message == WM_SYSKEYUP;

            if (isKeyDown || isKeyUp)
            {
                const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
                const auto controlDown = (::GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                const auto shiftDown = (::GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                const auto altDown = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

                if (directEntryKeyboardHandler(static_cast<int>(info->vkCode), isKeyDown, controlDown, shiftDown, altDown))
                    return 1;
            }
        }

        return ::CallNextHookEx(directEntryKeyboardHook, code, wParam, lParam);
    }

    juce::String sanitiseDecimalText(juce::String text)
    {
        text = text.retainCharacters("0123456789.,").replaceCharacter(',', '.');

        if (text.containsChar('.'))
        {
            const auto firstDecimal = text.indexOfChar('.');
            text = text.substring(0, firstDecimal + 1) + text.substring(firstDecimal + 1).removeCharacters(".");
        }

        return text;
    }
#endif
}

StrobeDisplayComponent::StrobeDisplayComponent(GuitarForgeStrobeTunerAudioProcessor& p)
    : processor(p)
{
    setOpaque(true);
#if SLAMMIN_TUNER_USE_OPENGL
    openGLContext.setComponentPaintingEnabled(true);
    openGLContext.attachTo(*this);
#endif
    startTimerHz(60);
}

StrobeDisplayComponent::~StrobeDisplayComponent()
{
    stopTimer();
#if SLAMMIN_TUNER_USE_OPENGL
    openGLContext.detach();
#endif
}

void StrobeDisplayComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    drawSurface(g, bounds, snapshot.pitch.voiced);

    bounds = bounds.reduced(12.0f);
    drawStrobeRows(g, bounds);
}

void StrobeDisplayComponent::resized()
{
    repaint();
}

void StrobeDisplayComponent::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto dt = lastFrameMs > 0.0 ? juce::jlimit(0.001, 0.050, (now - lastFrameMs) / 1000.0) : (1.0 / 60.0);
    lastFrameMs = now;

    snapshot = processor.getTunerSnapshot();
    const auto rowCount = static_cast<size_t>(juce::jlimit(minStrobeRows, maxStrobeRows, processor.getStrobeRowCount()));
    if (rowPhases.size() != rowCount)
        rowPhases.assign(rowCount, 0.0);

    const auto sensitivity = processor.getStrobeSensitivity();
    const auto reducedMotion = processor.isReducedMotionEnabled();
    const auto classicMode = processor.isClassicStrobeModeEnabled();

    if (classicMode)
    {
        const auto speedCents = snapshot.pitch.voiced ? snapshot.pitch.cents
            : (snapshot.rows.isEmpty() ? 0.0 : snapshot.rows.getFirst().speedCents);
        const auto clampedCents = juce::jlimit(-35.0, 35.0, speedCents);
        const auto cyclesPerSecond = reducedMotion ? 0.0 : clampedCents * 0.016 * sensitivity;
        auto phase = std::fmod(rowPhases.front() + (cyclesPerSecond * dt), 1.0);
        if (phase < 0.0)
            phase += 1.0;

        std::fill(rowPhases.begin(), rowPhases.end(), phase);
        repaint();
        return;
    }

    for (size_t index = 0; index < rowCount; ++index)
    {
        const auto hasRow = static_cast<int>(index) < snapshot.rows.size();
        const auto speedCents = hasRow ? snapshot.rows[static_cast<int>(index)].speedCents : 0.0;
        const auto clampedCents = juce::jlimit(-35.0, 35.0, speedCents);
        auto cyclesPerSecond = reducedMotion ? 0.0 : clampedCents * 0.016 * sensitivity;
        if (processor.isAlternateStrobeDirectionEnabled() && (index % 2) != 0)
            cyclesPerSecond = -cyclesPerSecond;

        rowPhases[index] = std::fmod(rowPhases[index] + (cyclesPerSecond * dt), 1.0);
        if (rowPhases[index] < 0.0)
            rowPhases[index] += 1.0;
    }

    repaint();
}

void StrobeDisplayComponent::drawStrobeRows(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto rowCount = juce::jlimit(minStrobeRows, maxStrobeRows, processor.getStrobeRowCount());
    const auto solidStyle = processor.isSolidStrobeStyleEnabled();
    const auto rowGap = solidStyle ? 0.0f : 6.0f;
    const auto rowHeight = (bounds.getHeight() - (rowGap * static_cast<float>(rowCount - 1))) / static_cast<float>(rowCount);
    const auto colour = accentColourForIndex(currentChoice(processor.parameters, "strobeColour"));
    const auto maxBlockWidth = juce::jlimit(140.0f, 300.0f, bounds.getWidth() * 0.26f);
    const auto ratioMode = processor.getStrobeRatioMode();
    const auto classicMode = processor.isClassicStrobeModeEnabled();
    const auto compactWidthScale = 0.68f;
    const auto unitWidth = juce::jmax(5.0f, maxBlockWidth / std::pow(2.0f, static_cast<float>(rowCount - 1)));
    const auto topWidePeriod = juce::jmax(5.0f, maxBlockWidth * 1.12f) * 2.0f;
    const auto topDyadicPeriod = unitWidth * std::pow(2.0f, static_cast<float>(rowCount));
    const auto classicReferencePeriod = ratioMode == 0 ? topWidePeriod
                                                       : topDyadicPeriod * (ratioMode == 2 ? compactWidthScale : 1.0f);

    g.setColour(juce::Colour::fromRGB(5, 7, 8));
    g.fillRoundedRectangle(bounds, panelRadius);

    g.saveState();
    juce::Path clipPath;
    clipPath.addRoundedRectangle(bounds, panelRadius);
    g.reduceClipRegion(clipPath, juce::AffineTransform());

    for (int rowIndex = 0; rowIndex < rowCount; ++rowIndex)
    {
        const auto rowBounds = juce::Rectangle<float>(bounds.getX(),
                                                      bounds.getY() + (static_cast<float>(rowIndex) * (rowHeight + rowGap)),
                                                      bounds.getWidth(),
                                                      rowHeight);
        const auto hasRow = rowIndex < snapshot.rows.size();
        const auto opacity = hasRow ? snapshot.rows[rowIndex].opacity : 0.18;
        const auto phase = rowIndex < static_cast<int>(rowPhases.size()) ? rowPhases[static_cast<size_t>(rowIndex)] : 0.0;
        auto barWidth = 5.0f;
        auto period = 10.0f;

        if (ratioMode == 0)
        {
            const auto wideScale = std::pow(2.0f, static_cast<float>(-rowIndex));
            barWidth = juce::jmax(5.0f, maxBlockWidth * 1.12f * wideScale);
            period = barWidth * 2.0f;
        }
        else
        {
            const auto levelFromBottom = (rowCount - 1) - rowIndex;
            const auto dyadicScale = std::pow(2.0f, static_cast<float>(levelFromBottom));
            const auto widthScale = ratioMode == 2 ? compactWidthScale : 1.0f;
            barWidth = juce::jmax(5.0f, unitWidth * dyadicScale * widthScale);
            period = barWidth * 2.0f;
        }

        const auto offset = static_cast<float>(phase) * (classicMode ? classicReferencePeriod : period);

        g.setColour(panelDark().darker(0.35f));
        if (solidStyle)
            g.fillRect(rowBounds);
        else
            g.fillRoundedRectangle(rowBounds, 4.0f);

        const auto alpha = juce::jlimit(0.10f, 0.95f, static_cast<float>(opacity));
        auto bandColour = colour.withAlpha(snapshot.pitch.voiced ? alpha : alpha * 0.46f);
        if (snapshot.pitch.attackSettling)
            bandColour = bandColour.interpolatedWith(textMuted(), 0.28f);

        auto startX = rowBounds.getX() + offset;
        while (startX > rowBounds.getX() - period)
            startX -= period;

        for (auto x = startX; x < rowBounds.getRight() + period; x += period)
        {
            auto bar = solidStyle
                ? juce::Rectangle<float>(x, rowBounds.getY(), barWidth, rowBounds.getHeight())
                : juce::Rectangle<float>(x, rowBounds.getY() + 2.0f, barWidth, rowBounds.getHeight() - 4.0f);

            if (solidStyle)
            {
                g.setColour(bandColour);
                g.fillRect(bar);
            }
            else
            {
                g.setColour(bandColour.withMultipliedAlpha(0.55f));
                g.fillRoundedRectangle(bar, 3.0f);
                g.setColour(bandColour.withAlpha(alpha));
                g.fillRoundedRectangle(bar.reduced(0.0f, rowBounds.getHeight() * 0.24f), 2.0f);
            }
        }

        if (! solidStyle)
        {
            g.setColour(juce::Colours::white.withAlpha(0.05f));
            g.drawHorizontalLine(juce::roundToInt(rowBounds.getCentreY()), rowBounds.getX(), rowBounds.getRight());
        }
    }

    g.restoreState();

    const auto centreX = bounds.getCentreX();
    g.setColour(lockGreen().withAlpha(snapshot.pitch.voiced ? 0.62f : 0.22f));
    g.drawLine(centreX, bounds.getY(), centreX, bounds.getBottom(), 1.4f);
    g.setColour(textMuted().withAlpha(0.26f));
    g.drawRoundedRectangle(bounds, panelRadius, 1.0f);
}

void StrobeDisplayComponent::drawNeedle(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour(panelDark().darker(0.40f));
    g.fillRoundedRectangle(bounds, panelRadius);

    const auto centre = bounds.getCentre();
    const auto radius = juce::jmin(bounds.getWidth() * 0.38f, bounds.getHeight() * 0.82f);
    const auto cents = juce::jlimit(-50.0, 50.0, snapshot.pitch.cents);
    const auto angle = juce::jmap(static_cast<float>(cents), -50.0f, 50.0f,
                                  -juce::MathConstants<float>::pi * 0.42f,
                                  juce::MathConstants<float>::pi * 0.42f);
    const auto needleEnd = juce::Point<float>(centre.x + std::sin(angle) * radius,
                                              centre.y - std::cos(angle) * radius);

    for (int mark = -50; mark <= 50; mark += 10)
    {
        const auto markAngle = juce::jmap(static_cast<float>(mark), -50.0f, 50.0f,
                                          -juce::MathConstants<float>::pi * 0.42f,
                                          juce::MathConstants<float>::pi * 0.42f);
        const auto outer = juce::Point<float>(centre.x + std::sin(markAngle) * radius,
                                              centre.y - std::cos(markAngle) * radius);
        const auto innerRadius = radius - (mark == 0 ? 24.0f : 14.0f);
        const auto inner = juce::Point<float>(centre.x + std::sin(markAngle) * innerRadius,
                                              centre.y - std::cos(markAngle) * innerRadius);
        g.setColour(mark == 0 ? lockGreen().withAlpha(0.75f) : textMuted().withAlpha(0.35f));
        g.drawLine(inner.x, inner.y, outer.x, outer.y, mark == 0 ? 2.2f : 1.0f);
    }

    g.setColour(colourForCents(snapshot.pitch.cents));
    g.drawLine(centre.x, centre.y, needleEnd.x, needleEnd.y, 4.0f);
    g.fillEllipse(juce::Rectangle<float>(14.0f, 14.0f).withCentre(centre));
}

GuitarForgeStrobeTunerAudioProcessorEditor::GuitarForgeStrobeTunerAudioProcessorEditor(GuitarForgeStrobeTunerAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      strobeDisplay(p)
{
    setWantsKeyboardFocus(true);
    logoImage = juce::ImageCache::getFromMemory(BinaryData::Slammin_Logo_Main_png,
                                                BinaryData::Slammin_Logo_Main_pngSize);
    configureControls();

    setResizeLimits(900, 620, 1680, 1040);
    setResizable(true, true);
    const auto savedSize = processor.getSavedEditorSize();
    setSize(savedSize.x, savedSize.y);
    startTimerHz(30);
}

GuitarForgeStrobeTunerAudioProcessorEditor::~GuitarForgeStrobeTunerAudioProcessorEditor()
{
    shuttingDownEditor = true;
    endDirectTextEntry(false);
    persistStableEditorSize();
    processor.flushUserPreferences();
}

void GuitarForgeStrobeTunerAudioProcessorEditor::configureControls()
{
    addAndMakeVisible(strobeDisplay);

    logoButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    logoButton.setTooltip("Slammin Captures");
    logoButton.onClick = [this] { launchBrandLinks(); };
    if (logoImage.isValid())
    {
        logoButton.setImages(false, true, true,
                             logoImage, 0.92f, juce::Colours::transparentBlack,
                             logoImage, 1.0f, juce::Colours::transparentBlack,
                             logoImage, 0.78f, juce::Colours::transparentBlack);
    }
    addAndMakeVisible(logoButton);

    strobeModeBox.addItemList(GuitarForgeStrobeTunerAudioProcessor::getStrobeModeChoiceNames(), 1);
    strobeModeBox.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    styleCombo(strobeModeBox);
    addAndMakeVisible(strobeModeBox);

    strobeRatioButton.setClickingTogglesState(false);
    strobeRatioButton.setButtonText(arrowText(1));
    strobeRatioButton.setTooltip("Switch strobe block size ratios");
    strobeRatioButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    strobeRatioButton.onClick = [this]
    {
        if (auto* parameter = processor.parameters.getParameter("strobeRatioMode"))
        {
            const auto nextMode = (processor.getStrobeRatioMode() + 1) % 3;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(nextMode)));
            parameter->endChangeGesture();
            strobeRatioButton.setButtonText(arrowText(nextMode));
            strobeDisplay.repaint();
        }
    };
    styleButton(strobeRatioButton);
    addAndMakeVisible(strobeRatioButton);

    barContrastButton.setClickingTogglesState(true);
    barContrastButton.setButtonText("Spectrum Bar");
    barContrastButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    styleButton(barContrastButton);
    addAndMakeVisible(barContrastButton);

    profileBox.addItemList(guitarforge::tuner::TuningProfileLibrary::getProfileChoiceNames(), 1);
    trackingBox.addItemList(juce::StringArray { "Balanced", "Fast", "Precision" }, 1);
    inputBox.addItemList(juce::StringArray { "Auto", "Left", "Right", "Mid" }, 1);
    colourBox.addItemList(GuitarForgeStrobeTunerAudioProcessor::getColourChoiceNames(), 1);

    for (auto* box : { &profileBox, &trackingBox, &inputBox, &colourBox })
    {
        styleCombo(*box);
        addAndMakeVisible(*box);
    }

    styleSlider(a4Slider);
    styleSlider(sensitivitySlider);
    addAndMakeVisible(a4Slider);
    addAndMakeVisible(sensitivitySlider);

    configureValueEditor(a4ValueEditor);
    configureValueEditor(sensitivityValueEditor);
    configureValueEditor(strobeRowsValueEditor);
    a4ValueEditor.onReturnKey = [this] { if (activeValueField == ActiveValueField::a4) endDirectTextEntry(true); else commitA4Editor(); };
    a4ValueEditor.onFocusLost = [this] { if (activeValueField != ActiveValueField::a4) commitA4Editor(); };
    a4ValueEditor.onEscapeKey = [this] { cancelDirectTextEntry(); };
    sensitivityValueEditor.onReturnKey = [this] { if (activeValueField == ActiveValueField::sensitivity) endDirectTextEntry(true); else commitSensitivityEditor(); };
    sensitivityValueEditor.onFocusLost = [this] { if (activeValueField != ActiveValueField::sensitivity) commitSensitivityEditor(); };
    sensitivityValueEditor.onEscapeKey = [this] { cancelDirectTextEntry(); };
    strobeRowsValueEditor.onReturnKey = [this] { if (activeValueField == ActiveValueField::strobeRows) endDirectTextEntry(true); else commitStrobeRowsEditor(); };
    strobeRowsValueEditor.onFocusLost = [this] { if (activeValueField != ActiveValueField::strobeRows) commitStrobeRowsEditor(); };
    strobeRowsValueEditor.onEscapeKey = [this] { cancelDirectTextEntry(); };
    addAndMakeVisible(a4ValueEditor);
    addAndMakeVisible(sensitivityValueEditor);
    addAndMakeVisible(strobeRowsValueEditor);
    a4ValueEditor.addMouseListener(this, true);
    sensitivityValueEditor.addMouseListener(this, true);
    strobeRowsValueEditor.addMouseListener(this, true);

    std::initializer_list<juce::Component*> commitOnClickComponents
    {
        &strobeDisplay, &logoButton, &strobeModeBox, &strobeRatioButton, &barContrastButton,
        &profileBox, &trackingBox, &inputBox, &colourBox,
        &a4Slider, &sensitivitySlider, &bandResolvedButton, &muteButton,
        &referenceToneButton, &reducedMotionButton, &versionLabel, &tuningSummaryLabel
    };

    for (auto* component : commitOnClickComponents)
        component->addMouseListener(this, true);

    bandResolvedButton.setButtonText("True");
    bandResolvedButton.setTooltip("Use independently measured harmonic bands for the strobe rows");
    muteButton.setButtonText("Mute");
    referenceToneButton.setButtonText("Tone");
    reducedMotionButton.setButtonText("Calm");
    for (auto* button : { &bandResolvedButton, &muteButton, &referenceToneButton, &reducedMotionButton })
    {
        styleButton(*button);
        addAndMakeVisible(*button);
    }

    versionLabel.setText("V1.2.5", juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::centredLeft);
    versionLabel.setBorderSize(juce::BorderSize<int>());
    versionLabel.setColour(juce::Label::textColourId, textMain());
    versionLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    versionLabel.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    versionLabel.setFont(uiFont(16.0f, true));
    addAndMakeVisible(versionLabel);

    tuningSummaryLabel.setJustificationType(juce::Justification::centredRight);
    tuningSummaryLabel.setMinimumHorizontalScale(0.50f);
    tuningSummaryLabel.setBorderSize(juce::BorderSize<int>());
    tuningSummaryLabel.setColour(juce::Label::textColourId, textMuted());
    tuningSummaryLabel.setFont(uiFont(15.0f, true));
    addAndMakeVisible(tuningSummaryLabel);

    profileAttachment = std::make_unique<ComboBoxAttachment>(processor.parameters, "profileIndex", profileBox);
    trackingAttachment = std::make_unique<ComboBoxAttachment>(processor.parameters, "trackingMode", trackingBox);
    inputAttachment = std::make_unique<ComboBoxAttachment>(processor.parameters, "inputMode", inputBox);
    colourAttachment = std::make_unique<ComboBoxAttachment>(processor.parameters, "strobeColour", colourBox);
    strobeModeAttachment = std::make_unique<ComboBoxAttachment>(processor.parameters, "strobeMode", strobeModeBox);
    a4Attachment = std::make_unique<SliderAttachment>(processor.parameters, "a4Hz", a4Slider);
    sensitivityAttachment = std::make_unique<SliderAttachment>(processor.parameters, "strobeSensitivity", sensitivitySlider);
    bandResolvedAttachment = std::make_unique<ButtonAttachment>(processor.parameters, "bandResolvedStrobe", bandResolvedButton);
    muteAttachment = std::make_unique<ButtonAttachment>(processor.parameters, "muteOutput", muteButton);
    referenceToneAttachment = std::make_unique<ButtonAttachment>(processor.parameters, "referenceToneEnabled", referenceToneButton);
    reducedMotionAttachment = std::make_unique<ButtonAttachment>(processor.parameters, "reducedMotion", reducedMotionButton);
    barContrastAttachment = std::make_unique<ButtonAttachment>(processor.parameters, "barContrast", barContrastButton);
    preferSharps = processor.isSharpNotationEnabled();
    updateValueEditors();
    applyAccentColours();
}

void GuitarForgeStrobeTunerAudioProcessorEditor::configureValueEditor(juce::TextEditor& editor)
{
    editor.setMultiLine(false);
    editor.setReturnKeyStartsNewLine(false);
    editor.setScrollbarsShown(false);
    editor.setJustification(juce::Justification::centred);
    editor.setSelectAllWhenFocused(true);
    editor.setInputRestrictions(12, "0123456789.,+- HzxX");
    editor.setWantsKeyboardFocus(true);
    editor.setMouseCursor(juce::MouseCursor::IBeamCursor);
    editor.setFont(uiFont(14.0f));
    editor.setColour(juce::TextEditor::backgroundColourId, panelDark());
    editor.setColour(juce::TextEditor::textColourId, textMain());
    editor.setColour(juce::TextEditor::outlineColourId, border());
    editor.setColour(juce::TextEditor::focusedOutlineColourId, lockGreen());
    editor.setColour(juce::TextEditor::highlightColourId, lockGreen().withAlpha(0.30f));
    editor.setColour(juce::TextEditor::highlightedTextColourId, textMain());
}

void GuitarForgeStrobeTunerAudioProcessorEditor::updateValueEditors()
{
    if (activeValueField != ActiveValueField::a4 && ! a4ValueEditor.hasKeyboardFocus(true))
        a4ValueEditor.setText(a4EditorText(a4Slider.getValue()), juce::dontSendNotification);

    if (activeValueField != ActiveValueField::sensitivity && ! sensitivityValueEditor.hasKeyboardFocus(true))
        sensitivityValueEditor.setText(durationEditorText(sensitivitySlider.getValue()), juce::dontSendNotification);

    if (activeValueField != ActiveValueField::strobeRows && ! strobeRowsValueEditor.hasKeyboardFocus(true))
        strobeRowsValueEditor.setText(strobeRowsEditorText(processor.getStrobeRowCount()), juce::dontSendNotification);
}

void GuitarForgeStrobeTunerAudioProcessorEditor::commitA4Editor()
{
    const auto value = juce::jlimit(a4MinHz, a4MaxHz, numericValueFromText(a4ValueEditor.getText(), a4Slider.getValue()));
    a4Slider.setValue(value, juce::sendNotificationSync);
    a4ValueEditor.setText(a4EditorText(value), juce::dontSendNotification);
}

void GuitarForgeStrobeTunerAudioProcessorEditor::commitSensitivityEditor()
{
    const auto value = juce::jlimit(durationMinX, durationMaxX, numericValueFromText(sensitivityValueEditor.getText(), sensitivitySlider.getValue()));
    sensitivitySlider.setValue(value, juce::sendNotificationSync);
    sensitivityValueEditor.setText(durationEditorText(value), juce::dontSendNotification);
}

void GuitarForgeStrobeTunerAudioProcessorEditor::commitStrobeRowsEditor()
{
    const auto fallback = static_cast<double>(processor.getStrobeRowCount());
    const auto value = juce::jlimit(minStrobeRows,
                                    maxStrobeRows,
                                    juce::roundToInt(numericValueFromText(strobeRowsValueEditor.getText(), fallback)));

    if (auto* parameter = processor.parameters.getParameter("strobeRows"))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(value)));

    strobeRowsValueEditor.setText(strobeRowsEditorText(value), juce::dontSendNotification);
}

void GuitarForgeStrobeTunerAudioProcessorEditor::beginDirectTextEntry(ActiveValueField field)
{
    if (field == ActiveValueField::none)
        return;

    if (activeValueField != ActiveValueField::none && activeValueField != field)
        endDirectTextEntry(true);

    activeValueField = field;
    auto* editor = valueEditorFor(field);
    if (editor == nullptr)
        return;

    juce::String numericText;
    if (field == ActiveValueField::a4)
        numericText = juce::String(a4Slider.getValue(), 2);
    else if (field == ActiveValueField::sensitivity)
        numericText = juce::String(sensitivitySlider.getValue(), 2);
    else
        numericText = juce::String(processor.getStrobeRowCount());

    editor->setText(numericText, juce::dontSendNotification);
    editor->grabKeyboardFocus();
    editor->selectAll();

    if (auto* peer = getPeer())
        peer->grabFocus();

#if JUCE_WINDOWS
    if (auto* hwnd = static_cast<HWND>(getWindowHandle()))
    {
        ::SetActiveWindow(hwnd);
        ::SetFocus(hwnd);
    }

    if (! showNativeValueEditor(field))
        installKeyboardCapture();
#endif
}

void GuitarForgeStrobeTunerAudioProcessorEditor::endDirectTextEntry(bool shouldCommit)
{
    const auto field = activeValueField;
    if (field == ActiveValueField::none)
        return;

#if JUCE_WINDOWS
    syncNativeValueEditorText();
    destroyNativeValueEditor();
    removeKeyboardCapture();
#endif

    if (shouldCommit)
    {
        if (field == ActiveValueField::a4)
            commitA4Editor();
        else if (field == ActiveValueField::sensitivity)
            commitSensitivityEditor();
        else if (field == ActiveValueField::strobeRows)
            commitStrobeRowsEditor();
    }

    activeValueField = ActiveValueField::none;
    updateValueEditors();
    repaint();
}

void GuitarForgeStrobeTunerAudioProcessorEditor::cancelDirectTextEntry()
{
    if (activeValueField == ActiveValueField::none)
    {
        updateValueEditors();
        return;
    }

#if JUCE_WINDOWS
    destroyNativeValueEditor();
    removeKeyboardCapture();
#endif

    activeValueField = ActiveValueField::none;
    updateValueEditors();
    repaint();
}

bool GuitarForgeStrobeTunerAudioProcessorEditor::handleDirectEntryKeyPress(const juce::KeyPress& key)
{
    if (activeValueField == ActiveValueField::none)
        return false;

    const auto code = key.getKeyCode();
    const auto character = key.getTextCharacter();

    if (key.getModifiers().isCtrlDown() || key.getModifiers().isCommandDown())
    {
        if (code == 'a' || code == 'A')
        {
            if (auto* editor = valueEditorFor(activeValueField))
                editor->selectAll();
        }
        else if (code == 'v' || code == 'V')
        {
            pasteDirectEntryText();
        }

        return true;
    }

    if (code == juce::KeyPress::returnKey || code == juce::KeyPress::tabKey)
    {
        endDirectTextEntry(true);
        return true;
    }

    if (code == juce::KeyPress::escapeKey)
    {
        cancelDirectTextEntry();
        return true;
    }

    if (code == juce::KeyPress::backspaceKey)
    {
        backspaceDirectEntryText();
        return true;
    }

    if (code == juce::KeyPress::deleteKey)
    {
        deleteDirectEntryText();
        return true;
    }

    if (code == juce::KeyPress::leftKey)
    {
        if (auto* editor = valueEditorFor(activeValueField))
            editor->moveCaretLeft(false, key.getModifiers().isShiftDown());
        return true;
    }

    if (code == juce::KeyPress::rightKey)
    {
        if (auto* editor = valueEditorFor(activeValueField))
            editor->moveCaretRight(false, key.getModifiers().isShiftDown());
        return true;
    }

    if (code == juce::KeyPress::homeKey)
    {
        if (auto* editor = valueEditorFor(activeValueField))
            editor->setCaretPosition(0);
        return true;
    }

    if (code == juce::KeyPress::endKey)
    {
        if (auto* editor = valueEditorFor(activeValueField))
            editor->moveCaretToEnd();
        return true;
    }

    if ((character >= '0' && character <= '9') || character == '.' || character == ',')
    {
        insertDirectEntryText(character == ',' ? "." : juce::String::charToString(character));
        return true;
    }

    return true;
}

void GuitarForgeStrobeTunerAudioProcessorEditor::insertDirectEntryText(const juce::String& text)
{
    auto* editor = valueEditorFor(activeValueField);
    if (editor == nullptr || text.isEmpty())
        return;

    if (text == ".")
    {
        if (activeValueField == ActiveValueField::strobeRows)
            return;

        const auto selection = editor->getHighlightedRegion();
        const auto current = editor->getText();
        const auto unselectedText = current.substring(0, selection.getStart()) + current.substring(selection.getEnd());
        if (unselectedText.containsChar('.') || unselectedText.containsChar(','))
            return;
    }

    editor->insertTextAtCaret(text);
    editor->grabKeyboardFocus();
}

void GuitarForgeStrobeTunerAudioProcessorEditor::pasteDirectEntryText()
{
    auto pasted = juce::SystemClipboard::getTextFromClipboard()
        .retainCharacters("0123456789.,")
        .replaceCharacter(',', '.');

    if (activeValueField == ActiveValueField::strobeRows)
        pasted = pasted.retainCharacters("0123456789");

    if (pasted.containsChar('.'))
    {
        const auto firstDecimal = pasted.indexOfChar('.');
        pasted = pasted.substring(0, firstDecimal + 1) + pasted.substring(firstDecimal + 1).removeCharacters(".");
    }

    insertDirectEntryText(pasted);
}

void GuitarForgeStrobeTunerAudioProcessorEditor::backspaceDirectEntryText()
{
    if (auto* editor = valueEditorFor(activeValueField))
    {
        if (editor->getHighlightedRegion().getLength() > 0)
            editor->insertTextAtCaret({});
        else
            editor->deleteBackwards(false);

        editor->grabKeyboardFocus();
    }
}

void GuitarForgeStrobeTunerAudioProcessorEditor::deleteDirectEntryText()
{
    if (auto* editor = valueEditorFor(activeValueField))
    {
        if (editor->getHighlightedRegion().getLength() > 0)
            editor->insertTextAtCaret({});
        else
            editor->deleteForwards(false);

        editor->grabKeyboardFocus();
    }
}

juce::TextEditor* GuitarForgeStrobeTunerAudioProcessorEditor::valueEditorFor(ActiveValueField field)
{
    if (field == ActiveValueField::a4)
        return &a4ValueEditor;

    if (field == ActiveValueField::sensitivity)
        return &sensitivityValueEditor;

    if (field == ActiveValueField::strobeRows)
        return &strobeRowsValueEditor;

    return nullptr;
}

#if JUCE_WINDOWS
bool GuitarForgeStrobeTunerAudioProcessorEditor::showNativeValueEditor(ActiveValueField field)
{
    auto* editor = valueEditorFor(field);
    auto* parentHwnd = static_cast<HWND>(getWindowHandle());
    if (editor == nullptr || parentHwnd == nullptr)
        return false;

    destroyNativeValueEditor();

    juce::String text;
    if (field == ActiveValueField::a4)
        text = juce::String(a4Slider.getValue(), 2);
    else if (field == ActiveValueField::sensitivity)
        text = juce::String(sensitivitySlider.getValue(), 2);
    else
        text = juce::String(processor.getStrobeRowCount());

    const auto bounds = editor->getBounds();
    const auto editX = bounds.getRight() - 3;
    const auto editY = bounds.getCentreY();

    nativeValueEditHandle = ::CreateWindowExW(0,
                                              L"EDIT",
                                              text.toWideCharPointer(),
                                              WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_CENTER,
                                              editX,
                                              editY,
                                              2,
                                              2,
                                              parentHwnd,
                                              nullptr,
                                              static_cast<HINSTANCE>(juce::Process::getCurrentModuleInstanceHandle()),
                                              nullptr);

    if (nativeValueEditHandle == nullptr)
        return false;

    ::SetWindowLongPtrW(nativeValueEditHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    nativeValueEditOriginalProc = reinterpret_cast<WNDPROC>(
        ::SetWindowLongPtrW(nativeValueEditHandle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&GuitarForgeStrobeTunerAudioProcessorEditor::nativeValueEditProc)));

    ::SendMessageW(nativeValueEditHandle, WM_SETFONT, reinterpret_cast<WPARAM>(::GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    ::SendMessageW(nativeValueEditHandle, EM_SETLIMITTEXT, 10, 0);
    ::SendMessageW(nativeValueEditHandle, EM_SETSEL, 0, -1);
    ::SetFocus(nativeValueEditHandle);
    return true;
}

void GuitarForgeStrobeTunerAudioProcessorEditor::destroyNativeValueEditor()
{
    if (nativeValueEditHandle == nullptr)
        return;

    auto* hwnd = nativeValueEditHandle;
    if (nativeValueEditOriginalProc != nullptr)
        ::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(nativeValueEditOriginalProc));

    nativeValueEditHandle = nullptr;
    nativeValueEditOriginalProc = nullptr;
    ::DestroyWindow(hwnd);
}

void GuitarForgeStrobeTunerAudioProcessorEditor::syncNativeValueEditorText()
{
    if (nativeValueEditHandle == nullptr || activeValueField == ActiveValueField::none)
        return;

    const auto length = ::GetWindowTextLengthW(nativeValueEditHandle);
    std::wstring text(static_cast<size_t>(juce::jmax(0, length)) + 1, L'\0');
    if (length > 0)
        ::GetWindowTextW(nativeValueEditHandle, text.data(), length + 1);

    if (auto* editor = valueEditorFor(activeValueField))
    {
        editor->setText(juce::String(text.c_str()), juce::dontSendNotification);
        editor->moveCaretToEnd();
    }
}

LRESULT CALLBACK GuitarForgeStrobeTunerAudioProcessorEditor::nativeValueEditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (auto* editor = reinterpret_cast<GuitarForgeStrobeTunerAudioProcessorEditor*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA)))
        return editor->handleNativeValueEditMessage(hwnd, message, wParam, lParam);

    return ::DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT GuitarForgeStrobeTunerAudioProcessorEditor::handleNativeValueEditMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    const auto callOriginal = [this, hwnd, message, wParam, lParam]() -> LRESULT
    {
        return nativeValueEditOriginalProc != nullptr
            ? ::CallWindowProcW(nativeValueEditOriginalProc, hwnd, message, wParam, lParam)
            : ::DefWindowProcW(hwnd, message, wParam, lParam);
    };

    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
    {
        const auto controlDown = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;

        if (wParam == VK_RETURN || wParam == VK_TAB)
        {
            endDirectTextEntry(true);
            return 0;
        }

        if (wParam == VK_ESCAPE)
        {
            cancelDirectTextEntry();
            return 0;
        }

        if (controlDown && (wParam == 'A' || wParam == 'a'))
        {
            ::SendMessageW(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
    }

    if (message == WM_CHAR)
    {
        const auto ch = static_cast<wchar_t>(wParam);

        if (ch == L'\r' || ch == L'\t' || ch == 27)
            return 0;

        const auto isDecimalSeparator = ch == L'.' || ch == L',';
        const auto wantsInteger = activeValueField == ActiveValueField::strobeRows;

        if (ch != 8 && ch != 127 && ! (ch >= L'0' && ch <= L'9') && ! isDecimalSeparator)
            return 0;

        if (wantsInteger && isDecimalSeparator)
            return 0;

        if (isDecimalSeparator)
        {
            const auto currentLength = ::GetWindowTextLengthW(hwnd);
            std::wstring current(static_cast<size_t>(juce::jmax(0, currentLength)) + 1, L'\0');
            if (currentLength > 0)
                ::GetWindowTextW(hwnd, current.data(), currentLength + 1);

            DWORD selectionStart = 0;
            DWORD selectionEnd = 0;
            ::SendMessageW(hwnd, EM_GETSEL, reinterpret_cast<WPARAM>(&selectionStart), reinterpret_cast<LPARAM>(&selectionEnd));
            const auto beforeSelection = juce::String(current.substr(0, selectionStart).c_str());
            const auto afterSelection = juce::String(current.substr(selectionEnd).c_str());
            if ((beforeSelection + afterSelection).containsChar('.'))
                return 0;
        }

        const auto result = callOriginal();
        syncNativeValueEditorText();
        return result;
    }

    if (message == WM_PASTE)
    {
        if (::OpenClipboard(hwnd))
        {
            if (auto* data = ::GetClipboardData(CF_UNICODETEXT))
            {
                if (auto* chars = static_cast<const wchar_t*>(::GlobalLock(data)))
                {
                    const auto rawText = juce::String(chars);
                    const auto cleaned = activeValueField == ActiveValueField::strobeRows
                        ? rawText.retainCharacters("0123456789")
                        : sanitiseDecimalText(rawText);
                    ::SendMessageW(hwnd, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(cleaned.toWideCharPointer()));
                    ::GlobalUnlock(data);
                    syncNativeValueEditorText();
                }
            }

            ::CloseClipboard();
        }

        return 0;
    }

    if (message == WM_KEYUP || message == WM_SYSKEYUP)
    {
        const auto result = callOriginal();
        syncNativeValueEditorText();
        return result;
    }

    return callOriginal();
}

void GuitarForgeStrobeTunerAudioProcessorEditor::installKeyboardCapture()
{
    directEntryKeyboardHandler = [this](int virtualKey, bool isKeyDown, bool controlDown, bool shiftDown, bool altDown)
    {
        return handleDirectEntryVirtualKey(virtualKey, isKeyDown, controlDown, shiftDown, altDown);
    };

    if (directEntryKeyboardHook == nullptr)
    {
        directEntryKeyboardHook = ::SetWindowsHookExW(WH_KEYBOARD_LL,
                                                      directEntryKeyboardProc,
                                                      static_cast<HINSTANCE>(juce::Process::getCurrentModuleInstanceHandle()),
                                                      0);
    }
}

void GuitarForgeStrobeTunerAudioProcessorEditor::removeKeyboardCapture()
{
    directEntryKeyboardHandler = nullptr;

    if (directEntryKeyboardHook != nullptr)
    {
        ::UnhookWindowsHookEx(directEntryKeyboardHook);
        directEntryKeyboardHook = nullptr;
    }
}

bool GuitarForgeStrobeTunerAudioProcessorEditor::handleDirectEntryVirtualKey(int virtualKey, bool isKeyDown, bool controlDown, bool shiftDown, bool altDown)
{
    if (activeValueField == ActiveValueField::none)
        return false;

    if (altDown && virtualKey == VK_F4)
        return false;

    if (! isKeyDown)
        return true;

    if (controlDown)
    {
        if (virtualKey == 'A')
        {
            if (auto* editor = valueEditorFor(activeValueField))
                editor->selectAll();
        }
        else if (virtualKey == 'V')
        {
            pasteDirectEntryText();
        }

        return true;
    }

    if (virtualKey >= '0' && virtualKey <= '9')
    {
        insertDirectEntryText(juce::String::charToString(static_cast<juce::juce_wchar>('0' + (virtualKey - '0'))));
        return true;
    }

    if (virtualKey >= VK_NUMPAD0 && virtualKey <= VK_NUMPAD9)
    {
        insertDirectEntryText(juce::String::charToString(static_cast<juce::juce_wchar>('0' + (virtualKey - VK_NUMPAD0))));
        return true;
    }

    switch (virtualKey)
    {
        case VK_RETURN:
        case VK_TAB:
            endDirectTextEntry(true);
            return true;

        case VK_ESCAPE:
            cancelDirectTextEntry();
            return true;

        case VK_BACK:
            backspaceDirectEntryText();
            return true;

        case VK_DELETE:
            deleteDirectEntryText();
            return true;

        case VK_LEFT:
            if (auto* editor = valueEditorFor(activeValueField))
                editor->moveCaretLeft(false, shiftDown);
            return true;

        case VK_RIGHT:
            if (auto* editor = valueEditorFor(activeValueField))
                editor->moveCaretRight(false, shiftDown);
            return true;

        case VK_HOME:
            if (auto* editor = valueEditorFor(activeValueField))
                editor->setCaretPosition(0);
            return true;

        case VK_END:
            if (auto* editor = valueEditorFor(activeValueField))
                editor->moveCaretToEnd();
            return true;

        case VK_DECIMAL:
        case VK_OEM_PERIOD:
        case VK_OEM_COMMA:
            if (activeValueField == ActiveValueField::strobeRows)
                return true;

            insertDirectEntryText(".");
            return true;

        default:
            return true;
    }
}
#endif

void GuitarForgeStrobeTunerAudioProcessorEditor::applyAccentColours()
{
    const auto accent = accentColourForIndex(currentChoice(processor.parameters, "strobeColour"));
    const auto accentSoft = accent.withAlpha(0.32f);

    for (auto* box : { &profileBox, &trackingBox, &inputBox, &colourBox, &strobeModeBox })
    {
        box->setColour(juce::ComboBox::arrowColourId, accent);
        box->setColour(juce::ComboBox::focusedOutlineColourId, accent);
    }

    trackingBox.setColour(juce::ComboBox::outlineColourId, border());
    strobeModeBox.setColour(juce::ComboBox::backgroundColourId, panelDark().interpolatedWith(accent, 0.16f));
    strobeModeBox.setColour(juce::ComboBox::outlineColourId, accent.withAlpha(0.68f));

    for (auto* slider : { &a4Slider, &sensitivitySlider })
    {
        slider->setColour(juce::Slider::trackColourId, accent);
        slider->setColour(juce::Slider::thumbColourId, accent.brighter(0.34f));
    }

    for (auto* editor : { &a4ValueEditor, &sensitivityValueEditor, &strobeRowsValueEditor })
        editor->setColour(juce::TextEditor::focusedOutlineColourId, accent);

    for (auto* button : { &bandResolvedButton, &muteButton, &referenceToneButton, &reducedMotionButton })
    {
        button->setColour(juce::ToggleButton::tickColourId, accent);
        button->setColour(juce::ToggleButton::tickDisabledColourId, accent.withAlpha(0.28f));
    }

    for (auto* button : { &barContrastButton, &strobeRatioButton })
    {
        button->setColour(juce::TextButton::buttonColourId, panelDark().interpolatedWith(accent, 0.16f));
        button->setColour(juce::TextButton::buttonOnColourId, accentSoft);
        button->setColour(juce::TextButton::textColourOffId, textMain());
        button->setColour(juce::TextButton::textColourOnId, textMain());
    }

    strobeRatioButton.setColour(juce::TextButton::textColourOffId, sharpAmber());
    strobeRatioButton.setColour(juce::TextButton::textColourOnId, sharpAmber());

    versionLabel.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    tuningSummaryLabel.setColour(juce::Label::textColourId, accent.interpolatedWith(textMuted(), 0.35f));
}

void GuitarForgeStrobeTunerAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient background(backgroundTop(), 0.0f, 0.0f,
                                    backgroundBottom(), 0.0f, static_cast<float>(getHeight()),
                                    false);
    background.addColour(0.46, juce::Colour::fromRGB(10, 13, 15));
    g.setGradientFill(background);
    g.fillAll();

    auto bounds = getLocalBounds().reduced(18);
    const auto header = bounds.removeFromTop(98);
    drawReadout(g, header);

    bounds.removeFromTop(12);
    bounds.removeFromTop(strobeDisplay.getHeight());
    bounds.removeFromTop(10);
    drawNeedleStrip(g, bounds.removeFromTop(92));

    g.setColour(textMuted());
    g.setFont(uiFont(12.0f, true));

    const auto labelOffset = 78;
    const auto labelWidth = 70;
    const auto profileLabel = profileBox.getBounds().withX(profileBox.getX() - labelOffset).withWidth(labelWidth);
    const auto a4Label = a4Slider.getBounds().withX(a4Slider.getX() - labelOffset).withWidth(labelWidth);
    const auto trackingLabel = trackingBox.getBounds().withX(trackingBox.getX() - labelOffset).withWidth(labelWidth);
    const auto inputLabel = inputBox.getBounds().withX(inputBox.getX() - labelOffset).withWidth(labelWidth);
    const auto colourLabel = colourBox.getBounds().withX(colourBox.getX() - labelOffset).withWidth(labelWidth);
    const auto sensLabel = sensitivitySlider.getBounds().withX(sensitivitySlider.getX() - labelOffset).withWidth(labelWidth);
    const auto strobeRowsLabel = strobeRowsValueEditor.getBounds()
        .withX(colourBox.getX() - labelOffset)
        .withWidth(96);

    for (const auto& pair : { std::pair<juce::Rectangle<int>, const char*> { profileLabel, "Tuning" },
                              { a4Label, "A4" },
                              { trackingLabel, "Track" },
                              { inputLabel, "Input" },
                              { sensLabel, "Duration" },
                              { colourLabel, "Color" },
                              { strobeRowsLabel, "Strobe Rows" } })
    {
        g.drawFittedText(pair.second, pair.first, juce::Justification::centredLeft, 1);
    }
}

void GuitarForgeStrobeTunerAudioProcessorEditor::resized()
{
    if (! shuttingDownEditor && getWidth() > 0 && getHeight() > 0)
    {
        pendingEditorSize = { getWidth(), getHeight() };
        pendingEditorSizeSinceMs = juce::Time::getMillisecondCounterHiRes();
        hasPendingEditorSize = true;
    }

    auto bounds = getLocalBounds().reduced(18);
    auto header = bounds.removeFromTop(98);
    logoButton.setBounds(header.reduced(18, 10).removeFromLeft(78));
    strobeModeBox.setBounds(header.reduced(18, 10).removeFromRight(300).removeFromBottom(28).removeFromRight(136));
    strobeRatioButton.setBounds(strobeModeBox.getX() - 76, strobeModeBox.getY() + 4, 18, 20);

    bounds.removeFromTop(12);
    const auto barHeight = 92;
    const auto controlsHeight = 150;
    const auto strobeHeight = juce::jmax(208, bounds.getHeight() - barHeight - controlsHeight - 32);
    strobeDisplay.setBounds(bounds.removeFromTop(strobeHeight));
    bounds.removeFromTop(10);
    auto barArea = bounds.removeFromTop(barHeight);
    auto barButtonArea = barArea.reduced(20, 12).removeFromRight(132);
    barContrastButton.setBounds(barButtonArea.withSizeKeepingCentre(118, 28));
    bounds.removeFromTop(12);

    auto controls = bounds.removeFromTop(controlsHeight);
    auto rowsArea = controls.removeFromTop(108);
    const auto controlHeight = 30;
    const auto rowGap = 9;
    const auto labelWidth = 78;
    const auto columnGap = 34;
    const auto columnWidth = (rowsArea.getWidth() - columnGap) / 2;
    auto left = rowsArea.removeFromLeft(columnWidth);
    rowsArea.removeFromLeft(columnGap);
    auto right = rowsArea;

    profileBox.setBounds(left.removeFromTop(controlHeight).withTrimmedLeft(labelWidth));
    left.removeFromTop(rowGap);
    auto a4Area = left.removeFromTop(controlHeight).withTrimmedLeft(labelWidth);
    a4ValueEditor.setBounds(a4Area.removeFromRight(92));
    a4Area.removeFromRight(8);
    a4Slider.setBounds(a4Area);
    left.removeFromTop(rowGap);
    trackingBox.setBounds(left.removeFromTop(controlHeight).withTrimmedLeft(labelWidth));

    inputBox.setBounds(right.removeFromTop(controlHeight).withTrimmedLeft(labelWidth));
    right.removeFromTop(rowGap);
    auto sensitivityArea = right.removeFromTop(controlHeight).withTrimmedLeft(labelWidth);
    sensitivityValueEditor.setBounds(sensitivityArea.removeFromRight(92));
    sensitivityArea.removeFromRight(8);
    sensitivitySlider.setBounds(sensitivityArea);
    right.removeFromTop(rowGap);
    colourBox.setBounds(right.removeFromTop(controlHeight).withTrimmedLeft(labelWidth));

    auto bottomRow = controls.removeFromBottom(34);
    auto versionArea = bottomRow.removeFromLeft(108);
    versionLabel.setBounds(versionArea);

    auto checkboxGroup = juce::Rectangle<int>(360, bottomRow.getHeight());
    checkboxGroup.setX(colourBox.getRight() - checkboxGroup.getWidth() + 30);
    checkboxGroup.setY(bottomRow.getY());
    auto summaryArea = bottomRow;
    summaryArea.setX(versionArea.getRight() + 12);
    summaryArea.setRight(trackingBox.getRight());
    tuningSummaryLabel.setBounds(summaryArea);

    const auto strobeRowsLabelX = colourBox.getX() - labelWidth;
    strobeRowsValueEditor.setBounds(strobeRowsLabelX + 112,
                                    bottomRow.getY() + 3,
                                    48,
                                    28);

    bandResolvedButton.setBounds(checkboxGroup.removeFromLeft(90));
    muteButton.setBounds(checkboxGroup.removeFromLeft(90));
    referenceToneButton.setBounds(checkboxGroup.removeFromLeft(90));
    reducedMotionButton.setBounds(checkboxGroup.removeFromLeft(90));
}

void GuitarForgeStrobeTunerAudioProcessorEditor::timerCallback()
{
#if JUCE_WINDOWS
    if (activeValueField != ActiveValueField::none)
    {
        DWORD foregroundProcessId = 0;
        if (auto* foregroundWindow = ::GetForegroundWindow())
            ::GetWindowThreadProcessId(foregroundWindow, &foregroundProcessId);

        if (foregroundProcessId != 0 && foregroundProcessId != ::GetCurrentProcessId())
            endDirectTextEntry(true);
    }
#endif

    snapshot = processor.getTunerSnapshot();
    preferSharps = processor.isSharpNotationEnabled();
    strobeRatioButton.setButtonText(arrowText(processor.getStrobeRatioMode()));
    barContrastButton.setButtonText(processor.isBarContrastEnabled() ? "Contrast Bar" : "Spectrum Bar");
    tuningSummaryLabel.setText(processor.getActiveTuningSummary(preferSharps), juce::dontSendNotification);
    updateValueEditors();
    applyAccentColours();
    persistStableEditorSize();
    repaint();
}

void GuitarForgeStrobeTunerAudioProcessorEditor::persistStableEditorSize()
{
    if (! hasPendingEditorSize)
        return;

    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto ageMs = now - pendingEditorSizeSinceMs;
    if (ageMs < editorSizeSettleMs)
        return;

    processor.setSavedEditorSize(pendingEditorSize.x, pendingEditorSize.y);
    hasPendingEditorSize = false;
    processor.flushUserPreferences();
}

void GuitarForgeStrobeTunerAudioProcessorEditor::drawReadout(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto active = snapshot.pitch.voiced;
    drawSurface(g, bounds.toFloat(), active);

    auto inner = bounds.reduced(18, 10);
    auto title = inner.removeFromLeft(320);
    title.removeFromLeft(112);
    auto right = inner.removeFromRight(300);
    inner.removeFromRight(16);
    auto noteArea = inner.removeFromLeft(142);
    noteClickBounds = noteArea;

    title.removeFromTop(11);
    g.setColour(textMuted());
    g.setFont(uiFont(12.0f, true));
    g.drawFittedText("SLAMMIN CAPTURES", title.removeFromTop(18), juce::Justification::centredLeft, 1);
    g.setColour(textMain());
    g.setFont(uiFont(32.0f, true));
    g.drawFittedText("SLAMMIN TUNER", title.removeFromTop(42), juce::Justification::centredLeft, 1);

    const auto noteText = active ? noteNameForMidi(snapshot.pitch.midi, preferSharps, false) : "--";
    const auto octaveText = active ? noteNameForMidi(snapshot.pitch.midi, preferSharps, true) : "--";
    const auto noteColour = active ? colourForCents(snapshot.pitch.cents) : textMuted();
    g.setColour(noteColour);
    g.setFont(uiFont(66.0f, true));
    g.drawFittedText(noteText, noteArea, juce::Justification::centred, 1);
    g.setColour(textMuted().withAlpha(0.72f));
    g.setFont(uiFont(12.0f, true));
    g.drawFittedText(preferSharps ? "#" : "b", noteArea.reduced(4).removeFromRight(18).removeFromBottom(18), juce::Justification::centred, 1);

    g.setColour(textMain());
    g.setFont(uiFont(20.0f, true));
    auto detail = inner.reduced(4, 5);
    g.drawFittedText(active ? centsText(snapshot.pitch.cents) : "No signal", detail.removeFromTop(28), juce::Justification::centredLeft, 1);
    g.setColour(textMuted());
    g.setFont(uiFont(14.0f));
    g.drawFittedText("Target " + octaveText + "  " + hzText(snapshot.pitch.targetHz), detail.removeFromTop(22), juce::Justification::centredLeft, 1);
    g.drawFittedText("Input " + hzText(snapshot.pitch.measuredHz) + "  Confidence " + juce::String(snapshot.pitch.confidence * 100.0, 0) + "%",
                     detail.removeFromTop(22), juce::Justification::centredLeft, 1);

    g.setColour(textMuted());
    g.setFont(uiFont(13.0f, true));
    g.drawFittedText("PROFILE", right.removeFromTop(18), juce::Justification::centredRight, 1);
    g.setColour(textMain());
    g.setFont(uiFont(18.0f, true));
    g.drawFittedText(processor.getActiveProfileName(), right.removeFromTop(28), juce::Justification::centredRight, 1);
    g.setColour(active && std::abs(snapshot.pitch.cents) <= 0.6 ? lockGreen() : textMuted());
    g.setFont(uiFont(14.0f));
    auto bottomRight = right;
    bottomRight.removeFromRight(146);
    g.drawFittedText(active && std::abs(snapshot.pitch.cents) <= 0.6 ? "LOCK" : "TRACK",
                     bottomRight, juce::Justification::centredRight, 1);
}

void GuitarForgeStrobeTunerAudioProcessorEditor::drawNeedleStrip(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    drawSurface(g, bounds.toFloat(), snapshot.pitch.voiced);
    auto area = bounds.reduced(20, 12);

    auto labelArea = area.removeFromLeft(150);
    g.setColour(textMuted());
    g.setFont(uiFont(12.0f, true));
    g.drawFittedText("CENTS", labelArea.removeFromTop(18), juce::Justification::centredLeft, 1);
    g.setColour(textMain());
    g.setFont(uiFont(24.0f, true));
    g.drawFittedText(snapshot.pitch.voiced ? centsText(snapshot.pitch.cents) : "--", labelArea.removeFromTop(34), juce::Justification::centredLeft, 1);
    g.setColour(textMuted());
    g.setFont(uiFont(13.0f));
    g.drawFittedText("A4 " + juce::String(a4Slider.getValue(), 2) + " Hz", labelArea, juce::Justification::centredLeft, 1);

    area.removeFromLeft(8);
    area.removeFromRight(132);

    auto strip = area.reduced(0, 8);
    strip.translate(-13, 0);
    const auto markerColour = accentColourForIndex(currentChoice(processor.parameters, "strobeColour"));
    const auto counterColour = counterColourFor(markerColour);
    const auto useContrastBar = processor.isBarContrastEnabled();

    juce::ColourGradient well(useContrastBar ? panelDark().darker(0.42f).interpolatedWith(counterColour, 0.10f) : juce::Colour::fromRGB(4, 5, 8),
                              static_cast<float>(strip.getX()), static_cast<float>(strip.getY()),
                              useContrastBar ? panelDark().darker(0.26f).interpolatedWith(counterColour, 0.18f) : juce::Colour::fromRGB(12, 13, 18),
                              static_cast<float>(strip.getX()), static_cast<float>(strip.getBottom()),
                              false);
    g.setGradientFill(well);
    g.fillRoundedRectangle(strip.toFloat(), 6.0f);

    const auto centreX = static_cast<float>(strip.getCentreX());
    const auto tickCount = 61;
    for (int tick = 0; tick < tickCount; ++tick)
    {
        const auto normalised = static_cast<float>(tick) / static_cast<float>(tickCount - 1);
        const auto x = juce::jmap(normalised, static_cast<float>(strip.getX() + 8), static_cast<float>(strip.getRight() - 8));
        const auto major = tick % 5 == 0;
        const auto tickHeight = major ? strip.getHeight() - 14.0f : strip.getHeight() - 24.0f;
        const auto y1 = static_cast<float>(strip.getCentreY()) - (tickHeight * 0.5f);
        const auto y2 = static_cast<float>(strip.getCentreY()) + (tickHeight * 0.5f);
        const auto colour = useContrastBar ? counterColour.interpolatedWith(textMain(), normalised * 0.18f) : spectralColour(normalised);

        g.setColour(colour.withAlpha(0.18f));
        g.drawLine(x, y1 - 5.0f, x, y2 + 5.0f, major ? 5.0f : 3.0f);
        g.setColour(colour.withAlpha(major ? 0.92f : 0.64f));
        g.drawLine(x, y1, x, y2, major ? 2.0f : 1.4f);
    }

    g.setColour(markerColour.withAlpha(0.86f));
    g.drawLine(centreX, static_cast<float>(strip.getY() + 4), centreX, static_cast<float>(strip.getBottom() - 4), 2.0f);

    const auto cents = juce::jlimit(-50.0, 50.0, snapshot.pitch.cents);
    const auto x = juce::jmap(static_cast<float>(cents), -50.0f, 50.0f, static_cast<float>(strip.getX() + 8), static_cast<float>(strip.getRight() - 8));
    const auto markerHeight = static_cast<float>(strip.getHeight() + 16);
    g.setColour(markerColour.withAlpha(snapshot.pitch.voiced ? 0.24f : 0.10f));
    g.fillRoundedRectangle(juce::Rectangle<float>(20.0f, markerHeight).withCentre({ x, static_cast<float>(strip.getCentreY()) }), 8.0f);
    g.setColour(markerColour.withAlpha(snapshot.pitch.voiced ? 0.96f : 0.36f));
    g.fillRoundedRectangle(juce::Rectangle<float>(7.0f, markerHeight - 6.0f).withCentre({ x, static_cast<float>(strip.getCentreY()) }), 3.0f);
    g.setColour(textMuted().withAlpha(0.36f));
    g.drawRoundedRectangle(strip.toFloat(), 6.0f, 1.0f);
}

bool GuitarForgeStrobeTunerAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    return handleDirectEntryKeyPress(key);
}

void GuitarForgeStrobeTunerAudioProcessorEditor::mouseUp(const juce::MouseEvent& event)
{
    if (noteClickBounds.contains(event.getEventRelativeTo(this).getPosition()))
    {
        preferSharps = ! processor.isSharpNotationEnabled();

        if (auto* parameter = processor.parameters.getParameter("preferSharps"))
            parameter->setValueNotifyingHost(preferSharps ? 1.0f : 0.0f);

        repaint();
    }
}

void GuitarForgeStrobeTunerAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    const auto screenPosition = event.getScreenPosition();

    if (a4ValueEditor.getScreenBounds().contains(screenPosition))
    {
        beginDirectTextEntry(ActiveValueField::a4);
        return;
    }

    if (sensitivityValueEditor.getScreenBounds().contains(screenPosition))
    {
        beginDirectTextEntry(ActiveValueField::sensitivity);
        return;
    }

    if (strobeRowsValueEditor.getScreenBounds().contains(screenPosition))
    {
        beginDirectTextEntry(ActiveValueField::strobeRows);
        return;
    }

    if (activeValueField != ActiveValueField::none)
        endDirectTextEntry(true);
}

void GuitarForgeStrobeTunerAudioProcessorEditor::launchBrandLinks()
{
    const juce::URL links[] =
    {
        juce::URL("https://slammincaptures.bigcartel.com/"),
        juce::URL("https://ko-fi.com/slammincaptures/shop"),
        juce::URL("https://www.tone3000.com/slamminmofo")
    };

    for (const auto& link : links)
        link.launchInDefaultBrowser();
}
