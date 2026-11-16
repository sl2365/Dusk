#include "PluginProcessor.h"
#include "DuskFilterTypes.h"

#include <array>
#include <cmath>

namespace
{
constexpr int korg35InternalDistortionStateBank = 23;

template <typename Float>
Float resonanceFeedbackGain(Float resonance) noexcept
{
    resonance = juce::jlimit(static_cast<Float>(0),
                             static_cast<Float>(1), resonance);
    return static_cast<Float>(3.75) * std::pow(
        resonance, static_cast<Float>(0.72));
}
}

std::complex<double> DuskAudioProcessor::getKorg35Response(
    double frequencyHz, double sampleRate, float cutoffHz,
    float resonancePercent, float slopeDbPerOctave,
    float feedbackTonePercent, float bassLossPercent,
    float resonanceCompensationPercent, float gainDb,
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
    const double feedbackGain = resonanceFeedbackGain(resonance);
    const double feedbackTone = juce::jlimit(
        0.0, 1.0, static_cast<double>(feedbackTonePercent) * 0.01);
    const double bassLoss = juce::jlimit(
        0.0, 1.0, static_cast<double>(bassLossPercent) * 0.01);
    const double compensation = juce::jlimit(
        0.0, 1.0,
        static_cast<double>(resonanceCompensationPercent) * 0.01);
    const int filterType = juce::jlimit(
        0, dusk::filtertype::count - 1, filterTypeChoice);
    const bool equaliserType = dusk::filtertype::usesGain(filterType);

    const auto onePoleResponse = [safeSampleRate, z](double frequency)
    {
        const double g = std::tan(
            juce::MathConstants<double>::pi
                * frequency / safeSampleRate);
        const double a = g / (1.0 + g);
        return std::complex<double>(
            a + (1.0 - a) * (2.0 * a)
                / (z - (1.0 - 2.0 * a)));
    };
    const auto lowPole = onePoleResponse(safeCutoff);
    const double toneCutoff = juce::jlimit(
        20.0, maximumCutoff,
        20.0 * std::pow(100.0, feedbackTone));
    const auto toneLow = onePoleResponse(toneCutoff);
    const auto toneHigh = std::complex<double>(1.0, 0.0) - toneLow;
    const auto feedbackToneResponse =
        std::complex<double>(1.0 - feedbackTone, 0.0)
        + feedbackTone * toneHigh;

    const auto cellOutputs = [&](double cellResonance)
    {
        const auto rawLow = lowPole * lowPole;
        const auto denominator = std::complex<double>(1.0, 0.0)
            + cellResonance * feedbackToneResponse * rawLow;
        const auto safeDenominator = std::abs(denominator) < 1.0e-12
            ? std::complex<double>(1.0e-12, 0.0) : denominator;
        const auto coreInput = std::complex<double>(1.0, 0.0)
                             / safeDenominator;
        const auto stageOne = lowPole * coreInput;
        const auto low = lowPole * stageOne;
        const auto band = 2.0 * (stageOne - low);
        const auto high = coreInput - 2.0 * stageOne + low;
        return dusk::filtertype::outputs(
            coreInput, low, band, high, gainDb);
    };

    std::array<std::complex<double>, maximumKorg35Cells + 1> taps {};
    taps[0] = { 1.0, 0.0 };
    std::array<std::complex<double>, dusk::filtertype::count>
        firstCellOutputs {};
    for (int cell = 0; cell < maximumKorg35Cells; ++cell)
    {
        const double cellResonance = feedbackGain
            * (cell == 0 ? 1.0 : 0.18);
        const auto outputs = cellOutputs(cellResonance);
        if (cell == 0)
            firstCellOutputs = outputs;
        taps[static_cast<size_t>(cell + 1)] =
            taps[static_cast<size_t>(cell)]
            * outputs[static_cast<size_t>(filterType)];
    }

    std::complex<double> selected;
    const double slope = juce::jlimit(
        6.0, 48.0, static_cast<double>(slopeDbPerOctave));
    if (equaliserType)
    {
        selected = taps[1];
    }
    else if (slope < 12.0)
    {
        const auto onePoleLow = lowPole;
        const auto lowerOutputs = dusk::filtertype::outputs(
            std::complex<double>(1.0, 0.0), onePoleLow,
            firstCellOutputs[1],
            std::complex<double>(1.0, 0.0) - onePoleLow, gainDb);
        const auto lower = lowerOutputs[static_cast<size_t>(filterType)];
        selected = lower + (taps[1] - lower) * ((slope - 6.0) / 6.0);
    }
    else
    {
        const double cellPosition = slope / 12.0;
        const int lowerCells = juce::jlimit(
            1, maximumKorg35Cells,
            static_cast<int>(std::floor(cellPosition)));
        const int upperCells = juce::jmin(
            maximumKorg35Cells, lowerCells + 1);
        const double fraction = cellPosition
            - static_cast<double>(lowerCells);
        selected = taps[static_cast<size_t>(lowerCells)]
            + (taps[static_cast<size_t>(upperCells)]
                - taps[static_cast<size_t>(lowerCells)]) * fraction;
    }

    const auto bassLossResponse = std::complex<double>(1.0, 0.0)
        - (0.78 * bassLoss * resonance) * onePoleResponse(110.0);
    const double compensationGain = 1.0
        + 2.2 * resonance * compensation;
    return selected * bassLossResponse * compensationGain;
}

