#include "PluginProcessor.h"
#include "DuskFilterTypes.h"

#include <array>
#include <cmath>

namespace
{
constexpr int steinerInternalDistortionStateBank = 21;
constexpr std::array<float, 4> steinerSpreadPattern
    { -0.75f, 0.45f, -0.20f, 0.75f };

template <typename Float>
Float shapeSteinerResonance(Float resonance, Float curve) noexcept
{
    resonance = juce::jlimit(
        static_cast<Float>(0), static_cast<Float>(1), resonance);
    curve = juce::jlimit(
        static_cast<Float>(-1), static_cast<Float>(1), curve);
    return std::pow(resonance,
                    std::pow(static_cast<Float>(8), -curve));
}
}

std::complex<double> DuskAudioProcessor::getSteinerParkerResponse(
    double frequencyHz, double sampleRate, float cutoffHz,
    float resonancePercent, float slopeDbPerOctave,
    float poleSpreadPercent, float resonanceCurvePercent,
    float resonanceCompensationPercent, float gainDb,
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
    const double resonance = shapeSteinerResonance(
        juce::jlimit(0.0, 1.0,
            static_cast<double>(resonancePercent) * 0.01),
        juce::jlimit(-1.0, 1.0,
            static_cast<double>(resonanceCurvePercent) * 0.01));
    const double spread = juce::jlimit(
        -1.0, 1.0, static_cast<double>(poleSpreadPercent) * 0.01);
    const double compensation = juce::jlimit(
        0.0, 1.0,
        static_cast<double>(resonanceCompensationPercent) * 0.01);
    const double inputGain = 1.0 + 2.5 * resonance * compensation;
    const int filterType = juce::jlimit(
        0, dusk::filtertype::count - 1, filterTypeChoice);

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
    const auto cellResponse = [safeSampleRate, z, gainDb](
        double cellCutoff, double cellResonance, double feedbackGain)
    {
        const double g = std::tan(
            juce::MathConstants<double>::pi
                * cellCutoff / safeSampleRate);
        const double damping = juce::jlimit(
            0.14, 1.41421356, 1.41421356 - 1.22 * cellResonance);
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
        auto low = std::complex<double>(
            a3 + a2 * state0 + (1.0 - a3) * state1);
        auto band = std::complex<double>(
            a2 + a1 * state0 - a2 * state1);
        auto high = std::complex<double>(1.0, 0.0)
                  - damping * band - low;
        const auto feedbackDenominator = std::complex<double>(1.0, 0.0)
            + feedbackGain * low / z;
        if (std::abs(feedbackDenominator) > 1.0e-12)
        {
            low /= feedbackDenominator;
            band /= feedbackDenominator;
            high /= feedbackDenominator;
        }
        return dusk::filtertype::outputs(
            std::complex<double>(1.0, 0.0), low, band, high, gainDb);
    };

    std::array<std::complex<double>, maximumSteinerCells + 1> taps {};
    taps[0] = { inputGain, 0.0 };
    std::array<std::complex<double>, dusk::filtertype::count>
        firstCellOutputs {};
    for (int cell = 0; cell < maximumSteinerCells; ++cell)
    {
        const double position = static_cast<double>(cell)
            / static_cast<double>(maximumSteinerCells - 1);
        const double cellCutoff = juce::jlimit(
            20.0, maximumCutoff,
            safeCutoff * std::pow(
                2.0, 1.5 * spread * static_cast<double>(
                    steinerSpreadPattern[static_cast<size_t>(cell)])));
        const double cellResonance = resonance
            * (cell == 0 ? 1.0 : 0.35 + 0.65 * (1.0 - position));
        const double feedbackGain = 0.35 * cellResonance
            * (1.0 - 0.12 * static_cast<double>(cell));
        const auto outputs = cellResponse(
            cellCutoff, cellResonance, feedbackGain);
        if (cell == 0)
            firstCellOutputs = outputs;
        taps[static_cast<size_t>(cell + 1)] =
            taps[static_cast<size_t>(cell)]
            * outputs[static_cast<size_t>(filterType)];
    }

    std::complex<double> selected;
    if (dusk::filtertype::usesGain(filterType))
    {
        selected = inputGain
            * firstCellOutputs[static_cast<size_t>(filterType)];
    }
    else
    {
        const double slope = juce::jlimit(
            6.0, 48.0, static_cast<double>(slopeDbPerOctave));
        if (slope < 12.0)
        {
            const auto onePoleLow = onePoleResponse(safeCutoff);
            const auto onePoleHigh = std::complex<double>(1.0, 0.0)
                                  - onePoleLow;
            const auto lowerOutputs = dusk::filtertype::outputs(
                std::complex<double>(1.0, 0.0), onePoleLow,
                firstCellOutputs[1], onePoleHigh, gainDb);
            const auto lower = inputGain
                * lowerOutputs[static_cast<size_t>(filterType)];
            selected = lower + (taps[1] - lower)
                * ((slope - 6.0) / 6.0);
        }
        else
        {
            const double cellPosition = slope / 12.0;
            const int lowerCells = juce::jlimit(
                1, maximumSteinerCells,
                static_cast<int>(std::floor(cellPosition)));
            const int upperCells = juce::jmin(
                maximumSteinerCells, lowerCells + 1);
            const double fraction = cellPosition - lowerCells;
            selected = taps[static_cast<size_t>(lowerCells)]
                + (taps[static_cast<size_t>(upperCells)]
                    - taps[static_cast<size_t>(lowerCells)]) * fraction;
        }
    }

    const double dcCoefficient = std::exp(
        -juce::MathConstants<double>::twoPi * 5.0 / safeSampleRate);
    const auto dcBlock = (z - std::complex<double>(1.0, 0.0))
        / (z - dcCoefficient);
    return dcBlock * selected;
}

