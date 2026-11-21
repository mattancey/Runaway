#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>
#include <cstdint>

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

    enum class LoopEngine : int
    {
        String = 0,     // Morphor Echon 6: single tuned BBD resonator
        Chord,          // Erica Synths x 112dB Razornator: 4-voice tuned parallel resonator
        Diffuse         // Strymon NightSky: 4x4 Hadamard FDN + octave shimmer
    };

    enum class EnvTarget : int
    {
        Off = 0,
        Drive,
        Character,
        LoopCutoff,
        Pitch
    };

    enum class LpgMode : int
    {
        Off = 0,
        Both,           // Buchla 292 classic Low Pass Gate (Filter + VCA coupled)
        VcaOnly,        // Pure Vactrol amplitude pluck
        FilterOnly      // Pure Vactrol resonant lowpass sweep
    };

    static constexpr int maxChannels = 2;
    static constexpr int numVoices = 4;
    static constexpr int maxSubBlock = 64;
    static constexpr int dryDelayBufferSize = 512;
    static constexpr int shimmerWindowSize = 2048;

    // Stateless preview helper for UI Transfer Curve X/Y visualization (message thread only)
    static float evalTransferPreview (int modeIndex, float x, float driveNorm, float charNorm) noexcept
    {
        const Mode m = static_cast<Mode> (juce::jlimit (0, 3, modeIndex));
        const float drv = juce::jlimit (0.0f, 1.0f, driveNorm);
        const float chr = juce::jlimit (0.0f, 1.0f, charNorm);

        switch (m)
        {
            case Mode::BuchlaSoma:
            {
                const float thr = chr * 0.28f;
                const float mag = std::abs (x);
                const float gate = (thr > 1.0e-5f) ? (mag * mag) / (mag * mag + thr * thr) : 1.0f;
                const float driven = (x * gate) * (1.0f + drv * 8.5f) + chr * 0.32f;
                return buchla259ParallelFold (driven);
            }

            case Mode::BifoldMorph:
            {
                const float buchlaOut = buchla259ParallelFold (x * (1.0f + drv * 7.5f));
                const float sergeOut = sergeSerialFold (x, drv);
                const float gB = std::cos (chr * juce::MathConstants<float>::halfPi);
                const float gS = std::sin (chr * juce::MathConstants<float>::halfPi);
                return (gB * buchlaOut + gS * sergeOut) * 0.95f;
            }

            case Mode::EricaXeno:
            {
                const float driven = x * (1.0f + drv * 15.0f) + (0.08f + chr * 0.35f);
                return std::tanh (driven);
            }

            case Mode::Werner112dB:
            default:
            {
                const float driven = x * (1.0f + drv * 6.5f);
                return std::tanh (driven * (0.95f + chr * 0.65f));
            }
        }
    }

    void prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        hostSampleRate = std::max (1000.0, sampleRate);
        activeChannels = juce::jlimit (1, maxChannels, numChannels);

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

        delayLineLength = static_cast<int> (std::ceil ((oversampledRate / 35.0) * 6.0)) + 128;
        for (size_t ch = 0; ch < maxChannels; ++ch)
            for (size_t v = 0; v < numVoices; ++v)
                delayBuffers[ch][v].assign (static_cast<size_t> (delayLineLength), 0.0f);

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
        wetGainSmooth.reset (hostSampleRate, smoothSec);

        pitchHzSmooth.reset (hostSampleRate, 0.025);
        feedbackSmooth.reset (hostSampleRate, smoothSec);
        loopCutoffSmooth.reset (hostSampleRate, smoothSec);
        spreadSmooth.reset (hostSampleRate, smoothSec);
        polaritySmooth.reset (hostSampleRate, 0.008);
        loopBypassSmooth.reset (hostSampleRate, 0.025);
        inLoopTrimSmooth.reset (hostSampleRate, 0.020);
        envAmountSmooth.reset (hostSampleRate, 0.020);
        lpgResSmooth.reset (hostSampleRate, 0.020);

        reset();
    }

    void reset()
    {
        if (oversampler != nullptr)
            oversampler->reset();

        preFilter.reset();

        for (size_t ch = 0; ch < maxChannels; ++ch)
        {
            adaaPrevX[ch] = 0.0f;
            dcPrevX[ch] = 0.0f;
            dcPrevY[ch] = 0.0f;
            svfIc1[ch] = 0.0f;
            svfIc2[ch] = 0.0f;
            screamIc1[ch] = 0.0f;
            screamIc2[ch] = 0.0f;
            loopLp12Ic1[ch] = 0.0f;
            loopLp12Ic2[ch] = 0.0f;
            loopLp6State[ch] = 0.0f;
            loopHpX1[ch] = 0.0f;
            loopHpY1[ch] = 0.0f;
            lpgIc1[ch] = 0.0f;
            lpgIc2[ch] = 0.0f;
            shimmerPhase[ch] = (ch == 0) ? 0.0f : 0.25f;
            dryRing[ch].fill (0.0f);

            for (size_t v = 0; v < numVoices; ++v)
            {
                delayWritePos[ch][v] = 0;
                fdnLp6A[ch][v] = 0.0f;
                fdnLp6B[ch][v] = 0.0f;
                std::fill (delayBuffers[ch][v].begin(), delayBuffers[ch][v].end(), 0.0f);
            }
        }

        lfoPhase[0] = 0.0f;
        lfoPhase[1] = 0.37f;
        envState = 0.0f;
        lastEnvelopeOut = 0.0f;
        vactrolFastEnv = 0.0f;
        vactrolSlowEnv = 0.0f;
        lastVactrolLevel = 0.0f;
        lastMidiStrikeCounter = 0;
        dryWritePos = 0;
    }

    int getLatencySamples() const noexcept { return latencySamples; }
    float getLastEnvelopeLevel() const noexcept { return lastEnvelopeOut; }
    float getLastVactrolLevel() const noexcept { return lastVactrolLevel; }

    void setParameters (int modeIndex,
                        int loopEngineIndex,
                        bool loopEnabled,
                        float inLoopLevelDb,
                        float preCutoffHz,
                        float driveNorm,
                        float characterNorm,
                        float pitchHz,
                        float feedbackNorm,
                        float loopCutoffHz,
                        float spreadNorm,
                        bool invertPolarity,
                        bool shaperInLoop,
                        int envTargetIndex,
                        float envAmountSigned,
                        float envReleaseMs,
                        int lpgModeIndex,
                        float lpgDecayMs,
                        float lpgResNorm,
                        uint32_t midiStrikeCounter,
                        float mixNorm,
                        float wetLevelDb) noexcept
    {
        currentMode = static_cast<Mode> (juce::jlimit (0, 3, modeIndex));
        currentLoopEngine = static_cast<LoopEngine> (juce::jlimit (0, 2, loopEngineIndex));
        currentEnvTarget = static_cast<EnvTarget> (juce::jlimit (0, 4, envTargetIndex));
        currentLpgMode = static_cast<LpgMode> (juce::jlimit (0, 3, lpgModeIndex));
        inLoopRouting = shaperInLoop;

        if (midiStrikeCounter != lastMidiStrikeCounter)
        {
            lastMidiStrikeCounter = midiStrikeCounter;
            vactrolFastEnv = 1.0f;
            vactrolSlowEnv = std::max (vactrolSlowEnv, 0.85f);
        }

        loopBypassSmooth.setTargetValue (loopEnabled ? 1.0f : 0.0f);

        const float inLoopTrimTarget = (loopEnabled && shaperInLoop)
            ? juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 12.0f, inLoopLevelDb))
            : 1.0f;
        inLoopTrimSmooth.setTargetValue (inLoopTrimTarget);

        preCutoffSmooth.setTargetValue (juce::jlimit (1800.0f, 20000.0f, preCutoffHz));
        driveSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, driveNorm));
        characterSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, characterNorm));
        pitchHzSmooth.setTargetValue (juce::jlimit (35.0f, 1200.0f, pitchHz));
        feedbackSmooth.setTargetValue (juce::jlimit (0.0f, 0.98f, feedbackNorm));
        loopCutoffSmooth.setTargetValue (juce::jlimit (200.0f, 16000.0f, loopCutoffHz));
        spreadSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, spreadNorm));
        polaritySmooth.setTargetValue (invertPolarity ? -1.0f : 1.0f);
        envAmountSmooth.setTargetValue (juce::jlimit (-1.0f, 1.0f, envAmountSigned));
        envReleaseMsCurrent = juce::jlimit (20.0f, 600.0f, envReleaseMs);
        lpgDecayMsCurrent = juce::jlimit (40.0f, 1200.0f, lpgDecayMs);
        lpgResSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, lpgResNorm));
        mixSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, mixNorm));
        wetGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 12.0f, wetLevelDb)));
    }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        if (oversampler == nullptr || buffer.getNumSamples() <= 0 || delayLineLength <= 16)
            return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = std::min (activeChannels, buffer.getNumChannels());
        if (numChannels <= 0)
            return;

        const float hostRateF = static_cast<float> (hostSampleRate);
        const float osRateF = static_cast<float> (oversampledRate);
        const float hpR = juce::jlimit (0.98f, 0.99995f,
                                        1.0f - (2.0f * juce::MathConstants<float>::pi * 18.0f / osRateF));
        const float lfoInc0 = 0.41f / hostRateF;
        const float lfoInc1 = 0.57f / hostRateF;
        const float shimmerInc = 1.0f / static_cast<float> (shimmerWindowSize);

        const float envAttCoeff = std::exp (-1.0f / (0.003f * hostRateF));
        const float envRelCoeff = std::exp (-1.0f / ((envReleaseMsCurrent * 0.001f) * hostRateF));

        // Dual-stage vactrol time constants (fast strike, organic asymmetric tail)
        const float vactrolAttFast = std::exp (-1.0f / (0.0018f * hostRateF));
        const float vactrolAttSlow = std::exp (-1.0f / (0.0065f * hostRateF));
        const float vactrolDecFast = std::exp (-1.0f / ((lpgDecayMsCurrent * 0.00075f) * hostRateF));
        const float vactrolDecSlow = std::exp (-1.0f / ((lpgDecayMsCurrent * 0.00155f) * hostRateF));

        static constexpr std::array<float, numVoices> chordPitchRatios { 1.0f, 1.498307f, 1.189207f, 2.0f };
        static constexpr std::array<float, numVoices> fdnRatios { 1.0f, 1.317f, 1.733f, 2.219f };

        int sampleOffset = 0;

        while (sampleOffset < numSamples)
        {
            const int subLen = std::min (maxSubBlock, numSamples - sampleOffset);

            std::array<float, maxSubBlock> driveSub {};
            std::array<float, maxSubBlock> charSub {};
            std::array<float, maxSubBlock> baseDelayLSub {};
            std::array<float, maxSubBlock> baseDelayRSub {};
            std::array<float, maxSubBlock> fbSignedSub {};
            std::array<float, maxSubBlock> spreadSub {};
            std::array<float, maxSubBlock> loopBypassSub {};
            std::array<float, maxSubBlock> inLoopTrimSub {};
            std::array<float, maxSubBlock> loopA1Sub {};
            std::array<float, maxSubBlock> loopA2Sub {};
            std::array<float, maxSubBlock> loopA3Sub {};
            std::array<float, maxSubBlock> lp6AlphaSub {};
            std::array<float, maxSubBlock> fdnLpAlphaSub {};
            std::array<float, maxSubBlock> lpgCvSub {};
            std::array<float, maxSubBlock> lpgResSub {};
            std::array<float, maxSubBlock> mixSub {};
            std::array<float, maxSubBlock> wetGainSub {};

            for (int i = 0; i < subLen; ++i)
            {
                const size_t si = static_cast<size_t> (i);
                const int sIdx = sampleOffset + i;

                float peakIn = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const size_t c = static_cast<size_t> (ch);
                    const float inSample = buffer.getSample (ch, sIdx);
                    peakIn = std::max (peakIn, std::abs (inSample));
                    dryRing[c][static_cast<size_t> (dryWritePos)] = inSample;
                }

                if (peakIn > envState)
                    envState = envAttCoeff * envState + (1.0f - envAttCoeff) * peakIn;
                else
                    envState = envRelCoeff * envState + (1.0f - envRelCoeff) * peakIn;

                const float envNorm = juce::jlimit (0.0f, 1.0f, envState * 1.45f);
                lastEnvelopeOut = envNorm;

                // Vactrol optical photoresistor response
                const float lpgRes = lpgResSmooth.getNextValue();
                const float strikeDrive = juce::jlimit (0.0f, 1.0f, peakIn * (1.45f + lpgRes * 1.15f));

                if (strikeDrive > vactrolFastEnv)
                    vactrolFastEnv = vactrolAttFast * vactrolFastEnv + (1.0f - vactrolAttFast) * strikeDrive;
                else
                    vactrolFastEnv = vactrolDecFast * vactrolFastEnv + (1.0f - vactrolDecFast) * strikeDrive;

                if (strikeDrive > vactrolSlowEnv)
                    vactrolSlowEnv = vactrolAttSlow * vactrolSlowEnv + (1.0f - vactrolAttSlow) * strikeDrive;
                else
                    vactrolSlowEnv = vactrolDecSlow * vactrolSlowEnv + (1.0f - vactrolDecSlow) * strikeDrive;

                const float vactrolCV = juce::jlimit (0.0f, 1.0f, 0.58f * vactrolFastEnv + 0.42f * vactrolSlowEnv);
                lastVactrolLevel = (currentLpgMode == LpgMode::Off) ? 0.0f : vactrolCV;
                lpgCvSub[si] = vactrolCV;
                lpgResSub[si] = lpgRes;

                const float cutoff = preCutoffSmooth.getNextValue();
                preFilter.setCutoffFrequency (cutoff);

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const float rawIn = dryRing[static_cast<size_t> (ch)][static_cast<size_t> (dryWritePos)];
                    const float preFiltered = preFilter.processSample (ch, rawIn);
                    buffer.setSample (ch, sIdx, preFiltered);
                }

                dryWritePos = (dryWritePos + 1) % dryDelayBufferSize;

                float drv = driveSmooth.getNextValue();
                float chr = characterSmooth.getNextValue();
                float pitchHz = pitchHzSmooth.getNextValue();
                float loopFc = loopCutoffSmooth.getNextValue();
                const float envMod = envNorm * envAmountSmooth.getNextValue();

                switch (currentEnvTarget)
                {
                    case EnvTarget::Drive:
                        drv = juce::jlimit (0.0f, 1.0f, drv + envMod * 0.80f);
                        break;

                    case EnvTarget::Character:
                        chr = juce::jlimit (0.0f, 1.0f, chr + envMod * 0.85f);
                        break;

                    case EnvTarget::LoopCutoff:
                        loopFc = juce::jlimit (200.0f, 16000.0f, loopFc * std::exp2 (envMod * 3.0f));
                        break;

                    case EnvTarget::Pitch:
                        pitchHz = juce::jlimit (35.0f, 1200.0f, pitchHz * std::exp2 (envMod * 1.0f));
                        break;

                    case EnvTarget::Off:
                    default:
                        break;
                }

                driveSub[si] = drv;
                charSub[si] = chr;
                mixSub[si] = mixSmooth.getNextValue();
                wetGainSub[si] = wetGainSmooth.getNextValue();

                const float fb = feedbackSmooth.getNextValue();
                const float pol = polaritySmooth.getNextValue();
                const float spr = spreadSmooth.getNextValue();
                const float loopActive = loopBypassSmooth.getNextValue();
                const float inLoopTrim = inLoopTrimSmooth.getNextValue();

                loopBypassSub[si] = loopActive;
                inLoopTrimSub[si] = inLoopTrim;
                fbSignedSub[si] = fb * pol * loopActive;
                spreadSub[si] = spr;

                lfoPhase[0] += lfoInc0;
                if (lfoPhase[0] >= 1.0f) lfoPhase[0] -= 1.0f;
                lfoPhase[1] += lfoInc1;
                if (lfoPhase[1] >= 1.0f) lfoPhase[1] -= 1.0f;

                const float driftL = std::sin (lfoPhase[0] * juce::MathConstants<float>::twoPi) * (spr * 0.0045f);
                const float driftR = std::sin (lfoPhase[1] * juce::MathConstants<float>::twoPi) * (spr * 0.0045f);
                const float detuneRatio = spr * 0.028f;

                const float baseDelay = osRateF / pitchHz;
                baseDelayLSub[si] = baseDelay * (1.0f - detuneRatio + driftL);
                baseDelayRSub[si] = baseDelay * (1.0f + detuneRatio + driftR);

                const float clampedFc = juce::jlimit (120.0f, 0.35f * osRateF, loopFc);
                const float g = std::tan (juce::MathConstants<float>::pi * clampedFc / osRateF);
                const float k = 1.15f;
                const float a1 = 1.0f / (1.0f + g * (g + k));
                loopA1Sub[si] = a1;
                loopA2Sub[si] = g * a1;
                loopA3Sub[si] = g * loopA2Sub[si];

                const float bbdClockFc = std::min (0.40f * osRateF, std::min (loopFc * 1.5f, pitchHz * 18.0f + 1200.0f));
                const float omega6 = 2.0f * juce::MathConstants<float>::pi * bbdClockFc / osRateF;
                lp6AlphaSub[si] = omega6 / (1.0f + omega6);

                const float omegaFdn = 2.0f * juce::MathConstants<float>::pi * clampedFc / osRateF;
                fdnLpAlphaSub[si] = omegaFdn / (1.0f + omegaFdn);
            }

            juce::dsp::AudioBlock<float> fullBlock (buffer);
            auto subBlock = fullBlock.getSubsetChannelBlock (0, static_cast<size_t> (numChannels))
                                     .getSubBlock (static_cast<size_t> (sampleOffset),
                                                   static_cast<size_t> (subLen));
            auto osBlock = oversampler->processSamplesUp (subBlock);

            const int osNumSamples = static_cast<int> (osBlock.getNumSamples());
            const int osFactor = std::max (1, osNumSamples / subLen);
            const float maxDelayF = static_cast<float> (delayLineLength - 6);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const size_t c = static_cast<size_t> (ch);
                float* chData = osBlock.getChannelPointer (c);

                for (int osIdx = 0; osIdx < osNumSamples; ++osIdx)
                {
                    const size_t hostIdx = static_cast<size_t> (std::min (subLen - 1, osIdx / osFactor));
                    const float drv = driveSub[hostIdx];
                    const float chr = charSub[hostIdx];
                    const float baseDelay = (ch == 0) ? baseDelayLSub[hostIdx] : baseDelayRSub[hostIdx];
                    const float fbSigned = fbSignedSub[hostIdx];
                    const float spr = spreadSub[hostIdx];
                    const float loopActive = loopBypassSub[hostIdx];
                    const float inLoopTrim = inLoopTrimSub[hostIdx];
                    const float a1 = loopA1Sub[hostIdx];
                    const float a2 = loopA2Sub[hostIdx];
                    const float a3 = loopA3Sub[hostIdx];
                    const float lp6Alpha = lp6AlphaSub[hostIdx];
                    const float fdnAlpha = fdnLpAlphaSub[hostIdx];

                    const float xIn = chData[osIdx];
                    float wetSignal = 0.0f;

                    if (currentLoopEngine == LoopEngine::String)
                    {
                        const float delayLen = juce::jlimit (4.0f, maxDelayF, baseDelay);
                        const float delayedRaw = readDelayCubic (delayBuffers[c][0], delayWritePos[c][0], delayLen);

                        loopLp6State[c] += lp6Alpha * (delayedRaw - loopLp6State[c]);

                        const float v3 = loopLp6State[c] - loopLp12Ic2[c];
                        const float v1 = a1 * loopLp12Ic1[c] + a2 * v3;
                        const float v2 = loopLp12Ic2[c] + a2 * loopLp12Ic1[c] + a3 * v3;
                        loopLp12Ic1[c] = 2.0f * v1 - loopLp12Ic1[c];
                        loopLp12Ic2[c] = 2.0f * v2 - loopLp12Ic2[c];

                        const float hpOut = v2 - loopHpX1[c] + hpR * loopHpY1[c];
                        loopHpX1[c] = v2;
                        loopHpY1[c] = hpOut;

                        const float fbLimited = std::tanh (hpOut * 0.78f) / 0.78f;
                        const float fbReturn = fbLimited * fbSigned;

                        float loopNode = 0.0f;
                        if (inLoopRouting)
                        {
                            loopNode = applyWaveshaper (ch, xIn + fbReturn, drv, chr);
                        }
                        else
                        {
                            const float shaped = applyWaveshaper (ch, xIn, drv, chr);
                            loopNode = shaped + fbReturn;
                        }

                        delayBuffers[c][0][static_cast<size_t> (delayWritePos[c][0])] = juce::jlimit (-4.0f, 4.0f, loopNode);
                        delayWritePos[c][0] = (delayWritePos[c][0] + 1) % delayLineLength;

                        wetSignal = loopNode;
                    }
                    else if (currentLoopEngine == LoopEngine::Chord)
                    {
                        std::array<float, numVoices> voiceOuts {};
                        float chordSum = 0.0f;

                        const std::array<float, numVoices> voiceWeights {
                            1.0f,
                            spr * 0.72f,
                            spr * 0.58f,
                            spr * 0.65f
                        };
                        const float normSum = voiceWeights[0] + voiceWeights[1] + voiceWeights[2] + voiceWeights[3];

                        for (size_t v = 0; v < numVoices; ++v)
                        {
                            const float stereoRatio = (ch == 0) ? (1.0f - 0.008f * static_cast<float> (v) * spr)
                                                                : (1.0f + 0.008f * static_cast<float> (v) * spr);
                            const float vDelay = juce::jlimit (4.0f, maxDelayF,
                                                               (baseDelay / chordPitchRatios[v]) * stereoRatio);
                            voiceOuts[v] = readDelayCubic (delayBuffers[c][v], delayWritePos[c][v], vDelay);
                            chordSum += voiceOuts[v] * voiceWeights[v];
                        }

                        chordSum /= std::max (1.0f, normSum);

                        loopLp6State[c] += lp6Alpha * (chordSum - loopLp6State[c]);
                        const float v3 = loopLp6State[c] - loopLp12Ic2[c];
                        const float v1 = a1 * loopLp12Ic1[c] + a2 * v3;
                        const float v2 = loopLp12Ic2[c] + a2 * loopLp12Ic1[c] + a3 * v3;
                        loopLp12Ic1[c] = 2.0f * v1 - loopLp12Ic1[c];
                        loopLp12Ic2[c] = 2.0f * v2 - loopLp12Ic2[c];

                        const float hpOut = v2 - loopHpX1[c] + hpR * loopHpY1[c];
                        loopHpX1[c] = v2;
                        loopHpY1[c] = hpOut;

                        const float fbLimited = std::tanh (hpOut * 0.78f) / 0.78f;
                        const float fbReturn = fbLimited * fbSigned;

                        float excite = 0.0f;
                        if (inLoopRouting)
                        {
                            excite = applyWaveshaper (ch, xIn + fbReturn, drv, chr);
                        }
                        else
                        {
                            const float shaped = applyWaveshaper (ch, xIn, drv, chr);
                            excite = shaped + fbReturn;
                        }

                        for (size_t v = 0; v < numVoices; ++v)
                        {
                            const float vFb = std::tanh (voiceOuts[v] * 0.78f) / 0.78f;
                            const float writeVal = inLoopRouting
                                ? excite
                                : (excite * 0.65f + vFb * fbSigned * 0.35f);
                            delayBuffers[c][v][static_cast<size_t> (delayWritePos[c][v])] = juce::jlimit (-4.0f, 4.0f, writeVal);
                            delayWritePos[c][v] = (delayWritePos[c][v] + 1) % delayLineLength;
                        }

                        wetSignal = excite;
                    }
                    else
                    {
                        std::array<float, numVoices> y {};
                        const float sizeScale = 2.35f;

                        for (size_t v = 0; v < numVoices; ++v)
                        {
                            const float stereoBias = (ch == 0) ? (1.0f - 0.015f * static_cast<float> (v) * spr)
                                                               : (1.0f + 0.015f * static_cast<float> (v) * spr);
                            const float vDelay = juce::jlimit (8.0f, maxDelayF,
                                                               baseDelay * sizeScale * fdnRatios[v] * stereoBias);
                            const float raw = readDelayCubic (delayBuffers[c][v], delayWritePos[c][v], vDelay);

                            fdnLp6A[c][v] += fdnAlpha * (raw - fdnLp6A[c][v]);
                            fdnLp6B[c][v] += fdnAlpha * (fdnLp6A[c][v] - fdnLp6B[c][v]);
                            y[v] = fdnLp6B[c][v];
                        }

                        const float a0 = y[0] + y[1];
                        const float a1_h = y[0] - y[1];
                        const float a2_h = y[2] + y[3];
                        const float a3_h = y[2] - y[3];

                        std::array<float, numVoices> h {
                            0.5f * (a0 + a2_h),
                            0.5f * (a1_h + a3_h),
                            0.5f * (a0 - a2_h),
                            0.5f * (a1_h - a3_h)
                        };

                        shimmerPhase[c] += shimmerInc;
                        if (shimmerPhase[c] >= 1.0f)
                            shimmerPhase[c] -= 1.0f;

                        const float p1 = shimmerPhase[c];
                        float p2 = p1 + 0.5f;
                        if (p2 >= 1.0f) p2 -= 1.0f;

                        const float wF = static_cast<float> (shimmerWindowSize);
                        const float shimD1 = juce::jlimit (8.0f, maxDelayF, 16.0f + (1.0f - p1) * wF);
                        const float shimD2 = juce::jlimit (8.0f, maxDelayF, 16.0f + (1.0f - p2) * wF);

                        const float s1 = readDelayCubic (delayBuffers[c][0], delayWritePos[c][0], shimD1);
                        const float s2 = readDelayCubic (delayBuffers[c][0], delayWritePos[c][0], shimD2);
                        const float w1 = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * p1);
                        const float w2 = 1.0f - w1;
                        const float shimmerSig = (s1 * w1 + s2 * w2) * (spr * 0.34f);

                        const float fdnSum = 0.5f * (h[0] + h[1] + h[2] + h[3]);
                        const float hpOut = fdnSum - loopHpX1[c] + hpR * loopHpY1[c];
                        loopHpX1[c] = fdnSum;
                        loopHpY1[c] = hpOut;

                        const float fbLimited = std::tanh (hpOut * 0.78f) / 0.78f;
                        const float fbReturn = fbLimited * fbSigned;

                        float excite = 0.0f;
                        if (inLoopRouting)
                            excite = applyWaveshaper (ch, xIn + fbReturn, drv, chr);
                        else
                            excite = applyWaveshaper (ch, xIn, drv, chr);

                        const float fdnFbGain = fbSigned * 0.94f;
                        for (size_t v = 0; v < numVoices; ++v)
                        {
                            const float shimTap = (v == 1 || v == 2) ? shimmerSig * fbSigned : 0.0f;
                            const float tankFeed = inLoopRouting
                                ? (excite * 0.5f + std::tanh (h[v] * 0.75f) * fdnFbGain * 0.65f + shimTap)
                                : (excite * 0.5f + std::tanh (h[v] * 0.75f) * fdnFbGain + shimTap);

                            delayBuffers[c][v][static_cast<size_t> (delayWritePos[c][v])] = juce::jlimit (-4.0f, 4.0f, tankFeed);
                            delayWritePos[c][v] = (delayWritePos[c][v] + 1) % delayLineLength;
                        }

                        wetSignal = inLoopRouting ? excite : (excite + fbReturn * loopActive);
                    }

                    wetSignal *= inLoopTrim;

                    const float dcR = 0.9992f;
                    const float dcY = wetSignal - dcPrevX[c] + dcR * dcPrevY[c];
                    dcPrevX[c] = wetSignal;
                    dcPrevY[c] = dcY;

                    chData[osIdx] = dcY;
                }
            }

            oversampler->processSamplesDown (subBlock);

            // Apply Buchla 292 Vactrol LPG on Wet path, then Wet Level & Dry/Wet Mix
            for (int i = 0; i < subLen; ++i)
            {
                const size_t si = static_cast<size_t> (i);
                const int sIdx = sampleOffset + i;
                const float mix = mixSub[si];
                const float wetGain = wetGainSub[si];
                const float cv = lpgCvSub[si];
                const float res = lpgResSub[si];

                const float cvCurve = cv * (0.30f + 0.70f * cv);
                const float lpgFc = juce::jlimit (100.0f, 0.43f * hostRateF, 110.0f + cvCurve * 15500.0f);
                const float lpgG = std::tan (juce::MathConstants<float>::pi * lpgFc / hostRateF);
                const float lpgK = 1.414f - res * 1.05f;
                const float lpgA1 = 1.0f / (1.0f + lpgG * (lpgG + lpgK));
                const float lpgA2 = lpgG * lpgA1;
                const float lpgA3 = lpgG * lpgA2;
                const float vcaGain = cv * (0.40f + 0.60f * cv);

                const int readIdx = (dryWritePos - subLen + i - latencySamples + dryDelayBufferSize * 2)
                                    % dryDelayBufferSize;

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const size_t c = static_cast<size_t> (ch);
                    const float dry = dryRing[c][static_cast<size_t> (readIdx)];
                    float wet = buffer.getSample (ch, sIdx);

                    // Run 12dB TPT LPG filter continuously when LPG != Off to keep filter state warm
                    if (currentLpgMode != LpgMode::Off)
                    {
                        const float v3 = wet - lpgIc2[c];
                        const float v1 = lpgA1 * lpgIc1[c] + lpgA2 * v3;
                        const float v2 = lpgIc2[c] + lpgA2 * lpgIc1[c] + lpgA3 * v3;
                        lpgIc1[c] = 2.0f * v1 - lpgIc1[c];
                        lpgIc2[c] = 2.0f * v2 - lpgIc2[c];

                        if (currentLpgMode == LpgMode::Both)
                            wet = v2 * vcaGain;
                        else if (currentLpgMode == LpgMode::VcaOnly)
                            wet = wet * vcaGain;
                        else
                            wet = v2;
                    }

                    wet *= wetGain;

                    if (std::abs (wet) > 1.0f)
                        wet = std::tanh (wet * 0.85f) / 0.85f;

                    const float out = dry * (1.0f - mix) + wet * mix;
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
    LoopEngine currentLoopEngine = LoopEngine::String;
    EnvTarget currentEnvTarget = EnvTarget::Off;
    LpgMode currentLpgMode = LpgMode::Off;
    bool inLoopRouting = false;

    float envReleaseMsCurrent = 140.0f;
    float envState = 0.0f;
    float lastEnvelopeOut = 0.0f;

    float lpgDecayMsCurrent = 280.0f;
    float vactrolFastEnv = 0.0f;
    float vactrolSlowEnv = 0.0f;
    float lastVactrolLevel = 0.0f;
    uint32_t lastMidiStrikeCounter = 0;

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
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> loopBypassSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inLoopTrimSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> envAmountSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lpgResSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> wetGainSmooth;

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
    std::array<float, maxChannels> lpgIc1 {};
    std::array<float, maxChannels> lpgIc2 {};
    std::array<float, maxChannels> lfoPhase {};
    std::array<float, maxChannels> shimmerPhase {};

    std::array<std::array<float, numVoices>, maxChannels> fdnLp6A {};
    std::array<std::array<float, numVoices>, maxChannels> fdnLp6B {};
    std::array<std::array<std::vector<float>, numVoices>, maxChannels> delayBuffers {};
    std::array<std::array<int, numVoices>, maxChannels> delayWritePos {};

    std::array<std::array<float, dryDelayBufferSize>, maxChannels> dryRing {};
    int dryWritePos = 0;
};
} // namespace runaway::dsp
