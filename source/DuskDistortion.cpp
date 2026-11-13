#include "PluginProcessor.h"
#include <cmath>

float DuskAudioProcessor::applyInputDrive(float sample, float amount,
                                          int distortionType) noexcept
{
    const auto safeAmount = juce::jlimit(0.0f, 1.0f, amount);
    if (safeAmount <= 0.0f)
        return sample;

    float saturated = sample;
    if (distortionType == 2)
    {
        const float driveGain = 1.0f + safeAmount * 9.0f;
        const float bias = 0.05f + safeAmount * 0.25f;
        const float zeroOffset = std::tanh(bias);
        const float reference = std::tanh(0.80f * driveGain + bias)
                              - zeroOffset;
        const float normaliser = juce::jmax(0.001f, reference);
        const float outputCompensation = 1.0f / (1.0f + 0.9f * safeAmount);
        saturated = (std::tanh(sample * driveGain + bias) - zeroOffset)
                  / normaliser * outputCompensation;
    }
    else if (distortionType == 1)
    {
        const float driveGain = 1.0f + safeAmount * 7.0f;
        const float normaliser = juce::jmax(
            0.001f, std::atan(driveGain));
        const float outputCompensation = 1.0f / (1.0f + 1.2f * safeAmount);
        saturated = std::atan(sample * driveGain)
                  / normaliser * outputCompensation;
    }
    else
    {
        const float driveGain = 1.0f + safeAmount * (16.0f - 1.0f);
        const float outputCompensation = 1.0f / (1.0f + 3.0f * safeAmount);
        saturated = std::tanh(sample * driveGain) * outputCompensation;
    }
    return sample + (saturated - sample) * safeAmount;
}

float DuskAudioProcessor::processRectifyDrive(float sample, float amount,
                                                    int channel) noexcept
{
    const auto index = distortionStateIndex(channel);
    const float safeAmount = juce::jlimit(0.0f, 1.0f, amount);
    const float driveGain = 1.0f + safeAmount * 11.0f;
    const float rectified = std::abs(std::tanh(sample * driveGain));
    const float dcBlocked = rectified - rectifyPreviousInput[index]
                          + rectifyDcCoefficient * rectifyPreviousOutput[index];
    rectifyPreviousInput[index] = rectified;
    rectifyPreviousOutput[index] = sanitiseSample(dcBlocked, 4.0f);

    const float outputCompensation = 0.78f / (1.0f + 0.65f * safeAmount);
    const float shaped = rectifyPreviousOutput[index] * outputCompensation;
    return sample + (shaped - sample) * safeAmount;
}

float DuskAudioProcessor::processCrackleDrive(float sample, float amount,
                                                   int channel) noexcept
{
    const auto index = distortionStateIndex(channel);
    const float safeAmount = juce::jlimit(0.0f, 1.0f, amount);
    const float activity = juce::jlimit(0.0f, 1.0f, std::abs(sample) * 2.5f);
    const float eventRateHz = 4.0f + safeAmount * safeAmount * 360.0f;
    const float eventProbability = eventRateHz
        / static_cast<float>(juce::jmax(1.0, currentSampleRate));
    const float trigger = nextNoiseSample(crackleNoiseState[index]) * 0.5f + 0.5f;
    if (trigger < eventProbability * (0.20f + activity * 0.80f))
    {
        const float polarity = nextNoiseSample(crackleNoiseState[index]);
        crackleBurst[index] = polarity * (0.025f + safeAmount * 0.32f)
                            * (0.30f + activity * 0.70f);
    }

    const float burstDurationSeconds = juce::jmap(safeAmount, 0.0035f, 0.00065f);
    const float burstDecay = juce::jlimit(
        0.0f, 0.999999f,
        1.0f - 1.0f
            / (burstDurationSeconds
                * static_cast<float>(juce::jmax(1.0, currentSampleRate))));
    const float burst = crackleBurst[index];
    crackleBurst[index] *= burstDecay;

    const float driveGain = 1.0f + safeAmount * 5.0f;
    const float normaliser = juce::jmax(0.001f, std::tanh(driveGain));
    const float rough = std::tanh(sample * driveGain) / normaliser + burst;
    const float dcBlocked = rough - cracklePreviousInput[index]
                          + crackleDcCoefficient * cracklePreviousOutput[index];
    cracklePreviousInput[index] = rough;
    cracklePreviousOutput[index] = sanitiseSample(dcBlocked, 4.0f);
    const float outputCompensation = 1.0f / (1.0f + safeAmount * 0.55f);
    const float shaped = cracklePreviousOutput[index] * outputCompensation;
    return sample + (shaped - sample) * safeAmount;
}

