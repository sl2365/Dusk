#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace
{
namespace ParameterIds
{
constexpr auto routing = "routing";
constexpr auto filter1Architecture = "filter1Architecture";
constexpr auto filter2Architecture = "filter2Architecture";
constexpr auto filter1Type = "filter1Type";
constexpr auto filter2Type = "filter2Type";
constexpr auto filter1Cutoff = "filter1Cutoff";
constexpr auto filter2Cutoff = "filter2Cutoff";
constexpr auto filter1Resonance = "filter1Resonance";
constexpr auto filter2Resonance = "filter2Resonance";
constexpr auto filter1ResonanceShape = "filter1ResonanceShape";
constexpr auto filter2ResonanceShape = "filter2ResonanceShape";
constexpr auto filter1ResonanceLimit = "filter1ResonanceLimit";
constexpr auto filter2ResonanceLimit = "filter2ResonanceLimit";
constexpr auto filter1BassLoss = "filter1BassLoss";
constexpr auto filter2BassLoss = "filter2BassLoss";
constexpr auto filter1CoreSaturation = "filter1CoreSaturation";
constexpr auto filter2CoreSaturation = "filter2CoreSaturation";
constexpr auto filter1SaturationAsymmetry = "filter1SaturationAsymmetry";
constexpr auto filter2SaturationAsymmetry = "filter2SaturationAsymmetry";
constexpr auto filter1PoleSpread = "filter1PoleSpread";
constexpr auto filter2PoleSpread = "filter2PoleSpread";
constexpr auto filter1Feedback = "filter1Feedback";
constexpr auto filter2Feedback = "filter2Feedback";
constexpr auto filter1FeedbackLimit = "filter1FeedbackLimit";
constexpr auto filter2FeedbackLimit = "filter2FeedbackLimit";
constexpr auto filter1LadderStageSaturation =
    "filter1LadderStageSaturation";
constexpr auto filter2LadderStageSaturation =
    "filter2LadderStageSaturation";
constexpr auto filter1LadderFeedbackSaturation =
    "filter1LadderFeedbackSaturation";
constexpr auto filter2LadderFeedbackSaturation =
    "filter2LadderFeedbackSaturation";
constexpr auto filter1LadderResonanceCompensation =
    "filter1LadderResonanceCompensation";
constexpr auto filter2LadderResonanceCompensation =
    "filter2LadderResonanceCompensation";
constexpr auto filter1OtaCellDrive = "filter1OtaCellDrive";
constexpr auto filter2OtaCellDrive = "filter2OtaCellDrive";
constexpr auto filter1OtaFeedbackDamping = "filter1OtaFeedbackDamping";
constexpr auto filter2OtaFeedbackDamping = "filter2OtaFeedbackDamping";
constexpr auto filter1OtaResonanceCompensation =
    "filter1OtaResonanceCompensation";
constexpr auto filter2OtaResonanceCompensation =
    "filter2OtaResonanceCompensation";
constexpr auto filter1Slope = "filter1Slope";
constexpr auto filter2Slope = "filter2Slope";
constexpr auto filter1Enabled = "filter1Enabled";
constexpr auto filter2Enabled = "filter2Enabled";
constexpr auto filter1DistortionType = "filter1DistortionType";
constexpr auto filter2DistortionType = "filter2DistortionType";
constexpr auto filter1Drive = "filter1Drive";
constexpr auto filter2Drive = "filter2Drive";
constexpr auto filter1DistortionPlacement = "filter1DistortionPlacement";
constexpr auto filter2DistortionPlacement = "filter2DistortionPlacement";
constexpr auto mix = "mix";
constexpr auto output = "output";
constexpr auto bypass = "bypass";
}

constexpr float smoothingSeconds = 0.020f;
constexpr float nonlinearSmoothingSeconds = 0.050f;
constexpr float maximumDriveGain = 16.0f;
constexpr int externalDistortionStateBank = 0;
constexpr int cleanInternalDistortionStateBank = 1;
constexpr int ladderInternalDistortionStateBank = 2;
constexpr int otaInternalDistortionStateBank = 3;

constexpr std::array<int, 17> distortionAlgorithmForSortedChoice
{
    // Amp, Asym, Bumpy, Comb, Crackle, Fresh, Fuzz, Hard Clip,
    // Lightning, Pixelated, Razor, Rectify, Samurai, Saturation,
    // Soft Clip, Stressed, Tube.
    6, 8, 7, 11, 4, 14, 16, 15, 5,
    10, 9, 3, 13, 1, 0, 12, 2
};

template <typename Float>
Float shapedResonance(Float resonance, Float shape) noexcept
{
    const auto safeResonance = juce::jlimit(
        static_cast<Float>(0), static_cast<Float>(1), resonance);
    const auto safeShape = juce::jlimit(
        static_cast<Float>(-1), static_cast<Float>(1), shape);
    const auto exponent = std::pow(static_cast<Float>(8), -safeShape);
    return std::pow(safeResonance, exponent);
}

template <typename Float>
Float upperRangeCharacter(Float amount) noexcept
{
    const auto safeAmount = juce::jlimit(
        static_cast<Float>(0), static_cast<Float>(1), amount);
    return std::pow(safeAmount, static_cast<Float>(1.5));
}

float onePoleCoefficient(float cutoffHz, double sampleRate) noexcept
{
    const auto safeRate = juce::jmax(1.0, sampleRate);
    const auto safeCutoff = juce::jlimit(
        1.0f, static_cast<float>(safeRate * 0.45), cutoffHz);
    return std::exp(-juce::MathConstants<float>::twoPi * safeCutoff
                    / static_cast<float>(safeRate));
}
}

int DuskAudioProcessor::distortionAlgorithmForChoice(int choice) noexcept
{
    const auto index = static_cast<size_t>(juce::jlimit(
        0, static_cast<int>(distortionAlgorithmForSortedChoice.size()) - 1,
        choice));
    return distortionAlgorithmForSortedChoice[index];
}

DuskAudioProcessor::DuskAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "DuskParameters", createParameterLayout())
{
    bindParameterPointers();
    for (auto& value : sequencerAnimatedNormalised)
        value.store(-1.0f, std::memory_order_relaxed);
    parameters.state.setProperty("schemaVersion", stateSchemaVersion, nullptr);
    setSavedParameterState(parameters.copyState());
    getSettingsDirectory().createDirectory();
    getPresetsDirectory().createDirectory();
    getSequencesDirectory().createDirectory();
    getModelsDirectory().createDirectory();
}

juce::File DuskAudioProcessor::getDataDirectory()
{
#if JUCE_WINDOWS
    std::array<wchar_t, 32768> modulePath {};
    const auto moduleHandle = static_cast<HMODULE>(
        juce::Process::getCurrentModuleInstanceHandle());
    if (moduleHandle != nullptr)
    {
        const auto pathLength = ::GetModuleFileNameW(
            moduleHandle, modulePath.data(),
            static_cast<DWORD>(modulePath.size()));
        if (pathLength > 0 && pathLength < modulePath.size() - 1)
        {
            const juce::File moduleFile(juce::String(modulePath.data()));
            auto folder = moduleFile.getParentDirectory();
            for (int depth = 0; depth < 6; ++depth)
            {
                if (folder.hasFileExtension(".vst3"))
                    return folder.getParentDirectory().getChildFile("Dusk");
                const auto parent = folder.getParentDirectory();
                if (parent == folder)
                    break;
                folder = parent;
            }
            return moduleFile.getParentDirectory().getChildFile("Dusk");
        }
    }
#endif
    return juce::File::getSpecialLocation(juce::File::currentApplicationFile)
        .getParentDirectory().getChildFile("Dusk");
}

juce::File DuskAudioProcessor::getSettingsDirectory()
{
    return getDataDirectory().getChildFile("Settings");
}

juce::File DuskAudioProcessor::getPresetsDirectory()
{
    return getDataDirectory().getChildFile("Presets");
}

juce::File DuskAudioProcessor::getSequencesDirectory()
{
    return getDataDirectory().getChildFile("Sequences");
}

juce::File DuskAudioProcessor::getModelsDirectory()
{
    return getDataDirectory().getChildFile("Models");
}

juce::Result DuskAudioProcessor::savePresetToFile(
    const juce::File& presetFile)
{
    if (presetFile == juce::File())
        return juce::Result::fail("No preset file was selected.");

    const auto parent = presetFile.getParentDirectory();
    const auto root = getPresetsDirectory();
    if (parent != root && ! parent.isAChildOf(root))
        return juce::Result::fail(
            "Presets must be saved inside the Dusk Presets folder.");
    if (! parent.isDirectory() && parent.createDirectory().failed())
        return juce::Result::fail("The preset folder could not be created.");

    auto state = parameters.copyState();
    state.setProperty("schemaVersion", stateSchemaVersion, nullptr);
    state.removeProperty("currentPresetPath", nullptr);
    const auto xml = state.createXml();
    if (xml == nullptr)
        return juce::Result::fail("The plugin state could not be encoded.");

    const juce::TemporaryFile temporaryFile(presetFile);
    if (! temporaryFile.getFile().replaceWithText(xml->toString())
        || ! temporaryFile.overwriteTargetFileWithTemporary())
        return juce::Result::fail("The preset could not be written safely.");

    parameters.state.setProperty(
        "currentPresetPath", presetFile.getFullPathName(), nullptr);
    setSavedParameterState(state);
    return juce::Result::ok();
}

juce::Result DuskAudioProcessor::loadPresetFromFile(
    const juce::File& presetFile)
{
    if (! presetFile.existsAsFile())
        return juce::Result::fail("The preset file no longer exists.");
    const auto parent = presetFile.getParentDirectory();
    const auto root = getPresetsDirectory();
    if (parent != root && ! parent.isAChildOf(root))
        return juce::Result::fail(
            "Presets must be loaded from the Dusk Presets folder.");

    const auto xml = juce::XmlDocument::parse(presetFile);
    if (xml == nullptr || ! xml->hasTagName("DuskParameters"))
        return juce::Result::fail("This is not a Dusk preset.");
    auto restored = juce::ValueTree::fromXml(*xml);
    if (static_cast<int>(restored.getProperty("schemaVersion", 0))
        != stateSchemaVersion)
        return juce::Result::fail(
            "This preset does not use the current Dusk schema.");

    restored.removeProperty("currentPresetPath", nullptr);
    setSavedParameterState(restored);
    parameters.replaceState(restored);
    parameters.state.setProperty(
        "currentPresetPath", presetFile.getFullPathName(), nullptr);
    pendingProcessingReset.store(true, std::memory_order_release);
    return juce::Result::ok();
}

juce::File DuskAudioProcessor::getCurrentPresetFile() const
{
    const auto path = parameters.state.getProperty(
        "currentPresetPath", juce::String()).toString();
    return path.isEmpty() ? juce::File() : juce::File(path);
}

void DuskAudioProcessor::setSavedParameterState(const juce::ValueTree& state)
{
    const juce::ScopedLock lock(savedParameterStateLock);
    savedParameterState = state.createCopy();
    savedParameterState.removeProperty("currentPresetPath", nullptr);
}

std::optional<float> DuskAudioProcessor::getSavedParameterValue(
    const juce::String& parameterId) const
{
    const juce::ScopedLock lock(savedParameterStateLock);
    for (int index = 0; index < savedParameterState.getNumChildren(); ++index)
    {
        const auto child = savedParameterState.getChild(index);
        if (child.getProperty("id").toString() != parameterId)
            continue;
        const float value = child.getProperty("value").toString()
            .getFloatValue();
        return std::isfinite(value) ? std::optional<float>(value)
                                    : std::nullopt;
    }
    return std::nullopt;
}

