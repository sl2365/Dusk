#pragma once

#include <JuceHeader.h>
#include "DuskArchitectures.h"
#include "DuskSequencer.h"
#include <array>
#include <complex>
#include <optional>

class DuskAudioProcessor final : public juce::AudioProcessor
{
public:
    DuskAudioProcessor();
    ~DuskAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    juce::AudioProcessorParameter* getBypassParameter() const override
    {
        return parameters.getParameter("bypass");
    }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout
        createParameterLayout();
    static int distortionAlgorithmForChoice(int choice) noexcept;
    static juce::File getDataDirectory();
    static juce::File getSettingsDirectory();
    static juce::File getPresetsDirectory();
    static juce::File getSequencesDirectory();
    static juce::File getModelsDirectory();
    juce::Result savePresetToFile(const juce::File&);
    juce::Result loadPresetFromFile(const juce::File&);
    juce::File getCurrentPresetFile() const;
    std::optional<float> getSavedParameterValue(
        const juce::String& parameterId) const;
    int getActiveSequencerStep() const noexcept
    {
        return activeSequencerStep.load(std::memory_order_relaxed);
    }
    std::optional<float> getSequencerAnimatedNormalised(
        const juce::String& parameterId) const noexcept;
    static std::complex<double> getCleanSvfResponse(
        double frequencyHz, double sampleRate, float cutoffHz,
        float resonancePercent, float slopeDbPerOctave,
        float resonanceShapePercent, float bassLossPercent,
        float resonanceLimitPercent, float poleSpreadPercent,
        float feedbackPercent, int filterTypeChoice) noexcept;
    static std::complex<double> getLadderResponse(
        double frequencyHz, double sampleRate, float cutoffHz,
        float resonancePercent, float slopeDbPerOctave,
        float resonanceCompensationPercent,
        int filterTypeChoice) noexcept;
    static std::complex<double> getOtaResponse(
        double frequencyHz, double sampleRate, float cutoffHz,
        float resonancePercent, float slopeDbPerOctave,
        float feedbackDampingPercent, float resonanceCompensationPercent,
        int filterTypeChoice) noexcept;
    static std::complex<double> getArchitectureResponse(
        int architectureChoice, double frequencyHz, double sampleRate,
        float cutoffHz, float resonancePercent, float slopeDbPerOctave,
        float resonanceShapePercent, float bassLossPercent,
        float resonanceLimitPercent, float poleSpreadPercent,
        float feedbackPercent, float ladderStageSaturationPercent,
        float ladderResonanceCompensationPercent,
        float otaCellDrivePercent, float otaFeedbackDampingPercent,
        float otaResonanceCompensationPercent,
        int filterTypeChoice) noexcept;

private:
    static constexpr int stateSchemaVersion = 17;
    static constexpr int filterEngineCount = 2;
    static constexpr int channelCount = 2;
    static constexpr int maximumLadderStages = 8;
    static constexpr int maximumOtaStages = 8;
    static constexpr int distortionStateCount =
        filterEngineCount * channelCount;
    static constexpr int distortionTypeCount = 17;
    static constexpr size_t distortionCombBufferSize = 96;

    using LinearSmoother = juce::SmoothedValue<
        float, juce::ValueSmoothingTypes::Linear>;

