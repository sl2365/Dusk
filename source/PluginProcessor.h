#pragma once

#include <JuceHeader.h>
#include "DuskArchitectures.h"
#include "DuskSequencer.h"
#include <array>
#include <complex>
#include <optional>
#include <vector>

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
    static std::complex<double> getCombResponse(
        double frequencyHz, double sampleRate, float combFrequencyHz,
        float feedbackPercent, float dampingPercent,
        float fineTuneCents, float polarityPercent,
        float dispersionPercent, int modeChoice) noexcept;
    static std::complex<double> getFormantResponse(
        double frequencyHz, double sampleRate, float vowelPercent,
        float shiftSemitones, float focusPercent,
        float formantOneLevelPercent, float formantTwoLevelPercent,
        float formantThreeLevelPercent, float bandwidthPercent,
        float spreadPercent, float tiltPercent) noexcept;
    static std::complex<double> getPhaserResponse(
        double frequencyHz, double sampleRate, float centreFrequencyHz,
        float feedbackPercent, float depthPercent, float stages,
        float spreadPercent, float skewPercent,
        float dampingPercent) noexcept;
    static std::complex<double> getArchitectureResponse(
        int architectureChoice, double frequencyHz, double sampleRate,
        float cutoffHz, float resonancePercent, float slopeDbPerOctave,
        float resonanceShapePercent, float bassLossPercent,
        float resonanceLimitPercent, float poleSpreadPercent,
        float feedbackPercent, float ladderStageSaturationPercent,
        float ladderResonanceCompensationPercent,
        float otaCellDrivePercent, float otaFeedbackDampingPercent,
        float otaResonanceCompensationPercent,
        float combFrequencyHz, float combFeedbackPercent,
        float combDampingPercent, float combFineTuneCents,
        float combPolarityPercent, float combDispersionPercent,
        int combModeChoice,
        float formantVowelPercent, float formantShiftSemitones,
        float formantFocusPercent, float formantOneLevelPercent,
        float formantTwoLevelPercent, float formantThreeLevelPercent,
        float formantBandwidthPercent, float formantSpreadPercent,
        float formantTiltPercent,
        float phaserFrequencyHz, float phaserFeedbackPercent,
        float phaserDepthPercent, float phaserStages,
        float phaserSpreadPercent, float phaserSkewPercent,
        float phaserDampingPercent,
        int filterTypeChoice) noexcept;

