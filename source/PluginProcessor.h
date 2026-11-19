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
    double getTailLengthSeconds() const override { return 10.0; }
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
        float feedbackPercent, float gainDb,
        int filterTypeChoice) noexcept;
    static std::complex<double> getLadderResponse(
        double frequencyHz, double sampleRate, float cutoffHz,
        float resonancePercent, float slopeDbPerOctave,
        float resonanceCompensationPercent, float gainDb,
        int filterTypeChoice) noexcept;
    static std::complex<double> getOtaResponse(
        double frequencyHz, double sampleRate, float cutoffHz,
        float resonancePercent, float slopeDbPerOctave,
        float feedbackDampingPercent, float resonanceCompensationPercent,
        float gainDb,
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
    static std::complex<double> getModalResponse(
        double frequencyHz, double sampleRate, float baseFrequencyHz,
        float decayMilliseconds, float bodyPercent, float modes,
        float ratioPercent, float inharmonicityPercent,
        float decayTiltPercent, float dampingPercent,
        float excitationPercent) noexcept;
    static std::complex<double> getSallenKeyResponse(
        double frequencyHz, double sampleRate, float cutoffHz,
        float resonancePercent, float slopeDbPerOctave,
        float qCurvePercent, float cellGainPercent,
        float stageCouplingPercent, float componentVariationPercent,
        float highFrequencyDampingPercent,
        float gainDb,
        int filterTypeChoice) noexcept;
    static std::complex<double> getDiodeLadderResponse(
        double frequencyHz, double sampleRate, float cutoffHz,
        float resonancePercent, float slopeDbPerOctave,
        float stageTiltPercent, float resonanceLossPercent,
        float resonanceCompensationPercent,
        float gainDb,
        int filterTypeChoice) noexcept;
    static std::complex<double> getSteinerParkerResponse(
        double frequencyHz, double sampleRate, float cutoffHz,
        float resonancePercent, float slopeDbPerOctave,
        float poleSpreadPercent, float resonanceCurvePercent,
        float resonanceCompensationPercent, float gainDb,
        int filterTypeChoice) noexcept;
    static std::complex<double> getVactrolLpgResponse(
        double frequencyHz, double sampleRate, float cutoffHz,
        float resonancePercent, float slopeDbPerOctave,
        float levelCouplingPercent, float bleedPercent, float gainDb,
        int filterTypeChoice) noexcept;
    static std::complex<double> getKorg35Response(
        double frequencyHz, double sampleRate, float cutoffHz,
        float resonancePercent, float slopeDbPerOctave,
        float feedbackTonePercent, float bassLossPercent,
        float resonanceCompensationPercent, float gainDb,
        int filterTypeChoice) noexcept;
    static std::complex<double> getSpectralBankResponse(
        double frequencyHz, double sampleRate, float centreHz,
        float spanOctaves, float depthPercent, float bands,
        float widthPercent, float patternPercent, float shiftPercent,
        float contrastPercent, float tiltPercent) noexcept;
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
        float modalFrequencyHz, float modalDecayMilliseconds,
        float modalBodyPercent, float modalModes,
        float modalRatioPercent, float modalInharmonicityPercent,
        float modalDecayTiltPercent, float modalDampingPercent,
        float modalExcitationPercent,
        float sallenKeyQCurvePercent, float sallenKeyCellGainPercent,
        float sallenKeyStageCouplingPercent,
        float sallenKeyComponentVariationPercent,
        float sallenKeyHighFrequencyDampingPercent,
        float diodeLadderStageTiltPercent,
        float diodeLadderResonanceLossPercent,
        float diodeLadderResonanceCompensationPercent,
        float steinerPoleSpreadPercent,
        float steinerResonanceCurvePercent,
        float steinerResonanceCompensationPercent,
        float vactrolRisePercent, float vactrolFallPercent,
        float vactrolMemoryPercent, float vactrolLevelCouplingPercent,
        float vactrolWarmthPercent, float vactrolBleedPercent,
        float korg35ChipDrivePercent, float korg35FeedbackClipPercent,
        float korg35BiasPercent, float korg35FeedbackTonePercent,
        float korg35BassLossPercent,
        float korg35ResonanceCompensationPercent,
        float spectralCentreHz, float spectralSpanOctaves,
        float spectralDepthPercent, float spectralBands,
        float spectralWidthPercent, float spectralPatternPercent,
        float spectralShiftPercent, float spectralContrastPercent,
        float spectralTiltPercent,
        float filterGainDb,
        int filterTypeChoice) noexcept;

