#include "PluginProcessor.h"
#include <cmath>

namespace
{
constexpr int formantBandCount = 3;
constexpr int firstFormantDistortionStateBank = 7;
constexpr float formantOutputGain = 0.82f;

struct VowelDefinition
{
    std::array<float, formantBandCount> frequency;
    std::array<float, formantBandCount> bandwidth;
};

// A, E, I, O and U supply stable vocal starting points. Dusk morphs their
// centre frequencies logarithmically so motion between vowels remains smooth.
constexpr std::array<VowelDefinition, 5> vowels
{{
    { { 800.0f, 1150.0f, 2900.0f }, { 80.0f, 90.0f, 120.0f } },
    { { 500.0f, 1750.0f, 2450.0f }, { 80.0f, 90.0f, 120.0f } },
    { { 300.0f, 2200.0f, 3000.0f }, { 60.0f, 100.0f, 120.0f } },
    { { 500.0f, 900.0f, 2400.0f }, { 70.0f, 80.0f, 120.0f } },
    { { 350.0f, 700.0f, 2400.0f }, { 60.0f, 80.0f, 120.0f } }
}};

template <typename Float>
struct FormantSettings
{
    std::array<Float, formantBandCount> frequency {};
    std::array<Float, formantBandCount> q {};
    std::array<Float, formantBandCount> gain {};
};

template <typename Float>
FormantSettings<Float> formantSettings(
    Float sampleRate, Float vowelPercent, Float shiftSemitones,
    Float focusPercent, Float formantOneLevelPercent,
    Float formantTwoLevelPercent, Float formantThreeLevelPercent,
    Float bandwidthPercent, Float spreadPercent, Float tiltPercent) noexcept
{
    const Float vowelPosition = juce::jlimit(
        static_cast<Float>(0), static_cast<Float>(4),
        vowelPercent * static_cast<Float>(0.04));
    const int lowerVowel = juce::jlimit(
        0, 4, static_cast<int>(std::floor(vowelPosition)));
    const int upperVowel = juce::jmin(4, lowerVowel + 1);
    const Float vowelFraction = vowelPosition
        - static_cast<Float>(lowerVowel);
    const Float shiftRatio = std::pow(
        static_cast<Float>(2), shiftSemitones / static_cast<Float>(12));
    const Float spreadOctaves = juce::jlimit(
        static_cast<Float>(-1), static_cast<Float>(1),
        spreadPercent * static_cast<Float>(0.01))
        * static_cast<Float>(0.5);
    const std::array<Float, formantBandCount> spreadRatio
    {{
        std::pow(static_cast<Float>(2), -spreadOctaves),
        static_cast<Float>(1),
        std::pow(static_cast<Float>(2), spreadOctaves)
    }};
    const Float focus = juce::jlimit(
        static_cast<Float>(0), static_cast<Float>(1),
        focusPercent * static_cast<Float>(0.01));
    const Float focusScale = std::pow(
        static_cast<Float>(2), static_cast<Float>(1) - focus * 2);
    const Float focusGain = static_cast<Float>(0.65)
        + focus * static_cast<Float>(0.70);
    const Float widthScale = juce::jlimit(
        static_cast<Float>(0.5), static_cast<Float>(2),
        bandwidthPercent * static_cast<Float>(0.01));
    const Float tilt = juce::jlimit(
        static_cast<Float>(-1), static_cast<Float>(1),
        tiltPercent * static_cast<Float>(0.01));
    const std::array<Float, formantBandCount> tiltGain
    {{
        std::pow(static_cast<Float>(2), -tilt * static_cast<Float>(0.75)),
        static_cast<Float>(1),
        std::pow(static_cast<Float>(2), tilt * static_cast<Float>(0.75))
    }};
    const std::array<Float, formantBandCount> level
    {{
        juce::jlimit(static_cast<Float>(0), static_cast<Float>(2),
                     formantOneLevelPercent * static_cast<Float>(0.01)),
        juce::jlimit(static_cast<Float>(0), static_cast<Float>(2),
                     formantTwoLevelPercent * static_cast<Float>(0.01)),
        juce::jlimit(static_cast<Float>(0), static_cast<Float>(2),
                     formantThreeLevelPercent * static_cast<Float>(0.01))
    }};
    constexpr std::array<float, formantBandCount> naturalGain
        { 1.0f, 0.80f, 0.55f };
    const Float maximumFrequency = juce::jmin(
        static_cast<Float>(16000), sampleRate * static_cast<Float>(0.42));

    FormantSettings<Float> settings;
    for (int band = 0; band < formantBandCount; ++band)
    {
        const auto index = static_cast<size_t>(band);
        const Float lowerFrequency = static_cast<Float>(
            vowels[static_cast<size_t>(lowerVowel)].frequency[index]);
        const Float upperFrequency = static_cast<Float>(
            vowels[static_cast<size_t>(upperVowel)].frequency[index]);
        const Float morphedFrequency = lowerFrequency * std::pow(
            upperFrequency / lowerFrequency, vowelFraction);
        settings.frequency[index] = juce::jlimit(
            static_cast<Float>(40), maximumFrequency,
            morphedFrequency * shiftRatio * spreadRatio[index]);

        const Float lowerBandwidth = static_cast<Float>(
            vowels[static_cast<size_t>(lowerVowel)].bandwidth[index]);
        const Float upperBandwidth = static_cast<Float>(
            vowels[static_cast<size_t>(upperVowel)].bandwidth[index]);
        const Float bandwidth = (lowerBandwidth
            + (upperBandwidth - lowerBandwidth) * vowelFraction)
            * focusScale * widthScale;
        settings.q[index] = juce::jlimit(
            static_cast<Float>(0.35), static_cast<Float>(40),
            settings.frequency[index]
                / juce::jmax(static_cast<Float>(20), bandwidth));
        settings.gain[index] = static_cast<Float>(naturalGain[index])
            * level[index] * tiltGain[index] * focusGain;
    }
    return settings;
}
}

