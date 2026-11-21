#include "PluginProcessor.h"
#include "PluginEditor.h"

RunawayAudioProcessor::RunawayAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RUNAWAY_PARAMS", createParameterLayout())
{
    modeParam = apvts.getRawParameterValue ("mode");
    preCutoffParam = apvts.getRawParameterValue ("preCutoff");
    driveParam = apvts.getRawParameterValue ("drive");
    characterParam = apvts.getRawParameterValue ("character");
    pitchParam = apvts.getRawParameterValue ("pitch");
    feedbackParam = apvts.getRawParameterValue ("feedback");
    loopCutoffParam = apvts.getRawParameterValue ("loopCutoff");
    spreadParam = apvts.getRawParameterValue ("spread");
    polarityParam = apvts.getRawParameterValue ("polarity");
    inLoopParam = apvts.getRawParameterValue ("inLoop");
    mixParam = apvts.getRawParameterValue ("mix");
    outputParam = apvts.getRawParameterValue ("output");
}

juce::AudioProcessorValueTreeState::ParameterLayout RunawayAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "mode", 1 },
        "Topology",
        juce::StringArray {
            "BUCHLA / SOMA",
            "BIFOLD (Buchla-Serge)",
            "ERICA / XENO",
            "112dB / CAT"
        },
        0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "preCutoff", 1 },
        "Pre-LPF",
        juce::NormalisableRange<float> (2000.0f, 20000.0f, 1.0f, 0.4f),
        14000.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "drive", 1 },
        "Drive",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f, 0.75f),
        35.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "character", 1 },
        "Character",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f, 1.0f),
        25.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "pitch", 1 },
        "Resonator Pitch",
        juce::NormalisableRange<float> (35.0f, 1200.0f, 0.1f, 0.35f),
        110.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "feedback", 1 },
        "Feedback",
        juce::NormalisableRange<float> (0.0f, 98.0f, 0.1f, 0.85f),
        35.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "loopCutoff", 1 },
        "Loop LPF",
        juce::NormalisableRange<float> (200.0f, 16000.0f, 1.0f, 0.4f),
        4500.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "spread", 1 },
        "Stereo Spread",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f, 1.0f),
        25.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "polarity", 1 },
        "Feedback Polarity",
        false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "inLoop", 1 },
        "Shaper In-Loop",
        false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 },
        "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f, 1.0f),
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output", 1 },
        "Output",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return { params.begin(), params.end() };
}

const juce::String RunawayAudioProcessor::getName() const { return JucePlugin_Name; }
bool RunawayAudioProcessor::acceptsMidi() const { return false; }
bool RunawayAudioProcessor::producesMidi() const { return false; }
bool RunawayAudioProcessor::isMidiEffect() const { return false; }
double RunawayAudioProcessor::getTailLengthSeconds() const { return 1.5; }
int RunawayAudioProcessor::getNumPrograms() { return 1; }
int RunawayAudioProcessor::getCurrentProgram() { return 0; }
void RunawayAudioProcessor::setCurrentProgram (int) {}
const juce::String RunawayAudioProcessor::getProgramName (int) { return {}; }
void RunawayAudioProcessor::changeProgramName (int, const juce::String&) {}

void RunawayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numCh = juce::jlimit (1, 2, getTotalNumInputChannels());
    const int safeBlockSize = std::max (64, samplesPerBlock);

    waveshaperEngine.prepare (sampleRate, safeBlockSize, numCh);
    setLatencySamples (waveshaperEngine.getLatencySamples());

    scopeDryScratch.setSize (1, safeBlockSize, false, true, false);

    for (int i = 0; i < scopeBufferSize; ++i)
    {
        scopeDryRing[static_cast<size_t> (i)].store (0.0f, std::memory_order_relaxed);
        scopeWetRing[static_cast<size_t> (i)].store (0.0f, std::memory_order_relaxed);
    }
    scopeWritePos.store (0, std::memory_order_relaxed);
}

void RunawayAudioProcessor::releaseResources()
{
    waveshaperEngine.reset();
}

bool RunawayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return mainIn == mainOut;
}

void RunawayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    if (numSamples <= 0 || totalNumInputChannels <= 0)
        return;

    const int modeIdx = static_cast<int> (std::lround (modeParam->load()));
    const float preCutoffHz = preCutoffParam->load();
    const float driveNorm = driveParam->load() * 0.01f;
    const float charNorm = characterParam->load() * 0.01f;
    const float pitchHz = pitchParam->load();
    const float feedbackNorm = feedbackParam->load() * 0.01f;
    const float loopCutoffHz = loopCutoffParam->load();
    const float spreadNorm = spreadParam->load() * 0.01f;
    const bool invertPolarity = (polarityParam->load() >= 0.5f);
    const bool shaperInLoop = (inLoopParam->load() >= 0.5f);
    const float mixNorm = mixParam->load() * 0.01f;
    const float outDb = outputParam->load();

    waveshaperEngine.setParameters (modeIdx,
                                    preCutoffHz,
                                    driveNorm,
                                    charNorm,
                                    pitchHz,
                                    feedbackNorm,
                                    loopCutoffHz,
                                    spreadNorm,
                                    invertPolarity,
                                    shaperInLoop,
                                    mixNorm,
                                    outDb);

    const int captureSamples = std::min (numSamples, scopeDryScratch.getNumSamples());
    float* dryCapture = scopeDryScratch.getWritePointer (0);
    const float* inL = buffer.getReadPointer (0);
    const float* inR = (totalNumInputChannels > 1) ? buffer.getReadPointer (1) : inL;

    for (int i = 0; i < captureSamples; ++i)
        dryCapture[i] = 0.5f * (inL[i] + inR[i]);

    waveshaperEngine.process (buffer);

    const float* outL = buffer.getReadPointer (0);
    const float* outR = (totalNumOutputChannels > 1) ? buffer.getReadPointer (1) : outL;
    int pos = scopeWritePos.load (std::memory_order_relaxed);

    for (int i = 0; i < captureSamples; ++i)
    {
        const float wetMono = 0.5f * (outL[i] + outR[i]);
        scopeDryRing[static_cast<size_t> (pos)].store (dryCapture[i], std::memory_order_relaxed);
        scopeWetRing[static_cast<size_t> (pos)].store (wetMono, std::memory_order_relaxed);
        pos = (pos + 1) % scopeBufferSize;
    }

    scopeWritePos.store (pos, std::memory_order_release);
}

void RunawayAudioProcessor::readScopeData (std::array<float, scopeBufferSize>& dryOut,
                                           std::array<float, scopeBufferSize>& wetOut) const noexcept
{
    const int head = scopeWritePos.load (std::memory_order_acquire);

    for (int i = 0; i < scopeBufferSize; ++i)
    {
        const int idx = (head + i) % scopeBufferSize;
        dryOut[static_cast<size_t> (i)] = scopeDryRing[static_cast<size_t> (idx)].load (std::memory_order_relaxed);
        wetOut[static_cast<size_t> (i)] = scopeWetRing[static_cast<size_t> (idx)].load (std::memory_order_relaxed);
    }
}

bool RunawayAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* RunawayAudioProcessor::createEditor()
{
    return new RunawayAudioProcessorEditor (*this);
}

void RunawayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void RunawayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RunawayAudioProcessor();
}
