#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <algorithm>
#include <memory>

namespace runaway::dsp
{
class WaveshaperEngine
{
public:
    enum class Mode : int
    {
        BuchlaSoma = 0, // Buchla 259 parallel folder + SOMA Harvezi Hazze Spoil/Spread
        BifoldMorph,    // Intellijel Bifold: Buchla parallel <-> Serge serial morph
        EricaXeno,      // Erica Synths x 112dB Xenodrive: ADAA1 tanh + Germanium bias + Scream EQ
        Werner112dB     // 112dB / Octave-CAT nonlinear TPT SVF
    };

    static constexpr int maxChannels = 2;
    static constexpr int maxSubBlock = 64;
    static constexpr int dryDelayBufferSize = 512;

    void prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        hostSampleRate = std::max (1000.0, sampleRate);
        activeChannels = juce::jlimit (1, maxChannels, numChannels);

        // 4x oversampling (2^2 = 4) using linear-phase equiripple FIR with integer latency
        // so Dry and Wet paths remain 100% phase-coherent at every Mix setting.
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            static_cast<size_t> (activeChannels),
            2,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
            true,
            true);

        oversampler->initProcessing (static_cast<size_t> (std::max (1, maxBlockSize)));
        oversampledRate = hostSampleRate * static_cast<double> (oversampler->getOversamplingFactor());

        constexpr int maxSafeLatency = dryDelayBufferSize - maxSubBlock - 1;
        latencySamples = juce::jlimit (0, maxSafeLatency,
                                       juce::roundToInt (oversampler->getLatencyInSamples()));

        juce::dsp::ProcessSpec hostSpec;
        hostSpec.sampleRate = hostSampleRate;
        hostSpec.maximumBlockSize = static_cast<juce::uint32> (std::max (1, maxBlockSize));
        hostSpec.numChannels = static_cast<juce::uint32> (activeChannels);

        preFilter.prepare (hostSpec);
        preFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        preFilter.setResonance (0.70710678f);

        const double smoothSec = 0.020;
        preCutoffSmooth.reset (hostSampleRate, smoothSec);
        driveSmooth.reset (hostSampleRate, smoothSec);
        characterSmooth.reset (hostSampleRate, smoothSec);
        mixSmooth.reset (hostSampleRate, smoothSec);
        outputGainSmooth.reset (hostSampleRate, smoothSec);

