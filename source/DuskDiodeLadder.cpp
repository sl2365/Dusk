#include "PluginProcessor.h"
#include "DuskFilterTypes.h"

#include <array>
#include <cmath>

namespace
{
constexpr int diodeLadderInternalDistortionStateBank = 20;
constexpr std::array<std::array<float, 9>, 9> binomialCoefficients {{
    {{ 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }},
    {{ 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }},
    {{ 1.0f, 2.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }},
    {{ 1.0f, 3.0f, 3.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }},
    {{ 1.0f, 4.0f, 6.0f, 4.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f }},
    {{ 1.0f, 5.0f, 10.0f, 10.0f, 5.0f, 1.0f, 0.0f, 0.0f, 0.0f }},
    {{ 1.0f, 6.0f, 15.0f, 20.0f, 15.0f, 6.0f, 1.0f, 0.0f, 0.0f }},
    {{ 1.0f, 7.0f, 21.0f, 35.0f, 35.0f, 21.0f, 7.0f, 1.0f, 0.0f }},
    {{ 1.0f, 8.0f, 28.0f, 56.0f, 70.0f, 56.0f, 28.0f, 8.0f, 1.0f }}
}};

template <typename Float>
Float criticalLadderFeedback(int poles) noexcept
{
    if (poles <= 2)
        return static_cast<Float>(8);
    const auto cornerMagnitude = std::cos(
        juce::MathConstants<Float>::pi / static_cast<Float>(poles));
    return static_cast<Float>(1)
        / std::pow(cornerMagnitude, static_cast<Float>(poles));
}

template <typename Float>
Float diodeCharacter(Float amount) noexcept
{
    return std::pow(juce::jlimit(
        static_cast<Float>(0), static_cast<Float>(1), amount),
        static_cast<Float>(1.35));
}
}

