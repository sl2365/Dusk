#include "PluginProcessor.h"
#include <cmath>

namespace
{
constexpr int phaserStageCount = 12;
constexpr int phaserTapCount = phaserStageCount / 2;
constexpr int phaserInternalDistortionStateBank = 10;

template <typename Float>
Float phaserStageFrequency(
    Float sampleRate, Float centreFrequencyHz, Float spreadPercent,
    Float skewPercent, Float stereoPercent, int channel,
    int stageIndex) noexcept
{
    const int pair = stageIndex / 2;
    const Float direction = (stageIndex & 1) == 0
        ? static_cast<Float>(-1) : static_cast<Float>(1);
    const Float radius = (static_cast<Float>(pair) + static_cast<Float>(0.5))
        / static_cast<Float>(phaserTapCount);
    const Float skew = juce::jlimit(
        static_cast<Float>(-1), static_cast<Float>(1),
        skewPercent * static_cast<Float>(0.01));
    const Float skewExponent = std::pow(static_cast<Float>(2), skew);
    const Float warpedRadius = std::pow(radius, skewExponent);
    const Float spread = juce::jlimit(
        static_cast<Float>(0), static_cast<Float>(1),
        spreadPercent * static_cast<Float>(0.01));
    const Float halfSpanOctaves = static_cast<Float>(0.15)
        + spread * static_cast<Float>(2.85);
    const Float stereo = juce::jlimit(
        static_cast<Float>(-1), static_cast<Float>(1),
        stereoPercent * static_cast<Float>(0.01));
    const Float stereoDirection = channel == 0
        ? static_cast<Float>(-1) : static_cast<Float>(1);
    const Float stereoOffset = stereoDirection * stereo
        * static_cast<Float>(0.75);
    const Float ratio = std::pow(
        static_cast<Float>(2),
        direction * warpedRadius * halfSpanOctaves + stereoOffset);
    return juce::jlimit(
        static_cast<Float>(20),
        juce::jmin(static_cast<Float>(20000),
                   sampleRate * static_cast<Float>(0.45)),
        centreFrequencyHz * ratio);
}

template <typename Float>
Float phaserFeedbackGain(Float feedbackPercent) noexcept
{
    return juce::jlimit(
        static_cast<Float>(-0.86), static_cast<Float>(0.86),
        feedbackPercent * (static_cast<Float>(0.86)
                           / static_cast<Float>(95)));
}

template <typename Float>
Float phaserFeedbackSafetyLimit(Float sample) noexcept
{
    constexpr Float ceiling = static_cast<Float>(2);
    return ceiling * std::tanh(sample / ceiling);
}

template <typename Float>
Float phaserFeedbackLoopNormalisation(
    Float feedbackGain, Float stages, Float saturation,
    Float saturationDrive, Float saturationScale) noexcept
{
    const Float stageSmallSignalGain =
        (static_cast<Float>(1) - saturation)
        + saturation * saturationDrive * saturationScale;
    const Float activeStages = juce::jlimit(
        static_cast<Float>(2), static_cast<Float>(12), stages);
    const Float stagePosition = (activeStages - static_cast<Float>(2))
        * static_cast<Float>(0.5);
    const int lower = juce::jlimit(
        0, phaserTapCount - 1,
        static_cast<int>(std::floor(stagePosition)));
    const int upper = juce::jmin(phaserTapCount - 1, lower + 1);
    const Float fraction = stagePosition - static_cast<Float>(lower);
    const Float safeStageGain = juce::jmax(
        static_cast<Float>(1), stageSmallSignalGain);
    const Float lowerGain = std::pow(
        safeStageGain, static_cast<Float>((lower + 1) * 2));
    const Float upperGain = std::pow(
        safeStageGain, static_cast<Float>((upper + 1) * 2));
    const Float selectedGain = lowerGain
        + (upperGain - lowerGain) * fraction;
    const Float rawLoopGain = std::abs(feedbackGain) * selectedGain;
    constexpr Float knee = static_cast<Float>(0.86);
    constexpr Float ceiling = static_cast<Float>(0.90);
    if (rawLoopGain <= knee)
        return static_cast<Float>(1);

    const Float span = ceiling - knee;
    const Float safeLoopGain = knee + span * std::tanh(
        (rawLoopGain - knee) / span);
    return safeLoopGain / juce::jmax(
        static_cast<Float>(0.000001), rawLoopGain);
}

template <typename Float>
Float phaserDampingPole(
    Float sampleRate, Float dampingPercent) noexcept
{
    const Float amount = juce::jlimit(
        static_cast<Float>(0), static_cast<Float>(1),
        dampingPercent * static_cast<Float>(0.01));
    const Float cutoff = static_cast<Float>(18000) * std::pow(
        static_cast<Float>(1000.0 / 18000.0), amount);
    return std::exp(-juce::MathConstants<Float>::twoPi
                    * cutoff / sampleRate);
}
}