float DuskAudioProcessor::processKorg35Filter(
    float sample, int engine, int channel, float filterTypePosition,
    float cutoffHz, float resonance, float slopeDbPerOctave,
    float chipDrivePercent, float feedbackClipPercent,
    float biasPercent, float feedbackTonePercent,
    float bassLossPercent, float resonanceCompensationPercent,
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
    const float maximumCutoff = juce::jmin(20000.0f,
                                           sampleRate * 0.45f);
    const float safeCutoff = juce::jlimit(
        20.0f, maximumCutoff, cutoffHz);
    const float safeResonance = juce::jlimit(0.0f, 1.0f, resonance);
    const float feedbackGain = resonanceFeedbackGain(safeResonance);
    const float drive = juce::jlimit(
        0.0f, 1.0f, chipDrivePercent * 0.01f);
    const float feedbackClip = juce::jlimit(
        0.0f, 1.0f, feedbackClipPercent * 0.01f);
    const float bias = juce::jlimit(
        -1.0f, 1.0f, biasPercent * 0.01f);
    const float feedbackTone = juce::jlimit(
        0.0f, 1.0f, feedbackTonePercent * 0.01f);
    const float bassLoss = juce::jlimit(
        0.0f, 1.0f, bassLossPercent * 0.01f);
    const float compensation = juce::jlimit(
        0.0f, 1.0f, resonanceCompensationPercent * 0.01f);
    const float type = juce::jlimit(
        0.0f, static_cast<float>(dusk::filtertype::count - 1),
        filterTypePosition);
    const bool equaliserType = dusk::filtertype::usesGain(
        static_cast<int>(std::round(type)));

    const float g = std::tan(
        juce::MathConstants<float>::pi * safeCutoff / sampleRate);
    const float coefficient = g / (1.0f + g);
    const float toneCutoff = juce::jlimit(
        20.0f, maximumCutoff,
        20.0f * std::pow(100.0f, feedbackTone));
    const float toneCoefficient = 1.0f - std::exp(
        -juce::MathConstants<float>::twoPi * toneCutoff / sampleRate);
    const float driveGain = 1.0f + 15.0f * std::pow(drive, 1.25f);
    const float driveBias = 0.18f * bias * drive;
    const float driveOffset = std::tanh(driveGain * driveBias) / driveGain;
    const float clipThreshold = 2.0f
        * std::pow(0.025f, std::pow(feedbackClip, 1.25f));
    const float feedbackBias = 0.35f * bias * feedbackClip;
    const float feedbackOffset = clipThreshold * std::tanh(
        feedbackBias / clipThreshold);

    const auto processOnePole = [sampleRate](
        float input, float frequency, float& state)
    {
        const float poleG = std::tan(
            juce::MathConstants<float>::pi * frequency / sampleRate);
        const float poleA = poleG / (1.0f + poleG);
        const float v = (input - state) * poleA;
        const float output = v + state;
        state = DuskAudioProcessor::sanitiseSample(output + v, 64.0f);
        return DuskAudioProcessor::sanitiseSample(output, 64.0f);
    };
    const auto saturateDrive = [drive, driveGain, driveBias,
                                driveOffset](float value)
    {
        if (drive <= 1.0e-6f)
            return value;
        const float saturated = std::tanh(
            driveGain * (value + driveBias)) / driveGain - driveOffset;
        return value + (saturated - value) * drive;
    };
    const auto saturateFeedback = [feedbackClip, clipThreshold,
                                   feedbackBias,
                                   feedbackOffset](float value)
    {
        if (feedbackClip <= 1.0e-6f)
            return value;
        return clipThreshold * std::tanh(
            (value + feedbackBias) / clipThreshold) - feedbackOffset;
    };

    std::array<float, maximumKorg35Cells + 1> taps {};
    taps[0] = sample;
    std::array<float, dusk::filtertype::count> firstCellOutputs {};
    float cellInput = sample;
    for (int cell = 0; cell < maximumKorg35Cells; ++cell)
    {
        const auto cellIndex = static_cast<size_t>(cell);
        auto& stageOneState = korg35StageOneState[cellIndex][stateIndex];
        auto& stageTwoState = korg35StageTwoState[cellIndex][stateIndex];
        auto& toneState = korg35FeedbackToneState[cellIndex][stateIndex];
        const float cellFeedbackGain = feedbackGain
            * (cell == 0 ? 1.0f : 0.18f);
        float coreInput = cellInput;
        float stageOne = 0.0f;
        float low = 0.0f;
        float feedbackSignal = toneState;
        for (int iteration = 0; iteration < 4; ++iteration)
        {
            coreInput = saturateDrive(
                cellInput - cellFeedbackGain
                    * saturateFeedback(feedbackSignal));
            const float v1 = (coreInput - stageOneState) * coefficient;
            stageOne = v1 + stageOneState;
            const float v2 = (stageOne - stageTwoState) * coefficient;
            low = v2 + stageTwoState;
            const float toneLow = toneState
                + toneCoefficient * (low - toneState);
            const float toneHigh = low - toneLow;
            const float calculatedFeedback = low
                + (toneHigh - low) * feedbackTone;
            feedbackSignal += 0.30f
                * (calculatedFeedback - feedbackSignal);
        }
        const float v1 = (coreInput - stageOneState) * coefficient;
        stageOne = sanitiseSample(v1 + stageOneState, 64.0f);
        stageOneState = sanitiseSample(stageOne + v1, 64.0f);
        const float v2 = (stageOne - stageTwoState) * coefficient;
        low = sanitiseSample(v2 + stageTwoState, 64.0f);
        stageTwoState = sanitiseSample(low + v2, 64.0f);
        toneState = sanitiseSample(
            toneState + toneCoefficient * (low - toneState), 64.0f);
        const float band = sanitiseSample(2.0f * (stageOne - low), 64.0f);
        const float high = sanitiseSample(
            coreInput - 2.0f * stageOne + low, 64.0f);
        const auto outputs = dusk::filtertype::outputs(
            coreInput, low, band, high, gainDb);
        if (cell == 0)
            firstCellOutputs = outputs;
        cellInput = sanitiseSample(
            dusk::filtertype::select(outputs, type), 64.0f);
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
            sample, safeCutoff, korg35OnePoleState[stateIndex]);
        const auto lowerOutputs = dusk::filtertype::outputs(
            sample, onePoleLow, firstCellOutputs[1],
            sample - onePoleLow, gainDb);
        const float lower = dusk::filtertype::select(lowerOutputs, type);
        filtered = lower + (taps[1] - lower)
            * ((slope - 6.0f) / 6.0f);
    }
    else
    {
        const float cellPosition = slope / 12.0f;
        const int lowerCells = juce::jlimit(
            1, maximumKorg35Cells,
            static_cast<int>(std::floor(cellPosition)));
        const int upperCells = juce::jmin(
            maximumKorg35Cells, lowerCells + 1);
        const float fraction = cellPosition
            - static_cast<float>(lowerCells);
        filtered = taps[static_cast<size_t>(lowerCells)]
            + (taps[static_cast<size_t>(upperCells)]
                - taps[static_cast<size_t>(lowerCells)]) * fraction;
    }

    const float bass = processOnePole(
        filtered, 110.0f, korg35BassLossState[stateIndex]);
    filtered -= bass * (0.78f * bassLoss * safeResonance);
    filtered *= 1.0f + 2.2f * safeResonance * compensation;
    if (internalDistortion)
        filtered = processSelectedDistortion(
            filtered, distortionAmount, engine, channel,
            currentDistortion, previousDistortion, distortionCrossfade,
            korg35InternalDistortionStateBank);

    const float dcCoefficient = std::exp(
        -juce::MathConstants<float>::twoPi * 5.0f / sampleRate);
    const float dcRemoved = sanitiseSample(
        filtered - korg35DcInput[stateIndex]
            + dcCoefficient * korg35DcOutput[stateIndex], 64.0f);
    korg35DcInput[stateIndex] = filtered;
    korg35DcOutput[stateIndex] = dcRemoved;
    return dcRemoved;
}