juce::AudioProcessorValueTreeState::ParameterLayout
DuskAudioProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;
    using API = juce::AudioParameterInt;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;

    const auto percentageAttributes = juce::AudioParameterFloatAttributes()
        .withLabel("%");
    const auto targetDepthAttributes = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float value, int)
        {
            return juce::String(juce::roundToInt(value * 100.0f)) + "%";
        })
        .withValueFromStringFunction([](const juce::String& text)
        {
            return text.getFloatValue() * 0.01f;
        });
    auto outputRange = juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f);
    outputRange.setSkewForCentre(0.0f);
    auto cutoffRange = juce::NormalisableRange<float>(
        20.0f, 20000.0f, 0.1f);
    cutoffRange.setSkewForCentre(1000.0f);
    const auto cutoffAttributes = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float value, int)
        {
            if (value >= 1000.0f)
                return juce::String(value * 0.001f, value >= 10000.0f ? 1 : 2)
                    + " kHz";
            return juce::String(juce::roundToInt(value)) + " Hz";
        })
        .withValueFromStringFunction([](const juce::String& text)
        {
            const auto trimmed = text.trim();
            const float value = trimmed.getFloatValue();
            return trimmed.containsIgnoreCase("k") ? value * 1000.0f
                                                    : value;
        });
    const auto slopeAttributes = juce::AudioParameterFloatAttributes()
        .withLabel("dB/oct")
        .withStringFromValueFunction([](float value, int)
        {
            return juce::String(value, 1) + " dB";
        })
        .withValueFromStringFunction([](const juce::String& text)
        {
            return text.getFloatValue();
        });

    juce::StringArray architectureNames;
    for (const auto& descriptor : dusk::architecture::descriptors)
        architectureNames.add(descriptor.displayName);

    // Architecture is independent per filter. Clean SVF, Ladder and OTA are
    // separate engines rather than relabelled Models.
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID(ParameterIds::routing, 1), "Filter Routing",
        juce::StringArray { "Filter 1", "Filter 2", "Serial 1 to 2",
                            "Parallel" }, 2));
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID(ParameterIds::filter1Architecture, 1),
        "Filter 1 Architecture", architectureNames, 0));
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID(ParameterIds::filter1Type, 1),
        "Filter 1 Type",
        juce::StringArray { "LPF", "BPF", "HPF", "NOTCH", "PEAK" }, 0));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter1Cutoff, 1),
        "Filter 1 Cutoff", cutoffRange, 20000.0f, cutoffAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter1Resonance, 1),
        "Filter 1 Resonance",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter1ResonanceShape, 1),
        "Filter 1 Resonance Shape",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter1ResonanceLimit, 1),
        "Filter 1 Resonance Limit",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter1BassLoss, 1),
        "Filter 1 Bass Loss",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter1CoreSaturation, 1),
        "Filter 1 Core Saturation",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter1SaturationAsymmetry, 1),
        "Filter 1 Saturation Asymmetry",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter1PoleSpread, 1),
        "Filter 1 Pole Spread",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter1Feedback, 1),
        "Filter 1 Feedback",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter1FeedbackLimit, 1),
        "Filter 1 Feedback Limit",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter1LadderStageSaturation, 1),
        "Filter 1 Ladder Stage Saturation",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 35.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter1LadderFeedbackSaturation, 1),
        "Filter 1 Ladder Feedback Saturation",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(
            ParameterIds::filter1LadderResonanceCompensation, 1),
        "Filter 1 Ladder Resonance Compensation",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter1OtaCellDrive, 1),
        "Filter 1 OTA Cell Drive",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 35.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter1OtaFeedbackDamping, 1),
        "Filter 1 OTA Feedback Damping",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 35.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(
            ParameterIds::filter1OtaResonanceCompensation, 1),
        "Filter 1 OTA Resonance Compensation",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter1Slope, 1),
        "Filter 1 Slope",
        juce::NormalisableRange<float>(6.0f, 48.0f, 0.1f), 12.0f,
        slopeAttributes));
    layout.push_back(std::make_unique<APB>(
        juce::ParameterID(ParameterIds::filter1Enabled, 1),
        "Filter 1 Power", true));
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID(ParameterIds::filter2Architecture, 1),
        "Filter 2 Architecture", architectureNames, 0));
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID(ParameterIds::filter2Type, 1),
        "Filter 2 Type",
        juce::StringArray { "LPF", "BPF", "HPF", "NOTCH", "PEAK" }, 0));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter2Cutoff, 1),
        "Filter 2 Cutoff", cutoffRange, 20000.0f, cutoffAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter2Resonance, 1),
        "Filter 2 Resonance",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter2ResonanceShape, 1),
        "Filter 2 Resonance Shape",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter2ResonanceLimit, 1),
        "Filter 2 Resonance Limit",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter2BassLoss, 1),
        "Filter 2 Bass Loss",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter2CoreSaturation, 1),
        "Filter 2 Core Saturation",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter2SaturationAsymmetry, 1),
        "Filter 2 Saturation Asymmetry",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter2PoleSpread, 1),
        "Filter 2 Pole Spread",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter2Feedback, 1),
        "Filter 2 Feedback",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter2FeedbackLimit, 1),
        "Filter 2 Feedback Limit",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter2LadderStageSaturation, 1),
        "Filter 2 Ladder Stage Saturation",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 35.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter2LadderFeedbackSaturation, 1),
        "Filter 2 Ladder Feedback Saturation",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(
            ParameterIds::filter2LadderResonanceCompensation, 1),
        "Filter 2 Ladder Resonance Compensation",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter2OtaCellDrive, 1),
        "Filter 2 OTA Cell Drive",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 35.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter2OtaFeedbackDamping, 1),
        "Filter 2 OTA Feedback Damping",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 35.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(
            ParameterIds::filter2OtaResonanceCompensation, 1),
        "Filter 2 OTA Resonance Compensation",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter2Slope, 1),
        "Filter 2 Slope",
        juce::NormalisableRange<float>(6.0f, 48.0f, 0.1f), 12.0f,
        slopeAttributes));
    layout.push_back(std::make_unique<APB>(
        juce::ParameterID(ParameterIds::filter2Enabled, 1),
        "Filter 2 Power", true));
    juce::StringArray distortionTypes {
        "Soft Clip", "Saturation", "Tube", "Rectify", "Crackle",
        "Lightning", "Amp", "Bumpy", "Asym", "Razor", "Pixelated",
        "Comb", "Stressed", "Samurai", "Fresh", "Hard Clip", "Fuzz"
    };
    distortionTypes.sortNatural();
    const int defaultDistortionType = distortionTypes.indexOf("Soft Clip");
    const juce::StringArray distortionPlacements {
        "Pre", "Internal", "Post"
    };
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID(ParameterIds::filter1DistortionType, 1),
        "Filter 1 Distortion Type", distortionTypes,
        defaultDistortionType));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter1Drive, 1),
        "Filter 1 Drive",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID(ParameterIds::filter1DistortionPlacement, 1),
        "Filter 1 Distortion Placement", distortionPlacements, 0));
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID(ParameterIds::filter2DistortionType, 1),
        "Filter 2 Distortion Type", distortionTypes,
        defaultDistortionType));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::filter2Drive, 1),
        "Filter 2 Drive",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID(ParameterIds::filter2DistortionPlacement, 1),
        "Filter 2 Distortion Placement", distortionPlacements, 0));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::mix, 1), "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 35.0f,
        percentageAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID(ParameterIds::output, 1), "Output", outputRange,
        0.0f, juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.push_back(std::make_unique<APB>(
        juce::ParameterID(ParameterIds::bypass, 1), "Bypass", false));

    layout.push_back(std::make_unique<APB>(
        juce::ParameterID("sequencerEnabled", 1), "Sequencer Enabled", true));
    layout.push_back(std::make_unique<APB>(
        juce::ParameterID("sequencerHostSync", 1), "Sequencer Host Sync", true));
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID("sequencerRate", 1), "Sequencer Rate",
        juce::StringArray { "1/128", "1/64T", "1/64", "1/32T", "1/32",
            "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2T",
            "1/2", "1/1" }, 6));
    layout.push_back(std::make_unique<API>(
        juce::ParameterID("sequencerStart", 1), "Sequencer Start", 1,
        dusk::sequencer::stepCount - 1, 1));
    layout.push_back(std::make_unique<API>(
        juce::ParameterID("sequencerEnd", 1), "Sequencer End", 2,
        dusk::sequencer::stepCount, dusk::sequencer::stepCount));
    layout.push_back(std::make_unique<APB>(
        juce::ParameterID("sequencerRangeLink", 1),
        "Sequencer Range Link", false));
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID("sequencerDirection", 1), "Sequencer Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played", "Random" },
        0));
    layout.push_back(std::make_unique<APB>(
        juce::ParameterID("sequencerBipolar", 1), "Sequencer Bipolar", false));
    layout.push_back(std::make_unique<APB>(
        juce::ParameterID("sequencerUnipolarNegative", 1),
        "Sequencer Unipolar Negative", false));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerAttack", 1), "Sequencer Attack",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f,
        targetDepthAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerRelease", 1), "Sequencer Release",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f,
        targetDepthAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerPeak", 1), "Sequencer Depth",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerMove", 1), "Sequencer Offset",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerHue", 1), "Sequencer Hue",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerSaturation", 1), "Sequencer Saturation",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerLuminance", 1), "Sequencer Luminance",
        juce::NormalisableRange<float>(0.2f, 1.0f, 0.001f), 0.5f));

    for (int step = 0; step < dusk::sequencer::stepCount; ++step)
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(dusk::sequencer::stepParameterId(step), 1),
            "Sequencer Step " + juce::String(step + 1),
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 1.0f));

    juce::StringArray targetNames { "None" };
    for (const auto& target : dusk::sequencer::targets)
        targetNames.add(target.displayName);
    for (int slot = 0; slot < dusk::sequencer::targetSlotCount; ++slot)
    {
        layout.push_back(std::make_unique<APC>(
            juce::ParameterID(dusk::sequencer::targetParameterId(slot), 1),
            "Sequencer Target " + juce::String(slot + 1), targetNames, 0));
        layout.push_back(std::make_unique<APB>(
            juce::ParameterID(
                dusk::sequencer::targetEnabledParameterId(slot), 1),
            "Sequencer Target " + juce::String(slot + 1) + " Enabled", true));
        layout.push_back(std::make_unique<APF>(
            juce::ParameterID(
                dusk::sequencer::targetDepthParameterId(slot), 1),
            "Sequencer Target " + juce::String(slot + 1) + " Depth",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f,
            targetDepthAttributes));
    }

    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerGateShortLength", 1),
        "Sequencer Gate Short Length",
        juce::NormalisableRange<float>(0.10f, 0.60f, 0.01f), 0.50f,
        targetDepthAttributes));
    layout.push_back(std::make_unique<APF>(
        juce::ParameterID("sequencerGateLongLength", 1),
        "Sequencer Gate Long Length",
        juce::NormalisableRange<float>(0.65f, 0.95f, 0.01f), 0.90f,
        targetDepthAttributes));
    for (int step = 0; step < dusk::sequencer::stepCount; ++step)
        layout.push_back(std::make_unique<APC>(
            juce::ParameterID(dusk::sequencer::gateStepParameterId(step), 1),
            "Sequencer Gate Step " + juce::String(step + 1),
            juce::StringArray { "Off", "Short", "Long", "Rest" },
            static_cast<int>(dusk::sequencer::GateStepMode::rest)));
    layout.push_back(std::make_unique<APC>(
        juce::ParameterID("sequencerGateMode", 1), "Sequencer Gate Mode",
        juce::StringArray { "Input", "Effect", "Output", "Off" }, 0));

    return { layout.begin(), layout.end() };
}

void DuskAudioProcessor::bindParameterPointers()
{
    const auto bind = [this](const juce::String& id)
    {
        auto* value = parameters.getRawParameterValue(id);
        jassert(value != nullptr);
        return value;
    };
    raw.routing = bind(ParameterIds::routing);
    raw.filter1Architecture = bind(ParameterIds::filter1Architecture);
    raw.filter2Architecture = bind(ParameterIds::filter2Architecture);
    raw.filter1Type = bind(ParameterIds::filter1Type);
    raw.filter2Type = bind(ParameterIds::filter2Type);
    raw.filter1Cutoff = bind(ParameterIds::filter1Cutoff);
    raw.filter2Cutoff = bind(ParameterIds::filter2Cutoff);
    raw.filter1Resonance = bind(ParameterIds::filter1Resonance);
    raw.filter2Resonance = bind(ParameterIds::filter2Resonance);
    raw.filter1ResonanceShape = bind(ParameterIds::filter1ResonanceShape);
    raw.filter2ResonanceShape = bind(ParameterIds::filter2ResonanceShape);
    raw.filter1ResonanceLimit = bind(ParameterIds::filter1ResonanceLimit);
    raw.filter2ResonanceLimit = bind(ParameterIds::filter2ResonanceLimit);
    raw.filter1BassLoss = bind(ParameterIds::filter1BassLoss);
    raw.filter2BassLoss = bind(ParameterIds::filter2BassLoss);
    raw.filter1CoreSaturation = bind(ParameterIds::filter1CoreSaturation);
    raw.filter2CoreSaturation = bind(ParameterIds::filter2CoreSaturation);
    raw.filter1SaturationAsymmetry = bind(
        ParameterIds::filter1SaturationAsymmetry);
    raw.filter2SaturationAsymmetry = bind(
        ParameterIds::filter2SaturationAsymmetry);
    raw.filter1PoleSpread = bind(ParameterIds::filter1PoleSpread);
    raw.filter2PoleSpread = bind(ParameterIds::filter2PoleSpread);
    raw.filter1Feedback = bind(ParameterIds::filter1Feedback);
    raw.filter2Feedback = bind(ParameterIds::filter2Feedback);
    raw.filter1FeedbackLimit = bind(ParameterIds::filter1FeedbackLimit);
    raw.filter2FeedbackLimit = bind(ParameterIds::filter2FeedbackLimit);
    raw.filter1LadderStageSaturation = bind(
        ParameterIds::filter1LadderStageSaturation);
    raw.filter2LadderStageSaturation = bind(
        ParameterIds::filter2LadderStageSaturation);
    raw.filter1LadderFeedbackSaturation = bind(
        ParameterIds::filter1LadderFeedbackSaturation);
    raw.filter2LadderFeedbackSaturation = bind(
        ParameterIds::filter2LadderFeedbackSaturation);
    raw.filter1LadderResonanceCompensation = bind(
        ParameterIds::filter1LadderResonanceCompensation);
    raw.filter2LadderResonanceCompensation = bind(
        ParameterIds::filter2LadderResonanceCompensation);
    raw.filter1OtaCellDrive = bind(ParameterIds::filter1OtaCellDrive);
    raw.filter2OtaCellDrive = bind(ParameterIds::filter2OtaCellDrive);
    raw.filter1OtaFeedbackDamping = bind(
        ParameterIds::filter1OtaFeedbackDamping);
    raw.filter2OtaFeedbackDamping = bind(
        ParameterIds::filter2OtaFeedbackDamping);
    raw.filter1OtaResonanceCompensation = bind(
        ParameterIds::filter1OtaResonanceCompensation);
    raw.filter2OtaResonanceCompensation = bind(
        ParameterIds::filter2OtaResonanceCompensation);
    raw.filter1Slope = bind(ParameterIds::filter1Slope);
    raw.filter2Slope = bind(ParameterIds::filter2Slope);
    raw.filter1Enabled = bind(ParameterIds::filter1Enabled);
    raw.filter2Enabled = bind(ParameterIds::filter2Enabled);
    raw.filter1DistortionType = bind(ParameterIds::filter1DistortionType);
    raw.filter2DistortionType = bind(ParameterIds::filter2DistortionType);
    raw.filter1Drive = bind(ParameterIds::filter1Drive);
    raw.filter2Drive = bind(ParameterIds::filter2Drive);
    raw.filter1DistortionPlacement = bind(
        ParameterIds::filter1DistortionPlacement);
    raw.filter2DistortionPlacement = bind(
        ParameterIds::filter2DistortionPlacement);
    raw.mix = bind(ParameterIds::mix);
    raw.output = bind(ParameterIds::output);
    raw.bypass = bind(ParameterIds::bypass);
    raw.sequencerEnabled = bind("sequencerEnabled");
    raw.sequencerHostSync = bind("sequencerHostSync");
    raw.sequencerRate = bind("sequencerRate");
    raw.sequencerStart = bind("sequencerStart");
    raw.sequencerEnd = bind("sequencerEnd");
    raw.sequencerDirection = bind("sequencerDirection");
    raw.sequencerBipolar = bind("sequencerBipolar");
    raw.sequencerUnipolarNegative = bind("sequencerUnipolarNegative");
    raw.sequencerAttack = bind("sequencerAttack");
    raw.sequencerRelease = bind("sequencerRelease");
    raw.sequencerPeak = bind("sequencerPeak");
    raw.sequencerMove = bind("sequencerMove");
    raw.sequencerGateShortLength = bind("sequencerGateShortLength");
    raw.sequencerGateLongLength = bind("sequencerGateLongLength");
    raw.sequencerGateMode = bind("sequencerGateMode");
    for (int step = 0; step < dusk::sequencer::stepCount; ++step)
    {
        const auto index = static_cast<size_t>(step);
        raw.sequencerSteps[index] = bind(
            dusk::sequencer::stepParameterId(step));
        raw.sequencerGateSteps[index] = bind(
            dusk::sequencer::gateStepParameterId(step));
    }
    for (int slot = 0; slot < dusk::sequencer::targetSlotCount; ++slot)
    {
        const auto index = static_cast<size_t>(slot);
        raw.sequencerTargets[index] = bind(
            dusk::sequencer::targetParameterId(slot));
        raw.sequencerTargetEnabled[index] = bind(
            dusk::sequencer::targetEnabledParameterId(slot));
        raw.sequencerTargetDepth[index] = bind(
            dusk::sequencer::targetDepthParameterId(slot));
    }
    for (size_t target = 0; target < dusk::sequencer::targets.size(); ++target)
        sequencerTargetParameters[target] = parameters.getParameter(
            dusk::sequencer::targets[target].parameterId);
}