    struct ParameterPointers
    {
        std::atomic<float>* routing = nullptr;
        std::atomic<float>* filter1Architecture = nullptr;
        std::atomic<float>* filter2Architecture = nullptr;
        std::atomic<float>* filter1Type = nullptr;
        std::atomic<float>* filter2Type = nullptr;
        std::atomic<float>* filter1Cutoff = nullptr;
        std::atomic<float>* filter2Cutoff = nullptr;
        std::atomic<float>* filter1Resonance = nullptr;
        std::atomic<float>* filter2Resonance = nullptr;
        std::atomic<float>* filter1ResonanceShape = nullptr;
        std::atomic<float>* filter2ResonanceShape = nullptr;
        std::atomic<float>* filter1ResonanceLimit = nullptr;
        std::atomic<float>* filter2ResonanceLimit = nullptr;
        std::atomic<float>* filter1BassLoss = nullptr;
        std::atomic<float>* filter2BassLoss = nullptr;
        std::atomic<float>* filter1CoreSaturation = nullptr;
        std::atomic<float>* filter2CoreSaturation = nullptr;
        std::atomic<float>* filter1SaturationAsymmetry = nullptr;
        std::atomic<float>* filter2SaturationAsymmetry = nullptr;
        std::atomic<float>* filter1PoleSpread = nullptr;
        std::atomic<float>* filter2PoleSpread = nullptr;
        std::atomic<float>* filter1Feedback = nullptr;
        std::atomic<float>* filter2Feedback = nullptr;
        std::atomic<float>* filter1FeedbackLimit = nullptr;
        std::atomic<float>* filter2FeedbackLimit = nullptr;
        std::atomic<float>* filter1LadderStageSaturation = nullptr;
        std::atomic<float>* filter2LadderStageSaturation = nullptr;
        std::atomic<float>* filter1LadderFeedbackSaturation = nullptr;
        std::atomic<float>* filter2LadderFeedbackSaturation = nullptr;
        std::atomic<float>* filter1LadderResonanceCompensation = nullptr;
        std::atomic<float>* filter2LadderResonanceCompensation = nullptr;
        std::atomic<float>* filter1OtaCellDrive = nullptr;
        std::atomic<float>* filter2OtaCellDrive = nullptr;
        std::atomic<float>* filter1OtaFeedbackDamping = nullptr;
        std::atomic<float>* filter2OtaFeedbackDamping = nullptr;
        std::atomic<float>* filter1OtaResonanceCompensation = nullptr;
        std::atomic<float>* filter2OtaResonanceCompensation = nullptr;
        std::atomic<float>* filter1Slope = nullptr;
        std::atomic<float>* filter2Slope = nullptr;
        std::atomic<float>* filter1Enabled = nullptr;
        std::atomic<float>* filter2Enabled = nullptr;
        std::atomic<float>* filter1DistortionType = nullptr;
        std::atomic<float>* filter2DistortionType = nullptr;
        std::atomic<float>* filter1Drive = nullptr;
        std::atomic<float>* filter2Drive = nullptr;
        std::atomic<float>* filter1DistortionPlacement = nullptr;
        std::atomic<float>* filter2DistortionPlacement = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* output = nullptr;
        std::atomic<float>* bypass = nullptr;
        std::atomic<float>* sequencerEnabled = nullptr;
        std::atomic<float>* sequencerHostSync = nullptr;
        std::atomic<float>* sequencerRate = nullptr;
        std::atomic<float>* sequencerStart = nullptr;
        std::atomic<float>* sequencerEnd = nullptr;
        std::atomic<float>* sequencerDirection = nullptr;
        std::atomic<float>* sequencerBipolar = nullptr;
        std::atomic<float>* sequencerUnipolarNegative = nullptr;
        std::atomic<float>* sequencerAttack = nullptr;
        std::atomic<float>* sequencerRelease = nullptr;
        std::atomic<float>* sequencerPeak = nullptr;
        std::atomic<float>* sequencerMove = nullptr;
        std::atomic<float>* sequencerGateShortLength = nullptr;
        std::atomic<float>* sequencerGateLongLength = nullptr;
        std::atomic<float>* sequencerGateMode = nullptr;
        std::array<std::atomic<float>*, dusk::sequencer::stepCount>
            sequencerSteps {};
        std::array<std::atomic<float>*, dusk::sequencer::stepCount>
            sequencerGateSteps {};
        std::array<std::atomic<float>*, dusk::sequencer::targetSlotCount>
            sequencerTargets {};
        std::array<std::atomic<float>*, dusk::sequencer::targetSlotCount>
            sequencerTargetEnabled {};
        std::array<std::atomic<float>*, dusk::sequencer::targetSlotCount>
            sequencerTargetDepth {};
    } raw;

    void bindParameterPointers();
    void setSavedParameterState(const juce::ValueTree&);
    static float load(const std::atomic<float>*) noexcept;
    bool isSequencerTargetAssigned(int targetChoice) const noexcept;
    float sequencerTargetDepth(int targetChoice) const noexcept;
    float sequencerModulatedValue(
        int targetChoice, float baseValue, float canonicalSequenceValue,
        bool bipolar, bool unipolarNegative,
        juce::RangedAudioParameter* valueParameter = nullptr) const noexcept;

