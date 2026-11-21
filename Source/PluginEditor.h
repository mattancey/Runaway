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
    void mouseDown (const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    void updateModeAesthetics();
    void updateLoopEngineLabels();
    void updateButtonLabels();
    void refreshPresetMenu (const juce::String& selectName = {});
    void triggerSavePreset();
    void configureKnob (juce::Slider& slider, juce::Label& title, const juce::String& text);
    void configureToggleButton (juce::TextButton& button);
    void configureActionButton (juce::TextButton& button);

    RunawayAudioProcessor& audioProcessor;
    RunawayLookAndFeel lookAndFeel;

    juce::TextButton abSlotButton;
    juce::TextButton abCopyButton;
    juce::ComboBox presetSelector;
    juce::TextButton savePresetButton;
    juce::ComboBox modeSelector;

    juce::ComboBox loopEngineSelector;
    juce::ComboBox envTargetSelector;
    juce::ComboBox lpgModeSelector;

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

    juce::Slider envAmountSlider;
    juce::Slider envReleaseSlider;
    juce::Label envAmountLabel;
    juce::Label envReleaseLabel;

    juce::Slider lpgDecaySlider;
    juce::Slider lpgResSlider;
    juce::Label lpgDecayLabel;
    juce::Label lpgResLabel;

    // Attachments declared AFTER UI components so they detach safely before destruction
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> loopEngineAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> envTargetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lpgModeAttachment;

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

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> envAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> envReleaseAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lpgDecayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lpgResAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopActiveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiTrackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> polarityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopModeAttachment;

    juce::Array<juce::File> userPresetFiles;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::Rectangle<int> scopeBounds;
    juce::Rectangle<int> row2Bounds;
    juce::Rectangle<int> row3LeftBounds;
    juce::Rectangle<int> row3RightBounds;
    juce::Rectangle<int> envMeterBounds;
    juce::Rectangle<int> lpgMeterBounds;

    std::array<float, RunawayAudioProcessor::scopeBufferSize> dryScope {};
    std::array<float, RunawayAudioProcessor::scopeBufferSize> wetScope {};
    bool showTransferCurve = false;
    float envMeterSmooth = 0.0f;
    float lpgMeterSmooth = 0.0f;
    int lastModeIndex = -1;
    int lastLoopEngineIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RunawayAudioProcessorEditor)
};
