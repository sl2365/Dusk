#include "PluginProcessor.h"
#include "DuskFilterTypes.h"

#include <array>
#include <cmath>

namespace
{
constexpr int vactrolInternalDistortionStateBank = 22;

template <typename Float>
Float vactrolGate(Float cutoffHz, Float levelCoupling,
                  Float bleed) noexcept
{
    const Float openness = juce::jlimit(
        static_cast<Float>(0), static_cast<Float>(1),
        std::log(juce::jmax(static_cast<Float>(20), cutoffHz)
                 / static_cast<Float>(20))
            / std::log(static_cast<Float>(50)));
    const Float openLevel = bleed
        + (static_cast<Float>(1) - bleed)
            * std::pow(openness, static_cast<Float>(0.65));
    return static_cast<Float>(1)
        - levelCoupling * (static_cast<Float>(1) - openLevel);
}
}

std::complex<double> DuskAudioProcessor::getVactrolLpgResponse(
    double frequencyHz, double sampleRate, float cutoffHz,
    float resonancePercent, float slopeDbPerOctave,
    float levelCouplingPercent, float bleedPercent, float gainDb,
    int filterTypeChoice) noexcept
{
    const double safeSampleRate = juce::jmax(1.0, sampleRate);
    const double maximumCutoff = juce::jmin(20000.0,
                                            safeSampleRate * 0.45);
    const double safeCutoff = juce::jlimit(
        20.0, maximumCutoff, static_cast<double>(cutoffHz));
    const double safeFrequency = juce::jlimit(
        0.0, safeSampleRate * 0.5, frequencyHz);
    const auto z = std::polar(
        1.0, juce::MathConstants<double>::twoPi
                 * safeFrequency / safeSampleRate);
    const double resonance = juce::jlimit(
        0.0, 1.0, static_cast<double>(resonancePercent) * 0.01);
    const int filterType = juce::jlimit(
        0, dusk::filtertype::count - 1, filterTypeChoice);
    const bool equaliserType = dusk::filtertype::usesGain(filterType);

    const auto cellResponse = [safeSampleRate, safeCutoff, z, gainDb](
        double q)
    {
        const double g = std::tan(
            juce::MathConstants<double>::pi
                * safeCutoff / safeSampleRate);
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
    const auto onePoleResponse = [safeSampleRate, safeCutoff, z]()
    {
        const double g = std::tan(
            juce::MathConstants<double>::pi
                * safeCutoff / safeSampleRate);
        const double a = g / (1.0 + g);
        return std::complex<double>(
            a + (1.0 - a) * (2.0 * a)
                / (z - (1.0 - 2.0 * a)));
    };

    const double q = 0.70710678 + resonance * 9.0;
    const auto outputs = cellResponse(q);
    std::array<std::complex<double>, maximumVactrolCells + 1> taps {};
    taps[0] = { 1.0, 0.0 };
    for (int cell = 0; cell < maximumVactrolCells; ++cell)
        taps[static_cast<size_t>(cell + 1)] =
            taps[static_cast<size_t>(cell)]
            * outputs[static_cast<size_t>(filterType)];

    std::complex<double> selected;
    const double slope = juce::jlimit(
        6.0, 48.0, static_cast<double>(slopeDbPerOctave));
    if (equaliserType)
    {
        selected = taps[1];
    }
    else if (slope < 12.0)
    {
        const auto low = onePoleResponse();
        const auto lowerOutputs = dusk::filtertype::outputs(
            std::complex<double>(1.0, 0.0), low, outputs[1],
            std::complex<double>(1.0, 0.0) - low, gainDb);
        const auto lower = lowerOutputs[static_cast<size_t>(filterType)];
        selected = lower + (taps[1] - lower) * ((slope - 6.0) / 6.0);
    }
    else
    {
        const double cellPosition = slope / 12.0;
        const int lowerCells = juce::jlimit(
            1, maximumVactrolCells,
            static_cast<int>(std::floor(cellPosition)));
        const int upperCells = juce::jmin(
            maximumVactrolCells, lowerCells + 1);
        const double fraction = cellPosition
            - static_cast<double>(lowerCells);
        selected = taps[static_cast<size_t>(lowerCells)]
            + (taps[static_cast<size_t>(upperCells)]
                - taps[static_cast<size_t>(lowerCells)]) * fraction;
    }

    const double gate = vactrolGate(
        safeCutoff,
        juce::jlimit(0.0, 1.0,
            static_cast<double>(levelCouplingPercent) * 0.01),
        juce::jlimit(0.0, 1.0,
            static_cast<double>(bleedPercent) * 0.01));
    return selected * gate;
}

float DuskAudioProcessor::processVactrolLpgFilter(
    float sample, int engine, int channel, float filterTypePosition,
    float cutoffHz, float resonance, float slopeDbPerOctave,
    float risePercent, float fallPercent, float memoryPercent,
    float levelCouplingPercent, float warmthPercent,
    float bleedPercent, float gainDb,
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
    const float maximumCutoff = juce::jmin(20000.0f,
                                           sampleRate * 0.45f);
    const float targetCutoff = juce::jlimit(
        20.0f, maximumCutoff, cutoffHz);
    const float rise = juce::jlimit(0.0f, 1.0f, risePercent * 0.01f);
    const float fall = juce::jlimit(0.0f, 1.0f, fallPercent * 0.01f);
    const float memory = juce::jlimit(0.0f, 1.0f,
                                      memoryPercent * 0.01f);
    const float levelCoupling = juce::jlimit(
        0.0f, 1.0f, levelCouplingPercent * 0.01f);
    const float warmth = juce::jlimit(0.0f, 1.0f,
                                      warmthPercent * 0.01f);
    const float bleed = juce::jlimit(0.0f, 1.0f,
                                     bleedPercent * 0.01f);

    auto& envelope = vactrolEnvelopeState[stateIndex];
    const float envelopeCoefficient = std::exp(
        -1.0f / (0.020f * sampleRate));
    envelope = sanitiseSample(
        envelopeCoefficient * envelope
            + (1.0f - envelopeCoefficient)
                * juce::jlimit(0.0f, 2.0f, std::abs(sample)),
        4.0f);

    auto& cutoffState = vactrolCutoffState[stateIndex];
    const float targetLogCutoff = std::log(targetCutoff);
    if (! std::isfinite(cutoffState) || cutoffState <= 0.0f)
        cutoffState = targetLogCutoff;
    const bool opening = targetLogCutoff > cutoffState;
    const float riseMilliseconds = 0.5f * std::pow(160.0f, rise);
    float fallMilliseconds = 5.0f * std::pow(300.0f, fall);
    fallMilliseconds *= 1.0f + 8.0f * memory
        * (0.25f + juce::jlimit(0.0f, 1.0f, envelope));
    const float timeSeconds = 0.001f
        * (opening ? riseMilliseconds : fallMilliseconds);
    const float coefficient = std::exp(
        -1.0f / juce::jmax(1.0f, timeSeconds * sampleRate));
    cutoffState = coefficient * cutoffState
        + (1.0f - coefficient) * targetLogCutoff;
    const float effectiveCutoff = juce::jlimit(
        20.0f, maximumCutoff, std::exp(cutoffState));

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

    const float q = 0.70710678f
        + juce::jlimit(0.0f, 1.0f, resonance) * 9.0f;
    const float damping = juce::jlimit(0.125f, 2.0f, 1.0f / q);
    const float g = std::tan(
        juce::MathConstants<float>::pi * effectiveCutoff / sampleRate);
    const float a1 = 1.0f / (1.0f + g * (g + damping));
    const float a2 = g * a1;
    const float a3 = g * a2;
    std::array<float, maximumVactrolCells + 1> taps {};
    taps[0] = sample;
    std::array<float, dusk::filtertype::count> firstOutputs {};
    float cellInput = sample;
    for (int cell = 0; cell < maximumVactrolCells; ++cell)
    {
        const auto cellIndex = static_cast<size_t>(cell);
        auto& integratorOne = vactrolIntegratorOne[cellIndex][stateIndex];
        auto& integratorTwo = vactrolIntegratorTwo[cellIndex][stateIndex];
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
            firstOutputs = outputs;
        cellInput = dusk::filtertype::select(outputs, type);
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
        const float low = processOnePole(
            sample, effectiveCutoff, vactrolOnePoleState[stateIndex]);
        const auto lowerOutputs = dusk::filtertype::outputs(
            sample, low, firstOutputs[1], sample - low, gainDb);
        const float lower = dusk::filtertype::select(lowerOutputs, type);
        filtered = lower + (taps[1] - lower)
            * ((slope - 6.0f) / 6.0f);
    }
    else
    {
        const float cellPosition = slope / 12.0f;
        const int lowerCells = juce::jlimit(
            1, maximumVactrolCells,
            static_cast<int>(std::floor(cellPosition)));
        const int upperCells = juce::jmin(
            maximumVactrolCells, lowerCells + 1);
        const float fraction = cellPosition
            - static_cast<float>(lowerCells);
        filtered = taps[static_cast<size_t>(lowerCells)]
            + (taps[static_cast<size_t>(upperCells)]
                - taps[static_cast<size_t>(lowerCells)]) * fraction;
    }

    if (warmth > 1.0e-6f)
    {
        const float drive = 1.0f + 5.0f * warmth;
        const float saturated = std::tanh(filtered * drive)
            / std::tanh(drive);
        filtered += (saturated - filtered) * warmth;
    }
    if (internalDistortion)
        filtered = processSelectedDistortion(
            filtered, distortionAmount, engine, channel,
            currentDistortion, previousDistortion, distortionCrossfade,
            vactrolInternalDistortionStateBank);
    filtered *= vactrolGate(effectiveCutoff, levelCoupling, bleed);
    return sanitiseSample(filtered, 64.0f);
}