float DuskAudioProcessor::load(const std::atomic<float>* value) noexcept
{
    const float loaded = value != nullptr
        ? value->load(std::memory_order_relaxed) : 0.0f;
    return std::isfinite(loaded) ? loaded : 0.0f;
}

bool DuskAudioProcessor::isSequencerTargetAssigned(int targetChoice) const noexcept
{
    if (targetChoice <= 0)
        return false;
    for (size_t slot = 0; slot < raw.sequencerTargets.size(); ++slot)
        if (load(raw.sequencerTargetEnabled[slot]) >= 0.5f
            && static_cast<int>(std::lround(load(raw.sequencerTargets[slot])))
                   == targetChoice)
            return true;
    return false;
}

float DuskAudioProcessor::sequencerTargetDepth(int targetChoice) const noexcept
{
    if (targetChoice <= 0)
        return 1.0f;
    for (size_t slot = 0; slot < raw.sequencerTargets.size(); ++slot)
        if (load(raw.sequencerTargetEnabled[slot]) >= 0.5f
            && static_cast<int>(std::lround(load(raw.sequencerTargets[slot])))
                   == targetChoice)
            return juce::jlimit(
                0.0f, 1.0f, load(raw.sequencerTargetDepth[slot]));
    return 1.0f;
}

float DuskAudioProcessor::sequencerModulatedValue(
    int targetChoice, float baseValue, float canonicalSequenceValue,
    bool bipolar, bool unipolarNegative,
    juce::RangedAudioParameter* valueParameter) const noexcept
{
    if (! isSequencerTargetAssigned(targetChoice)
        || targetChoice > static_cast<int>(sequencerTargetParameters.size()))
        return baseValue;
    auto* parameter = valueParameter != nullptr
        ? valueParameter
        : sequencerTargetParameters[static_cast<size_t>(targetChoice - 1)];
    if (parameter == nullptr)
        return baseValue;

    canonicalSequenceValue = dusk::sequencer::applyTargetDepth(
        canonicalSequenceValue, bipolar, sequencerTargetDepth(targetChoice));
    const float baseNormalised = juce::jlimit(
        0.0f, 1.0f, parameter->convertTo0to1(baseValue));
    float modulatedNormalised = baseNormalised;
    if (bipolar)
    {
        const float deviation = 2.0f * canonicalSequenceValue - 1.0f;
        const float radius = juce::jmin(
            baseNormalised, 1.0f - baseNormalised);
        modulatedNormalised = baseNormalised + deviation * radius;
    }
    else
    {
        const float amount = juce::jlimit(
            0.0f, 1.0f, 2.0f * canonicalSequenceValue - 1.0f);
        modulatedNormalised = unipolarNegative
            ? baseNormalised - amount * baseNormalised
            : baseNormalised + amount * (1.0f - baseNormalised);
    }
    modulatedNormalised = juce::jlimit(
        0.0f, 1.0f, modulatedNormalised);
    sequencerAnimatedNormalised[static_cast<size_t>(targetChoice - 1)].store(
        modulatedNormalised, std::memory_order_relaxed);
    return parameter->convertFrom0to1(modulatedNormalised);
}

std::optional<float> DuskAudioProcessor::getSequencerAnimatedNormalised(
    const juce::String& parameterId) const noexcept
{
    const int choice = dusk::sequencer::targetChoiceForParameter(parameterId);
    if (choice <= 0
        || choice > static_cast<int>(sequencerAnimatedNormalised.size())
        || load(raw.sequencerEnabled) < 0.5f
        || ! isSequencerTargetAssigned(choice))
        return std::nullopt;
    const float value = sequencerAnimatedNormalised[
        static_cast<size_t>(choice - 1)].load(std::memory_order_relaxed);
    if (! std::isfinite(value) || value < 0.0f)
        return std::nullopt;
    return juce::jlimit(0.0f, 1.0f, value);
}

bool DuskAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output
        && (output == juce::AudioChannelSet::mono()
            || output == juce::AudioChannelSet::stereo());
}

void DuskAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = juce::jmax(1.0, sampleRate);
    coreAsymmetryDcCoefficient = onePoleCoefficient(
        5.0f, currentSampleRate);
    rectifyDcCoefficient = onePoleCoefficient(18.0f, currentSampleRate);
    crackleDcCoefficient = onePoleCoefficient(22.0f, currentSampleRate);
    lightningDcCoefficient = onePoleCoefficient(28.0f, currentSampleRate);
    ampDcCoefficient = onePoleCoefficient(24.0f, currentSampleRate);
    ampToneCoefficient = onePoleCoefficient(9000.0f, currentSampleRate);
    bumpyDcCoefficient = onePoleCoefficient(22.0f, currentSampleRate);
    bumpyToneCoefficient = onePoleCoefficient(1450.0f, currentSampleRate);
    asymmetricalDcCoefficient = onePoleCoefficient(22.0f, currentSampleRate);
    razorToneCoefficient = onePoleCoefficient(2100.0f, currentSampleRate);
    razorDcCoefficient = onePoleCoefficient(26.0f, currentSampleRate);
    distortionCombDampingCoefficient = onePoleCoefficient(
        7600.0f, currentSampleRate);
    stressedDcCoefficient = onePoleCoefficient(26.0f, currentSampleRate);
    samuraiToneCoefficient = onePoleCoefficient(2700.0f, currentSampleRate);
    samuraiDcCoefficient = onePoleCoefficient(28.0f, currentSampleRate);
    freshToneCoefficient = onePoleCoefficient(3400.0f, currentSampleRate);
    fuzzDcCoefficient = onePoleCoefficient(18.0f, currentSampleRate);
    fuzzToneCoefficient = onePoleCoefficient(5200.0f, currentSampleRate);
    resetProcessingState();

    const auto resetSmoother = [this](LinearSmoother& smoother, float value)
    {
        smoother.reset(currentSampleRate, smoothingSeconds);
        smoother.setCurrentAndTargetValue(value);
    };
    const std::array<const std::atomic<float>*, filterEngineCount> drives {
        raw.filter1Drive, raw.filter2Drive
    };
    const std::array<const std::atomic<float>*, filterEngineCount>
        filterTypes { raw.filter1Type, raw.filter2Type };
    const std::array<const std::atomic<float>*, filterEngineCount> cutoffs {
        raw.filter1Cutoff, raw.filter2Cutoff
    };
    const std::array<const std::atomic<float>*, filterEngineCount> resonances {
        raw.filter1Resonance, raw.filter2Resonance
    };
    const std::array<const std::atomic<float>*, filterEngineCount>
        resonanceShapes {
            raw.filter1ResonanceShape, raw.filter2ResonanceShape
        };
    const std::array<const std::atomic<float>*, filterEngineCount>
        resonanceLimits {
            raw.filter1ResonanceLimit, raw.filter2ResonanceLimit
        };
    const std::array<const std::atomic<float>*, filterEngineCount> bassLosses {
        raw.filter1BassLoss, raw.filter2BassLoss
    };
    const std::array<const std::atomic<float>*, filterEngineCount>
        coreSaturations {
            raw.filter1CoreSaturation, raw.filter2CoreSaturation
        };
    const std::array<const std::atomic<float>*, filterEngineCount>
        saturationAsymmetries {
            raw.filter1SaturationAsymmetry,
            raw.filter2SaturationAsymmetry
        };
    const std::array<const std::atomic<float>*, filterEngineCount>
        poleSpreads {
            raw.filter1PoleSpread, raw.filter2PoleSpread
        };
    const std::array<const std::atomic<float>*, filterEngineCount>
        feedbacks {
            raw.filter1Feedback, raw.filter2Feedback
        };
    const std::array<const std::atomic<float>*, filterEngineCount>
        feedbackLimits {
            raw.filter1FeedbackLimit, raw.filter2FeedbackLimit
        };
    const std::array<const std::atomic<float>*, filterEngineCount>
        ladderStageSaturations {
            raw.filter1LadderStageSaturation,
            raw.filter2LadderStageSaturation
        };
    const std::array<const std::atomic<float>*, filterEngineCount>
        ladderFeedbackSaturations {
            raw.filter1LadderFeedbackSaturation,
            raw.filter2LadderFeedbackSaturation
        };
    const std::array<const std::atomic<float>*, filterEngineCount>
        ladderResonanceCompensations {
            raw.filter1LadderResonanceCompensation,
            raw.filter2LadderResonanceCompensation
        };
    const std::array<const std::atomic<float>*, filterEngineCount>
        otaCellDrives {
            raw.filter1OtaCellDrive, raw.filter2OtaCellDrive
        };
    const std::array<const std::atomic<float>*, filterEngineCount>
        otaFeedbackDampings {
            raw.filter1OtaFeedbackDamping,
            raw.filter2OtaFeedbackDamping
        };
    const std::array<const std::atomic<float>*, filterEngineCount>
        otaResonanceCompensations {
            raw.filter1OtaResonanceCompensation,
            raw.filter2OtaResonanceCompensation
        };
    const std::array<const std::atomic<float>*, filterEngineCount>
        architectures {
            raw.filter1Architecture, raw.filter2Architecture
        };
    const std::array<const std::atomic<float>*, filterEngineCount> slopes {
        raw.filter1Slope, raw.filter2Slope
    };
    const std::array<const std::atomic<float>*, filterEngineCount> enabled {
        raw.filter1Enabled, raw.filter2Enabled
    };
    const std::array<const std::atomic<float>*, filterEngineCount> types {
        raw.filter1DistortionType, raw.filter2DistortionType
    };
    for (int engine = 0; engine < filterEngineCount; ++engine)
    {
        const auto index = static_cast<size_t>(engine);
        resetSmoother(driveValue[index], juce::jlimit(
            0.0f, 1.0f, load(drives[index]) * 0.01f));
        resetSmoother(filterTypeValue[index], juce::jlimit(
            0.0f, 4.0f, load(filterTypes[index])));
        resetSmoother(cutoffValue[index], juce::jlimit(
            20.0f, 20000.0f, load(cutoffs[index])));
        resetSmoother(resonanceValue[index], juce::jlimit(
            0.0f, 1.0f, load(resonances[index]) * 0.01f));
        resetSmoother(resonanceShapeValue[index], juce::jlimit(
            -1.0f, 1.0f, load(resonanceShapes[index]) * 0.01f));
        resetSmoother(resonanceLimitValue[index], juce::jlimit(
            0.0f, 1.0f, load(resonanceLimits[index]) * 0.01f));
        resetSmoother(bassLossValue[index], juce::jlimit(
            0.0f, 1.0f, load(bassLosses[index]) * 0.01f));
        coreSaturationValue[index].reset(
            currentSampleRate, nonlinearSmoothingSeconds);
        coreSaturationValue[index].setCurrentAndTargetValue(juce::jlimit(
            0.0f, 1.0f, load(coreSaturations[index]) * 0.01f));
        saturationAsymmetryValue[index].reset(
            currentSampleRate, nonlinearSmoothingSeconds);
        saturationAsymmetryValue[index].setCurrentAndTargetValue(
            juce::jlimit(-1.0f, 1.0f,
                load(saturationAsymmetries[index]) * 0.01f));
        resetSmoother(poleSpreadValue[index], juce::jlimit(
            0.0f, 1.0f, load(poleSpreads[index]) * 0.01f));
        feedbackValue[index].reset(
            currentSampleRate, nonlinearSmoothingSeconds);
        feedbackValue[index].setCurrentAndTargetValue(juce::jlimit(
            0.0f, 1.0f, load(feedbacks[index]) * 0.01f));
        feedbackLimitValue[index].reset(
            currentSampleRate, nonlinearSmoothingSeconds);
        feedbackLimitValue[index].setCurrentAndTargetValue(juce::jlimit(
            0.0f, 1.0f, load(feedbackLimits[index]) * 0.01f));
        ladderStageSaturationValue[index].reset(
            currentSampleRate, nonlinearSmoothingSeconds);
        ladderStageSaturationValue[index].setCurrentAndTargetValue(
            juce::jlimit(0.0f, 1.0f,
                load(ladderStageSaturations[index]) * 0.01f));
        ladderFeedbackSaturationValue[index].reset(
            currentSampleRate, nonlinearSmoothingSeconds);
        ladderFeedbackSaturationValue[index].setCurrentAndTargetValue(
            juce::jlimit(0.0f, 1.0f,
                load(ladderFeedbackSaturations[index]) * 0.01f));
        ladderResonanceCompensationValue[index].reset(
            currentSampleRate, nonlinearSmoothingSeconds);
        ladderResonanceCompensationValue[index].setCurrentAndTargetValue(
            juce::jlimit(0.0f, 1.0f,
                load(ladderResonanceCompensations[index]) * 0.01f));
        otaCellDriveValue[index].reset(
            currentSampleRate, nonlinearSmoothingSeconds);
        otaCellDriveValue[index].setCurrentAndTargetValue(juce::jlimit(
            0.0f, 1.0f, load(otaCellDrives[index]) * 0.01f));
        otaFeedbackDampingValue[index].reset(
            currentSampleRate, nonlinearSmoothingSeconds);
        otaFeedbackDampingValue[index].setCurrentAndTargetValue(juce::jlimit(
            0.0f, 1.0f, load(otaFeedbackDampings[index]) * 0.01f));
        otaResonanceCompensationValue[index].reset(
            currentSampleRate, nonlinearSmoothingSeconds);
        otaResonanceCompensationValue[index].setCurrentAndTargetValue(
            juce::jlimit(0.0f, 1.0f,
                load(otaResonanceCompensations[index]) * 0.01f));
        resetSmoother(slopeValue[index], juce::jlimit(
            6.0f, 48.0f, load(slopes[index])));
        const int selectedArchitecture = dusk::architecture::normaliseChoice(
            static_cast<int>(std::lround(load(architectures[index]))));
        currentArchitectureChoice[index] = selectedArchitecture;
        previousArchitectureChoice[index] = selectedArchitecture;
        architectureCrossfadeValue[index].reset(
            currentSampleRate, nonlinearSmoothingSeconds);
        architectureCrossfadeValue[index].setCurrentAndTargetValue(1.0f);
        resetSmoother(filterEnabledValue[index],
            load(enabled[index]) >= 0.5f ? 1.0f : 0.0f);
        currentDistortionType[index] = distortionAlgorithmForChoice(
            static_cast<int>(std::lround(load(types[index]))));
        previousDistortionType[index] = currentDistortionType[index];
        resetSmoother(distortionTypeCrossfade[index], 1.0f);
    }
    resetSmoother(mixValue,
        juce::jlimit(0.0f, 1.0f, load(raw.mix) * 0.01f));
    resetSmoother(outputGainValue,
        juce::Decibels::decibelsToGain(load(raw.output)));
    resetSmoother(bypassValue, load(raw.bypass) >= 0.5f ? 1.0f : 0.0f);
    sequencerFreePhase = 0.0;
    sequencerHeldNotes = 0;
    previousSequencerCanonical = 1.0f;
    sequencerGateGain = 1.0f;
}

