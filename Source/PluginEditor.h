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
    void configureKnob (juce::Slider& slider, juce::Label& title, const juce::String& text);

    RunawayAudioProcessor& audioProcessor;
    RunawayLookAndFeel lookAndFeel;

    juce::ComboBox modeSelector;

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

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> preCutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> characterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;

    juce::Rectangle<int> scopeBounds;
    std::array<float, RunawayAudioProcessor::scopeBufferSize> dryScope {};
    std::array<float, RunawayAudioProcessor::scopeBufferSize> wetScope {};
    int lastModeIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RunawayAudioProcessorEditor)
};