float DuskAudioProcessor::processLightningDrive(float sample, float amount,
                                                      int channel) noexcept
{
    const auto index = distortionStateIndex(channel);
    const float safeAmount = juce::jlimit(0.0f, 1.0f, amount);
    const float activity = juce::jlimit(0.0f, 1.0f, std::abs(sample) * 3.0f);
    const float whiteNoise = nextNoiseSample(lightningNoiseState[index])
                           * safeAmount * safeAmount * 0.075f
                           * (0.20f + activity * 0.80f);
    const float driveGain = 1.0f + safeAmount * 18.0f;
    const float driven = sample * driveGain;
    const float hardClipped = juce::jlimit(-1.0f, 1.0f, driven);
    const float sharp = juce::jlimit(
        -1.05f, 1.05f, hardClipped + whiteNoise);
    const float dcBlocked = sharp - lightningPreviousInput[index]
                          + lightningDcCoefficient
                            * lightningPreviousOutput[index];
    lightningPreviousInput[index] = sharp;
    lightningPreviousOutput[index] = sanitiseSample(dcBlocked, 4.0f);

    const float outputCompensation = 0.92f / (1.0f + safeAmount * 1.35f);
    const float shaped = lightningPreviousOutput[index] * outputCompensation;
    return sample + (shaped - sample) * safeAmount;
}

float DuskAudioProcessor::processAmpDrive(float sample, float amount,
                                               int channel) noexcept
{
    const auto index = distortionStateIndex(channel);
    const float safeAmount = juce::jlimit(0.0f, 1.0f, amount);
    const float preampGain = 1.0f + safeAmount * 13.0f;
    const float bias = 0.08f + safeAmount * 0.22f;
    const float zeroOffset = std::tanh(bias);
    const float reference = juce::jmax(
        0.001f, std::tanh(preampGain * 0.75f + bias) - zeroOffset);
    const float preamp = (std::tanh(sample * preampGain + bias) - zeroOffset)
                       / reference;
    const float powerGain = 1.0f + safeAmount * 3.5f;
    const float powerReference = juce::jmax(0.001f, std::tanh(powerGain));
    const float powerStage = std::tanh(preamp * powerGain) / powerReference;

    ampLowpassState[index] = (1.0f - ampToneCoefficient) * powerStage
                           + ampToneCoefficient * ampLowpassState[index];
    const float dcBlocked = ampLowpassState[index] - ampPreviousInput[index]
                          + ampDcCoefficient * ampPreviousOutput[index];
    ampPreviousInput[index] = ampLowpassState[index];
    ampPreviousOutput[index] = sanitiseSample(dcBlocked, 4.0f);

    const float outputCompensation = 0.92f / (1.0f + safeAmount * 0.35f);
    const float shaped = ampPreviousOutput[index] * outputCompensation;
    return sample + (shaped - sample) * safeAmount;
}

float DuskAudioProcessor::processBumpyDrive(float sample, float amount,
                                                 int channel) noexcept
{
    const auto index = distortionStateIndex(channel);
    const float safeAmount = juce::jlimit(0.0f, 1.0f, amount);

    bumpyToneState[index] = (1.0f - bumpyToneCoefficient) * sample
                          + bumpyToneCoefficient * bumpyToneState[index];
    const float midBand = sample - bumpyToneState[index];
    const float preEmphasised = sample + safeAmount
        * (0.72f * midBand - 0.10f * bumpyToneState[index]);
    const float driveGain = 1.0f + safeAmount * 12.0f;
    const float driven = preEmphasised * driveGain;
    const float base = std::tanh(driven);
    const float ripple = std::sin(driven * (2.7f + safeAmount * 2.3f)
                                  + 0.35f * std::sin(driven * 1.7f));
    const float rippleEnvelope = safeAmount * 0.16f
        * (1.0f - std::exp(-std::abs(driven) * 0.80f));
    const float asymmetric = base + ripple * rippleEnvelope
                           + safeAmount * 0.055f * base * base;
    const float dcBlocked = asymmetric - bumpyPreviousInput[index]
                          + bumpyDcCoefficient
                            * bumpyPreviousOutput[index];
    bumpyPreviousInput[index] = asymmetric;
    bumpyPreviousOutput[index] = sanitiseSample(dcBlocked, 4.0f);

    const float outputCompensation = 0.92f / (1.0f + safeAmount * 1.05f);
    const float shaped = bumpyPreviousOutput[index] * outputCompensation;
    return sample + (shaped - sample) * safeAmount;
}

