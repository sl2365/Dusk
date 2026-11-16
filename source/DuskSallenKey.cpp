#include "PluginProcessor.h"
#include "DuskFilterTypes.h"

#include <array>
#include <cmath>

namespace
{
constexpr int sallenKeyInternalDistortionStateBank = 19;
constexpr std::array<float, 4> componentSpreadPattern
    { -0.75f, 0.45f, -0.20f, 0.75f };

template <typename Float>
Float shapeSallenKeyResonance(Float resonance, Float qCurve) noexcept
{
    resonance = juce::jlimit(static_cast<Float>(0),
                             static_cast<Float>(1), resonance);
    qCurve = juce::jlimit(static_cast<Float>(-1),
                          static_cast<Float>(1), qCurve);
    return std::pow(resonance,
                    std::pow(static_cast<Float>(8), -qCurve));
}
}

std::complex<double> DuskAudioProcessor::getSallenKeyResponse(
    double frequencyHz, double sampleRate, float cutoffHz,
    float resonancePercent, float slopeDbPerOctave,
    float qCurvePercent, float cellGainPercent,
    float stageCouplingPercent, float componentVariationPercent,
    float highFrequencyDampingPercent,
    float gainDb,
    int filterTypeChoice) noexcept
{
    const double safeSampleRate = juce::jmax(1.0, sampleRate);
    const double maximumCutoff = juce::jmin(
        20000.0, safeSampleRate * 0.45);
    const double safeCutoff = juce::jlimit(
        20.0, maximumCutoff, static_cast<double>(cutoffHz));
    const double safeFrequency = juce::jlimit(
        0.0, safeSampleRate * 0.5, frequencyHz);
    const auto z = std::polar(
        1.0, juce::MathConstants<double>::twoPi
                 * safeFrequency / safeSampleRate);
    const double shapedResonance = shapeSallenKeyResonance(
        juce::jlimit(0.0, 1.0,
            static_cast<double>(resonancePercent) * 0.01),
        juce::jlimit(-1.0, 1.0,
            static_cast<double>(qCurvePercent) * 0.01));
    const double cellGain = juce::jlimit(
        0.0, 1.0, static_cast<double>(cellGainPercent) * 0.01);
    const double coupling = juce::jlimit(
        0.0, 1.0, static_cast<double>(stageCouplingPercent) * 0.01);
    const double variation = juce::jlimit(
        -1.0, 1.0,
        static_cast<double>(componentVariationPercent) * 0.01);
    const double highFrequencyDamping = juce::jlimit(
        0.0, 1.0,
        static_cast<double>(highFrequencyDampingPercent) * 0.01);
    const int filterType = juce::jlimit(
        0, dusk::filtertype::count - 1, filterTypeChoice);
    const bool equaliserType = dusk::filtertype::usesGain(filterType);

    const auto cellResponse = [safeSampleRate, z, gainDb](
        double cellCutoff, double q)
    {
        const double g = std::tan(
            juce::MathConstants<double>::pi
                * cellCutoff / safeSampleRate);
        const double damping = juce::jlimit(0.125, 2.0, 1.0 / q);
        const double a1 = 1.0 / (1.0 + g * (g + damping));
        const double a2 = g * a1;
        const double a3 = g * a2;
        const auto m00 = z - (2.0 * a1 - 1.0);
        const std::complex<double> m01(2.0 * a2, 0.0);
        const std::complex<double> m10(-2.0 * a2, 0.0);
        const auto m11 = z - (1.0 - 2.0 * a3);
        const auto determinant = m00 * m11 - m01 * m10;
        if (std::abs(determinant) < 1.0e-18)
            return std::array<std::complex<double>,
                              dusk::filtertype::count> {};
        const std::complex<double> b0(2.0 * a2, 0.0);
        const std::complex<double> b1(2.0 * a3, 0.0);
        const auto state0 = (b0 * m11 - m01 * b1) / determinant;
        const auto state1 = (m00 * b1 - b0 * m10) / determinant;
        const auto low = std::complex<double>(
            a3 + a2 * state0 + (1.0 - a3) * state1);
        const auto band = std::complex<double>(
            a2 + a1 * state0 - a2 * state1);
        const auto high = std::complex<double>(1.0, 0.0)
                        - damping * band - low;
        return dusk::filtertype::outputs(
            std::complex<double>(1.0, 0.0), low, band, high, gainDb);
    };
    const auto onePoleResponse = [safeSampleRate, z](double cellCutoff)
    {
        const double g = std::tan(
            juce::MathConstants<double>::pi
                * cellCutoff / safeSampleRate);
        const double a = g / (1.0 + g);
        return std::complex<double>(
            a + (1.0 - a) * (2.0 * a)
                / (z - (1.0 - 2.0 * a)));
    };

    const double maximumQ = 0.70710678
        + shapedResonance * (1.5 + 8.5 * cellGain);
    std::array<std::complex<double>, maximumSallenKeyCells + 1> taps {};
    taps[0] = { 1.0, 0.0 };
    std::array<std::complex<double>, dusk::filtertype::count>
        firstCellOutputs {};
    for (int cell = 0; cell < maximumSallenKeyCells; ++cell)
    {
        const double stagePosition = static_cast<double>(cell)
            / static_cast<double>(maximumSallenKeyCells - 1);
        const double resonanceDistribution = cell == 0
            ? 1.0 : 0.15 + 0.85 * std::pow(coupling, stagePosition);
        const double q = juce::jlimit(
            0.5, 8.0,
            0.70710678 + (maximumQ - 0.70710678)
                * resonanceDistribution
                * (1.0 - 0.72 * highFrequencyDamping * stagePosition));
        const double spreadOctaves = variation
            * static_cast<double>(componentSpreadPattern[
                static_cast<size_t>(cell)]);
        const double dampOctaves = -0.55 * highFrequencyDamping
            * stagePosition;
        const double cellCutoff = juce::jlimit(
            20.0, maximumCutoff,
            safeCutoff * std::pow(2.0, spreadOctaves + dampOctaves));
        const auto outputs = cellResponse(cellCutoff, q);
        if (cell == 0)
            firstCellOutputs = outputs;
        taps[static_cast<size_t>(cell + 1)] =
            taps[static_cast<size_t>(cell)]
            * outputs[static_cast<size_t>(filterType)];
    }

    const double slope = juce::jlimit(
        6.0, 48.0, static_cast<double>(slopeDbPerOctave));
    if (equaliserType)
        return taps[1];
    if (slope < 12.0)
    {
        const auto onePoleLow = onePoleResponse(safeCutoff);
        const auto onePoleHigh = std::complex<double>(1.0, 0.0)
                               - onePoleLow;
        const auto lowerOutputs = dusk::filtertype::outputs(
            std::complex<double>(1.0, 0.0), onePoleLow,
            firstCellOutputs[1], onePoleHigh, gainDb);
        const double blend = (slope - 6.0) / 6.0;
        const auto lower = lowerOutputs[static_cast<size_t>(filterType)];
        return lower + (taps[1] - lower) * blend;
    }

    const double cellPosition = slope / 12.0;
    const int lowerCells = juce::jlimit(
        1, maximumSallenKeyCells,
        static_cast<int>(std::floor(cellPosition)));
    const int upperCells = juce::jmin(
        maximumSallenKeyCells, lowerCells + 1);
    const double fraction = cellPosition - static_cast<double>(lowerCells);
    const auto lower = taps[static_cast<size_t>(lowerCells)];
    const auto upper = taps[static_cast<size_t>(upperCells)];
    return lower + (upper - lower) * fraction;
}