std::complex<double> DuskAudioProcessor::getDiodeLadderResponse(
    double frequencyHz, double sampleRate, float cutoffHz,
    float resonancePercent, float slopeDbPerOctave,
    float stageTiltPercent, float resonanceLossPercent,
    float resonanceCompensationPercent,
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
    const double stageTilt = juce::jlimit(
        -1.0, 1.0, static_cast<double>(stageTiltPercent) * 0.01);

    std::array<std::complex<double>, maximumDiodeLadderStages> stageLow {};
    std::array<std::complex<double>, maximumDiodeLadderStages + 1> lowTaps {};
    lowTaps[0] = { 1.0, 0.0 };
    for (int stage = 0; stage < maximumDiodeLadderStages; ++stage)
    {
        const double position = static_cast<double>(stage)
            / static_cast<double>(maximumDiodeLadderStages - 1) - 0.5;
        const double stageCutoff = juce::jlimit(
            20.0, maximumCutoff,
            safeCutoff * std::pow(2.0, 1.5 * stageTilt * position));
        const double g = std::tan(
            juce::MathConstants<double>::pi
                * stageCutoff / safeSampleRate);
        const double coefficient = g / (1.0 + g);
        stageLow[static_cast<size_t>(stage)] = std::complex<double>(
            coefficient + (1.0 - coefficient) * (2.0 * coefficient)
                / (z - (1.0 - 2.0 * coefficient)));
        lowTaps[static_cast<size_t>(stage + 1)] =
            lowTaps[static_cast<size_t>(stage)]
            * stageLow[static_cast<size_t>(stage)];
    }

    const int filterType = juce::jlimit(
        0, dusk::filtertype::count - 1, filterTypeChoice);
    const auto responseForPoles = [&stageLow, &lowTaps, filterType,
                                   gainDb](int poles)
    {
        poles = juce::jlimit(1, maximumDiodeLadderStages, poles);
        const auto low = lowTaps[static_cast<size_t>(poles)];
        std::complex<double> high(1.0, 0.0);
        for (int stage = 0; stage < poles; ++stage)
            high *= std::complex<double>(1.0, 0.0)
                - stageLow[static_cast<size_t>(stage)];
        int lowOrder = (poles + 1) / 2;
        int highOrder = poles - lowOrder;
        if (highOrder == 0)
            highOrder = 1;
        std::complex<double> band(1.0, 0.0);
        for (int stage = 0; stage < lowOrder; ++stage)
            band *= stageLow[static_cast<size_t>(stage)];
        for (int stage = lowOrder;
             stage < juce::jmin(poles, lowOrder + highOrder); ++stage)
            band *= std::complex<double>(1.0, 0.0)
                - stageLow[static_cast<size_t>(stage)];
        band *= std::pow(2.0, 0.5 * (lowOrder + highOrder));
        return dusk::filtertype::outputs(
            std::complex<double>(1.0, 0.0), low, band, high, gainDb)
                [static_cast<size_t>(filterType)];
    };

    const double polePosition = dusk::filtertype::usesGain(filterType)
        ? 2.0
        : juce::jlimit(
            1.0, static_cast<double>(maximumDiodeLadderStages),
            static_cast<double>(slopeDbPerOctave) / 6.0);
    const int lowerPoles = juce::jlimit(
        1, maximumDiodeLadderStages,
        static_cast<int>(std::floor(polePosition)));
    const int upperPoles = juce::jmin(
        maximumDiodeLadderStages, lowerPoles + 1);
    const double poleFraction = polePosition - lowerPoles;
    const auto lowerResponse = responseForPoles(lowerPoles);
    const auto upperResponse = responseForPoles(upperPoles);
    const auto selectedResponse = lowerResponse
        + (upperResponse - lowerResponse) * poleFraction;
    const auto lowerFeedback = lowTaps[static_cast<size_t>(lowerPoles)];
    const auto upperFeedback = lowTaps[static_cast<size_t>(upperPoles)];
    const auto feedbackResponse = lowerFeedback
        + (upperFeedback - lowerFeedback) * poleFraction;

    const double resonance = juce::jlimit(
        0.0, 1.0, static_cast<double>(resonancePercent) * 0.01);
    const double loss = juce::jlimit(
        0.0, 1.0, static_cast<double>(resonanceLossPercent) * 0.01);
    const double compensation = juce::jlimit(
        0.0, 1.0,
        static_cast<double>(resonanceCompensationPercent) * 0.01);
    const double maximumFeedback = criticalLadderFeedback<double>(lowerPoles)
        + (criticalLadderFeedback<double>(upperPoles)
            - criticalLadderFeedback<double>(lowerPoles)) * poleFraction;
    const double effectiveResonance = resonance
        * (1.0 - 0.78 * loss * resonance);
    const double feedbackGain = 0.97 * maximumFeedback
        * effectiveResonance;
    const double inputGain = 1.0 + 3.5 * resonance * compensation;
    const auto denominator = std::complex<double>(1.0, 0.0)
        + feedbackGain * feedbackResponse;
    const double dcCoefficient = std::exp(
        -juce::MathConstants<double>::twoPi * 5.0 / safeSampleRate);
    const auto dcBlock = (z - std::complex<double>(1.0, 0.0))
        / (z - dcCoefficient);
    if (std::abs(denominator) < 1.0e-12)
        return dcBlock * selectedResponse * inputGain * 1.0e12;
    return dcBlock * selectedResponse * inputGain / denominator;
}