        reset();
    }

    void reset()
    {
        if (oversampler != nullptr)
            oversampler->reset();

        preFilter.reset();

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            adaaPrevX[ch] = 0.0f;
            dcPrevX[ch] = 0.0f;
            dcPrevY[ch] = 0.0f;
            svfIc1[ch] = 0.0f;
            svfIc2[ch] = 0.0f;
            screamIc1[ch] = 0.0f;
            screamIc2[ch] = 0.0f;
            dryRing[ch].fill (0.0f);
        }

        dryWritePos = 0;
    }

    int getLatencySamples() const noexcept { return latencySamples; }

    void setParameters (int modeIndex,
                        float preCutoffHz,
                        float driveNorm,
                        float characterNorm,
                        float mixNorm,
                        float outputDb) noexcept
    {
        currentMode = static_cast<Mode> (juce::jlimit (0, 3, modeIndex));
        preCutoffSmooth.setTargetValue (juce::jlimit (1800.0f, 20000.0f, preCutoffHz));
        driveSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, driveNorm));
        characterSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, characterNorm));
        mixSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, mixNorm));
        outputGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 12.0f, outputDb)));
    }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        if (oversampler == nullptr || buffer.getNumSamples() <= 0)
            return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = std::min (activeChannels, buffer.getNumChannels());
        if (numChannels <= 0)
            return;

        int sampleOffset = 0;

        while (sampleOffset < numSamples)
        {
            const int subLen = std::min (maxSubBlock, numSamples - sampleOffset);

            std::array<float, maxSubBlock> driveSub {};
            std::array<float, maxSubBlock> charSub {};
            std::array<float, maxSubBlock> mixSub {};
            std::array<float, maxSubBlock> outGainSub {};

            for (int i = 0; i < subLen; ++i)
            {
                const float cutoff = preCutoffSmooth.getNextValue();
                preFilter.setCutoffFrequency (cutoff);

                driveSub[static_cast<size_t> (i)] = driveSmooth.getNextValue();
                charSub[static_cast<size_t> (i)] = characterSmooth.getNextValue();
                mixSub[static_cast<size_t> (i)] = mixSmooth.getNextValue();
                outGainSub[static_cast<size_t> (i)] = outputGainSmooth.getNextValue();

                const int sIdx = sampleOffset + i;

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const float inSample = buffer.getSample (ch, sIdx);
                    dryRing[static_cast<size_t> (ch)][static_cast<size_t> (dryWritePos)] = inSample;

                    const float preFiltered = preFilter.processSample (ch, inSample);
                    buffer.setSample (ch, sIdx, preFiltered);
                }

                dryWritePos = (dryWritePos + 1) % dryDelayBufferSize;
            }

            juce::dsp::AudioBlock<float> fullBlock (buffer);
            auto subBlock = fullBlock.getSubsetChannelBlock (0, static_cast<size_t> (numChannels))
                                     .getSubBlock (static_cast<size_t> (sampleOffset),
                                                   static_cast<size_t> (subLen));
            auto osBlock = oversampler->processSamplesUp (subBlock);

            const int osNumSamples = static_cast<int> (osBlock.getNumSamples());
            const int osFactor = std::max (1, osNumSamples / subLen);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* chData = osBlock.getChannelPointer (static_cast<size_t> (ch));

                for (int osIdx = 0; osIdx < osNumSamples; ++osIdx)
                {
                    const int hostIdx = std::min (subLen - 1, osIdx / osFactor);
                    const float drv = driveSub[static_cast<size_t> (hostIdx)];
                    const float chr = charSub[static_cast<size_t> (hostIdx)];

                    const float x = chData[osIdx];
                    float y = x;

                    switch (currentMode)
                    {
                        case Mode::BuchlaSoma:
                            y = processBuchlaSoma (ch, x, drv, chr);
                            break;

                        case Mode::BifoldMorph:
                            y = processBifoldMorph (ch, x, drv, chr);
                            break;

                        case Mode::EricaXeno:
                            y = processEricaXeno (ch, x, drv, chr);
                            break;

                        case Mode::Werner112dB:
                            y = processWerner112dB (ch, x, drv, chr);
                            break;
                    }

                    // Gentle 1-pole DC blocker at oversampled rate
                    const float dcR = 0.9992f;
                    const float dcY = y - dcPrevX[ch] + dcR * dcPrevY[ch];
                    dcPrevX[ch] = y;
                    dcPrevY[ch] = dcY;

                    chData[osIdx] = dcY;
                }
            }

            oversampler->processSamplesDown (subBlock);

            for (int i = 0; i < subLen; ++i)
            {
                const int sIdx = sampleOffset + i;
                const float mix = mixSub[static_cast<size_t> (i)];
                const float gain = outGainSub[static_cast<size_t> (i)];

                const int readIdx = (dryWritePos - subLen + i - latencySamples + dryDelayBufferSize * 2)
                                    % dryDelayBufferSize;

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const float dry = dryRing[static_cast<size_t> (ch)][static_cast<size_t> (readIdx)];
                    const float wet = buffer.getSample (ch, sIdx);
                    float out = (dry * (1.0f - mix) + wet * mix) * gain;

                    if (std::abs (out) > 1.0f)
                        out = std::tanh (out * 0.85f) / 0.85f;

                    buffer.setSample (ch, sIdx, out);
                }
            }

            sampleOffset += subLen;
        }
    }

