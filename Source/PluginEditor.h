#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include <array>
#include <memory>

class RunawayLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void setAccentColour (juce::Colour c) noexcept { accent = c; }
    juce::Colour getAccentColour() const noexcept { return accent; }

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override;

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

private:
    juce::Colour accent { 0xffff7a2f };
};

class RunawayAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit RunawayAudioProcessorEditor (RunawayAudioProcessor& processor);
    ~RunawayAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateModeAesthetics();
    void updateLoopEngineLabels();
    void updateButtonLabels();
    void configureKnob (juce::Slider& slider, juce::Label& title, const juce::String& text);
    void configureToggleButton (juce::TextButton& button);

    RunawayAudioProcessor& audioProcessor;
    RunawayLookAndFeel lookAndFeel;

    juce::ComboBox modeSelector;
    juce::ComboBox loopEngineSelector;

    // Row 1: Waveshaper + Master
    juce::Slider preCutoffSlider;
    juce::Slider driveSlider;
    juce::Slider characterSlider;
    juce::Slider mixSlider;
    juce::Slider outputSlider;

    juce::Label preCutoffLabel;
    juce::Label driveLabel;
    juce::Label characterLabel;
    juce::Label mixLabel;
    juce::Label outputLabel;

    // Row 2: Loop Resonator / Chord / Diffuse + Dedicated Loop Level + Toggles
    juce::Slider pitchSlider;
    juce::Slider feedbackSlider;
    juce::Slider loopCutoffSlider;
    juce::Slider spreadSlider;
    juce::Slider loopLevelSlider;

    juce::Label pitchLabel;
    juce::Label feedbackLabel;
    juce::Label loopCutoffLabel;
    juce::Label spreadLabel;
    juce::Label loopLevelLabel;

    juce::TextButton loopActiveButton;
    juce::TextButton midiTrackButton;
    juce::TextButton polarityButton;
    juce::TextButton loopModeButton;

    // Declare attachments AFTER UI components so they detach safely before destruction
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> loopEngineAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> preCutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> characterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> loopCutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spreadAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> loopLevelAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopActiveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiTrackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> polarityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopModeAttachment;

    juce::Rectangle<int> scopeBounds;
    juce::Rectangle<int> row2Bounds;
    std::array<float, RunawayAudioProcessor::scopeBufferSize> dryScope {};
    std::array<float, RunawayAudioProcessor::scopeBufferSize> wetScope {};
    int lastModeIndex = -1;
    int lastLoopEngineIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RunawayAudioProcessorEditor)
};