float DuskAudioProcessor::processSteinerParkerFilter(
    float sample, int engine, int channel, float filterTypePosition,
    float cutoffHz, float resonance, float slopeDbPerOctave,
    float diodeDrivePercent, float feedbackClipPercent,
    float biasPercent, float poleSpreadPercent,
    float resonanceCurvePercent, float resonanceCompensationPercent,
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
    const auto filterTypeGains = dusk::filtertype::gainsForDecibels(gainDb);
    const float shapedResonance = shapeSteinerResonance(
        juce::jlimit(0.0f, 1.0f, resonance),
        juce::jlimit(-1.0f, 1.0f, resonanceCurvePercent * 0.01f));
    const float drive = juce::jlimit(
        0.0f, 1.0f, diodeDrivePercent * 0.01f);
    const float clip = juce::jlimit(
        0.0f, 1.0f, feedbackClipPercent * 0.01f);
    const float bias = juce::jlimit(
        -1.0f, 1.0f, biasPercent * 0.01f);
    const float spread = juce::jlimit(
        -1.0f, 1.0f, poleSpreadPercent * 0.01f);
    const float compensation = juce::jlimit(
        0.0f, 1.0f, resonanceCompensationPercent * 0.01f);
    const float inputGain = 1.0f + 2.5f * shapedResonance * compensation;
    const float type = juce::jlimit(
        0.0f, static_cast<float>(dusk::filtertype::count - 1),
        filterTypePosition);
    const bool equaliserType = dusk::filtertype::usesGain(
        static_cast<int>(std::round(type)));
    const float slope = juce::jlimit(6.0f, 48.0f, slopeDbPerOctave);
    const int activeCells = equaliserType || slope < 12.0f
        ? juce::jmin(2, maximumSteinerCells)
        : juce::jmin(maximumSteinerCells,
            static_cast<int>(std::floor(slope / 12.0f)) + 1);

    const float driveShape = std::pow(drive, 1.25f);
    const float driveAmount = 1.0f + 31.0f * driveShape;
    const float positiveThreshold = std::pow(
        5.0f, bias * driveShape) / driveAmount;
    const float negativeThreshold = std::pow(
        5.0f, -bias * driveShape) / driveAmount;
    const auto saturateDiode = [driveShape, positiveThreshold,
                                negativeThreshold](float value)
    {
        if (driveShape <= 1.0e-6f)
            return value;
        const float threshold = value >= 0.0f
            ? positiveThreshold : negativeThreshold;
        const float curved = threshold * std::tanh(value / threshold);
        return value + (curved - value) * driveShape;
    };
    const float clipShape = std::pow(clip, 1.25f);
    const float clipDrive = 1.0f + 47.0f * clipShape;
    const float clipPositiveThreshold = std::pow(
        4.0f, bias * clipShape) / clipDrive;
    const float clipNegativeThreshold = std::pow(
        4.0f, -bias * clipShape) / clipDrive;
    const auto clipFeedback = [clipShape, clipPositiveThreshold,
                               clipNegativeThreshold](float value)
    {
        if (clipShape <= 1.0e-6f)
            return value;
        const float threshold = value >= 0.0f
            ? clipPositiveThreshold : clipNegativeThreshold;
        const float curved = threshold * std::tanh(value / threshold);
        return value + (curved - value) * clipShape;
    };
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

    const float drivenInput = saturateDiode(sample * inputGain);
    std::array<float, maximumSteinerCells + 1> taps {};
    taps[0] = drivenInput;
    std::array<float, dusk::filtertype::count> firstCellOutputs {};
    float cellInput = drivenInput;
    for (int cell = 0; cell < activeCells; ++cell)
    {
        const auto cellIndex = static_cast<size_t>(cell);
        const float position = static_cast<float>(cell)
            / static_cast<float>(maximumSteinerCells - 1);
        const float cellCutoff = juce::jlimit(
            20.0f, maximumCutoff,
            safeCutoff * std::pow(
                2.0f, 1.5f * spread * steinerSpreadPattern[cellIndex]));
        const float cellResonance = shapedResonance
            * (cell == 0 ? 1.0f : 0.35f + 0.65f * (1.0f - position));
        const float feedbackGain = 0.35f * cellResonance
            * (1.0f - 0.12f * static_cast<float>(cell));
        float feedbackSignal = clipFeedback(
            steinerFeedbackReturn[cellIndex][stateIndex]);
        if (cell == 0 && internalDistortion)
            feedbackSignal = processSelectedDistortion(
                feedbackSignal, distortionAmount, engine, channel,
                currentDistortion, previousDistortion,
                distortionCrossfade, steinerInternalDistortionStateBank);
        const float cellDriveInput = sanitiseSample(
            saturateDiode(cellInput - feedbackGain * feedbackSignal),
            64.0f);
        const float damping = juce::jlimit(
            0.14f, 1.41421356f,
            1.41421356f - 1.22f * cellResonance);
        const float g = std::tan(
            juce::MathConstants<float>::pi * cellCutoff / sampleRate);
        const float a1 = 1.0f / (1.0f + g * (g + damping));
        const float a2 = g * a1;
        const float a3 = g * a2;
        auto& integratorOne = steinerIntegratorOne[cellIndex][stateIndex];
        auto& integratorTwo = steinerIntegratorTwo[cellIndex][stateIndex];
        const float v3 = cellDriveInput - integratorTwo;
        const float v1 = a1 * integratorOne + a2 * v3;
        const float v2 = integratorTwo + a2 * integratorOne + a3 * v3;
        integratorOne = sanitiseSample(2.0f * v1 - integratorOne, 64.0f);
        integratorTwo = sanitiseSample(2.0f * v2 - integratorTwo, 64.0f);
        const float low = sanitiseSample(v2, 64.0f);
        const float band = sanitiseSample(v1, 64.0f);
        const float high = sanitiseSample(
            cellDriveInput - damping * band - low, 64.0f);
        const auto outputs = dusk::filtertype::outputs(
            cellDriveInput, low, band, high, filterTypeGains);
        if (cell == 0)
            firstCellOutputs = outputs;
        steinerFeedbackReturn[cellIndex][stateIndex] = low;
        cellInput = sanitiseSample(
            saturateDiode(dusk::filtertype::select(outputs, type)), 64.0f);
        taps[cellIndex + 1] = cellInput;
    }

    float selected = 0.0f;
    if (equaliserType)
    {
        selected = dusk::filtertype::select(firstCellOutputs, type);
    }
    else
    {
        if (slope < 12.0f)
        {
            const float onePoleLow = processOnePole(
                drivenInput, safeCutoff, steinerOnePoleState[stateIndex]);
            const float onePoleHigh = drivenInput - onePoleLow;
            const auto lowerOutputs = dusk::filtertype::outputs(
                drivenInput, onePoleLow, firstCellOutputs[1],
                onePoleHigh, filterTypeGains);
            const float lower = dusk::filtertype::select(
                lowerOutputs, type);
            selected = lower + (taps[1] - lower)
                * ((slope - 6.0f) / 6.0f);
        }
        else
        {
            const float cellPosition = slope / 12.0f;
            const int lowerCells = juce::jlimit(
                1, maximumSteinerCells,
                static_cast<int>(std::floor(cellPosition)));
            const int upperCells = juce::jmin(
                maximumSteinerCells, lowerCells + 1);
            const float fraction = cellPosition
                - static_cast<float>(lowerCells);
            selected = taps[static_cast<size_t>(lowerCells)]
                + (taps[static_cast<size_t>(upperCells)]
                    - taps[static_cast<size_t>(lowerCells)]) * fraction;
        }
    }

    const float dcRemoved = sanitiseSample(
        selected - steinerDcInput[stateIndex]
            + coreAsymmetryDcCoefficient * steinerDcOutput[stateIndex],
        64.0f);
    steinerDcInput[stateIndex] = selected;
    steinerDcOutput[stateIndex] = dcRemoved;
    return dcRemoved;
}
