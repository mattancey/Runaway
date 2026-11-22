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

    struct CosmosMod
    {
        float pitchDrift = 0.0f;
        float filterDrift = 0.0f;
        float diffuseDrift = 0.0f;
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
        cosmosSmooth.reset (hostSampleRate, 0.030);
        polaritySmooth.reset (hostSampleRate, 0.008);
        loopBypassSmooth.reset (hostSampleRate, 0.025);
        inLoopTrimSmooth.reset (hostSampleRate, 0.020);
        envAmountSmooth.reset (hostSampleRate, 0.020);
        lpgResSmooth.reset (hostSampleRate, 0.020);

        cosmosWanderAlpha = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 0.28f
                                             / static_cast<float> (hostSampleRate));

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

            cosmosWanderA[ch] = 0.0f;
            cosmosWanderB[ch] = 0.0f;
            cosmosWanderTarget[ch] = 0.0f;
            cosmosWanderHold[ch] = 0;

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
        cosmosPhase[0] = { 0.0f, 0.31f, 0.67f };
        cosmosPhase[1] = { 0.43f, 0.79f, 0.17f };
        cosmosRng[0] = 0x13579bdfu;
        cosmosRng[1] = 0x2468ace1u;

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
                        float cosmosNorm,
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
        cosmosSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, cosmosNorm));
        polaritySmooth.setTargetValue (invertPolarity ? -1.0f : 1.0f);
        envAmountSmooth.setTargetValue (juce::jlimit (-1.0f, 1.0f, envAmountSigned));
        envReleaseMsCurrent = juce::jlimit (20.0f, 600.0f, envReleaseMs);
        lpgDecayMsCurrent = juce::jlimit (40.0f, 1200.0f, lpgDecayMs);
        lpgResSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, lpgResNorm));
        mixSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, mixNorm));
        wetGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 12.0f, wetLevelDb)));
    }

    CosmosMod cosmos (int ch, float amount) noexcept
    {
        const size_t c = static_cast<size_t> (juce::jlimit (0, maxChannels - 1, ch));
        const float rate = static_cast<float> (hostSampleRate);

        static constexpr std::array<std::array<float, 3>, maxChannels> freqsHz {{
            { 0.043f, 0.113f, 0.271f },
            { 0.059f, 0.149f, 0.317f }
        }};

        for (size_t k = 0; k < 3; ++k)
        {
            cosmosPhase[c][k] += freqsHz[c][k] / rate;
            if (cosmosPhase[c][k] >= 1.0f)
                cosmosPhase[c][k] -= 1.0f;
        }

        if (--cosmosWanderHold[c] <= 0)
        {
            cosmosRng[c] ^= cosmosRng[c] << 13;
            cosmosRng[c] ^= cosmosRng[c] >> 17;
            cosmosRng[c] ^= cosmosRng[c] << 5;

            const float randNorm = static_cast<float> (cosmosRng[c] & 0x00ffffffu) / 8388608.0f - 1.0f;
            cosmosWanderTarget[c] = randNorm;

            const int minSamples = std::max (1, static_cast<int> (0.35f * rate));
            const int jitter = static_cast<int> (static_cast<float> (cosmosRng[c] >> 24) * (0.0025f * rate));
            cosmosWanderHold[c] = minSamples + jitter;
        }

        cosmosWanderA[c] += cosmosWanderAlpha * (cosmosWanderTarget[c] - cosmosWanderA[c]);
        cosmosWanderB[c] += cosmosWanderAlpha * (cosmosWanderA[c] - cosmosWanderB[c]);

        const float twoPi = juce::MathConstants<float>::twoPi;
        const float s0 = std::sin (cosmosPhase[c][0] * twoPi);
        const float s1 = std::sin (cosmosPhase[c][1] * twoPi);
        const float s2 = std::sin (cosmosPhase[c][2] * twoPi);
        const float wander = juce::jlimit (-1.0f, 1.0f, cosmosWanderB[c] * 1.45f);

        const float curve = amount * (0.35f + 0.65f * amount);

        CosmosMod out;
        out.pitchDrift = curve * 0.024f * (0.32f * s0 + 0.23f * s1 + 0.15f * s2 + 0.30f * wander);
        out.filterDrift = curve * (0.45f * s1 - 0.25f * s0 + 0.30f * wander);
        out.diffuseDrift = curve * (0.40f * s2 + 0.35f * s0 + 0.25f * wander);
        return out;
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
            std::array<std::array<float, maxSubBlock>, maxChannels> loopA1Sub {};
            std::array<std::array<float, maxSubBlock>, maxChannels> loopA2Sub {};
            std::array<std::array<float, maxSubBlock>, maxChannels> loopA3Sub {};
            std::array<std::array<float, maxSubBlock>, maxChannels> lp6AlphaSub {};
            std::array<std::array<float, maxSubBlock>, maxChannels> fdnLpAlphaSub {};
            std::array<std::array<float, maxSubBlock>, maxChannels> cosmosDiffuseSub {};
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
                const float cosmosAmt = cosmosSmooth.getNextValue();
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

                const CosmosMod cosmosL = cosmos (0, cosmosAmt);
                const CosmosMod cosmosR = (numChannels > 1) ? cosmos (1, cosmosAmt) : cosmosL;

                cosmosDiffuseSub[0][si] = cosmosL.diffuseDrift;
                cosmosDiffuseSub[1][si] = cosmosR.diffuseDrift;

                const float driftL = std::sin (lfoPhase[0] * juce::MathConstants<float>::twoPi) * (spr * 0.0045f);
                const float driftR = std::sin (lfoPhase[1] * juce::MathConstants<float>::twoPi) * (spr * 0.0045f);
                const float detuneRatio = spr * 0.028f;

                const float baseDelay = osRateF / pitchHz;
                baseDelayLSub[si] = baseDelay * (1.0f - detuneRatio + driftL + cosmosL.pitchDrift);
                baseDelayRSub[si] = baseDelay * (1.0f + detuneRatio + driftR + cosmosR.pitchDrift);

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const size_t c = static_cast<size_t> (ch);
                    const float f