    static float applyInputDrive(float sample, float amount,
                                 int distortionType) noexcept;
    float processDistortionType(float sample, float amount, int engine,
                                int channel, int distortionType) noexcept;
    float processSvfFilter(float sample, int engine, int channel,
                           float filterTypePosition, float cutoffHz,
                           float resonance,
                           float slopeDbPerOctave,
                           float bassLoss,
                           float coreSaturation,
                           float saturationAsymmetry,
                           float poleSpread,
                           float feedback,
                           float feedbackLimit) noexcept;
    float processLadderFilter(
        float sample, int engine, int channel, float filterTypePosition,
        float cutoffHz, float resonance, float slopeDbPerOctave,
        float stageSaturation, float feedbackSaturation,
        float resonanceCompensation) noexcept;
    float processOtaFilter(
        float sample, int engine, int channel, float filterTypePosition,
        float cutoffHz, float resonance, float slopeDbPerOctave,
        float cellDrive, float feedbackDamping,
        float resonanceCompensation) noexcept;
    float processFilterArchitecture(
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
        float otaResonanceCompensation) noexcept;
    float processRectifyDrive(float sample, float amount, int channel) noexcept;
    float processCrackleDrive(float sample, float amount, int channel) noexcept;
    float processLightningDrive(float sample, float amount, int channel) noexcept;
    float processAmpDrive(float sample, float amount, int channel) noexcept;
    float processBumpyDrive(float sample, float amount, int channel) noexcept;
    float processAsymmetricalDrive(float sample, float amount,
                                   int channel) noexcept;
    float processRazorDrive(float sample, float amount, int channel) noexcept;
    float processPixelatedDrive(float sample, float amount,
                                int channel) noexcept;
    float processCombDrive(float sample, float amount, int channel) noexcept;
    float processStressedDrive(float sample, float amount,
                               int channel) noexcept;
    float processSamuraiDrive(float sample, float amount, int channel) noexcept;
    float processFreshDrive(float sample, float amount, int channel) noexcept;
    static float processHardClipDrive(float sample, float amount) noexcept;
    float processFuzzDrive(float sample, float amount, int channel) noexcept;
    size_t distortionStateIndex(int channel) const noexcept;
    void resetDistortionState() noexcept;
    void resetFilterState() noexcept;
    void resetProcessingState() noexcept;
    static float nextNoiseSample(uint32_t& state) noexcept;
    static float sanitiseSample(float sample, float limit) noexcept;

    std::atomic<int> activeSequencerStep { -1 };
    double sequencerFreePhase = 0.0;
    int sequencerHeldNotes = 0;
    float previousSequencerCanonical = 1.0f;
    float sequencerGateGain = 1.0f;
    std::array<juce::RangedAudioParameter*, dusk::sequencer::targets.size()>
        sequencerTargetParameters {};
    mutable std::array<std::atomic<float>, dusk::sequencer::targets.size()>
        sequencerAnimatedNormalised {};

    double currentSampleRate = 44100.0;
    std::array<int, filterEngineCount> currentDistortionType {};
    std::array<int, filterEngineCount> previousDistortionType {};
    std::array<LinearSmoother, filterEngineCount> driveValue;
    std::array<LinearSmoother, filterEngineCount> filterTypeValue;
    std::array<LinearSmoother, filterEngineCount> cutoffValue;
    std::array<LinearSmoother, filterEngineCount> resonanceValue;
    std::array<LinearSmoother, filterEngineCount> resonanceShapeValue;
    std::array<LinearSmoother, filterEngineCount> resonanceLimitValue;
    std::array<LinearSmoother, filterEngineCount> bassLossValue;
    std::array<LinearSmoother, filterEngineCount> coreSaturationValue;
    std::array<LinearSmoother, filterEngineCount> saturationAsymmetryValue;
    std::array<LinearSmoother, filterEngineCount> poleSpreadValue;
    std::array<LinearSmoother, filterEngineCount> feedbackValue;
    std::array<LinearSmoother, filterEngineCount> feedbackLimitValue;
    std::array<LinearSmoother, filterEngineCount>
        ladderStageSaturationValue;
    std::array<LinearSmoother, filterEngineCount>
        ladderFeedbackSaturationValue;
    std::array<LinearSmoother, filterEngineCount>
        ladderResonanceCompensationValue;
    std::array<LinearSmoother, filterEngineCount> otaCellDriveValue;
    std::array<LinearSmoother, filterEngineCount> otaFeedbackDampingValue;
    std::array<LinearSmoother, filterEngineCount>
        otaResonanceCompensationValue;
    std::array<LinearSmoother, filterEngineCount> slopeValue;
    std::array<LinearSmoother, filterEngineCount> architectureCrossfadeValue;
    std::array<int, filterEngineCount> currentArchitectureChoice {};
    std::array<int, filterEngineCount> previousArchitectureChoice {};
    std::array<LinearSmoother, filterEngineCount> filterEnabledValue;
    LinearSmoother mixValue;
    LinearSmoother outputGainValue;
    LinearSmoother bypassValue;
    std::array<LinearSmoother, filterEngineCount> distortionTypeCrossfade;
    int activeDistortionEngine = 0;