float DuskAudioProcessor::processAsymmetricalDrive(
    float sample, float amount, int channel) noexcept
{
    const auto index = distortionStateIndex(channel);
    const float safeAmount = juce::jlimit(0.0f, 1.0f, amount);
    const float driveGain = 1.0f + safeAmount * 11.0f;
    const float bias = 0.035f + safeAmount * 0.20f;
    const float zeroOffset = std::tanh(bias);
    const float positiveScale = juce::jmax(
        0.001f,
        std::tanh(driveGain * (1.0f + safeAmount * 0.55f) + bias)
            - zeroOffset);
    const float negativeScale = juce::jmax(
        0.001f,
        zeroOffset
            - std::tanh(-driveGain * (0.72f + safeAmount * 0.18f) + bias));
    const float shaped = sample >= 0.0f
        ? (std::tanh(sample * driveGain * (1.0f + safeAmount * 0.55f)
                     + bias) - zeroOffset) / positiveScale
        : (std::tanh(sample * driveGain * (0.72f + safeAmount * 0.18f)
                     + bias) - zeroOffset) / negativeScale;
    const float dcBlocked = shaped - asymmetricalPreviousInput[index]
                          + asymmetricalDcCoefficient
                                * asymmetricalPreviousOutput[index];
    asymmetricalPreviousInput[index] = shaped;
    asymmetricalPreviousOutput[index] = sanitiseSample(dcBlocked, 4.0f);

    const float outputCompensation = 0.94f / (1.0f + safeAmount * 0.68f);
    const float output = asymmetricalPreviousOutput[index]
                       * outputCompensation;
    return sample + (output - sample) * safeAmount;
}

float DuskAudioProcessor::processRazorDrive(float sample, float amount,
                                                 int channel) noexcept
{
    const auto index = distortionStateIndex(channel);
    const float safeAmount = juce::jlimit(0.0f, 1.0f, amount);
    razorToneState[index] = (1.0f - razorToneCoefficient) * sample
                          + razorToneCoefficient * razorToneState[index];
    const float edge = sample - razorToneState[index];
    const float preEmphasised = sample
                              + edge * safeAmount * (1.2f + safeAmount * 2.4f);
    const float driveGain = 1.0f + safeAmount * 19.0f;
    const float driven = preEmphasised * driveGain;
    const float sharp = std::tanh(driven)
                      + std::sin(driven * (2.6f + safeAmount * 1.8f))
                            * safeAmount * 0.15f;
    const float dcBlocked = sharp - razorPreviousInput[index]
                          + razorDcCoefficient * razorPreviousOutput[index];
    razorPreviousInput[index] = sharp;
    razorPreviousOutput[index] = sanitiseSample(dcBlocked, 4.0f);

    const float outputCompensation = 0.88f / (1.0f + safeAmount * 1.10f);
    const float output = razorPreviousOutput[index] * outputCompensation;
    return sample + (output - sample) * safeAmount;
}

float DuskAudioProcessor::processPixelatedDrive(
    float sample, float amount, int channel) noexcept
{
    const auto index = distortionStateIndex(channel);
    const float safeAmount = juce::jlimit(0.0f, 1.0f, amount);
    const float holdRate = 1.0f - 0.94f * safeAmount * safeAmount;
    pixelatedPhase[index] += holdRate;
    if (pixelatedPhase[index] >= 1.0f)
    {
        pixelatedPhase[index] -= 1.0f;
        pixelatedHeldSample[index] = sample;
    }

    const int bitDepth = juce::jlimit(
        4, 16, juce::roundToInt(16.0f - safeAmount * 12.0f));
    const float quantisationLevels = static_cast<float>(
        (1u << static_cast<unsigned int>(bitDepth - 1)) - 1u);
    const float dither = nextNoiseSample(pixelatedNoiseState[index])
                       * safeAmount * 0.35f / quantisationLevels;
    const float quantised = std::round(
        (pixelatedHeldSample[index] + dither) * quantisationLevels)
        / quantisationLevels;
    const float output = sanitiseSample(
        quantised / (1.0f + safeAmount * 0.08f), 2.0f);
    return sample + (output - sample) * safeAmount;
}