void DuskAudioProcessor::updateDiodeLadderCoefficients(size_t engine) noexcept
{
    const float filterTypePosition = filterTypeValue[engine].getCurrentValue();
    const float cutoffHz = cutoffValue[engine].getCurrentValue();
    const float resonance = resonanceValue[engine].getCurrentValue();
    const float slopeDbPerOctave = slopeValue[engine].getCurrentValue();
    const float diodeSaturationPercent =
        diodeLadderSaturationValue[engine].getCurrentValue();
    const float feedbackClipPercent = diodeLadderFeedbackClipValue[engine].getCurrentValue();
    const float biasPercent = diodeLadderBiasValue[engine].getCurrentValue();
    const float stageTiltPercent = diodeLadderStageTiltValue[engine].getCurrentValue();
    const float resonanceLossPercent =
        diodeLadderResonanceLossValue[engine].getCurrentValue();
    const float resonanceCompensationPercent =
        diodeLadderResonanceCompensationValue[engine].getCurrentValue();
    const float gainDb = filterGainValue[engine].getCurrentValue();
    const float sampleRate = static_cast<float>(
        juce::jmax(1.0, currentSampleRate));
    const std::array<float, 12> coefficientParameters {
        sampleRate, filterTypePosition, cutoffHz, resonance,
        slopeDbPerOctave, diodeSaturationPercent, feedbackClipPercent,
        biasPercent, stageTiltPercent, resonanceLossPercent,
        resonanceCompensationPercent, gainDb
    };
    auto& cache = diodeLadderCoefficientCache[engine];
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
        const bool saturationChanged = initialise
            || coefficientParameters[5] != previous[5]
            || coefficientParameters[7] != previous[7];
        const bool feedbackClipChanged = initialise
            || coefficientParameters[6] != previous[6]
            || coefficientParameters[7] != previous[7];
        const bool stageTiltChanged = initialise
            || coefficientParameters[8] != previous[8];
        const bool topologyChanged = typeChanged || slopeChanged;
        const bool coefficientsChanged = cutoffChanged
            || stageTiltChanged || topologyChanged;

        if (initialise || coefficientParameters[11] != previous[11])
        {
            const auto gains = dusk::filtertype::gainsForDecibels(gainDb);
            cache.gainMinusOne = gains.gainMinusOne;
            cache.lowTiltMinusOne = gains.lowTiltMinusOne;
            cache.highTiltMinusOne = gains.highTiltMinusOne;
        }
        if (topologyChanged)
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
            const int filterTypeChoice = juce::jlimit(
                0, dusk::filtertype::count - 1,
                static_cast<int>(std::round(filterTypePosition)));
            const float polePosition = dusk::filtertype::usesGain(
                filterTypeChoice)
                ? 2.0f
                : juce::jlimit(
                    1.0f,
                    static_cast<float>(maximumDiodeLadderStages),
                    slopeDbPerOctave / 6.0f);
            cache.lowerPoles = juce::jlimit(
                1, maximumDiodeLadderStages,
                static_cast<int>(std::floor(polePosition)));
            cache.upperPoles = juce::jmin(
                maximumDiodeLadderStages, cache.lowerPoles + 1);
            cache.poleFraction = polePosition
                - static_cast<float>(cache.lowerPoles);
            const auto bandScaleForPoles = [](int poles)
            {
                const int lowOrder = (poles + 1) / 2;
                int highOrder = poles - lowOrder;
                if (highOrder == 0)
                    highOrder = 1;
                return std::pow(
                    2.0f, 0.5f * static_cast<float>(
                        lowOrder + highOrder));
            };
            cache.lowerBandScale = bandScaleForPoles(cache.lowerPoles);
            cache.upperBandScale = bandScaleForPoles(cache.upperPoles);
        }
        if (saturationChanged)
        {
            const float saturation = juce::jlimit(
                0.0f, 1.0f, diodeSaturationPercent * 0.01f);
            const float bias = juce::jlimit(
                -1.0f, 1.0f, biasPercent * 0.01f);
            cache.saturationShape = diodeCharacter(saturation);
            const float saturationDrive = 1.0f
                + 39.0f * cache.saturationShape;
            cache.positiveThreshold = std::pow(
                5.0f, bias * cache.saturationShape) / saturationDrive;
            cache.negativeThreshold = std::pow(
                5.0f, -bias * cache.saturationShape) / saturationDrive;
        }
        if (feedbackClipChanged)
        {
            cache.feedbackClip = juce::jlimit(
                0.0f, 1.0f, feedbackClipPercent * 0.01f);
            const float bias = juce::jlimit(
                -1.0f, 1.0f, biasPercent * 0.01f);
            cache.clipShape = diodeCharacter(cache.feedbackClip);
            const float clipDrive = 1.0f + 47.0f * cache.clipShape;
            cache.clipPositiveThreshold = std::pow(
                4.0f, bias * cache.clipShape) / clipDrive;
            cache.clipNegativeThreshold = std::pow(
                4.0f, -bias * cache.clipShape) / clipDrive;
        }
        if (resonanceChanged
            || initialise || coefficientParameters[10] != previous[10])
        {
            const float safeResonance = juce::jlimit(
                0.0f, 1.0f, resonance);
            const float compensation = juce::jlimit(
                0.0f, 1.0f,
                resonanceCompensationPercent * 0.01f);
            cache.inputGain = 1.0f
                + 3.5f * safeResonance * compensation;
        }
        if (coefficientsChanged)
        {
            const float maximumCutoff = juce::jmin(
                20000.0f, sampleRate * 0.45f);
            const float safeCutoff = juce::jlimit(
                20.0f, maximumCutoff, cutoffHz);
            const float stageTilt = juce::jlimit(
                -1.0f, 1.0f, stageTiltPercent * 0.01f);
            cache.coefficients.fill(0.0f);
            for (int stage = 0; stage < cache.upperPoles; ++stage)
            {
                const float position = static_cast<float>(stage)
                    / static_cast<float>(maximumDiodeLadderStages - 1)
                    - 0.5f;
                const float stageCutoff = juce::jlimit(
                    20.0f, maximumCutoff,
                    safeCutoff * std::pow(
                        2.0f, 1.5f * stageTilt * position));
                const float g = std::tan(
                    juce::MathConstants<float>::pi
                        * stageCutoff / sampleRate);
                cache.coefficients[static_cast<size_t>(stage)]
                    = g / (1.0f + g);
            }
        }
        if (resonanceChanged || topologyChanged
            || initialise || coefficientParameters[9] != previous[9])
        {
            const float safeResonance = juce::jlimit(
                0.0f, 1.0f, resonance);
            const float resonanceLoss = juce::jlimit(
                0.0f, 1.0f, resonanceLossPercent * 0.01f);
            const float lowerMaximumFeedback
                = criticalLadderFeedback<float>(cache.lowerPoles);
            const float upperMaximumFeedback
                = criticalLadderFeedback<float>(cache.upperPoles);
            const float maximumFeedback = lowerMaximumFeedback
                + (upperMaximumFeedback - lowerMaximumFeedback)
                    * cache.poleFraction;
            const float effectiveResonance = safeResonance
                * (1.0f - 0.78f * resonanceLoss * safeResonance);
            cache.feedbackGain = 0.97f * maximumFeedback
                * effectiveResonance;
        }
        if (coefficientsChanged)
        {
            float lowerFeedthrough = 1.0f;
            for (int stage = 0; stage < cache.lowerPoles; ++stage)
                lowerFeedthrough *= cache.coefficients[
                    static_cast<size_t>(stage)];
            float upperFeedthrough = lowerFeedthrough;
            if (cache.upperPoles > cache.lowerPoles)
                upperFeedthrough *= cache.coefficients[
                    static_cast<size_t>(cache.lowerPoles)];
            cache.feedbackFeedthrough = lowerFeedthrough
                + (upperFeedthrough - lowerFeedthrough)
                    * cache.poleFraction;
        }
        cache.parameters = coefficientParameters;
        cache.valid = true;
    }
}