void DuskAudioProcessor::releaseResources()
{
    resetProcessingState();
    activeSequencerStep.store(-1, std::memory_order_relaxed);
}

void DuskAudioProcessor::resetFilterState() noexcept
{
    svfIntegratorOne.fill(0.0f);
    svfIntegratorTwo.fill(0.0f);
    onePoleState.fill(0.0f);
    bassShelfState.fill(0.0f);
    coreAsymmetryDcInput.fill(0.0f);
    coreAsymmetryDcOutput.fill(0.0f);
    filterFeedbackReturn.fill(0.0f);
    for (auto& stage : ladderStageState)
        stage.fill(0.0f);
    for (auto& stage : otaStageState)
        stage.fill(0.0f);
    otaFeedbackReturn.fill(0.0f);
    otaFeedbackDampingState.fill(0.0f);
    for (auto& stage : slopeIntegratorOne)
        stage.fill(0.0f);
    for (auto& stage : slopeIntegratorTwo)
        stage.fill(0.0f);
}

void DuskAudioProcessor::resetProcessingState() noexcept
{
    resetDistortionState();
    resetFilterState();
}

std::complex<double> DuskAudioProcessor::getCleanSvfResponse(
    double frequencyHz, double sampleRate, float cutoffHz,
    float resonancePercent, float slopeDbPerOctave,
    float resonanceShapePercent, float bassLossPercent,
    float resonanceLimitPercent, float poleSpreadPercent,
    float feedbackPercent, int filterTypeChoice) noexcept
{
    const double safeSampleRate = juce::jmax(1.0, sampleRate);
    const double safeFrequency = juce::jlimit(
        0.0, safeSampleRate * 0.5, frequencyHz);
    const double safeCutoff = juce::jlimit(
        20.0, juce::jmin(20000.0, safeSampleRate * 0.45),
        static_cast<double>(cutoffHz));
    const auto z = std::polar(
        1.0, juce::MathConstants<double>::twoPi
                 * safeFrequency / safeSampleRate);
    const double maximumCutoff = juce::jmin(
        20000.0, safeSampleRate * 0.45);
    const auto stageResponse = [safeSampleRate, z](
        double stageCutoff, double resonance)
    {
        const double g = std::tan(
            juce::MathConstants<double>::pi
                * stageCutoff / safeSampleRate);
        const double damping = 1.41421356 - 1.36421356 * resonance;
        const double a1 = 1.0 / (1.0 + g * (g + damping));
        const double a2 = g * a1;
        const double a3 = g * a2;
        const auto m00 = z - (2.0 * a1 - 1.0);
        const std::complex<double> m01(2.0 * a2, 0.0);
        const std::complex<double> m10(-2.0 * a2, 0.0);
        const auto m11 = z - (1.0 - 2.0 * a3);
        const auto determinant = m00 * m11 - m01 * m10;
        if (std::abs(determinant) < 1.0e-18)
            return std::array<std::complex<double>, 5>
            {
                std::complex<double>(0.0, 0.0),
                std::complex<double>(0.0, 0.0),
                std::complex<double>(0.0, 0.0),
                std::complex<double>(0.0, 0.0),
                std::complex<double>(0.0, 0.0)
            };
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
        const auto notch = low + high;
        const auto peak = low - high;
        return std::array<std::complex<double>, 5>
            { low, band, high, notch, peak };
    };
    const auto onePoleResponse = [safeSampleRate, z](double stageCutoff)
    {
        const double g = std::tan(
            juce::MathConstants<double>::pi
                * stageCutoff / safeSampleRate);
        const double a = g / (1.0 + g);
        return std::complex<double>(
            a + (1.0 - a) * (2.0 * a)
                / (z - (1.0 - 2.0 * a)));
    };

    // Exact z-domain response of the audio path below. The lower half blends
    // a true one-pole response into the resonant two-pole core. Above 12 dB,
    // three neutral two-pole stages progressively descend from the top of the
    // usable band, reaching 24, 36 and 48 dB/octave without fixed switches.
    // Selectable distortion remains outside this small-signal calculation.
    // Feedback Limit is nonlinear but has unity slope at the origin, so the
    // graph can represent the exact low-level one-sample feedback loop.
    const double resonance = shapedResonance(
        juce::jlimit(0.0, 1.0,
            static_cast<double>(resonancePercent) * 0.01),
        juce::jlimit(-1.0, 1.0,
            static_cast<double>(resonanceShapePercent) * 0.01))
        * juce::jlimit(0.0, 1.0,
            static_cast<double>(resonanceLimitPercent) * 0.01);
    const double slope = juce::jlimit(
        6.0, 48.0, static_cast<double>(slopeDbPerOctave));
    const double poleSpread = juce::jlimit(
        0.0, 1.0, static_cast<double>(poleSpreadPercent) * 0.01);
    const double feedbackGain = 0.995 * juce::jlimit(
        0.0, 1.0, static_cast<double>(feedbackPercent) * 0.01);
    const int filterType = juce::jlimit(0, 4, filterTypeChoice);
    const auto selectResponse = [filterType](
        const std::array<std::complex<double>, 5>& outputs,
        bool normaliseBand)
    {
        const double gain = filterType == 1 && normaliseBand
            ? 1.41421356 : 1.0;
        return outputs[static_cast<size_t>(filterType)] * gain;
    };
    const auto primaryOutputs = stageResponse(safeCutoff, resonance);
    const auto primary = selectResponse(primaryOutputs, false);
    std::complex<double> response;
    if (slope < 12.0)
    {
        const double blend = (slope - 6.0) / 6.0;
        const auto onePoleLow = onePoleResponse(safeCutoff);
        const auto onePoleHigh = std::complex<double>(1.0, 0.0)
                               - onePoleLow;
        const std::array<std::complex<double>, 5> lowerSlopeResponses
        {
            onePoleLow, primaryOutputs[1], onePoleHigh,
            std::complex<double>(1.0, 0.0), onePoleLow - onePoleHigh
        };
        const auto lowerSlopeResponse = lowerSlopeResponses[
            static_cast<size_t>(filterType)];
        response = lowerSlopeResponse * (1.0 - blend) + primary * blend;
    }
    else
    {
        response = primary;
        const double extraStageAmount = (slope - 12.0) / 12.0;
        for (int stage = 0; stage < 3; ++stage)
        {
            const double amount = juce::jlimit(
                0.0, 1.0, extraStageAmount - static_cast<double>(stage));
            double additionalCutoff = safeCutoff;
            double activation = amount;
            if (filterType == 0)
            {
                additionalCutoff = juce::jlimit(
                    safeCutoff, maximumCutoff,
                    safeCutoff / juce::jmax(1.0e-6, amount));
                activation = juce::jlimit(0.0, 1.0, amount * 20.0);
            }
            else if (filterType == 2)
            {
                additionalCutoff = juce::jlimit(
                    20.0, safeCutoff, safeCutoff * amount);
                activation = juce::jlimit(0.0, 1.0, amount * 20.0);
            }
            constexpr std::array<double, 3> spreadPattern
                { 1.0, -1.0, 0.0 };
            const double typeDirection = filterType == 2 ? -1.0 : 1.0;
            const double spreadOctaves = 3.0 * poleSpread * amount
                * spreadPattern[static_cast<size_t>(stage)] * typeDirection;
            additionalCutoff = juce::jlimit(
                20.0, maximumCutoff,
                additionalCutoff * std::pow(2.0, spreadOctaves));
            const auto additionalStage = selectResponse(
                stageResponse(additionalCutoff, 0.0), true);
            response *= (1.0 - activation) + activation * additionalStage;
        }
    }

    // Model the resonance-linked low-end thinning of classic feedback
    // filters with a smooth low shelf two octaves below Cutoff. At 0% the
    // shelf is exactly unity; at 100% and maximum effective resonance it
    // reaches -24 dB while leaving the cutoff peak substantially intact.
    const double resonanceInfluence = resonance * juce::jlimit(
        0.0, 1.0, (slope - 6.0) / 6.0);
    const double bassLoss = juce::jlimit(
        0.0, 1.0, static_cast<double>(bassLossPercent) * 0.01);
    const double lowGain = std::pow(
        10.0, (-24.0 * bassLoss * resonanceInfluence) / 20.0);
    const double shelfCutoff = juce::jlimit(
        20.0, maximumCutoff, safeCutoff * 0.25);
    const auto shelfLowPass = onePoleResponse(shelfCutoff);
    const auto openLoopResponse = response
        * (1.0 + (lowGain - 1.0) * shelfLowPass);
    const auto feedbackDenominator = std::complex<double>(1.0, 0.0)
        - feedbackGain * openLoopResponse / z;
    if (std::abs(feedbackDenominator) < 1.0e-12)
        return openLoopResponse * 1.0e12;
    return openLoopResponse / feedbackDenominator;
}

std::complex<double> DuskAudioProcessor::getLadderResponse(
    double frequencyHz, double sampleRate, float cutoffHz,
    float resonancePercent, float slopeDbPerOctave,
    float resonanceCompensationPercent,
    int filterTypeChoice) noexcept
{
    const double safeSampleRate = juce::jmax(1.0, sampleRate);
    const double safeFrequency = juce::jlimit(
        0.0, safeSampleRate * 0.5, frequencyHz);
    const double safeCutoff = juce::jlimit(
        20.0, juce::jmin(20000.0, safeSampleRate * 0.45),
        static_cast<double>(cutoffHz));
    const auto z = std::polar(
        1.0, juce::MathConstants<double>::twoPi
                 * safeFrequency / safeSampleRate);
    const double g = std::tan(
        juce::MathConstants<double>::pi * safeCutoff / safeSampleRate);
    const double coefficient = g / (1.0 + g);
    const auto onePole = std::complex<double>(
        coefficient + (1.0 - coefficient) * (2.0 * coefficient)
            / (z - (1.0 - 2.0 * coefficient)));
    const int filterType = juce::jlimit(0, 4, filterTypeChoice);
    const auto responseForPoles = [onePole, filterType](int poles)
    {
        poles = juce::jlimit(1, maximumLadderStages, poles);
        const auto low = std::pow(onePole, poles);
        const auto high = std::pow(
            std::complex<double>(1.0, 0.0) - onePole, poles);
        int lowOrder = (poles + 1) / 2;
        int highOrder = poles - lowOrder;
        if (highOrder == 0)
            highOrder = 1;
        const auto band = std::pow(2.0, 0.5 * (lowOrder + highOrder))
            * std::pow(onePole, lowOrder)
            * std::pow(std::complex<double>(1.0, 0.0) - onePole,
                       highOrder);
        const std::array<std::complex<double>, 5> outputs
            { low, band, high, low + high, low - high };
        return outputs[static_cast<size_t>(filterType)];
    };
    const double polePosition = juce::jlimit(
        1.0, static_cast<double>(maximumLadderStages),
        static_cast<double>(slopeDbPerOctave) / 6.0);
    const int lowerPoles = juce::jlimit(
        1, maximumLadderStages, static_cast<int>(std::floor(polePosition)));
    const int upperPoles = juce::jmin(maximumLadderStages, lowerPoles + 1);
    const double poleFraction = polePosition - lowerPoles;
    const auto selectedResponse = responseForPoles(lowerPoles)
        + (responseForPoles(upperPoles) - responseForPoles(lowerPoles))
            * poleFraction;
    const auto lowerLow = std::pow(onePole, lowerPoles);
    const auto upperLow = std::pow(onePole, upperPoles);
    const auto feedbackResponse = lowerLow
        + (upperLow - lowerLow) * poleFraction;
    const double resonance = juce::jlimit(
        0.0, 1.0, static_cast<double>(resonancePercent) * 0.01);
    const double compensation = juce::jlimit(
        0.0, 1.0,
        static_cast<double>(resonanceCompensationPercent) * 0.01);
    const double inputGain = 1.0 + 3.0 * resonance * compensation;
    const auto criticalFeedback = [](int poles)
    {
        if (poles <= 2)
            return 8.0;
        const double cornerMagnitude = std::cos(
            juce::MathConstants<double>::pi / poles);
        return 1.0 / std::pow(cornerMagnitude, poles);
    };
    const double maximumFeedback = criticalFeedback(lowerPoles)
        + (criticalFeedback(upperPoles) - criticalFeedback(lowerPoles))
            * poleFraction;
    const double feedbackGain = 0.98 * maximumFeedback * resonance;
    const auto denominator = std::complex<double>(1.0, 0.0)
        + feedbackGain * feedbackResponse;
    if (std::abs(denominator) < 1.0e-12)
        return selectedResponse * inputGain * 1.0e12;
    return selectedResponse * inputGain / denominator;
}

