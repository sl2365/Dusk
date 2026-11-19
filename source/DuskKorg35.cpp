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

void DuskAudioProcessor::updateKorg35Coefficients(size_t engine) noexcept
{
    const float filterTypePosition = filterTypeValue[engine].getCurrentValue();
    const float cutoffHz = cutoffValue[engine].getCurrentValue();
    const float resonance = resonanceValue[engine].getCurrentValue();
    const float slopeDbPerOctave = slopeValue[engine].getCurrentValue();
    const float chipDrivePercent =
        korg35ChipDriveValue[engine].getCurrentValue();
    const float feedbackClipPercent =
        korg35FeedbackClipValue[engine].getCurrentValue();
    const float biasPercent = korg35BiasValue[engine].getCurrentValue();
    const float feedbackTonePercent =
        korg35FeedbackToneValue[engine].getCurrentValue();
    const float bassLossPercent = korg35BassLossValue[engine].getCurrentValue();
    const float resonanceCompensationPercent =
        korg35ResonanceCompensationValue[engine].getCurrentValue();
    const float gainDb = filterGainValue[engine].getCurrentValue();
    const float sampleRate = static_cast<float>(
        juce::jmax(1.0, currentSampleRate));
    const std::array<float, 12> coefficientParameters {
        sampleRate, filterTypePosition, cutoffHz, resonance,
        slopeDbPerOctave, chipDrivePercent, feedbackClipPercent,
        biasPercent, feedbackTonePercent, bassLossPercent,
        resonanceCompensationPercent, gainDb
    };
    auto& cache = korg35CoefficientCache[engine];
    if (! cache.valid || coefficientParameters != cache.parameters)
    {
        const bool initialise = ! cache.valid;
        const auto& previous = cache.parameters;
        const bool sampleRateChanged = initialise
            || coefficientParameters[0] != previous[0];
        const bool typeChanged = initialise
            || coefficientParameters[1] != previous[1];
        const bool cutoffChanged = sampleRateChanged
            || coefficientParameters[2] != previous[2];
        const bool resonanceChanged = initialise
            || coefficientParameters[3] != previous[3];
        const bool slopeChanged = initialise
            || coefficientParameters[4] != previous[4];
        const bool driveChanged = initialise
            || coefficientParameters[5] != previous[5]
            || coefficientParameters[7] != previous[7];
        const bool feedbackClipChanged = initialise
            || coefficientParameters[6] != previous[6]
            || coefficientParameters[7] != previous[7];
        const bool toneChanged = sampleRateChanged
            || coefficientParameters[8] != previous[8];

        if (initialise || coefficientParameters[11] != previous[11])
        {
            const auto gains = dusk::filtertype::gainsForDecibels(gainDb);
            cache.gainMinusOne = gains.gainMinusOne;
            cache.lowTiltMinusOne = gains.lowTiltMinusOne;
            cache.highTiltMinusOne = gains.highTiltMinusOne;
        }
        if (typeChanged || slopeChanged)
        {
            cache.type = juce::jlimit(
                0.0f, static_cast<float>(dusk::filtertype::count - 1),
                filterTypePosition);
            cache.typeLeft = juce::jlimit(
                0, dusk::filtertype::count - 1,
                static_cast<int>(std::floor(cache.type)));
            cache.typeRight = juce::jmin(
                dusk::filtertype::count - 1, cache.typeLeft + 1);
            cache.typeFraction = cache.type
                - static_cast<float>(cache.typeLeft);
            cache.equaliserType = dusk::filtertype::usesGain(
                static_cast<int>(std::round(cache.type)));
            cache.slope = juce::jlimit(
                6.0f, 48.0f, slopeDbPerOctave);
            cache.activeCells = cache.equaliserType || cache.slope < 12.0f
                ? juce::jmin(2, maximumKorg35Cells)
                : juce::jmin(maximumKorg35Cells,
                    static_cast<int>(std::floor(cache.slope / 12.0f)) + 1);
            cache.lowSlopeFraction = (cache.slope - 6.0f) / 6.0f;
            const float cellPosition = cache.slope / 12.0f;
            cache.lowerCells = juce::jlimit(
                1, maximumKorg35Cells,
                static_cast<int>(std::floor(cellPosition)));
            cache.upperCells = juce::jmin(
                maximumKorg35Cells, cache.lowerCells + 1);
            cache.cellFraction = cellPosition
                - static_cast<float>(cache.lowerCells);
        }
        if (resonanceChanged)
        {
            const float safeResonance = juce::jlimit(
                0.0f, 1.0f, resonance);
            cache.feedbackGain = resonanceFeedbackGain(safeResonance);
        }
        if (cutoffChanged)
        {
            const float maximumCutoff = juce::jmin(
                20000.0f, sampleRate * 0.45f);
            const float safeCutoff = juce::jlimit(
                20.0f, maximumCutoff, cutoffHz);
            const float g = std::tan(
                juce::MathConstants<float>::pi
                    * safeCutoff / sampleRate);
            cache.coefficient = g / (1.0f + g);
        }
        if (toneChanged)
        {
            cache.feedbackTone = juce::jlimit(
                0.0f, 1.0f, feedbackTonePercent * 0.01f);
            const float maximumCutoff = juce::jmin(
                20000.0f, sampleRate * 0.45f);
            const float toneCutoff = juce::jlimit(
                20.0f, maximumCutoff,
                20.0f * std::pow(100.0f, cache.feedbackTone));
            cache.toneCoefficient = 1.0f - std::exp(
                -juce::MathConstants<float>::twoPi
                    * toneCutoff / sampleRate);
        }
        if (sampleRateChanged)
        {
            const float bassG = std::tan(
                juce::MathConstants<float>::pi * 110.0f / sampleRate);
            cache.bassCoefficient = bassG / (1.0f + bassG);
            cache.dcCoefficient = std::exp(
                -juce::MathConstants<float>::twoPi * 5.0f / sampleRate);
        }
        if (driveChanged)
        {
            cache.drive = juce::jlimit(
                0.0f, 1.0f, chipDrivePercent * 0.01f);
            const float bias = juce::jlimit(
                -1.0f, 1.0f, biasPercent * 0.01f);
            cache.driveGain = 1.0f
                + 15.0f * std::pow(cache.drive, 1.25f);
            cache.driveBias = 0.18f * bias * cache.drive;
            cache.driveOffset = std::tanh(
                cache.driveGain * cache.driveBias) / cache.driveGain;
        }
        if (feedbackClipChanged)
        {
            cache.feedbackClip = juce::jlimit(
                0.0f, 1.0f, feedbackClipPercent * 0.01f);
            const float bias = juce::jlimit(
                -1.0f, 1.0f, biasPercent * 0.01f);
            cache.clipThreshold = 2.0f * std::pow(
                0.025f, std::pow(cache.feedbackClip, 1.25f));
            cache.feedbackBias = 0.35f * bias * cache.feedbackClip;
            cache.feedbackOffset = cache.clipThreshold * std::tanh(
                cache.feedbackBias / cache.clipThreshold);
        }
        if (resonanceChanged
            || initialise || coefficientParameters[9] != previous[9])
        {
            const float safeResonance = juce::jlimit(
                0.0f, 1.0f, resonance);
            const float bassLoss = juce::jlimit(
                0.0f, 1.0f, bassLossPercent * 0.01f);
            cache.bassLossAmount = 0.78f * bassLoss * safeResonance;
        }
        if (resonanceChanged
            || initialise || coefficientParameters[10] != previous[10])
        {
            const float safeResonance = juce::jlimit(
                0.0f, 1.0f, resonance);
            const float compensation = juce::jlimit(
                0.0f, 1.0f,
                resonanceCompensationPercent * 0.01f);
            cache.outputCompensation = 1.0f
                + 2.2f * safeResonance * compensation;
        }
        cache.parameters = coefficientParameters;
        cache.valid = true;
    }
}