float DuskAudioProcessor::processCombDrive(float sample, float amount,
                                                int channel) noexcept
{
    const auto index = distortionStateIndex(channel);
    const float safeAmount = juce::jlimit(0.0f, 1.0f, amount);
    const size_t combLength = static_cast<size_t>(juce::jlimit(
        2, static_cast<int>(distortionCombBufferSize - 1),
        juce::roundToInt(47.0f - safeAmount * 36.0f)
            + static_cast<int>(index) * 3));
    const size_t write = distortionCombWritePosition[index];
    const size_t read = (write + distortionCombBufferSize - combLength)
                      % distortionCombBufferSize;
    const float resonantSample = distortionCombBuffer[index][read];
    distortionCombDampingState[index] =
        (1.0f - distortionCombDampingCoefficient) * resonantSample
        + distortionCombDampingCoefficient
            * distortionCombDampingState[index];
    const float resonance = 0.15f + safeAmount * 0.70f;
    distortionCombBuffer[index][write] = std::tanh(
        sample + distortionCombDampingState[index] * resonance);
    distortionCombWritePosition[index] =
        (write + 1u) % distortionCombBufferSize;

    const float driveGain = 1.0f + safeAmount * 2.5f;
    const float resonant = sample
        + distortionCombDampingState[index] * (0.30f + safeAmount * 0.60f);
    const float normaliser = juce::jmax(0.001f, std::tanh(driveGain));
    const float shaped = std::tanh(resonant * driveGain) / normaliser;
    const float output = shaped / (1.0f + safeAmount * 0.38f);
    return sample + (output - sample) * safeAmount;
}

float DuskAudioProcessor::processStressedDrive(
    float sample, float amount, int channel) noexcept
{
    const auto index = distortionStateIndex(channel);
    const float safeAmount = juce::jlimit(0.0f, 1.0f, amount);
    const float driveGain = 1.0f + safeAmount * 21.0f;
    const float driven = sample * driveGain;
    const float compressed = std::tanh(driven);
    const float folded = std::sin(
        driven * (1.25f + safeAmount * 0.75f));
    const float stressed = compressed * (1.0f - safeAmount * 0.30f)
                         + folded * safeAmount * 0.30f
                         + compressed * compressed * safeAmount * 0.045f;
    const float dcBlocked = stressed - stressedPreviousInput[index]
                          + stressedDcCoefficient
                                * stressedPreviousOutput[index];
    stressedPreviousInput[index] = stressed;
    stressedPreviousOutput[index] = sanitiseSample(dcBlocked, 4.0f);

    const float outputCompensation = 0.90f / (1.0f + safeAmount * 1.12f);
    const float output = stressedPreviousOutput[index] * outputCompensation;
    return sample + (output - sample) * safeAmount;
}

float DuskAudioProcessor::processSamuraiDrive(
    float sample, float amount, int channel) noexcept
{
    const auto index = distortionStateIndex(channel);
    const float safeAmount = juce::jlimit(0.0f, 1.0f, amount);
    samuraiToneState[index] = (1.0f - samuraiToneCoefficient) * sample
                            + samuraiToneCoefficient
                                * samuraiToneState[index];
    const float edge = sample - samuraiToneState[index];
    const float driven = (sample + edge * safeAmount * 2.8f)
                       * (1.0f + safeAmount * 15.0f);
    const float deadZone = 0.035f + safeAmount * 0.10f;
    const float slicedMagnitude = juce::jmax(
        0.0f, std::abs(driven) - deadZone);
    const float sliced = std::copysign(slicedMagnitude, driven);
    const float blade = std::tanh(sliced * (2.8f + safeAmount * 5.5f))
                      + std::sin(driven * 4.6f) * safeAmount * 0.10f;
    const float dcBlocked = blade - samuraiPreviousInput[index]
                          + samuraiDcCoefficient
                                * samuraiPreviousOutput[index];
    samuraiPreviousInput[index] = blade;
    samuraiPreviousOutput[index] = sanitiseSample(dcBlocked, 4.0f);

    const float outputCompensation = 0.87f / (1.0f + safeAmount * 0.78f);
    const float output = samuraiPreviousOutput[index] * outputCompensation;
    return sample + (output - sample) * safeAmount;
}