std::complex<double> DuskAudioProcessor::getOtaResponse(
    double frequencyHz, double sampleRate, float cutoffHz,
    float resonancePercent, float slopeDbPerOctave,
    float feedbackDampingPercent, float resonanceCompensationPercent,
    int filterTypeChoice) noexcept
{
    const double safeSampleRate = juce::jmax(1.0, sampleRate);
    const double safeFrequency = juce::jlimit(
        0.0, safeSampleRate * 0.5, frequencyHz);
    const double maximumCutoff = juce::jmin(
        20000.0, safeSampleRate * 0.45);
    const double safeCutoff = juce::jlimit(
        20.0, maximumCutoff, static_cast<double>(cutoffHz));
    const auto z = std::polar(
        1.0, juce::MathConstants<double>::twoPi
                 * safeFrequency / safeSampleRate);
    const double g = std::tan(
        juce::MathConstants<double>::pi * safeCutoff / safeSampleRate);
    const double coefficient = g / (1.0 + g);
    const auto cell = std::complex<double>(
        coefficient + (1.0 - coefficient) * (2.0 * coefficient)
            / (z - (1.0 - 2.0 * coefficient)));
    const int filterType = juce::jlimit(0, 4, filterTypeChoice);
    const auto responseForCells = [cell, filterType](int cells)
    {
        cells = juce::jlimit(1, maximumOtaStages, cells);
        const auto low = std::pow(cell, cells);
        const auto high = std::pow(
            std::complex<double>(1.0, 0.0) - cell, cells);
        int lowOrder = (cells + 1) / 2;
        int highOrder = cells - lowOrder;
        if (highOrder == 0)
            highOrder = 1;
        const auto band = std::pow(2.0, 0.5 * (lowOrder + highOrder))
            * std::pow(cell, lowOrder)
            * std::pow(std::complex<double>(1.0, 0.0) - cell,
                       highOrder);
        const std::array<std::complex<double>, 5> outputs
            { low, band, high, low + high, low - high };
        return outputs[static_cast<size_t>(filterType)];
    };
    const double cellPosition = juce::jlimit(
        1.0, static_cast<double>(maximumOtaStages),
        static_cast<double>(slopeDbPerOctave) / 6.0);
    const int lowerCells = juce::jlimit(
        1, maximumOtaStages, static_cast<int>(std::floor(cellPosition)));
    const int upperCells = juce::jmin(maximumOtaStages, lowerCells + 1);
    const double cellFraction = cellPosition - lowerCells;
    const auto selectedResponse = responseForCells(lowerCells)
        + (responseForCells(upperCells) - responseForCells(lowerCells))
            * cellFraction;
    const auto lowerLow = std::pow(cell, lowerCells);
    const auto upperLow = std::pow(cell, upperCells);
    const auto feedbackResponse = lowerLow
        + (upperLow - lowerLow) * cellFraction;
    const double resonance = juce::jlimit(
        0.0, 1.0, static_cast<double>(resonancePercent) * 0.01);
    const double damping = juce::jlimit(
        0.0, 1.0, static_cast<double>(feedbackDampingPercent) * 0.01);
    const double effectiveDamping = std::pow(damping, 0.75);
    const double dampingCutoff = juce::jlimit(
        100.0, maximumCutoff,
        maximumCutoff * std::pow(0.005, effectiveDamping));
    const double dampingCoefficient = 1.0 - std::exp(
        -juce::MathConstants<double>::twoPi
            * dampingCutoff / safeSampleRate);
    const auto dampingResponse = dampingCoefficient * z
        / (z - (1.0 - dampingCoefficient));
    const double dampingStability = 1.0 - 0.82 * effectiveDamping;
    const double feedbackGain = 0.95 * dampingStability * resonance;
    const double compensation = juce::jlimit(
        0.0, 1.0,
        static_cast<double>(resonanceCompensationPercent) * 0.01);
    const double inputGain = 1.0 + 3.0 * resonance * compensation;
    const auto denominator = std::complex<double>(1.0, 0.0)
        + feedbackGain * dampingResponse * feedbackResponse / z;
    if (std::abs(denominator) < 1.0e-12)
        return selectedResponse * inputGain * 1.0e12;
    return selectedResponse * inputGain / denominator;
}

std::complex<double> DuskAudioProcessor::getArchitectureResponse(
    int architectureChoice, double frequencyHz, double sampleRate,
    float cutoffHz, float resonancePercent, float slopeDbPerOctave,
    float resonanceShapePercent, float bassLossPercent,
    float resonanceLimitPercent, float poleSpreadPercent,
    float feedbackPercent, float ladderStageSaturationPercent,
    float ladderResonanceCompensationPercent,
    float otaCellDrivePercent, float otaFeedbackDampingPercent,
    float otaResonanceCompensationPercent,
    int filterTypeChoice) noexcept
{
    juce::ignoreUnused(ladderStageSaturationPercent, otaCellDrivePercent);
    switch (static_cast<dusk::architecture::Id>(
        dusk::architecture::normaliseChoice(architectureChoice)))
    {
        case dusk::architecture::Id::ladder:
            return getLadderResponse(
                frequencyHz, sampleRate, cutoffHz, resonancePercent,
                slopeDbPerOctave, ladderResonanceCompensationPercent,
                filterTypeChoice);
        case dusk::architecture::Id::ota:
            return getOtaResponse(
                frequencyHz, sampleRate, cutoffHz, resonancePercent,
                slopeDbPerOctave, otaFeedbackDampingPercent,
                otaResonanceCompensationPercent, filterTypeChoice);
        case dusk::architecture::Id::cleanSvf:
        default:
            return getCleanSvfResponse(
                frequencyHz, sampleRate, cutoffHz, resonancePercent,
                slopeDbPerOctave, resonanceShapePercent, bassLossPercent,
                resonanceLimitPercent, poleSpreadPercent, feedbackPercent,
                filterTypeChoice);
    }
}

float DuskAudioProcessor::processSvfFilter(
    float sample, int engine, int channel, float filterTypePosition,
    float cutoffHz, float resonance, float slopeDbPerOctave,
    float bassLoss, float coreSaturation,
    float saturationAsymmetry, float poleSpread,
    float feedback, float feedbackLimit,
    bool internalDistortion, float distortionAmount,
    int currentDistortion, int previousDistortion,
    float distortionCrossfade) noexcept
{
    const int safeEngine = juce::jlimit(
        0, filterEngineCount - 1, engine);
    const int safeChannel = juce::jlimit(0, channelCount - 1, channel);
    const auto stateIndex = static_cast<size_t>(
        safeEngine * channelCount + safeChannel);
    const float sampleRate = static_cast<float>(
        juce::jmax(1.0, currentSampleRate));
    const float safeCutoff = juce::jlimit(
        20.0f, juce::jmin(20000.0f, sampleRate * 0.45f), cutoffHz);
    const float maximumCutoff = juce::jmin(
        20000.0f, sampleRate * 0.45f);
    const float safeCoreSaturation = juce::jlimit(
        0.0f, 1.0f, coreSaturation);
    const float safePoleSpread = juce::jlimit(
        0.0f, 1.0f, poleSpread);
    const float feedbackGain = 0.995f * juce::jlimit(
        0.0f, 1.0f, feedback);
    const float feedbackThreshold = 0.10f * std::pow(
        80.0f, juce::jlimit(0.0f, 1.0f, feedbackLimit));
    const float boundedFeedback = feedbackThreshold * std::tanh(
        filterFeedbackReturn[stateIndex] / feedbackThreshold);
    const float internalFeedback = internalDistortion
        ? sanitiseSample(processSelectedDistortion(
            boundedFeedback, distortionAmount, engine, channel,
            currentDistortion, previousDistortion, distortionCrossfade,
            cleanInternalDistortionStateBank), 8.0f)
        : boundedFeedback;
    const float filterInput = sanitiseSample(
        sample + feedbackGain * internalFeedback, 64.0f);
    const float saturationCharacter = upperRangeCharacter(
        safeCoreSaturation);
    const float saturationThreshold = safeCoreSaturation <= 1.0e-6f
        ? 4.0f
        : 4.0f * std::pow(0.0125f, saturationCharacter);
    const float safeAsymmetry = juce::jlimit(
        -1.0f, 1.0f, saturationAsymmetry);
    const float asymmetryCharacter = std::copysign(
        std::pow(std::abs(safeAsymmetry), 1.25f), safeAsymmetry)
        * std::sqrt(saturationCharacter);
    const auto symmetricSaturate = [safeCoreSaturation,
                                    saturationThreshold](float value)
    {
        if (safeCoreSaturation <= 1.0e-6f)
            return value;
        return saturationThreshold * std::tanh(value / saturationThreshold);
    };
    const auto asymmetricSaturate = [safeCoreSaturation,
                                     saturationThreshold,
                                     asymmetryCharacter](float value)
    {
        if (safeCoreSaturation <= 1.0e-6f)
            return value;
        // Different positive and negative thresholds create bounded even
        // harmonics while retaining zero at zero and unity slope at the
        // origin on both sides. The existing delta DC blocker removes the
        // resulting programme-dependent offset without touching core memory.
        const float polarity = value >= 0.0f ? 1.0f : -1.0f;
        const float polarityThreshold = saturationThreshold * std::pow(
            6.0f, -polarity * asymmetryCharacter);
        return polarityThreshold * std::tanh(value / polarityThreshold);
    };
    const auto processStage = [sampleRate](
        float input, float stageCutoff, float stageResonance,
        float& integratorOne, float& integratorTwo)
    {
        const float safeResonance = juce::jlimit(
            0.0f, 1.0f, stageResonance);
        const float g = std::tan(
            juce::MathConstants<float>::pi * stageCutoff / sampleRate);
        const float damping = 1.41421356f
                            - 1.36421356f * safeResonance;
        const float a1 = 1.0f / (1.0f + g * (g + damping));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = input - integratorTwo;
        const float v1 = a1 * integratorOne + a2 * v3;
        const float v2 = integratorTwo
                       + a2 * integratorOne + a3 * v3;
        integratorOne = DuskAudioProcessor::sanitiseSample(
            2.0f * v1 - integratorOne, 64.0f);
        integratorTwo = DuskAudioProcessor::sanitiseSample(
            2.0f * v2 - integratorTwo, 64.0f);
        const float low = DuskAudioProcessor::sanitiseSample(v2, 64.0f);
        const float band = DuskAudioProcessor::sanitiseSample(v1, 64.0f);
        const float high = DuskAudioProcessor::sanitiseSample(
            input - damping * band - low, 64.0f);
        const float notch = DuskAudioProcessor::sanitiseSample(
            low + high, 64.0f);
        const float peak = DuskAudioProcessor::sanitiseSample(
            low - high, 64.0f);
        return std::array<float, 5> { low, band, high, notch, peak };
    };
    const auto selectOutput = [](const std::array<float, 5>& outputs,
                                 float typePosition,
                                 bool normaliseBand)
    {
        const float type = juce::jlimit(0.0f, 4.0f, typePosition);
        const int leftIndex = juce::jlimit(
            0, 4, static_cast<int>(std::floor(type)));
        const int rightIndex = juce::jmin(4, leftIndex + 1);
        const float fraction = type - static_cast<float>(leftIndex);
        const auto valueAt = [&outputs, normaliseBand](int index)
        {
            const float gain = index == 1 && normaliseBand
                ? 1.41421356f : 1.0f;
            return outputs[static_cast<size_t>(index)] * gain;
        };
        const float left = valueAt(leftIndex);
        return left + (valueAt(rightIndex) - left) * fraction;
    };
    const auto processOnePole = [sampleRate](
        float input, float stageCutoff, float& state)
    {
        const float g = std::tan(
            juce::MathConstants<float>::pi * stageCutoff / sampleRate);
        const float a = g / (1.0f + g);
        const float v = (input - state) * a;
        const float output = v + state;
        state = DuskAudioProcessor::sanitiseSample(output + v, 64.0f);
        return DuskAudioProcessor::sanitiseSample(output, 64.0f);
    };

    // A true one-pole path provides the 6 dB endpoint and blends into the
    // resonant two-pole core by 12 dB. Three continuously running neutral
    // two-pole stages then descend into range to reach 24, 36 and 48 dB.
    // Keeping every state live makes fast Slope automation click-free. Core
    // Sat and Sat Asym deliberately do not rewrite these persistent states;
    // their memory-preserving shaping is applied to the completed core signal.
    const float type = juce::jlimit(0.0f, 4.0f, filterTypePosition);
    const float onePoleLow = processOnePole(
        filterInput, safeCutoff, onePoleState[stateIndex]);
    const float onePoleHigh = filterInput - onePoleLow;
    const auto primaryOutputs = processStage(
        filterInput, safeCutoff, resonance, svfIntegratorOne[stateIndex],
        svfIntegratorTwo[stateIndex]);
    const float primary = selectOutput(primaryOutputs, type, false);
    const std::array<float, 5> lowerSlopeOutputs
    {
        onePoleLow, primaryOutputs[1], onePoleHigh,
        filterInput, onePoleLow - onePoleHigh
    };
    const float lowerSlopeOutput = selectOutput(
        lowerSlopeOutputs, type, false);
    const float slope = juce::jlimit(
        6.0f, 48.0f, slopeDbPerOctave);
    float cascaded = primary;
    const float extraStageAmount = juce::jmax(
        0.0f, (slope - 12.0f) / 12.0f);
    for (int stage = 0; stage < 3; ++stage)
    {
        const auto stageIndex = static_cast<size_t>(stage);
        const float amount = juce::jlimit(
            0.0f, 1.0f,
            extraStageAmount - static_cast<float>(stage));
        const float lowPassCutoff = juce::jlimit(
            safeCutoff, maximumCutoff,
            safeCutoff / juce::jmax(1.0e-6f, amount));
        const float highPassCutoff = juce::jlimit(
            20.0f, safeCutoff, safeCutoff * amount);
        const int leftType = juce::jlimit(
            0, 4, static_cast<int>(std::floor(type)));
        const int rightType = juce::jmin(4, leftType + 1);
        const float typeFraction = type - static_cast<float>(leftType);
        const auto cutoffForType = [lowPassCutoff, highPassCutoff,
                                    safeCutoff](int selectedType)
        {
            if (selectedType == 0)
                return lowPassCutoff;
            if (selectedType == 2)
                return highPassCutoff;
            return safeCutoff;
        };
        const float additionalCutoff = cutoffForType(leftType)
            + (cutoffForType(rightType) - cutoffForType(leftType))
                * typeFraction;
        constexpr std::array<float, 3> spreadPattern
            { 1.0f, -1.0f, 0.0f };
        const auto spreadDirectionForType = [](int selectedType)
        {
            return selectedType == 2 ? -1.0f : 1.0f;
        };
        const float spreadDirection = spreadDirectionForType(leftType)
            + (spreadDirectionForType(rightType)
                - spreadDirectionForType(leftType)) * typeFraction;
        const float spreadOctaves = 3.0f * safePoleSpread * amount
            * spreadPattern[stageIndex] * spreadDirection;
        const float spreadCutoff = juce::jlimit(
            20.0f, maximumCutoff,
            additionalCutoff * std::pow(2.0f, spreadOctaves));
        const auto stageOutputs = processStage(
            cascaded, spreadCutoff, 0.0f,
            slopeIntegratorOne[stageIndex][stateIndex],
            slopeIntegratorTwo[stageIndex][stateIndex]);
        const float edgeActivation = juce::jlimit(
            0.0f, 1.0f, amount * 20.0f);
        const auto activationForType = [edgeActivation, amount](
            int selectedType)
        {
            return selectedType == 0 || selectedType == 2
                ? edgeActivation : amount;
        };
        const float activation = activationForType(leftType)
            + (activationForType(rightType)
                - activationForType(leftType)) * typeFraction;
        const float stageOutput = selectOutput(stageOutputs, type, true);
        cascaded += (stageOutput - cascaded) * activation;
    }
    const float filtered = slope < 12.0f
        ? lowerSlopeOutput + (primary - lowerSlopeOutput)
            * ((slope - 6.0f) / 6.0f)
        : cascaded;
    const float resonanceInfluence = resonance * juce::jlimit(
        0.0f, 1.0f, (slope - 6.0f) / 6.0f);
    const float safeBassLoss = juce::jlimit(0.0f, 1.0f, bassLoss);
    const float lowGain = juce::Decibels::decibelsToGain(
        -24.0f * safeBassLoss * resonanceInfluence);
    const float shelfCutoff = juce::jlimit(
        20.0f, maximumCutoff, safeCutoff * 0.25f);
    const float symmetricCore = symmetricSaturate(filtered);
    const float asymmetricCore = asymmetricSaturate(filtered);
    const float asymmetryDelta = asymmetricCore - symmetricCore;
    const float dcRemovedDelta = sanitiseSample(
        asymmetryDelta - coreAsymmetryDcInput[stateIndex]
            + coreAsymmetryDcCoefficient
                * coreAsymmetryDcOutput[stateIndex],
        64.0f);
    coreAsymmetryDcInput[stateIndex] = sanitiseSample(
        asymmetryDelta, 64.0f);
    coreAsymmetryDcOutput[stateIndex] = dcRemovedDelta;
    const float saturatedCore = sanitiseSample(
        symmetricCore + dcRemovedDelta, 64.0f);
    const float shelfLow = processOnePole(
        saturatedCore, shelfCutoff, bassShelfState[stateIndex]);
    const float output = sanitiseSample(
        saturatedCore + (lowGain - 1.0f) * shelfLow, 64.0f);
    filterFeedbackReturn[stateIndex] = output;
    return output;
}

