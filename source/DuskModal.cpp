#include "PluginProcessor.h"
#include <array>
#include <cmath>
#include <complex>

namespace
{
constexpr int modalModeCount = 8;
constexpr int firstModalDistortionStateBank = 11;
constexpr double modalOutputGain = 1.75;
constexpr std::array<double, modalModeCount> harmonicRatios
    { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0 };
constexpr std::array<double, modalModeCount> metallicRatios
    { 1.0, 1.414213562, 2.0, 2.756, 3.842, 5.404, 7.59, 10.67 };

struct ModalMode
{
    double coefficientOne = 0.0;
    double coefficientTwo = 0.0;
    double numeratorGain = 0.0;
    double weight = 0.0;
};

struct ModalSettings
{
    std::array<ModalMode, modalModeCount> modes {};
    double body = 0.0;
};

ModalSettings makeModalSettings(
    double sampleRate, float baseFrequencyHz, float decayMilliseconds,
    float bodyPercent, float modes, float ratioPercent,
    float inharmonicityPercent, float decayTiltPercent,
    float dampingPercent, float excitationPercent) noexcept
{
    ModalSettings settings;
    const double safeRate = juce::jmax(1.0, sampleRate);
    const double maximumFrequency = safeRate * 0.45;
    const double baseFrequency = juce::jlimit(
        20.0, juce::jmin(4000.0, maximumFrequency),
        static_cast<double>(baseFrequencyHz));
    const double baseDecaySeconds = juce::jlimit(
        0.020, 5.0, static_cast<double>(decayMilliseconds) * 0.001);
    const double modePosition = juce::jlimit(
        1.0, static_cast<double>(modalModeCount),
        static_cast<double>(modes));
    const double ratioMorph = juce::jlimit(
        0.0, 1.0, static_cast<double>(ratioPercent) * 0.01);
    const double inharmonicity = juce::jlimit(
        -1.0, 1.0, static_cast<double>(inharmonicityPercent) * 0.01);
    const double ratioExponent = 1.0 + 0.35 * inharmonicity;
    const double decayTilt = juce::jlimit(
        -1.0, 1.0, static_cast<double>(decayTiltPercent) * 0.01);
    const double damping = juce::jlimit(
        0.0, 1.0, static_cast<double>(dampingPercent) * 0.01);
    const double excitation = juce::jlimit(
        -1.0, 1.0, static_cast<double>(excitationPercent) * 0.01);
    double weightTotal = 0.0;

    for (int mode = 0; mode < modalModeCount; ++mode)
    {
        const auto index = static_cast<size_t>(mode);
        const double active = juce::jlimit(
            0.0, 1.0, modePosition - static_cast<double>(mode));
        const double ratio = harmonicRatios[index] * std::pow(
            metallicRatios[index] / harmonicRatios[index], ratioMorph);
        const double shapedRatio = std::pow(ratio, ratioExponent);
        const double modeFrequency = juce::jlimit(
            20.0, maximumFrequency, baseFrequency * shapedRatio);
        const double modeFraction = static_cast<double>(mode)
            / static_cast<double>(modalModeCount - 1);
        const double decaySeconds = juce::jlimit(
            0.008, 8.0,
            baseDecaySeconds * std::pow(4.0, decayTilt * modeFraction)
                * std::pow(0.06, damping * modeFraction));
        const double radius = std::exp(
            std::log(0.001) / (decaySeconds * safeRate));
        const double omega = juce::MathConstants<double>::twoPi
            * modeFrequency / safeRate;
        const double cosine = std::cos(omega);
        const double coefficientOne = 2.0 * radius * cosine;
        const double coefficientTwo = -radius * radius;
        const std::complex<double> z = std::polar(1.0, -omega);
        const auto denominator = std::complex<double>(1.0, 0.0)
            - coefficientOne * z - coefficientTwo * z * z;
        const auto numerator = std::complex<double>(1.0, 0.0) - z * z;
        const double centreNormalisation = std::abs(numerator) > 1.0e-9
            ? std::abs(denominator) / std::abs(numerator) : 0.0;
        // A unit-peak normalisation alone makes long modal decays almost
        // impossible to excite from transients. This bounded energy
        // compensation keeps their ring audible while the output protection
        // below prevents pathological sustained inputs from running away.
        const double excitationCompensation = juce::jlimit(
            1.0, 48.0,
            0.55 / std::sqrt(juce::jmax(1.0e-9, 1.0 - radius)));
        const double numeratorGain = centreNormalisation
            * excitationCompensation;
        const double naturalFalloff = 1.0 / std::sqrt(1.0 + mode);
        const double weight = active * naturalFalloff
            * std::pow(juce::jmax(1.0, shapedRatio), 0.75 * excitation);

        settings.modes[index] = {
            coefficientOne, coefficientTwo, numeratorGain, weight
        };
        weightTotal += weight;
    }

    const double normalisation = weightTotal > 1.0e-9
        ? modalOutputGain / weightTotal : 0.0;
    for (auto& mode : settings.modes)
        mode.weight *= normalisation;
    settings.body = juce::jlimit(
        0.0, 1.0, static_cast<double>(bodyPercent) * 0.01);
    return settings;
}
}