private:
    static float tanhF1 (float x) noexcept
    {
        const float ax = std::abs (x);
        return ax + std::log1p (std::exp (-2.0f * ax)) - 0.69314718056f;
    }

    float processADAATanh (int ch, float x) noexcept
    {
        const float xPrev = adaaPrevX[ch];
        adaaPrevX[ch] = x;

        const float diff = x - xPrev;
        if (std::abs (diff) < 1.0e-4f)
            return std::tanh (0.5f * (x + xPrev));

        return (tanhF1 (x) - tanhF1 (xPrev)) / diff;
    }

    static float buchla259ParallelFold (float x) noexcept
    {
        static constexpr std::array<float, 5> thresholds { 0.35f, 0.72f, 1.12f, 1.58f, 2.15f };
        static constexpr std::array<float, 5> weights    { 0.90f, -0.78f, 0.68f, -0.58f, 0.48f };

        float sum = std::tanh (x * 0.95f);

        for (size_t i = 0; i < thresholds.size(); ++i)
        {
            const float t = thresholds[i];
            const float ax = std::abs (x);
            if (ax > t)
            {
                const float excess = ax - t;
                const float fold = std::copysign (excess * std::tanh (excess * 1.8f), x);
                sum += weights[i] * fold;
            }
        }

        return std::tanh (sum * 0.65f);
    }

    static float sergeSerialFold (float x, float stagesDrive) noexcept
    {
        const float g = 1.0f + stagesDrive * 4.5f;
        const float s1 = std::sin (x * g);
        const float s2 = std::sin (s1 * 2.35f + x * 0.45f);
        return std::tanh (0.72f * s2 + 0.28f * s1);
    }

    float processBuchlaSoma (int ch, float x, float drive, float spoil) noexcept
    {
        const float thr = spoil * 0.28f;
        const float mag = std::abs (x);
        const float gate = (thr > 1.0e-5f) ? (mag * mag) / (mag * mag + thr * thr) : 1.0f;

        const float preGain = 1.0f + drive * 8.5f;
        const float bias = spoil * 0.32f;
        const float driven = (x * gate) * preGain + bias;

        const float folded = buchla259ParallelFold (driven);
        return 0.85f * folded + 0.15f * processADAATanh (ch, folded * 1.4f);
    }

    float processBifoldMorph (int ch, float x, float drive, float morph) noexcept
    {
        juce::ignoreUnused (ch);
        const float preGain = 1.0f + drive * 7.5f;
        const float driven = x * preGain;

        const float buchlaOut = buchla259ParallelFold (driven);
        const float sergeOut = sergeSerialFold (x, drive);

        const float gB = std::cos (morph * juce::MathConstants<float>::halfPi);
        const float gS = std::sin (morph * juce::MathConstants<float>::halfPi);

        return (gB * buchlaOut + gS * sergeOut) * 0.95f;
    }

    float processEricaXeno (int ch, float x, float drive, float scream) noexcept
    {
        const float gain = 1.0f + drive * 15.0f;
        const float germBias = 0.08f + scream * 0.35f;
        const float driven = x * gain + germBias;

        const float clipped = processADAATanh (ch, driven);

        const float fc = 650.0f + scream * 2400.0f;
        const float g = std::tan (juce::MathConstants<float>::pi * std::min (fc, 0.35f * static_cast<float> (oversampledRate))
                                  / static_cast<float> (oversampledRate));
        const float k = 1.0f / (1.0f + scream * 3.8f);
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        const float sV3 = clipped - screamIc2[ch];
        const float sV1 = a1 * screamIc1[ch] + a2 * sV3;
        const float sV2 = screamIc2[ch] + a2 * screamIc1[ch] + a3 * sV3;
        screamIc1[ch] = 2.0f * sV1 - screamIc1[ch];
        screamIc2[ch] = 2.0f * sV2 - screamIc2[ch];

        const float band = k * sV1;
        return std::tanh ((clipped + band * (0.3f + scream * 1.4f)) * 0.85f);
    }

    float processWerner112dB (int ch, float x, float drive, float resonanceNorm) noexcept
    {
        const float inputGain = 1.0f + drive * 6.5f;
        const float inDriven = x * inputGain;

        const float fc = 280.0f + (1.0f - drive * 0.35f) * 3200.0f + resonanceNorm * 2200.0f;
        const float clampedFc = juce::jlimit (60.0f, 0.38f * static_cast<float> (oversampledRate), fc);
        const float g = std::tan (juce::MathConstants<float>::pi * clampedFc / static_cast<float> (oversampledRate));

        const float k = juce::jlimit (0.14f, 1.4f, 1.4f - resonanceNorm * 1.26f);

        const float denom = 1.0f + g * (g + k);
        const float hp = (inDriven - (k + g) * svfIc1[ch] - svfIc2[ch]) / denom;

        const float v1 = std::tanh (g * hp);
        const float bp = std::tanh (svfIc1[ch] + v1);
        svfIc1[ch] = std::tanh (bp + v1);

        const float v2 = std::tanh (g * bp);
        const float lp = std::tanh (svfIc2[ch] + v2);
        svfIc2[ch] = std::tanh (lp + v2);

        return (lp * 0.88f + bp * (0.12f + resonanceNorm * 0.2f));
    }

    double hostSampleRate = 44100.0;
    double oversampledRate = 176400.0;
    int activeChannels = 2;
    int latencySamples = 0;

    Mode currentMode = Mode::BuchlaSoma;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::dsp::StateVariableTPTFilter<float> preFilter;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> preCutoffSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> characterSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGainSmooth;

    std::array<float, maxChannels> adaaPrevX {};
    std::array<float, maxChannels> dcPrevX {};
    std::array<float, maxChannels> dcPrevY {};
    std::array<float, maxChannels> svfIc1 {};
    std::array<float, maxChannels> svfIc2 {};
    std::array<float, maxChannels> screamIc1 {};
    std::array<float, maxChannels> screamIc2 {};

    std::array<std::array<float, dryDelayBufferSize>, maxChannels> dryRing {};
    int dryWritePos = 0;
};
} // namespace runaway::dsp