std::complex<double> DuskAudioProcessor::getFormantResponse(
    double frequencyHz, double sampleRate, float vowelPercent,
    float shiftSemitones, float focusPercent,
    float formantOneLevelPercent, float formantTwoLevelPercent,
    float formantThreeLevelPercent, float bandwidthPercent,
    float spreadPercent, float tiltPercent) noexcept
{
    const double safeSampleRate = juce::jmax(1.0, sampleRate);
    const auto settings = formantSettings(
        safeSampleRate, static_cast<double>(vowelPercent),
        static_cast<double>(shiftSemitones),
        static_cast<double>(focusPercent),
        static_cast<double>(formantOneLevelPercent),
        static_cast<double>(formantTwoLevelPercent),
        static_cast<double>(formantThreeLevelPercent),
        static_cast<double>(bandwidthPercent),
        static_cast<double>(spreadPercent),
        static_cast<double>(tiltPercent));
    const double omega = juce::MathConstants<double>::twoPi
        * juce::jlimit(0.0, safeSampleRate * 0.5, frequencyHz)
        / safeSampleRate;
    const auto zInverse = std::polar(1.0, -omega);
    const auto bilinearNumerator = 1.0 - zInverse;
    const auto bilinearDenominator = 1.0 + zInverse;
    std::complex<double> response {};
    if (std::abs(bilinearDenominator) < 1.0e-12)
        return response;

    for (int band = 0; band < formantBandCount; ++band)
    {
        const auto index = static_cast<size_t>(band);
        const double g = std::tan(
            juce::MathConstants<double>::pi
            * settings.frequency[index] / safeSampleRate);
        const double k = 1.0 / settings.q[index];
        const auto s = bilinearNumerator
            / (g * bilinearDenominator);
        const auto denominator = s * s + k * s + 1.0;
        if (std::abs(denominator) > 1.0e-12)
            response += settings.gain[index]
                * (k * s / denominator);
    }
    return static_cast<double>(formantOutputGain) * response;
}

float DuskAudioProcessor::processFormantFilter(
    float sample, int engine, int channel, float vowelPercent,
    float shiftSemitones, float focusPercent,
    float formantOneLevelPercent, float formantTwoLevelPercent,
    float formantThreeLevelPercent, float bandwidthPercent,
    float spreadPercent, float tiltPercent,
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
    const auto settings = formantSettings(
        sampleRate, vowelPercent, shiftSemitones, focusPercent,
        formantOneLevelPercent, formantTwoLevelPercent,
        formantThreeLevelPercent, bandwidthPercent, spreadPercent,
        tiltPercent);
    float output = 0.0f;
    for (int band = 0; band < formantBandCount; ++band)
    {
        const auto index = static_cast<size_t>(band);
        const float g = std::tan(
            juce::MathConstants<float>::pi
            * settings.frequency[index] / sampleRate);
        const float k = 1.0f / settings.q[index];
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        auto& integratorOne = formantIntegratorOne[index][stateIndex];
        auto& integratorTwo = formantIntegratorTwo[index][stateIndex];
        const float v3 = sample - integratorTwo;
        const float v1 = a1 * integratorOne + a2 * v3;
        const float v2 = integratorTwo + a2 * integratorOne + a3 * v3;
        integratorOne = sanitiseSample(2.0f * v1 - integratorOne, 64.0f);
        integratorTwo = sanitiseSample(2.0f * v2 - integratorTwo, 64.0f);
        float bandOutput = sanitiseSample(k * v1, 16.0f);
        if (internalDistortion)
            bandOutput = sanitiseSample(processSelectedDistortion(
                bandOutput, distortionAmount, safeEngine, safeChannel,
                currentDistortion, previousDistortion,
                distortionCrossfade,
                firstFormantDistortionStateBank + band), 16.0f);
        output += settings.gain[index] * bandOutput;
    }
    return sanitiseSample(formantOutputGain * output, 64.0f);
}
