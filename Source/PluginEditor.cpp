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

    configureActionButton (abSlotButton);
    abSlotButton.onClick = [this]
    {
        audioProcessor.toggleABSlot();
        updateButtonLabels();
    };

    configureActionButton (abCopyButton);
    abCopyButton.setButtonText ("COPY");
    abCopyButton.onClick = [this]
    {
        audioProcessor.copyActiveSlotToOther();
    };

    presetSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1a1d24));
    presetSelector.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff323846));
    presetSelector.setColour (juce::ComboBox::textColourId, juce::Colour (0xffcfd6e6));
    presetSelector.onChange = [this]
    {
        const int id = presetSelector.getSelectedId();
        if (id >= 1 && id <= RunawayAudioProcessor::numFactoryPresets)
        {
            audioProcessor.loadFactoryPreset (id - 1);
        }
        else if (id >= 100)
        {
            const int userIdx = id - 100;
            if (userIdx >= 0 && userIdx < userPresetFiles.size())
                audioProcessor.loadUserPresetFromFile (userPresetFiles[userIdx]);
        }
    };
    addAndMakeVisible (presetSelector);
    refreshPresetMenu();

    configureActionButton (savePresetButton);
    savePresetButton.setButtonText ("SAVE");
    savePresetButton.onClick = [this] { triggerSavePreset(); };

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

    lpgModeSelector.addItemList ({
        "LPG: OFF",
        "LPG: BOTH (292)",
        "LPG: VCA ONLY",
        "LPG: FILTER ONLY"
    }, 1);
    lpgModeSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff181b22));
    lpgModeSelector.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff323846));
    lpgModeSelector.setColour (juce::ComboBox::textColourId, juce::Colour (0xffe8ecf4));
    addAndMakeVisible (lpgModeSelector);

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

    configureKnob (lpgDecaySlider, lpgDecayLabel, "LPG DECAY");
    configureKnob (lpgResSlider, lpgResLabel, "LPG RING / Q");

    configureToggleButton (loopActiveButton);
    configureToggleButton (midiTrackButton);
    configureToggleButton (polarityButton);
    configureToggleButton (loopModeButton);

    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "mode", modeSelector);
    loopEngineAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "loopEngine", loopEngineSelector);
    envTargetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "envTarget", envTargetSelector);
    lpgModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "lpgMode", lpgModeSelector);

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

    lpgDecayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "lpgDecay", lpgDecaySlider);
    lpgResAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "lpgRes", lpgResSlider);

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
    setSize (880, 720);
    startTimerHz (30);
}

RunawayAudioProcessorEditor::~RunawayAudioProcessorEditor()
{
    stopTimer();
    fileChooser.reset();
    setLookAndFeel (nullptr);
}

void RunawayAudioProcessorEditor::refreshPresetMenu (const juce::String& selectName)
{
    presetSelector.clear (juce::dontSendNotification);
    presetSelector.setTextWhenNothingSelected ("PRESETS...");

    const auto factoryNames = RunawayAudioProcessor::getFactoryPresetNames();
    for (int i = 0; i < factoryNames.size(); ++i)
        presetSelector.addItem (factoryNames[i], i + 1);

    userPresetFiles = RunawayAudioProcessor::scanUserPresetFiles();
    if (! userPresetFiles.isEmpty())
    {
        presetSelector.addSeparator();
        presetSelector.addSectionHeading ("USER PRESETS");

        for (int i = 0; i < userPresetFiles.size(); ++i)
        {
            const auto name = userPresetFiles[i].getFileNameWithoutExtension();
            presetSelector.addItem (name, 100 + i);
            if (selectName.isNotEmpty() && name == selectName)
                presetSelector.setSelectedId (100 + i, juce::dontSendNotification);
        }
    }
}

void RunawayAudioProcessorEditor::triggerSavePreset()
{
    const auto dir = RunawayAudioProcessor::getUserPresetsDirectory();
    if (! dir.isDirectory())
        dir.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser> (
        "Save Runaway Preset",
        dir.getChildFile ("MyPatch.runaway"),
        "*.runaway");

    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto result = chooser.getResult();
            if (result != juce::File{})
            {
                const auto target = result.hasFileExtension (".runaway")
                    ? result
                    : result.withFileExtension (".runaway");
                if (audioProcessor.saveUserPresetToFile (target))
                    refreshPresetMenu (target.getFileNameWithoutExtension());
            }
        });
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