private:
    static constexpr int stateSchemaVersion = 20;
    static constexpr int filterEngineCount = 2;
    static constexpr int channelCount = 2;
    static constexpr int maximumLadderStages = 8;
    static constexpr int maximumOtaStages = 8;
    static constexpr int combModeCount = 3;
    static constexpr int formantBandCount = 3;
    static constexpr int maximumPhaserStages = 12;
    static constexpr int filterStateCount =
        filterEngineCount * channelCount;
    static constexpr int distortionStateBankCount = 11;
    static constexpr int distortionStateCount =
        filterStateCount * distortionStateBankCount;
    static constexpr int distortionTypeCount = 17;
    static constexpr size_t distortionCombBufferSize = 96;

    using LinearSmoother = juce::SmoothedValue<
        float, juce::ValueSmoothingTypes::Linear>;
    using MultiplicativeSmoother = juce::SmoothedValue<
        float, juce::ValueSmoothingTypes::Multiplicative>;

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
        std::atomic<float>* filter1CombMode = nullptr;
        std::atomic<float>* filter2CombMode = nullptr;
        std::atomic<float>* filter1CombFrequency = nullptr;
        std::atomic<float>* filter2CombFrequency = nullptr;
        std::atomic<float>* filter1CombFeedback = nullptr;
        std::atomic<float>* filter2CombFeedback = nullptr;
        std::atomic<float>* filter1CombDamping = nullptr;
        std::atomic<float>* filter2CombDamping = nullptr;
        std::atomic<float>* filter1CombFineTune = nullptr;
        std::atomic<float>* filter2CombFineTune = nullptr;
        std::atomic<float>* filter1CombPolarity = nullptr;
        std::atomic<float>* filter2CombPolarity = nullptr;
        std::atomic<float>* filter1CombStereoSpread = nullptr;
        std::atomic<float>* filter2CombStereoSpread = nullptr;
        std::atomic<float>* filter1CombDispersion = nullptr;
        std::atomic<float>* filter2CombDispersion = nullptr;
        std::atomic<float>* filter1CombLoopSaturation = nullptr;
        std::atomic<float>* filter2CombLoopSaturation = nullptr;
        std::atomic<float>* filter1CombFeedbackLimit = nullptr;
        std::atomic<float>* filter2CombFeedbackLimit = nullptr;
        std::atomic<float>* filter1FormantVowel = nullptr;
        std::atomic<float>* filter2FormantVowel = nullptr;
        std::atomic<float>* filter1FormantShift = nullptr;
        std::atomic<float>* filter2FormantShift = nullptr;
        std::atomic<float>* filter1FormantFocus = nullptr;
        std::atomic<float>* filter2FormantFocus = nullptr;
        std::atomic<float>* filter1FormantOneLevel = nullptr;
        std::atomic<float>* filter2FormantOneLevel = nullptr;
        std::atomic<float>* filter1FormantTwoLevel = nullptr;
        std::atomic<float>* filter2FormantTwoLevel = nullptr;
        std::atomic<float>* filter1FormantThreeLevel = nullptr;
        std::atomic<float>* filter2FormantThreeLevel = nullptr;
        std::atomic<float>* filter1FormantBandwidth = nullptr;
        std::atomic<float>* filter2FormantBandwidth = nullptr;
        std::atomic<float>* filter1FormantSpread = nullptr;
        std::atomic<float>* filter2FormantSpread = nullptr;
        std::atomic<float>* filter1FormantTilt = nullptr;
        std::atomic<float>* filter2FormantTilt = nullptr;
        std::atomic<float>* filter1PhaserFrequency = nullptr;
        std::atomic<float>* filter2PhaserFrequency = nullptr;
        std::atomic<float>* filter1PhaserFeedback = nullptr;
        std::atomic<float>* filter2PhaserFeedback = nullptr;
        std::atomic<float>* filter1PhaserDepth = nullptr;
        std::atomic<float>* filter2PhaserDepth = nullptr;
        std::atomic<float>* filter1PhaserStages = nullptr;
        std::atomic<float>* filter2PhaserStages = nullptr;
        std::atomic<float>* filter1PhaserSpread = nullptr;
        std::atomic<float>* filter2PhaserSpread = nullptr;
        std::atomic<float>* filter1PhaserSkew = nullptr;
        std::atomic<float>* filter2PhaserSkew = nullptr;
        std::atomic<float>* filter1PhaserDamping = nullptr;
        std::atomic<float>* filter2PhaserDamping = nullptr;
        std::atomic<float>* filter1PhaserStageSaturation = nullptr;
        std::atomic<float>* filter2PhaserStageSaturation = nullptr;
        std::atomic<float>* filter1PhaserStereo = nullptr;
        std::atomic<float>* filter2PhaserStereo = nullptr;
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
                                int channel, int distortionType,
                                int stateBank) noexcept;
    float processSelectedDistortion(
        float sample, float amount, int engine, int channel,
        int currentType, int previousType, float typeCrossfade,
        int stateBank) noexcept;
    float processSvfFilter(float sample, int engine, int channel,
                           float filterTypePosition, float cutoffHz,
                           float resonance,
                           float slopeDbPerOctave,
                           float bassLoss,
                           float coreSaturation,
                           float saturationAsymmetry,
                           float poleSpread,
                           float feedback,
                           float feedbackLimit,
                           bool internalDistortion, float distortionAmount,
                           int currentDistortion, int previousDistortion,
                           float distortionCrossfade) noexcept;
    float processLadderFilter(
        float sample, int engine, int channel, float filterTypePosition,
        float cutoffHz, float resonance, float slopeDbPerOctave,
        float stageSaturation, float feedbackSaturation,
        float resonanceCompensation,
        bool internalDistortion, float distortionAmount,
        int currentDistortion, int previousDistortion,
        float distortionCrossfade) noexcept;
    float processOtaFilter(
        float sample, int engine, int channel, float filterTypePosition,
        float cutoffHz, float resonance, float slopeDbPerOctave,
        float cellDrive, float feedbackDamping,
        float resonanceCompensation,
        bool internalDistortion, float distortionAmount,
        int currentDistortion, int previousDistortion,
        float distortionCrossfade) noexcept;
    float processCombFilter(
        float sample, int engine, int channel, int previousMode,
        int currentMode, float modeCrossfade, float frequencyHz,
        float feedback, float damping, float fineTuneCents,
        float polarity, float stereoSpread, float dispersion,
        float loopSaturation, float feedbackLimit,
        bool internalDistortion, float distortionAmount,
        int currentDistortion, int previousDistortion,
        float distortionCrossfade) noexcept;
    float processFormantFilter(
        float sample, int engine, int channel, float vowelPercent,
        float shiftSemitones, float focusPercent,
        float formantOneLevelPercent, float formantTwoLevelPercent,
        float formantThreeLevelPercent, float bandwidthPercent,
        float spreadPercent, float tiltPercent,
        bool internalDistortion, float distortionAmount,
        int currentDistortion, int previousDistortion,
        float distortionCrossfade) noexcept;
    float processPhaserFilter(
        float sample, int engine, int channel, float centreFrequencyHz,
        float feedbackPercent, float depthPercent, float stages,
        float spreadPercent, float skewPercent, float dampingPercent,
        float stageSaturationPercent, float stereoPercent,
        bool internalDistortion, float distortionAmount,
        int currentDistortion, int previousDistortion,
        float distortionCrossfade) noexcept;
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
        float otaResonanceCompensation,
        int previousCombMode, int currentCombMode,
        float combModeCrossfade, float combFrequency,
        float combFeedback, float combDamping, float combFineTune,
        float combPolarity, float combStereoSpread,
        float combDispersion, float combLoopSaturation,
        float combFeedbackLimit,
        float formantVowel, float formantShift, float formantFocus,
        float formantOneLevel, float formantTwoLevel,
        float formantThreeLevel, float formantBandwidth,
        float formantSpread, float formantTilt,
        float phaserFrequency, float phaserFeedback, float phaserDepth,
        float phaserStages, float phaserSpread, float phaserSkew,
        float phaserDamping, float phaserStageSaturation,
        float phaserStereo,
        bool internalDistortion, float distortionAmount,
        int currentDistortion, int previousDistortion,
        float distortionCrossfade) noexcept;
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
    void prepareCombState();
    void resetCombState() noexcept;
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
    std::array<MultiplicativeSmoother, filterEngineCount> combFrequencyValue;
    std::array<LinearSmoother, filterEngineCount> combFeedbackValue;
    std::array<LinearSmoother, filterEngineCount> combDampingValue;
    std::array<LinearSmoother, filterEngineCount> combFineTuneValue;
    std::array<LinearSmoother, filterEngineCount> combPolarityValue;
    std::array<LinearSmoother, filterEngineCount> combStereoSpreadValue;
    std::array<LinearSmoother, filterEngineCount> combDispersionValue;
    std::array<LinearSmoother, filterEngineCount> combLoopSaturationValue;
    std::array<LinearSmoother, filterEngineCount> combFeedbackLimitValue;
    std::array<LinearSmoother, filterEngineCount> formantVowelValue;
    std::array<LinearSmoother, filterEngineCount> formantShiftValue;
    std::array<LinearSmoother, filterEngineCount> formantFocusValue;
    std::array<LinearSmoother, filterEngineCount> formantOneLevelValue;
    std::array<LinearSmoother, filterEngineCount> formantTwoLevelValue;
    std::array<LinearSmoother, filterEngineCount> formantThreeLevelValue;
    std::array<LinearSmoother, filterEngineCount> formantBandwidthValue;
    std::array<LinearSmoother, filterEngineCount> formantSpreadValue;
    std::array<LinearSmoother, filterEngineCount> formantTiltValue;
    std::array<MultiplicativeSmoother, filterEngineCount> phaserFrequencyValue;
    std::array<LinearSmoother, filterEngineCount> phaserFeedbackValue;
    std::array<LinearSmoother, filterEngineCount> phaserDepthValue;
    std::array<LinearSmoother, filterEngineCount> phaserStagesValue;
    std::array<LinearSmoother, filterEngineCount> phaserSpreadValue;
    std::array<LinearSmoother, filterEngineCount> phaserSkewValue;
    std::array<LinearSmoother, filterEngineCount> phaserDampingValue;
    std::array<LinearSmoother, filterEngineCount> phaserStageSaturationValue;
    std::array<LinearSmoother, filterEngineCount> phaserStereoValue;
    std::array<int, filterEngineCount> currentCombMode {};
    std::array<int, filterEngineCount> previousCombMode {};
    std::array<LinearSmoother, filterEngineCount> combModeCrossfadeValue;
    std::array<LinearSmoother, filterEngineCount> slopeValue;
    std::array<LinearSmoother, filterEngineCount> architectureCrossfadeValue;
    std::array<int, filterEngineCount> currentArchitectureChoice {};
    std::array<int, filterEngineCount> previousArchitectureChoice {};
    std::array<LinearSmoother, filterEngineCount> filterEnabledValue;
    LinearSmoother mixValue;
    LinearSmoother outputGainValue;
    LinearSmoother bypassValue;
    std::array<LinearSmoother, filterEngineCount> distortionTypeCrossfade;
    int activeDistortionStateSlot = 0;

    std::array<float, filterStateCount> svfIntegratorOne {};
    std::array<float, filterStateCount> svfIntegratorTwo {};
    std::array<float, filterStateCount> onePoleState {};
    std::array<float, filterStateCount> bassShelfState {};
    std::array<float, filterStateCount> coreAsymmetryDcInput {};
    std::array<float, filterStateCount> coreAsymmetryDcOutput {};
    std::array<float, filterStateCount> filterFeedbackReturn {};
    std::array<std::array<float, filterStateCount>,
               maximumLadderStages>
        ladderStageState {};
    std::array<std::array<float, filterStateCount>, maximumOtaStages>
        otaStageState {};
    std::array<float, filterStateCount> otaFeedbackReturn {};
    std::array<float, filterStateCount> otaFeedbackDampingState {};
    std::array<std::array<float, filterStateCount>, 3>
        slopeIntegratorOne {};
    std::array<std::array<float, filterStateCount>, 3>
        slopeIntegratorTwo {};
    std::array<std::array<std::vector<float>, filterStateCount>,
               combModeCount>
        combDelayBuffer;
    std::array<std::array<size_t, filterStateCount>, combModeCount>
        combWritePosition {};
    std::array<std::array<float, filterStateCount>, combModeCount>
        combDampingState {};
    std::array<std::array<float, filterStateCount>, combModeCount>
        combFractionalState {};
    std::array<std::array<float, filterStateCount>, combModeCount>
        combDispersionState {};
    std::array<std::array<float, filterStateCount>, combModeCount>
        combLowCutState {};
    std::array<std::array<float, filterStateCount>, formantBandCount>
        formantIntegratorOne {};
    std::array<std::array<float, filterStateCount>, formantBandCount>
        formantIntegratorTwo {};
    std::array<std::array<float, filterStateCount>, maximumPhaserStages>
        phaserAllPassState {};
    std::array<float, filterStateCount> phaserFeedbackReturn {};

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
    std::array<uint32_t, distortionStateCount> crackleNoiseState {};
    std::array<uint32_t, distortionStateCount> lightningNoiseState {};
    std::array<uint32_t, distortionStateCount> pixelatedNoiseState {};
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