float DuskAudioProcessor::processSallenKeyFilter(
    float sample, int engine, int channel, float filterTypePosition,
    float cutoffHz, float resonance, float slopeDbPerOctave,
    float qCurvePercent, float cellGainPercent,
    float stageCouplingPercent, float componentVariationPercent,
    float opAmpSaturationPercent, float highFrequencyDampingPercent,
    float gainDb,
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
    const float maximumCutoff = juce::jmin(20000.0f, sampleRate * 0.45f);
    const float safeCutoff = juce::jlimit(
        20.0f, maximumCutoff, cutoffHz);
    const float shapedResonance = shapeSallenKeyResonance(
        juce::jlimit(0.0f, 1.0f, resonance),
        juce::jlimit(-1.0f, 1.0f, qCurvePercent * 0.01f));
    const float cellGain = juce::jlimit(
        0.0f, 1.0f, cellGainPercent * 0.01f);
    const float coupling = juce::jlimit(
        0.0f, 1.0f, stageCouplingPercent * 0.01f);
    const float variation = juce::jlimit(
        -1.0f, 1.0f, componentVariationPercent * 0.01f);
    const float opAmpSaturation = juce::jlimit(
        0.0f, 1.0f, opAmpSaturationPercent * 0.01f);
    const float highFrequencyDamping = juce::jlimit(
        0.0f, 1.0f, highFrequencyDampingPercent * 0.01f);
    const float type = juce::jlimit(
        0.0f, static_cast<float>(dusk::filtertype::count - 1),
        filterTypePosition);
    const bool equaliserType = dusk::filtertype::usesGain(
        static_cast<int>(std::round(type)));

    const auto processOnePole = [sampleRate](
        float input, float cellCutoff, float& state)
    {
        const float g = std::tan(
            juce::MathConstants<float>::pi * cellCutoff / sampleRate);
        const float a = g / (1.0f + g);
        const float v = (input - state) * a;
        const float output = v + state;
        state = DuskAudioProcessor::sanitiseSample(output + v, 64.0f);
        return DuskAudioProcessor::sanitiseSample(output, 64.0f);
    };
    const auto selectOutput = [type](
        const std::array<float, dusk::filtertype::count>& outputs)
    {
        const int left = juce::jlimit(
            0, dusk::filtertype::count - 1,
            static_cast<int>(std::floor(type)));
        const int right = juce::jmin(
            dusk::filtertype::count - 1, left + 1);
        const float fraction = type - static_cast<float>(left);
        return outputs[static_cast<size_t>(left)]
            + (outputs[static_cast<size_t>(right)]
                - outputs[static_cast<size_t>(left)]) * fraction;
    };
    const auto saturate = [opAmpSaturation](float value)
    {
        if (opAmpSaturation <= 1.0e-6f)
            return value;
        const float character = std::pow(opAmpSaturation, 1.35f);
        const float threshold = 2.0f * std::pow(0.025f, character);
        return threshold * std::tanh(value / threshold);
    };

    const float maximumQ = 0.70710678f
        + shapedResonance * (1.5f + 8.5f * cellGain);
    std::array<float, maximumSallenKeyCells + 1> taps {};
    taps[0] = sample;
    std::array<float, dusk::filtertype::count> firstCellOutputs {};
    float cellInput = sample;
    for (int cell = 0; cell < maximumSallenKeyCells; ++cell)
    {
        const auto cellIndex = static_cast<size_t>(cell);
        const float stagePosition = static_cast<float>(cell)
            / static_cast<float>(maximumSallenKeyCells - 1);
        const float resonanceDistribution = cell == 0
            ? 1.0f : 0.15f + 0.85f * std::pow(coupling, stagePosition);
        const float q = juce::jlimit(
            0.5f, 8.0f,
            0.70710678f + (maximumQ - 0.70710678f)
                * resonanceDistribution
                * (1.0f - 0.72f * highFrequencyDamping * stagePosition));
        const float damping = juce::jlimit(0.125f, 2.0f, 1.0f / q);
        const float spreadOctaves = variation
            * componentSpreadPattern[cellIndex];
        const float dampOctaves = -0.55f * highFrequencyDamping
            * stagePosition;
        const float cellCutoff = juce::jlimit(
            20.0f, maximumCutoff,
            safeCutoff * std::pow(2.0f, spreadOctaves + dampOctaves));
        const float g = std::tan(
            juce::MathConstants<float>::pi * cellCutoff / sampleRate);
        const float a1 = 1.0f / (1.0f + g * (g + damping));
        const float a2 = g * a1;
        const float a3 = g * a2;
        auto& integratorOne = sallenKeyIntegratorOne[cellIndex][stateIndex];
        auto& integratorTwo = sallenKeyIntegratorTwo[cellIndex][stateIndex];
        const float v3 = cellInput - integratorTwo;
        const float v1 = a1 * integratorOne + a2 * v3;
        const float v2 = integratorTwo + a2 * integratorOne + a3 * v3;
        integratorOne = sanitiseSample(2.0f * v1 - integratorOne, 64.0f);
        integratorTwo = sanitiseSample(2.0f * v2 - integratorTwo, 64.0f);
        const float low = sanitiseSample(v2, 64.0f);
        const float band = sanitiseSample(v1, 64.0f);
        const float high = sanitiseSample(
            cellInput - damping * band - low, 64.0f);
        const auto outputs = dusk::filtertype::outputs(
            cellInput, low, band, high, gainDb);
        if (cell == 0)
            firstCellOutputs = outputs;
        cellInput = sanitiseSample(saturate(selectOutput(outputs)), 64.0f);
        taps[cellIndex + 1] = cellInput;
    }

    const float slope = juce::jlimit(6.0f, 48.0f, slopeDbPerOctave);
    float filtered = 0.0f;
    if (equaliserType)
    {
        filtered = taps[1];
    }
    else if (slope < 12.0f)
    {
        const float onePoleLow = processOnePole(
            sample, safeCutoff, sallenKeyOnePoleState[stateIndex]);
        const float onePoleHigh = sample - onePoleLow;
        const auto lowerOutputs = dusk::filtertype::outputs(
            sample, onePoleLow, firstCellOutputs[1], onePoleHigh, gainDb);
        const float lower = selectOutput(lowerOutputs);
        filtered = lower + (taps[1] - lower) * ((slope - 6.0f) / 6.0f);
    }
    else
    {
        const float cellPosition = slope / 12.0f;
        const int lowerCells = juce::jlimit(
            1, maximumSallenKeyCells,
            static_cast<int>(std::floor(cellPosition)));
        const int upperCells = juce::jmin(
            maximumSallenKeyCells, lowerCells + 1);
        const float fraction = cellPosition - static_cast<float>(lowerCells);
        filtered = taps[static_cast<size_t>(lowerCells)]
            + (taps[static_cast<size_t>(upperCells)]
                - taps[static_cast<size_t>(lowerCells)]) * fraction;
    }

    if (internalDistortion)
        filtered = processSelectedDistortion(
            filtered, distortionAmount, engine, channel,
            currentDistortion, previousDistortion, distortionCrossfade,
            sallenKeyInternalDistortionStateBank);
    return sanitiseSample(filtered, 64.0f);
}