float DuskAudioProcessor::processDiodeLadderFilter(
    float sample, int engine, int channel, bool internalDistortion,
    float distortionAmount, int currentDistortion, int previousDistortion,
    float distortionCrossfade) noexcept
{
    const int safeEngine = juce::jlimit(0, filterEngineCount - 1, engine);
    const int safeChannel = juce::jlimit(0, channelCount - 1, channel);
    const auto stateIndex = static_cast<size_t>(
        safeEngine * channelCount + safeChannel);
    const auto& cache = diodeLadderCoefficientCache[static_cast<size_t>(safeEngine)];
    const dusk::filtertype::Gains filterTypeGains {
        cache.gainMinusOne, cache.lowTiltMinusOne,
        cache.highTiltMinusOne
    };
    const auto saturateDiode = [&cache](float value)
    {
        if (cache.saturationShape <= 1.0e-6f)
            return value;
        const float threshold = value >= 0.0f
            ? cache.positiveThreshold : cache.negativeThreshold;
        const float curved = threshold * std::tanh(value / threshold);
        return value + (curved - value) * cache.saturationShape;
    };

    const int lowerPoles = cache.lowerPoles;
    const int upperPoles = cache.upperPoles;
    const float poleFraction = cache.poleFraction;
    const auto& coefficients = cache.coefficients;
    const float feedbackGain = cache.feedbackGain;

    std::array<float, maximumDiodeLadderStages + 1> memoryTaps {};
    float memoryInput = 0.0f;
    for (int stage = 0; stage < upperPoles; ++stage)
    {
        const auto stageIndex = static_cast<size_t>(stage);
        const float coefficient = coefficients[stageIndex];
        const float state = diodeLadderStageState[stageIndex][stateIndex];
        const float output = coefficient * memoryInput
            + (1.0f - coefficient) * state;
        memoryTaps[stageIndex + 1] = output;
        memoryInput = output;
    }
    const float memoryFeedback = memoryTaps[static_cast<size_t>(lowerPoles)]
        + (memoryTaps[static_cast<size_t>(upperPoles)]
            - memoryTaps[static_cast<size_t>(lowerPoles)]) * poleFraction;
    const float internalMemoryFeedback = internalDistortion
        ? sanitiseSample(processSelectedDistortion(
            memoryFeedback, distortionAmount, engine, channel,
            currentDistortion, previousDistortion, distortionCrossfade,
            diodeLadderInternalDistortionStateBank), 8.0f)
        : memoryFeedback;
    const float feedbackFeedthrough = cache.feedbackFeedthrough;

    const float drivenInput = sample * cache.inputGain;
    float ladderInput = (drivenInput
        - feedbackGain * internalMemoryFeedback)
        / (1.0f + feedbackGain * feedbackFeedthrough);
    if (cache.feedbackClip > 1.0e-6f && feedbackGain > 1.0e-6f)
    {
        for (int iteration = 0; iteration < 4; ++iteration)
        {
            const float feedbackSignal = internalMemoryFeedback
                + feedbackFeedthrough * ladderInput;
            const float threshold = feedbackSignal >= 0.0f
                ? cache.clipPositiveThreshold
                : cache.clipNegativeThreshold;
            const float curve = std::tanh(feedbackSignal / threshold);
            const float curvedFeedback = threshold * curve;
            const float clippedFeedback = feedbackSignal
                + (curvedFeedback - feedbackSignal) * cache.clipShape;
            const float clippingSlope = 1.0f
                + ((1.0f - curve * curve) - 1.0f) * cache.clipShape;
            const float derivative = 1.0f
                + feedbackGain * feedbackFeedthrough * clippingSlope;
            if (std::abs(derivative) <= 1.0e-6f)
                break;
            const float previousInput = ladderInput;
            ladderInput -= (ladderInput
                + feedbackGain * clippedFeedback - drivenInput)
                    / derivative;
            ladderInput = sanitiseSample(ladderInput, 64.0f);
            if (juce::exactlyEqual(ladderInput, previousInput))
                break;
        }
    }
    ladderInput = sanitiseSample(ladderInput, 64.0f);

    std::array<float, maximumDiodeLadderStages + 1> taps {};
    taps[0] = ladderInput;
    float stageInput = ladderInput;
    for (int stage = 0; stage < upperPoles; ++stage)
    {
        const auto stageIndex = static_cast<size_t>(stage);
        auto& state = diodeLadderStageState[stageIndex][stateIndex];
        const float delta = coefficients[stageIndex]
            * (saturateDiode(stageInput) - saturateDiode(state));
        const float output = sanitiseSample(state + delta, 64.0f);
        state = sanitiseSample(output + delta, 64.0f);
        taps[stageIndex + 1] = output;
        stageInput = output;
    }

    const auto outputsForPoles = [&taps, &filterTypeGains](
        int poles, float bandScale)
    {
        poles = juce::jlimit(1, maximumDiodeLadderStages, poles);
        const float low = taps[static_cast<size_t>(poles)];
        float high = 0.0f;
        for (int index = 0; index <= poles; ++index)
            high += ((index & 1) == 0 ? 1.0f : -1.0f)
                * binomialCoefficients[static_cast<size_t>(poles)]
                    [static_cast<size_t>(index)]
                * taps[static_cast<size_t>(index)];
        int lowOrder = (poles + 1) / 2;
        int highOrder = poles - lowOrder;
        if (highOrder == 0)
            highOrder = 1;
        float band = 0.0f;
        for (int index = 0; index <= highOrder; ++index)
            band += ((index & 1) == 0 ? 1.0f : -1.0f)
                * binomialCoefficients[static_cast<size_t>(highOrder)]
                    [static_cast<size_t>(index)]
                * taps[static_cast<size_t>(lowOrder + index)];
        band *= bandScale;
        return dusk::filtertype::outputs(
            taps[0], low, band, high, filterTypeGains);
    };
    const auto selectType = [&cache](
        const std::array<float, dusk::filtertype::count>& outputs)
    {
        return outputs[static_cast<size_t>(cache.typeLeft)]
            + (outputs[static_cast<size_t>(cache.typeRight)]
                - outputs[static_cast<size_t>(cache.typeLeft)])
                    * cache.typeFraction;
    };
    const auto selectedForPoles = [&](int poles, float bandScale)
    {
        if (juce::exactlyEqual(cache.type, 0.0f))
            return taps[static_cast<size_t>(poles)];
        return selectType(outputsForPoles(poles, bandScale));
    };
    float selected = selectedForPoles(lowerPoles, cache.lowerBandScale);
    if (! juce::exactlyEqual(poleFraction, 0.0f))
    {
        const float upper = selectedForPoles(upperPoles, cache.upperBandScale);
        selected += (upper - selected) * poleFraction;
    }
    selected = sanitiseSample(selected, 64.0f);
    const float dcRemoved = sanitiseSample(
        selected - diodeLadderDcInput[stateIndex]
            + coreAsymmetryDcCoefficient
                * diodeLadderDcOutput[stateIndex],
        64.0f);
    diodeLadderDcInput[stateIndex] = selected;
    diodeLadderDcOutput[stateIndex] = dcRemoved;
    return dcRemoved;
}