    std::array<float, distortionStateCount> svfIntegratorOne {};
    std::array<float, distortionStateCount> svfIntegratorTwo {};
    std::array<float, distortionStateCount> onePoleState {};
    std::array<float, distortionStateCount> bassShelfState {};
    std::array<float, distortionStateCount> coreAsymmetryDcInput {};
    std::array<float, distortionStateCount> coreAsymmetryDcOutput {};
    std::array<float, distortionStateCount> filterFeedbackReturn {};
    std::array<std::array<float, distortionStateCount>,
               maximumLadderStages>
        ladderStageState {};
    std::array<std::array<float, distortionStateCount>, maximumOtaStages>
        otaStageState {};
    std::array<float, distortionStateCount> otaFeedbackReturn {};
    std::array<float, distortionStateCount> otaFeedbackDampingState {};
    std::array<std::array<float, distortionStateCount>, 3>
        slopeIntegratorOne {};
    std::array<std::array<float, distortionStateCount>, 3>
        slopeIntegratorTwo {};

    std::array<float, distortionStateCount> rectifyPreviousInput {};
    std::array<float, distortionStateCount> rectifyPreviousOutput {};
    std::array<float, distortionStateCount> crackleBurst {};
    std::array<float, distortionStateCount> cracklePreviousInput {};
    std::array<float, distortionStateCount> cracklePreviousOutput {};
    std::array<float, distortionStateCount> lightningPreviousInput {};
    std::array<float, distortionStateCount> lightningPreviousOutput {};
    std::array<float, distortionStateCount> ampLowpassState {};
    std::array<float, distortionStateCount> ampPreviousInput {};
    std::array<float, distortionStateCount> ampPreviousOutput {};
    std::array<float, distortionStateCount> bumpyToneState {};
    std::array<float, distortionStateCount> bumpyPreviousInput {};
    std::array<float, distortionStateCount> bumpyPreviousOutput {};
    std::array<float, distortionStateCount> asymmetricalPreviousInput {};
    std::array<float, distortionStateCount> asymmetricalPreviousOutput {};
    std::array<float, distortionStateCount> razorToneState {};
    std::array<float, distortionStateCount> razorPreviousInput {};
    std::array<float, distortionStateCount> razorPreviousOutput {};
    std::array<float, distortionStateCount> pixelatedHeldSample {};
    std::array<float, distortionStateCount> pixelatedPhase {};
    std::array<std::array<float, distortionCombBufferSize>,
               distortionStateCount>
        distortionCombBuffer {};
    std::array<size_t, distortionStateCount> distortionCombWritePosition {};
    std::array<float, distortionStateCount> distortionCombDampingState {};
    std::array<float, distortionStateCount> stressedPreviousInput {};
    std::array<float, distortionStateCount> stressedPreviousOutput {};
    std::array<float, distortionStateCount> samuraiToneState {};
    std::array<float, distortionStateCount> samuraiPreviousInput {};
    std::array<float, distortionStateCount> samuraiPreviousOutput {};
    std::array<float, distortionStateCount> freshToneState {};
    std::array<float, distortionStateCount> fuzzToneState {};
    std::array<float, distortionStateCount> fuzzPreviousInput {};
    std::array<float, distortionStateCount> fuzzPreviousOutput {};
    std::array<uint32_t, distortionStateCount> crackleNoiseState
        { 0xa511e9b3u, 0x63d83595u, 0x9e3779b9u, 0x7f4a7c15u };
    std::array<uint32_t, distortionStateCount> lightningNoiseState
        { 0x27d4eb2du, 0x165667b1u, 0x85ebca6bu, 0xc2b2ae35u };
    std::array<uint32_t, distortionStateCount> pixelatedNoiseState
        { 0x94d049bbu, 0x369dea0fu, 0xd1b54a35u, 0x8cb92ba7u };
    float rectifyDcCoefficient = 0.0f;
    float crackleDcCoefficient = 0.0f;
    float lightningDcCoefficient = 0.0f;
    float ampDcCoefficient = 0.0f;
    float ampToneCoefficient = 0.0f;
    float bumpyDcCoefficient = 0.0f;
    float bumpyToneCoefficient = 0.0f;
    float asymmetricalDcCoefficient = 0.0f;
    float razorToneCoefficient = 0.0f;
    float razorDcCoefficient = 0.0f;
    float distortionCombDampingCoefficient = 0.0f;
    float stressedDcCoefficient = 0.0f;
    float samuraiToneCoefficient = 0.0f;
    float samuraiDcCoefficient = 0.0f;
    float freshToneCoefficient = 0.0f;
    float fuzzDcCoefficient = 0.0f;
    float fuzzToneCoefficient = 0.0f;
    float coreAsymmetryDcCoefficient = 0.0f;

    mutable juce::CriticalSection savedParameterStateLock;
    juce::ValueTree savedParameterState;
    std::atomic<bool> pendingProcessingReset { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuskAudioProcessor)
};
