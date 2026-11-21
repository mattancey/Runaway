#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
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

        // Pre-allocate BBD resonator delay lines at 4x oversampled rate (down to 25 Hz + spread headroom)
        delayLineLength = static_cast<int> (std::ceil (oversampledRate / 25.0)) + 64;
        for (int ch = 0; ch < maxChannels; ++ch)
            delayBuffers[static_cast<size_t> (ch)].assign (static_cast<size_t> (delayLineLength), 0.0f);

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

        pitchHzSmooth.reset (hostSampleRate, 0.025);
        feedbackSmooth.reset (hostSampleRate, smoothSec);
        loopCutoffSmooth.reset (hostSampleRate, smoothSec);
        spreadSmooth.reset (hostSampleRate, smoothSec);
        polaritySmooth.reset (hostSampleRate, 0.008);

        reset();
    }

    void reset()
    {
        if (oversampler != nullptr)
            oversampler->reset();

        preFilter.reset();

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            const size_t c = static_cast<size_t> (ch);
            adaaPrevX[c] = 0.0f;
            dcPrevX[c] = 0.0f;
            dcPrevY[c] = 0.0f;
            svfIc1[c] = 0.0f;
            svfIc2[c] = 0.0f;
            screamIc1[c] = 0.0f;
            screamIc2[c] = 0.0f;
            loopLp12Ic1[c] = 0.0f;
            loopLp12Ic2[c] = 0.0f;
            loopLp6State[c] = 0.0f;
            loopHpX1[c] = 0.0f;
            loopHpY1[c] = 0.0f;
            feedbackTap[c] = 0.0f;
            delayWritePos[c] = 0;
            dryRing[c].fill (0.0f);
            std::fill (delayBuffers[c].begin(), delayBuffers[c].end(), 0.0f);
        }

        lfoPhase[0] = 0.0f;
        lfoPhase[1] = 0.37f;
        dryWritePos = 0;
    }

    int getLatencySamples() const noexcept { return latencySamples; }

    void setParameters (int modeIndex,
                        float preCutoffHz,
                        float driveNorm,
                        float characterNorm,
                        float pitchHz,
                        float feedbackNorm,
                        float loopCutoffHz,
                        float spreadNorm,
                        bool invertPolarity,
                        bool shaperInLoop,
                        float mixNorm,
                        float outputDb) noexcept
    {
        currentMode = static_cast<Mode> (juce::jlimit (0, 3, modeIndex));
        inLoopRouting = shaperInLoop;

        preCutoffSmooth.setTargetValue (juce::jlimit (1800.0f, 20000.0f, preCutoffHz));
        driveSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, driveNorm));
        characterSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, characterNorm));
        pitchHzSmooth.setTargetValue (juce::jlimit (35.0f, 1200.0f, pitchHz));
        feedbackSmooth.setTargetValue (juce::jlimit (0.0f, 0.98f, feedbackNorm));
        loopCutoffSmooth.setTargetValue (juce::jlimit (200.0f, 16000.0f, loopCutoffHz));
        spreadSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, spreadNorm));
        polaritySmooth.setTargetValue (invertPolarity ? -1.0f : 1.0f);
        mixSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, mixNorm));
        outputGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 12.0f, outputDb)));
    }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        if (oversampler == nullptr || buffer.getNumSamples() <= 0 || delayLineLength <= 8)
            return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = std::min (activeChannels, buffer.getNumChannels());
        if (numChannels <= 0)
            return;

        const float osRateF = static_cast<float> (oversampledRate);
        const float hpR = juce::jlimit (0.98f, 0.99995f,
                                        1.0f - (2.0f * juce::MathConstants<float>::pi * 18.0f / osRateF));
        const float lfoInc0 = 0.41f / static_cast<float> (hostSampleRate);
        const float lfoInc1 = 0.57f / static_cast<float> (hostSampleRate);

        int sampleOffset = 0;

        while (sampleOffset < numSamples)
        {
            const int subLen = std::min (maxSubBlock, numSamples - sampleOffset);

            std::array<float, maxSubBlock> driveSub {};
            std::array<float, maxSubBlock> charSub {};
            std::array<float, maxSubBlock> delayLSub {};
            std::array<float, maxSubBlock> delayRSub {};
            std::array<float, maxSubBlock> fbSignedSub {};
            std::array<float, maxSubBlock> loopGSub {};
            std::array<float, maxSubBlock> loopA1Sub {};
            std::array<float, maxSubBlock> loopA2Sub {};
            std::array<float, maxSubBlock> loopA3Sub {};
            std::array<float, maxSubBlock> lp6AlphaSub {};
            std::array<float, maxSubBlock> mixSub {};
            std::array<float, maxSubBlock> outGainSub {};

            for (int i = 0; i < subLen; ++i)
            {
                const size_t si = static_cast<size_t> (i);
                const float cutoff = preCutoffSmooth.getNextValue();
                preFilter.setCutoffFrequency (cutoff);

                driveSub[si] = driveSmooth.getNextValue();
                charSub[si] = characterSmooth.getNextValue();
                mixSub[si] = mixSmooth.getNextValue();
                outGainSub[si] = outputGainSmooth.getNextValue();

                const float pitchHz = pitchHzSmooth.getNextValue();
                const float fb = feedbackSmooth.getNextValue();
                const float pol = polaritySmooth.getNextValue();
                const float loopFc = loopCutoffSmooth.getNextValue();
                const float spr = spreadSmooth.getNextValue();

                fbSignedSub[si] = fb * pol;

                // Subtle Sealegs-inspired dual LFO drift & L/R detune offset
                lfoPhase[0] += lfoInc0;
                if (lfoPhase[0] >= 1.0f) lfoPhase[0] -= 1.0f;
                lfoPhase[1] += lfoInc1;
                if (lfoPhase[1] >= 1.0f) lfoPhase[1] -= 1.0f;

                const float driftL = std::sin (lfoPhase[0] * juce::MathConstants<float>::twoPi) * (spr * 0.0045f);
                const float driftR = std::sin (lfoPhase[1] * juce::MathConstants<float>::twoPi) * (spr * 0.0045f);
                const float detuneRatio = spr * 0.028f;

                const float baseDelay = osRateF / pitchHz;
                const float maxDelay = static_cast<float> (delayLineLength - 4);
                delayLSub[si] = juce::jlimit (4.0f, maxDelay, baseDelay * (1.0f - detuneRatio + driftL));
                delayRSub[si] = juce::jlimit (4.0f, maxDelay, baseDelay * (1.0f + detuneRatio + driftR));

                // Pre-compute 12 dB/oct loop TPT SVF coefficients at oversampled rate
                const float clampedFc = juce::jlimit (120.0f, 0.35f * osRateF, loopFc);
                const float g = std::tan (juce::MathConstants<float>::pi * clampedFc / osRateF);
                const float k = 1.15f; // gentle Q ~ 0.87 for warm Echon 6 ping
                const float a1 = 1.0f / (1.0f + g * (g + k));
                loopGSub[si] = g;
                loopA1Sub[si] = a1;
                loopA2Sub[si] = g * a1;
                loopA3Sub[si] = g * loopA2Sub[si];

                // Gentle 6 dB/oct BBD damping tied softly to resonator pitch + loop cutoff
                const float bbdClockFc = std::min (0.40f * osRateF, std::min (loopFc * 1.5f, pitchHz * 18.0f + 1200.0f));
                const float omega6 = 2.0f * juce::MathConstants<float>::pi * bbdClockFc / osRateF;
                lp6AlphaSub[si] = omega6 / (1.0f + omega6);

                const int sIdx = sampleOffset + i;

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const size_t c = static_cast<size_t> (ch);
                    const float inSample = buffer.getSample (ch, sIdx);
                    dryRing[c][static_cast<size_t> (dryWritePos)] = inSample;

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
                const size_t c = static_cast<size_t> (ch);
                float* chData = osBlock.getChannelPointer (c);
                std::vector<float>& dBuf = delayBuffers[c];

                for (int osIdx = 0; osIdx < osNumSamples; ++osIdx)
                {
                    const size_t hostIdx = static_cast<size_t> (std::min (subLen - 1, osIdx / osFactor));
                    const float drv = driveSub[hostIdx];
                    const float chr = charSub[hostIdx];
                    const float delayLen = (ch == 0) ? delayLSub[hostIdx] : delayRSub[hostIdx];
                    const float fbSigned = fbSignedSub[hostIdx];
                    const float a1 = loopA1Sub[hostIdx];
                    const float a2 = loopA2Sub[hostIdx];
                    const float a3 = loopA3Sub[hostIdx];
                    const float lp6Alpha = lp6AlphaSub[hostIdx];

                    const float xIn = chData[osIdx];

                    // 1. Read previous delayed feedback sample using cubic Hermite interpolation
                    const float delayedRaw = readDelayCubic (dBuf, delayWritePos[c], delayLen);

                    // 2. Morphor Echon 6 double damping: LP 6dB + LP 12dB + 18Hz anti-DC HPF
                    loopLp6State[c] += lp6Alpha * (delayedRaw - loopLp6State[c]);

                    const float v3 = loopLp6State[c] - loopLp12Ic2[c];
                    const float v1 = a1 * loopLp12Ic1[c] + a2 * v3;
                    const float v2 = loopLp12Ic2[c] + a2 * loopLp12Ic1[c] + a3 * v3;
                    loopLp12Ic1[c] = 2.0f * v1 - loopLp12Ic1[c];
                    loopLp12Ic2[c] = 2.0f * v2 - loopLp12Ic2[c];

                    // Remove sub-18Hz / DC drift inside loop
                    const float hpOut = v2 - loopHpX1[c] + hpR * loopHpY1[c];
                    loopHpX1[c] = v2;
                    loopHpY1[c] = hpOut;

                    // 3. SOMA-inspired soft optical loop limiter prevents harsh blow-up at 98% feedback
                    const float fbLimited = std::tanh (hpOut * 0.78f) / 0.78f;
                    const float fbReturn = fbLimited * fbSigned;

                    // 4. Topology routing: PRE-LOOP vs IN-LOOP waveshaping
                    float loopSignal = 0.0f;

                    if (inLoopRouting)
                    {
                        const float shaperIn = xIn + fbReturn;
                        loopSignal = applyWaveshaper (ch, shaperIn, drv, chr);
                    }
                    else
                    {
                        const float shaped = applyWaveshaper (ch, xIn, drv, chr);
                        loopSignal = shaped + fbReturn;
                    }

                    // Gentle 1-pole DC blocker on output path
                    const float dcR = 0.9992f;
                    const float dcY = loopSignal - dcPrevX[c] + dcR * dcPrevY[c];
                    dcPrevX[c] = loopSignal;
                    dcPrevY[c] = dcY;

                    // 5. Write current node back into BBD delay line (soft-bounded for stability)
                    dBuf[static_cast<size_t> (delayWritePos[c])] = juce::jlimit (-4.0f, 4.0f, loopSignal);
                    delayWritePos[c] = (delayWritePos[c] + 1) % delayLineLength;

                    chData[osIdx] = dcY;
                }
            }

            oversampler->processSamplesDown (subBlock);

            for (int i = 0; i < subLen; ++i)
            {
                const size_t si = static_cast<size_t> (i);
                const int sIdx = sampleOffset + i;
                const float mix = mixSub[si];
                const float gain = outGainSub[si];

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
    float readDelayCubic (const std::vector<float>& buf, int writePos, float delaySamples) const noexcept
    {
        float readPos = static_cast<float> (writePos) - delaySamples;
        const float lenF = static_cast<float> (delayLineLength);
        while (readPos < 0.0f)
            readPos += lenF;
        while (readPos >= lenF)
            readPos -= lenF;

        const int i1 = static_cast<int> (readPos);
        const float frac = readPos - static_cast<float> (i1);

        const int i0 = (i1 - 1 + delayLineLength) % delayLineLength;
        const int i2 = (i1 + 1) % delayLineLength;
        const int i3 = (i1 + 2) % delayLineLength;

        const float y0 = buf[static_cast<size_t> (i0)];
        const float y1 = buf[static_cast<size_t> (i1)];
        const float y2 = buf[static_cast<size_t> (i2)];
        const float y3 = buf[static_cast<size_t> (i3)];

        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    float applyWaveshaper (int ch, float x, float drv, float chr) noexcept
    {
        switch (currentMode)
        {
            case Mode::BuchlaSoma:  return processBuchlaSoma (ch, x, drv, chr);
            case Mode::BifoldMorph: return processBifoldMorph (ch, x, drv, chr);
            case Mode::EricaXeno:   return processEricaXeno (ch, x, drv, chr);
            case Mode::Werner112dB: return processWerner112dB (ch, x, drv, chr);
        }
        return x;
    }

    static float tanhF1 (float x) noexcept
    {
        const float ax = std::abs (x);
        return ax + std::log1p (std::exp (-2.0f * ax)) - 0.69314718056f;
    }

    float processADAATanh (int ch, float x) noexcept
    {
        const size_t c = static_cast<size_t> (ch);
        const float xPrev = adaaPrevX[c];
        adaaPrevX[c] = x;

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
        const size_t c = static_cast<size_t> (ch);
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

        const float sV3 = clipped - screamIc2[c];
        const float sV1 = a1 * screamIc1[c] + a2 * sV3;
        const float sV2 = screamIc2[c] + a2 * screamIc1[c] + a3 * sV3;
        screamIc1[c] = 2.0f * sV1 - screamIc1[c];
        screamIc2[c] = 2.0f * sV2 - screamIc2[c];

        const float band = k * sV1;
        return std::tanh ((clipped + band * (0.3f + scream * 1.4f)) * 0.85f);
    }

    float processWerner112dB (int ch, float x, float drive, float resonanceNorm) noexcept
    {
        const size_t c = static_cast<size_t> (ch);
        const float inputGain = 1.0f + drive * 6.5f;
        const float inDriven = x * inputGain;

        const float fc = 280.0f + (1.0f - drive * 0.35f) * 3200.0f + resonanceNorm * 2200.0f;
        const float clampedFc = juce::jlimit (60.0f, 0.38f * static_cast<float> (oversampledRate), fc);
        const float g = std::tan (juce::MathConstants<float>::pi * clampedFc / static_cast<float> (oversampledRate));

        const float k = juce::jlimit (0.14f, 1.4f, 1.4f - resonanceNorm * 1.26f);

        const float denom = 1.0f + g * (g + k);
        const float hp = (inDriven - (k + g) * svfIc1[c] - svfIc2[c]) / denom;

        const float v1 = std::tanh (g * hp);
        const float bp = std::tanh (svfIc1[c] + v1);
        svfIc1[c] = std::tanh (bp + v1);

        const float v2 = std::tanh (g * bp);
        const float lp = std::tanh (svfIc2[c] + v2);
        svfIc2[c] = std::tanh (lp + v2);

        return (lp * 0.88f + bp * (0.12f + resonanceNorm * 0.2f));
    }

    double hostSampleRate = 44100.0;
    double oversampledRate = 176400.0;
    int activeChannels = 2;
    int latencySamples = 0;
    int delayLineLength = 0;

    Mode currentMode = Mode::BuchlaSoma;
    bool inLoopRouting = false;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::dsp::StateVariableTPTFilter<float> preFilter;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> preCutoffSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> characterSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> pitchHzSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> feedbackSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> loopCutoffSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> spreadSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> polaritySmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGainSmooth;

    std::array<float, maxChannels> adaaPrevX {};
    std::array<float, maxChannels> dcPrevX {};
    std::array<float, maxChannels> dcPrevY {};
    std::array<float, maxChannels> svfIc1 {};
    std::array<float, maxChannels> svfIc2 {};
    std::array<float, maxChannels> screamIc1 {};
    std::array<float, maxChannels> screamIc2 {};

    std::array<float, maxChannels> loopLp12Ic1 {};
    std::array<float, maxChannels> loopLp12Ic2 {};
    std::array<float, maxChannels> loopLp6State {};
    std::array<float, maxChannels> loopHpX1 {};
    std::array<float, maxChannels> loopHpY1 {};
    std::array<float, maxChannels> feedbackTap {};
    std::array<float, maxChannels> lfoPhase {};

    std::array<std::vector<float>, maxChannels> delayBuffers {};
    std::array<int, maxChannels> delayWritePos {};

    std::array<std::array<float, dryDelayBufferSize>, maxChannels> dryRing {};
    int dryWritePos = 0;
};
} // namespace runaway::dsp