float DuskAudioProcessor::processLadderFilter(
    float sample, int engine, int channel, float filterTypePosition,
    float cutoffHz, float resonance, float slopeDbPerOctave,
    float stageSaturation, float feedbackSaturation,
    float resonanceCompensation,
    bool internalDistortion, float distortionAmount,
    int currentDistortion, int previousDistortion,
    float distortionCrossfade) noexcept
{
    const int safeEngine = juce::jlimit(
        0, filterEngineCount - 1, engine);
    const int safeChannel = juce::jlimit(0, channelCount - 1, channel);
    const auto stateIndex = static_cast<size_t>(
        safeEngine * channelCount + safeChannel);
    const float sampleRate = static_cast<float>(
        juce::jmax(1.0, currentSampleRate));
    const float safeCutoff = juce::jlimit(
        20.0f, juce::jmin(20000.0f, sampleRate * 0.45f), cutoffHz);
    const float g = std::tan(
        juce::MathConstants<float>::pi * safeCutoff / sampleRate);
    const float coefficient = g / (1.0f + g);
    const float safeResonance = juce::jlimit(0.0f, 1.0f, resonance);
    const float safeSaturation = juce::jlimit(
        0.0f, 1.0f, stageSaturation);
    const float safeFeedbackSaturation = juce::jlimit(
        0.0f, 1.0f, feedbackSaturation);
    const float safeCompensation = juce::jlimit(
        0.0f, 1.0f, resonanceCompensation);
    const float inputGain = 1.0f
        + 3.0f * safeResonance * safeCompensation;
    const float saturationCharacter = upperRangeCharacter(safeSaturation);
    const auto saturate = [safeSaturation,
                           saturationCharacter](float value)
    {
        if (safeSaturation <= 1.0e-6f)
            return value;
        const float drive = 1.0f + 31.0f * saturationCharacter;
        const float curved = std::tanh(drive * value) / drive;
        return value + (curved - value) * saturationCharacter;
    };
    const float polePosition = juce::jlimit(
        1.0f, static_cast<float>(maximumLadderStages),
        slopeDbPerOctave / 6.0f);
    const int lowerPoles = juce::jlimit(
        1, maximumLadderStages, static_cast<int>(std::floor(polePosition)));
    const int upperPoles = juce::jmin(maximumLadderStages, lowerPoles + 1);
    const float poleFraction = polePosition - static_cast<float>(lowerPoles);
    const auto criticalFeedback = [](int poles)
    {
        if (poles <= 2)
            return 8.0f;
        const float cornerMagnitude = std::cos(
            juce::MathConstants<float>::pi / static_cast<float>(poles));
        return 1.0f / std::pow(cornerMagnitude, static_cast<float>(poles));
    };
    const float maximumFeedback = criticalFeedback(lowerPoles)
        + (criticalFeedback(upperPoles) - criticalFeedback(lowerPoles))
            * poleFraction;
    const float feedbackGain = 0.98f * maximumFeedback * safeResonance;
    std::array<float, maximumLadderStages + 1> memoryTaps {};
    float memoryInput = 0.0f;
    for (int stage = 0; stage < maximumLadderStages; ++stage)
    {
        const float state = ladderStageState[
            static_cast<size_t>(stage)][stateIndex];
        const float output = coefficient * memoryInput
            + (1.0f - coefficient) * state;
        memoryTaps[static_cast<size_t>(stage + 1)] = output;
        memoryInput = output;
    }
    const float memoryFeedback = memoryTaps[
        static_cast<size_t>(lowerPoles)]
        + (memoryTaps[static_cast<size_t>(upperPoles)]
            - memoryTaps[static_cast<size_t>(lowerPoles)]) * poleFraction;
    const float internalMemoryFeedback = internalDistortion
        ? sanitiseSample(processSelectedDistortion(
            memoryFeedback, distortionAmount, engine, channel,
            currentDistortion, previousDistortion, distortionCrossfade,
            ladderInternalDistortionStateBank), 8.0f)
        : memoryFeedback;
    const float lowerFeedthrough = std::pow(coefficient, lowerPoles);
    const float upperFeedthrough = std::pow(coefficient, upperPoles);
    const float feedbackFeedthrough = lowerFeedthrough
        + (upperFeedthrough - lowerFeedthrough) * poleFraction;
    const float drivenInput = sample * inputGain;
    float ladderInput = (drivenInput
        - feedbackGain * internalMemoryFeedback)
        / (1.0f + feedbackGain * feedbackFeedthrough);
    if (safeFeedbackSaturation > 1.0e-6f
        && feedbackGain > 1.0e-6f)
    {
        const float feedbackCharacter = upperRangeCharacter(
            safeFeedbackSaturation);
        const float feedbackDrive = 1.0f
            + 31.0f * feedbackCharacter;
        for (int iteration = 0; iteration < 4; ++iteration)
        {
            const float feedbackSignal = internalMemoryFeedback
                + feedbackFeedthrough * ladderInput;
            const float curve = std::tanh(
                feedbackDrive * feedbackSignal);
            const float curvedFeedback = curve / feedbackDrive;
            const float saturatedFeedback = feedbackSignal
                + (curvedFeedback - feedbackSignal)
                    * feedbackCharacter;
            const float saturationSlope = 1.0f
                + ((1.0f - curve * curve) - 1.0f)
                    * feedbackCharacter;
            const float derivative = 1.0f
                + feedbackGain * feedbackFeedthrough
                    * saturationSlope;
            if (std::abs(derivative) <= 1.0e-6f)
                break;
            ladderInput -= (ladderInput
                + feedbackGain * saturatedFeedback - drivenInput)
                    / derivative;
            ladderInput = sanitiseSample(ladderInput, 64.0f);
        }
    }
    ladderInput = sanitiseSample(ladderInput, 64.0f);
    std::array<float, maximumLadderStages + 1> taps {};
    taps[0] = ladderInput;
    float stageInput = ladderInput;
    for (int stage = 0; stage < maximumLadderStages; ++stage)
    {
        auto& state = ladderStageState[static_cast<size_t>(stage)][stateIndex];
        const float delta = coefficient
            * (saturate(stageInput) - saturate(state));
        const float output = sanitiseSample(state + delta, 64.0f);
        state = sanitiseSample(output + delta, 64.0f);
        taps[static_cast<size_t>(stage + 1)] = output;
        stageInput = output;
    }
    const auto binomial = [](int n, int k)
    {
        int result = 1;
        for (int index = 1; index <= k; ++index)
            result = result * (n - index + 1) / index;
        return static_cast<float>(result);
    };
    const auto outputsForPoles = [&taps, &binomial](int poles)
    {
        poles = juce::jlimit(1, maximumLadderStages, poles);
        const float low = taps[static_cast<size_t>(poles)];
        float high = 0.0f;
        for (int index = 0; index <= poles; ++index)
            high += ((index & 1) == 0 ? 1.0f : -1.0f)
                * binomial(poles, index)
                * taps[static_cast<size_t>(index)];
        int lowOrder = (poles + 1) / 2;
        int highOrder = poles - lowOrder;
        if (highOrder == 0)
            highOrder = 1;
        float band = 0.0f;
        for (int index = 0; index <= highOrder; ++index)
            band += ((index & 1) == 0 ? 1.0f : -1.0f)
                * binomial(highOrder, index)
                * taps[static_cast<size_t>(lowOrder + index)];
        band *= std::pow(2.0f, 0.5f * (lowOrder + highOrder));
        return std::array<float, 5>
            { low, band, high, low + high, low - high };
    };
    const auto selectType = [filterTypePosition](
        const std::array<float, 5>& outputs)
    {
        const float type = juce::jlimit(0.0f, 4.0f, filterTypePosition);
        const int left = juce::jlimit(
            0, 4, static_cast<int>(std::floor(type)));
        const int right = juce::jmin(4, left + 1);
        const float fraction = type - static_cast<float>(left);
        return outputs[static_cast<size_t>(left)]
            + (outputs[static_cast<size_t>(right)]
                - outputs[static_cast<size_t>(left)]) * fraction;
    };
    const auto lowerOutputs = outputsForPoles(lowerPoles);
    const auto upperOutputs = outputsForPoles(upperPoles);
    const float lowerSelected = selectType(lowerOutputs);
    const float upperSelected = selectType(upperOutputs);
    return sanitiseSample(
        lowerSelected + (upperSelected - lowerSelected) * poleFraction,
        64.0f);
}

float DuskAudioProcessor::processOtaFilter(
    float sample, int engine, int channel, float filterTypePosition,
    float cutoffHz, float resonance, float slopeDbPerOctave,
    float cellDrive, float feedbackDamping,
    float resonanceCompensation,
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
    const float g = std::tan(
        juce::MathConstants<float>::pi * safeCutoff / sampleRate);
    const float coefficient = g / (1.0f + g);
    const float safeResonance = juce::jlimit(0.0f, 1.0f, resonance);
    const float safeDrive = juce::jlimit(0.0f, 1.0f, cellDrive);
    const float safeDamping = juce::jlimit(
        0.0f, 1.0f, feedbackDamping);
    const float safeCompensation = juce::jlimit(
        0.0f, 1.0f, resonanceCompensation);
    const float cellPosition = juce::jlimit(
        1.0f, static_cast<float>(maximumOtaStages),
        slopeDbPerOctave / 6.0f);
    const int lowerCells = juce::jlimit(
        1, maximumOtaStages, static_cast<int>(std::floor(cellPosition)));
    const int upperCells = juce::jmin(maximumOtaStages, lowerCells + 1);
    const float cellFraction = cellPosition - static_cast<float>(lowerCells);
    const float effectiveDamping = std::pow(safeDamping, 0.75f);
    const float dampingStability = 1.0f - 0.82f * effectiveDamping;
    const float feedbackGain = 0.95f * dampingStability * safeResonance;
    const float inputGain = 1.0f
        + 3.0f * safeResonance * safeCompensation;
    const float dampingCutoff = juce::jlimit(
        100.0f, maximumCutoff,
        maximumCutoff * std::pow(0.005f, effectiveDamping));
    const float dampingCoefficient = 1.0f - std::exp(
        -juce::MathConstants<float>::twoPi
            * dampingCutoff / sampleRate);
    auto& dampingState = otaFeedbackDampingState[stateIndex];
    dampingState = sanitiseSample(
        dampingState + dampingCoefficient
            * (otaFeedbackReturn[stateIndex] - dampingState),
        64.0f);
    const float boundedFeedback = 2.0f * std::tanh(
        dampingState * 0.5f);
    const float internalFeedback = internalDistortion
        ? sanitiseSample(processSelectedDistortion(
            boundedFeedback, distortionAmount, engine, channel,
            currentDistortion, previousDistortion, distortionCrossfade,
            otaInternalDistortionStateBank), 4.0f)
        : boundedFeedback;
    const float drivenInput = sanitiseSample(
        sample * inputGain - feedbackGain * internalFeedback, 64.0f);
    const float driveCharacter = upperRangeCharacter(safeDrive);
    const float driveGain = 1.0f + 39.0f * driveCharacter;
    const auto saturateDifference = [safeDrive, driveCharacter,
                                     driveGain](float value)
    {
        if (safeDrive <= 1.0e-6f)
            return value;
        const float curved = std::tanh(driveGain * value) / driveGain;
        return value + (curved - value) * driveCharacter;
    };
    std::array<float, maximumOtaStages + 1> taps {};
    taps[0] = drivenInput;
    float cellInput = drivenInput;
    for (int cell = 0; cell < maximumOtaStages; ++cell)
    {
        auto& state = otaStageState[static_cast<size_t>(cell)][stateIndex];
        const float delta = coefficient
            * saturateDifference(cellInput - state);
        const float output = sanitiseSample(state + delta, 64.0f);
        state = sanitiseSample(output + delta, 64.0f);
        taps[static_cast<size_t>(cell + 1)] = output;
        cellInput = output;
    }
    const auto binomial = [](int n, int k)
    {
        int result = 1;
        for (int index = 1; index <= k; ++index)
            result = result * (n - index + 1) / index;
        return static_cast<float>(result);
    };
    const auto outputsForCells = [&taps, &binomial](int cells)
    {
        cells = juce::jlimit(1, maximumOtaStages, cells);
        const float low = taps[static_cast<size_t>(cells)];
        float high = 0.0f;
        for (int index = 0; index <= cells; ++index)
            high += ((index & 1) == 0 ? 1.0f : -1.0f)
                * binomial(cells, index)
                * taps[static_cast<size_t>(index)];
        int lowOrder = (cells + 1) / 2;
        int highOrder = cells - lowOrder;
        if (highOrder == 0)
            highOrder = 1;
        float band = 0.0f;
        for (int index = 0; index <= highOrder; ++index)
            band += ((index & 1) == 0 ? 1.0f : -1.0f)
                * binomial(highOrder, index)
                * taps[static_cast<size_t>(lowOrder + index)];
        band *= std::pow(2.0f, 0.5f * (lowOrder + highOrder));
        return std::array<float, 5>
            { low, band, high, low + high, low - high };
    };
    const auto selectType = [filterTypePosition](
        const std::array<float, 5>& outputs)
    {
        const float type = juce::jlimit(0.0f, 4.0f, filterTypePosition);
        const int left = juce::jlimit(
            0, 4, static_cast<int>(std::floor(type)));
        const int right = juce::jmin(4, left + 1);
        const float fraction = type - static_cast<float>(left);
        return outputs[static_cast<size_t>(left)]
            + (outputs[static_cast<size_t>(right)]
                - outputs[static_cast<size_t>(left)]) * fraction;
    };
    const auto lowerOutputs = outputsForCells(lowerCells);
    const auto upperOutputs = outputsForCells(upperCells);
    const float lowerSelected = selectType(lowerOutputs);
    const float upperSelected = selectType(upperOutputs);
    otaFeedbackReturn[stateIndex] = sanitiseSample(
        taps[static_cast<size_t>(lowerCells)]
            + (taps[static_cast<size_t>(upperCells)]
                - taps[static_cast<size_t>(lowerCells)]) * cellFraction,
        64.0f);
    return sanitiseSample(
        lowerSelected + (upperSelected - lowerSelected) * cellFraction,
        64.0f);
}