private:
    static constexpr int stateSchemaVersion = 27;
    static constexpr int filterEngineCount = 2;
    static constexpr int channelCount = 2;
    static constexpr int maximumLadderStages = 8;
    static constexpr int maximumOtaStages = 8;
    static constexpr int combModeCount = 3;
    static constexpr int formantBandCount = 3;
    static constexpr int maximumPhaserStages = 12;
    static constexpr int modalModeCount = 8;
    static constexpr int maximumSallenKeyCells = 4;
    static constexpr int maximumDiodeLadderStages = 8;
    static constexpr int maximumSteinerCells = 4;
    static constexpr int maximumVactrolCells = 4;
    static constexpr int maximumKorg35Cells = 4;
    static constexpr int maximumSpectralBands = 8;
    static constexpr int filterStateCount =
        filterEngineCount * channelCount;
    static constexpr int distortionStateBankCount = 25;
    static constexpr int distortionStateCount =
        filterStateCount * distortionStateBankCount;
    static constexpr int distortionTypeCount = 17;
    static constexpr size_t distortionCombBufferSize = 96;

    using LinearSmoother = juce::SmoothedValue<
        float, juce::ValueSmoothingTypes::Linear>;
    using MultiplicativeSmoother = juce::SmoothedValue<
        float, juce::ValueSmoothingTypes::Multiplicative>;

    struct DiodeLadderCoefficientCache
    {
        std::array<float, 12> parameters {};
        std::array<float, maximumDiodeLadderStages> coefficients {};
        float gainMinusOne = 0.0f;
        float lowTiltMinusOne = 0.0f;
        float highTiltMinusOne = 0.0f;
        float saturationShape = 0.0f;
        float positiveThreshold = 1.0f;
        float negativeThreshold = 1.0f;
        float feedbackClip = 0.0f;
        float clipShape = 0.0f;
        float clipPositiveThreshold = 1.0f;
        float clipNegativeThreshold = 1.0f;
        float inputGain = 1.0f;
        float feedbackGain = 0.0f;
        float feedbackFeedthrough = 0.0f;
        float type = 0.0f;
        float typeFraction = 0.0f;
        float poleFraction = 0.0f;
        float lowerBandScale = 1.0f;
        float upperBandScale = 1.0f;
        int typeLeft = 0;
        int typeRight = 0;
        int lowerPoles = 1;
        int upperPoles = 1;
        bool valid = false;
    };

    struct Korg35CoefficientCache
    {
        std::array<float, 12> parameters {};
        float gainMinusOne = 0.0f;
        float lowTiltMinusOne = 0.0f;
        float highTiltMinusOne = 0.0f;
        float feedbackGain = 0.0f;
        float drive = 0.0f;
        float feedbackClip = 0.0f;
        float feedbackTone = 0.0f;
        float coefficient = 0.0f;
        float toneCoefficient = 0.0f;
        float bassCoefficient = 0.0f;
        float driveGain = 1.0f;
        float driveBias = 0.0f;
        float driveOffset = 0.0f;
        float clipThreshold = 1.0f;
        float feedbackBias = 0.0f;
        float feedbackOffset = 0.0f;
        float bassLossAmount = 0.0f;
        float outputCompensation = 1.0f;
        float dcCoefficient = 0.0f;
        float type = 0.0f;
        float typeFraction = 0.0f;
        float slope = 12.0f;
        float lowSlopeFraction = 1.0f;
        float cellFraction = 0.0f;
        int typeLeft = 0;
        int typeRight = 0;
        int activeCells = 2;
        int lowerCells = 1;
        int upperCells = 2;
        bool equaliserType = false;
        bool valid = false;
    };

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
        std::atomic<float>* filter1ModalFrequency = nullptr;
        std::atomic<float>* filter2ModalFrequency = nullptr;
        std::atomic<float>* filter1ModalDecay = nullptr;
        std::atomic<float>* filter2ModalDecay = nullptr;
        std::atomic<float>* filter1ModalBody = nullptr;
        std::atomic<float>* filter2ModalBody = nullptr;
        std::atomic<float>* filter1ModalModes = nullptr;
        std::atomic<float>* filter2ModalModes = nullptr;
        std::atomic<float>* filter1ModalRatio = nullptr;
        std::atomic<float>* filter2ModalRatio = nullptr;
        std::atomic<float>* filter1ModalInharmonicity = nullptr;
        std::atomic<float>* filter2ModalInharmonicity = nullptr;
        std::atomic<float>* filter1ModalDecayTilt = nullptr;
        std::atomic<float>* filter2ModalDecayTilt = nullptr;
        std::atomic<float>* filter1ModalDamping = nullptr;
        std::atomic<float>* filter2ModalDamping = nullptr;
        std::atomic<float>* filter1ModalExcitation = nullptr;
        std::atomic<float>* filter2ModalExcitation = nullptr;
        std::atomic<float>* filter1SallenKeyQCurve = nullptr;
        std::atomic<float>* filter2SallenKeyQCurve = nullptr;
        std::atomic<float>* filter1SallenKeyCellGain = nullptr;
        std::atomic<float>* filter2SallenKeyCellGain = nullptr;
        std::atomic<float>* filter1SallenKeyStageCoupling = nullptr;
        std::atomic<float>* filter2SallenKeyStageCoupling = nullptr;
        std::atomic<float>* filter1SallenKeyComponentVariation = nullptr;
        std::atomic<float>* filter2SallenKeyComponentVariation = nullptr;
        std::atomic<float>* filter1SallenKeyOpAmpSaturation = nullptr;
        std::atomic<float>* filter2SallenKeyOpAmpSaturation = nullptr;
        std::atomic<float>* filter1SallenKeyHighFrequencyDamping = nullptr;
        std::atomic<float>* filter2SallenKeyHighFrequencyDamping = nullptr;
        std::atomic<float>* filter1DiodeLadderSaturation = nullptr;
        std::atomic<float>* filter2DiodeLadderSaturation = nullptr;
        std::atomic<float>* filter1DiodeLadderFeedbackClip = nullptr;
        std::atomic<float>* filter2DiodeLadderFeedbackClip = nullptr;
        std::atomic<float>* filter1DiodeLadderBias = nullptr;
        std::atomic<float>* filter2DiodeLadderBias = nullptr;
        std::atomic<float>* filter1DiodeLadderStageTilt = nullptr;
        std::atomic<float>* filter2DiodeLadderStageTilt = nullptr;
        std::atomic<float>* filter1DiodeLadderResonanceLoss = nullptr;
        std::atomic<float>* filter2DiodeLadderResonanceLoss = nullptr;
        std::atomic<float>* filter1DiodeLadderResonanceCompensation = nullptr;
        std::atomic<float>* filter2DiodeLadderResonanceCompensation = nullptr;
        std::atomic<float>* filter1Gain = nullptr;
        std::atomic<float>* filter2Gain = nullptr;
        std::atomic<float>* filter1SteinerDiodeDrive = nullptr;
        std::atomic<float>* filter2SteinerDiodeDrive = nullptr;
        std::atomic<float>* filter1SteinerFeedbackClip = nullptr;
        std::atomic<float>* filter2SteinerFeedbackClip = nullptr;
        std::atomic<float>* filter1SteinerBias = nullptr;
        std::atomic<float>* filter2SteinerBias = nullptr;
        std::atomic<float>* filter1SteinerPoleSpread = nullptr;
        std::atomic<float>* filter2SteinerPoleSpread = nullptr;
        std::atomic<float>* filter1SteinerResonanceCurve = nullptr;
        std::atomic<float>* filter2SteinerResonanceCurve = nullptr;
        std::atomic<float>* filter1SteinerResonanceCompensation = nullptr;
        std::atomic<float>* filter2SteinerResonanceCompensation = nullptr;
        std::atomic<float>* filter1VactrolRise = nullptr;
        std::atomic<float>* filter2VactrolRise = nullptr;
        std::atomic<float>* filter1VactrolFall = nullptr;
        std::atomic<float>* filter2VactrolFall = nullptr;
        std::atomic<float>* filter1VactrolMemory = nullptr;
        std::atomic<float>* filter2VactrolMemory = nullptr;
        std::atomic<float>* filter1VactrolLevelCoupling = nullptr;
        std::atomic<float>* filter2VactrolLevelCoupling = nullptr;
        std::atomic<float>* filter1VactrolWarmth = nullptr;
        std::atomic<float>* filter2VactrolWarmth = nullptr;
        std::atomic<float>* filter1VactrolBleed = nullptr;
        std::atomic<float>* filter2VactrolBleed = nullptr;
        std::atomic<float>* filter1Korg35ChipDrive = nullptr;
        std::atomic<float>* filter2Korg35ChipDrive = nullptr;
        std::atomic<float>* filter1Korg35FeedbackClip = nullptr;
        std::atomic<float>* filter2Korg35FeedbackClip = nullptr;
        std::atomic<float>* filter1Korg35Bias = nullptr;
        std::atomic<float>* filter2Korg35Bias = nullptr;
        std::atomic<float>* filter1Korg35FeedbackTone = nullptr;
        std::atomic<float>* filter2Korg35FeedbackTone = nullptr;
        std::atomic<float>* filter1Korg35BassLoss = nullptr;
        std::atomic<float>* filter2Korg35BassLoss = nullptr;
        std::atomic<float>* filter1Korg35ResonanceCompensation = nullptr;
        std::atomic<float>* filter2Korg35ResonanceCompensation = nullptr;
        std::atomic<float>* filter1SpectralCentre = nullptr;
        std::atomic<float>* filter2SpectralCentre = nullptr;
        std::atomic<float>* filter1SpectralSpan = nullptr;
        std::atomic<float>* filter2SpectralSpan = nullptr;
        std::atomic<float>* filter1SpectralDepth = nullptr;
        std::atomic<float>* filter2SpectralDepth = nullptr;
        std::atomic<float>* filter1SpectralBands = nullptr;
        std::atomic<float>* filter2SpectralBands = nullptr;
        std::atomic<float>* filter1SpectralWidth = nullptr;
        std::atomic<float>* filter2SpectralWidth = nullptr;
        std::atomic<float>* filter1SpectralPattern = nullptr;
        std::atomic<float>* filter2SpectralPattern = nullptr;
        std::atomic<float>* filter1SpectralShift = nullptr;
        std::atomic<float>* filter2SpectralShift = nullptr;
        std::atomic<float>* filter1SpectralContrast = nullptr;
        std::atomic<float>* filter2SpectralContrast = nullptr;
        std::atomic<float>* filter1SpectralTilt = nullptr;
        std::atomic<float>* filter2SpectralTilt = nullptr;
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
    float sequencerModulatedValue(
        int targetChoice, float baseValue, float canonicalSequenceValue,
        bool bipolar, bool unipolarNegative, float targetDepth) const noexcept;

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
                           float gainDb,
                           bool internalDistortion, float distortionAmount,
                           int currentDistortion, int previousDistortion,
                           float distortionCrossfade) noexcept;
    float processLadderFilter(
        float sample, int engine, int channel, float filterTypePosition,
        float cutoffHz, float resonance, float slopeDbPerOctave,
        float stageSaturation, float feedbackSaturation,
        float resonanceCompensation, float gainDb,
        bool internalDistortion, float distortionAmount,
        int currentDistortion, int previousDistortion,
        float distortionCrossfade) noexcept;
    float processOtaFilter(
        float sample, int engine, int channel, float filterTypePosition,
        float cutoffHz, float resonance, float slopeDbPerOctave,
        float cellDrive, float feedbackDamping,
        float resonanceCompensation, float gainDb,
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
    float processModalFilter(
        float sample, int engine, int channel, float baseFrequencyHz,
        float decayMilliseconds, float bodyPercent, float modes,
        float ratioPercent, float inharmonicityPercent,
        float decayTiltPercent, float dampingPercent,
        float excitationPercent,
        bool internalDistortion, float distortionAmount,
        int currentDistortion, int previousDistortion,
        float distortionCrossfade) noexcept;
    float processSallenKeyFilter(
        float sample, int engine, int channel, float filterTypePosition,
        float cutoffHz, float resonance, float slopeDbPerOctave,
        float qCurvePercent, float cellGainPercent,
        float stageCouplingPercent, float componentVariationPercent,
        float opAmpSaturationPercent, float highFrequencyDampingPercent,
        float gainDb,
        bool internalDistortion, float distortionAmount,
        int currentDistortion, int previousDistortion,
        float distortionCrossfade) noexcept;
    void updateDiodeLadderCoefficients(size_t engine) noexcept;
    float processDiodeLadderFilter(
        float sample, int engine, int channel, bool internalDistortion,
        float distortionAmount, int currentDistortion, int previousDistortion,
        float distortionCrossfade) noexcept;
    float processSteinerParkerFilter(
        float sample, int engine, int channel, float filterTypePosition,
        float cutoffHz, float resonance, float slopeDbPerOctave,
        float diodeDrivePercent, float feedbackClipPercent,
        float biasPercent, float poleSpreadPercent,
        float resonanceCurvePercent, float resonanceCompensationPercent,
        float gainDb,
        bool internalDistortion, float distortionAmount,
        int currentDistortion, int previousDistortion,
        float distortionCrossfade) noexcept;
    float processVactrolLpgFilter(
        float sample, int engine, int channel, float filterTypePosition,
        float cutoffHz, float resonance, float slopeDbPerOctave,
        float risePercent, float fallPercent, float memoryPercent,
        float levelCouplingPercent, float warmthPercent,
        float bleedPercent, float gainDb,
        bool internalDistortion, float distortionAmount,
        int currentDistortion, int previousDistortion,
        float distortionCrossfade) noexcept;
    void updateKorg35Coefficients(size_t engine) noexcept;
    float processKorg35Filter(
        float sample, int engine, int channel, bool internalDistortion,
        float distortionAmount, int currentDistortion, int previousDistortion,
        float distortionCrossfade) noexcept;
    float processSpectralBankFilter(
        float sample, int engine, int channel, float centreHz,
        float spanOctaves, float depthPercent, float bands,
        float widthPercent, float patternPercent, float shiftPercent,
        float contrastPercent, float tiltPercent,
        bool internalDistortion, float distortionAmount,
        int currentDistortion, int previousDistortion,
        float distortionCrossfade) noexcept;
    float processFilterArchitecture(
        float sample, int engine, int channel, bool internalDistortion) noexcept;
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
    std::array<MultiplicativeSmoother, filterEngineCount> modalFrequencyValue;
    std::array<LinearSmoother, filterEngineCount> modalDecayValue;
    std::array<LinearSmoother, filterEngineCount> modalBodyValue;
    std::array<LinearSmoother, filterEngineCount> modalModesValue;
    std::array<LinearSmoother, filterEngineCount> modalRatioValue;
    std::array<LinearSmoother, filterEngineCount> modalInharmonicityValue;
    std::array<LinearSmoother, filterEngineCount> modalDecayTiltValue;
    std::array<LinearSmoother, filterEngineCount> modalDampingValue;
    std::array<LinearSmoother, filterEngineCount> modalExcitationValue;
    std::array<LinearSmoother, filterEngineCount> sallenKeyQCurveValue;
    std::array<LinearSmoother, filterEngineCount> sallenKeyCellGainValue;
    std::array<LinearSmoother, filterEngineCount>
        sallenKeyStageCouplingValue;
    std::array<LinearSmoother, filterEngineCount>
        sallenKeyComponentVariationValue;
    std::array<LinearSmoother, filterEngineCount>
        sallenKeyOpAmpSaturationValue;
    std::array<LinearSmoother, filterEngineCount>
        sallenKeyHighFrequencyDampingValue;
    std::array<LinearSmoother, filterEngineCount>
        diodeLadderSaturationValue;
    std::array<LinearSmoother, filterEngineCount>
        diodeLadderFeedbackClipValue;
    std::array<LinearSmoother, filterEngineCount> diodeLadderBiasValue;
    std::array<LinearSmoother, filterEngineCount> diodeLadderStageTiltValue;
    std::array<LinearSmoother, filterEngineCount>
        diodeLadderResonanceLossValue;
    std::array<LinearSmoother, filterEngineCount>
        diodeLadderResonanceCompensationValue;
    std::array<LinearSmoother, filterEngineCount> filterGainValue;
    std::array<LinearSmoother, filterEngineCount> steinerDiodeDriveValue;
    std::array<LinearSmoother, filterEngineCount> steinerFeedbackClipValue;
    std::array<LinearSmoother, filterEngineCount> steinerBiasValue;
    std::array<LinearSmoother, filterEngineCount> steinerPoleSpreadValue;
    std::array<LinearSmoother, filterEngineCount> steinerResonanceCurveValue;
    std::array<LinearSmoother, filterEngineCount>
        steinerResonanceCompensationValue;
    std::array<LinearSmoother, filterEngineCount> vactrolRiseValue;
    std::array<LinearSmoother, filterEngineCount> vactrolFallValue;
    std::array<LinearSmoother, filterEngineCount> vactrolMemoryValue;
    std::array<LinearSmoother, filterEngineCount>
        vactrolLevelCouplingValue;
    std::array<LinearSmoother, filterEngineCount> vactrolWarmthValue;
    std::array<LinearSmoother, filterEngineCount> vactrolBleedValue;
    std::array<LinearSmoother, filterEngineCount> korg35ChipDriveValue;
    std::array<LinearSmoother, filterEngineCount> korg35FeedbackClipValue;
    std::array<LinearSmoother, filterEngineCount> korg35BiasValue;
    std::array<LinearSmoother, filterEngineCount> korg35FeedbackToneValue;
    std::array<LinearSmoother, filterEngineCount> korg35BassLossValue;
    std::array<LinearSmoother, filterEngineCount>
        korg35ResonanceCompensationValue;
    std::array<MultiplicativeSmoother, filterEngineCount>
        spectralCentreValue;
    std::array<LinearSmoother, filterEngineCount> spectralSpanValue;
    std::array<LinearSmoother, filterEngineCount> spectralDepthValue;
    std::array<LinearSmoother, filterEngineCount> spectralBandsValue;
    std::array<LinearSmoother, filterEngineCount> spectralWidthValue;
    std::array<LinearSmoother, filterEngineCount> spectralPatternValue;
    std::array<LinearSmoother, filterEngineCount> spectralShiftValue;
    std::array<LinearSmoother, filterEngineCount> spectralContrastValue;
    std::array<LinearSmoother, filterEngineCount> spectralTiltValue;
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
    std::array<std::array<float, 10>, filterEngineCount>
        formantCoefficientParameters {};
    std::array<std::array<float, formantBandCount>, filterEngineCount>
        formantCoefficientA1 {};
    std::array<std::array<float, formantBandCount>, filterEngineCount>
        formantCoefficientA2 {};
    std::array<std::array<float, formantBandCount>, filterEngineCount>
        formantCoefficientA3 {};
    std::array<std::array<float, formantBandCount>, filterEngineCount>
        formantCoefficientK {};
    std::array<std::array<float, formantBandCount>, filterEngineCount>
        formantCoefficientGain {};
    std::array<bool, filterEngineCount> formantCoefficientsValid {};
    std::array<std::array<float, filterStateCount>, maximumPhaserStages>
        phaserAllPassState {};
    std::array<float, filterStateCount> phaserFeedbackReturn {};
    std::array<std::array<float, 9>, filterStateCount>
        phaserCoefficientParameters {};
    std::array<std::array<float, maximumPhaserStages>, filterStateCount>
        phaserCoefficient {};
    std::array<float, filterStateCount> phaserCachedFeedback {};
    std::array<float, filterStateCount> phaserCachedSaturation {};
    std::array<float, filterStateCount> phaserCachedSaturationDrive {};
    std::array<float, filterStateCount> phaserCachedSaturationScale {};
    std::array<float, filterStateCount> phaserCachedLoopNormalisation {};
    std::array<float, filterStateCount> phaserCachedDampingPole {};
    std::array<bool, filterStateCount> phaserCoefficientsValid {};
    std::array<std::array<float, filterStateCount>, modalModeCount>
        modalOutputOne {};
    std::array<std::array<float, filterStateCount>, modalModeCount>
        modalOutputTwo {};
    std::array<float, filterStateCount> modalInputOne {};
    std::array<float, filterStateCount> modalInputTwo {};
    std::array<std::array<double, 10>, filterEngineCount>
        modalCoefficientParameters {};
    std::array<std::array<double, modalModeCount>, filterEngineCount>
        modalCoefficientOne {};
    std::array<std::array<double, modalModeCount>, filterEngineCount>
        modalCoefficientTwo {};
    std::array<std::array<double, modalModeCount>, filterEngineCount>
        modalNumeratorGain {};
    std::array<std::array<double, modalModeCount>, filterEngineCount>
        modalWeight {};
    std::array<double, filterEngineCount> modalBodyCache {};
    std::array<bool, filterEngineCount> modalCoefficientsValid {};
    std::array<std::array<float, filterStateCount>, maximumSallenKeyCells>
        sallenKeyIntegratorOne {};
    std::array<std::array<float, filterStateCount>, maximumSallenKeyCells>
        sallenKeyIntegratorTwo {};
    std::array<float, filterStateCount> sallenKeyOnePoleState {};
    std::array<std::array<float, filterStateCount>,
               maximumDiodeLadderStages>
        diodeLadderStageState {};
    std::array<DiodeLadderCoefficientCache, filterEngineCount>
        diodeLadderCoefficientCache {};
    std::array<float, filterStateCount> diodeLadderDcInput {};
    std::array<float, filterStateCount> diodeLadderDcOutput {};
    std::array<std::array<float, filterStateCount>, maximumSteinerCells>
        steinerIntegratorOne {};
    std::array<std::array<float, filterStateCount>, maximumSteinerCells>
        steinerIntegratorTwo {};
    std::array<std::array<float, filterStateCount>, maximumSteinerCells>
        steinerFeedbackReturn {};
    std::array<float, filterStateCount> steinerOnePoleState {};
    std::array<float, filterStateCount> steinerDcInput {};
    std::array<float, filterStateCount> steinerDcOutput {};
    std::array<std::array<float, filterStateCount>, maximumVactrolCells>
        vactrolIntegratorOne {};
    std::array<std::array<float, filterStateCount>, maximumVactrolCells>
        vactrolIntegratorTwo {};
    std::array<float, filterStateCount> vactrolOnePoleState {};
    std::array<float, filterStateCount> vactrolCutoffState {};
    std::array<float, filterStateCount> vactrolEnvelopeState {};
    std::array<std::array<float, filterStateCount>, maximumKorg35Cells>
        korg35StageOneState {};
    std::array<std::array<float, filterStateCount>, maximumKorg35Cells>
        korg35StageTwoState {};
    std::array<std::array<float, filterStateCount>, maximumKorg35Cells>
        korg35FeedbackToneState {};
    std::array<Korg35CoefficientCache, filterEngineCount>
        korg35CoefficientCache {};
    std::array<float, filterStateCount> korg35OnePoleState {};
    std::array<float, filterStateCount> korg35BassLossState {};
    std::array<float, filterStateCount> korg35DcInput {};
    std::array<float, filterStateCount> korg35DcOutput {};
    std::array<std::array<float, filterStateCount>, maximumSpectralBands>
        spectralIntegratorOne {};
    std::array<std::array<float, filterStateCount>, maximumSpectralBands>
        spectralIntegratorTwo {};
    std::array<std::array<float, maximumSpectralBands>, filterEngineCount>
        spectralCoefficient {};
    std::array<std::array<float, maximumSpectralBands>, filterEngineCount>
        spectralDamping {};
    std::array<std::array<float, maximumSpectralBands>, filterEngineCount>
        spectralWeight {};
    std::array<float, filterEngineCount> spectralOutputScale {};
    std::array<int, filterEngineCount> spectralCoefficientCountdown {};

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