std::complex<double> DuskAudioProcessor::getPhaserResponse(
    double frequencyHz, double sampleRate, float centreFrequencyHz,
    float feedbackPercent, float depthPercent, float stages,
    float spreadPercent, float skewPercent,
    float dampingPercent) noexcept
{
    const double safeRate = juce::jmax(1.0, sampleRate);
    const double omega = juce::MathConstants<double>::twoPi
        * juce::jlimit(0.0, safeRate * 0.5, frequencyHz) / safeRate;
    const auto zInverse = std::polar(1.0, -omega);
    std::array<std::complex<double>, phaserTapCount> taps {};
    std::complex<double> cascade { 1.0, 0.0 };
    for (int stage = 0; stage < phaserStageCount; ++stage)
    {
        const double stageFrequency = phaserStageFrequency(
            safeRate, static_cast<double>(centreFrequencyHz),
            static_cast<double>(spreadPercent),
            static_cast<double>(skewPercent), 0.0, 0, stage);
        const double g = std::tan(
            juce::MathConstants<double>::pi * stageFrequency / safeRate);
        const double coefficient = (1.0 - g) / (1.0 + g);
        const auto denominator = 1.0 + coefficient * zInverse;
        if (std::abs(denominator) < 1.0e-12)
            return {};
        cascade *= (coefficient + zInverse) / denominator;
        if ((stage & 1) != 0)
            taps[static_cast<size_t>(stage / 2)] = cascade;
    }

    const double stagePosition = juce::jlimit(
        0.0, static_cast<double>(phaserTapCount - 1),
        (static_cast<double>(stages) - 2.0) * 0.5);
    const int lower = juce::jlimit(
        0, phaserTapCount - 1, static_cast<int>(std::floor(stagePosition)));
    const int upper = juce::jmin(phaserTapCount - 1, lower + 1);
    const double fraction = stagePosition - static_cast<double>(lower);
    const auto allPass = taps[static_cast<size_t>(lower)]
        + (taps[static_cast<size_t>(upper)]
           - taps[static_cast<size_t>(lower)]) * fraction;
    const double pole = phaserDampingPole(
        safeRate, static_cast<double>(dampingPercent));
    const auto damping = (1.0 - pole) / (1.0 - pole * zInverse);
    const double feedback = phaserFeedbackGain(
        static_cast<double>(feedbackPercent));
    const auto loopDenominator = 1.0
        - feedback * zInverse * damping * allPass;
    const auto closedAllPass = std::abs(loopDenominator) > 1.0e-12
        ? allPass / loopDenominator
        : allPass * 1.0e12;
    const double depth = juce::jlimit(
        0.0, 1.0, static_cast<double>(depthPercent) * 0.01);
    return (1.0 - 0.5 * depth) + (0.5 * depth) * closedAllPass;
}

float DuskAudioProcessor::processPhaserFilter(
    float sample, int engine, int channel, float centreFrequencyHz,
    float feedbackPercent, float depthPercent, float stages,
    float spreadPercent, float skewPercent, float dampingPercent,
    float stageSaturationPercent, float stereoPercent,
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
    const float feedback = phaserFeedbackGain(feedbackPercent);
    float allPassInput = sanitiseSample(
        sample + feedback * phaserFeedbackReturn[stateIndex], 32.0f);
    std::array<float, phaserTapCount> taps {};
    const float saturation = juce::jlimit(
        0.0f, 1.0f, stageSaturationPercent * 0.01f);
    const float saturationDrive = 1.0f
        + 7.0f * std::pow(saturation, 1.5f);
    const float saturationScale = 1.0f
        / juce::jmax(0.001f, std::tanh(saturationDrive));
    const float feedbackLoopNormalisation =
        phaserFeedbackLoopNormalisation(
            feedback, stages, saturation,
            saturationDrive, saturationScale);

    for (int stage = 0; stage < phaserStageCount; ++stage)
    {
        const float stageFrequency = phaserStageFrequency(
            sampleRate, centreFrequencyHz, spreadPercent, skewPercent,
            stereoPercent, safeChannel, stage);
        const float g = std::tan(
            juce::MathConstants<float>::pi
            * stageFrequency / sampleRate);
        const float coefficient = (1.0f - g) / (1.0f + g);
        auto& state = phaserAllPassState[static_cast<size_t>(stage)]
            [stateIndex];
        float output = coefficient * allPassInput + state;
        state = sanitiseSample(allPassInput - coefficient * output, 64.0f);
        if (saturation > 0.0f)
        {
            const float coloured = std::tanh(output * saturationDrive)
                * saturationScale;
            output += saturation * (coloured - output);
        }
        allPassInput = sanitiseSample(output, 32.0f);
        if ((stage & 1) != 0)
            taps[static_cast<size_t>(stage / 2)] = allPassInput;
    }

    const float stagePosition = juce::jlimit(
        0.0f, static_cast<float>(phaserTapCount - 1),
        (stages - 2.0f) * 0.5f);
    const int lower = juce::jlimit(
        0, phaserTapCount - 1,
        static_cast<int>(std::floor(stagePosition)));
    const int upper = juce::jmin(phaserTapCount - 1, lower + 1);
    const float fraction = stagePosition - static_cast<float>(lower);
    const float selected = taps[static_cast<size_t>(lower)]
        + (taps[static_cast<size_t>(upper)]
           - taps[static_cast<size_t>(lower)]) * fraction;

    float feedbackSample = selected;
    if (internalDistortion)
        feedbackSample = processSelectedDistortion(
            feedbackSample, distortionAmount, safeEngine, safeChannel,
            currentDistortion, previousDistortion, distortionCrossfade,
            phaserInternalDistortionStateBank);
    feedbackSample *= feedbackLoopNormalisation;
    const float pole = phaserDampingPole(sampleRate, dampingPercent);
    const float dampedFeedback = sanitiseSample(
        (1.0f - pole) * feedbackSample
            + pole * phaserFeedbackReturn[stateIndex], 32.0f);
    phaserFeedbackReturn[stateIndex] = phaserFeedbackSafetyLimit(
        dampedFeedback);

    const float depth = juce::jlimit(0.0f, 1.0f, depthPercent * 0.01f);
    return sanitiseSample(
        (1.0f - 0.5f * depth) * sample
            + (0.5f * depth) * selected, 64.0f);
}
