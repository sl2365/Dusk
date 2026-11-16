#include "PluginProcessor.h"

#include <array>
#include <cmath>

namespace
{
constexpr int spectralInternalDistortionStateBank = 24;
constexpr std::array<int, 8> spectralActivationRank
    { 6, 4, 2, 0, 1, 3, 5, 7 };

template <typename Float>
Float spectralQ(Float widthPercent) noexcept
{
    const Float width = juce::jlimit(
        static_cast<Float>(0), static_cast<Float>(1),
        widthPercent * static_cast<Float>(0.01));
    return static_cast<Float>(0.55)
        * std::pow(static_cast<Float>(18.181818), width);
}

template <typename Float>
Float bandActivation(Float bands, int band) noexcept
{
    return juce::jlimit(
        static_cast<Float>(0), static_cast<Float>(1),
        bands - static_cast<Float>(spectralActivationRank[
            static_cast<size_t>(band)]));
}

template <typename Float>
Float bandWeight(int band, Float patternPercent, Float shiftPercent,
                 Float contrastPercent, Float tiltPercent) noexcept
{
    const Float position = static_cast<Float>(band)
        / static_cast<Float>(7) - static_cast<Float>(0.5);
    const Float pattern = juce::jlimit(
        static_cast<Float>(0), static_cast<Float>(1),
        patternPercent * static_cast<Float>(0.01));
    const Float shift = juce::jlimit(
        static_cast<Float>(-1), static_cast<Float>(1),
        shiftPercent * static_cast<Float>(0.01));
    const Float contrast = juce::jlimit(
        static_cast<Float>(0), static_cast<Float>(1),
        contrastPercent * static_cast<Float>(0.01));
    const Float tilt = juce::jlimit(
        static_cast<Float>(-1), static_cast<Float>(1),
        tiltPercent * static_cast<Float>(0.01));
    const Float cycles = static_cast<Float>(0.5)
        + static_cast<Float>(3.5) * pattern;
    const Float phase = juce::MathConstants<Float>::twoPi
        * (cycles * (position + static_cast<Float>(0.5))
            + static_cast<Float>(0.5) * shift);
    const Float patterned = std::exp(
        static_cast<Float>(2.4) * contrast * std::cos(phase));
    const Float tilted = std::pow(
        static_cast<Float>(2), static_cast<Float>(2) * tilt * position);
    return patterned * tilted;
}
}

std::complex<double> DuskAudioProcessor::getSpectralBankResponse(
    double frequencyHz, double sampleRate, float centreHz,
    float spanOctaves, float depthPercent, float bands,
    float widthPercent, float patternPercent, float shiftPercent,
    float contrastPercent, float tiltPercent) noexcept
{
    const double safeSampleRate = juce::jmax(1.0, sampleRate);
    const double maximumFrequency = juce::jmin(
        20000.0, safeSampleRate * 0.45);
    const double safeCentre = juce::jlimit(
        40.0, maximumFrequency, static_cast<double>(centreHz));
    const double safeSpan = juce::jlimit(
        0.5, 6.0, static_cast<double>(spanOctaves));
    const double safeBands = juce::jlimit(
        2.0, 8.0, static_cast<double>(bands));
    const double depth = juce::jlimit(
        0.0, 1.0, static_cast<double>(depthPercent) * 0.01);
    const double q = spectralQ(static_cast<double>(widthPercent));
    const double damping = 1.0 / q;
    const double safeFrequency = juce::jlimit(
        0.0, safeSampleRate * 0.5, frequencyHz);
    const auto z = std::polar(
        1.0, juce::MathConstants<double>::twoPi
                 * safeFrequency / safeSampleRate);

    std::complex<double> bankResponse;
    double activeSum = 0.0;
    double weightSum = 0.0;
    for (int band = 0; band < maximumSpectralBands; ++band)
    {
        const double active = bandActivation(safeBands, band);
        if (active <= 0.0)
            continue;
        const double position = static_cast<double>(band) / 7.0 - 0.5;
        const double bandFrequency = juce::jlimit(
            20.0, maximumFrequency,
            safeCentre * std::pow(2.0, position * safeSpan));
        const double g = std::tan(
            juce::MathConstants<double>::pi
                * bandFrequency / safeSampleRate);
        const double a1 = 1.0 / (1.0 + g * (g + damping));
        const double a2 = g * a1;
        const double a3 = g * a2;
        const auto m00 = z - (2.0 * a1 - 1.0);
        const std::complex<double> m01(2.0 * a2, 0.0);
        const std::complex<double> m10(-2.0 * a2, 0.0);
        const auto m11 = z - (1.0 - 2.0 * a3);
        const auto determinant = m00 * m11 - m01 * m10;
        if (std::abs(determinant) < 1.0e-18)
            continue;
        const std::complex<double> b0(2.0 * a2, 0.0);
        const std::complex<double> b1(2.0 * a3, 0.0);
        const auto state0 = (b0 * m11 - m01 * b1) / determinant;
        const auto state1 = (m00 * b1 - b0 * m10) / determinant;
        const auto bandResponse = damping
            * std::complex<double>(a2 + a1 * state0 - a2 * state1);
        const double weight = active * bandWeight(
            band, static_cast<double>(patternPercent),
            static_cast<double>(shiftPercent),
            static_cast<double>(contrastPercent),
            static_cast<double>(tiltPercent));
        bankResponse += weight * bandResponse;
        activeSum += active;
        weightSum += weight;
    }
    const double scale = weightSum > 1.0e-12
        ? juce::jlimit(0.2, 4.0, 0.65 * activeSum / weightSum) : 0.0;
    bankResponse *= scale;
    return std::complex<double>(1.0 - depth, 0.0)
        + depth * bankResponse;
}

