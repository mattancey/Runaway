#include "PluginEditor.h"

void RunawayLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                           int x, int y, int width, int height,
                                           float sliderPosProportional,
                                           float rotaryStartAngle,
                                           float rotaryEndAngle,
                                           juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                                static_cast<float> (y),
                                                static_cast<float> (width),
                                                static_cast<float> (height)).reduced (6.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    juce::Path trackArc;
    trackArc.addCentredArc (centre.x, centre.y, radius, radius,
                            0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff262a33));
    g.strokePath (trackArc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius, radius,
                            0.0f, rotaryStartAngle, angle, true);
    g.setColour (accent);
    g.strokePath (valueArc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

    const float bodyR = radius * 0.72f;
    juce::ColourGradient bodyGrad (juce::Colour (0xff2c313b), centre.x, centre.y - bodyR,
                                   juce::Colour (0xff15181e), centre.x, centre.y + bodyR, false);
    g.setGradientFill (bodyGrad);
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

    g.setColour (juce::Colour (0xff3c4250));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    juce::Path pointer;
    const float pLen = bodyR * 0.78f;
    const float pStart = bodyR * 0.25f;
    pointer.startNewSubPath (centre.x + std::sin (angle) * pStart,
                             centre.y - std::cos (angle) * pStart);
    pointer.lineTo (centre.x + std::sin (angle) * pLen,
                    centre.y - std::cos (angle) * pLen);
    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.strokePath (pointer, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
}

void RunawayLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                               juce::Button& button,
                                               const juce::Colour&,
                                               bool shouldDrawButtonAsHighlighted,
                                               bool)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool active = button.getToggleState();

    const auto fill = active ? accent.withAlpha (0.24f)
                             : (shouldDrawButtonAsHighlighted ? juce::Colour (0xff232833) : juce::Colour (0xff171a21));
    const auto border = active ? accent : juce::Colour (0xff323846);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (border);
    g.drawRoundedRectangle (bounds, 6.0f, active ? 1.4f : 1.0f);
}

