#include "PluginProcessor.h"
#include <cmath>

namespace
{
constexpr int feedbackCombMode = 0;
constexpr int feedforwardCombMode = 1;
constexpr int allPassCombMode = 2;
constexpr int firstCombDistortionStateBank = 4;
constexpr float maximumCombFeedback = 0.98f;
constexpr float maximumCombOutputAmount = 1.35f;
constexpr float maximumCombFrequency = 8000.0f;
constexpr float maximumDispersionCoefficient = 0.72f;
constexpr float maximumLowCutFrequency = 45.0f;
constexpr float maximumLowCutMix = 0.45f;

float lowFrequencyProtection(float frequencyHz, float feedback) noexcept
{
    const float feedbackAmount = juce::jlimit(
        0.0f, 1.0f, (feedback - 0.70f) / 0.25f);
    const float lowFrequencyAmount = juce::jlimit(
        0.0f, 1.0f, (320.0f - frequencyHz) / 280.0f);
    return feedbackAmount * lowFrequencyAmount;
}

template <typename Float>
Float combDampingCutoff(Float sampleRate, Float damping) noexcept
{
    const Float maximumCutoff = juce::jmin(
        static_cast<Float>(20000), sampleRate * static_cast<Float>(0.45));
    const Float shapedDamping = std::pow(
        juce::jlimit(static_cast<Float>(0), static_cast<Float>(1), damping),
        static_cast<Float>(1.5));
    return juce::jlimit(
        static_cast<Float>(20), maximumCutoff,
        maximumCutoff * std::pow(static_cast<Float>(0.035), shapedDamping));
}

template <typename Float>
Float combOutputAmount(Float feedback, Float polarity) noexcept
{
    const Float amount = juce::jlimit(
        static_cast<Float>(0), static_cast<Float>(1), feedback);
    const Float sign = juce::jlimit(
        static_cast<Float>(-1), static_cast<Float>(1), polarity);
    return static_cast<Float>(maximumCombOutputAmount)
        * std::pow(amount, static_cast<Float>(0.65)) * sign;
}
}

void DuskAudioProcessor::prepareCombState()
{
    const auto requiredSize = static_cast<size_t>(juce::jmax(
        8, juce::roundToInt(std::ceil(currentSampleRate / 20.0)) + 8));
    for (auto& modeBuffers : combDelayBuffer)
        for (auto& buffer : modeBuffers)
            buffer.assign(requiredSize, 0.0f);
    resetCombState();
}

void DuskAudioProcessor::resetCombState() noexcept
{
    for (auto& modeBuffers : combDelayBuffer)
        for (auto& buffer : modeBuffers)
            std::fill(buffer.begin(), buffer.end(), 0.0f);
    for (auto& positions : combWritePosition)
        positions.fill(0u);
    for (auto& states : combDampingState)
        states.fill(0.0f);
    for (auto& states : combFractionalState)
        states.fill(0.0f);
    for (auto& states : combDispersionState)
        states.fill(0.0f);
    for (auto& states : combLowCutState)
        states.fill(0.0f);
}

