#pragma once

#include <JuceHeader.h>
#include <array>
#include <algorithm>
#include <cmath>

namespace dusk::sequencer
{
constexpr int stepCount = 32;
constexpr int targetSlotCount = 16;
constexpr int rateChoiceCount = 14;

enum class Direction
{
    loop = 0,
    bounce,
    reverse,
    played,
    random
};

enum class GateStepMode
{
    off = 0,
    shortStep,
    longStep,
    rest
};

enum class Waveform
{
    saw = 0,
    sawDouble,
    sawDown,
    sawDownDouble,
    sine,
    sineDouble,
    triangle,
    triangleDouble,
    pulse25,
    pulse25Double,
    square,
    squareDouble,
    pulse75,
    pulse75Double
};

struct TargetInfo
{
    const char* parameterId;
    const char* displayName;
};

inline constexpr std::array<TargetInfo, 100> targets
{{
    { "mix", "Dry / Wet" },
    { "output", "Output" },
    { "filter1Drive", "F1 Drive" },
    { "filter2Drive", "F2 Drive" },
    { "filter1Cutoff", "F1 Cutoff" },
    { "filter1Resonance", "F1 Resonance" },
    { "filter2Cutoff", "F2 Cutoff" },
    { "filter2Resonance", "F2 Resonance" },
    { "sequencerAttack", "Seq Attack" },
    { "sequencerRelease", "Seq Release" },
    { "sequencerPeak", "Seq Depth" },
    { "sequencerMove", "Seq Offset" },
    { "sequencerStart", "Seq Start" },
    { "sequencerEnd", "Seq End" },
    { "sequencerGateShortLength", "Gate Short" },
    { "sequencerGateLongLength", "Gate Long" },
    { "filter1Slope", "F1 Slope" },
    { "filter2Slope", "F2 Slope" },
    { "filter1ResonanceShape", "F1 Res Shape" },
    { "filter2ResonanceShape", "F2 Res Shape" },
    { "filter1BassLoss", "F1 Bass Loss" },
    { "filter2BassLoss", "F2 Bass Loss" },
    { "filter1ResonanceLimit", "F1 Res Limit" },
    { "filter2ResonanceLimit", "F2 Res Limit" },
    { "filter1CoreSaturation", "F1 Core Sat" },
    { "filter2CoreSaturation", "F2 Core Sat" },
    { "filter1SaturationAsymmetry", "F1 Sat Asym" },
    { "filter2SaturationAsymmetry", "F2 Sat Asym" },
    { "filter1PoleSpread", "F1 Pole Spread" },
    { "filter2PoleSpread", "F2 Pole Spread" },
    { "filter1Feedback", "F1 Feedback" },
    { "filter2Feedback", "F2 Feedback" },
    { "filter1FeedbackLimit", "F1 FB Limit" },
    { "filter2FeedbackLimit", "F2 FB Limit" },
    { "filter1LadderStageSaturation", "F1 Stage Sat" },
    { "filter2LadderStageSaturation", "F2 Stage Sat" },
    { "filter1LadderResonanceCompensation", "F1 Res Comp" },
    { "filter2LadderResonanceCompensation", "F2 Res Comp" },
    { "filter1LadderFeedbackSaturation", "F1 FB Sat" },
    { "filter2LadderFeedbackSaturation", "F2 FB Sat" },
    { "filter1OtaCellDrive", "F1 OTA Drive" },
    { "filter2OtaCellDrive", "F2 OTA Drive" },
    { "filter1OtaFeedbackDamping", "F1 OTA Damping" },
    { "filter2OtaFeedbackDamping", "F2 OTA Damping" },
    { "filter1OtaResonanceCompensation", "F1 OTA Res Comp" },
    { "filter2OtaResonanceCompensation", "F2 OTA Res Comp" },
    { "filter1CombFrequency", "F1 Comb Frequency" },
    { "filter2CombFrequency", "F2 Comb Frequency" },
    { "filter1CombFeedback", "F1 Comb Feedback" },
    { "filter2CombFeedback", "F2 Comb Feedback" },
    { "filter1CombDamping", "F1 Comb Damping" },
    { "filter2CombDamping", "F2 Comb Damping" },
    { "filter1CombFineTune", "F1 Comb Fine Tune" },
    { "filter2CombFineTune", "F2 Comb Fine Tune" },
    { "filter1CombPolarity", "F1 Comb Polarity" },
    { "filter2CombPolarity", "F2 Comb Polarity" },
    { "filter1CombStereoSpread", "F1 Comb Stereo" },
    { "filter2CombStereoSpread", "F2 Comb Stereo" },
    { "filter1CombDispersion", "F1 Comb Dispersion" },
    { "filter2CombDispersion", "F2 Comb Dispersion" },
    { "filter1CombLoopSaturation", "F1 Comb Loop Sat" },
    { "filter2CombLoopSaturation", "F2 Comb Loop Sat" },
    { "filter1CombFeedbackLimit", "F1 Comb FB Limit" },
    { "filter2CombFeedbackLimit", "F2 Comb FB Limit" },
    { "filter1FormantVowel", "F1 Formant Vowel" },
    { "filter2FormantVowel", "F2 Formant Vowel" },
    { "filter1FormantShift", "F1 Formant Shift" },
    { "filter2FormantShift", "F2 Formant Shift" },
    { "filter1FormantFocus", "F1 Formant Focus" },
    { "filter2FormantFocus", "F2 Formant Focus" },
    { "filter1FormantOneLevel", "F1 Formant 1 Level" },
    { "filter2FormantOneLevel", "F2 Formant 1 Level" },
    { "filter1FormantTwoLevel", "F1 Formant 2 Level" },
    { "filter2FormantTwoLevel", "F2 Formant 2 Level" },
    { "filter1FormantThreeLevel", "F1 Formant 3 Level" },
    { "filter2FormantThreeLevel", "F2 Formant 3 Level" },
    { "filter1FormantBandwidth", "F1 Formant Width" },
    { "filter2FormantBandwidth", "F2 Formant Width" },
    { "filter1FormantSpread", "F1 Formant Spread" },
    { "filter2FormantSpread", "F2 Formant Spread" },
    { "filter1FormantTilt", "F1 Formant Tilt" },
    { "filter2FormantTilt", "F2 Formant Tilt" },
    { "filter1PhaserFrequency", "F1 Phaser Frequency" },
    { "filter2PhaserFrequency", "F2 Phaser Frequency" },
    { "filter1PhaserFeedback", "F1 Phaser Feedback" },
    { "filter2PhaserFeedback", "F2 Phaser Feedback" },
    { "filter1PhaserDepth", "F1 Phaser Depth" },
    { "filter2PhaserDepth", "F2 Phaser Depth" },
    { "filter1PhaserStages", "F1 Phaser Stages" },
    { "filter2PhaserStages", "F2 Phaser Stages" },
    { "filter1PhaserSpread", "F1 Phaser Spread" },
    { "filter2PhaserSpread", "F2 Phaser Spread" },
    { "filter1PhaserSkew", "F1 Phaser Skew" },
    { "filter2PhaserSkew", "F2 Phaser Skew" },
    { "filter1PhaserDamping", "F1 Phaser Damping" },
    { "filter2PhaserDamping", "F2 Phaser Damping" },
    { "filter1PhaserStageSaturation", "F1 Phaser Stage Sat" },
    { "filter2PhaserStageSaturation", "F2 Phaser Stage Sat" },
    { "filter1PhaserStereo", "F1 Phaser Stereo" },
    { "filter2PhaserStereo", "F2 Phaser Stereo" }
}};

inline constexpr int mixTargetChoice = 1;
inline constexpr int outputTargetChoice = 2;
inline constexpr int filter1DriveTargetChoice = 3;
inline constexpr int filter2DriveTargetChoice = 4;
inline constexpr int filter1CutoffTargetChoice = 5;
inline constexpr int filter1ResonanceTargetChoice = 6;
inline constexpr int filter2CutoffTargetChoice = 7;
inline constexpr int filter2ResonanceTargetChoice = 8;
inline constexpr int sequencerAttackTargetChoice = 9;
inline constexpr int sequencerReleaseTargetChoice = 10;
inline constexpr int sequencerDepthTargetChoice = 11;
inline constexpr int sequencerOffsetTargetChoice = 12;
inline constexpr int sequencerStartTargetChoice = 13;
inline constexpr int sequencerEndTargetChoice = 14;
inline constexpr int sequencerGateShortTargetChoice = 15;
inline constexpr int sequencerGateLongTargetChoice = 16;
inline constexpr int filter1SlopeTargetChoice = 17;
inline constexpr int filter2SlopeTargetChoice = 18;
inline constexpr int filter1ResonanceShapeTargetChoice = 19;
inline constexpr int filter2ResonanceShapeTargetChoice = 20;
inline constexpr int filter1BassLossTargetChoice = 21;
inline constexpr int filter2BassLossTargetChoice = 22;
inline constexpr int filter1ResonanceLimitTargetChoice = 23;
inline constexpr int filter2ResonanceLimitTargetChoice = 24;
inline constexpr int filter1CoreSaturationTargetChoice = 25;
inline constexpr int filter2CoreSaturationTargetChoice = 26;
inline constexpr int filter1SaturationAsymmetryTargetChoice = 27;
inline constexpr int filter2SaturationAsymmetryTargetChoice = 28;
inline constexpr int filter1PoleSpreadTargetChoice = 29;
inline constexpr int filter2PoleSpreadTargetChoice = 30;
inline constexpr int filter1FeedbackTargetChoice = 31;
inline constexpr int filter2FeedbackTargetChoice = 32;
inline constexpr int filter1FeedbackLimitTargetChoice = 33;
inline constexpr int filter2FeedbackLimitTargetChoice = 34;
inline constexpr int filter1LadderStageSaturationTargetChoice = 35;
inline constexpr int filter2LadderStageSaturationTargetChoice = 36;
inline constexpr int filter1LadderResonanceCompensationTargetChoice = 37;
inline constexpr int filter2LadderResonanceCompensationTargetChoice = 38;
inline constexpr int filter1LadderFeedbackSaturationTargetChoice = 39;
inline constexpr int filter2LadderFeedbackSaturationTargetChoice = 40;
inline constexpr int filter1OtaCellDriveTargetChoice = 41;
inline constexpr int filter2OtaCellDriveTargetChoice = 42;
inline constexpr int filter1OtaFeedbackDampingTargetChoice = 43;
inline constexpr int filter2OtaFeedbackDampingTargetChoice = 44;
inline constexpr int filter1OtaResonanceCompensationTargetChoice = 45;
inline constexpr int filter2OtaResonanceCompensationTargetChoice = 46;
inline constexpr int filter1CombFrequencyTargetChoice = 47;
inline constexpr int filter2CombFrequencyTargetChoice = 48;
inline constexpr int filter1CombFeedbackTargetChoice = 49;
inline constexpr int filter2CombFeedbackTargetChoice = 50;
inline constexpr int filter1CombDampingTargetChoice = 51;
inline constexpr int filter2CombDampingTargetChoice = 52;
inline constexpr int filter1CombFineTuneTargetChoice = 53;
inline constexpr int filter2CombFineTuneTargetChoice = 54;
inline constexpr int filter1CombPolarityTargetChoice = 55;
inline constexpr int filter2CombPolarityTargetChoice = 56;
inline constexpr int filter1CombStereoSpreadTargetChoice = 57;
inline constexpr int filter2CombStereoSpreadTargetChoice = 58;
inline constexpr int filter1CombDispersionTargetChoice = 59;
inline constexpr int filter2CombDispersionTargetChoice = 60;
inline constexpr int filter1CombLoopSaturationTargetChoice = 61;
inline constexpr int filter2CombLoopSaturationTargetChoice = 62;
inline constexpr int filter1CombFeedbackLimitTargetChoice = 63;
inline constexpr int filter2CombFeedbackLimitTargetChoice = 64;
inline constexpr int filter1FormantVowelTargetChoice = 65;
inline constexpr int filter2FormantVowelTargetChoice = 66;
inline constexpr int filter1FormantShiftTargetChoice = 67;
inline constexpr int filter2FormantShiftTargetChoice = 68;
inline constexpr int filter1FormantFocusTargetChoice = 69;
inline constexpr int filter2FormantFocusTargetChoice = 70;
inline constexpr int filter1FormantOneLevelTargetChoice = 71;
inline constexpr int filter2FormantOneLevelTargetChoice = 72;
inline constexpr int filter1FormantTwoLevelTargetChoice = 73;
inline constexpr int filter2FormantTwoLevelTargetChoice = 74;
inline constexpr int filter1FormantThreeLevelTargetChoice = 75;
inline constexpr int filter2FormantThreeLevelTargetChoice = 76;
inline constexpr int filter1FormantBandwidthTargetChoice = 77;
inline constexpr int filter2FormantBandwidthTargetChoice = 78;
inline constexpr int filter1FormantSpreadTargetChoice = 79;
inline constexpr int filter2FormantSpreadTargetChoice = 80;
inline constexpr int filter1FormantTiltTargetChoice = 81;
inline constexpr int filter2FormantTiltTargetChoice = 82;
inline constexpr int filter1PhaserFrequencyTargetChoice = 83;
inline constexpr int filter2PhaserFrequencyTargetChoice = 84;
inline constexpr int filter1PhaserFeedbackTargetChoice = 85;
inline constexpr int filter2PhaserFeedbackTargetChoice = 86;
inline constexpr int filter1PhaserDepthTargetChoice = 87;
inline constexpr int filter2PhaserDepthTargetChoice = 88;
inline constexpr int filter1PhaserStagesTargetChoice = 89;
inline constexpr int filter2PhaserStagesTargetChoice = 90;
inline constexpr int filter1PhaserSpreadTargetChoice = 91;
inline constexpr int filter2PhaserSpreadTargetChoice = 92;
inline constexpr int filter1PhaserSkewTargetChoice = 93;
inline constexpr int filter2PhaserSkewTargetChoice = 94;
inline constexpr int filter1PhaserDampingTargetChoice = 95;
inline constexpr int filter2PhaserDampingTargetChoice = 96;
inline constexpr int filter1PhaserStageSaturationTargetChoice = 97;
inline constexpr int filter2PhaserStageSaturationTargetChoice = 98;
inline constexpr int filter1PhaserStereoTargetChoice = 99;
inline constexpr int filter2PhaserStereoTargetChoice = 100;

inline int targetChoiceForParameter(const juce::String& parameterId)
{
    for (size_t index = 0; index < targets.size(); ++index)
        if (parameterId == targets[index].parameterId)
            return static_cast<int>(index) + 1;
    return 0;
}

inline juce::String targetName(int choice)
{
    if (choice <= 0 || choice > static_cast<int>(targets.size()))
        return {};
    return targets[static_cast<size_t>(choice - 1)].displayName;
}

inline juce::String stepParameterId(int step)
{
    return "seqStep" + juce::String(step + 1);
}

inline juce::String gateStepParameterId(int step)
{
    return "sequencerGateStep" + juce::String(step + 1);
}

inline GateStepMode gateStepModeFromChoice(float choice) noexcept
{
    return static_cast<GateStepMode>(juce::jlimit(
        static_cast<int>(GateStepMode::off),
        static_cast<int>(GateStepMode::rest),
        static_cast<int>(std::lround(choice))));
}

inline float gateStepTarget(GateStepMode mode, float position,
                            float shortLength, float longLength) noexcept
{
    position = juce::jlimit(0.0f, 1.0f, position);
    shortLength = juce::jlimit(0.10f, 0.60f, shortLength);
    longLength = juce::jlimit(0.65f, 0.95f, longLength);

    switch (mode)
    {
        case GateStepMode::shortStep:
            return position < shortLength ? 1.0f : 0.0f;
        case GateStepMode::longStep:
            return position < longLength ? 1.0f : 0.0f;
        case GateStepMode::rest:
            return 1.0f;
        case GateStepMode::off:
        default:
            return 0.0f;
    }
}

inline juce::String targetParameterId(int slot)
{
    return "seqTarget" + juce::String(slot + 1);
}

inline juce::String targetEnabledParameterId(int slot)
{
    return "seqTargetEnabled" + juce::String(slot + 1);
}

inline juce::String targetDepthParameterId(int slot)
{
    return targetParameterId(slot) + "_depth";
}

inline float applyTargetDepth(float canonicalSequenceValue, bool,
                              float depth) noexcept
{
    // Both sequence displays share one canonical domain: 0.5 is
    // neutral, Unipolar uses 0.5-1.0 and Bipolar uses 0.0-1.0. Moving towards
    // that neutral point makes 0% hold the saved destination and 100% retain
    // the established modulation exactly.
    canonicalSequenceValue = juce::jlimit(
        0.0f, 1.0f, canonicalSequenceValue);
    depth = juce::jlimit(0.0f, 1.0f, depth);
    return 0.5f + (canonicalSequenceValue - 0.5f) * depth;
}

inline double beatsForRate(int rateIndex) noexcept
{
    constexpr std::array<double, rateChoiceCount> beats
    {
        1.0 / 32.0, 1.0 / 24.0, 1.0 / 16.0, 1.0 / 12.0,
        1.0 / 8.0, 1.0 / 6.0, 1.0 / 4.0, 1.0 / 3.0,
        1.0 / 2.0, 2.0 / 3.0, 1.0, 4.0 / 3.0, 2.0, 4.0
    };
    return beats[static_cast<size_t>(juce::jlimit(
        0, rateChoiceCount - 1, rateIndex))];
}

inline unsigned hash(unsigned value) noexcept
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

inline int greatestCommonDivisor(int left, int right) noexcept
{
    while (right != 0)
    {
        const int remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

inline int randomOffset(int tick, int length) noexcept
{
    if (length <= 1)
        return 0;
    const int shuffle = tick / length;
    const int position = tick % length;
    const unsigned seed = hash(static_cast<unsigned>(length * 131
                                                     + shuffle * 977));
    int stride = 1 + static_cast<int>(seed
        % static_cast<unsigned>(length - 1));
    while (greatestCommonDivisor(stride, length) != 1)
        stride = stride % (length - 1) + 1;
    const int offset = static_cast<int>(
        hash(static_cast<unsigned>(length) ^ 0x9e3779b9U)
        % static_cast<unsigned>(length));
    return (offset + (shuffle & 1) + position * stride) % length;
}

struct Position
{
    int current = 0;
    int previous = 0;
    float fraction = 0.0f;
};

inline Position positionForPhase(double phase, int length,
                                 Direction direction) noexcept
{
    length = juce::jmax(1, length);
    const int cycleLength = direction == Direction::bounce && length > 1
                          ? 2 * (length - 1)
                          : direction == Direction::random ? length * 8
                          : length;
    phase = std::fmod(phase, static_cast<double>(cycleLength));
    if (phase < 0.0)
        phase += static_cast<double>(cycleLength);
    const int tick = juce::jlimit(
        0, cycleLength - 1, static_cast<int>(std::floor(phase)));
    const auto offsetAt = [=](int value)
    {
        value = (value % cycleLength + cycleLength) % cycleLength;
        if (direction == Direction::reverse)
            return length - 1 - value;
        if (direction == Direction::bounce && value >= length)
            return cycleLength - value;
        if (direction == Direction::random)
            return randomOffset(value, length);
        return value;
    };
    return { offsetAt(tick), offsetAt(tick - 1),
             static_cast<float>(phase - std::floor(phase)) };
}

inline float transition(float previous, float current, float position,
                        float attack, float release) noexcept
{
    previous = juce::jlimit(0.0f, 1.0f, previous);
    current = juce::jlimit(0.0f, 1.0f, current);
    if (std::abs(current - previous) <= 0.0001f)
        return current;
    const float amount = juce::jlimit(
        0.0f, 1.0f, current >= previous ? attack : release);
    if (amount <= 0.000001f)
        return current;
    const float progress = juce::jlimit(0.0f, 1.0f, position / amount);
    const float eased = 1.0f - (1.0f - progress) * (1.0f - progress);
    return previous + eased * (current - previous);
}

inline float applyDepthOffset(float value, float minimum, float maximum,
                              float depth, float offsetAmount,
                              bool bipolar) noexcept
{
    value = juce::jlimit(0.0f, 1.0f, value);
    minimum = juce::jlimit(0.0f, 1.0f, minimum);
    maximum = juce::jlimit(minimum, 1.0f, maximum);
    depth = juce::jlimit(-1.0f, 1.0f, depth);
    offsetAmount = juce::jlimit(-1.0f, 1.0f, offsetAmount);
    const float lowerLimit = bipolar ? -1.0f : 0.0f;
    const auto toWorking = [lowerLimit](float input)
    {
        return juce::jlimit(lowerLimit, 1.0f, 2.0f * input - 1.0f);
    };
    const auto fromWorking = [](float input)
    {
        return juce::jlimit(0.0f, 1.0f, 0.5f * (input + 1.0f));
    };
    float workingValue = toWorking(value);
    float workingMinimum = toWorking(minimum);
    float workingMaximum = toWorking(maximum);
    const float excursion = bipolar
        ? juce::jmax(std::abs(workingMinimum),
                     std::abs(workingMaximum))
        : workingMaximum;
    float scale = depth <= 0.0f ? 1.0f + depth : 1.0f;
    if (depth > 0.0f && excursion > 0.000001f)
        scale = 1.0f + depth * (1.0f / excursion - 1.0f);
    workingValue *= scale;
    workingMinimum *= scale;
    workingMaximum *= scale;
    const float offset = offsetAmount >= 0.0f
        ? offsetAmount * (1.0f - workingMaximum)
        : (-offsetAmount) * (lowerLimit - workingMinimum);
    return fromWorking(workingValue + offset);
}

inline float evaluate(
    const std::array<float, stepCount>& steps,
    double phase, int firstStep, int lastStep, Direction direction,
    float attack, float release, int* activeStep = nullptr) noexcept
{
    firstStep = juce::jlimit(0, stepCount - 1, firstStep);
    lastStep = juce::jlimit(firstStep, stepCount - 1, lastStep);
    const auto position = positionForPhase(
        phase, lastStep - firstStep + 1, direction);
    const int currentStep = firstStep + position.current;
    const int previousStep = firstStep + position.previous;
    if (activeStep != nullptr)
        *activeStep = currentStep;
    return transition(steps[static_cast<size_t>(previousStep)],
                      steps[static_cast<size_t>(currentStep)],
                      position.fraction, attack, release);
}

inline std::array<float, stepCount> makeWaveform(Waveform waveform,
                                                 bool bipolar) noexcept
{
    std::array<float, stepCount> result {};
    const int preset = static_cast<int>(waveform);
    const bool doubled = (preset & 1) != 0;
    const int shape = preset / 2;
    const int period = doubled ? stepCount / 2 : stepCount;
    for (int step = 0; step < stepCount; ++step)
    {
        const int local = step % period;
        const float phase = static_cast<float>(local)
                          / static_cast<float>(period);
        float value = 0.0f;
        if (shape == 0)
            value = static_cast<float>(local) / static_cast<float>(period - 1);
        else if (shape == 1)
            value = 1.0f - static_cast<float>(local)
                          / static_cast<float>(period - 1);
        else if (shape == 2)
            value = 0.5f + 0.5f * std::sin(
                juce::MathConstants<float>::twoPi * phase);
        else if (shape == 3)
            value = 1.0f - std::abs(2.0f * phase - 1.0f);
        else if (shape == 4)
            value = phase < 0.25f ? 1.0f : 0.0f;
        else if (shape == 5)
            value = phase < 0.50f ? 1.0f : 0.0f;
        else
            value = phase < 0.75f ? 1.0f : 0.0f;
        result[static_cast<size_t>(step)] = bipolar
            ? juce::jlimit(0.0f, 1.0f, value)
            : 0.5f + 0.5f * juce::jlimit(0.0f, 1.0f, value);
    }
    return result;
}
}