float DuskAudioProcessor::processFreshDrive(float sample, float amount,
                                                 int channel) noexcept
{
    const auto index = distortionStateIndex(channel);
    const float safeAmount = juce::jlimit(0.0f, 1.0f, amount);
    freshToneState[index] = (1.0f - freshToneCoefficient) * sample
                          + freshToneCoefficient * freshToneState[index];
    const float highBand = sample - freshToneState[index];
    const float exciterGain = 2.0f + safeAmount * 10.0f;
    const float exciterNormaliser = juce::jmax(
        0.001f, std::tanh(exciterGain));
    const float excitedHigh = std::tanh(highBand * exciterGain)
                           / exciterNormaliser;
    const float brightened = sample
        + excitedHigh * safeAmount * (0.32f + safeAmount * 0.40f);
    const float outputDrive = 1.0f + safeAmount * 0.55f;
    const float outputNormaliser = juce::jmax(
        0.001f, std::tanh(outputDrive));
    const float output = std::tanh(brightened * outputDrive)
                       / outputNormaliser
                       / (1.0f + safeAmount * 0.12f);
    return sample + (sanitiseSample(output, 2.0f) - sample) * safeAmount;
}

float DuskAudioProcessor::processHardClipDrive(float sample,
                                                    float amount) noexcept
{
    const float safeAmount = juce::jlimit(0.0f, 1.0f, amount);
    if (safeAmount <= 0.0f)
        return sample;

    const float inputGain = 1.0f + safeAmount * 19.0f;
    const float clipped = juce::jlimit(-1.0f, 1.0f, sample * inputGain);
    const float outputCompensation = 1.0f / (1.0f + safeAmount * 0.80f);
    const float shaped = clipped * outputCompensation;
    return sample + (shaped - sample) * safeAmount;
}

float DuskAudioProcessor::processFuzzDrive(float sample, float amount,
                                                int channel) noexcept
{
    const auto index = distortionStateIndex(channel);
    const float safeAmount = juce::jlimit(0.0f, 1.0f, amount);
    if (safeAmount <= 0.0f)
        return sample;

    // A deliberately compressed, asymmetric fuzz stage rather than another
    // generic clipper.  The unequal rails create even harmonics, the fixed
    // post-clip tone removes brittle square-wave fizz, and DC removal keeps
    // the bias from entering the effect input.
    const float inputGain = 3.0f + safeAmount * 47.0f;
    const float bias = 0.015f + safeAmount * 0.12f;
    const float positiveRail = 0.70f - safeAmount * 0.18f;
    const float negativeRail = -(0.50f - safeAmount * 0.10f);
    const float driven = sample * inputGain + bias;
    const float clipped = juce::jlimit(
        negativeRail, positiveRail, driven);
    const float zeroOffset = juce::jlimit(
        negativeRail, positiveRail, bias);
    const float asymmetric = (clipped - zeroOffset)
        / juce::jmax(0.001f, juce::jmax(positiveRail, -negativeRail));

    fuzzToneState[index] = (1.0f - fuzzToneCoefficient) * asymmetric
                         + fuzzToneCoefficient * fuzzToneState[index];
    const float dcBlocked = fuzzToneState[index] - fuzzPreviousInput[index]
                          + fuzzDcCoefficient * fuzzPreviousOutput[index];
    fuzzPreviousInput[index] = fuzzToneState[index];
    fuzzPreviousOutput[index] = sanitiseSample(dcBlocked, 4.0f);

    const float outputCompensation = 0.78f
        / (1.0f + safeAmount * 0.30f);
    const float shaped = fuzzPreviousOutput[index] * outputCompensation;
    return sample + (shaped - sample) * safeAmount;
}

