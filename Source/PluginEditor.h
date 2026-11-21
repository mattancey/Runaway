// Fichier: Source/PluginProcessor.h

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/WaveshaperEngine.h"
#include <array>
#include <atomic>

class RunawayAudioProcessor final : public juce::AudioProcessor,
                                    private juce::AudioProcessorValueTreeState::Listener,
                                    private juce::AsyncUpdater
{
public:
    static constexpr int scopeBufferSize = 512;
    static constexpr int numModes = 4;
    static constexpr int numPerModeParams = 13;

    RunawayAudioProcessor();
    ~RunawayAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    void readScopeData (std::array<float, scopeBufferSize>& dryOut,
                        std::array<float, scopeBufferSize>& wetOut) const noexcept;

    float getActiveMidiFreqHz() const noexcept
    {
        return hasMidiNote.load (std::memory_order_relaxed)
            ? midiNoteFreqHz.load (std::memory_order_relaxed)
            : 0.0f;
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static const std::array<const char*, numPerModeParams>& getPerModeParamIds() noexcept;

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void applyModeSwitch (int nextMode);
    void initDefaultModePresets();

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* preCutoffParam = nullptr;
    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* characterParam = nullptr;
    std::atomic<float>* loopEngineParam = nullptr;
    std::atomic<float>* loopActiveParam = nullptr;
    std::atomic<float>* loopLevelParam = nullptr;
    std::atomic<float>* pitchParam = nullptr;
    std::atomic<float>* feedbackParam = nullptr;
    std::atomic<float>* loopCutoffParam = nullptr;
    std::atomic<float>* spreadParam = nullptr;
    std::atomic<float>* polarityParam = nullptr;
    std::atomic<float>* inLoopParam = nullptr;
    std::atomic<float>* midiTrackParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputParam = nullptr;

    std::array<std::array<std::atomic<float>, numPerModeParams>, numModes> modePresets {};
    std::atomic<int> activeModeIndex { 0 };
    std::atomic<int> pendingModeIndex { 0 };
    std::atomic<bool> isRecallingMode { false };

    std::atomic<float> midiNoteFreqHz { 110.0f };
    std::atomic<bool> hasMidiNote { false };

    runaway::dsp::WaveshaperEngine waveshaperEngine;

    juce::AudioBuffer<float> scopeDryScratch;
    std::array<std::atomic<float>, scopeBufferSize> scopeDryRing {};
    std::array<std::atomic<float>, scopeBufferSize> scopeWetRing {};
    std::atomic<int> scopeWritePos { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RunawayAudioProcessor)
};