float DuskAudioProcessor::processKorg35Filter(
    float sample, int engine, int channel, bool internalDistortion,
    float distortionAmount, int currentDistortion, int previousDistortion,
    float distortionCrossfade) noexcept
{
    const int safeEngine = juce::jlimit(0, filterEngineCount - 1, engine);
    const int safeChannel = juce::jlimit(0, channelCount - 1, channel);
    const auto stateIndex = static_cast<size_t>(
        safeEngine * channelCount + safeChannel);
    const auto& cache = korg35CoefficientCache[static_cast<size_t>(safeEngine)];
    const dusk::filtertype::Gains filterTypeGains {
        cache.gainMinusOne, cache.lowTiltMinusOne,
        cache.highTiltMinusOne
    };
    const auto processOnePole = [](float input, float coefficient,
                                   float& state)
    {
        const float v = (input - state) * coefficient;
        const float output = v + state;
        state = DuskAudioProcessor::sanitiseSample(output + v, 64.0f);
        return DuskAudioProcessor::sanitiseSample(output, 64.0f);
    };
    const auto saturateDrive = [&cache](float value)
    {
        if (cache.drive <= 1.0e-6f)
            return value;
        const float saturated = std::tanh(
            cache.driveGain * (value + cache.driveBias))
                / cache.driveGain - cache.driveOffset;
        return value + (saturated - value) * cache.drive;
    };
    const auto saturateFeedback = [&cache](float value)
    {
        if (cache.feedbackClip <= 1.0e-6f)
            return value;
        return cache.clipThreshold * std::tanh(
            (value + cache.feedbackBias) / cache.clipThreshold)
                - cache.feedbackOffset;
    };

    std::array<float, maximumKorg35Cells + 1> taps {};
    taps[0] = sample;
    const bool lowPassOnly = juce::exactlyEqual(cache.type, 0.0f);
    float firstCellBand = 0.0f;
    float cellInput = sample;
    for (int cell = 0; cell < cache.activeCells; ++cell)
    {
        const auto cellIndex = static_cast<size_t>(cell);
        auto& stageOneState = korg35StageOneState[cellIndex][stateIndex];
        auto& stageTwoState = korg35StageTwoState[cellIndex][stateIndex];
        auto& toneState = korg35FeedbackToneState[cellIndex][stateIndex];
        const float cellFeedbackGain = cache.feedbackGain
            * (cell == 0 ? 1.0f : 0.18f);
        float coreInput;
        if (juce::exactlyEqual(cellFeedbackGain, 0.0f))
        {
            coreInput = saturateDrive(cellInput);
        }
        else
        {
            const auto inputForFeedback = [&](float feedback)
            {
                return saturateDrive(cellInput - cellFeedbackGain
                    * saturateFeedback(feedback));
            };
            float feedbackSignal = toneState;
            // Three estimates feed the fourth input evaluation. Only that
            // final input is used to update the cell's persistent state.
            for (int iteration = 0; iteration < 3; ++iteration)
            {
                const float estimate = inputForFeedback(feedbackSignal);
                const float v1 = (estimate - stageOneState) * cache.coefficient;
                const float stageOne = v1 + stageOneState;
                const float v2 = (stageOne - stageTwoState) * cache.coefficient;
                const float low = v2 + stageTwoState;
                const float toneLow = toneState
                    + cache.toneCoefficient * (low - toneState);
                const float toneHigh = low - toneLow;
                const float calculatedFeedback = low
                    + (toneHigh - low) * cache.feedbackTone;
                feedbackSignal += 0.30f * (calculatedFeedback - feedbackSignal);
            }
            coreInput = inputForFeedback(feedbackSignal);
        }
        const float v1 = (coreInput - stageOneState) * cache.coefficient;
        const float stageOne = sanitiseSample(v1 + stageOneState, 64.0f);
        stageOneState = sanitiseSample(stageOne + v1, 64.0f);
        const float v2 = (stageOne - stageTwoState) * cache.coefficient;
        const float low = sanitiseSample(v2 + stageTwoState, 64.0f);
        stageTwoState = sanitiseSample(low + v2, 64.0f);
        toneState = sanitiseSample(
            toneState + cache.toneCoefficient * (low - toneState), 64.0f);
        cellInput = low;
        if (! lowPassOnly)
        {
            const float band = sanitiseSample(2.0f * (stageOne - low), 64.0f);
            const float high = sanitiseSample(
                coreInput - 2.0f * stageOne + low, 64.0f);
            const auto outputs = dusk::filtertype::outputs(
                coreInput, low, band, high, filterTypeGains);
            if (cell == 0)
                firstCellBand = band;
            const float selected = outputs[static_cast<size_t>(cache.typeLeft)]
                + (outputs[static_cast<size_t>(cache.typeRight)]
                    - outputs[static_cast<size_t>(cache.typeLeft)])
                        * cache.typeFraction;
            cellInput = sanitiseSample(selected, 64.0f);
        }
        taps[cellIndex + 1] = cellInput;
    }

    float filtered = 0.0f;
    if (cache.equaliserType)
    {
        filtered = taps[1];
    }
    else if (cache.slope < 12.0f)
    {
        const float onePoleLow = processOnePole(
            sample, cache.coefficient, korg35OnePoleState[stateIndex]);
        float lower = onePoleLow;
        if (! lowPassOnly)
        {
            const auto lowerOutputs = dusk::filtertype::outputs(
                sample, onePoleLow, firstCellBand, sample - onePoleLow,
                filterTypeGains);
            lower = lowerOutputs[static_cast<size_t>(cache.typeLeft)]
                + (lowerOutputs[static_cast<size_t>(cache.typeRight)]
                    - lowerOutputs[static_cast<size_t>(cache.typeLeft)])
                        * cache.typeFraction;
        }
        filtered = lower + (taps[1] - lower)
            * cache.lowSlopeFraction;
    }
    else
    {
        filtered = taps[static_cast<size_t>(cache.lowerCells)]
            + (taps[static_cast<size_t>(cache.upperCells)]
                - taps[static_cast<size_t>(cache.lowerCells)])
                    * cache.cellFraction;
    }

    const float bass = processOnePole(
        filtered, cache.bassCoefficient, korg35BassLossState[stateIndex]);
    filtered -= bass * cache.bassLossAmount;
    filtered *= cache.outputCompensation;
    if (internalDistortion)
        filtered = processSelectedDistortion(
            filtered, distortionAmount, engine, channel,
            currentDistortion, previousDistortion, distortionCrossfade,
            korg35InternalDistortionStateBank);

    const float dcRemoved = sanitiseSample(
        filtered - korg35DcInput[stateIndex]
            + cache.dcCoefficient * korg35DcOutput[stateIndex], 64.0f);
    korg35DcInput[stateIndex] = filtered;
    korg35DcOutput[stateIndex] = dcRemoved;
    return dcRemoved;
}