std::complex<double> DuskAudioProcessor::getCombResponse(
    double frequencyHz, double sampleRate, float combFrequencyHz,
    float feedbackPercent, float dampingPercent,
    float fineTuneCents, float polarityPercent,
    float dispersionPercent, int modeChoice) noexcept
{
    const double safeSampleRate = juce::jmax(1.0, sampleRate);
    const double maximumFrequency = juce::jmin(
        static_cast<double>(maximumCombFrequency), safeSampleRate / 2.2);
    const double tunedFrequency = juce::jlimit(
        20.0, maximumFrequency,
        static_cast<double>(combFrequencyHz)
            * std::pow(2.0, static_cast<double>(fineTuneCents) / 1200.0));
    const double delaySamples = juce::jlimit(
        2.0, safeSampleRate / 20.0,
        safeSampleRate / tunedFrequency);
    const int integerDelay = juce::jmax(
        2, static_cast<int>(std::floor(delaySamples)));
    const double fraction = delaySamples - static_cast<double>(integerDelay);
    const double omega = juce::MathConstants<double>::twoPi
        * juce::jlimit(0.0, safeSampleRate * 0.5, frequencyHz)
        / safeSampleRate;
    const auto unitDelay = std::polar(1.0, -omega);
    const double fractionalCoefficient = fraction > 1.0e-9
        ? (1.0 - fraction) / (1.0 + fraction) : 1.0;
    const auto fractionalDelay = fraction > 1.0e-9
        ? (fractionalCoefficient + unitDelay)
            / (1.0 + fractionalCoefficient * unitDelay)
        : std::complex<double>(1.0, 0.0);
    const auto delay = std::pow(unitDelay, integerDelay)
        * fractionalDelay;

    const double damping = juce::jlimit(
        0.0, 1.0, static_cast<double>(dampingPercent) * 0.01);
    const double dampingCutoff = combDampingCutoff(
        safeSampleRate, damping);
    const double dampingCoefficient = 1.0 - std::exp(
        -juce::MathConstants<double>::twoPi
            * dampingCutoff / safeSampleRate);
    const auto dampingResponse = dampingCoefficient
        / (1.0 - (1.0 - dampingCoefficient) * unitDelay);

    const double dispersion = juce::jlimit(
        0.0, 1.0, static_cast<double>(dispersionPercent) * 0.01);
    constexpr double allPassCoefficient = maximumDispersionCoefficient;
    const auto allPass = (-allPassCoefficient + unitDelay)
        / (1.0 - allPassCoefficient * unitDelay);
    const auto dispersionResponse = 1.0 + dispersion * (allPass - 1.0);
    const double protection = lowFrequencyProtection(
        static_cast<float>(tunedFrequency),
        juce::jlimit(0.0f, 1.0f, feedbackPercent * 0.01f));
    const double lowCutFrequency = 8.0
        + protection * (maximumLowCutFrequency - 8.0);
    const double lowCutCoefficient = 1.0 - std::exp(
        -juce::MathConstants<double>::twoPi
            * lowCutFrequency / safeSampleRate);
    const auto lowPassResponse = lowCutCoefficient
        / (1.0 - (1.0 - lowCutCoefficient) * unitDelay);
    const auto highPassResponse = 1.0 - lowPassResponse;
    const auto lowCutResponse = 1.0
        + protection * maximumLowCutMix * (highPassResponse - 1.0);
    const auto delayedBranch = delay * dampingResponse
        * dispersionResponse * lowCutResponse;
    const double signedFeedback = maximumCombFeedback
        * juce::jlimit(0.0, 1.0,
            static_cast<double>(feedbackPercent) * 0.01)
        * juce::jlimit(-1.0, 1.0,
            static_cast<double>(polarityPercent) * 0.01);
    const double audibleAmount = combOutputAmount(
        juce::jlimit(0.0, 1.0,
            static_cast<double>(feedbackPercent) * 0.01),
        juce::jlimit(-1.0, 1.0,
            static_cast<double>(polarityPercent) * 0.01));

    switch (juce::jlimit(feedbackCombMode, allPassCombMode, modeChoice))
    {
        case feedforwardCombMode:
            return 1.0 + audibleAmount * delayedBranch;
        case allPassCombMode:
        {
            const auto denominator = 1.0 - signedFeedback * delayedBranch;
            return std::abs(denominator) > 1.0e-12
                ? (-signedFeedback + delayedBranch) / denominator
                : std::complex<double>(1.0, 0.0);
        }
        case feedbackCombMode:
        default:
        {
            const auto denominator = 1.0 - signedFeedback * delayedBranch;
            return std::abs(denominator) > 1.0e-12
                ? 1.0 + audibleAmount * delayedBranch / denominator
                : std::complex<double>(1.0, 0.0);
        }
    }
}

