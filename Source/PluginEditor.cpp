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

    const auto fill = active ? accent.withAlpha (0.22f)
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

    configureKnob (preCutoffSlider, preCutoffLabel, "PRE-LPF");
    configureKnob (driveSlider, driveLabel, "DRIVE");
    configureKnob (characterSlider, characterLabel, "SPOIL");
    configureKnob (mixSlider, mixLabel, "MIX");
    configureKnob (outputSlider, outputLabel, "OUTPUT");

    configureKnob (pitchSlider, pitchLabel, "RESO PITCH");
    configureKnob (feedbackSlider, feedbackLabel, "FEEDBACK");
    configureKnob (loopCutoffSlider, loopCutoffLabel, "LOOP LPF");
    configureKnob (spreadSlider, spreadLabel, "BBD SPREAD");

    configureToggleButton (polarityButton);
    configureToggleButton (loopModeButton);

    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "mode", modeSelector);
    preCutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "preCutoff", preCutoffSlider);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "drive", driveSlider);
    characterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "character", characterSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "mix", mixSlider);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "output", outputSlider);

    pitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "pitch", pitchSlider);
    feedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "feedback", feedbackSlider);
    loopCutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "loopCutoff", loopCutoffSlider);
    spreadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "spread", spreadSlider);
    polarityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "polarity", polarityButton);
    loopModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "inLoop", loopModeButton);

    // Format Pitch knob as "110 Hz (A2)" for musical resonator tuning
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
    updateButtonLabels();
    setSize (760, 560);
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
    button.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffc0c8d8));
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    addAndMakeVisible (button);
}

void RunawayAudioProcessorEditor::updateButtonLabels()
{
    polarityButton.setButtonText (polarityButton.getToggleState() ? "POLARITY: - HOLLOW" : "POLARITY: + STRING");
    loopModeButton.setButtonText (loopModeButton.getToggleState() ? "LOOP: IN-SHAPER" : "LOOP: PRE-SHAPER");
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
    updateButtonLabels();
    audioProcessor.readScopeData (dryScope, wetScope);
    repaint (scopeBounds);
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
    g.drawText ("RUNAWAY", 20, 10, 160, 26, juce::Justification::centredLeft);

    g.setColour (lookAndFeel.getAccentColour());
    g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    g.drawText ("STAGE 2  •  4x WAVESHAPER + BBD RESONATOR LOOP", 20, 34, 340, 14, juce::Justification::centredLeft);

    // Oscilloscope panel
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

    // Subtle resonator section container
    const auto rf = row2Bounds.toFloat();
    g.setColour (juce::Colour (0xff13161d));
    g.fillRoundedRectangle (rf, 8.0f);
    g.setColour (juce::Colour (0xff232833));
    g.drawRoundedRectangle (rf, 8.0f, 1.0f);

    g.setColour (juce::Colour (0xff6b7488));
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText ("BBD RESONATOR LOOP  (ECHON 6  /  SOMA LYRA  /  SEALEGS)",
                row2Bounds.getX() + 12, row2Bounds.getY() + 6, row2Bounds.getWidth() - 24, 14,
                juce::Justification::left);

    g.setColour (juce::Colour (0xff596275));
    g.setFont (juce::Font (juce::FontOptions (10.5f)));
    g.drawText ("NEXT STAGE READY:  Razornator Chord Mode  +  NightSky Diffuse FDN  +  MIDI Pitch Track",
                20, getHeight() - 22, getWidth() - 40, 16, juce::Justification::centred);
}

void RunawayAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (56);
    modeSelector.setBounds (header.removeFromRight (380).reduced (16, 12));

    area.reduce (20, 12);
    area.removeFromBottom (20);

    scopeBounds = area.removeFromTop (128);
    area.removeFromTop (12);

    // Row 1: 5 knobs (Shaper + Master)
    auto row1 = area.removeFromTop (140);
    const int colW = area.getWidth() / 5;

    auto placeKnob = [&] (const juce::Rectangle<int>& row, int index, juce::Slider& s, juce::Label& l)
    {
        auto slot = row.withX (row.getX() + index * colW).withWidth (colW);
        l.setBounds (slot.removeFromTop (18));
        s.setBounds (slot.reduced (4, 0));
    };

    placeKnob (row1, 0, preCutoffSlider, preCutoffLabel);
    placeKnob (row1, 1, driveSlider, driveLabel);
    placeKnob (row1, 2, characterSlider, characterLabel);
    placeKnob (row1, 3, mixSlider, mixLabel);
    placeKnob (row1, 4, outputSlider, outputLabel);

    area.removeFromTop (10);

    // Row 2: BBD Resonator panel (4 knobs + 2 hardware switches in column 5)
    row2Bounds = area;
    auto innerRow2 = row2Bounds.reduced (6, 8);
    innerRow2.removeFromTop (18); // section label space

    const int col2W = innerRow2.getWidth() / 5;
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

    auto toggleCol = innerRow2.withX (innerRow2.getX() + 4 * col2W).withWidth (col2W).reduced (6, 12);
    polarityButton.setBounds (toggleCol.removeFromTop (40));
    toggleCol.removeFromTop (10);
    loopModeButton.setBounds (toggleCol.removeFromTop (40));
}