size_t DuskAudioProcessor::distortionStateIndex(int channel) const noexcept
{
    const int safeEngine = juce::jlimit(
        0, filterEngineCount - 1, activeDistortionEngine);
    const int safeChannel = juce::jlimit(0, channelCount - 1, channel);
    return static_cast<size_t>(safeEngine * channelCount + safeChannel);
}

float DuskAudioProcessor::processDistortionType(
    float sample, float amount, int engine, int channel,
    int distortionType) noexcept
{
    activeDistortionEngine = juce::jlimit(
        0, filterEngineCount - 1, engine);
    // Several stateful distortion models still perform filters, noise and
    // nonlinear functions when their wet amount is exactly zero.  Their
    // output is the untouched sample in that state, so avoid dispatching the
    // inactive model altogether.  The smoothed Drive value still exercises
    // the full path as soon as it moves above zero.
    if (amount <= 0.0f)
        return sample;

    switch (juce::jlimit(0, distortionTypeCount - 1, distortionType))
    {
        case 0:  return applyInputDrive(sample, amount, 0);
        case 1:  return applyInputDrive(sample, amount, 1);
        case 2:  return applyInputDrive(sample, amount, 2);
        case 3:  return processRectifyDrive(sample, amount, channel);
        case 4:  return processCrackleDrive(sample, amount, channel);
        case 5:  return processLightningDrive(sample, amount, channel);
        case 6:  return processAmpDrive(sample, amount, channel);
        case 7:  return processBumpyDrive(sample, amount, channel);
        case 8:  return processAsymmetricalDrive(sample, amount, channel);
        case 9:  return processRazorDrive(sample, amount, channel);
        case 10: return processPixelatedDrive(sample, amount, channel);
        case 11: return processCombDrive(sample, amount, channel);
        case 12: return processStressedDrive(sample, amount, channel);
        case 13: return processSamuraiDrive(sample, amount, channel);
        case 14: return processFreshDrive(sample, amount, channel);
        case 15: return processHardClipDrive(sample, amount);
        case 16: return processFuzzDrive(sample, amount, channel);
        default: break;
    }

    return sample;
}

void DuskAudioProcessor::resetDistortionState() noexcept
{
    rectifyPreviousInput.fill(0.0f);
    rectifyPreviousOutput.fill(0.0f);
    crackleBurst.fill(0.0f);
    cracklePreviousInput.fill(0.0f);
    cracklePreviousOutput.fill(0.0f);
    crackleNoiseState = {
        0xa511e9b3u, 0x63d83595u, 0x9e3779b9u, 0x7f4a7c15u };
    lightningPreviousInput.fill(0.0f);
    lightningPreviousOutput.fill(0.0f);
    lightningNoiseState = {
        0x27d4eb2du, 0x165667b1u, 0x85ebca6bu, 0xc2b2ae35u };
    ampLowpassState.fill(0.0f);
    ampPreviousInput.fill(0.0f);
    ampPreviousOutput.fill(0.0f);
    bumpyToneState.fill(0.0f);
    bumpyPreviousInput.fill(0.0f);
    bumpyPreviousOutput.fill(0.0f);
    asymmetricalPreviousInput.fill(0.0f);
    asymmetricalPreviousOutput.fill(0.0f);
    razorToneState.fill(0.0f);
    razorPreviousInput.fill(0.0f);
    razorPreviousOutput.fill(0.0f);
    pixelatedHeldSample.fill(0.0f);
    pixelatedPhase.fill(1.0f);
    pixelatedNoiseState = {
        0x94d049bbu, 0x369dea0fu, 0xd1b54a35u, 0x8cb92ba7u };
    distortionCombBuffer = {};
    distortionCombWritePosition.fill(0u);
    distortionCombDampingState.fill(0.0f);
    stressedPreviousInput.fill(0.0f);
    stressedPreviousOutput.fill(0.0f);
    samuraiToneState.fill(0.0f);
    samuraiPreviousInput.fill(0.0f);
    samuraiPreviousOutput.fill(0.0f);
    freshToneState.fill(0.0f);
    fuzzToneState.fill(0.0f);
    fuzzPreviousInput.fill(0.0f);
    fuzzPreviousOutput.fill(0.0f);
}

