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

    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "mode", modeSelector);
    preCutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "preCutoff", preCutoffSlider);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "drive", driveSlider);
    characterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "character", characterSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "mix", mixSlider);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "output", outputSlider);

    updateModeAesthetics();
    setSize (740, 440);
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
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 18);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffb6becd));
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff14171d));
    addAndMakeVisible (slider);

    title.setText (text, juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    title.setColour (juce::Label::textColourId, juce::Colour (0xff8a93a6));
    addAndMakeVisible (title);
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
    g.drawText ("STAGE 1  •  4x OVERSAMPLED WAVESHAPER CORE", 20, 34, 300, 14, juce::Justification::centredLeft);

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
    g.drawText ("FOLDED OUT", scopeBounds.getX() + 68, scopeBounds.getY() + 8, 90, 14, juce::Justification::left);

    g.setColour (juce::Colour (0xff596275));
    g.setFont (juce::Font (juce::FontOptions (10.5f)));
    g.drawText ("NEXT STAGE READY:  BBD Resonator (Morphor Echon 6)  +  SOMA Loop Comp  +  NightSky Diffuse FDN",
                20, getHeight() - 24, getWidth() - 40, 16, juce::Justification::centred);
}

void RunawayAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (56);
    modeSelector.setBounds (header.removeFromRight (380).reduced (16, 12));

    area.reduce (20, 14);
    area.removeFromBottom (22);

    scopeBounds = area.removeFromTop (155);
    area.removeFromTop (14);

    const int knobCount = 5;
    const int slotW = area.getWidth() / knobCount;

    auto placeKnob = [&] (int index, juce::Slider& s, juce::Label& l)
    {
        auto slot = area.withX (area.getX() + index * slotW).withWidth (slotW);
        l.setBounds (slot.removeFromTop (20));
        s.setBounds (slot.reduced (4, 0));
    };

    placeKnob (0, preCutoffSlider, preCutoffLabel);
    placeKnob (1, driveSlider, driveLabel);
    placeKnob (2, characterSlider, characterLabel);
    placeKnob (3, mixSlider, mixLabel);
    placeKnob (4, outputSlider, outputLabel);
}