RunawayAudioProcessorEditor::RunawayAudioProcessorEditor (RunawayAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    auto& apvts = audioProcessor.getAPVTS();

    presetSelector.addItemList (RunawayAudioProcessor::getFactoryPresetNames(), 1);
    presetSelector.setTextWhenNothingSelected ("PRESETS...");
    presetSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1a1d24));
    presetSelector.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff323846));
    presetSelector.setColour (juce::ComboBox::textColourId, juce::Colour (0xffcfd6e6));
    presetSelector.onChange = [this]
    {
        const int sel = presetSelector.getSelectedId() - 1;
        if (sel >= 0)
            audioProcessor.loadFactoryPreset (sel);
    };
    addAndMakeVisible (presetSelector);

    modeSelector.addItemList ({
        "BUCHLA / SOMA  —  259 Folder + Harvezi Spoil",
        "BIFOLD MORPH   —  Buchla Parallel <-> Serge Serial",
        "ERICA / XENO   —  ADAA Tanh + Germanium Scream",
        "112dB / CAT    —  Nonlinear Werner TPT SVF"
    }, 1);
    modeSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1a1d24));
    modeSelector.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff323846));
    modeSelector.setColour (juce::ComboBox::textColourId, juce::Colour (0xffe8ecf4));
    addAndMakeVisible (modeSelector);

    loopEngineSelector.addItemList ({
        "STRING   —  Morphor Echon 6 BBD",
        "CHORD    —  112dB Razornator 4-Voice",
        "DIFFUSE  —  Strymon NightSky FDN"
    }, 1);
    loopEngineSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff181b22));
    loopEngineSelector.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff323846));
    loopEngineSelector.setColour (juce::ComboBox::textColourId, juce::Colour (0xffe8ecf4));
    addAndMakeVisible (loopEngineSelector);

    envTargetSelector.addItemList ({
        "TARGET: OFF",
        "TARGET: DRIVE / FOLD",
        "TARGET: CHARACTER",
        "TARGET: LOOP LPF",
        "TARGET: RESO PITCH"
    }, 1);
    envTargetSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff181b22));
    envTargetSelector.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff323846));
    envTargetSelector.setColour (juce::ComboBox::textColourId, juce::Colour (0xffe8ecf4));
    addAndMakeVisible (envTargetSelector);

    configureKnob (preCutoffSlider, preCutoffLabel, "PRE-LPF");
    configureKnob (driveSlider, driveLabel, "DRIVE");
    configureKnob (characterSlider, characterLabel, "SPOIL");
    configureKnob (mixSlider, mixLabel, "MIX");
    configureKnob (outputSlider, outputLabel, "WET LEVEL");

    configureKnob (pitchSlider, pitchLabel, "RESO PITCH");
    configureKnob (feedbackSlider, feedbackLabel, "FEEDBACK");
    configureKnob (loopCutoffSlider, loopCutoffLabel, "LOOP LPF");
    configureKnob (spreadSlider, spreadLabel, "BBD SPREAD");
    configureKnob (loopLevelSlider, loopLevelLabel, "IN-LOOP LVL");

    configureKnob (envAmountSlider, envAmountLabel, "ENV AMOUNT");
    configureKnob (envReleaseSlider, envReleaseLabel, "ENV RELEASE");

    configureToggleButton (loopActiveButton);
    configureToggleButton (midiTrackButton);
    configureToggleButton (polarityButton);
    configureToggleButton (loopModeButton);

    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "mode", modeSelector);
    loopEngineAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "loopEngine", loopEngineSelector);
    envTargetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "envTarget", envTargetSelector);

    preCutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "preCutoff", preCutoffSlider);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "drive", driveSlider);
    characterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "character", characterSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "mix", mixSlider);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "output", outputSlider);

    pitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "pitch", pitchSlider);
    feedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "feedback", feedbackSlider);
    loopCutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "loopCutoff", loopCutoffSlider);
    spreadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "spread", spreadSlider);
    loopLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "loopLevel", loopLevelSlider);

    envAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "envAmount", envAmountSlider);
    envReleaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "envRelease", envReleaseSlider);

    loopActiveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "loopActive", loopActiveButton);
    midiTrackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "midiTrack", midiTrackButton);
    polarityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "polarity", polarityButton);
    loopModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "inLoop", loopModeButton);

    pitchSlider.textFromValueFunction = [] (double hz)
    {
        static constexpr std::array<const char*, 12> noteNames {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        const double midiFloat = 69.0 + 12.0 * std::log2 (std::max (1.0, hz) / 440.0);
        const int midiNote = juce::roundToInt (midiFloat);
        const int octave = (midiNote / 12) - 1;
        const size_t pc = static_cast<size_t> (((midiNote % 12) + 12) % 12);
        return juce::String (juce::roundToInt (hz)) + " Hz (" + noteNames[pc] + juce::String (octave) + ")";
    };
    pitchSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 108, 18);
    pitchSlider.updateText();

    updateModeAesthetics();
    updateLoopEngineLabels();
    updateButtonLabels();
    setSize (840, 690);
    startTimerHz (30);
}

RunawayAudioProcessorEditor::~RunawayAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RunawayAudioProcessorEditor::configureKnob (juce::Slider& slider, juce::Label& title, const juce::String& text)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 18);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffb6becd));
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff14171d));
    addAndMakeVisible (slider);

    title.setText (text, juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    title.setColour (juce::Label::textColourId, juce::Colour (0xff8a93a6));
    addAndMakeVisible (title);
}

void RunawayAudioProcessorEditor::configureToggleButton (juce::TextButton& button)
{
    button.setClickingTogglesState (true);
    button.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff8e96aa));
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    addAndMakeVisible (button);
}

