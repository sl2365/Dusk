#pragma once

#include "PluginProcessor.h"
#include <cmath>
#include <functional>

class DuskAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                       public juce::DragAndDropContainer,
                                       private juce::Timer,
                                       private juce::ListBoxModel
{
public:
    explicit DuskAudioProcessorEditor(DuskAudioProcessor&);
    ~DuskAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    using SliderAttachment =
        juce::AudioProcessorValueTreeState::SliderAttachment;

    enum class MainView : int
    {
        filter = 0,
        designer
    };

    class DetentedSlider final : public juce::Slider
    {
    public:
        std::function<bool()> onSavedStateReset;
        void setCentreDetent(double value, double radius)
        {
            centreDetent = value;
            detentRadius = juce::jmax(0.0, radius);
            centreDetentEnabled = true;
            getProperties().set("hasCentreDetent", true);
            getProperties().set("centreDetentValue", value);
        }
        double snapValue(double attemptedValue, DragMode dragMode) override
        {
            if (centreDetentEnabled && dragMode != notDragging
                && std::abs(attemptedValue - centreDetent) <= detentRadius)
                return centreDetent;
            return attemptedValue;
        }
        void mouseDoubleClick(const juce::MouseEvent& event) override
        {
            if (onSavedStateReset != nullptr && onSavedStateReset())
                return;
            juce::Slider::mouseDoubleClick(event);
        }
    private:
        double centreDetent = 0.0;
        double detentRadius = 0.0;
        bool centreDetentEnabled = false;
    };

    class SteppedComboBox final : public juce::ComboBox
    {
    public:
        void mouseWheelMove(const juce::MouseEvent&,
                            const juce::MouseWheelDetails& wheel) override
        {
            if (! isEnabled() || getNumItems() <= 0
                || std::abs(wheel.deltaY) < 0.0001f)
                return;
            const int direction = wheel.deltaY > 0.0f ? -1 : 1;
            const auto now = juce::Time::getMillisecondCounter();
            if (direction == lastWheelDirection
                && now - lastWheelTimeMs < 100u)
                return;
            lastWheelDirection = direction;
            lastWheelTimeMs = now;
            const int current = juce::jmax(0, getSelectedItemIndex());
            const int next = juce::jlimit(
                0, getNumItems() - 1, current + direction);
            if (next != getSelectedItemIndex())
                setSelectedItemIndex(next, juce::sendNotificationSync);
        }
    private:
        juce::uint32 lastWheelTimeMs = 0;
        int lastWheelDirection = 0;
    };

    class PresetDisplayLabel final : public juce::Label
    {
    public:
        std::function<void(int)> onStep;
        std::function<void()> onShowMenu;
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseWheelMove(const juce::MouseEvent&,
                            const juce::MouseWheelDetails&) override;
    private:
        juce::uint32 lastWheelTimeMs = 0;
        int lastWheelDirection = 0;
    };

    class PresetListBox final : public juce::ListBox
    {
    public:
        using juce::ListBox::ListBox;
        std::function<bool(int)> onHorizontalStep;
        bool keyPressed(const juce::KeyPress& key) override
        {
            int direction = 0;
            if (key.getKeyCode() == juce::KeyPress::leftKey)
                direction = -1;
            else if (key.getKeyCode() == juce::KeyPress::rightKey)
                direction = 1;
            if (direction != 0 && onHorizontalStep != nullptr
                && onHorizontalStep(direction))
                return true;
            return juce::ListBox::keyPressed(key);
        }
    };

    class ParameterDragLabel final : public juce::Label
    {
    public:
        std::function<juce::String()> parameterIdProvider;
        bool dragHandleOnly = false;
        bool dragHandleRight = false;
        void setTargetAppearance(bool assigned, juce::Colour accent);
        void paint(juce::Graphics&) override;
        void mouseDrag(const juce::MouseEvent&) override;
    private:
        bool targetAssigned = false;
        juce::Colour targetAccent { 0xff34d6c6 };
    };

    struct Dial
    {
        DetentedSlider slider;
        ParameterDragLabel label;
        std::unique_ptr<SliderAttachment> attachment;
        juce::String help;
        juce::String parameterId;
    };

    class ResponseDisplay final : public juce::Component
    {
    public:
        explicit ResponseDisplay(DuskAudioProcessor& owner)
            : processor(owner) {}
        std::function<juce::Colour(int)> colourForFilter;
        void paint(juce::Graphics&) override;
    private:
        DuskAudioProcessor& processor;
    };

    class SequencerPanel;
    class FolderTreeItem;
    class LookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        explicit LookAndFeel(bool compactPopupFont = false);
        juce::Label* createSliderTextBox(juce::Slider&) override;
        juce::Font getPopupMenuFont() override;
        juce::Font getComboBoxFont(juce::ComboBox&) override;
        void drawRotarySlider(juce::Graphics&, int, int, int, int,
                              float, float, float, juce::Slider&) override;
        void drawLinearSlider(juce::Graphics&, int, int, int, int,
                              float, float, float,
                              juce::Slider::SliderStyle,
                              juce::Slider&) override;
        void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                              bool, bool) override;
        void drawPopupMenuItem(juce::Graphics&,
                               const juce::Rectangle<int>&,
                               bool, bool, bool, bool, bool,
                               const juce::String&, const juce::String&,
                               const juce::Drawable*,
                               const juce::Colour*) override;
        void drawPopupMenuSectionHeader(
            juce::Graphics&, const juce::Rectangle<int>&,
            const juce::String&) override;
        void getIdealPopupMenuSectionHeaderSizeWithOptions(
            const juce::String&, int, int&, int&,
            const juce::PopupMenu::Options&) override;
        void drawPopupMenuBackground(juce::Graphics&, int, int) override;
        juce::PopupMenu::Options getOptionsForComboBoxPopupMenu(
            juce::ComboBox&, juce::Label&) override;
        void setPopupBorderColour(juce::Colour colour)
        {
            popupBorderColour = colour;
        }
        juce::Colour getPopupBorderColour() const noexcept
        {
            return popupBorderColour;
        }
    private:
        juce::Colour popupBorderColour { 0xff1479b8 };
        bool useCompactPopupFont = false;
    };

    DuskAudioProcessor& processor;
    LookAndFeel lookAndFeel;
    juce::Component content;
    std::vector<std::unique_ptr<Dial>> dials;
    Dial* filter1DriveDial = nullptr;
    Dial* filter2DriveDial = nullptr;
    Dial* filter1CutoffDial = nullptr;
    Dial* filter2CutoffDial = nullptr;
    Dial* filter1ResonanceDial = nullptr;
    Dial* filter2ResonanceDial = nullptr;
    Dial* filter1ResonanceShapeDial = nullptr;
    Dial* filter2ResonanceShapeDial = nullptr;
    Dial* filter1ResonanceLimitDial = nullptr;
    Dial* filter2ResonanceLimitDial = nullptr;
    Dial* filter1BassLossDial = nullptr;
    Dial* filter2BassLossDial = nullptr;
    Dial* filter1CoreSaturationDial = nullptr;
    Dial* filter2CoreSaturationDial = nullptr;
    Dial* filter1SaturationAsymmetryDial = nullptr;
    Dial* filter2SaturationAsymmetryDial = nullptr;
    Dial* filter1PoleSpreadDial = nullptr;
    Dial* filter2PoleSpreadDial = nullptr;
    Dial* filter1FeedbackDial = nullptr;
    Dial* filter2FeedbackDial = nullptr;
    Dial* filter1FeedbackLimitDial = nullptr;
    Dial* filter2FeedbackLimitDial = nullptr;
    Dial* filter1LadderStageSaturationDial = nullptr;
    Dial* filter2LadderStageSaturationDial = nullptr;
    Dial* filter1LadderFeedbackSaturationDial = nullptr;
    Dial* filter2LadderFeedbackSaturationDial = nullptr;
    Dial* filter1LadderResonanceCompensationDial = nullptr;
    Dial* filter2LadderResonanceCompensationDial = nullptr;
    Dial* filter1OtaCellDriveDial = nullptr;
    Dial* filter2OtaCellDriveDial = nullptr;
    Dial* filter1OtaFeedbackDampingDial = nullptr;
    Dial* filter2OtaFeedbackDampingDial = nullptr;
    Dial* filter1OtaResonanceCompensationDial = nullptr;
    Dial* filter2OtaResonanceCompensationDial = nullptr;
    Dial* filter1CombFrequencyDial = nullptr;
    Dial* filter2CombFrequencyDial = nullptr;
    Dial* filter1CombFeedbackDial = nullptr;
    Dial* filter2CombFeedbackDial = nullptr;
    Dial* filter1CombDampingDial = nullptr;
    Dial* filter2CombDampingDial = nullptr;
    Dial* filter1CombFineTuneDial = nullptr;
    Dial* filter2CombFineTuneDial = nullptr;
    Dial* filter1CombPolarityDial = nullptr;
    Dial* filter2CombPolarityDial = nullptr;
    Dial* filter1CombStereoSpreadDial = nullptr;
    Dial* filter2CombStereoSpreadDial = nullptr;
    Dial* filter1CombDispersionDial = nullptr;
    Dial* filter2CombDispersionDial = nullptr;
    Dial* filter1CombLoopSaturationDial = nullptr;
    Dial* filter2CombLoopSaturationDial = nullptr;
    Dial* filter1CombFeedbackLimitDial = nullptr;
    Dial* filter2CombFeedbackLimitDial = nullptr;
    Dial* filter1FormantVowelDial = nullptr;
    Dial* filter2FormantVowelDial = nullptr;
    Dial* filter1FormantShiftDial = nullptr;
    Dial* filter2FormantShiftDial = nullptr;
    Dial* filter1FormantFocusDial = nullptr;
    Dial* filter2FormantFocusDial = nullptr;
    Dial* filter1FormantOneLevelDial = nullptr;
    Dial* filter2FormantOneLevelDial = nullptr;
    Dial* filter1FormantTwoLevelDial = nullptr;
    Dial* filter2FormantTwoLevelDial = nullptr;
    Dial* filter1FormantThreeLevelDial = nullptr;
    Dial* filter2FormantThreeLevelDial = nullptr;
    Dial* filter1FormantBandwidthDial = nullptr;
    Dial* filter2FormantBandwidthDial = nullptr;
    Dial* filter1FormantSpreadDial = nullptr;
    Dial* filter2FormantSpreadDial = nullptr;
    Dial* filter1FormantTiltDial = nullptr;
    Dial* filter2FormantTiltDial = nullptr;
    Dial* filter1PhaserFrequencyDial = nullptr;
    Dial* filter2PhaserFrequencyDial = nullptr;
    Dial* filter1PhaserFeedbackDial = nullptr;
    Dial* filter2PhaserFeedbackDial = nullptr;
    Dial* filter1PhaserDepthDial = nullptr;
    Dial* filter2PhaserDepthDial = nullptr;
    Dial* filter1PhaserStagesDial = nullptr;
    Dial* filter2PhaserStagesDial = nullptr;
    Dial* filter1PhaserSpreadDial = nullptr;
    Dial* filter2PhaserSpreadDial = nullptr;
    Dial* filter1PhaserSkewDial = nullptr;
    Dial* filter2PhaserSkewDial = nullptr;
    Dial* filter1PhaserDampingDial = nullptr;
    Dial* filter2PhaserDampingDial = nullptr;
    Dial* filter1PhaserStageSaturationDial = nullptr;
    Dial* filter2PhaserStageSaturationDial = nullptr;
    Dial* filter1PhaserStereoDial = nullptr;
    Dial* filter2PhaserStereoDial = nullptr;
    Dial* filter1ModalFrequencyDial = nullptr;
    Dial* filter2ModalFrequencyDial = nullptr;
    Dial* filter1ModalDecayDial = nullptr;
    Dial* filter2ModalDecayDial = nullptr;
    Dial* filter1ModalBodyDial = nullptr;
    Dial* filter2ModalBodyDial = nullptr;
    Dial* filter1ModalModesDial = nullptr;
    Dial* filter2ModalModesDial = nullptr;
    Dial* filter1ModalRatioDial = nullptr;
    Dial* filter2ModalRatioDial = nullptr;
    Dial* filter1ModalInharmonicityDial = nullptr;
    Dial* filter2ModalInharmonicityDial = nullptr;
    Dial* filter1ModalDecayTiltDial = nullptr;
    Dial* filter2ModalDecayTiltDial = nullptr;
    Dial* filter1ModalDampingDial = nullptr;
    Dial* filter2ModalDampingDial = nullptr;
    Dial* filter1ModalExcitationDial = nullptr;
    Dial* filter2ModalExcitationDial = nullptr;
    Dial* filter1SallenKeyQCurveDial = nullptr;
    Dial* filter2SallenKeyQCurveDial = nullptr;
    Dial* filter1SallenKeyCellGainDial = nullptr;
    Dial* filter2SallenKeyCellGainDial = nullptr;
    Dial* filter1SallenKeyStageCouplingDial = nullptr;
    Dial* filter2SallenKeyStageCouplingDial = nullptr;
    Dial* filter1SallenKeyComponentVariationDial = nullptr;
    Dial* filter2SallenKeyComponentVariationDial = nullptr;
    Dial* filter1SallenKeyOpAmpSaturationDial = nullptr;
    Dial* filter2SallenKeyOpAmpSaturationDial = nullptr;
    Dial* filter1SallenKeyHighFrequencyDampingDial = nullptr;
    Dial* filter2SallenKeyHighFrequencyDampingDial = nullptr;
    Dial* filter1DiodeLadderSaturationDial = nullptr;
    Dial* filter2DiodeLadderSaturationDial = nullptr;
    Dial* filter1DiodeLadderFeedbackClipDial = nullptr;
    Dial* filter2DiodeLadderFeedbackClipDial = nullptr;
    Dial* filter1DiodeLadderBiasDial = nullptr;
    Dial* filter2DiodeLadderBiasDial = nullptr;
    Dial* filter1DiodeLadderStageTiltDial = nullptr;
    Dial* filter2DiodeLadderStageTiltDial = nullptr;
    Dial* filter1DiodeLadderResonanceLossDial = nullptr;
    Dial* filter2DiodeLadderResonanceLossDial = nullptr;
    Dial* filter1DiodeLadderResonanceCompensationDial = nullptr;
    Dial* filter2DiodeLadderResonanceCompensationDial = nullptr;
    Dial* filter1SteinerDiodeDriveDial = nullptr;
    Dial* filter2SteinerDiodeDriveDial = nullptr;
    Dial* filter1SteinerFeedbackClipDial = nullptr;
    Dial* filter2SteinerFeedbackClipDial = nullptr;
    Dial* filter1SteinerBiasDial = nullptr;
    Dial* filter2SteinerBiasDial = nullptr;
    Dial* filter1SteinerPoleSpreadDial = nullptr;
    Dial* filter2SteinerPoleSpreadDial = nullptr;
    Dial* filter1SteinerResonanceCurveDial = nullptr;
    Dial* filter2SteinerResonanceCurveDial = nullptr;
    Dial* filter1SteinerResonanceCompensationDial = nullptr;
    Dial* filter2SteinerResonanceCompensationDial = nullptr;
    Dial* filter1VactrolRiseDial = nullptr;
    Dial* filter2VactrolRiseDial = nullptr;
    Dial* filter1VactrolFallDial = nullptr;
    Dial* filter2VactrolFallDial = nullptr;
    Dial* filter1VactrolMemoryDial = nullptr;
    Dial* filter2VactrolMemoryDial = nullptr;
    Dial* filter1VactrolLevelCouplingDial = nullptr;
    Dial* filter2VactrolLevelCouplingDial = nullptr;
    Dial* filter1VactrolWarmthDial = nullptr;
    Dial* filter2VactrolWarmthDial = nullptr;
    Dial* filter1VactrolBleedDial = nullptr;
    Dial* filter2VactrolBleedDial = nullptr;
    Dial* filter1Korg35ChipDriveDial = nullptr;
    Dial* filter2Korg35ChipDriveDial = nullptr;
    Dial* filter1Korg35FeedbackClipDial = nullptr;
    Dial* filter2Korg35FeedbackClipDial = nullptr;
    Dial* filter1Korg35BiasDial = nullptr;
    Dial* filter2Korg35BiasDial = nullptr;
    Dial* filter1Korg35FeedbackToneDial = nullptr;
    Dial* filter2Korg35FeedbackToneDial = nullptr;
    Dial* filter1Korg35BassLossDial = nullptr;
    Dial* filter2Korg35BassLossDial = nullptr;
    Dial* filter1Korg35ResonanceCompensationDial = nullptr;
    Dial* filter2Korg35ResonanceCompensationDial = nullptr;
    Dial* filter1SpectralCentreDial = nullptr;
    Dial* filter2SpectralCentreDial = nullptr;
    Dial* filter1SpectralSpanDial = nullptr;
    Dial* filter2SpectralSpanDial = nullptr;
    Dial* filter1SpectralDepthDial = nullptr;
    Dial* filter2SpectralDepthDial = nullptr;
    Dial* filter1SpectralBandsDial = nullptr;
    Dial* filter2SpectralBandsDial = nullptr;
    Dial* filter1SpectralWidthDial = nullptr;
    Dial* filter2SpectralWidthDial = nullptr;
    Dial* filter1SpectralPatternDial = nullptr;
    Dial* filter2SpectralPatternDial = nullptr;
    Dial* filter1SpectralShiftDial = nullptr;
    Dial* filter2SpectralShiftDial = nullptr;
    Dial* filter1SpectralContrastDial = nullptr;
    Dial* filter2SpectralContrastDial = nullptr;
    Dial* filter1SpectralTiltDial = nullptr;
    Dial* filter2SpectralTiltDial = nullptr;
    Dial* filter1SlopeDial = nullptr;
    Dial* filter2SlopeDial = nullptr;
    Dial* filter1GainDial = nullptr;
    Dial* filter2GainDial = nullptr;
    Dial* mixDial = nullptr;
    Dial* outputDial = nullptr;
    Dial* themeDial = nullptr;
    Dial* browserMixDial = nullptr;
    std::unique_ptr<juce::Drawable> logoDrawable;
    juce::ToggleButton logoMenuButton;
    juce::Label versionLabel;
    SteppedComboBox routingBox;
    SteppedComboBox filter1ArchitectureBox;
    SteppedComboBox filter2ArchitectureBox;
    SteppedComboBox filter1TypeBox;
    SteppedComboBox filter2TypeBox;
    SteppedComboBox filter1CombModeBox;
    SteppedComboBox filter2CombModeBox;
    SteppedComboBox filter1DistortionTypeBox;
    SteppedComboBox filter2DistortionTypeBox;
    SteppedComboBox filter1DistortionPlacementBox;
    SteppedComboBox filter2DistortionPlacementBox;
    SteppedComboBox filter1ModelBox;
    SteppedComboBox filter2ModelBox;
    juce::Label routingLabel;
    juce::Label filter1ArchitectureLabel;
    juce::Label filter2ArchitectureLabel;
    juce::Label filter1TypeLabel;
    juce::Label filter2TypeLabel;
    juce::Label filter1CombModeLabel;
    juce::Label filter2CombModeLabel;
    juce::Label filter1DistortionTypeLabel;
    juce::Label filter2DistortionTypeLabel;
    juce::Label filter1DistortionPlacementLabel;
    juce::Label filter2DistortionPlacementLabel;
    juce::Label filter1ModelLabel;
    juce::Label filter2ModelLabel;
    juce::Label designer1Placeholder;
    juce::Label designer2Placeholder;
    ResponseDisplay responseDisplay;
    juce::ToggleButton filterViewButton { "FILTER" };
    juce::ToggleButton designerViewButton { "DESIGNER" };
    juce::ToggleButton viewSwitcherButton;
    juce::ToggleButton filter1PowerButton;
    juce::ToggleButton filter2PowerButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        routingAttachment,
        filter1ArchitectureAttachment, filter2ArchitectureAttachment,
        filter1TypeAttachment, filter2TypeAttachment,
        filter1CombModeAttachment, filter2CombModeAttachment,
        filter1DistortionTypeAttachment, filter2DistortionTypeAttachment,
        filter1DistortionPlacementAttachment,
        filter2DistortionPlacementAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        filter1PowerAttachment, filter2PowerAttachment;
    juce::ToggleButton bypassButton;
    std::unique_ptr<juce::ParameterAttachment> bypassAttachment;
    juce::ToggleButton mixLockButton;
    juce::ToggleButton presetBrowserButton;
    juce::ToggleButton previousPresetButton;
    juce::ToggleButton nextPresetButton;
    PresetDisplayLabel currentPresetLabel;
    juce::TreeView presetFolderTree;
    PresetListBox presetList { "Preset list", this };
    juce::Label presetFolderLabel;
    juce::Label presetListLabel;
    juce::ToggleButton presetSaveButton;
    juce::ToggleButton presetSaveAsButton;
    juce::ToggleButton presetOpenFolderButton;
    juce::ToggleButton randomButton { "!!!" };
    juce::ToggleButton sequencerButton { "SEQ" };
    juce::ToggleButton filter1ModelMenuButton { "..." };
    juce::ToggleButton filter2ModelMenuButton { "..." };
    std::unique_ptr<SequencerPanel> sequencerPanel;
    std::unique_ptr<FolderTreeItem> presetFolderRoot;
    std::unique_ptr<juce::FileChooser> presetFileChooser;
    std::unique_ptr<juce::FileChooser> filterModelFileChooser;
    std::unique_ptr<juce::AlertWindow> presetFolderNameWindow;
    juce::Array<juce::File> visiblePresetFiles;
    juce::File selectedPresetFolder;
    juce::File currentPresetFile;
    juce::File copiedPresetFile;
    juce::StringArray favouritePresetPaths;
    juce::Label information;

    static constexpr int designWidth = 926;
    static constexpr int collapsedDesignHeight = 276;
    static constexpr int expandedDesignHeight = 530;
    int activeDesignHeight() const noexcept
    {
        return sequencerOpen ? expandedDesignHeight : collapsedDesignHeight;
    }

    Dial* addDial(const char*, const char*, const char*,
                  bool attachToParameter = true);
    bool restoreDialFromSavedState(Dial&);
    void updateContextSensitiveHelp();
    void setMainView(MainView);
    void showLogoZoomMenu();
    void setZoomPercent(int);
    void updateArchitectureContext();
    int architectureChoiceForFilter(int filterIndex) const;
    void refreshFilterModelBoxes();
    void handleFilterModelSelection(int filterIndex);
    void applyFactoryFilterModel(int filterIndex, int selectedId);
    juce::Result loadUserFilterModel(int filterIndex,
                                     const juce::File& modelFile);
    juce::Result saveFilterModelToFile(int filterIndex,
                                       const juce::File& modelFile);
    void showSaveFilterModelAs(int filterIndex);
    void showFilterModelMenu(int filterIndex);
    void markFilterModelCustom(int filterIndex);
    bool isFilterModelParameter(const juce::String& parameterId,
                                int& filterIndex) const;
    juce::Colour filterAccent(int filterIndex) const;
    juce::Colour panelColour(int filterIndex) const;
    void setPresetBrowserOpen(bool);
    void updatePresetBrowserVisibility();
    void updateFilterControlEnablement();
    void rebuildPresetFolderTree();
    void selectPresetFolder(const juce::File&);
    void selectFavouritePresets();
    void refreshPresetList();
    void updatePresetNavigationDisplay();
    void setSequencerOpen(bool);
    void updateSequencerTargetHighlights();
    void updateAnimatedParameterDisplays();
    void randomiseEffectParameters();
    bool handlePresetCursorKey(int direction);
    void stepPreset(int direction);
    void showPresetQuickMenu();
    void loadPresetFile(const juce::File&);
    void loadPresetAtBrowserCell(int row, int column);
    void showPresetContextMenu(const juce::File&, juce::Point<int>);
    void showPresetFolderContextMenu(const juce::File&, bool,
                                     juce::Point<int>);
    void copyPreset(const juce::File&);
    void pastePresetIntoFolder(const juce::File&);
    void showCreatePresetFolderDialog(const juce::File&);
    void confirmDeletePresetFolder(const juce::File&);
    juce::Array<juce::File> favouritePresetFiles() const;
    juce::Array<juce::File> navigationPresetFiles() const;
    bool isPresetFavourite(const juce::File&) const;
    void togglePresetFavourite(const juce::File&);
    void loadFavouritePresets();
    void saveFavouritePresets() const;
    void saveCurrentPreset();
    void showPresetSaveAs();
    int getNumRows() override;
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override;
    void listBoxItemClicked(int, const juce::MouseEvent&) override;
    void returnKeyPressed(int) override;
    void selectedRowsChanged(int) override;
    void timerCallback() override;
    void saveInterfaceSettings() const;

    int observedEditorWidth = designWidth;
    double observedThemeValue = 100.0;
    int stableSettingsTicks = 0;
    bool settingsDirty = false;
    bool restoringLockedMix = false;
    double lockedMixValue = 35.0;
    bool presetBrowserOpen = false;
    MainView mainView = MainView::filter;
    std::array<int, 2> displayedArchitectureChoices { -1, -1 };
    std::array<int, 2> displayedFilterEnabled { -1, -1 };
    juce::Array<juce::File> userFilterModelFiles;
    std::array<juce::File, 2> selectedUserFilterModelFiles;
    std::array<juce::String, 2> selectedFactoryFilterModels;
    bool applyingFilterModel = false;
    bool filterModelBoxesReady = false;
    bool favouritesViewSelected = false;
    bool sequencerOpen = false;
    bool previousPresetKeyDown = false;
    bool nextPresetKeyDown = false;
    int selectedPresetBrowserColumn = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuskAudioProcessorEditor)
};