float DuskAudioProcessor::processFilterArchitecture(
    float sample, int previousArchitecture, int currentArchitecture,
    float architectureCrossfade, int engine, int channel,
    float filterTypePosition, float cutoffHz, float basicResonance,
    float cleanResonance,
    float slopeDbPerOctave, float bassLoss, float coreSaturation,
    float saturationAsymmetry, float poleSpread, float feedback,
    float feedbackLimit, float ladderStageSaturation,
    float ladderFeedbackSaturation,
    float ladderResonanceCompensation, float otaCellDrive,
    float otaFeedbackDamping,
    float otaResonanceCompensation,
    bool internalDistortion, float distortionAmount,
    int currentDistortion, int previousDistortion,
    float distortionCrossfade) noexcept
{
    const float cleanOutput = processSvfFilter(
        sample, engine, channel, filterTypePosition, cutoffHz,
        cleanResonance, slopeDbPerOctave, bassLoss, coreSaturation,
        saturationAsymmetry, poleSpread, feedback, feedbackLimit,
        internalDistortion, distortionAmount, currentDistortion,
        previousDistortion, distortionCrossfade);
    const float ladderOutput = processLadderFilter(
        sample, engine, channel, filterTypePosition, cutoffHz,
        basicResonance, slopeDbPerOctave, ladderStageSaturation,
        ladderFeedbackSaturation,
        ladderResonanceCompensation, internalDistortion, distortionAmount,
        currentDistortion, previousDistortion, distortionCrossfade);
    const float otaOutput = processOtaFilter(
        sample, engine, channel, filterTypePosition, cutoffHz,
        basicResonance, slopeDbPerOctave, otaCellDrive,
        otaFeedbackDamping, otaResonanceCompensation, internalDistortion,
        distortionAmount, currentDistortion, previousDistortion,
        distortionCrossfade);
    const auto outputForArchitecture = [cleanOutput, ladderOutput, otaOutput](
        int architecture)
    {
        switch (static_cast<dusk::architecture::Id>(
            dusk::architecture::normaliseChoice(architecture)))
        {
            case dusk::architecture::Id::ladder: return ladderOutput;
            case dusk::architecture::Id::ota: return otaOutput;
            case dusk::architecture::Id::cleanSvf:
            default: return cleanOutput;
        }
    };
    const float previousOutput = outputForArchitecture(previousArchitecture);
    const float currentOutput = outputForArchitecture(currentArchitecture);
    const float blend = juce::jlimit(
        0.0f, 1.0f, architectureCrossfade);
    return sanitiseSample(
        previousOutput + (currentOutput - previousOutput) * blend, 64.0f);
}

float DuskAudioProcessor::nextNoiseSample(uint32_t& state) noexcept
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(static_cast<int32_t>(state))
        / 2147483648.0f;
}

float DuskAudioProcessor::sanitiseSample(float sample, float limit) noexcept
{
    if (! std::isfinite(sample))
        return 0.0f;
    return juce::jlimit(-limit, limit, sample);
}

void DuskAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int sampleCount = buffer.getNumSamples();
    const int hostChannels = buffer.getNumChannels();
    const int activeChannels = juce::jmin(channelCount, hostChannels);
    for (int channel = activeChannels; channel < hostChannels; ++channel)
        buffer.clear(channel, 0, sampleCount);
    if (sampleCount <= 0 || activeChannels <= 0)
        return;
    if (pendingProcessingReset.exchange(false, std::memory_order_acq_rel))
        resetProcessingState();

    const bool sequenceEnabled = load(raw.sequencerEnabled) >= 0.5f;
    const bool hostSync = load(raw.sequencerHostSync) >= 0.5f;
    const auto direction = static_cast<dusk::sequencer::Direction>(
        juce::jlimit(0, 4, static_cast<int>(std::lround(
            load(raw.sequencerDirection)))));

    juce::Optional<juce::AudioPlayHead::PositionInfo> hostPosition;
    if (sequenceEnabled && hostSync
        && direction != dusk::sequencer::Direction::played)
        if (auto* playhead = getPlayHead())
            hostPosition = playhead->getPosition();
    double bpm = 120.0;
    if (hostPosition.hasValue())
        if (const auto hostBpm = hostPosition->getBpm())
            if (std::isfinite(*hostBpm) && *hostBpm > 0.0)
                bpm = juce::jlimit(10.0, 1000.0, *hostBpm);

    bool phraseTriggered = false;
    if (sequenceEnabled && direction == dusk::sequencer::Direction::played)
    {
        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();
            if (message.isNoteOn())
            {
                if (sequencerHeldNotes == 0)
                    phraseTriggered = true;
                ++sequencerHeldNotes;
            }
            else if (message.isNoteOff())
                sequencerHeldNotes = juce::jmax(0, sequencerHeldNotes - 1);
            else if (message.isAllNotesOff() || message.isAllSoundOff())
                sequencerHeldNotes = 0;
        }
    }
    else
        sequencerHeldNotes = 0;

    std::array<float, dusk::sequencer::stepCount> steps {};
    float sequenceMinimum = 1.0f;
    float sequenceMaximum = 0.0f;
    for (int step = 0; step < dusk::sequencer::stepCount; ++step)
    {
        const auto index = static_cast<size_t>(step);
        steps[index] = juce::jlimit(
            0.0f, 1.0f, load(raw.sequencerSteps[index]));
        sequenceMinimum = juce::jmin(sequenceMinimum, steps[index]);
        sequenceMaximum = juce::jmax(sequenceMaximum, steps[index]);
    }
    const bool bipolar = load(raw.sequencerBipolar) >= 0.5f;
    const bool unipolarNegative = ! bipolar
        && load(raw.sequencerUnipolarNegative) >= 0.5f;
    const auto modulatedPrevious = [this, sequenceEnabled, bipolar,
                                    unipolarNegative](
        int target, const std::atomic<float>* parameter)
    {
        const float base = load(parameter);
        return sequenceEnabled
            ? sequencerModulatedValue(target, base,
                previousSequencerCanonical, bipolar, unipolarNegative)
            : base;
    };
    const float peak = juce::jlimit(-1.0f, 1.0f, modulatedPrevious(
        dusk::sequencer::sequencerDepthTargetChoice, raw.sequencerPeak));
    const float move = juce::jlimit(-1.0f, 1.0f, modulatedPrevious(
        dusk::sequencer::sequencerOffsetTargetChoice, raw.sequencerMove));
    if (sequenceEnabled
        && (std::abs(peak) > 0.000001f || std::abs(move) > 0.000001f))
        for (auto& value : steps)
            value = dusk::sequencer::applyDepthOffset(
                value, sequenceMinimum, sequenceMaximum,
                peak, move, bipolar);

    const int rate = juce::jlimit(
        0, dusk::sequencer::rateChoiceCount - 1,
        static_cast<int>(std::lround(load(raw.sequencerRate))));
    const double beatsPerStep = dusk::sequencer::beatsForRate(rate);
    const double phaseIncrement = bpm / (
        60.0 * beatsPerStep * currentSampleRate);
    if (phraseTriggered)
        sequencerFreePhase = 0.0;
    double gatePhaseAtBlockStart = sequencerFreePhase;
    double sequencePhase = sequencerFreePhase
        + 0.5 * static_cast<double>(sampleCount) * phaseIncrement;
    if (sequenceEnabled && hostSync
        && direction != dusk::sequencer::Direction::played
        && hostPosition.hasValue())
        if (const auto ppq = hostPosition->getPpqPosition())
            if (std::isfinite(*ppq))
            {
                sequencePhase = *ppq / beatsPerStep;
                gatePhaseAtBlockStart = sequencePhase;
            }
    if (sequenceEnabled)
        sequencerFreePhase += static_cast<double>(sampleCount) * phaseIncrement;
    if (sequencerFreePhase > 1048576.0)
        sequencerFreePhase = std::fmod(
            sequencerFreePhase,
            static_cast<double>(dusk::sequencer::stepCount * 8));

    const int firstStep = juce::jlimit(
        1, dusk::sequencer::stepCount - 1,
        static_cast<int>(std::lround(modulatedPrevious(
            dusk::sequencer::sequencerStartTargetChoice,
            raw.sequencerStart)))) - 1;
    const int lastStep = juce::jlimit(
        firstStep + 2, dusk::sequencer::stepCount,
        static_cast<int>(std::lround(modulatedPrevious(
            dusk::sequencer::sequencerEndTargetChoice,
            raw.sequencerEnd)))) - 1;
    const float attack = juce::jlimit(0.0f, 1.0f, modulatedPrevious(
        dusk::sequencer::sequencerAttackTargetChoice, raw.sequencerAttack));
    const float release = juce::jlimit(0.0f, 1.0f, modulatedPrevious(
        dusk::sequencer::sequencerReleaseTargetChoice,
        raw.sequencerRelease));
    int activeStep = -1;
    float canonical = 1.0f;
    if (sequenceEnabled)
        canonical = dusk::sequencer::evaluate(
            steps, sequencePhase, firstStep, lastStep, direction,
            attack, release, &activeStep);
    previousSequencerCanonical = canonical;
    activeSequencerStep.store(activeStep, std::memory_order_relaxed);

    const auto sequenced = [this, sequenceEnabled, canonical, bipolar,
                            unipolarNegative](
        int target, const std::atomic<float>* parameter)
    {
        const float base = load(parameter);
        return sequenceEnabled
            ? sequencerModulatedValue(target, base, canonical,
                bipolar, unipolarNegative)
            : base;
    };
    driveValue[0].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter1DriveTargetChoice,
            raw.filter1Drive) * 0.01f));
    driveValue[1].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter2DriveTargetChoice,
            raw.filter2Drive) * 0.01f));
    filterTypeValue[0].setTargetValue(juce::jlimit(
        0.0f, 4.0f, load(raw.filter1Type)));
    filterTypeValue[1].setTargetValue(juce::jlimit(
        0.0f, 4.0f, load(raw.filter2Type)));
    cutoffValue[0].setTargetValue(juce::jlimit(
        20.0f, 20000.0f, sequenced(
            dusk::sequencer::filter1CutoffTargetChoice,
            raw.filter1Cutoff)));
    resonanceValue[0].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter1ResonanceTargetChoice,
            raw.filter1Resonance) * 0.01f));
    cutoffValue[1].setTargetValue(juce::jlimit(
        20.0f, 20000.0f, sequenced(
            dusk::sequencer::filter2CutoffTargetChoice,
            raw.filter2Cutoff)));
    resonanceValue[1].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter2ResonanceTargetChoice,
            raw.filter2Resonance) * 0.01f));
    resonanceShapeValue[0].setTargetValue(juce::jlimit(
        -1.0f, 1.0f, sequenced(
            dusk::sequencer::filter1ResonanceShapeTargetChoice,
            raw.filter1ResonanceShape) * 0.01f));
    resonanceShapeValue[1].setTargetValue(juce::jlimit(
        -1.0f, 1.0f, sequenced(
            dusk::sequencer::filter2ResonanceShapeTargetChoice,
            raw.filter2ResonanceShape) * 0.01f));
    resonanceLimitValue[0].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter1ResonanceLimitTargetChoice,
            raw.filter1ResonanceLimit) * 0.01f));
    resonanceLimitValue[1].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter2ResonanceLimitTargetChoice,
            raw.filter2ResonanceLimit) * 0.01f));
    bassLossValue[0].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter1BassLossTargetChoice,
            raw.filter1BassLoss) * 0.01f));
    bassLossValue[1].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter2BassLossTargetChoice,
            raw.filter2BassLoss) * 0.01f));
    coreSaturationValue[0].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter1CoreSaturationTargetChoice,
            raw.filter1CoreSaturation) * 0.01f));
    coreSaturationValue[1].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter2CoreSaturationTargetChoice,
            raw.filter2CoreSaturation) * 0.01f));
    saturationAsymmetryValue[0].setTargetValue(juce::jlimit(
        -1.0f, 1.0f, sequenced(
            dusk::sequencer::filter1SaturationAsymmetryTargetChoice,
            raw.filter1SaturationAsymmetry) * 0.01f));
    saturationAsymmetryValue[1].setTargetValue(juce::jlimit(
        -1.0f, 1.0f, sequenced(
            dusk::sequencer::filter2SaturationAsymmetryTargetChoice,
            raw.filter2SaturationAsymmetry) * 0.01f));
    poleSpreadValue[0].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter1PoleSpreadTargetChoice,
            raw.filter1PoleSpread) * 0.01f));
    poleSpreadValue[1].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter2PoleSpreadTargetChoice,
            raw.filter2PoleSpread) * 0.01f));
    feedbackValue[0].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter1FeedbackTargetChoice,
            raw.filter1Feedback) * 0.01f));
    feedbackValue[1].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter2FeedbackTargetChoice,
            raw.filter2Feedback) * 0.01f));
    feedbackLimitValue[0].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter1FeedbackLimitTargetChoice,
            raw.filter1FeedbackLimit) * 0.01f));
    feedbackLimitValue[1].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter2FeedbackLimitTargetChoice,
            raw.filter2FeedbackLimit) * 0.01f));
    ladderStageSaturationValue[0].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter1LadderStageSaturationTargetChoice,
            raw.filter1LadderStageSaturation) * 0.01f));
    ladderStageSaturationValue[1].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter2LadderStageSaturationTargetChoice,
            raw.filter2LadderStageSaturation) * 0.01f));
    ladderFeedbackSaturationValue[0].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter1LadderFeedbackSaturationTargetChoice,
            raw.filter1LadderFeedbackSaturation) * 0.01f));
    ladderFeedbackSaturationValue[1].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter2LadderFeedbackSaturationTargetChoice,
            raw.filter2LadderFeedbackSaturation) * 0.01f));
    ladderResonanceCompensationValue[0].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter1LadderResonanceCompensationTargetChoice,
            raw.filter1LadderResonanceCompensation) * 0.01f));
    ladderResonanceCompensationValue[1].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter2LadderResonanceCompensationTargetChoice,
            raw.filter2LadderResonanceCompensation) * 0.01f));
    otaCellDriveValue[0].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter1OtaCellDriveTargetChoice,
            raw.filter1OtaCellDrive) * 0.01f));
    otaCellDriveValue[1].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter2OtaCellDriveTargetChoice,
            raw.filter2OtaCellDrive) * 0.01f));
    otaFeedbackDampingValue[0].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter1OtaFeedbackDampingTargetChoice,
            raw.filter1OtaFeedbackDamping) * 0.01f));
    otaFeedbackDampingValue[1].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter2OtaFeedbackDampingTargetChoice,
            raw.filter2OtaFeedbackDamping) * 0.01f));
    otaResonanceCompensationValue[0].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter1OtaResonanceCompensationTargetChoice,
            raw.filter1OtaResonanceCompensation) * 0.01f));
    otaResonanceCompensationValue[1].setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(
            dusk::sequencer::filter2OtaResonanceCompensationTargetChoice,
            raw.filter2OtaResonanceCompensation) * 0.01f));
    slopeValue[0].setTargetValue(juce::jlimit(
        6.0f, 48.0f, sequenced(
            dusk::sequencer::filter1SlopeTargetChoice,
            raw.filter1Slope)));
    slopeValue[1].setTargetValue(juce::jlimit(
        6.0f, 48.0f, sequenced(
            dusk::sequencer::filter2SlopeTargetChoice,
            raw.filter2Slope)));
    filterEnabledValue[0].setTargetValue(
        load(raw.filter1Enabled) >= 0.5f ? 1.0f : 0.0f);
    filterEnabledValue[1].setTargetValue(
        load(raw.filter2Enabled) >= 0.5f ? 1.0f : 0.0f);
    mixValue.setTargetValue(juce::jlimit(
        0.0f, 1.0f, sequenced(dusk::sequencer::mixTargetChoice,
            raw.mix) * 0.01f));
    outputGainValue.setTargetValue(juce::Decibels::decibelsToGain(
        sequenced(dusk::sequencer::outputTargetChoice, raw.output)));
    bypassValue.setTargetValue(load(raw.bypass) >= 0.5f ? 1.0f : 0.0f);

    const std::array<const std::atomic<float>*, filterEngineCount> types {
        raw.filter1DistortionType, raw.filter2DistortionType
    };
    for (int engine = 0; engine < filterEngineCount; ++engine)
    {
        const auto index = static_cast<size_t>(engine);
        const int selectedDistortion = distortionAlgorithmForChoice(
            static_cast<int>(std::lround(load(types[index]))));
        if (selectedDistortion != currentDistortionType[index])
        {
            previousDistortionType[index] = currentDistortionType[index];
            currentDistortionType[index] = selectedDistortion;
            distortionTypeCrossfade[index].setCurrentAndTargetValue(0.0f);
            distortionTypeCrossfade[index].reset(
                currentSampleRate, smoothingSeconds);
            distortionTypeCrossfade[index].setTargetValue(1.0f);
        }
    }

    const int routing = juce::jlimit(
        0, 3, static_cast<int>(std::lround(load(raw.routing))));
    const std::array<int, filterEngineCount> architecture {
        dusk::architecture::normaliseChoice(static_cast<int>(std::lround(
            load(raw.filter1Architecture)))),
        dusk::architecture::normaliseChoice(static_cast<int>(std::lround(
            load(raw.filter2Architecture))))
    };
    for (int engine = 0; engine < filterEngineCount; ++engine)
    {
        const auto index = static_cast<size_t>(engine);
        if (architecture[index] == currentArchitectureChoice[index])
            continue;
        previousArchitectureChoice[index] = currentArchitectureChoice[index];
        currentArchitectureChoice[index] = architecture[index];
        architectureCrossfadeValue[index].setCurrentAndTargetValue(0.0f);
        architectureCrossfadeValue[index].reset(
            currentSampleRate, nonlinearSmoothingSeconds);
        architectureCrossfadeValue[index].setTargetValue(1.0f);
    }
    const std::array<int, filterEngineCount> placement {
        juce::jlimit(0, 2, static_cast<int>(std::lround(
            load(raw.filter1DistortionPlacement)))),
        juce::jlimit(0, 2, static_cast<int>(std::lround(
            load(raw.filter2DistortionPlacement))))
    };

    std::array<dusk::sequencer::GateStepMode,
               dusk::sequencer::stepCount> gateSteps {};
    for (int step = 0; step < dusk::sequencer::stepCount; ++step)
        gateSteps[static_cast<size_t>(step)] =
            dusk::sequencer::gateStepModeFromChoice(
                load(raw.sequencerGateSteps[static_cast<size_t>(step)]));
    const float shortLength = juce::jlimit(0.10f, 0.60f, sequenced(
        dusk::sequencer::sequencerGateShortTargetChoice,
        raw.sequencerGateShortLength));
    const float longLength = juce::jlimit(0.65f, 0.95f, sequenced(
        dusk::sequencer::sequencerGateLongTargetChoice,
        raw.sequencerGateLongLength));
    const int gateMode = juce::jlimit(
        0, 3, juce::roundToInt(load(raw.sequencerGateMode)));
    const float gateSmoothing = 1.0f - std::exp(
        -1.0f / static_cast<float>(0.002 * currentSampleRate));

    auto* left = buffer.getWritePointer(0);
    auto* right = activeChannels > 1 ? buffer.getWritePointer(1) : nullptr;
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const float dryL = left[sample];
        const float dryR = right != nullptr ? right[sample] : dryL;
        float gateTarget = 1.0f;
        if (sequenceEnabled && gateMode != 3)
        {
            const auto position = dusk::sequencer::positionForPhase(
                gatePhaseAtBlockStart
                    + static_cast<double>(sample) * phaseIncrement,
                lastStep - firstStep + 1, direction);
            gateTarget = dusk::sequencer::gateStepTarget(
                gateSteps[static_cast<size_t>(firstStep + position.current)],
                position.fraction, shortLength, longLength);
        }
        sequencerGateGain += gateSmoothing
            * (gateTarget - sequencerGateGain);
        sequencerGateGain = juce::jlimit(
            0.0f, 1.0f, sequencerGateGain);

        float effectInputL = gateMode == 0
            ? dryL * sequencerGateGain : dryL;
        float effectInputR = gateMode == 0
            ? dryR * sequencerGateGain : dryR;
        const std::array<float, filterEngineCount> drive {
            driveValue[0].getNextValue(), driveValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount> cutoff {
            cutoffValue[0].getNextValue(), cutoffValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount> filterType {
            filterTypeValue[0].getNextValue(),
            filterTypeValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount> resonanceAmount {
            resonanceValue[0].getNextValue(), resonanceValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount> resonanceShape {
            resonanceShapeValue[0].getNextValue(),
            resonanceShapeValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount> resonance {
            shapedResonance(resonanceAmount[0], resonanceShape[0])
                * resonanceLimitValue[0].getNextValue(),
            shapedResonance(resonanceAmount[1], resonanceShape[1])
                * resonanceLimitValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount> bassLoss {
            bassLossValue[0].getNextValue(), bassLossValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount> coreSaturation {
            coreSaturationValue[0].getNextValue(),
            coreSaturationValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount> saturationAsymmetry {
            saturationAsymmetryValue[0].getNextValue(),
            saturationAsymmetryValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount> poleSpread {
            poleSpreadValue[0].getNextValue(),
            poleSpreadValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount> feedback {
            feedbackValue[0].getNextValue(),
            feedbackValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount> feedbackLimit {
            feedbackLimitValue[0].getNextValue(),
            feedbackLimitValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount> ladderStageSaturation {
            ladderStageSaturationValue[0].getNextValue(),
            ladderStageSaturationValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount> ladderFeedbackSaturation {
            ladderFeedbackSaturationValue[0].getNextValue(),
            ladderFeedbackSaturationValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount>
            ladderResonanceCompensation {
                ladderResonanceCompensationValue[0].getNextValue(),
                ladderResonanceCompensationValue[1].getNextValue()
            };
        const std::array<float, filterEngineCount> otaCellDrive {
            otaCellDriveValue[0].getNextValue(),
            otaCellDriveValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount> otaFeedbackDamping {
            otaFeedbackDampingValue[0].getNextValue(),
            otaFeedbackDampingValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount>
            otaResonanceCompensation {
                otaResonanceCompensationValue[0].getNextValue(),
                otaResonanceCompensationValue[1].getNextValue()
            };
        const std::array<float, filterEngineCount> architectureCrossfade {
            architectureCrossfadeValue[0].getNextValue(),
            architectureCrossfadeValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount> slope {
            slopeValue[0].getNextValue(), slopeValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount> filterEnabled {
            filterEnabledValue[0].getNextValue(),
            filterEnabledValue[1].getNextValue()
        };
        const std::array<float, filterEngineCount> typeBlend {
            distortionTypeCrossfade[0].getNextValue(),
            distortionTypeCrossfade[1].getNextValue()
        };
        const auto processEngine = [this, &drive, &filterType, &cutoff,
                                    &resonance, &bassLoss, &coreSaturation,
                                    &saturationAsymmetry, &poleSpread,
                                    &feedback, &feedbackLimit, &slope,
                                    &ladderStageSaturation,
                                    &ladderFeedbackSaturation,
                                    &ladderResonanceCompensation,
                                    &otaCellDrive, &otaFeedbackDamping,
                                    &otaResonanceCompensation,
                                    &filterEnabled, &typeBlend, &placement,
                                    &architectureCrossfade,
                                    &resonanceAmount](
            float input, int engine, int channel)
        {
            const auto index = static_cast<size_t>(engine);
            const auto distort = [this, &drive, &typeBlend, engine,
                                  channel, index](float value)
            {
                return processSelectedDistortion(
                    value, drive[index], engine, channel,
                    currentDistortionType[index],
                    previousDistortionType[index], typeBlend[index],
                    externalDistortionStateBank);
            };

            float output = input;
            if (placement[index] == 0)
                output = distort(output);
            output = processFilterArchitecture(
                output, previousArchitectureChoice[index],
                currentArchitectureChoice[index],
                architectureCrossfade[index], engine, channel,
                filterType[index], cutoff[index], resonanceAmount[index],
                resonance[index],
                slope[index], bassLoss[index], coreSaturation[index],
                saturationAsymmetry[index], poleSpread[index],
                feedback[index], feedbackLimit[index],
                ladderStageSaturation[index],
                ladderFeedbackSaturation[index],
                ladderResonanceCompensation[index], otaCellDrive[index],
                otaFeedbackDamping[index],
                otaResonanceCompensation[index], placement[index] == 1,
                drive[index], currentDistortionType[index],
                previousDistortionType[index], typeBlend[index]);
            if (placement[index] == 2)
                output = distort(output);
            return input + (output - input) * filterEnabled[index];
        };
        const auto routeSample = [routing, &processEngine](
            float input, int channel)
        {
            if (routing == 0)
                return processEngine(input, 0, channel);
            if (routing == 1)
                return processEngine(input, 1, channel);
            if (routing == 3)
                return 0.5f * (processEngine(input, 0, channel)
                             + processEngine(input, 1, channel));
            return processEngine(
                processEngine(input, 0, channel), 1, channel);
        };
        float wetL = routeSample(effectInputL, 0);
        float wetR = routeSample(effectInputR, 1);
        if (gateMode == 1)
        {
            wetL *= sequencerGateGain;
            wetR *= sequencerGateGain;
        }
        const float mix = mixValue.getNextValue();
        const float gain = outputGainValue.getNextValue();
        float processedL = gain * (dryL * (1.0f - mix) + wetL * mix);
        float processedR = gain * (dryR * (1.0f - mix) + wetR * mix);
        if (gateMode == 2)
        {
            processedL *= sequencerGateGain;
            processedR *= sequencerGateGain;
        }
        const float bypass = bypassValue.getNextValue();
        left[sample] = sanitiseSample(
            processedL + (dryL - processedL) * bypass, 64.0f);
        if (right != nullptr)
            right[sample] = sanitiseSample(
                processedR + (dryR - processedR) * bypass, 64.0f);
    }
    for (int engine = 0; engine < filterEngineCount; ++engine)
    {
        const auto index = static_cast<size_t>(engine);
        if (! distortionTypeCrossfade[index].isSmoothing())
            previousDistortionType[index] = currentDistortionType[index];
    }
}

void DuskAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    state.setProperty("schemaVersion", stateSchemaVersion, nullptr);
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, destination);
}

void DuskAudioProcessor::setStateInformation(const void* data, int size)
{
    const auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr || ! xml->hasTagName("DuskParameters"))
        return;
    auto state = juce::ValueTree::fromXml(*xml);
    if (static_cast<int>(state.getProperty("schemaVersion", 0))
        != stateSchemaVersion)
        return;
    setSavedParameterState(state);
    parameters.replaceState(state);
    pendingProcessingReset.store(true, std::memory_order_release);
}

juce::AudioProcessorEditor* DuskAudioProcessor::createEditor()
{
    return new DuskAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DuskAudioProcessor();
}