void RunawayAudioProcessorEditor::updateButtonLabels()
{
    const bool loopOn = loopActiveButton.getToggleState();
    const bool inLoopOn = loopModeButton.getToggleState();
    loopActiveButton.setButtonText (loopOn ? "LOOP: ON" : "LOOP: BYPASS");

    const float midiHz = audioProcessor.getActiveMidiFreqHz();
    if (midiTrackButton.getToggleState())
    {
        midiTrackButton.setButtonText (midiHz > 1.0f
                                           ? ("MIDI: " + juce::String (juce::roundToInt (midiHz)) + " Hz")
                                           : "MIDI: WAIT");
    }
    else
    {
        midiTrackButton.setButtonText ("MIDI TRACK");
    }

    polarityButton.setButtonText (polarityButton.getToggleState() ? "POL: - HOLLOW" : "POL: + STRING");
    loopModeButton.setButtonText (inLoopOn ? "ROUTING: IN-LOOP" : "ROUTING: PRE");

    const float loopDim = loopOn ? 1.0f : 0.42f;
    pitchSlider.setAlpha (loopDim);
    feedbackSlider.setAlpha (loopDim);
    loopCutoffSlider.setAlpha (loopDim);
    spreadSlider.setAlpha (loopDim);

    const float inLoopDim = (loopOn && inLoopOn) ? 1.0f : 0.38f;
    loopLevelSlider.setAlpha (inLoopDim);
    loopLevelLabel.setAlpha (inLoopDim);

    const bool envActive = (envTargetSelector.getSelectedId() > 1);
    const float envDim = envActive ? 1.0f : 0.50f;
    envAmountSlider.setAlpha (envDim);
    envReleaseSlider.setAlpha (envDim);
}

void RunawayAudioProcessorEditor::updateLoopEngineLabels()
{
    const int engIdx = std::max (0, loopEngineSelector.getSelectedId() - 1);
    if (engIdx == lastLoopEngineIndex)
        return;

    lastLoopEngineIndex = engIdx;

    switch (engIdx)
    {
        case 0:
            spreadLabel.setText ("BBD SPREAD", juce::dontSendNotification);
            pitchLabel.setText ("RESO PITCH", juce::dontSendNotification);
            break;

        case 1:
            spreadLabel.setText ("CHORD BLOOM", juce::dontSendNotification);
            pitchLabel.setText ("CHORD ROOT", juce::dontSendNotification);
            break;

        case 2:
        default:
            spreadLabel.setText ("FDN GLIMMER", juce::dontSendNotification);
            pitchLabel.setText ("SPACE SIZE", juce::dontSendNotification);
            break;
    }
}

void RunawayAudioProcessorEditor::updateModeAesthetics()
{
    const int idx = std::max (0, modeSelector.getSelectedId() - 1);
    if (idx == lastModeIndex)
        return;

    lastModeIndex = idx;

    switch (idx)
    {
        case 0:
            lookAndFeel.setAccentColour (juce::Colour (0xffff7a2f));
            characterLabel.setText ("SPOIL (SOMA)", juce::dontSendNotification);
            driveLabel.setText ("SPREAD / FOLD", juce::dontSendNotification);
            break;

        case 1:
            lookAndFeel.setAccentColour (juce::Colour (0xff3be8b0));
            characterLabel.setText ("BIFOLD MORPH", juce::dontSendNotification);
            driveLabel.setText ("FOLD DEPTH", juce::dontSendNotification);
            break;

        case 2:
            lookAndFeel.setAccentColour (juce::Colour (0xffff3864));
            characterLabel.setText ("XENO SCREAM", juce::dontSendNotification);
            driveLabel.setText ("PRE-GAIN", juce::dontSendNotification);
            break;

        case 3:
        default:
            lookAndFeel.setAccentColour (juce::Colour (0xff5ca9ff));
            characterLabel.setText ("RESONANCE Q", juce::dontSendNotification);
            driveLabel.setText ("CORE DRIVE", juce::dontSendNotification);
            break;
    }

    repaint();
}

void RunawayAudioProcessorEditor::timerCallback()
{
    updateModeAesthetics();
    updateLoopEngineLabels();
    updateButtonLabels();

    const float targetEnv = audioProcessor.getEnvelopeMeterLevel();
    envMeterSmooth = envMeterSmooth * 0.65f + targetEnv * 0.35f;

    audioProcessor.readScopeData (dryScope, wetScope);
    repaint (scopeBounds);
    repaint (envMeterBounds);
}

void RunawayAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0e1015));

    auto header = getLocalBounds().removeFromTop (56);
    g.setColour (juce::Colour (0xff151820));
    g.fillRect (header);

    g.setColour (juce::Colour (0xff262b36));
    g.drawHorizontalLine (header.getBottom(), 0.0f, static_cast<float> (getWidth()));

    g.setColour (juce::Colour (0xfff0f3fa));
    g.setFont (juce::Font (juce::FontOptions (21.0f, juce::Font::bold)));
    g.drawText ("RUNAWAY", 20, 10, 140, 26, juce::Justification::centredLeft);

    g.setColour (lookAndFeel.getAccentColour());
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText ("4x WAVESHAPER • ECHON 6 / NIGHTSKY / ANALOG HEAT", 20, 34, 340, 14, juce::Justification::centredLeft);

    const auto sf = scopeBounds.toFloat();
    g.setColour (juce::Colour (0xff090b0f));
    g.fillRoundedRectangle (sf, 8.0f);
    g.setColour (juce::Colour (0xff232833));
    g.drawRoundedRectangle (sf, 8.0f, 1.0f);

    g.setColour (juce::Colour (0xff181c24));
    g.drawHorizontalLine (scopeBounds.getCentreY(), sf.getX() + 6.0f, sf.getRight() - 6.0f);
    for (int i = 1; i < 4; ++i)
    {
        const float gx = sf.getX() + sf.getWidth() * (static_cast<float> (i) / 4.0f);
        g.drawVerticalLine (static_cast<int> (gx), sf.getY() + 6.0f, sf.getBottom() - 6.0f);
    }

    if (scopeBounds.getWidth() > 16)
    {
        juce::Path dryPath;
        juce::Path wetPath;

        const float midY = sf.getCentreY();
        const float ampScale = sf.getHeight() * 0.42f;
        const int n = RunawayAudioProcessor::scopeBufferSize;

        for (int i = 0; i < n; ++i)
        {
            const float px = sf.getX() + 6.0f
                             + (sf.getWidth() - 12.0f) * (static_cast<float> (i) / static_cast<float> (n - 1));
            const float dY = midY - juce::jlimit (-1.1f, 1.1f, dryScope[static_cast<size_t> (i)]) * ampScale;
            const float wY = midY - juce::jlimit (-1.1f, 1.1f, wetScope[static_cast<size_t> (i)]) * ampScale;

            if (i == 0)
            {
                dryPath.startNewSubPath (px, dY);
                wetPath.startNewSubPath (px, wY);
            }
            else
            {
                dryPath.lineTo (px, dY);
                wetPath.lineTo (px, wY);
            }
        }

        g.setColour (juce::Colour (0xff495163).withAlpha (0.55f));
        g.strokePath (dryPath, juce::PathStrokeType (1.2f));

        g.setColour (lookAndFeel.getAccentColour());
        g.strokePath (wetPath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.setColour (juce::Colour (0xff677085));
    g.drawText ("DRY IN", scopeBounds.getX() + 12, scopeBounds.getY() + 8, 60, 14, juce::Justification::left);
    g.setColour (lookAndFeel.getAccentColour());
    g.drawText ("RUNAWAY OUT", scopeBounds.getX() + 68, scopeBounds.getY() + 8, 100, 14, juce::Justification::left);

    const auto rf = row2Bounds.toFloat();
    g.setColour (juce::Colour (0xff13161d));
    g.fillRoundedRectangle (rf, 8.0f);
    g.setColour (juce::Colour (0xff232833));
    g.drawRoundedRectangle (rf, 8.0f, 1.0f);

    g.setColour (juce::Colour (0xff6b7488));
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText ("LOOP ENGINE",
                row2Bounds.getX() + 14, row2Bounds.getY() + 14, 90, 18,
                juce::Justification::centredLeft);

    const auto ef = row3Bounds.toFloat();
    g.setColour (juce::Colour (0xff13161d));
    g.fillRoundedRectangle (ef, 8.0f);
    g.setColour (juce::Colour (0xff232833));
    g.drawRoundedRectangle (ef, 8.0f, 1.0f);

    g.setColour (juce::Colour (0xff6b7488));
    g.drawText ("ENV FOLLOWER  (ANALOG HEAT)",
                row3Bounds.getX() + 14, row3Bounds.getY() + 10, 210, 16,
                juce::Justification::centredLeft);

    const auto mf = envMeterBounds.toFloat();
    g.setColour (juce::Colour (0xff090b0f));
    g.fillRoundedRectangle (mf, 4.0f);
    g.setColour (juce::Colour (0xff252a36));
    g.drawRoundedRectangle (mf, 4.0f, 1.0f);

    const float fillW = std::max (0.0f, (mf.getWidth() - 4.0f) * juce::jlimit (0.0f, 1.0f, envMeterSmooth));
    if (fillW > 1.0f)
    {
        g.setColour (lookAndFeel.getAccentColour().withAlpha (0.85f));
        g.fillRoundedRectangle (mf.getX() + 2.0f, mf.getY() + 2.0f, fillW, mf.getHeight() - 4.0f, 3.0f);
    }

    g.setColour (juce::Colour (0xff677085));
    g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    g.drawText ("ENV ACTIVITY", envMeterBounds.getX(), envMeterBounds.getY() - 16, envMeterBounds.getWidth(), 14,
                juce::Justification::centredLeft);
}

void RunawayAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (56);
    auto headerRight = header.removeFromRight (535).reduced (14, 12);
    presetSelector.setBounds (headerRight.removeFromLeft (185));
    headerRight.removeFromLeft (8);
    modeSelector.setBounds (headerRight);

    area.reduce (20, 12);

    scopeBounds = area.removeFromTop (112);
    area.removeFromTop (10);

    auto row1 = area.removeFromTop (128);
    const int col1W = area.getWidth() / 5;

    auto placeKnob1 = [&] (int index, juce::Slider& s, juce::Label& l)
    {
        auto slot = row1.withX (row1.getX() + index * col1W).withWidth (col1W);
        l.setBounds (slot.removeFromTop (18));
        s.setBounds (slot.reduced (4, 0));
    };

    placeKnob1 (0, preCutoffSlider, preCutoffLabel);
    placeKnob1 (1, driveSlider, driveLabel);
    placeKnob1 (2, characterSlider, characterLabel);
    placeKnob1 (3, mixSlider, mixLabel);
    placeKnob1 (4, outputSlider, outputLabel);

    area.removeFromTop (10);

    row2Bounds = area.removeFromTop (186);
    auto innerRow2 = row2Bounds.reduced (10, 8);

    auto loopTopBar = innerRow2.removeFromTop (30);
    loopTopBar.removeFromLeft (94);
    loopEngineSelector.setBounds (loopTopBar.removeFromLeft (270).reduced (0, 2));
    loopActiveButton.setBounds (loopTopBar.removeFromRight (120).reduced (2, 1));
    loopTopBar.removeFromRight (8);
    midiTrackButton.setBounds (loopTopBar.removeFromRight (120).reduced (2, 1));

    innerRow2.removeFromTop (8);

    const int col2W = innerRow2.getWidth() / 6;
    auto placeKnob2 = [&] (int index, juce::Slider& s, juce::Label& l)
    {
        auto slot = innerRow2.withX (innerRow2.getX() + index * col2W).withWidth (col2W);
        l.setBounds (slot.removeFromTop (18));
        s.setBounds (slot.reduced (2, 0));
    };

    placeKnob2 (0, pitchSlider, pitchLabel);
    placeKnob2 (1, feedbackSlider, feedbackLabel);
    placeKnob2 (2, loopCutoffSlider, loopCutoffLabel);
    placeKnob2 (3, spreadSlider, spreadLabel);
    placeKnob2 (4, loopLevelSlider, loopLevelLabel);

    auto toggleCol = innerRow2.withX (innerRow2.getX() + 5 * col2W).withWidth (col2W).reduced (4, 14);
    polarityButton.setBounds (toggleCol.removeFromTop (38));
    toggleCol.removeFromTop (10);
    loopModeButton.setBounds (toggleCol.removeFromTop (38));

    area.removeFromTop (10);

    row3Bounds = area;
    auto innerRow3 = row3Bounds.reduced (14, 10);

    auto envLeftCol = innerRow3.removeFromLeft (220);
    envLeftCol.removeFromTop (24);
    envTargetSelector.setBounds (envLeftCol.removeFromTop (30));

    innerRow3.removeFromLeft (16);

    auto placeEnvKnob = [&] (juce::Rectangle<int> slot, juce::Slider& s, juce::Label& l)
    {
        l.setBounds (slot.removeFromTop (16));
        s.setBounds (slot.reduced (4, 0));
    };

    placeEnvKnob (innerRow3.removeFromLeft (130), envAmountSlider, envAmountLabel);
    innerRow3.removeFromLeft (8);
    placeEnvKnob (innerRow3.removeFromLeft (130), envReleaseSlider, envReleaseLabel);

    innerRow3.removeFromLeft (20);
    envMeterBounds = innerRow3.withSizeKeepingCentre (innerRow3.getWidth(), 18).translated (0, 8);
}