std::complex<double> DuskAudioProcessor::getModalResponse(
    double frequencyHz, double sampleRate, float baseFrequencyHz,
    float decayMilliseconds, float bodyPercent, float modes,
    float ratioPercent, float inharmonicityPercent,
    float decayTiltPercent, float dampingPercent,
    float excitationPercent) noexcept
{
    const auto settings = makeModalSettings(
        sampleRate, baseFrequencyHz, decayMilliseconds, bodyPercent, modes,
        ratioPercent, inharmonicityPercent, decayTiltPercent,
        dampingPercent, excitationPercent);
    const double safeRate = juce::jmax(1.0, sampleRate);
    const double safeFrequency = juce::jlimit(
        1.0, safeRate * 0.49, frequencyHz);
    const std::complex<double> z = std::polar(
        1.0, -juce::MathConstants<double>::twoPi
                 * safeFrequency / safeRate);
    std::complex<double> modalResponse(0.0, 0.0);
    for (const auto& mode : settings.modes)
    {
        const auto denominator = std::complex<double>(1.0, 0.0)
            - mode.coefficientOne * z - mode.coefficientTwo * z * z;
        if (std::abs(denominator) > 1.0e-12)
            modalResponse += mode.weight * mode.numeratorGain
                * (std::complex<double>(1.0, 0.0) - z * z)
                / denominator;
    }
    return (1.0 - settings.body) + settings.body * modalResponse;
}

float DuskAudioProcessor::processModalFilter(
    float sample, int engine, int channel, float baseFrequencyHz,
    float decayMilliseconds, float bodyPercent, float modes,
    float ratioPercent, float inharmonicityPercent,
    float decayTiltPercent, float dampingPercent,
    float excitationPercent,
    bool internalDistortion, float distortionAmount,
    int currentDistortion, int previousDistortion,
    float distortionCrossfade) noexcept
{
    const int safeEngine = juce::jlimit(0, filterEngineCount - 1, engine);
    const int safeChannel = juce::jlimit(0, channelCount - 1, channel);
    const auto stateIndex = static_cast<size_t>(
        safeEngine * channelCount + safeChannel);
    const auto settings = makeModalSettings(
        currentSampleRate, baseFrequencyHz, decayMilliseconds, bodyPercent,
        modes, ratioPercent, inharmonicityPercent, decayTiltPercent,
        dampingPercent, excitationPercent);
    const float delayedInput = modalInputTwo[stateIndex];
    float modalOutput = 0.0f;

    for (int mode = 0; mode < modalModeCount; ++mode)
    {
        const auto index = static_cast<size_t>(mode);
        const auto& coefficients = settings.modes[index];
        float output = static_cast<float>(
            coefficients.numeratorGain
                * static_cast<double>(sample - delayedInput)
            + coefficients.coefficientOne
                * static_cast<double>(modalOutputOne[index][stateIndex])
            + coefficients.coefficientTwo
                * static_cast<double>(modalOutputTwo[index][stateIndex]));
        output = sanitiseSample(output, 64.0f);
        modalOutputTwo[index][stateIndex] =
            modalOutputOne[index][stateIndex];
        modalOutputOne[index][stateIndex] = output;
        if (internalDistortion && std::abs(coefficients.weight) > 1.0e-9)
            output = processSelectedDistortion(
                output, distortionAmount, safeEngine, safeChannel,
                currentDistortion, previousDistortion,
                distortionCrossfade, firstModalDistortionStateBank + mode);
        modalOutput += static_cast<float>(coefficients.weight) * output;
    }

    modalInputTwo[stateIndex] = modalInputOne[stateIndex];
    modalInputOne[stateIndex] = sample;
    constexpr float protectionLevel = 4.0f;
    const float protectedModal = protectionLevel * std::tanh(
        modalOutput / protectionLevel);
    const float body = static_cast<float>(settings.body);
    return sanitiseSample(
        sample + (protectedModal - sample) * body, 64.0f);
}