float DuskAudioProcessor::processSpectralBankFilter(
    float sample, int engine, int channel, float centreHz,
    float spanOctaves, float depthPercent, float bands,
    float widthPercent, float patternPercent, float shiftPercent,
    float contrastPercent, float tiltPercent,
    bool internalDistortion, float distortionAmount,
    int currentDistortion, int previousDistortion,
    float distortionCrossfade) noexcept
{
    const int safeEngine = juce::jlimit(0, filterEngineCount - 1, engine);
    const int safeChannel = juce::jlimit(0, channelCount - 1, channel);
    const auto engineIndex = static_cast<size_t>(safeEngine);
    const auto stateIndex = static_cast<size_t>(
        safeEngine * channelCount + safeChannel);
    const float sampleRate = static_cast<float>(
        juce::jmax(1.0, currentSampleRate));
    const float maximumFrequency = juce::jmin(
        20000.0f, sampleRate * 0.45f);
    const float safeCentre = juce::jlimit(
        40.0f, maximumFrequency, centreHz);
    const float safeSpan = juce::jlimit(0.5f, 6.0f, spanOctaves);
    const float safeBands = juce::jlimit(2.0f, 8.0f, bands);
    const float depth = juce::jlimit(0.0f, 1.0f, depthPercent * 0.01f);

    auto& coefficientCountdown = spectralCoefficientCountdown[engineIndex];
    const bool updateCoefficients = safeChannel == 0
        && coefficientCountdown <= 0;
    if (updateCoefficients)
    {
        const float damping = 1.0f / spectralQ(widthPercent);
        float activeSum = 0.0f;
        float weightSum = 0.0f;
        for (int band = 0; band < maximumSpectralBands; ++band)
        {
            const auto bandIndex = static_cast<size_t>(band);
            const float active = bandActivation(safeBands, band);
            const float position = static_cast<float>(band) / 7.0f - 0.5f;
            const float bandFrequency = juce::jlimit(
                20.0f, maximumFrequency,
                safeCentre * std::pow(2.0f, position * safeSpan));
            spectralCoefficient[engineIndex][bandIndex] = std::tan(
                juce::MathConstants<float>::pi
                    * bandFrequency / sampleRate);
            spectralDamping[engineIndex][bandIndex] = damping;
            const float weight = active * bandWeight(
                band, patternPercent, shiftPercent,
                contrastPercent, tiltPercent);
            spectralWeight[engineIndex][bandIndex] = weight;
            activeSum += active;
            weightSum += weight;
        }
        spectralOutputScale[engineIndex] = weightSum > 1.0e-6f
            ? juce::jlimit(0.2f, 4.0f,
                0.65f * activeSum / weightSum) : 0.0f;
        coefficientCountdown = 7;
    }
    else if (safeChannel == 0)
        --coefficientCountdown;

    float bankOutput = 0.0f;
    for (int band = 0; band < maximumSpectralBands; ++band)
    {
        const auto bandIndex = static_cast<size_t>(band);
        const float weight = spectralWeight[engineIndex][bandIndex];
        if (weight <= 1.0e-8f)
            continue;
        const float g = spectralCoefficient[engineIndex][bandIndex];
        const float damping = spectralDamping[engineIndex][bandIndex];
        const float a1 = 1.0f / (1.0f + g * (g + damping));
        const float a2 = g * a1;
        const float a3 = g * a2;
        auto& integratorOne = spectralIntegratorOne[bandIndex][stateIndex];
        auto& integratorTwo = spectralIntegratorTwo[bandIndex][stateIndex];
        const float v3 = sample - integratorTwo;
        const float v1 = a1 * integratorOne + a2 * v3;
        const float v2 = integratorTwo + a2 * integratorOne + a3 * v3;
        integratorOne = sanitiseSample(2.0f * v1 - integratorOne, 64.0f);
        integratorTwo = sanitiseSample(2.0f * v2 - integratorTwo, 64.0f);
        bankOutput += weight * damping * v1;
    }
    bankOutput = sanitiseSample(
        bankOutput * spectralOutputScale[engineIndex], 64.0f);
    if (internalDistortion)
        bankOutput = processSelectedDistortion(
            bankOutput, distortionAmount, engine, channel,
            currentDistortion, previousDistortion, distortionCrossfade,
            spectralInternalDistortionStateBank);
    return sanitiseSample(sample + (bankOutput - sample) * depth, 64.0f);
}