void RunawayAudioProcessorEditor::configureActionButton (juce::TextButton& button)
{
    button.setClickingTogglesState (false);
    button.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd5dceb));
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    addAndMakeVisible (button);
}

void RunawayAudioProcessorEditor::updateButtonLabels()
{
    abSlotButton.setButtonText (audioProcessor.getActiveABSlot() == 0 ? "SLOT: A" : "SLOT: B");

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

    const bool lpgActive = (lpgModeSelector.getSelectedId() > 1);
    const float lpgDim = lpgActive ? 1.0f : 0.45f;
    lpgDecaySlider.setAlpha (lpgDim);
    lpgResSlider.setAlpha (lpgDim);
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

void RunawayAudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    if (scopeBounds.contains (event.getPosition()))
    {
        showTransferCurve = ! showTransferCurve;
        repaint (scopeBounds);
    }
}

void RunawayAudioProcessorEditor::timerCallback()
{
    updateModeAesthetics();
    updateLoopEngineLabels();
    updateButtonLabels();

    const float targetEnv = audioProcessor.getEnvelopeMeterLevel();
    envMeterSmooth = envMeterSmooth * 0.65f + targetEnv * 0.35f;

    const float targetLpg = audioProcessor.getVactrolMeterLevel();
    lpgMeterSmooth = lpgMeterSmooth * 0.55f + targetLpg * 0.45f;

    audioProcessor.readScopeData (dryScope, wetScope);
    repaint (scopeBounds);
    repaint (envMeterBounds);
    repaint (lpgMeterBounds);
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
    g.drawText ("RUNAWAY", 18, 10, 130, 26, juce::Justification::centredLeft);

    g.setColour (lookAndFeel.getAccentColour());
    g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    g.drawText ("v0.5  •  BUCHLA LPG / ECHON 6 / NIGHTSKY", 18, 34, 260, 14, juce::Justification::centredLeft);

    // Visualiser panel (Scope or Transfer Curve X/Y)
    const auto sf = scopeBounds.toFloat();
    g.setColour (juce::Colour (0xff090b0f));
    g.fillRoundedRectangle (sf, 8.0f);
    g.setColour (juce::Colour (0xff232833));
    g.drawRoundedRectangle (sf, 8.0f, 1.0f);

    g.setColour (juce::Colour (0xff181c24));
    g.drawHorizontalLine (scopeBounds.getCentreY(), sf.getX() + 6.0f, sf.getRight() - 6.0f);
    g.drawVerticalLine (scopeBounds.getCentreX(), sf.getY() + 6.0f, sf.getBottom() - 6.0f);

    if (scopeBounds.getWidth() > 16)
    {
        if (showTransferCurve)
        {
            // Subtle y = x reference line
            g.setColour (juce::Colour (0xff2f3644));
            g.drawLine (sf.getX() + 12.0f, sf.getBottom() - 8.0f,
                        sf.getRight() - 12.0f, sf.getY() + 8.0f, 1.0f);

            const int modeIdx = std::max (0, modeSelector.getSelectedId() - 1);
            const float drvNorm = static_cast<float> (driveSlider.getValue() * 0.01);
            const float chrNorm = static_cast<float> (characterSlider.getValue() * 0.01);

            juce::Path curvePath;
            constexpr int numPts = 180;
            const float midY = sf.getCentreY();
            const float ampScale = sf.getHeight() * 0.42f;

            for (int i = 0; i < numPts; ++i)
            {
                const float norm = static_cast<float> (i) / static_cast<float> (numPts - 1);
                const float xIn = -1.0f + 2.0f * norm;
                const float yOut = runaway::dsp::WaveshaperEngine::evalTransferPreview (modeIdx, xIn, drvNorm, chrNorm);
                const float px = sf.getX() + 12.0f + (sf.getWidth() - 24.0f) * norm;
                const float py = midY - juce::jlimit (-1.1f, 1.1f, yOut) * ampScale;

                if (i == 0)
                    curvePath.startNewSubPath (px, py);
                else
                    curvePath.lineTo (px, py);
            }

            g.setColour (lookAndFeel.getAccentColour());
            g.strokePath (curvePath, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));

            // Live input excursion tracer along curve
            const float livePeak = juce::jlimit (0.0f, 1.0f, audioProcessor.getLiveInputPeak());
            if (livePeak > 0.01f)
            {
                for (float sign : { -1.0f, 1.0f })
                {
                    const float xPt = sign * livePeak;
                    const float yPt = runaway::dsp::WaveshaperEngine::evalTransferPreview (modeIdx, xPt, drvNorm, chrNorm);
                    const float px = sf.getX() + 12.0f + (sf.getWidth() - 24.0f) * (0.5f + 0.5f * xPt);
                    const float py = midY - juce::jlimit (-1.1f, 1.1f, yPt) * ampScale;
                    g.setColour (juce::Colours::white.withAlpha (0.9f));
                    g.fillEllipse (px - 3.5f, py - 3.5f, 7.0f, 7.0f);
                }
            }
        }
        else
        {
            for (int i = 1; i < 4; ++i)
            {
                if (i == 2) continue;
                const float gx = sf.getX() + sf.getWidth() * (static_cast<float> (i) / 4.0f);
                g.setColour (juce::Colour (0xff181c24));
                g.drawVerticalLine (static_cast<int> (gx), sf.getY() + 6.0f, sf.getBottom() - 6.0f);
            }

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
    }

    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    if (showTransferCurve)
    {
        g.setColour (lookAndFeel.getAccentColour());
        g.drawText ("TRANSFER CURVE  y = f(x)", scopeBounds.getX() + 12, scopeBounds.getY() + 8, 170, 14, juce::Justification::left);
    }
    else
    {
        g.setColour (juce::Colour (0xff677085));
        g.drawText ("DRY IN", scopeBounds.getX() + 12, scopeBounds.getY() + 8, 60, 14, juce::Justification::left);
        g.setColour (lookAndFeel.getAccentColour());
        g.drawText ("RUNAWAY OUT", scopeBounds.getX() + 68, scopeBounds.getY() + 8, 100, 14, juce::Justification::left);
    }

    // Clickable view pill
    auto viewPill = juce::Rectangle<int> (scopeBounds.getRight() - 150, scopeBounds.getY() + 6, 140, 18);
    g.setColour (juce::Colour (0xff181c24));
    g.fillRoundedRectangle (viewPill.toFloat(), 4.0f);
    g.setColour (juce::Colour (0xff8a93a6));
    g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    g.drawText (showTransferCurve ? "VIEW: CURVE  (CLICK)" : "VIEW: SCOPE  (CLICK)",
                viewPill, juce::Justification::centred);

    // Row 2 container (Loop Engine)
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

    // Row 3 Left container (Envelope Follower)
    g.setColour (juce::Colour (0xff13161d));
    g.fillRoundedRectangle (row3LeftBounds.toFloat(), 8.0f);
    g.setColour (juce::Colour (0xff232833));
    g.drawRoundedRectangle (row3LeftBounds.toFloat(), 8.0f, 1.0f);

    g.setColour (juce::Colour (0xff6b7488));
    g.drawText ("ENV FOLLOWER  (ANALOG HEAT)",
                row3LeftBounds.getX() + 12, row3LeftBounds.getY() + 8, 220, 16,
                juce::Justification::centredLeft);

    // Row 3 Right container (Buchla 292 Vactrol LPG)
    g.setColour (juce::Colour (0xff13161d));
    g.fillRoundedRectangle (row3RightBounds.toFloat(), 8.0f);
    g.setColour (juce::Colour (0xff232833));
    g.drawRoundedRectangle (row3RightBounds.toFloat(), 8.0f, 1.0f);

    g.setColour (juce::Colour (0xff6b7488));
    g.drawText ("VACTROL LPG  (BUCHLA 292 / OPTOMIX)",
                row3RightBounds.getX() + 12, row3RightBounds.getY() + 8, 260, 16,
                juce::Justification::centredLeft);

    auto drawMeter = [&] (const juce::Rectangle<int>& box, float val, const juce::String& labelText)
    {
        const auto mf = box.toFloat();
        g.setColour (juce::Colour (0xff090b0f));
        g.fillRoundedRectangle (mf, 4.0f);
        g.setColour (juce::Colour (0xff252a36));
        g.drawRoundedRectangle (mf, 4.0f, 1.0f);

        const float fillW = std::max (0.0f, (mf.getWidth() - 4.0f) * juce::jlimit (0.0f, 1.0f, val));
        if (fillW > 1.0f)
        {
            g.setColour (lookAndFeel.getAccentColour().withAlpha (0.85f));
            g.fillRoundedRectangle (mf.getX() + 2.0f, mf.getY() + 2.0f, fillW, mf.getHeight() - 4.0f, 3.0f);
        }

        g.setColour (juce::Colour (0xff677085));
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        g.drawText (labelText, box.getX(), box.getY() - 16, box.getWidth(), 14, juce::Justification::centredLeft);
    };

    drawMeter (envMeterBounds, envMeterSmooth, "ENV ACTIVITY");
    drawMeter (lpgMeterBounds, lpgMeterSmooth, "VACTROL LED");
}

void RunawayAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Header: A/B + Copy + Presets + Save + Topology
    auto header = area.removeFromTop (56);
    auto headerRight = header.removeFromRight (605).reduced (12, 12);

    abSlotButton.setBounds (headerRight.removeFromLeft (64));
    headerRight.removeFromLeft (4);
    abCopyButton.setBounds (headerRight.removeFromLeft (46));
    headerRight.removeFromLeft (8);
    presetSelector.setBounds (headerRight.removeFromLeft (162));
    headerRight.removeFromLeft (4);
    savePresetButton.setBounds (headerRight.removeFromLeft (48));
    headerRight.removeFromLeft (8);
    modeSelector.setBounds (headerRight);

    area.reduce (20, 10);

    scopeBounds = area.removeFromTop (108);
    area.removeFromTop (10);

    // Row 1: 5 knobs (Shaper + Master)
    auto row1 = area.removeFromTop (124);
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

    // Row 2: Loop Engine container
    row2Bounds = area.removeFromTop (180);
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

    auto toggleCol = innerRow2.withX (innerRow2.getX() + 5 * col2W).withWidth (col2W).reduced (4, 12);
    polarityButton.setBounds (toggleCol.removeFromTop (36));
    toggleCol.removeFromTop (10);
    loopModeButton.setBounds (toggleCol.removeFromTop (36));

    area.removeFromTop (10);

    // Row 3: Split Env Follower (left) + Vactrol LPG (right)
    const int halfW = (area.getWidth() - 12) / 2;
    row3LeftBounds = area.removeFromLeft (halfW);
    area.removeFromLeft (12);
    row3RightBounds = area;

    // Left: Env Follower
    auto innerLeft = row3LeftBounds.reduced (12, 10);
    innerLeft.removeFromTop (22);
    auto envCtrlCol = innerLeft.removeFromLeft (158);
    envTargetSelector.setBounds (envCtrlCol.removeFromTop (30));
    envCtrlCol.removeFromTop (28);
    envMeterBounds = envCtrlCol.removeFromTop (16);

    innerLeft.removeFromLeft (10);
    const int envKnobW = innerLeft.getWidth() / 2;
    auto envSlot1 = innerLeft.removeFromLeft (envKnobW);
    envAmountLabel.setBounds (envSlot1.removeFromTop (16));
    envAmountSlider.setBounds (envSlot1.reduced (2, 0));

    envReleaseLabel.setBounds (innerLeft.removeFromTop (16));
    envReleaseSlider.setBounds (innerLeft.reduced (2, 0));

    // Right: Vactrol LPG
    auto innerRight = row3RightBounds.reduced (12, 10);
    innerRight.removeFromTop (22);
    auto lpgCtrlCol = innerRight.removeFromLeft (158);
    lpgModeSelector.setBounds (lpgCtrlCol.removeFromTop (30));
    lpgCtrlCol.removeFromTop (28);
    lpgMeterBounds = lpgCtrlCol.removeFromTop (16);

    innerRight.removeFromLeft (10);
    const int lpgKnobW = innerRight.getWidth() / 2;
    auto lpgSlot1 = innerRight.removeFromLeft (lpgKnobW);
    lpgDecayLabel.setBounds (lpgSlot1.removeFromTop (16));
    lpgDecaySlider.setBounds (lpgSlot1.reduced (2, 0));

    lpgResLabel.setBounds (innerRight.removeFromTop (16));
    lpgResSlider.setBounds (innerRight.reduced (2, 0));
}