float DuskAudioProcessor::processCombFilter(
    float sample, int engine, int channel, int previousMode,
    int currentMode, float modeCrossfade, float frequencyHz,
    float feedback, float damping, float fineTuneCents,
    float polarity, float stereoSpread, float dispersion,
    float loopSaturation, float feedbackLimit,
    bool internalDistortion, float distortionAmount,
    int currentDistortion, int previousDistortion,
    float distortionCrossfade) noexcept
{
    const int safeEngine = juce::jlimit(0, filterEngineCount - 1, engine);
    const int safeChannel = juce::jlimit(0, channelCount - 1, channel);
    const auto stateIndex = static_cast<size_t>(
        safeEngine * channelCount + safeChannel);
    const float sampleRate = static_cast<float>(
        juce::jmax(1.0, currentSampleRate));
    const float channelOffset = channelCount > 1
        ? (safeChannel == 0 ? -0.5f : 0.5f) * stereoSpread
        : 0.0f;
    const float tunedFrequency = juce::jlimit(
        20.0f, juce::jmin(maximumCombFrequency, sampleRate / 2.2f),
        frequencyHz * std::pow(
            2.0f, (fineTuneCents + channelOffset) / 1200.0f));
    const float targetDelaySamples = juce::jlimit(
        2.0f, sampleRate / 20.0f, sampleRate / tunedFrequency);
    const int integerDelay = juce::jmax(
        2, static_cast<int>(std::floor(targetDelaySamples)));
    const float delayFraction = targetDelaySamples
        - static_cast<float>(integerDelay);
    const float safeDamping = juce::jlimit(0.0f, 1.0f, damping);
    const float dampingCutoff = combDampingCutoff(
        sampleRate, safeDamping);
    const float dampingCoefficient = 1.0f - std::exp(
        -juce::MathConstants<float>::twoPi
            * dampingCutoff / sampleRate);
    const float safeDispersion = juce::jlimit(
        0.0f, 1.0f, dispersion);
    const float signedFeedback = maximumCombFeedback
        * juce::jlimit(0.0f, 1.0f, feedback)
        * juce::jlimit(-1.0f, 1.0f, polarity);
    const float audibleAmount = combOutputAmount(
        juce::jlimit(0.0f, 1.0f, feedback),
        juce::jlimit(-1.0f, 1.0f, polarity));
    const float protection = lowFrequencyProtection(
        tunedFrequency, juce::jlimit(0.0f, 1.0f, feedback));
    const float lowCutFrequency = 8.0f
        + protection * (maximumLowCutFrequency - 8.0f);
    const float lowCutCoefficient = 1.0f - std::exp(
        -juce::MathConstants<float>::twoPi
            * lowCutFrequency / sampleRate);
    const float saturationCharacter = std::pow(
        juce::jlimit(0.0f, 1.0f, loopSaturation), 1.5f);
    const float saturationDrive = 1.0f + 31.0f * saturationCharacter;
    const float feedbackThreshold = 0.10f * std::pow(
        80.0f, juce::jlimit(0.0f, 1.0f, feedbackLimit));
    std::array<float, combModeCount> modeOutputs {};

    for (int mode = 0; mode < combModeCount; ++mode)
    {
        auto& buffer = combDelayBuffer[static_cast<size_t>(mode)][stateIndex];
        if (buffer.size() < 8u)
        {
            modeOutputs[static_cast<size_t>(mode)] = sample;
            continue;
        }
        auto& writePosition = combWritePosition[
            static_cast<size_t>(mode)][stateIndex];
        const size_t size = buffer.size();
        const size_t newerIndex = (writePosition + size
            - static_cast<size_t>(integerDelay) % size) % size;
        const float baseDelayed = buffer[newerIndex];
        auto& fractionalState = combFractionalState[
            static_cast<size_t>(mode)][stateIndex];
        float delayed = baseDelayed;
        if (delayFraction > 1.0e-5f)
        {
            const float fractionalCoefficient =
                (1.0f - delayFraction) / (1.0f + delayFraction);
            delayed = fractionalCoefficient * baseDelayed
                + fractionalState;
            fractionalState = sanitiseSample(
                baseDelayed - fractionalCoefficient * delayed, 64.0f);
        }
        else
            fractionalState = 0.0f;

        auto& dampingState = combDampingState[
            static_cast<size_t>(mode)][stateIndex];
        dampingState = sanitiseSample(
            dampingState + dampingCoefficient * (delayed - dampingState),
            64.0f);
        auto& dispersionState = combDispersionState[
            static_cast<size_t>(mode)][stateIndex];
        const float allPass = sanitiseSample(
            -maximumDispersionCoefficient * dampingState
                + dispersionState,
            64.0f);
        dispersionState = sanitiseSample(
            dampingState + maximumDispersionCoefficient * allPass, 64.0f);
        float loopSignal = dampingState
            + (allPass - dampingState) * safeDispersion;
        auto& lowCutState = combLowCutState[
            static_cast<size_t>(mode)][stateIndex];
        lowCutState = sanitiseSample(
            lowCutState + lowCutCoefficient * (loopSignal - lowCutState),
            64.0f);
        const float highPassed = loopSignal - lowCutState;
        loopSignal += (highPassed - loopSignal)
            * (protection * maximumLowCutMix);
        if (internalDistortion)
            loopSignal = processSelectedDistortion(
                loopSignal, distortionAmount, safeEngine, safeChannel,
                currentDistortion, previousDistortion,
                distortionCrossfade,
                firstCombDistortionStateBank + mode);
        if (saturationCharacter > 1.0e-6f)
        {
            const float saturated = std::tanh(
                saturationDrive * loopSignal) / saturationDrive;
            loopSignal += (saturated - loopSignal) * saturationCharacter;
        }
        loopSignal = feedbackThreshold * std::tanh(
            loopSignal / feedbackThreshold);

        float output = sample;
        float writeValue = sample;
        if (mode == feedbackCombMode)
        {
            output = sample + audibleAmount * loopSignal;
            writeValue = sample + signedFeedback * loopSignal;
        }
        else if (mode == feedforwardCombMode)
        {
            output = sample + audibleAmount * loopSignal;
        }
        else
        {
            output = -signedFeedback * sample + loopSignal;
            writeValue = sample + signedFeedback * output;
        }
        output = sanitiseSample(output, 64.0f);
        buffer[writePosition] = sanitiseSample(writeValue, 64.0f);
        writePosition = (writePosition + 1u) % size;
        modeOutputs[static_cast<size_t>(mode)] = output;
    }

    const int safePreviousMode = juce::jlimit(
        feedbackCombMode, allPassCombMode, previousMode);
    const int safeCurrentMode = juce::jlimit(
        feedbackCombMode, allPassCombMode, currentMode);
    const float blend = juce::jlimit(0.0f, 1.0f, modeCrossfade);
    const float previousOutput = modeOutputs[
        static_cast<size_t>(safePreviousMode)];
    const float currentOutput = modeOutputs[
        static_cast<size_t>(safeCurrentMode)];
    return sanitiseSample(
        previousOutput + (currentOutput - previousOutput) * blend, 64.0f);
}
