#include "PluginEditor.h"
#include <BinaryData.h>
#include <algorithm>
#include <cmath>

namespace
{
const juce::Colour knobTrackColour(0xff343940);
const juce::Colour knobBodyColour(0xff2b3036);
const juce::Colour knobLabelColour(0xff8d949e);
const juce::Colour knobValueColour(knobLabelColour.brighter(0.18f));
const juce::Colour controlAccentColour(0xff65cbb9);
const juce::Colour knobIndicatorColour(0xffd2b84f);
const juce::Colour tempoSyncLedColour(0xff55c96b);
const juce::Colour linkLedColour(0xffffad42);
const juce::Colour bipolarLedColour(0xffd96b72);
const juce::Colour unipolarNegativeLedColour(0xff8f68cf);
const juce::Colour powerLedColour(0xff82d16a);
const juce::Colour presetAccentColour(0xff72b9a9);
constexpr auto presetFileExtension = ".duskpreset";
constexpr auto modelFileExtension = ".duskmodel";
constexpr auto sequencerTargetDragPrefix = "dusk-sequencer-target:";
const juce::Identifier contextHelpTitleProperty("contextHelpTitle");

constexpr std::array<const char*, 8> cleanModelParameterSuffixes
{{
    "ResonanceShape", "ResonanceLimit", "BassLoss", "CoreSaturation",
    "SaturationAsymmetry", "PoleSpread", "Feedback", "FeedbackLimit"
}};

constexpr std::array<const char*, 3> ladderModelParameterSuffixes
{{
    "LadderStageSaturation", "LadderFeedbackSaturation",
    "LadderResonanceCompensation"
}};

constexpr std::array<const char*, 3> otaModelParameterSuffixes
{{
    "OtaCellDrive", "OtaFeedbackDamping", "OtaResonanceCompensation"
}};

struct CleanFactoryModel
{
    const char* name;
    std::array<float, cleanModelParameterSuffixes.size()> values;
};

struct LadderFactoryModel
{
    const char* name;
    std::array<float, ladderModelParameterSuffixes.size()> values;
};

struct OtaFactoryModel
{
    const char* name;
    std::array<float, otaModelParameterSuffixes.size()> values;
};

constexpr std::array<CleanFactoryModel, 4> cleanFactoryModels
{{
    { "Clean SVF", { 0.0f, 100.0f, 0.0f, 0.0f,
                      0.0f, 0.0f, 0.0f, 50.0f } },
    { "Smooth SVF", { -70.0f, 60.0f, 10.0f, 25.0f,
                       0.0f, 25.0f, 10.0f, 30.0f } },
    { "Warm SVF", { -15.0f, 85.0f, 55.0f, 85.0f,
                     45.0f, 35.0f, 35.0f, 20.0f } },
    { "Wide Resonance", { 60.0f, 100.0f, 65.0f, 45.0f,
                           -25.0f, 100.0f, 80.0f, 70.0f } }
}};

constexpr std::array<LadderFactoryModel, 4> ladderFactoryModels
{{
    { "Clean Ladder", { 0.0f, 0.0f, 0.0f } },
    { "Classic Ladder", { 45.0f, 25.0f, 55.0f } },
    { "Acid Ladder", { 100.0f, 100.0f, 100.0f } },
    { "Warm Vintage", { 72.0f, 50.0f, 85.0f } }
}};

constexpr std::array<OtaFactoryModel, 4> otaFactoryModels
{{
    { "Clean OTA", { 0.0f, 0.0f, 0.0f } },
    { "Punchy OTA", { 30.0f, 15.0f, 65.0f } },
    { "Hot OTA", { 85.0f, 30.0f, 80.0f } },
    { "Dark OTA", { 55.0f, 90.0f, 70.0f } }
}};

constexpr int cleanFactoryModelIdBase = 1000;
constexpr int ladderFactoryModelIdBase = 1100;
constexpr int otaFactoryModelIdBase = 1200;
constexpr int userFilterModelIdBase = 2000;

juce::String contextHelpText(const juce::String& title,
                             const juce::String& description)
{
    auto cleanDescription = description.trim()
        .replace("\r", " ").replace("\n", " ");
    while (cleanDescription.contains("  "))
        cleanDescription = cleanDescription.replace("  ", " ");
    const auto cleanTitle = title.trim().toUpperCase();
    if (cleanTitle.isEmpty())
        return cleanDescription;
    if (cleanDescription.isEmpty())
        return cleanTitle;
    return cleanTitle + " | " + cleanDescription;
}

juce::Array<juce::File> presetFilesInFolder(const juce::File& folder)
{
    juce::Array<juce::File> files;
    if (! folder.isDirectory())
        return files;

    folder.findChildFiles(files, juce::File::findFiles, false);
    files.removeIf([](const juce::File& file)
    {
        return ! file.hasFileExtension(presetFileExtension);
    });
    std::sort(files.begin(), files.end(),
              [](const juce::File& first, const juce::File& second)
              {
                  return first.getFileNameWithoutExtension().compareNatural(
                             second.getFileNameWithoutExtension(), true) < 0;
              });
    return files;
}

juce::Path makeStarPath(juce::Rectangle<float> bounds)
{
    juce::Path path;
    const auto centre = bounds.getCentre();
    const float outerRadius = 0.5f * juce::jmin(bounds.getWidth(),
                                                bounds.getHeight());
    const float innerRadius = outerRadius * 0.43f;
    for (int point = 0; point < 10; ++point)
    {
        const float radius = (point % 2 == 0) ? outerRadius : innerRadius;
        const float angle = -juce::MathConstants<float>::halfPi
            + static_cast<float>(point)
                * juce::MathConstants<float>::pi / 5.0f;
        const juce::Point<float> position(
            centre.x + std::cos(angle) * radius,
            centre.y + std::sin(angle) * radius);
        if (point == 0)
            path.startNewSubPath(position);
        else
            path.lineTo(position);
    }
    path.closeSubPath();
    return path;
}

constexpr std::array<const char*, 17> distortionSummaries
{
    "Dense tanh clipping with strong static level compensation",
    "Open arctangent saturation with brighter, gentler harmonics",
    "Asymmetric valve-style soft saturation with even harmonics",
    "Full-wave folding followed by DC-safe filtering",
    "Signal-responsive broken-contact pops over compact saturation",
    "Sharp hard clipping charged with signal-tracked white noise",
    "Cascaded Class-A-style preamp and power-stage overdrive",
    "Uneven frequency-shaped waveshaping with nonlinear ripples",
    "Different positive and negative curves for strong even harmonics",
    "Upper-mid pre-emphasis feeding sharp ridged saturation",
    "Bit-depth and sample-rate reduction with quiet deterministic dither",
    "Short resonant comb recurrence feeding bounded saturation",
    "Compressed wavefolding with a strained asymmetric edge",
    "Sliced, upper-mid-emphasised clipping with a metallic blade",
    "Bright high-band excitation over restrained clean saturation",
    "Clean flat-threshold clipping without added noise or tone filtering",
    "High-gain asymmetric fuzz with compressed sustain and a restrained top end"
};
constexpr std::array<const char*, 17> distortionDetails
{
    "DIST increases density and compression through a level-compensated soft curve.",
    "DIST moves smoothly from clean into open saturation without darkening the top end.",
    "DIST moves from clean warmth into rounded compression and asymmetric colour.",
    "DIST moves from the dry signal into octave-rich electronic crunch.",
    "DIST increases crack density, broken-contact edge and compact grit.",
    "DIST increases electrical bite and noise while keeping the input bounded.",
    "DIST moves from warm edge into raw drive with a restrained amp-like top end.",
    "DIST increases shifting ridges, asymmetric grit and midrange emphasis.",
    "DIST progressively separates the two polarities without leaving a DC offset.",
    "DIST intensifies high-frequency teeth and narrow, cutting harmonic ridges.",
    "DIST lowers effective resolution and update rate for increasingly coarse digital texture.",
    "DIST shortens and strengthens the resonant path for metallic, pitched colour.",
    "DIST pushes a smooth curve into folded, tense and increasingly unstable harmonics.",
    "DIST widens the dead zone and sharpens the bright, sliced transient edge.",
    "DIST adds clear upper harmonics and presence while leaving the low band comparatively intact.",
    "DIST lowers the clipping threshold for progressively harder, squared-off transients.",
    "DIST increases asymmetric clipping, dense sustain and dark post-clip fuzz colour."
};

juce::Colour filterColourForId(int filterIndex, int typeId)
{
    static constexpr std::array<juce::uint32, 8> filterOneColours
    {
        0xff1479b8, 0xff159b91, 0xff3d6406, 0xffa46c3f,
        0xffc14521, 0xff5968d8, 0xff82436e, 0xff6e5fa8
    };
    static constexpr std::array<juce::uint32, 8> filterTwoColours
    {
        0xff82436e, 0xffb04378, 0xffc23d82, 0xff6e5fa8,
        0xff5968d8, 0xff159b91, 0xffa46c3f, 0xffc14521
    };
    const auto& colours = filterIndex == 2 ? filterTwoColours
                                            : filterOneColours;
    return juce::Colour(colours[static_cast<size_t>(juce::jlimit(
        0, static_cast<int>(colours.size()) - 1, typeId))]);
}

struct InterfaceSettings
{
    int scalePercent = 100;
    double themePercent = 100.0;
    bool sequencerOpen = false;
    bool sequencerAnimate = false;
    juce::String mainView { "Filter" };
};

InterfaceSettings loadInterfaceSettings()
{
    InterfaceSettings settings;
    const auto file = DuskAudioProcessor::getSettingsDirectory()
        .getChildFile("Settings.ini");
    if (! file.existsAsFile())
        return settings;

    juce::StringArray lines;
    lines.addLines(file.loadFileAsString());
    bool inGuiSection = false;
    for (auto line : lines)
    {
        line = line.trim();
        if (line.startsWithChar('['))
        {
            inGuiSection = line.equalsIgnoreCase("[GUI]");
            continue;
        }
        if (! inGuiSection)
            continue;

        const int separator = line.indexOfChar('=');
        if (separator <= 0)
            continue;
        const auto key = line.substring(0, separator).trim();
        const auto value = line.substring(separator + 1).trim();
        if (key.equalsIgnoreCase("ScalePercent"))
            settings.scalePercent = juce::jlimit(75, 200, value.getIntValue());
        else if (key.equalsIgnoreCase("ThemePercent"))
            settings.themePercent = juce::jlimit(0.0, 100.0,
                                                  static_cast<double>(value.getFloatValue()));
        else if (key.equalsIgnoreCase("SequencerOpen"))
            settings.sequencerOpen = value.getIntValue() != 0;
        else if (key.equalsIgnoreCase("SequencerAnimate"))
            settings.sequencerAnimate = value.getIntValue() != 0;
        else if (key.equalsIgnoreCase("MainView"))
            settings.mainView = value;
    }
    return settings;
}

class PresetPopupList final : public juce::PopupMenu::CustomComponent,
                              private juce::ListBoxModel
{
public:
    PresetPopupList(juce::Array<juce::File> filesToUse,
                    const juce::File& currentFile,
                    std::function<void(const juce::File&)> selectionCallback)
        : juce::PopupMenu::CustomComponent(false),
          files(std::move(filesToUse)),
          onSelected(std::move(selectionCallback)),
          list("Preset quick list", this)
    {
        list.setRowHeight(rowHeight);
        list.setColour(juce::ListBox::backgroundColourId,
                       juce::Colour(0xff14181d));
        list.setColour(juce::ListBox::outlineColourId,
                       juce::Colours::transparentBlack);
        list.setOutlineThickness(0);
        addAndMakeVisible(list);
        list.updateContent();

        const int currentRow = files.indexOf(currentFile);
        if (currentRow >= 0)
        {
            list.selectRow(currentRow, false, false);
            rowToReveal = currentRow;
        }
    }

    void getIdealSize(int& idealWidth, int& idealHeight) override
    {
        idealWidth = 224;
        idealHeight = juce::jmax(1, juce::jmin(maximumVisibleRows,
                                               files.size())) * rowHeight + 4;
    }

    void resized() override
    {
        // Leave the same two-pixel frame visible as the Mode and Type menus.
        list.setBounds(getLocalBounds().reduced(2));
        if (rowToReveal >= 0)
        {
            list.scrollToEnsureRowIsOnscreen(rowToReveal);
            rowToReveal = -1;
        }
    }

private:
    int getNumRows() override
    {
        return files.size();
    }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height,
                          bool rowIsSelected) override
    {
        if (! juce::isPositiveAndBelow(row, files.size()))
            return;

        if (rowIsSelected)
        {
            g.setColour(juce::Colour(0xff2a3037));
            g.fillRoundedRectangle(3.0f, 1.0f,
                                   static_cast<float>(width - 6),
                                   static_cast<float>(height - 2), 2.0f);
        }

        g.setColour(rowIsSelected
            ? knobIndicatorColour.brighter(0.28f)
            : knobValueColour);
        g.setFont(getLookAndFeel().getPopupMenuFont());
        g.drawFittedText(files.getReference(row).getFileNameWithoutExtension(),
                         8, 0, juce::jmax(0, width - 16), height,
                         juce::Justification::centredLeft, 1, 0.75f);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        chooseRow(row);
    }

    void returnKeyPressed(int row) override
    {
        chooseRow(row);
    }

    void chooseRow(int row)
    {
        if (! juce::isPositiveAndBelow(row, files.size()))
            return;

        if (onSelected)
            onSelected(files.getReference(row));
        triggerMenuItem();
    }

    static constexpr int rowHeight = 20;
    static constexpr int maximumVisibleRows = 20;
    juce::Array<juce::File> files;
    std::function<void(const juce::File&)> onSelected;
    juce::ListBox list;
    int rowToReveal = -1;
};
}

class DuskAudioProcessorEditor::FolderTreeItem final
    : public juce::TreeViewItem
{
public:
    FolderTreeItem(juce::File directoryToUse,
                   std::function<void(const juce::File&, bool)>
                       selectionCallback,
                   std::function<void(const juce::File&, bool,
                                      juce::Point<int>)> contextCallback,
                   bool favouriteItem = false,
                   int depth = 0)
        : directory(std::move(directoryToUse)),
          onSelected(std::move(selectionCallback)),
          onContext(std::move(contextCallback)),
          isFavouriteItem(favouriteItem)
    {
        if (isFavouriteItem || depth >= 32)
            return;

        if (depth == 0)
            addSubItem(new FolderTreeItem(
                directory, onSelected, onContext, true, depth + 1));

        juce::Array<juce::File> childDirectories;
        directory.findChildFiles(childDirectories, juce::File::findDirectories,
                                 false);
        std::sort(childDirectories.begin(), childDirectories.end(),
                  [](const juce::File& first, const juce::File& second)
                  {
                      return first.getFileName().compareNatural(
                                 second.getFileName(), true) < 0;
                  });
        for (const auto& child : childDirectories)
            if (! child.isSymbolicLink())
                addSubItem(new FolderTreeItem(
                    child, onSelected, onContext, false, depth + 1));
    }

    bool mightContainSubItems() override
    {
        return getNumSubItems() > 0;
    }

    juce::String getUniqueName() const override
    {
        return isFavouriteItem
            ? directory.getFullPathName() + "::Favourites"
            : directory.getFullPathName();
    }

    int getItemHeight() const override
    {
        return 22;
    }

    void paintItem(juce::Graphics& g, int width, int height) override
    {
        if (isSelected())
        {
            g.setColour(presetAccentColour.withAlpha(0.30f));
            g.fillRoundedRectangle(0.0f, 1.0f, static_cast<float>(width),
                                   static_cast<float>(height - 2), 3.0f);
        }

        if (isFavouriteItem)
        {
            g.setColour(isSelected() ? knobIndicatorColour
                                     : knobLabelColour.brighter(0.12f));
            g.fillPath(makeStarPath({ 2.0f,
                static_cast<float>(height) * 0.5f - 6.0f,
                12.0f, 12.0f }));
        }
        else
        {
            const auto folderBounds = juce::Rectangle<float>(
                2.0f, static_cast<float>(height) * 0.5f - 5.0f,
                13.0f, 10.0f);
            g.setColour(isSelected() ? knobIndicatorColour
                                     : knobLabelColour.brighter(0.12f));
            g.fillRoundedRectangle(folderBounds, 1.5f);
            g.fillRect(folderBounds.getX() + 1.5f,
                       folderBounds.getY() - 2.0f, 5.0f, 3.0f);
        }
        g.setColour(isSelected() ? juce::Colours::white
                                 : knobValueColour);
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawFittedText(isFavouriteItem ? "Favourites"
                                         : directory.getFileName(),
                         21, 0, juce::jmax(0, width - 24), height,
                         juce::Justification::centredLeft, 1);
    }

    void itemSelectionChanged(bool isNowSelected) override
    {
        if (isNowSelected && onSelected)
            onSelected(directory, isFavouriteItem);
        repaintItem();
    }

    void itemClicked(const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu() && onContext)
            onContext(directory, isFavouriteItem,
                      event.getScreenPosition());
    }

    FolderTreeItem* findItemForDirectory(const juce::File& target,
                                         bool findFavourites = false)
    {
        if (isFavouriteItem == findFavourites
            && (findFavourites || directory == target))
            return this;

        for (int index = 0; index < getNumSubItems(); ++index)
            if (auto* child = dynamic_cast<FolderTreeItem*>(getSubItem(index)))
                if (auto* match = child->findItemForDirectory(
                        target, findFavourites))
                    return match;
        return nullptr;
    }

private:
    juce::File directory;
    std::function<void(const juce::File&, bool)> onSelected;
    std::function<void(const juce::File&, bool, juce::Point<int>)> onContext;
    bool isFavouriteItem = false;
};

namespace
{
void setParameterActualValue(juce::RangedAudioParameter* parameter,
                             float actualValue)
{
    if (parameter == nullptr)
        return;
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(actualValue));
    parameter->endChangeGesture();
}
}

class DuskAudioProcessorEditor::SequencerPanel final
    : public juce::Component
{
public:
    class StepGrid final : public juce::Component,
                           public juce::SettableTooltipClient
    {
    public:
        StepGrid(DuskAudioProcessor& audioProcessor,
                 LookAndFeel& sharedLookAndFeelToUse,
                 LookAndFeel& compactLookAndFeelToUse)
            : processor(audioProcessor),
              sharedPopupLookAndFeel(sharedLookAndFeelToUse),
              compactPopupLookAndFeel(compactLookAndFeelToUse)
        {
            for (int step = 0; step < dusk::sequencer::stepCount; ++step)
            {
                steps[static_cast<size_t>(step)]
                    = processor.parameters.getParameter(
                        dusk::sequencer::stepParameterId(step));
                gateSteps[static_cast<size_t>(step)]
                    = processor.parameters.getParameter(
                        dusk::sequencer::gateStepParameterId(step));
            }
            start = processor.parameters.getRawParameterValue("sequencerStart");
            end = processor.parameters.getRawParameterValue("sequencerEnd");
            peak = processor.parameters.getRawParameterValue("sequencerPeak");
            move = processor.parameters.getRawParameterValue("sequencerMove");
            attack = processor.parameters.getRawParameterValue(
                "sequencerAttack");
            release = processor.parameters.getRawParameterValue(
                "sequencerRelease");
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
            setTooltip("Click or drag vertically to set the sequencer steps");
        }

        void setAccent(juce::Colour colour)
        {
            accent = colour;
            repaint();
        }

        void setPolarityMode(int newMode)
        {
            newMode = juce::jlimit(0, 2, newMode);
            if (polarityMode == newMode)
                return;

            polarityMode = newMode;
            repaint();
        }

        void paint(juce::Graphics& graphics) override
        {
            const auto whole = getLocalBounds().toFloat();
            graphics.setColour(juce::Colour(0xff10151a));
            graphics.fillRoundedRectangle(whole, 5.0f);
            const auto graph = graphBounds();
            const float columnWidth = graph.getWidth()
                                    / dusk::sequencer::stepCount;
            const bool isBipolar = polarityMode == 1;
            const bool isNegative = polarityMode == 2;
            const int first = juce::jlimit(0, 30,
                juce::roundToInt(start != nullptr ? start->load() : 1.0f) - 1);
            const int last = juce::jlimit(first + 1, 31,
                juce::roundToInt(end != nullptr ? end->load() : 32.0f) - 1);
            const int active = processor.getActiveSequencerStep();

            std::array<float, dusk::sequencer::stepCount> values {};
            float patternMinimum = 1.0f;
            float patternMaximum = 0.0f;
            for (int step = 0; step < dusk::sequencer::stepCount; ++step)
            {
                const auto index = static_cast<size_t>(step);
                values[index] = steps[index] != nullptr
                    ? steps[index]->convertFrom0to1(steps[index]->getValue())
                    : 1.0f;
                patternMinimum = juce::jmin(patternMinimum, values[index]);
                patternMaximum = juce::jmax(patternMaximum, values[index]);
            }
            const float peakValue = peak != nullptr ? peak->load() : 0.0f;
            const float moveValue = move != nullptr ? move->load() : 0.0f;
            for (auto& value : values)
                value = dusk::sequencer::applyDepthOffset(
                    value, patternMinimum, patternMaximum,
                    peakValue, moveValue, isBipolar);

            const auto displayedMagnitude = [isBipolar](float canonical)
            {
                return isBipolar ? juce::jlimit(0.0f, 1.0f, canonical)
                    : juce::jlimit(0.0f, 1.0f, 2.0f * canonical - 1.0f);
            };
            const auto displayY = [graph, isBipolar, isNegative,
                                   &displayedMagnitude](float canonical)
            {
                const float displayed = displayedMagnitude(canonical);
                if (isBipolar)
                    return graph.getBottom() - graph.getHeight() * displayed;
                return isNegative ? graph.getY()
                                      + graph.getHeight() * displayed
                                  : graph.getBottom()
                                      - graph.getHeight() * displayed;
            };
            if (isBipolar)
            {
                graphics.setColour(accent.withAlpha(0.22f));
                graphics.drawHorizontalLine(
                    juce::roundToInt(graph.getCentreY()), graph.getX(),
                    graph.getRight());
            }

            graphics.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            for (int step = 0; step < dusk::sequencer::stepCount; ++step)
            {
                const float x = graph.getX() + columnWidth * step;
                const auto cell = juce::Rectangle<float>(
                    x, graph.getY(), columnWidth, graph.getHeight());
                if ((step / 4) % 2 != 0)
                {
                    graphics.setColour(juce::Colour(0x0dffffff));
                    graphics.fillRect(cell);
                }
                if (step == active)
                {
                    graphics.setColour(accent.withAlpha(0.22f));
                    graphics.fillRect(cell);
                }
                const float displayed = displayedMagnitude(
                    values[static_cast<size_t>(step)]);
                graphics.setColour(accent.withAlpha(
                    step == active ? 0.95f : 0.68f));
                if (isBipolar)
                {
                    const float y = graph.getBottom()
                                  - graph.getHeight() * displayed;
                    graphics.fillRect(x + 1.0f,
                        juce::jmin(y, graph.getCentreY()),
                        juce::jmax(1.0f, columnWidth - 2.0f),
                        std::abs(y - graph.getCentreY()));
                }
                else
                {
                    const float y = displayY(values[static_cast<size_t>(step)]);
                    graphics.fillRect(x + 1.0f,
                        isNegative ? graph.getY() : y,
                        juce::jmax(1.0f, columnWidth - 2.0f),
                        juce::jmax(0.0f, graph.getHeight() * displayed));
                }
                graphics.setColour(juce::Colour(0x553b4650));
                graphics.drawVerticalLine(juce::roundToInt(x + columnWidth),
                                           graph.getY(), graph.getBottom());
                if (step % 4 == 0)
                {
                    graphics.setColour(knobLabelColour);
                    graphics.drawText(juce::String(step + 1),
                        juce::roundToInt(x), 0,
                        juce::roundToInt(columnWidth * 4.0f), 12,
                        juce::Justification::centredLeft);
                }
            }

            const float attackValue = juce::jlimit(
                0.0f, 1.0f, attack != nullptr ? attack->load() : 0.0f);
            const float releaseValue = juce::jlimit(
                0.0f, 1.0f, release != nullptr ? release->load() : 0.0f);
            juce::Path envelope;
            constexpr int pointsPerStep = 16;
            for (int step = 0; step < dusk::sequencer::stepCount; ++step)
            {
                const int previous = (step - 1
                    + dusk::sequencer::stepCount)
                    % dusk::sequencer::stepCount;
                for (int point = 0; point <= pointsPerStep; ++point)
                {
                    const float position = static_cast<float>(point)
                                         / static_cast<float>(pointsPerStep);
                    const float value = dusk::sequencer::transition(
                        values[static_cast<size_t>(previous)],
                        values[static_cast<size_t>(step)], position,
                        attackValue, releaseValue);
                    const float x = graph.getX() + columnWidth
                        * (static_cast<float>(step) + position);
                    const float y = displayY(value);
                    if (step == 0 && point == 0)
                        envelope.startNewSubPath(x, y);
                    else
                        envelope.lineTo(x, y);
                }
            }
            graphics.setColour(juce::Colour(0xb0000000));
            graphics.strokePath(envelope, juce::PathStrokeType(3.2f));
            graphics.setColour(accent.brighter(0.50f));
            graphics.strokePath(envelope, juce::PathStrokeType(1.35f));
            for (int step = 0; step < dusk::sequencer::stepCount; ++step)
            {
                if (step >= first && step <= last)
                    continue;
                const float x = graph.getX() + columnWidth * step;
                graphics.setColour(juce::Colour(0xb0101419));
                graphics.fillRect(x, graph.getY(), columnWidth,
                                  graph.getHeight());
            }
            graphics.setColour(accent.withAlpha(0.72f));
            graphics.drawRoundedRectangle(graph, 4.0f, 1.1f);

            const auto gateRow = gateBounds();
            const float gateColumnWidth = gateRow.getWidth()
                                        / dusk::sequencer::stepCount;
            for (int step = 0; step < dusk::sequencer::stepCount; ++step)
            {
                const auto index = static_cast<size_t>(step);
                const float x = gateRow.getX() + gateColumnWidth * step;
                const auto cell = juce::Rectangle<float>(
                    x + 1.0f, gateRow.getY(),
                    juce::jmax(1.0f, gateColumnWidth - 2.0f),
                    gateRow.getHeight());
                const auto mode = gateSteps[index] != nullptr
                    ? dusk::sequencer::gateStepModeFromChoice(
                        gateSteps[index]->convertFrom0to1(
                            gateSteps[index]->getValue()))
                    : dusk::sequencer::GateStepMode::rest;
                const auto cellAccent = accent.withAlpha(
                    step == active ? 0.95f : 0.72f);

                graphics.setColour(juce::Colour(0xff111820));
                graphics.fillRoundedRectangle(cell, 2.0f);
                if (mode == dusk::sequencer::GateStepMode::shortStep)
                {
                    auto fill = cell.reduced(2.0f);
                    fill.setWidth(fill.getWidth() * 0.5f);
                    graphics.setColour(cellAccent);
                    graphics.fillRoundedRectangle(fill, 1.5f);
                }
                else if (mode == dusk::sequencer::GateStepMode::longStep
                         || mode == dusk::sequencer::GateStepMode::rest)
                {
                    graphics.setColour(cellAccent);
                    graphics.fillRoundedRectangle(cell.reduced(2.0f), 1.5f);
                    if (mode == dusk::sequencer::GateStepMode::rest)
                    {
                        const auto centre = cell.getCentre();
                        const float halfWidth = cell.getWidth() * 0.22f;
                        const float halfHeight = cell.getHeight() * 0.19f;
                        juce::Path arrow;
                        arrow.startNewSubPath(centre.x - halfWidth, centre.y);
                        arrow.lineTo(centre.x + halfWidth, centre.y);
                        arrow.startNewSubPath(centre.x + halfWidth, centre.y);
                        arrow.lineTo(centre.x + halfWidth - halfHeight,
                                     centre.y - halfHeight);
                        arrow.startNewSubPath(centre.x + halfWidth, centre.y);
                        arrow.lineTo(centre.x + halfWidth - halfHeight,
                                     centre.y + halfHeight);
                        graphics.setColour(juce::Colour(0xff10151a)
                                               .withAlpha(0.88f));
                        graphics.strokePath(
                            arrow, juce::PathStrokeType(1.25f));
                    }
                }

                graphics.setColour(
                    mode == dusk::sequencer::GateStepMode::off
                        ? knobTrackColour : cellAccent.brighter(0.18f));
                graphics.drawRoundedRectangle(
                    cell, 2.0f, step == active ? 1.4f : 0.8f);
                if (step < first || step > last)
                {
                    graphics.setColour(juce::Colour(0xb0101419));
                    graphics.fillRoundedRectangle(cell, 2.0f);
                }
            }
        }

        void mouseDown(const juce::MouseEvent& event) override
        {
            if (event.mods.isPopupMenu())
            {
                if (gateBounds().contains(event.position))
                    showGateMenu();
                else
                    showPatternMenu();
                return;
            }
            lastEditedCell = -1;
            editingGateRow = gateBounds().contains(event.position);
            if (editingGateRow)
            {
                cycleGateMode(event.position);
                return;
            }
            editAt(event.position);
        }

        void mouseDrag(const juce::MouseEvent& event) override
        {
            if (editingGateRow)
                return;
            editAt(event.position);
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            lastEditedCell = -1;
            editingGateRow = false;
        }

        void applyWaveform(dusk::sequencer::Waveform waveform)
        {
            const bool isBipolar = polarityMode == 1;
            const auto values = dusk::sequencer::makeWaveform(
                waveform, isBipolar);
            for (int step = 0; step < dusk::sequencer::stepCount; ++step)
                setParameterActualValue(steps[static_cast<size_t>(step)],
                                        values[static_cast<size_t>(step)]);
            repaint();
        }

        void nudge(int direction)
        {
            std::array<float, dusk::sequencer::stepCount> values {};
            for (int step = 0; step < dusk::sequencer::stepCount; ++step)
            {
                const auto index = static_cast<size_t>(step);
                values[index] = steps[index]->convertFrom0to1(
                    steps[index]->getValue());
            }
            for (int step = 0; step < dusk::sequencer::stepCount; ++step)
            {
                const int source = (step - direction
                    + dusk::sequencer::stepCount)
                    % dusk::sequencer::stepCount;
                setParameterActualValue(steps[static_cast<size_t>(step)],
                                        values[static_cast<size_t>(source)]);
            }
            repaint();
        }

        void nudgeGates(int direction)
        {
            std::array<float, dusk::sequencer::stepCount> values {};
            for (int step = 0; step < dusk::sequencer::stepCount; ++step)
            {
                const auto index = static_cast<size_t>(step);
                values[index] = gateSteps[index]->convertFrom0to1(
                    gateSteps[index]->getValue());
            }
            for (int step = 0; step < dusk::sequencer::stepCount; ++step)
            {
                const int source = (step - direction
                    + dusk::sequencer::stepCount)
                    % dusk::sequencer::stepCount;
                setParameterActualValue(gateSteps[static_cast<size_t>(step)],
                                        values[static_cast<size_t>(source)]);
            }
            repaint();
        }

        std::function<void()> onSaveRequested;

    private:
        juce::Rectangle<float> graphBounds() const
        {
            const auto gate = gateBounds();
            auto graph = getLocalBounds().toFloat().reduced(5.0f)
                .withTrimmedTop(12.0f);
            graph.setBottom(gate.getY() - 4.0f);
            return graph;
        }

        juce::Rectangle<float> gateBounds() const
        {
            const auto inner = getLocalBounds().toFloat().reduced(5.0f);
            const float columnWidth = inner.getWidth()
                                    / dusk::sequencer::stepCount;
            const float height = juce::jmax(
                8.0f, std::floor(columnWidth * 0.60f));
            return { inner.getX(), inner.getBottom() - height,
                     inner.getWidth(), height };
        }

        void cycleGateMode(juce::Point<float> position)
        {
            const auto row = gateBounds();
            const int step = juce::jlimit(
                0, dusk::sequencer::stepCount - 1,
                static_cast<int>(std::floor(
                    (position.x - row.getX()) / row.getWidth()
                        * dusk::sequencer::stepCount)));
            auto* parameter = gateSteps[static_cast<size_t>(step)];
            if (parameter == nullptr)
                return;
            const int current = static_cast<int>(
                dusk::sequencer::gateStepModeFromChoice(
                    parameter->convertFrom0to1(parameter->getValue())));
            const int next = (current + 1)
                % (static_cast<int>(
                    dusk::sequencer::GateStepMode::rest) + 1);
            setParameterActualValue(parameter, static_cast<float>(next));
            repaint();
        }

        void editAt(juce::Point<float> position)
        {
            const auto graph = graphBounds();
            if (! graph.contains(position))
                return;
            const float column = (position.x - graph.getX()) / graph.getWidth()
                               * dusk::sequencer::stepCount;
            const int step = juce::jlimit(0,
                dusk::sequencer::stepCount - 1,
                static_cast<int>(std::floor(column)));
            const bool isBipolar = polarityMode == 1;
            const bool isNegative = polarityMode == 2;
            const float displayed = isBipolar
                ? juce::jlimit(0.0f, 1.0f,
                    (graph.getBottom() - position.y) / graph.getHeight())
                : juce::jlimit(0.0f, 1.0f,
                    (isNegative ? position.y - graph.getY()
                                : graph.getBottom() - position.y)
                        / graph.getHeight());
            const float canonical = isBipolar
                ? displayed : 0.5f + 0.5f * displayed;
            setParameterActualValue(steps[static_cast<size_t>(step)],
                                    canonical);
            lastEditedCell = step;
            repaint();
        }

        void showPatternMenu()
        {
            compactPopupLookAndFeel.setPopupBorderColour(
                sharedPopupLookAndFeel.getPopupBorderColour());
            juce::PopupMenu menu;
            menu.setLookAndFeel(&compactPopupLookAndFeel);
            menu.addItem(1, "Zero");
            menu.addItem(2, "Max");
            menu.addItem(3, "Min");
            menu.addItem(4, "Random");
            menu.addSeparator();
            menu.addItem(5, "Reset");
            menu.addItem(6, "Save Sequence...");
            menu.addSeparator();
            menu.addItem(7, "Copy");
            menu.addItem(8, "Paste", clipboardValid);
            const juce::Component::SafePointer<StepGrid> safeThis(this);
            menu.showMenuAsync(
                juce::PopupMenu::Options()
                    .withTargetComponent(this)
                    .withMinimumWidth(128)
                    .withStandardItemHeight(20),
                [safeThis](int result)
                {
                    if (safeThis == nullptr || result == 0)
                        return;
                    auto& grid = *safeThis;
                    const bool isBipolar = grid.polarityMode == 1;
                    if (result >= 1 && result <= 4)
                    {
                        juce::Random random;
                        for (auto* parameter : grid.steps)
                        {
                            const float value = result == 1 ? 0.5f
                                : result == 2 ? 1.0f
                                : result == 3 ? (isBipolar ? 0.0f : 0.5f)
                                : (isBipolar ? random.nextFloat()
                                             : 0.5f
                                                 + 0.5f * random.nextFloat());
                            setParameterActualValue(parameter, value);
                        }
                    }
                    else if (result == 5)
                    {
                        for (int step = 0;
                             step < dusk::sequencer::stepCount; ++step)
                        {
                            auto* parameter = grid.steps[static_cast<size_t>(step)];
                            const auto saved = grid.processor.getSavedParameterValue(
                                dusk::sequencer::stepParameterId(step));
                            const float value = saved.has_value()
                                ? *saved
                                : parameter->convertFrom0to1(
                                      parameter->getDefaultValue());
                            setParameterActualValue(parameter, value);
                        }
                    }
                    else if (result == 6
                             && grid.onSaveRequested != nullptr)
                    {
                        grid.onSaveRequested();
                    }
                    else if (result == 7)
                    {
                        for (size_t index = 0; index < grid.steps.size(); ++index)
                            clipboard[index]
                                = grid.steps[index]->convertFrom0to1(
                                    grid.steps[index]->getValue());
                        clipboardValid = true;
                    }
                    else if (result == 8 && clipboardValid)
                    {
                        for (size_t index = 0; index < grid.steps.size(); ++index)
                            setParameterActualValue(grid.steps[index],
                                                    clipboard[index]);
                    }
                    grid.repaint();
                });
        }

        void showGateMenu()
        {
            compactPopupLookAndFeel.setPopupBorderColour(
                sharedPopupLookAndFeel.getPopupBorderColour());
            juce::PopupMenu menu;
            menu.setLookAndFeel(&compactPopupLookAndFeel);
            menu.addItem(1, "Short");
            menu.addItem(2, "Long");
            menu.addItem(3, "Rest");
            menu.addItem(4, "Off");
            menu.addItem(5, "Random");
            menu.addSeparator();
            menu.addItem(6, "Reset");
            menu.addSeparator();
            menu.addItem(7, "Copy");
            menu.addItem(8, "Paste", gateClipboardValid);
            const juce::Component::SafePointer<StepGrid> safeThis(this);
            menu.showMenuAsync(
                juce::PopupMenu::Options()
                    .withTargetComponent(this)
                    .withMinimumWidth(128)
                    .withStandardItemHeight(20),
                [safeThis](int result)
                {
                    if (safeThis == nullptr || result == 0)
                        return;
                    auto& grid = *safeThis;
                    if (result >= 1 && result <= 4)
                    {
                        const std::array<dusk::sequencer::GateStepMode, 4>
                            modes
                            {
                                dusk::sequencer::GateStepMode::shortStep,
                                dusk::sequencer::GateStepMode::longStep,
                                dusk::sequencer::GateStepMode::rest,
                                dusk::sequencer::GateStepMode::off
                            };
                        const auto mode = modes[static_cast<size_t>(result - 1)];
                        for (auto* parameter : grid.gateSteps)
                            setParameterActualValue(
                                parameter, static_cast<float>(mode));
                    }
                    else if (result == 5)
                    {
                        juce::Random random;
                        for (auto* parameter : grid.gateSteps)
                            setParameterActualValue(parameter,
                                static_cast<float>(random.nextInt(4)));
                    }
                    else if (result == 6)
                    {
                        for (int step = 0;
                             step < dusk::sequencer::stepCount; ++step)
                        {
                            auto* parameter = grid.gateSteps[
                                static_cast<size_t>(step)];
                            const auto saved = grid.processor
                                .getSavedParameterValue(
                                    dusk::sequencer::gateStepParameterId(
                                        step));
                            const float value = saved.has_value()
                                ? *saved
                                : parameter->convertFrom0to1(
                                      parameter->getDefaultValue());
                            setParameterActualValue(parameter, value);
                        }
                    }
                    else if (result == 7)
                    {
                        for (size_t index = 0;
                             index < grid.gateSteps.size(); ++index)
                            gateClipboard[index] = grid.gateSteps[index]
                                ->convertFrom0to1(
                                    grid.gateSteps[index]->getValue());
                        gateClipboardValid = true;
                    }
                    else if (result == 8 && gateClipboardValid)
                    {
                        for (size_t index = 0;
                             index < grid.gateSteps.size(); ++index)
                            setParameterActualValue(grid.gateSteps[index],
                                                    gateClipboard[index]);
                    }
                    grid.repaint();
                });
        }

        DuskAudioProcessor& processor;
        LookAndFeel& sharedPopupLookAndFeel;
        LookAndFeel& compactPopupLookAndFeel;
        std::array<juce::RangedAudioParameter*,
                   dusk::sequencer::stepCount> steps {};
        std::array<juce::RangedAudioParameter*,
                   dusk::sequencer::stepCount> gateSteps {};
        std::atomic<float>* start = nullptr;
        std::atomic<float>* end = nullptr;
        std::atomic<float>* peak = nullptr;
        std::atomic<float>* move = nullptr;
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* release = nullptr;
        juce::Colour accent { controlAccentColour };
        int polarityMode = 0;
        int lastEditedCell = -1;
        bool editingGateRow = false;
        inline static std::array<float,
                                 dusk::sequencer::stepCount> clipboard {};
        inline static bool clipboardValid = false;
        inline static std::array<float,
                                 dusk::sequencer::stepCount>
            gateClipboard {};
        inline static bool gateClipboardValid = false;
    };

    class TargetList final : public juce::Component,
                             public juce::DragAndDropTarget,
                             public juce::SettableTooltipClient
    {
    public:
        TargetList(DuskAudioProcessor& audioProcessor,
                   std::function<void(const juce::String&)> helpCallback)
            : processor(audioProcessor), showHelp(std::move(helpCallback))
        {
            for (int slot = 0; slot < dusk::sequencer::targetSlotCount;
                 ++slot)
            {
                choices[static_cast<size_t>(slot)]
                    = processor.parameters.getParameter(
                        dusk::sequencer::targetParameterId(slot));
                enabled[static_cast<size_t>(slot)]
                    = processor.parameters.getParameter(
                        dusk::sequencer::targetEnabledParameterId(slot));
                depths[static_cast<size_t>(slot)]
                    = processor.parameters.getParameter(
                        dusk::sequencer::targetDepthParameterId(slot));
            }
            setTooltip("Drop a Dusk knob label here. The small knob sets that target's individual modulation depth.");
        }

        bool isInterestedInDragSource(const SourceDetails& details) override
        {
            const auto description = details.description.toString();
            return description.startsWith(sequencerTargetDragPrefix)
                && dusk::sequencer::targetChoiceForParameter(
                    description.fromFirstOccurrenceOf(
                        sequencerTargetDragPrefix, false, false)) > 0;
        }

        void itemDragEnter(const SourceDetails&) override
        {
            dragOver = true;
            repaint();
        }

        void itemDragExit(const SourceDetails&) override
        {
            dragOver = false;
            repaint();
        }

        void itemDropped(const SourceDetails& details) override
        {
            dragOver = false;
            const int requested = dusk::sequencer::targetChoiceForParameter(
                details.description.toString().fromFirstOccurrenceOf(
                    sequencerTargetDragPrefix, false, false));
            if (requested <= 0)
                return;
            int freeSlot = -1;
            for (int slot = 0; slot < dusk::sequencer::targetSlotCount;
                 ++slot)
            {
                const int existing = choiceForSlot(slot);
                if (existing == requested)
                {
                    setParameterActualValue(enabled[static_cast<size_t>(slot)], 1.0f);
                    repaint();
                    return;
                }
                if (existing == 0 && freeSlot < 0)
                    freeSlot = slot;
            }
            if (freeSlot >= 0)
            {
                setParameterActualValue(choices[static_cast<size_t>(freeSlot)],
                                        static_cast<float>(requested));
                setParameterActualValue(enabled[static_cast<size_t>(freeSlot)],
                                        1.0f);
            }
            repaint();
        }

        void paint(juce::Graphics& graphics) override
        {
            graphics.setColour(juce::Colour(0xff10151a));
            graphics.fillRoundedRectangle(getLocalBounds().toFloat(), 5.0f);
            graphics.setColour(dragOver
                ? controlAccentColour.brighter(0.25f)
                : controlAccentColour.withAlpha(0.65f));
            graphics.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f),
                                           5.0f, dragOver ? 2.0f : 1.0f);
            graphics.setColour(knobLabelColour);
            graphics.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            graphics.drawText("TARGETS", 5, 3, getWidth() - 10, 14,
                              juce::Justification::centred);
            graphics.setColour(knobIndicatorColour);
            graphics.setFont(juce::FontOptions(12.5f, juce::Font::bold));
            graphics.drawText("!!!", randomButtonBounds(),
                              juce::Justification::centred, false);
            const auto slots = assignedSlots();
            const auto depthColour = filterColourForId(1, 0);
            if (slots.empty())
            {
                graphics.setColour(knobLabelColour.darker(0.25f));
                graphics.setFont(juce::FontOptions(10.5f));
                graphics.drawFittedText("DROP PARAMETER LABELS HERE",
                    8, 28, getWidth() - 16, getHeight() - 36,
                    juce::Justification::centred, 2);
                return;
            }
            const int visibleRows = juce::jmax(1, (getHeight() - 20) / rowHeight);
            const int maximumFirst = juce::jmax(
                0, static_cast<int>(slots.size()) - visibleRows);
            firstVisibleRow = juce::jlimit(0, maximumFirst, firstVisibleRow);
            for (int visibleRow = 0;
                 visibleRow < visibleRows
                    && firstVisibleRow + visibleRow
                        < static_cast<int>(slots.size());
                 ++visibleRow)
            {
                const int slot = slots[static_cast<size_t>(
                    firstVisibleRow + visibleRow)];
                const int y = 20 + visibleRow * rowHeight;
                const bool isEnabled = enabledForSlot(slot);
                graphics.setColour(isEnabled
                                             ? controlAccentColour.withAlpha(0.18f)
                                             : juce::Colour(0xff171c21));
                graphics.fillRoundedRectangle(4.0f, static_cast<float>(y),
                    static_cast<float>(getWidth() - 8),
                    static_cast<float>(rowHeight - 2), 2.5f);
                graphics.setColour(isEnabled ? controlAccentColour
                                             : knobTrackColour);
                graphics.fillEllipse(8.0f, static_cast<float>(y + 6), 6.0f, 6.0f);

                const auto knob = depthBoundsForVisibleRow(visibleRow).toFloat();
                const float rowAlpha = isEnabled ? 1.0f : 0.42f;
                graphics.setColour(juce::Colour(0xff090c0f).withMultipliedAlpha(
                    rowAlpha));
                graphics.fillEllipse(knob);
                graphics.setColour(depthColour.withAlpha(
                    0.42f * rowAlpha));
                graphics.drawEllipse(knob.reduced(0.5f), 1.0f);
                const float depth = depthForSlot(slot);
                if (depth > 0.0001f)
                {
                    constexpr float startAngle =
                        juce::MathConstants<float>::pi * 1.20f;
                    constexpr float arcLength =
                        juce::MathConstants<float>::pi * 1.60f;
                    juce::Path arc;
                    const auto arcBounds = knob.reduced(2.5f);
                    arc.addCentredArc(
                        arcBounds.getCentreX(), arcBounds.getCentreY(),
                        arcBounds.getWidth() * 0.5f,
                        arcBounds.getHeight() * 0.5f, 0.0f,
                        startAngle, startAngle + arcLength * depth, true);
                    graphics.setColour(depthColour.withAlpha(
                        0.90f * rowAlpha));
                    graphics.strokePath(
                        arc, juce::PathStrokeType(
                            1.7f, juce::PathStrokeType::curved,
                            juce::PathStrokeType::rounded));
                }

                graphics.setColour(isEnabled ? knobValueColour
                                             : knobLabelColour.darker(0.35f));
                graphics.setFont(juce::FontOptions(10.5f,
                    isEnabled ? juce::Font::bold : juce::Font::plain));
                graphics.drawFittedText(dusk::sequencer::targetName(
                        choiceForSlot(slot)), 38, y + 1, getWidth() - 61,
                    rowHeight - 4, juce::Justification::centredLeft, 1);
                graphics.setColour(knobLabelColour);
                const float closeX = static_cast<float>(getWidth() - 12);
                const float closeY = static_cast<float>(y)
                                   + 0.5f * static_cast<float>(rowHeight - 2);
                graphics.drawLine(closeX - 3.0f, closeY - 3.0f,
                                  closeX + 3.0f, closeY + 3.0f, 1.25f);
                graphics.drawLine(closeX + 3.0f, closeY - 3.0f,
                                  closeX - 3.0f, closeY + 3.0f, 1.25f);
            }
            if (canScrollUp())
            {
                graphics.setColour(controlAccentColour);
                const auto bounds = scrollUpBounds().toFloat();
                const auto centre = bounds.getCentre();
                graphics.drawLine(centre.x - 4.0f, centre.y + 2.0f,
                                  centre.x, centre.y - 2.0f, 1.6f);
                graphics.drawLine(centre.x, centre.y - 2.0f,
                                  centre.x + 4.0f, centre.y + 2.0f, 1.6f);
            }
            if (canScrollDown())
            {
                graphics.setColour(controlAccentColour);
                const auto bounds = scrollDownBounds().toFloat();
                const auto centre = bounds.getCentre();
                graphics.drawLine(centre.x - 4.0f, centre.y - 2.0f,
                                  centre.x, centre.y + 2.0f, 1.6f);
                graphics.drawLine(centre.x, centre.y + 2.0f,
                                  centre.x + 4.0f, centre.y - 2.0f, 1.6f);
            }
        }

        void mouseDown(const juce::MouseEvent& event) override
        {
            if (randomButtonBounds().contains(event.getPosition()))
            {
                randomiseTargets();
                return;
            }
            if (canScrollUp() && scrollUpBounds().contains(event.getPosition()))
            {
                --firstVisibleRow;
                repaint();
                return;
            }
            if (canScrollDown()
                && scrollDownBounds().contains(event.getPosition()))
            {
                ++firstVisibleRow;
                repaint();
                return;
            }
            if (event.y < 20)
                return;
            const int row = (event.y - 20) / rowHeight + firstVisibleRow;
            const auto slots = assignedSlots();
            if (! juce::isPositiveAndBelow(row,
                                           static_cast<int>(slots.size())))
                return;
            const int slot = slots[static_cast<size_t>(row)];
            const int visibleRow = row - firstVisibleRow;
            if (depthBoundsForVisibleRow(visibleRow).contains(event.getPosition()))
            {
                draggedDepthParameter = depths[static_cast<size_t>(slot)];
                if (draggedDepthParameter != nullptr)
                {
                    dragStartDepth = depthForSlot(slot);
                    dragStartY = event.y;
                    draggedDepthParameter->beginChangeGesture();
                }
            }
            else if (event.x >= getWidth() - 24)
                setParameterActualValue(choices[static_cast<size_t>(slot)], 0.0f);
            else
                setParameterActualValue(enabled[static_cast<size_t>(slot)],
                                        enabledForSlot(slot) ? 0.0f : 1.0f);
            repaint();
        }

        void mouseDrag(const juce::MouseEvent& event) override
        {
            if (draggedDepthParameter == nullptr)
                return;

            const float sensitivity = event.mods.isShiftDown() ? 360.0f
                                                                : 120.0f;
            const float depth = juce::jlimit(
                0.0f, 1.0f,
                dragStartDepth
                    + static_cast<float>(dragStartY - event.y) / sensitivity);
            draggedDepthParameter->setValueNotifyingHost(
                draggedDepthParameter->convertTo0to1(depth));
            setDepthTooltip(depth);
            repaint();
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            if (draggedDepthParameter != nullptr)
            {
                draggedDepthParameter->endChangeGesture();
                draggedDepthParameter = nullptr;
            }
        }

        void mouseDoubleClick(const juce::MouseEvent& event) override
        {
            const int row = (event.y - 20) / rowHeight + firstVisibleRow;
            const auto slots = assignedSlots();
            if (event.y < 20
                || ! juce::isPositiveAndBelow(
                    row, static_cast<int>(slots.size())))
                return;

            const int visibleRow = row - firstVisibleRow;
            if (! depthBoundsForVisibleRow(visibleRow).contains(
                    event.getPosition()))
                return;

            auto* parameter = depths[static_cast<size_t>(
                slots[static_cast<size_t>(row)])];
            if (parameter == nullptr)
                return;
            if (draggedDepthParameter != nullptr)
            {
                draggedDepthParameter->endChangeGesture();
                draggedDepthParameter = nullptr;
            }
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
            parameter->endChangeGesture();
            setDepthTooltip(parameter->convertFrom0to1(
                parameter->getValue()));
            repaint();
        }

        void mouseMove(const juce::MouseEvent& event) override
        {
            if (randomButtonBounds().contains(event.getPosition()))
            {
                setMouseCursor(juce::MouseCursor::PointingHandCursor);
                const auto tooltip =
                    "Randomise target assignments, enabled states and individual Depths";
                setTooltip(tooltip);
                if (showHelp != nullptr)
                    showHelp(contextHelpText(
                        "Randomise Targets", tooltip));
                return;
            }
            if ((canScrollUp()
                    && scrollUpBounds().contains(event.getPosition()))
                || (canScrollDown()
                    && scrollDownBounds().contains(event.getPosition())))
            {
                setMouseCursor(juce::MouseCursor::PointingHandCursor);
                const bool upwards = scrollUpBounds().contains(
                    event.getPosition());
                const auto tooltip = upwards ? "Show earlier targets"
                                             : "Show later targets";
                setTooltip(tooltip);
                if (showHelp != nullptr)
                    showHelp(contextHelpText("Targets", tooltip));
                return;
            }

            const int row = (event.y - 20) / rowHeight + firstVisibleRow;
            const auto slots = assignedSlots();
            const int visibleRow = row - firstVisibleRow;
            if (event.y >= 20
                && juce::isPositiveAndBelow(
                    row, static_cast<int>(slots.size()))
                && depthBoundsForVisibleRow(visibleRow).contains(
                    event.getPosition()))
            {
                setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
                setDepthTooltip(depthForSlot(
                    slots[static_cast<size_t>(row)]));
            }
            else
            {
                setMouseCursor(juce::MouseCursor::NormalCursor);
                setTooltip("Drop a Dusk knob label here. The small knob sets that target's individual modulation depth.");
            }
        }

        void mouseWheelMove(const juce::MouseEvent& event,
                            const juce::MouseWheelDetails& wheel) override
        {
            const auto slots = assignedSlots();
            const int row = (event.y - 20) / rowHeight + firstVisibleRow;
            const int visibleRow = row - firstVisibleRow;
            if (event.y >= 20
                && juce::isPositiveAndBelow(
                    row, static_cast<int>(slots.size()))
                && depthBoundsForVisibleRow(visibleRow).contains(
                    event.getPosition()))
            {
                auto* parameter = depths[static_cast<size_t>(
                    slots[static_cast<size_t>(row)])];
                if (parameter != nullptr)
                {
                    const float depth = juce::jlimit(
                        0.0f, 1.0f,
                        parameter->convertFrom0to1(parameter->getValue())
                            + wheel.deltaY * 0.12f);
                    parameter->beginChangeGesture();
                    parameter->setValueNotifyingHost(
                        parameter->convertTo0to1(depth));
                    parameter->endChangeGesture();
                    setDepthTooltip(depth);
                    repaint();
                }
                return;
            }

            const int visibleRows = juce::jmax(1, (getHeight() - 20) / rowHeight);
            const int maximumFirst = juce::jmax(
                0, static_cast<int>(slots.size()) - visibleRows);
            firstVisibleRow = juce::jlimit(
                0, maximumFirst,
                firstVisibleRow + (wheel.deltaY > 0.0f ? -1 : 1));
            repaint();
        }

    private:
        void randomiseTargets()
        {
            juce::Random random;
            const auto randomise = [&random](juce::RangedAudioParameter* parameter)
            {
                if (parameter == nullptr)
                    return;
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(random.nextFloat());
                parameter->endChangeGesture();
            };

            for (int slot = 0; slot < dusk::sequencer::targetSlotCount;
                 ++slot)
            {
                const auto index = static_cast<size_t>(slot);
                randomise(choices[index]);
                randomise(enabled[index]);
                randomise(depths[index]);
            }

            firstVisibleRow = 0;
            repaint();
            if (showHelp != nullptr)
                showHelp(contextHelpText(
                    "Randomise Targets",
                    "Target assignments, enabled states and individual Depths were randomised."));
        }

        juce::Rectangle<int> randomButtonBounds() const
        {
            return { 0, 2, 24, 16 };
        }

        int choiceForSlot(int slot) const
        {
            auto* parameter = choices[static_cast<size_t>(slot)];
            return parameter != nullptr ? juce::roundToInt(
                parameter->convertFrom0to1(parameter->getValue())) : 0;
        }

        bool enabledForSlot(int slot) const
        {
            auto* parameter = enabled[static_cast<size_t>(slot)];
            return parameter != nullptr
                && parameter->convertFrom0to1(parameter->getValue()) >= 0.5f;
        }

        float depthForSlot(int slot) const
        {
            auto* parameter = depths[static_cast<size_t>(slot)];
            return parameter != nullptr
                ? juce::jlimit(0.0f, 1.0f,
                    parameter->convertFrom0to1(parameter->getValue()))
                : 1.0f;
        }

        juce::Rectangle<int> depthBoundsForVisibleRow(int visibleRow) const
        {
            return { 18, 21 + visibleRow * rowHeight, 16, 16 };
        }

        void setDepthTooltip(float depth)
        {
            const auto tooltip = "Target Depth: modulation amount ("
                + juce::String(juce::roundToInt(depth * 100.0f)) + "%)";
            setTooltip(tooltip);
            if (showHelp != nullptr)
                showHelp(contextHelpText("Target Depth", tooltip));
        }

        std::vector<int> assignedSlots() const
        {
            std::vector<int> result;
            for (int slot = 0; slot < dusk::sequencer::targetSlotCount;
                 ++slot)
                if (choiceForSlot(slot) > 0)
                    result.push_back(slot);
            return result;
        }

        int visibleRowCount() const
        {
            return juce::jmax(1, (getHeight() - 20) / rowHeight);
        }

        bool canScrollUp() const
        {
            return firstVisibleRow > 0;
        }

        bool canScrollDown() const
        {
            return firstVisibleRow + visibleRowCount()
                < static_cast<int>(assignedSlots().size());
        }

        juce::Rectangle<int> scrollUpBounds() const
        {
            return { getWidth() - 22, 1, 20, 18 };
        }

        juce::Rectangle<int> scrollDownBounds() const
        {
            return { getWidth() - 22, getHeight() - 14, 20, 13 };
        }

        DuskAudioProcessor& processor;
        std::function<void(const juce::String&)> showHelp;
        std::array<juce::RangedAudioParameter*,
                   dusk::sequencer::targetSlotCount> choices {};
        std::array<juce::RangedAudioParameter*,
                   dusk::sequencer::targetSlotCount> enabled {};
        std::array<juce::RangedAudioParameter*,
                   dusk::sequencer::targetSlotCount> depths {};
        juce::RangedAudioParameter* draggedDepthParameter = nullptr;
        float dragStartDepth = 1.0f;
        int dragStartY = 0;
        bool dragOver = false;
        int firstVisibleRow = 0;
        static constexpr int rowHeight = 20;
    };

    SequencerPanel(DuskAudioProcessor& audioProcessor,
                   LookAndFeel& sharedLookAndFeel,
                   std::function<juce::Colour()> surroundColourProvider,
                   std::function<void(const juce::String&)> helpCallback)
        : processor(audioProcessor),
          surroundColour(std::move(surroundColourProvider)),
          showHelp(std::move(helpCallback)),
          grid(audioProcessor, sharedLookAndFeel, patternMenuLookAndFeel),
          targets(audioProcessor, showHelp)
    {
        setLookAndFeel(&sharedLookAndFeel);
        addMouseListener(this, true);
        addAndMakeVisible(grid);
        addAndMakeVisible(targets);
        const std::array<juce::Component*, 33> components
        {{ &hostSync, &enabledButton, &bipolarButton,
           &animateButton, &gateModeButton,
           &waveformBox, &userSequenceBox, &directionBox,
           &rateSlider, &startSlider, &endSlider, &attackSlider,
           &releaseSlider, &peakSlider, &moveSlider, &colourSlider,
           &saturationSlider, &luminanceSlider,
           &startLabel, &endLabel, &attackLabel, &releaseLabel,
           &peakLabel, &moveLabel, &shortGateLabel, &longGateLabel,
           &nudgeLeft, &nudgeRight, &gateNudgeLeft, &gateNudgeRight,
           &rangeLinkButton,
           &shortGateSlider, &longGateSlider }};
        for (auto* component : components)
            addAndMakeVisible(component);

        hostSync.setButtonText("HOST SYNC");
        enabledButton.setButtonText("ON");
        bipolarButton.setButtonText("UNIPOLAR +");
        animateButton.setButtonText("ANIMATE");
        hostSync.setName("Host Sync");
        enabledButton.setName("Sequencer On");
        bipolarButton.setName("Sequencer Polarity");
        animateButton.setName("Animate");
        gateModeButton.setName("Gate Mode");
        waveformBox.setName("Waveform");
        userSequenceBox.setName("User Sequence");
        directionBox.setName("Direction");
        rateSlider.setName("Rate");
        startSlider.setName("Start");
        endSlider.setName("End");
        rangeLinkButton.setName("Start / End Link");
        colourSlider.setName("Hue");
        saturationSlider.setName("Saturation");
        luminanceSlider.setName("Luminance");
        attackSlider.setName("Attack");
        releaseSlider.setName("Release");
        peakSlider.setName("Depth");
        moveSlider.setName("Offset");
        shortGateSlider.setName("Short");
        longGateSlider.setName("Long");
        grid.setName("Steps");
        targets.setName("Targets");
        for (auto* button : { &hostSync, &enabledButton, &bipolarButton,
                              &animateButton })
        {
            button->setClickingTogglesState(true);
            button->setColour(juce::ToggleButton::textColourId,
                              knobLabelColour);
            button->getProperties().set("activeColour",
                                        controlAccentColour.toString());
            button->getProperties().set("sequencerLabelStyle", true);
        }
        hostSync.getProperties().set("activeColour",
                                     tempoSyncLedColour.toString());
        enabledButton.getProperties().set("activeColour",
                                          linkLedColour.toString());
        bipolarButton.setClickingTogglesState(false);
        bipolarButton.setToggleState(true, juce::dontSendNotification);
        bipolarButton.getProperties().set("fixedLedCentreX", 9.0f);
        gateModeButton.setClickingTogglesState(false);
        gateModeButton.setToggleState(true, juce::dontSendNotification);
        gateModeButton.getProperties().set("fixedLedCentreX", 9.0f);
        gateModeButton.setColour(juce::ToggleButton::textColourId,
                                 knobLabelColour);
        gateModeButton.getProperties().set("sequencerLabelStyle", true);
        nudgeLeft.setName("Nudge pattern left");
        nudgeRight.setName("Nudge pattern right");
        gateNudgeLeft.setName("Nudge Gates left");
        gateNudgeRight.setName("Nudge Gates right");
        for (auto* button : { &nudgeLeft, &nudgeRight,
                              &gateNudgeLeft, &gateNudgeRight })
            button->setClickingTogglesState(false);
        nudgeLeft.getProperties().set("presetArrowDirection", -1);
        nudgeRight.getProperties().set("presetArrowDirection", 1);
        gateNudgeLeft.getProperties().set("presetArrowDirection", -1);
        gateNudgeRight.getProperties().set("presetArrowDirection", 1);
        for (auto* button : { &nudgeLeft, &nudgeRight,
                              &gateNudgeLeft, &gateNudgeRight })
            button->getProperties().set("compactPresetArrow", true);
        rangeLinkButton.setButtonText({});
        rangeLinkButton.setClickingTogglesState(true);
        rangeLinkButton.getProperties().set("rangeLinkIcon", true);
        rangeLinkButton.getProperties().set("compactRangeLink", true);
        rangeLinkButton.getProperties().set("activeColour",
                                             controlAccentColour.toString());
        nudgeLeft.onClick = [this] { grid.nudge(-1); };
        nudgeRight.onClick = [this] { grid.nudge(1); };
        gateNudgeLeft.onClick = [this] { grid.nudgeGates(-1); };
        gateNudgeRight.onClick = [this] { grid.nudgeGates(1); };

        hostSync.setTooltip("Synchronise the sequencer clock to the host tempo");
        enabledButton.setTooltip("Enable or disable sequencer modulation");
        bipolarButton.setTooltip(
            "Cycle target movement between Unipolar +, Bipolar and Unipolar -");
        animateButton.setTooltip(
            "Animate assigned controls without changing their stored values");
        animateButton.onClick = [this]
        {
            updateAnimatedControls();
            if (onAnimateChanged != nullptr)
                onAnimateChanged();
        };
        waveformBox.setTooltip("Replace the steps with a generated waveform");
        userSequenceBox.setTooltip("Load a saved user sequence");
        rateSlider.setTooltip("Set the sequencer rate");
        startSlider.setTooltip("Set the first step in the playback range");
        rangeLinkButton.setTooltip("Link Start and End so the range moves without changing length");
        endSlider.setTooltip("Set the last step in the playback range");
        directionBox.setTooltip("Choose the sequencer playback direction");
        colourSlider.setTooltip("Choose the hue used by the steps and assigned target labels");
        saturationSlider.setTooltip("Adjust the saturation of the sequencer colour");
        luminanceSlider.setTooltip("Adjust only the luminance of the sequencer steps");
        attackSlider.setTooltip("Smooth the rising edge between sequence steps");
        releaseSlider.setTooltip("Smooth the falling edge between sequence steps");
        peakSlider.setTooltip("Shrink or expand the sequence shape; 0% leaves it unchanged");
        moveSlider.setTooltip("Offset the complete sequence up or down");
        nudgeLeft.setTooltip("Move the step values one position left");
        nudgeRight.setTooltip("Move the step values one position right");
        gateNudgeLeft.setTooltip("Move the Gate states one position left");
        gateNudgeRight.setTooltip("Move the Gate states one position right");
        shortGateSlider.setTooltip(
            "Set how much of a step remains open in Short Gate mode");
        longGateSlider.setTooltip(
            "Set how much of a step remains open in Long Gate mode");
        gateModeButton.setTooltip(
            "Choose whether Gate acts before the effect, on the effect signal, on the complete output, or is bypassed");

        const auto configureTargetLabel = [](ParameterDragLabel& label,
                                             const juce::String& text,
                                             const juce::String& parameterId)
        {
            label.setText(text, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.setFont(juce::Font(juce::FontOptions(
                10.5f, juce::Font::bold)));
            label.setColour(juce::Label::textColourId, knobLabelColour);
            label.setInterceptsMouseClicks(true, false);
            label.parameterIdProvider = [parameterId] { return parameterId; };
            label.setTooltip("Drag " + text
                             + " onto a sequencer target slot");
        };
        configureTargetLabel(startLabel, "START", "sequencerStart");
        configureTargetLabel(endLabel, "END", "sequencerEnd");
        configureTargetLabel(attackLabel, "ATTACK", "sequencerAttack");
        configureTargetLabel(releaseLabel, "RELEASE", "sequencerRelease");
        configureTargetLabel(peakLabel, "DEPTH", "sequencerPeak");
        configureTargetLabel(moveLabel, "OFFSET", "sequencerMove");
        configureTargetLabel(shortGateLabel, "SHORT",
                             "sequencerGateShortLength");
        configureTargetLabel(longGateLabel, "LONG",
                             "sequencerGateLongLength");
        startLabel.setTooltip(startSlider.getTooltip());
        endLabel.setTooltip(endSlider.getTooltip());
        attackLabel.setTooltip(attackSlider.getTooltip());
        releaseLabel.setTooltip(releaseSlider.getTooltip());
        peakLabel.setTooltip(peakSlider.getTooltip());
        moveLabel.setTooltip(moveSlider.getTooltip());
        shortGateLabel.setTooltip(shortGateSlider.getTooltip());
        longGateLabel.setTooltip(longGateSlider.getTooltip());

        waveformBox.addItem("Saw", 1);
        waveformBox.addItem("Saw Double", 2);
        waveformBox.addItem("Saw Down", 3);
        waveformBox.addItem("Saw Down Double", 4);
        waveformBox.addItem("Sine", 5);
        waveformBox.addItem("Sine Double", 6);
        waveformBox.addItem("Triangle", 7);
        waveformBox.addItem("Triangle Double", 8);
        waveformBox.addItem("Pulse 25", 9);
        waveformBox.addItem("Pulse 25 Double", 10);
        waveformBox.addItem("Square", 11);
        waveformBox.addItem("Square Double", 12);
        waveformBox.addItem("Pulse 75", 13);
        waveformBox.addItem("Pulse 75 Double", 14);
        waveformBox.setTextWhenNothingSelected("WAVEFORM");
        waveformBox.onChange = [this]
        {
            const int selected = waveformBox.getSelectedItemIndex();
            if (selected >= 0)
                grid.applyWaveform(
                    static_cast<dusk::sequencer::Waveform>(selected));
            waveformBox.setSelectedId(0, juce::dontSendNotification);
        };

        directionBox.addItemList(
            { "Loop", "Bounce", "Reverse", "Played", "Random" }, 1);
        for (auto* box : { &waveformBox, &userSequenceBox, &directionBox })
        {
            box->setJustificationType(juce::Justification::centred);
            box->setColour(juce::ComboBox::backgroundColourId,
                           juce::Colour(0xff19151f));
            box->setColour(juce::ComboBox::textColourId, knobValueColour);
            box->setColour(juce::ComboBox::outlineColourId, knobTrackColour);
            box->setColour(juce::ComboBox::arrowColourId,
                           knobIndicatorColour);
            box->setScrollWheelEnabled(true);
            box->getProperties().set("compactClosedCombo", true);
        }
        directionAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                processor.parameters, "sequencerDirection", directionBox);
        hostSyncAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::ButtonAttachment>(
                processor.parameters, "sequencerHostSync", hostSync);
        enabledAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::ButtonAttachment>(
                processor.parameters, "sequencerEnabled", enabledButton);
        enabledButton.onStateChange = [this]
        {
            updateSequencerControlEnablement();
        };
        updateSequencerControlEnablement();
        auto* bipolarParameter = processor.parameters.getParameter(
            "sequencerBipolar");
        auto* negativeParameter = processor.parameters.getParameter(
            "sequencerUnipolarNegative");
        jassert(bipolarParameter != nullptr && negativeParameter != nullptr);
        if (bipolarParameter != nullptr && negativeParameter != nullptr)
        {
            bipolarModeAttachment = std::make_unique<juce::ParameterAttachment>(
                *bipolarParameter,
                [this](float)
                {
                    if (! updatingPolarityParameters)
                        syncPolarityFromParameters();
                }, nullptr);
            negativeModeAttachment = std::make_unique<juce::ParameterAttachment>(
                *negativeParameter,
                [this](float)
                {
                    if (! updatingPolarityParameters)
                        syncPolarityFromParameters();
                }, nullptr);
            bipolarButton.onClick = [this] { cyclePolarityMode(); };
            bipolarModeAttachment->sendInitialUpdate();
            negativeModeAttachment->sendInitialUpdate();
            syncPolarityFromParameters();
        }
        rangeLinkAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::ButtonAttachment>(
                processor.parameters, "sequencerRangeLink", rangeLinkButton);
        if (auto* gateModeParameter = processor.parameters.getParameter(
                "sequencerGateMode"))
        {
            gateModeAttachment = std::make_unique<juce::ParameterAttachment>(
                *gateModeParameter,
                [this](float)
                {
                    if (! updatingGateModeParameter)
                        syncGateModeFromParameter();
                }, nullptr);
            gateModeButton.onClick = [this] { cycleGateMode(); };
            gateModeAttachment->sendInitialUpdate();
            syncGateModeFromParameter();
        }

        configureKnob(rateSlider, 0.0, 13.0, 1.0);
        configureKnob(startSlider, 1.0, 31.0, 1.0);
        configureKnob(endSlider, 2.0, 32.0, 1.0);
        configureKnob(attackSlider, 0.0, 1.0, 0.001);
        configureKnob(releaseSlider, 0.0, 1.0, 0.001);
        attackSlider.textFromValueFunction
            = releaseSlider.textFromValueFunction
            = [](double value)
            {
                return juce::String(juce::roundToInt(value * 100.0)) + "%";
            };
        attackSlider.valueFromTextFunction
            = releaseSlider.valueFromTextFunction
            = [](const juce::String& text)
            {
                const double entered = text.getDoubleValue();
                return juce::jlimit(0.0, 1.0,
                                    entered > 1.0 || text.containsChar('%')
                                        ? entered * 0.01 : entered);
            };
        attackSlider.setTextBoxStyle(
            juce::Slider::TextBoxBelow, false, 52, 14);
        releaseSlider.setTextBoxStyle(
            juce::Slider::TextBoxBelow, false, 52, 14);
        configureKnob(shortGateSlider, 0.10, 0.60, 0.01);
        configureKnob(longGateSlider, 0.65, 0.95, 0.01);
        shortGateSlider.setDoubleClickReturnValue(true, 0.50);
        longGateSlider.setDoubleClickReturnValue(true, 0.90);
        configureVerticalSlider(peakSlider, -1.0, 1.0, 0.001);
        configureVerticalSlider(moveSlider, -1.0, 1.0, 0.001);
        peakSlider.setScrollWheelEnabled(true);
        moveSlider.setScrollWheelEnabled(true);
        peakSlider.setCentreDetent(0.0, 0.03);
        peakSlider.setDoubleClickReturnValue(true, 0.0);
        moveSlider.setCentreDetent(0.0, 0.03);
        colourSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        colourSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        colourSlider.setRange(0.0, 1.0, 0.001);
        colourSlider.setValue(0.5, juce::dontSendNotification);
        colourSlider.onValueChange = [this] { updateAccent(); };
        colourSlider.setDoubleClickReturnValue(true, 0.5);
        saturationSlider.setSliderStyle(
            juce::Slider::RotaryHorizontalVerticalDrag);
        saturationSlider.setTextBoxStyle(
            juce::Slider::NoTextBox, false, 0, 0);
        saturationSlider.setRange(0.0, 1.0, 0.001);
        saturationSlider.setValue(0.5, juce::dontSendNotification);
        saturationSlider.onValueChange = [this] { updateAccent(); };
        saturationSlider.setDoubleClickReturnValue(true, 0.5);
        luminanceSlider.setSliderStyle(
            juce::Slider::RotaryHorizontalVerticalDrag);
        luminanceSlider.setTextBoxStyle(
            juce::Slider::NoTextBox, false, 0, 0);
        luminanceSlider.setRange(0.2, 1.0, 0.001);
        luminanceSlider.setValue(0.5, juce::dontSendNotification);
        luminanceSlider.onValueChange = [this] { updateAccent(); };
        luminanceSlider.setDoubleClickReturnValue(true, 0.5);

        colourAttachment = makeSliderAttachment(
            "sequencerHue", colourSlider);
        saturationAttachment = makeSliderAttachment(
            "sequencerSaturation", saturationSlider);
        luminanceAttachment = makeSliderAttachment(
            "sequencerLuminance", luminanceSlider);

        rateAttachment = makeSliderAttachment("sequencerRate", rateSlider);
        startAttachment = makeSliderAttachment("sequencerStart", startSlider);
        endAttachment = makeSliderAttachment("sequencerEnd", endSlider);
        attackAttachment = makeSliderAttachment("sequencerAttack", attackSlider);
        releaseAttachment = makeSliderAttachment("sequencerRelease", releaseSlider);
        peakAttachment = makeSliderAttachment("sequencerPeak", peakSlider);
        moveAttachment = makeSliderAttachment("sequencerMove", moveSlider);
        shortGateAttachment = makeSliderAttachment(
            "sequencerGateShortLength", shortGateSlider);
        longGateAttachment = makeSliderAttachment(
            "sequencerGateLongLength", longGateSlider);
        configureSavedReset(rateSlider, "sequencerRate");
        configureSavedReset(startSlider, "sequencerStart");
        configureSavedReset(endSlider, "sequencerEnd");
        configureSavedReset(attackSlider, "sequencerAttack");
        configureSavedReset(releaseSlider, "sequencerRelease");
        configureSavedReset(peakSlider, "sequencerPeak");
        configureSavedReset(moveSlider, "sequencerMove");
        configureSavedReset(shortGateSlider, "sequencerGateShortLength");
        configureSavedReset(longGateSlider, "sequencerGateLongLength");
        linkedRangeLength = juce::jmax(
            1, juce::roundToInt(endSlider.getValue()
                               - startSlider.getValue()));
        rangeLinkButton.onClick = [this]
        {
            if (rangeLinkButton.getToggleState())
                linkedRangeLength = juce::jmax(
                    1, juce::roundToInt(endSlider.getValue()
                                       - startSlider.getValue()));
        };
        startSlider.onValueChange = [this]
        {
            if (! rangeLinkButton.getToggleState() || updatingLinkedRange)
                return;
            const juce::ScopedValueSetter<bool> guard(updatingLinkedRange, true);
            const int startValue = juce::roundToInt(startSlider.getValue());
            const int requestedEnd = startValue + linkedRangeLength;
            if (requestedEnd <= dusk::sequencer::stepCount)
                endSlider.setValue(requestedEnd, juce::sendNotificationSync);
            else
                startSlider.setValue(dusk::sequencer::stepCount
                                         - linkedRangeLength,
                                     juce::sendNotificationSync);
        };
        endSlider.onValueChange = [this]
        {
            if (! rangeLinkButton.getToggleState() || updatingLinkedRange)
                return;
            const juce::ScopedValueSetter<bool> guard(updatingLinkedRange, true);
            const int endValue = juce::roundToInt(endSlider.getValue());
            const int requestedStart = endValue - linkedRangeLength;
            if (requestedStart >= 1)
                startSlider.setValue(requestedStart, juce::sendNotificationSync);
            else
                endSlider.setValue(1 + linkedRangeLength,
                                   juce::sendNotificationSync);
        };
        rateSlider.textFromValueFunction = [](double value)
        {
            static const std::array<const char*, 14> names
            { "1/128", "1/64T", "1/64", "1/32T", "1/32", "1/16T",
              "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2T", "1/2", "1/1" };
            return juce::String(names[static_cast<size_t>(juce::jlimit(
                0, 13, juce::roundToInt(value)))]);
        };
        peakSlider.textFromValueFunction = moveSlider.textFromValueFunction
            = [](double value) { return juce::String(value * 100.0, 0) + "%"; };
        shortGateSlider.textFromValueFunction
            = longGateSlider.textFromValueFunction
            = [](double value) { return juce::String(value * 100.0, 0) + "%"; };
        attackSlider.updateText();
        releaseSlider.updateText();

        grid.onSaveRequested = [this] { saveUserSequence(); };
        userSequenceBox.setTextWhenNothingSelected("USER");
        userSequenceBox.onChange = [this] { loadSelectedUserSequence(); };
        refreshUserSequences();
        updateAccent();
    }

    ~SequencerPanel() override
    {
        removeMouseListener(this);
        setLookAndFeel(nullptr);
    }

    double getHue() const noexcept { return colourSlider.getValue(); }
    juce::Colour getAccent() const noexcept { return accent; }
    bool isTargetAssigned(const juce::String& parameterId) const
    {
        const int requested = dusk::sequencer::targetChoiceForParameter(
            parameterId);
        if (requested <= 0)
            return false;

        for (int slot = 0; slot < dusk::sequencer::targetSlotCount;
             ++slot)
        {
            auto* parameter = processor.parameters.getParameter(
                dusk::sequencer::targetParameterId(slot));
            if (parameter != nullptr
                && juce::roundToInt(parameter->convertFrom0to1(
                       parameter->getValue())) == requested)
                return true;
        }
        return false;
    }
    double getSaturation() const noexcept
    {
        return saturationSlider.getValue();
    }
    double getLuminance() const noexcept
    {
        return luminanceSlider.getValue();
    }
    bool getAnimate() const noexcept
    {
        return animateButton.getToggleState();
    }
    void setAnimate(bool shouldAnimate)
    {
        animateButton.setToggleState(shouldAnimate,
                                     juce::dontSendNotification);
        updateAnimatedControls();
    }
    std::function<void()> onAnimateChanged;
    void setHue(double hue)
    {
        colourSlider.setValue(juce::jlimit(0.0, 1.0, hue),
                              juce::dontSendNotification);
        updateAccent();
    }
    void setSaturation(double saturation)
    {
        saturationSlider.setValue(juce::jlimit(0.0, 1.0, saturation),
                                  juce::dontSendNotification);
        updateAccent();
    }
    void setLuminance(double luminance)
    {
        luminanceSlider.setValue(juce::jlimit(0.2, 1.0, luminance),
                                 juce::dontSendNotification);
        updateAccent();
    }
    void updateTargetHighlights()
    {
        const auto update = [this](ParameterDragLabel& label)
        {
            const auto parameterId = label.parameterIdProvider != nullptr
                ? label.parameterIdProvider() : juce::String();
            label.setTargetAppearance(isTargetAssigned(parameterId), accent);
        };
        update(startLabel);
        update(endLabel);
        update(attackLabel);
        update(releaseLabel);
        update(peakLabel);
        update(moveLabel);
        update(shortGateLabel);
        update(longGateLabel);
    }
    void timerTick()
    {
        updateSequencerControlEnablement();
        if (! updatingPolarityParameters)
            syncPolarityFromParameters();
        if (! updatingGateModeParameter)
            syncGateModeFromParameter();
        grid.repaint();
        targets.repaint();
        updateAnimatedControls();
    }

    void paint(juce::Graphics& graphics) override
    {
        const auto surround = surroundColour != nullptr
            ? surroundColour() : juce::Colour(0xff24292f);
        graphics.setColour(surround);
        graphics.fillRoundedRectangle(getLocalBounds().toFloat(), 12.0f);
        const auto panelColour = juce::Colour(0xff171b20);
        const auto drawPanel = [&graphics, panelColour](juce::Rectangle<float> area)
        {
            graphics.setColour(panelColour);
            graphics.fillRoundedRectangle(area, 8.0f);
            graphics.setColour(knobTrackColour);
            graphics.drawRoundedRectangle(area, 8.0f, 1.0f);
        };
        drawPanel({ 10.0f, 10.0f, static_cast<float>(getWidth() - 20), 68.0f });
        drawPanel({ 10.0f, 84.0f, static_cast<float>(getWidth() - 20), 152.0f });
        graphics.setColour(sequencerControlsEnabled
            ? knobLabelColour : knobLabelColour.darker(0.48f));
        graphics.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        graphics.drawText("RATE", 243, 11, 60, 13,
                          juce::Justification::centred);
        graphics.drawText("DIRECTION", 443, 11, 106, 13,
                          juce::Justification::centred);
        graphics.drawText("HUE", 773, 11, 46, 13,
                          juce::Justification::centred);
        graphics.drawText("SAT", 815, 11, 38, 13,
                          juce::Justification::centred);
        graphics.drawText("LUM", 853, 11, 38, 13,
                          juce::Justification::centred);
        graphics.drawText("STEPS", 16, 190, 68, 13,
                          juce::Justification::centred);
        graphics.drawText("GATE", 82, 190, 68, 13,
                          juce::Justification::centred);
    }

    void resized() override
    {
        hostSync.setBounds(24, 14, 88, 24);
        animateButton.setBounds(150, 14, 68, 24);
        waveformBox.setBounds(23, 45, 90, 20);
        userSequenceBox.setBounds(139, 45, 90, 20);
        rateSlider.setBounds(251, 26, 44, 48);
        startLabel.setBounds(299, 11, 60, 13);
        startSlider.setBounds(307, 26, 44, 48);
        rangeLinkButton.setBounds(358, 33, 18, 18);
        endLabel.setBounds(379, 11, 60, 13);
        endSlider.setBounds(387, 26, 44, 48);
        directionBox.setBounds(451, 33, 90, 20);
        shortGateLabel.setBounds(553, 11, 60, 13);
        shortGateSlider.setBounds(561, 26, 44, 48);
        longGateLabel.setBounds(611, 11, 60, 13);
        longGateSlider.setBounds(619, 26, 44, 48);
        gateModeButton.setBounds(676, 31, 96, 24);
        colourSlider.setBounds(781, 29, 30, 30);
        saturationSlider.setBounds(819, 29, 30, 30);
        luminanceSlider.setBounds(857, 29, 30, 30);
        enabledButton.setBounds(20, 88, 34, 24);
        bipolarButton.setBounds(56, 88, 84, 24);
        attackLabel.setBounds(16, 118, 68, 13);
        releaseLabel.setBounds(82, 118, 68, 13);
        peakLabel.setBounds(149, 88, 64, 13);
        moveLabel.setBounds(149, 156, 64, 13);
        attackSlider.setBounds(24, 131, 52, 49);
        releaseSlider.setBounds(90, 131, 52, 49);
        peakSlider.setBounds(167, 102, 28, 58);
        moveSlider.setBounds(167, 170, 28, 62);
        nudgeLeft.setBounds(27, 204, 20, 20);
        nudgeRight.setBounds(53, 204, 20, 20);
        gateNudgeLeft.setBounds(93, 204, 20, 20);
        gateNudgeRight.setBounds(119, 204, 20, 20);
        grid.setBounds(210, 92, 554, 136);
        targets.setBounds(774, 92, getWidth() - 792, 136);
    }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    void setPolarityMode(int polarityMode)
    {
        polarityMode = juce::jlimit(0, 2, polarityMode);
        displayedPolarityMode = polarityMode;
        grid.setPolarityMode(polarityMode);

        const bool isBipolar = polarityMode == 1;
        const bool isNegative = polarityMode == 2;
        bipolarButton.setButtonText(isBipolar ? "BIPOLAR"
            : (isNegative ? "UNIPOLAR -" : "UNIPOLAR +"));
        bipolarButton.getProperties().set(
            "activeColour",
            (isBipolar ? bipolarLedColour
                       : (isNegative ? unipolarNegativeLedColour
                                     : controlAccentColour)).toString());
        bipolarButton.setToggleState(true, juce::dontSendNotification);
        bipolarButton.repaint();
    }

    void syncPolarityFromParameters()
    {
        const auto* bipolar = processor.parameters.getRawParameterValue(
            "sequencerBipolar");
        const auto* negative = processor.parameters.getRawParameterValue(
            "sequencerUnipolarNegative");
        const bool isBipolar = bipolar != nullptr && bipolar->load() >= 0.5f;
        const bool isNegative = ! isBipolar && negative != nullptr
            && negative->load() >= 0.5f;
        const int polarityMode = isBipolar ? 1 : (isNegative ? 2 : 0);
        if (displayedPolarityMode == polarityMode)
            return;
        setPolarityMode(polarityMode);
    }

    void cyclePolarityMode()
    {
        if (bipolarModeAttachment == nullptr
            || negativeModeAttachment == nullptr)
            return;

        const int nextMode = (juce::jlimit(0, 2, displayedPolarityMode) + 1) % 3;

        // Advance the label, LED and grid as one state. The two persisted
        // booleans are then written as a canonical pair without exposing the
        // intermediate pair through either attachment callback.
        setPolarityMode(nextMode);
        const juce::ScopedValueSetter<bool> guard(
            updatingPolarityParameters, true);
        negativeModeAttachment->setValueAsCompleteGesture(
            nextMode == 2 ? 1.0f : 0.0f);
        bipolarModeAttachment->setValueAsCompleteGesture(
            nextMode == 1 ? 1.0f : 0.0f);
    }

    void setGateMode(int gateMode)
    {
        gateMode = juce::jlimit(0, 3, gateMode);
        displayedGateMode = gateMode;
        const auto text = gateMode == 0 ? "INPUT"
                        : gateMode == 1 ? "EFFECT"
                        : gateMode == 2 ? "OUTPUT"
                                        : "OFF";
        const auto colour = gateMode == 0 ? linkLedColour
                          : gateMode == 1 ? controlAccentColour
                          : gateMode == 2 ? bipolarLedColour
                                          : juce::Colour(0xff596069);
        gateModeButton.setButtonText(text);
        gateModeButton.getProperties().set(
            "activeColour", colour.toString());
        gateModeButton.setToggleState(
            gateMode != 3, juce::dontSendNotification);
        gateModeButton.repaint();
    }

    void syncGateModeFromParameter()
    {
        const auto* gateMode = processor.parameters.getRawParameterValue(
            "sequencerGateMode");
        const int mode = gateMode != nullptr
            ? juce::jlimit(0, 3, juce::roundToInt(gateMode->load())) : 0;
        if (displayedGateMode != mode)
            setGateMode(mode);
    }

    void cycleGateMode()
    {
        if (gateModeAttachment == nullptr)
            return;

        const int nextMode = (juce::jlimit(0, 3, displayedGateMode) + 1) % 4;
        setGateMode(nextMode);
        const juce::ScopedValueSetter<bool> guard(
            updatingGateModeParameter, true);
        gateModeAttachment->setValueAsCompleteGesture(
            static_cast<float>(nextMode));
    }

    void mouseEnter(const juce::MouseEvent& event) override
    {
        if (showHelp == nullptr)
            return;

        const auto isWithin = [](juce::Component* component,
                                 const juce::Component& owner)
        {
            for (auto* current = component; current != nullptr;
                 current = current->getParentComponent())
                if (current == &owner)
                    return true;
            return false;
        };
        juce::String controlTitle;
        const std::array<juce::Slider*, 12> sequencerSliders
        {{ &rateSlider, &startSlider, &endSlider,
           &attackSlider, &releaseSlider, &peakSlider,
           &moveSlider, &colourSlider, &saturationSlider,
           &luminanceSlider, &shortGateSlider, &longGateSlider }};
        for (auto* slider : sequencerSliders)
            if (isWithin(event.eventComponent, *slider))
            {
                controlTitle = slider->getName().trim();
                break;
            }

        juce::String title = controlTitle;
        juce::String tooltipText;
        for (auto* component = event.eventComponent;
             component != nullptr && component != this;
             component = component->getParentComponent())
        {
            if (title.isEmpty())
                title = component->getName().trim();
            if (tooltipText.isEmpty())
                if (auto* tooltip = dynamic_cast<juce::TooltipClient*>(component))
                    tooltipText = tooltip->getTooltip().trim();
        }
        if (title.isEmpty())
            for (auto* component = event.eventComponent;
                 component != nullptr && component != this;
                 component = component->getParentComponent())
                if (auto* label = dynamic_cast<juce::Label*>(component))
                {
                    title = label->getText().trim();
                    if (title.isNotEmpty())
                        break;
                }
        if (tooltipText.isNotEmpty())
            showHelp(contextHelpText(title, tooltipText));
    }

    void configureKnob(juce::Slider& slider, double minimum,
                       double maximum, double interval)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
        slider.setRange(minimum, maximum, interval);
        slider.setMouseDragSensitivity(80);
        slider.setColour(juce::Slider::rotarySliderFillColourId,
                         knobIndicatorColour);
        slider.setColour(juce::Slider::textBoxTextColourId, knobValueColour);
        slider.setColour(juce::Slider::textBoxBackgroundColourId,
                         juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxOutlineColourId,
                         juce::Colours::transparentBlack);
    }

    void updateSequencerControlEnablement()
    {
        const bool shouldEnable = enabledButton.getToggleState();
        if (sequencerControlsEnabled == shouldEnable)
            return;

        sequencerControlsEnabled = shouldEnable;
        const std::array<juce::Component*, 34> controls
        {{ &hostSync, &bipolarButton, &animateButton, &gateModeButton,
           &waveformBox, &userSequenceBox, &directionBox,
           &rateSlider, &startSlider, &endSlider, &attackSlider,
           &releaseSlider, &peakSlider, &moveSlider, &colourSlider,
           &saturationSlider, &luminanceSlider,
           &startLabel, &endLabel, &attackLabel, &releaseLabel,
           &peakLabel, &moveLabel, &shortGateLabel, &longGateLabel,
           &nudgeLeft, &nudgeRight, &gateNudgeLeft, &gateNudgeRight,
           &rangeLinkButton, &shortGateSlider, &longGateSlider,
           &grid, &targets }};
        for (auto* control : controls)
        {
            control->setEnabled(shouldEnable);
            control->setAlpha(shouldEnable ? 1.0f : 0.48f);
        }
        repaint();
    }

    void configureVerticalSlider(juce::Slider& slider, double minimum,
                                 double maximum, double interval)
    {
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange(minimum, maximum, interval);
        slider.setMouseDragSensitivity(80);
        slider.setColour(juce::Slider::rotarySliderFillColourId,
                         knobIndicatorColour);
    }

    std::unique_ptr<SliderAttachment> makeSliderAttachment(
        const juce::String& id, juce::Slider& slider)
    {
        return std::make_unique<SliderAttachment>(processor.parameters,
                                                   id, slider);
    }

    void configureSavedReset(DetentedSlider& slider, juce::String id)
    {
        slider.onSavedStateReset = [this, &slider, id = std::move(id)]
        {
            const auto saved = processor.getSavedParameterValue(id);
            if (! saved.has_value())
                return false;
            slider.setValue(*saved, juce::sendNotificationSync);
            return true;
        };
    }

    void updateAccent()
    {
        // Target labels and the three colour controls must remain readable.
        // LUM deliberately affects only the step display.
        accent = juce::Colour::fromHSV(
            static_cast<float>(colourSlider.getValue()),
            static_cast<float>(saturationSlider.getValue()),
            0.72f, 1.0f);
        const auto stepAccent = juce::Colour::fromHSV(
            static_cast<float>(colourSlider.getValue()),
            static_cast<float>(saturationSlider.getValue()),
            static_cast<float>(luminanceSlider.getValue()), 1.0f);
        colourSlider.setColour(juce::Slider::rotarySliderFillColourId, accent);
        saturationSlider.setColour(juce::Slider::rotarySliderFillColourId,
                                   accent);
        luminanceSlider.setColour(juce::Slider::rotarySliderFillColourId,
                                  accent);
        grid.setAccent(stepAccent);
        updateTargetHighlights();
        repaint();
    }

    void updateAnimatedControls()
    {
        const auto update = [this](juce::Slider& slider,
                                   const juce::String& parameterId)
        {
            const auto value = animateButton.getToggleState()
                ? processor.getSequencerAnimatedNormalised(parameterId)
                : std::nullopt;
            auto& sliderProperties = slider.getProperties();
            if (value.has_value())
            {
                const bool changed = ! sliderProperties.contains("animatedNormalised")
                    || std::abs(static_cast<float>(
                           sliderProperties["animatedNormalised"]) - *value)
                           > 0.000001f;
                if (changed)
                {
                    sliderProperties.set("animatedNormalised", *value);
                    slider.repaint();
                }
            }
            else if (sliderProperties.contains("animatedNormalised"))
            {
                sliderProperties.remove("animatedNormalised");
                slider.repaint();
            }
        };
        update(attackSlider, "sequencerAttack");
        update(releaseSlider, "sequencerRelease");
        update(peakSlider, "sequencerPeak");
        update(moveSlider, "sequencerMove");
        update(startSlider, "sequencerStart");
        update(endSlider, "sequencerEnd");
        update(shortGateSlider, "sequencerGateShortLength");
        update(longGateSlider, "sequencerGateLongLength");
    }

    void refreshUserSequences()
    {
        const auto selectedText = userSequenceBox.getText();
        userSequenceFiles.clear();
        const auto directory = DuskAudioProcessor::getSequencesDirectory();
        directory.createDirectory();
        directory.findChildFiles(userSequenceFiles, juce::File::findFiles,
                                 false, "*.duskseq");
        std::sort(userSequenceFiles.begin(), userSequenceFiles.end(),
            [](const juce::File& left, const juce::File& right)
            {
                return left.getFileNameWithoutExtension().compareNatural(
                           right.getFileNameWithoutExtension(), true) < 0;
            });
        userSequenceBox.clear(juce::dontSendNotification);
        for (int index = 0; index < userSequenceFiles.size(); ++index)
            userSequenceBox.addItem(
                userSequenceFiles[index].getFileNameWithoutExtension(),
                index + 1);
        userSequenceBox.setTextWhenNothingSelected(
            selectedText.isNotEmpty() ? selectedText : "USER");
    }

    void loadSelectedUserSequence()
    {
        const int index = userSequenceBox.getSelectedId() - 1;
        if (! juce::isPositiveAndBelow(index, userSequenceFiles.size()))
            return;
        const auto xml = juce::XmlDocument::parse(userSequenceFiles[index]);
        if (xml == nullptr || ! xml->hasTagName("DuskSequence"))
            return;
        if (xml->hasAttribute("gateShort"))
            shortGateSlider.setValue(
                xml->getDoubleAttribute("gateShort", 0.50),
                juce::sendNotificationSync);
        if (xml->hasAttribute("gateLong"))
            longGateSlider.setValue(
                xml->getDoubleAttribute("gateLong", 0.90),
                juce::sendNotificationSync);
        for (auto* stepElement : xml->getChildWithTagNameIterator("Step"))
        {
            const int step = stepElement->getIntAttribute("index", -1);
            if (! juce::isPositiveAndBelow(step,
                                           dusk::sequencer::stepCount))
                continue;
            setParameterActualValue(processor.parameters.getParameter(
                dusk::sequencer::stepParameterId(step)),
                static_cast<float>(stepElement->getDoubleAttribute("value", 1.0)));
            if (stepElement->hasAttribute("gate"))
                setParameterActualValue(processor.parameters.getParameter(
                    dusk::sequencer::gateStepParameterId(step)),
                    static_cast<float>(stepElement->getDoubleAttribute(
                        "gate", static_cast<double>(
                            static_cast<int>(
                                dusk::sequencer::GateStepMode::rest)))));
        }
        grid.repaint();
    }

    void saveUserSequence()
    {
        sequenceFileChooser = std::make_unique<juce::FileChooser>(
            "Save Dusk User Sequence",
            DuskAudioProcessor::getSequencesDirectory()
                .getChildFile("New Sequence.duskseq"), "*.duskseq");
        const juce::Component::SafePointer<SequencerPanel> safeThis(this);
        sequenceFileChooser->launchAsync(
            juce::FileBrowserComponent::saveMode
                | juce::FileBrowserComponent::canSelectFiles,
            [safeThis](const juce::FileChooser& chooser)
            {
                if (safeThis == nullptr)
                    return;
                auto file = chooser.getResult();
                if (file == juce::File())
                    return;
                if (! file.hasFileExtension(".duskseq"))
                    file = file.withFileExtension(".duskseq");
                auto xml = std::make_unique<juce::XmlElement>(
                    "DuskSequence");
                xml->setAttribute("gateShort",
                    safeThis->shortGateSlider.getValue());
                xml->setAttribute("gateLong",
                    safeThis->longGateSlider.getValue());
                for (int step = 0; step < dusk::sequencer::stepCount;
                     ++step)
                {
                    auto* element = xml->createNewChildElement("Step");
                    element->setAttribute("index", step);
                    const auto read = [&safeThis](const juce::String& id)
                    {
                        auto* parameter = safeThis->processor.parameters
                            .getParameter(id);
                        return parameter != nullptr
                            ? parameter->convertFrom0to1(parameter->getValue())
                            : 0.0f;
                    };
                    element->setAttribute("value", read(
                        dusk::sequencer::stepParameterId(step)));
                    element->setAttribute("gate", read(
                        dusk::sequencer::gateStepParameterId(step)));
                }
                if (xml->writeTo(file))
                    safeThis->refreshUserSequences();
                safeThis->sequenceFileChooser.reset();
            });
    }

    DuskAudioProcessor& processor;
    std::function<juce::Colour()> surroundColour;
    std::function<void(const juce::String&)> showHelp;
    LookAndFeel patternMenuLookAndFeel { true };
    StepGrid grid;
    TargetList targets;
    juce::ToggleButton hostSync;
    juce::ToggleButton enabledButton;
    juce::ToggleButton bipolarButton;
    juce::ToggleButton animateButton;
    juce::ToggleButton gateModeButton;
    juce::ToggleButton rangeLinkButton;
    juce::ComboBox waveformBox;
    juce::ComboBox userSequenceBox;
    juce::ComboBox directionBox;
    juce::ToggleButton nudgeLeft;
    juce::ToggleButton nudgeRight;
    juce::ToggleButton gateNudgeLeft;
    juce::ToggleButton gateNudgeRight;
    DetentedSlider rateSlider;
    DetentedSlider startSlider;
    DetentedSlider endSlider;
    DetentedSlider attackSlider;
    DetentedSlider releaseSlider;
    DetentedSlider peakSlider;
    DetentedSlider moveSlider;
    DetentedSlider shortGateSlider;
    DetentedSlider longGateSlider;
    ParameterDragLabel startLabel;
    ParameterDragLabel endLabel;
    ParameterDragLabel attackLabel;
    ParameterDragLabel releaseLabel;
    ParameterDragLabel peakLabel;
    ParameterDragLabel moveLabel;
    ParameterDragLabel shortGateLabel;
    ParameterDragLabel longGateLabel;
    juce::Slider colourSlider;
    juce::Slider saturationSlider;
    juce::Slider luminanceSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        hostSyncAttachment, enabledAttachment,
        rangeLinkAttachment;
    std::unique_ptr<juce::ParameterAttachment> bipolarModeAttachment,
        negativeModeAttachment, gateModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        directionAttachment;
    std::unique_ptr<SliderAttachment> rateAttachment, startAttachment,
        endAttachment, attackAttachment, releaseAttachment, peakAttachment,
        moveAttachment, colourAttachment, saturationAttachment,
        luminanceAttachment, shortGateAttachment, longGateAttachment;
    juce::Array<juce::File> userSequenceFiles;
    std::unique_ptr<juce::FileChooser> sequenceFileChooser;
    juce::Colour accent { 0xff34d6c6 };
    int linkedRangeLength = dusk::sequencer::stepCount - 1;
    int displayedPolarityMode = -1;
    int displayedGateMode = -1;
    bool updatingPolarityParameters = false;
    bool updatingGateModeParameter = false;
    bool updatingLinkedRange = false;
    bool sequencerControlsEnabled = true;
};

void DuskAudioProcessorEditor::ResponseDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colour(0xff11151a));
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(knobTrackColour);
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

    const auto graph = bounds.reduced(8.0f, 19.0f)
        .withTrimmedBottom(10.0f);
    const double sampleRate = processor.getSampleRate() > 0.0
        ? processor.getSampleRate() : 44100.0;
    const double minimumFrequency = 20.0;
    const double maximumFrequency = juce::jmax(
        minimumFrequency,
        juce::jmin(20000.0, sampleRate * 0.45));
    constexpr double maximumDb = 30.0;
    constexpr double minimumDb = -96.0;
    const auto xForFrequency = [graph, minimumFrequency,
                                maximumFrequency](double frequency)
    {
        const double fraction = std::log(frequency / minimumFrequency)
            / std::log(maximumFrequency / minimumFrequency);
        return graph.getX() + graph.getWidth()
            * static_cast<float>(juce::jlimit(0.0, 1.0, fraction));
    };
    const auto yForDb = [graph, minimumDb, maximumDb](double decibels)
    {
        const double clamped = juce::jlimit(
            minimumDb, maximumDb, decibels);
        const double fraction = (maximumDb - clamped)
            / (maximumDb - minimumDb);
        return graph.getY() + graph.getHeight()
            * static_cast<float>(fraction);
    };

    g.setColour(knobTrackColour.withAlpha(0.56f));
    for (const double frequency : { 100.0, 1000.0, 10000.0 })
        if (frequency < maximumFrequency)
            g.drawVerticalLine(
                juce::roundToInt(xForFrequency(frequency)),
                graph.getY(), graph.getBottom());
    for (const double decibels :
         { 24.0, 12.0, -24.0, -48.0, -72.0, -96.0 })
        g.drawHorizontalLine(juce::roundToInt(yForDb(decibels)),
                             graph.getX(), graph.getRight());
    g.setColour(knobLabelColour.withAlpha(0.82f));
    g.drawHorizontalLine(juce::roundToInt(yForDb(0.0)),
                         graph.getX(), graph.getRight());

    g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
    g.drawText("0 dB", juce::roundToInt(graph.getX()),
               juce::roundToInt(yForDb(0.0) - 12.0f), 28, 10,
               juce::Justification::centredLeft, false);
    g.drawText("20 Hz", juce::roundToInt(graph.getX()),
               juce::roundToInt(graph.getBottom() + 1.0f), 38, 10,
               juce::Justification::centredLeft, false);
    const auto maximumFrequencyText = maximumFrequency >= 19500.0
        ? juce::String("20 kHz")
        : juce::String(maximumFrequency * 0.001, 1) + " kHz";
    g.drawText(maximumFrequencyText,
               juce::roundToInt(graph.getRight() - 42.0f),
               juce::roundToInt(graph.getBottom() + 1.0f), 40, 10,
               juce::Justification::centredRight, false);

    g.setColour(knobValueColour);
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.drawText("FILTER RESPONSE", getLocalBounds().removeFromTop(17),
               juce::Justification::centred, false);

    const auto parameterValue = [this](const char* parameterId)
    {
        const juce::String id(parameterId);
        auto* const parameter = processor.parameters.getParameter(id);
        if (parameter != nullptr)
            if (const auto animated =
                    processor.getSequencerAnimatedNormalised(id))
                return parameter->convertFrom0to1(*animated);
        const auto* const rawValue =
            processor.parameters.getRawParameterValue(id);
        return rawValue != nullptr ? rawValue->load() : 0.0f;
    };
    const std::array<float, 2> cutoff {
        parameterValue("filter1Cutoff"),
        parameterValue("filter2Cutoff")
    };
    const std::array<float, 2> resonance {
        parameterValue("filter1Resonance"),
        parameterValue("filter2Resonance")
    };
    const std::array<float, 2> resonanceShape {
        parameterValue("filter1ResonanceShape"),
        parameterValue("filter2ResonanceShape")
    };
    const std::array<float, 2> resonanceLimit {
        parameterValue("filter1ResonanceLimit"),
        parameterValue("filter2ResonanceLimit")
    };
    const std::array<float, 2> bassLoss {
        parameterValue("filter1BassLoss"),
        parameterValue("filter2BassLoss")
    };
    const std::array<float, 2> poleSpread {
        parameterValue("filter1PoleSpread"),
        parameterValue("filter2PoleSpread")
    };
    const std::array<float, 2> feedback {
        parameterValue("filter1Feedback"),
        parameterValue("filter2Feedback")
    };
    const std::array<float, 2> ladderStageSaturation {
        parameterValue("filter1LadderStageSaturation"),
        parameterValue("filter2LadderStageSaturation")
    };
    const std::array<float, 2> ladderResonanceCompensation {
        parameterValue("filter1LadderResonanceCompensation"),
        parameterValue("filter2LadderResonanceCompensation")
    };
    const std::array<float, 2> otaCellDrive {
        parameterValue("filter1OtaCellDrive"),
        parameterValue("filter2OtaCellDrive")
    };
    const std::array<float, 2> otaFeedbackDamping {
        parameterValue("filter1OtaFeedbackDamping"),
        parameterValue("filter2OtaFeedbackDamping")
    };
    const std::array<float, 2> otaResonanceCompensation {
        parameterValue("filter1OtaResonanceCompensation"),
        parameterValue("filter2OtaResonanceCompensation")
    };
    const std::array<int, 2> architecture {
        dusk::architecture::normaliseChoice(juce::roundToInt(
            parameterValue("filter1Architecture"))),
        dusk::architecture::normaliseChoice(juce::roundToInt(
            parameterValue("filter2Architecture")))
    };
    const std::array<float, 2> slope {
        parameterValue("filter1Slope"),
        parameterValue("filter2Slope")
    };
    const std::array<bool, 2> enabled {
        parameterValue("filter1Enabled") >= 0.5f,
        parameterValue("filter2Enabled") >= 0.5f
    };
    const int routing = juce::jlimit(
        0, 3, juce::roundToInt(parameterValue("routing")));
    const std::array<int, 2> filterType {
        juce::jlimit(0, 4,
            juce::roundToInt(parameterValue("filter1Type"))),
        juce::jlimit(0, 4,
            juce::roundToInt(parameterValue("filter2Type")))
    };

    juce::Path filter1Path;
    juce::Path filter2Path;
    juce::Path combinedPath;
    const int pointCount = juce::jmax(
        2, juce::roundToInt(graph.getWidth()));
    for (int point = 0; point <= pointCount; ++point)
    {
        const double fraction = static_cast<double>(point)
            / static_cast<double>(pointCount);
        const double frequency = minimumFrequency * std::pow(
            maximumFrequency / minimumFrequency, fraction);
        const auto filter1Response = enabled[0]
            ? DuskAudioProcessor::getArchitectureResponse(
                architecture[0], frequency, sampleRate, cutoff[0],
                resonance[0], slope[0],
                resonanceShape[0], bassLoss[0], resonanceLimit[0],
                poleSpread[0], feedback[0], ladderStageSaturation[0],
                ladderResonanceCompensation[0], otaCellDrive[0],
                otaFeedbackDamping[0], otaResonanceCompensation[0],
                filterType[0])
            : std::complex<double>(1.0, 0.0);
        const auto filter2Response = enabled[1]
            ? DuskAudioProcessor::getArchitectureResponse(
                architecture[1], frequency, sampleRate, cutoff[1],
                resonance[1], slope[1],
                resonanceShape[1], bassLoss[1], resonanceLimit[1],
                poleSpread[1], feedback[1], ladderStageSaturation[1],
                ladderResonanceCompensation[1], otaCellDrive[1],
                otaFeedbackDamping[1], otaResonanceCompensation[1],
                filterType[1])
            : std::complex<double>(1.0, 0.0);
        std::complex<double> combinedResponse;
        if (routing == 0)
            combinedResponse = filter1Response;
        else if (routing == 1)
            combinedResponse = filter2Response;
        else if (routing == 3)
            combinedResponse = 0.5
                * (filter1Response + filter2Response);
        else
            combinedResponse = filter1Response * filter2Response;

        const auto decibels = [](const std::complex<double>& response)
        {
            return 20.0 * std::log10(
                juce::jmax(1.0e-6, std::abs(response)));
        };
        const float x = graph.getX() + graph.getWidth()
            * static_cast<float>(fraction);
        const float filter1Y = yForDb(decibels(filter1Response));
        const float filter2Y = yForDb(decibels(filter2Response));
        const float combinedY = yForDb(decibels(combinedResponse));
        if (point == 0)
        {
            filter1Path.startNewSubPath(x, filter1Y);
            filter2Path.startNewSubPath(x, filter2Y);
            combinedPath.startNewSubPath(x, combinedY);
        }
        else
        {
            filter1Path.lineTo(x, filter1Y);
            filter2Path.lineTo(x, filter2Y);
            combinedPath.lineTo(x, combinedY);
        }
    }

    const auto filter1Colour = colourForFilter != nullptr
        ? colourForFilter(1) : controlAccentColour;
    const auto filter2Colour = colourForFilter != nullptr
        ? colourForFilter(2) : knobIndicatorColour;
    const auto combinedColour = juce::Colour(0xffd6dbe0);
    g.saveState();
    g.reduceClipRegion(graph.toNearestInt());
    g.setColour(filter1Colour.withAlpha(enabled[0] ? 0.90f : 0.34f));
    g.strokePath(filter1Path, juce::PathStrokeType(
        1.45f, juce::PathStrokeType::curved,
        juce::PathStrokeType::rounded));
    g.setColour(filter2Colour.withAlpha(enabled[1] ? 0.90f : 0.34f));
    g.strokePath(filter2Path, juce::PathStrokeType(
        1.45f, juce::PathStrokeType::curved,
        juce::PathStrokeType::rounded));
    g.setColour(combinedColour.withAlpha(0.94f));
    g.strokePath(combinedPath, juce::PathStrokeType(
        1.9f, juce::PathStrokeType::curved,
        juce::PathStrokeType::rounded));
    g.restoreState();

    const float legendY = bounds.getBottom() - 8.0f;
    g.setColour(filter1Colour);
    g.drawLine(bounds.getCentreX() - 70.0f, legendY,
               bounds.getCentreX() - 60.0f, legendY, 2.0f);
    g.setColour(filter2Colour);
    g.drawLine(bounds.getCentreX() - 20.0f, legendY,
               bounds.getCentreX() - 10.0f, legendY, 2.0f);
    g.setColour(combinedColour);
    g.drawLine(bounds.getCentreX() + 31.0f, legendY,
               bounds.getCentreX() + 41.0f, legendY, 2.0f);
    g.setColour(knobLabelColour);
    g.setFont(juce::Font(juce::FontOptions(7.5f)));
    g.drawText("F1", juce::roundToInt(bounds.getCentreX() - 58.0f),
               juce::roundToInt(legendY - 6.0f), 18, 12,
               juce::Justification::centredLeft, false);
    g.drawText("F2", juce::roundToInt(bounds.getCentreX() - 8.0f),
               juce::roundToInt(legendY - 6.0f), 18, 12,
               juce::Justification::centredLeft, false);
    g.drawText("OUT", juce::roundToInt(bounds.getCentreX() + 43.0f),
               juce::roundToInt(legendY - 6.0f), 24, 12,
               juce::Justification::centredLeft, false);
}

void DuskAudioProcessorEditor::PresetDisplayLabel::paint(
    juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(findColour(juce::Label::backgroundColourId));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(findColour(juce::Label::outlineColourId));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
    g.setColour(isEnabled()
        ? findColour(juce::Label::textColourId)
        : findColour(juce::Label::textColourId).darker(0.45f));
    g.setFont(getFont());
    g.drawFittedText(getText(), getLocalBounds().reduced(6, 1),
                     getJustificationType(), 1, 0.72f);
}

void DuskAudioProcessorEditor::PresetDisplayLabel::mouseDown(
    const juce::MouseEvent& event)
{
    if (isEnabled() && event.mods.isLeftButtonDown() && onShowMenu != nullptr)
        onShowMenu();
}

void DuskAudioProcessorEditor::PresetDisplayLabel::mouseWheelMove(
    const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (! isEnabled() || onStep == nullptr
        || std::abs(wheel.deltaY) < 0.0001f)
        return;

    const int direction = wheel.deltaY > 0.0f ? -1 : 1;
    const auto now = juce::Time::getMillisecondCounter();
    if (direction == lastWheelDirection && now - lastWheelTimeMs < 100u)
        return;

    lastWheelDirection = direction;
    lastWheelTimeMs = now;
    onStep(direction);
}

void DuskAudioProcessorEditor::ParameterDragLabel::setTargetAppearance(
    bool assigned, juce::Colour accent)
{
    if (targetAssigned == assigned && targetAccent == accent)
        return;

    targetAssigned = assigned;
    targetAccent = accent;
    setColour(juce::Label::textColourId,
              targetAssigned ? targetAccent : knobLabelColour);
    repaint();
}

void DuskAudioProcessorEditor::ParameterDragLabel::paint(
    juce::Graphics& graphics)
{
    const auto parameterId = parameterIdProvider != nullptr
        ? parameterIdProvider() : juce::String();
    const bool draggable = dusk::sequencer::targetChoiceForParameter(
        parameterId) > 0;
    if (! draggable)
    {
        if (! dragHandleOnly)
            juce::Label::paint(graphics);
        return;
    }

    auto colour = targetAssigned ? targetAccent : knobLabelColour;
    if (! isEnabled())
        colour = colour.darker(0.42f);

    const auto caption = dragHandleOnly ? juce::String()
                                        : getText().trim();
    graphics.setColour(colour);
    const auto labelFont = getFont();
    graphics.setFont(labelFont);
    if (caption.isEmpty())
    {
        graphics.drawFittedText("::", getLocalBounds(),
                                juce::Justification::centred, 1, 0.72f);
        return;
    }

    // Keep the caption itself centred over its knob.  The drag marker belongs
    // to this same label component, but sits one measured space outside the
    // centred caption so it cannot pull the visible control name sideways.
    graphics.drawFittedText(caption, getLocalBounds(),
                            getJustificationType(), 1, 0.72f);
    const float captionWidth = juce::GlyphArrangement::getStringWidth(
        labelFont, caption);
    const float markerAdvance = juce::GlyphArrangement::getStringWidth(
        labelFont, "::");
    const float gap = juce::GlyphArrangement::getStringWidth(labelFont, " ");
    const float captionLeft = static_cast<float>(getWidth()) * 0.5f
                            - captionWidth * 0.5f;
    const float markerX = dragHandleRight
        ? captionLeft + captionWidth + gap
        : captionLeft - gap - markerAdvance;
    // Give the glyphs a little drawing room beyond their typographic advance;
    // otherwise JUCE can clip the outer colon at small GUI scales.
    constexpr float markerPadding = 2.0f;
    const int markerLeft = juce::roundToInt(markerX - markerPadding);
    const int markerWidth = juce::jmax(
        1, juce::roundToInt(markerAdvance + markerPadding * 2.0f));
    graphics.drawText("::", markerLeft, 0, markerWidth, getHeight(),
                      getJustificationType(), false);
}

void DuskAudioProcessorEditor::ParameterDragLabel::mouseDrag(
    const juce::MouseEvent& event)
{
    if (event.getDistanceFromDragStart() < 4)
        return;
    const auto parameterId = parameterIdProvider != nullptr
        ? parameterIdProvider() : juce::String();
    if (dusk::sequencer::targetChoiceForParameter(parameterId) <= 0)
        return;
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
        if (! container->isDragAndDropActive())
            container->startDragging(
                juce::var(juce::String(sequencerTargetDragPrefix) + parameterId),
                this);
}

DuskAudioProcessorEditor::LookAndFeel::LookAndFeel(
    bool compactPopupFont)
    : useCompactPopupFont(compactPopupFont)
{
    setColour(juce::Slider::textBoxTextColourId, knobValueColour);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff14181d));
    setColour(juce::PopupMenu::textColourId, knobValueColour);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,
              juce::Colour(0xff2a3037));
    setColour(juce::PopupMenu::highlightedTextColourId,
              knobValueColour);
}

juce::Font DuskAudioProcessorEditor::LookAndFeel::getPopupMenuFont()
{
    auto font = juce::LookAndFeel_V4::getPopupMenuFont();
    if (useCompactPopupFont)
        font.setHeight(juce::jmax(9.0f, font.getHeight() - 1.5f));
    return font;
}

juce::Font DuskAudioProcessorEditor::LookAndFeel::getComboBoxFont(
    juce::ComboBox& box)
{
    auto font = juce::LookAndFeel_V4::getComboBoxFont(box);
    if (static_cast<bool>(box.getProperties().getWithDefault(
            "compactClosedCombo", false)))
        font.setHeight(font.getHeight() * 0.85f);
    return font;
}

juce::Label* DuskAudioProcessorEditor::LookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
    label->setFont(juce::Font(juce::FontOptions(12.5f)));
    return label;
}

void DuskAudioProcessorEditor::LookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float position, float startAngle, float endAngle, juce::Slider& slider)
{
    if (slider.getProperties().contains("animatedNormalised"))
        position = juce::jlimit(0.0f, 1.0f, static_cast<float>(
            slider.getProperties()["animatedNormalised"]));
    const auto radius = static_cast<float>(juce::jmin(width, height)) * 0.40f + 2.0f;
    const auto centre = juce::Point<float>(static_cast<float>(x + width) * 0.5f,
                                           static_cast<float>(y + height) * 0.5f);
    const auto angle = startAngle + position * (endAngle - startAngle);
    const bool enabled = slider.isEnabled();
    const auto trackColour = enabled ? knobTrackColour : juce::Colour(0xff25292e);
    const auto bodyColour = enabled ? knobBodyColour : juce::Colour(0xff202328);
    auto indicatorColour = enabled ? slider.findColour(juce::Slider::rotarySliderFillColourId)
                                   : juce::Colour(0xff555b63);

    if (enabled && slider.isMouseOverOrDragging())
        indicatorColour = indicatorColour.brighter(0.10f);

    float fillStartAngle = startAngle;
    float fillEndAngle = angle;
    if (static_cast<bool>(slider.getProperties().getWithDefault("hasCentreDetent", false)))
    {
        const double centreDetent = static_cast<double>(slider.getProperties()
            .getWithDefault("centreDetentValue", 0.0));
        const auto normalisedCentre = static_cast<float>(slider.getNormalisableRange()
            .convertTo0to1(centreDetent));
        const float centreAngle = startAngle + normalisedCentre * (endAngle - startAngle);
        fillStartAngle = juce::jmin(angle, centreAngle);
        fillEndAngle = juce::jmax(angle, centreAngle);
    }

    constexpr float ringThickness = 2.0f;
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius,
                        0.0f, startAngle, endAngle, true);
    g.setColour(trackColour);
    g.strokePath(track, juce::PathStrokeType(ringThickness,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    if (std::abs(fillEndAngle - fillStartAngle) > 0.0001f)
    {
        juce::Path fill;
        fill.addCentredArc(centre.x, centre.y, radius, radius,
                           0.0f, fillStartAngle, fillEndAngle, true);
        g.setColour(indicatorColour);
        g.strokePath(fill, juce::PathStrokeType(ringThickness,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    g.setColour(bodyColour);
    g.fillEllipse(centre.x - radius * 0.72f,
                  centre.y - radius * 0.72f,
                  radius * 1.44f,
                  radius * 1.44f);
    g.setColour(indicatorColour);
    g.drawLine(centre.x, centre.y,
               centre.x + std::sin(angle) * radius * 0.58f,
               centre.y - std::cos(angle) * radius * 0.58f,
               2.0f);
}

void DuskAudioProcessorEditor::LookAndFeel::drawLinearSlider(
    juce::Graphics& graphics, int x, int y, int width, int height,
    float sliderPosition, float minSliderPosition, float maxSliderPosition,
    juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (slider.getProperties().contains("animatedNormalised"))
    {
        const float animated = juce::jlimit(
            0.0f, 1.0f, static_cast<float>(
                slider.getProperties()["animatedNormalised"]));
        sliderPosition = style == juce::Slider::LinearVertical
            ? static_cast<float>(y + height - 1)
                - animated * static_cast<float>(height - 2)
            : static_cast<float>(x + 1)
                + animated * static_cast<float>(width - 2);
    }
    if (style != juce::Slider::LinearVertical
        && style != juce::Slider::LinearHorizontal)
    {
        juce::LookAndFeel_V4::drawLinearSlider(
            graphics, x, y, width, height, sliderPosition,
            minSliderPosition, maxSliderPosition, style, slider);
        return;
    }

    const bool enabled = slider.isEnabled();
    const auto accent = enabled
        ? slider.findColour(juce::Slider::rotarySliderFillColourId)
        : juce::Colour(0xff555b63);
    if (style == juce::Slider::LinearHorizontal)
    {
        const float left = static_cast<float>(x + 1);
        const float right = static_cast<float>(x + width - 1);
        const float centreX = 0.5f * (left + right);
        const float centreY = static_cast<float>(y + height / 2);
        graphics.setColour(enabled
            ? juce::Colour(0xffb5bac0).withAlpha(0.86f)
            : juce::Colour(0xff555b63).withAlpha(0.58f));
        graphics.drawLine(left, centreY, right, centreY, 1.25f);
        graphics.drawLine(centreX, centreY - 4.0f,
                          centreX, centreY + 4.0f, 1.0f);
        graphics.setColour(accent);
        graphics.fillRoundedRectangle(sliderPosition - 2.0f,
                                      centreY - 6.0f,
                                      4.0f, 12.0f, 1.0f);
        return;
    }

    const float centreX = static_cast<float>(x + width / 2);
    const float top = static_cast<float>(y + 1);
    const float bottom = static_cast<float>(y + height - 1);
    const float centreY = 0.5f * (top + bottom);
    graphics.setColour(enabled
        ? knobTrackColour.withAlpha(0.90f)
        : juce::Colour(0xff555b63).withAlpha(0.58f));
    graphics.drawLine(centreX, top, centreX, bottom, 1.25f);
    graphics.setColour(accent.withAlpha(enabled ? 0.92f : 0.42f));
    graphics.drawLine(centreX, centreY, centreX, sliderPosition, 2.6f);
    graphics.setColour(enabled
        ? knobTrackColour.withAlpha(0.90f)
        : juce::Colour(0xff555b63).withAlpha(0.58f));
    graphics.drawLine(centreX - 4.0f, centreY,
                      centreX + 4.0f, centreY, 1.0f);
    graphics.setColour(accent);
    graphics.fillRoundedRectangle(centreX - 6.0f,
                                  sliderPosition - 2.0f,
                                  12.0f, 4.0f, 1.0f);
}

void DuskAudioProcessorEditor::LookAndFeel::drawToggleButton(
    juce::Graphics& g, juce::ToggleButton& button,
    bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const bool ledOnly = static_cast<bool>(button.getProperties().getWithDefault("ledOnly", false));
    const bool statusLedOnly = static_cast<bool>(
        button.getProperties().getWithDefault("statusLedOnly", false));
    const bool snowflakeIcon = static_cast<bool>(
        button.getProperties().getWithDefault("snowflakeIcon", false));
    const bool listIcon = static_cast<bool>(
        button.getProperties().getWithDefault("listIcon", false));
    const int presetArrowDirection = static_cast<int>(
        button.getProperties().getWithDefault("presetArrowDirection", 0));
    const bool saveIcon = static_cast<bool>(
        button.getProperties().getWithDefault("saveIcon", false));
    const bool saveAsIcon = static_cast<bool>(
        button.getProperties().getWithDefault("saveAsIcon", false));
    const bool folderIcon = static_cast<bool>(
        button.getProperties().getWithDefault("folderIcon", false));
    const bool rangeLinkIcon = static_cast<bool>(
        button.getProperties().getWithDefault("rangeLinkIcon", false));
    const bool randomIcon = static_cast<bool>(
        button.getProperties().getWithDefault("randomIcon", false));
    const bool mixLockIcon = static_cast<bool>(
        button.getProperties().getWithDefault("mixLockIcon", false));
    const bool sequencerLabelStyle = static_cast<bool>(
        button.getProperties().getWithDefault("sequencerLabelStyle", false));
    const bool standardTextButton = static_cast<bool>(
        button.getProperties().getWithDefault("standardTextButton", false));
    const bool active = button.getToggleState();
    const bool enabled = button.isEnabled();
    auto activeColour = ledOnly ? juce::Colour(0xffff9363) : controlAccentColour;
    const auto configuredColour = button.getProperties()
        .getWithDefault("activeColour", juce::var()).toString();
    if (configuredColour.isNotEmpty())
        activeColour = juce::Colour::fromString(configuredColour);

    if (shouldDrawButtonAsHighlighted)
        activeColour = activeColour.brighter(0.10f);
    if (shouldDrawButtonAsDown)
        activeColour = activeColour.darker(0.15f);

    if (snowflakeIcon || listIcon || presetArrowDirection != 0
        || saveIcon || saveAsIcon || folderIcon || rangeLinkIcon
        || randomIcon || mixLockIcon || standardTextButton)
    {
        const auto buttonBounds = bounds.reduced(0.5f);
        if (! mixLockIcon)
        {
            g.setColour(active && enabled ? activeColour.darker(0.32f)
                                         : juce::Colour(0xff242a30));
            g.fillRoundedRectangle(buttonBounds, 3.0f);
            g.setColour(active && enabled ? activeColour.brighter(0.12f)
                                         : (shouldDrawButtonAsHighlighted
                                                ? knobLabelColour
                                                : juce::Colour(0xff59616a)));
            g.drawRoundedRectangle(buttonBounds, 3.0f, 1.0f);
        }

        const auto iconColour = mixLockIcon
            ? (active && enabled ? activeColour
                                 : juce::Colour(0xff69717b))
            : presetArrowDirection != 0
            ? knobIndicatorColour.withAlpha(enabled ? 0.9f : 0.2f)
            : (enabled
                   ? (active ? juce::Colours::white : knobValueColour)
                   : knobLabelColour.darker(0.45f));
        g.setColour(iconColour);
        if (mixLockIcon)
        {
            const auto centre = bounds.getCentre();
            const auto body = juce::Rectangle<float>(10.0f, 8.0f)
                .withCentre(centre.translated(0.0f, 2.5f));
            const auto shackle = juce::Rectangle<float>(7.0f, 8.0f)
                .withCentre(centre.translated(0.0f, -2.0f));
            g.drawRoundedRectangle(shackle, 3.5f, 1.5f);
            g.fillRoundedRectangle(body, 1.5f);
            g.setColour(juce::Colour(0xff242a30));
            g.fillEllipse(centre.x - 1.0f, centre.y + 1.0f,
                          2.0f, 2.0f);
        }
        else if (randomIcon)
        {
            g.setFont(juce::Font(juce::FontOptions(
                15.0f, juce::Font::bold)));
            g.drawText("!!!", button.getLocalBounds(),
                       juce::Justification::centred, false);
        }
        else if (standardTextButton)
        {
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.drawFittedText(button.getButtonText(),
                             button.getLocalBounds().reduced(5, 2),
                             juce::Justification::centred, 1);
        }
        else if (rangeLinkIcon)
        {
            const auto centre = bounds.getCentre();
            const float iconScale = static_cast<bool>(
                button.getProperties().getWithDefault(
                    "compactRangeLink", false)) ? 0.75f : 1.0f;
            const auto upper = juce::Rectangle<float>(
                10.0f * iconScale, 7.0f * iconScale)
                .withCentre(centre.translated(-3.5f * iconScale,
                                              -2.5f * iconScale));
            const auto lower = juce::Rectangle<float>(
                10.0f * iconScale, 7.0f * iconScale)
                .withCentre(centre.translated(3.5f * iconScale,
                                              2.5f * iconScale));
            g.drawRoundedRectangle(upper, 3.0f * iconScale,
                                   1.4f * iconScale);
            g.drawRoundedRectangle(lower, 3.0f * iconScale,
                                   1.4f * iconScale);
            g.drawLine(centre.x - 2.0f * iconScale,
                       centre.y - 1.5f * iconScale,
                       centre.x + 2.0f * iconScale,
                       centre.y + 1.5f * iconScale,
                       1.4f * iconScale);
        }
        else if (snowflakeIcon)
        {
            const auto centre = bounds.getCentre();
            const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight())
                               * 0.275f;
            for (int axis = 0; axis < 3; ++axis)
            {
                const float angle = static_cast<float>(axis)
                                  * juce::MathConstants<float>::pi / 3.0f;
                const juce::Point<float> direction(std::cos(angle),
                                                    std::sin(angle));
                g.drawLine(centre.x - direction.x * radius,
                           centre.y - direction.y * radius,
                           centre.x + direction.x * radius,
                           centre.y + direction.y * radius, 1.5f);
            }
            g.fillEllipse(centre.x - 1.25f, centre.y - 1.25f, 2.5f, 2.5f);
        }
        else if (listIcon)
        {
            const float left = bounds.getX() + bounds.getWidth() * 0.26f;
            const float lineLeft = bounds.getX() + bounds.getWidth() * 0.43f;
            const float lineRight = bounds.getRight() - bounds.getWidth() * 0.22f;
            for (int row = 0; row < 3; ++row)
            {
                const float y = bounds.getY() + bounds.getHeight()
                              * (0.31f + static_cast<float>(row) * 0.19f);
                g.fillEllipse(left - 1.25f, y - 1.25f, 2.5f, 2.5f);
                g.drawLine(lineLeft, y, lineRight, y, 1.5f);
            }
        }
        else if (presetArrowDirection != 0)
        {
            const auto centre = bounds.getCentre();
            const float arrowScale = static_cast<bool>(
                button.getProperties().getWithDefault(
                    "compactPresetArrow", false)) ? 0.75f : 1.0f;
            const float innerOffset = 2.0f * arrowScale;
            const float pointOffset = 3.0f * arrowScale;
            const float halfHeight = 7.0f * arrowScale;
            juce::Path arrow;
            if (presetArrowDirection < 0)
            {
                arrow.startNewSubPath(centre.x + innerOffset,
                                      centre.y - halfHeight);
                arrow.lineTo(centre.x - pointOffset, centre.y);
                arrow.lineTo(centre.x + innerOffset,
                             centre.y + halfHeight);
            }
            else
            {
                arrow.startNewSubPath(centre.x - innerOffset,
                                      centre.y - halfHeight);
                arrow.lineTo(centre.x + pointOffset, centre.y);
                arrow.lineTo(centre.x - innerOffset,
                             centre.y + halfHeight);
            }
            g.strokePath(arrow, juce::PathStrokeType(2.0f * arrowScale));
        }
        else if (folderIcon)
        {
            const auto icon = juce::Rectangle<float>(14.0f, 10.0f)
                .withCentre(bounds.getCentre().translated(0.0f, 1.0f));
            g.fillRoundedRectangle(icon, 1.5f);
            g.fillRect(icon.getX() + 1.5f, icon.getY() - 2.0f,
                       5.0f, 3.0f);
        }
        else
        {
            const auto icon = bounds.withSizeKeepingCentre(14.0f, 14.0f);

            g.drawRoundedRectangle(icon, 1.5f, 1.4f);
            g.fillRect(icon.getX() + 3.0f, icon.getY(),
                       icon.getWidth() - 6.0f, 5.0f);
            g.setColour(juce::Colour(0xff242a30));
            g.fillRect(icon.getX() + 5.0f, icon.getY() + 1.0f,
                       icon.getWidth() - 9.0f, 3.0f);
            g.setColour(iconColour);
            g.drawRect(icon.getX() + 3.5f, icon.getBottom() - 7.0f,
                       icon.getWidth() - 7.0f, 5.0f, 1.2f);

            if (saveAsIcon)
            {
                const auto pencilStart = juce::Point<float>(
                    icon.getRight() - 4.0f, icon.getBottom() + 2.5f);
                const auto pencilEnd = juce::Point<float>(
                    icon.getRight() + 3.0f, icon.getBottom() - 4.5f);
                g.drawLine({ pencilStart, pencilEnd }, 2.8f);
                g.fillEllipse(pencilEnd.x - 1.6f, pencilEnd.y - 1.6f,
                              3.2f, 3.2f);
                g.drawLine(pencilStart.x - 1.3f, pencilStart.y + 1.3f,
                           pencilStart.x + 1.3f, pencilStart.y - 1.3f,
                           1.3f);
            }
        }
        return;
    }

    const bool centredLed = ledOnly || statusLedOnly;
    const float diameter = centredLed
                         ? juce::jmin(bounds.getWidth(), bounds.getHeight()) - 6.0f
                         : 10.0f;
    const juce::Font toggleFont(sequencerLabelStyle
        ? juce::FontOptions(10.5f, juce::Font::bold)
        : juce::FontOptions(11.0f));
    const float textWidth = statusLedOnly ? 0.0f
        : juce::GlyphArrangement::getStringWidth(toggleFont, button.getButtonText());
    const float contentWidth = diameter + 5.0f + textWidth;
    const float contentX = bounds.getCentreX() - contentWidth * 0.5f;
    const float fixedLedCentreX = static_cast<float>(button.getProperties()
        .getWithDefault("fixedLedCentreX", -1.0f));
    const juce::Point<float> centre = centredLed
        ? bounds.getCentre()
        : juce::Point<float>(fixedLedCentreX >= 0.0f
                                 ? fixedLedCentreX
                                 : contentX + diameter * 0.5f,
                             bounds.getCentreY());
    const auto ledBounds = juce::Rectangle<float>(diameter, diameter).withCentre(centre);
    if (active && enabled)
    {
        g.setColour(activeColour.withAlpha(0.20f));
        g.fillEllipse(ledBounds.expanded(3.0f));
    }

    if (statusLedOnly)
    {
        const auto disabledActiveColour = activeColour.darker(0.72f);
        g.setColour(enabled
            ? (active ? activeColour : juce::Colour(0xff343940))
            : (active ? disabledActiveColour : juce::Colour(0xff282c31)));
        g.fillEllipse(ledBounds);
        g.setColour(enabled
            ? (active ? activeColour.brighter(0.30f) : juce::Colour(0xff596069))
            : (active ? disabledActiveColour.brighter(0.12f)
                      : juce::Colour(0xff41474e)));
        g.drawEllipse(ledBounds, 1.0f);
        if (button.getButtonText().isNotEmpty())
        {
            g.setColour(enabled
                ? (active ? juce::Colour(0xff17201b) : knobValueColour)
                : knobValueColour.darker(0.45f));
            g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
            g.drawText(button.getButtonText(), ledBounds.toNearestInt(),
                       juce::Justification::centred, false);
        }
        return;
    }

    if (ledOnly)
    {
        g.setColour(juce::Colour(0xff24282e));
        g.fillEllipse(ledBounds);
        g.setColour(active ? activeColour : juce::Colour(0xff69717b));
        g.drawEllipse(ledBounds, 1.0f);

        const float iconRadius = diameter * 0.285f;
        const float gapAngle = juce::MathConstants<float>::pi * 0.23f;
        juce::Path powerRing;
        powerRing.addCentredArc(centre.x, centre.y, iconRadius, iconRadius, 0.0f,
                               gapAngle,
                               juce::MathConstants<float>::twoPi - gapAngle,
                               true);
        g.strokePath(powerRing, juce::PathStrokeType(1.8f,
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        g.drawLine(centre.x, centre.y - iconRadius * 0.82f,
                   centre.x, centre.y + iconRadius * 0.10f, 1.8f);
        return;
    }

    g.setColour(active ? activeColour : juce::Colour(0xff343940));
    g.fillEllipse(ledBounds);
    g.setColour(active ? activeColour.brighter(0.30f) : juce::Colour(0xff596069));
    g.drawEllipse(ledBounds, 1.0f);

    g.setColour(button.isEnabled()
        ? button.findColour(juce::ToggleButton::textColourId)
        : knobLabelColour.darker(0.35f));
    g.setFont(toggleFont);
    const int textX = juce::roundToInt(fixedLedCentreX >= 0.0f
        ? centre.x + diameter * 0.5f + 5.0f
        : contentX + diameter + 5.0f);
    g.drawFittedText(button.getButtonText(), textX, 0,
                     juce::jmax(0, button.getWidth() - textX), button.getHeight(),
                     juce::Justification::centredLeft, 1);
}

void DuskAudioProcessorEditor::LookAndFeel::drawPopupMenuItem(
    juce::Graphics& g, const juce::Rectangle<int>& area,
    bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
    bool hasSubMenu, const juce::String& text,
    const juce::String& shortcutKeyText, const juce::Drawable* icon,
    const juce::Colour* textColour)
{
    const auto insetArea = area.withTrimmedLeft(3).withTrimmedRight(3);
    juce::LookAndFeel_V4::drawPopupMenuItem(
        g, insetArea, isSeparator, isActive, isHighlighted, isTicked,
        hasSubMenu, text, shortcutKeyText, icon, textColour);
}

void DuskAudioProcessorEditor::LookAndFeel::drawPopupMenuBackground(
    juce::Graphics& g, int width, int height)
{
    g.fillAll(juce::Colour(0xff14181d));
    g.setColour(popupBorderColour);
    g.drawRect(0, 0, width, height, 2);
}

juce::PopupMenu::Options
DuskAudioProcessorEditor::LookAndFeel::getOptionsForComboBoxPopupMenu(
    juce::ComboBox& box, juce::Label& label)
{
    return juce::PopupMenu::Options()
        .withTargetComponent(&box)
        .withTargetScreenArea(box.getScreenBounds())
        .withPreferredPopupDirection(
            juce::PopupMenu::Options::PopupDirection::downwards)
        .withMinimumWidth(box.getWidth())
        .withMaximumNumColumns(1)
        .withStandardItemHeight(label.getHeight());
}

DuskAudioProcessorEditor::DuskAudioProcessorEditor(DuskAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), responseDisplay(p)
{
    const auto storedSettings = loadInterfaceSettings();
    const auto presetsDirectory = DuskAudioProcessor::getPresetsDirectory();
    loadFavouritePresets();
    const auto storedPresetFile = processor.getCurrentPresetFile();
    const auto storedPresetFolder = storedPresetFile.getParentDirectory();
    if (storedPresetFile != juce::File()
        && (storedPresetFolder == presetsDirectory
            || storedPresetFolder.isAChildOf(presetsDirectory)))
    {
        currentPresetFile = storedPresetFile;
        selectedPresetFolder = storedPresetFolder;
    }

    logoDrawable = juce::Drawable::createFromImageData(
        BinaryData::logo_svg, static_cast<size_t>(BinaryData::logo_svgSize));
    setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(content);
    content.addMouseListener(this, true);
    content.setInterceptsMouseClicks(false, true);

    versionLabel.setText("v0.18.0", juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::centredLeft);
    versionLabel.setColour(juce::Label::textColourId, knobIndicatorColour);
    versionLabel.setFont(juce::Font(juce::FontOptions(8.5f,
                                                     juce::Font::bold)));
    content.addAndMakeVisible(versionLabel);

    const auto configureViewButton = [this](
        juce::ToggleButton& button, const juce::String& tooltip)
    {
        button.setClickingTogglesState(true);
        button.setRadioGroupId(0x4455534b);
        button.getProperties().set("fixedLedCentreX", 8.0f);
        button.getProperties().set(
            "activeColour", knobIndicatorColour.toString());
        button.setColour(juce::ToggleButton::textColourId, knobValueColour);
        button.setTooltip(tooltip);
        content.addAndMakeVisible(button);
    };
    configureViewButton(
        filterViewButton, "Show the main controls for Filter 1 and Filter 2");
    configureViewButton(
        designerViewButton,
        "Show both context-sensitive Filter Mode Designer areas");
    if (storedSettings.mainView.equalsIgnoreCase("Designer"))
        mainView = MainView::designer;
    filterViewButton.setToggleState(mainView == MainView::filter,
                                    juce::dontSendNotification);
    designerViewButton.setToggleState(mainView == MainView::designer,
                                      juce::dontSendNotification);
    filterViewButton.onClick = [this] { setMainView(MainView::filter); };
    designerViewButton.onClick = [this] { setMainView(MainView::designer); };

    filter1DriveDial = addDial(
        "filter1Drive", "DRIVE",
        "Filter 1 distortion amount. Placement determines whether its harmonics are created before or after the selected architecture.");
    filter2DriveDial = addDial(
        "filter2Drive", "DRIVE",
        "Filter 2 distortion amount. This engine owns its distortion type, amount and placement independently from Filter 1.");
    filter1CutoffDial = addDial(
        "filter1Cutoff", "CUTOFF",
        "Filter 1 cutoff or centre frequency from 20 Hz to 20 kHz for the selected response Type.");
    filter2CutoffDial = addDial(
        "filter2Cutoff", "CUTOFF",
        "Filter 2 cutoff or centre frequency from 20 Hz to 20 kHz for the selected response Type.");
    filter1ResonanceDial = addDial(
        "filter1Resonance", "RES",
        "Filter 1 resonance. Higher values strengthen the selected architecture's cutoff feedback and peak; each architecture maps it through its own stable resonance structure.");
    filter2ResonanceDial = addDial(
        "filter2Resonance", "RES",
        "Filter 2 resonance. Each filter and architecture keeps completely independent state and settings.");
    filter1ResonanceShapeDial = addDial(
        "filter1ResonanceShape", "RES SHAPE",
        "Shape Filter 1's resonance response. Turn left for a restrained, gradual build or right for an earlier, more aggressive peak. The centre position preserves the original Clean SVF response.");
    filter2ResonanceShapeDial = addDial(
        "filter2ResonanceShape", "RES SHAPE",
        "Shape Filter 2's resonance response independently. Negative values soften the build, positive values make it more immediate, and zero preserves the original response.");
    filter1ResonanceShapeDial->slider.setCentreDetent(0.0, 2.0);
    filter2ResonanceShapeDial->slider.setCentreDetent(0.0, 2.0);
    filter1ResonanceShapeDial->slider.setDoubleClickReturnValue(true, 0.0);
    filter2ResonanceShapeDial->slider.setDoubleClickReturnValue(true, 0.0);
    filter1BassLossDial = addDial(
        "filter1BassLoss", "BASS LOSS",
        "Model Filter 1's resonance-linked low-end thinning. At 0% the current Clean SVF response is unchanged; higher values progressively reduce deep bass as effective resonance rises.");
    filter2BassLossDial = addDial(
        "filter2BassLoss", "BASS LOSS",
        "Model Filter 2's resonance-linked low-end thinning independently. The effect follows Resonance and Resonance Shape, and naturally disappears at the non-resonant 6 dB slope.");
    filter1BassLossDial->slider.setDoubleClickReturnValue(true, 0.0);
    filter2BassLossDial->slider.setDoubleClickReturnValue(true, 0.0);
    filter1ResonanceLimitDial = addDial(
        "filter1ResonanceLimit", "RES LIMIT",
        "Set Filter 1's maximum available resonance independently from the basic RES position. Lower values create a restrained model; 100% preserves the current full range.");
    filter2ResonanceLimitDial = addDial(
        "filter2ResonanceLimit", "RES LIMIT",
        "Set Filter 2's maximum available resonance. This model-level ceiling applies consistently to LPF, BPF, HPF, Notch and Peak responses.");
    filter1ResonanceLimitDial->slider.setDoubleClickReturnValue(true, 100.0);
    filter2ResonanceLimitDial->slider.setDoubleClickReturnValue(true, 100.0);
    filter1CoreSaturationDial = addDial(
        "filter1CoreSaturation", "CORE SAT",
        "Add memory-safe saturation to Filter 1's completed core signal. The upper range is deliberately pronounced; 0% remains exactly transparent.");
    filter2CoreSaturationDial = addDial(
        "filter2CoreSaturation", "CORE SAT",
        "Add independent memory-safe saturation to Filter 2. It preserves the linear small-signal response while becoming strongly coloured near maximum.");
    filter1CoreSaturationDial->slider.setDoubleClickReturnValue(true, 0.0);
    filter2CoreSaturationDial->slider.setDoubleClickReturnValue(true, 0.0);
    filter1SaturationAsymmetryDial = addDial(
        "filter1SaturationAsymmetry", "SAT ASYM",
        "Give Filter 1 different positive and negative saturation thresholds. 0% is symmetrical; Core Sat must be above 0%.");
    filter2SaturationAsymmetryDial = addDial(
        "filter2SaturationAsymmetry", "SAT ASYM",
        "Reverse Filter 2's stronger saturation polarity by moving left or right. The asymmetric difference remains DC-blocked.");
    filter1SaturationAsymmetryDial->slider.setCentreDetent(0.0, 2.0);
    filter2SaturationAsymmetryDial->slider.setCentreDetent(0.0, 2.0);
    filter1SaturationAsymmetryDial->slider.setDoubleClickReturnValue(
        true, 0.0);
    filter2SaturationAsymmetryDial->slider.setDoubleClickReturnValue(
        true, 0.0);
    filter1PoleSpreadDial = addDial(
        "filter1PoleSpread", "POLE SPRD",
        "Separate Filter 1's cascaded poles by up to three octaves above 12 dB/octave, strongly broadening and reshaping the transition.");
    filter2PoleSpreadDial = addDial(
        "filter2PoleSpread", "POLE SPRD",
        "Separate Filter 2's cascaded poles above 12 dB/octave independently. The response graph shows the resulting transition shape; the control has no effect at 12 dB/octave or below.");
    filter1PoleSpreadDial->slider.setDoubleClickReturnValue(true, 0.0);
    filter2PoleSpreadDial->slider.setDoubleClickReturnValue(true, 0.0);
    filter1FeedbackDial = addDial(
        "filter1Feedback", "FEEDBACK",
        "Return Filter 1's previous completed output into its core. Higher values intensify resonance and can become self-sustaining; the dedicated bounded return keeps the feedback state controlled.");
    filter2FeedbackDial = addDial(
        "filter2Feedback", "FEEDBACK",
        "Return Filter 2's previous completed output into its core independently. The response display follows the linear feedback response before nonlinear limiting takes over.");
    filter1FeedbackDial->slider.setDoubleClickReturnValue(true, 0.0);
    filter2FeedbackDial->slider.setDoubleClickReturnValue(true, 0.0);
    filter1FeedbackLimitDial = addDial(
        "filter1FeedbackLimit", "FB LIMIT",
        "Set Filter 1's feedback-return ceiling from 0.1 to 8. Lower values compress oscillation earlier; Feedback must be above 0%.");
    filter2FeedbackLimitDial = addDial(
        "filter2FeedbackLimit", "FB LIMIT",
        "Set Filter 2's feedback-return ceiling independently. The limiter preserves a unity small-signal response, so the graph shows Feedback while this control shapes high-level behaviour.");
    filter1FeedbackLimitDial->slider.setDoubleClickReturnValue(true, 50.0);
    filter2FeedbackLimitDial->slider.setDoubleClickReturnValue(true, 50.0);
    filter1LadderStageSaturationDial = addDial(
        "filter1LadderStageSaturation", "STAGE SAT",
        "Saturate every Filter 1 Ladder stage. The upper range now compresses strongly while retaining unity small-signal response.");
    filter2LadderStageSaturationDial = addDial(
        "filter2LadderStageSaturation", "STAGE SAT",
        "Set Filter 2 Ladder stage saturation independently. This is internal architecture colour, separate from the selectable Pre/Post distortion bank.");
    filter1LadderFeedbackSaturationDial = addDial(
        "filter1LadderFeedbackSaturation", "FB SAT",
        "Strongly soft-limit Filter 1's Ladder resonance return. It becomes pronounced with high Resonance while 0% remains linear.");
    filter2LadderFeedbackSaturationDial = addDial(
        "filter2LadderFeedbackSaturation", "FB SAT",
        "Saturate Filter 2's Ladder resonance return independently. This changes feedback character rather than stage colour and has no effect while Resonance is 0%.");
    filter1LadderResonanceCompensationDial = addDial(
        "filter1LadderResonanceCompensation", "RES COMP",
        "Restore Filter 1 Ladder level as Resonance rises, reaching four-times input gain at the maximum combined setting.");
    filter2LadderResonanceCompensationDial = addDial(
        "filter2LadderResonanceCompensation", "RES COMP",
        "Compensate Filter 2 Ladder resonance independently. The response graph follows this low-level gain compensation exactly.");
    filter1LadderStageSaturationDial->slider.setDoubleClickReturnValue(
        true, 35.0);
    filter2LadderStageSaturationDial->slider.setDoubleClickReturnValue(
        true, 35.0);
    filter1LadderFeedbackSaturationDial->slider.setDoubleClickReturnValue(
        true, 0.0);
    filter2LadderFeedbackSaturationDial->slider.setDoubleClickReturnValue(
        true, 0.0);
    filter1LadderResonanceCompensationDial->slider
        .setDoubleClickReturnValue(true, 50.0);
    filter2LadderResonanceCompensationDial->slider
        .setDoubleClickReturnValue(true, 50.0);
    filter1OtaCellDriveDial = addDial(
        "filter1OtaCellDrive", "CELL DRIVE",
        "Drive Filter 1's OTA cells internally. The upper range now compresses aggressively while 0% remains linear.");
    filter2OtaCellDriveDial = addDial(
        "filter2OtaCellDrive", "CELL DRIVE",
        "Drive Filter 2's OTA cells independently. This is architecture colour inside the integrator chain, separate from the selectable Pre/Post distortion bank.");
    filter1OtaFeedbackDampingDial = addDial(
        "filter1OtaFeedbackDamping", "FB DAMP",
        "Darken and progressively restrain Filter 1's OTA resonance return. Resonance must be above 0% for an audible result.");
    filter2OtaFeedbackDampingDial = addDial(
        "filter2OtaFeedbackDamping", "FB DAMP",
        "Darken Filter 2's OTA resonance return independently. It has no audible effect while Resonance is 0% and is included in the calculated response.");
    filter1OtaResonanceCompensationDial = addDial(
        "filter1OtaResonanceCompensation", "RES COMP",
        "Restore Filter 1 OTA level as Resonance rises, reaching four-times input gain at the maximum combined setting.");
    filter2OtaResonanceCompensationDial = addDial(
        "filter2OtaResonanceCompensation", "RES COMP",
        "Restore Filter 2 OTA resonance level independently. The response display follows this low-level compensation exactly.");
    filter1OtaCellDriveDial->slider.setDoubleClickReturnValue(true, 35.0);
    filter2OtaCellDriveDial->slider.setDoubleClickReturnValue(true, 35.0);
    filter1OtaFeedbackDampingDial->slider.setDoubleClickReturnValue(
        true, 35.0);
    filter2OtaFeedbackDampingDial->slider.setDoubleClickReturnValue(
        true, 35.0);
    filter1OtaResonanceCompensationDial->slider
        .setDoubleClickReturnValue(true, 50.0);
    filter2OtaResonanceCompensationDial->slider
        .setDoubleClickReturnValue(true, 50.0);
    for (auto* designerDial : std::array<Dial*, 28>
         { filter1ResonanceShapeDial, filter2ResonanceShapeDial,
           filter1ResonanceLimitDial, filter2ResonanceLimitDial,
           filter1BassLossDial, filter2BassLossDial,
           filter1CoreSaturationDial, filter2CoreSaturationDial,
           filter1SaturationAsymmetryDial, filter2SaturationAsymmetryDial,
           filter1PoleSpreadDial, filter2PoleSpreadDial,
           filter1FeedbackDial, filter2FeedbackDial,
           filter1FeedbackLimitDial, filter2FeedbackLimitDial,
           filter1LadderStageSaturationDial,
           filter2LadderStageSaturationDial,
           filter1LadderFeedbackSaturationDial,
           filter2LadderFeedbackSaturationDial,
           filter1LadderResonanceCompensationDial,
           filter2LadderResonanceCompensationDial,
           filter1OtaCellDriveDial, filter2OtaCellDriveDial,
           filter1OtaFeedbackDampingDial,
           filter2OtaFeedbackDampingDial,
           filter1OtaResonanceCompensationDial,
           filter2OtaResonanceCompensationDial })
        designerDial->label.setFont(juce::Font(juce::FontOptions(
            9.0f, juce::Font::bold)));
    filter1SlopeDial = addDial(
        "filter1Slope", "SLOPE",
        "Continuously adjust Filter 1 from 6 to 48 dB/octave using the selected architecture's own pole and stage structure.");
    filter2SlopeDial = addDial(
        "filter2Slope", "SLOPE",
        "Continuously adjust Filter 2 from 6 to 48 dB/octave independently from Filter 1.");
    filter1SlopeDial->slider.setDoubleClickReturnValue(true, 12.0);
    filter2SlopeDial->slider.setDoubleClickReturnValue(true, 12.0);
    mixDial = addDial(
        "mix", "MIX",
        "Blend between the dry signal and Dusk's routed dual-filter effect path.");
    mixDial->slider.setCentreDetent(50.0, 2.0);
    mixDial->slider.setDoubleClickReturnValue(true, 50.0);
    outputDial = addDial(
        "output", "OUT",
        "Adjust the final output level. The 0 dB position has a tactile detent.");
    outputDial->slider.setCentreDetent(0.0, 0.6);
    outputDial->slider.setDoubleClickReturnValue(true, 0.0);
    themeDial = addDial(
        "", "THEME",
        "Turn clockwise for full filter colour or left for charcoal. The fixed colour bar always remains visible.",
        false);
    themeDial->label.setJustificationType(juce::Justification::centred);
    themeDial->slider.setRange(0.0, 100.0, 0.1);
    themeDial->slider.setValue(storedSettings.themePercent,
                               juce::dontSendNotification);
    themeDial->slider.setDoubleClickReturnValue(true, 100.0);
    themeDial->slider.setTextBoxStyle(
        juce::Slider::NoTextBox, false, 0, 0);
    themeDial->slider.onValueChange = [this]
    {
        information.setText(
            contextHelpText(
                "Theme",
                "Turn clockwise for full Filter 1 / Filter 2 colour, or left for charcoal."),
            juce::dontSendNotification);
        repaint();
    };

    browserMixDial = addDial(
        "mix", "MIX",
        "Browser copy of the main Mix control. It changes the same Mix level without becoming a sequencer target.");
    browserMixDial->parameterId.clear();
    browserMixDial->label.parameterIdProvider = {};
    browserMixDial->label.setInterceptsMouseClicks(false, false);
    browserMixDial->label.setTooltip("Browser copy of the main Mix control");
    browserMixDial->slider.setTextBoxStyle(
        juce::Slider::NoTextBox, false, 0, 0);
    browserMixDial->slider.setCentreDetent(50.0, 2.0);
    browserMixDial->slider.setDoubleClickReturnValue(true, 50.0);
    browserMixDial->slider.setTooltip(
        "Adjust Mix while the preset browser is open");
    browserMixDial->slider.setVisible(false);
    browserMixDial->label.setVisible(false);

    const auto configurePanelTitle = [this](
        juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, knobValueColour);
        label.setFont(juce::Font(juce::FontOptions(
            10.5f, juce::Font::bold)));
        content.addAndMakeVisible(label);
    };
    configurePanelTitle(filter1TitleLabel, "FILTER 1");
    configurePanelTitle(filter2TitleLabel, "FILTER 2");

    const auto configureDesignerTitle = [this](
        juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, knobValueColour);
        label.setFont(juce::Font(juce::FontOptions(
            10.5f, juce::Font::bold)));
        label.setMinimumHorizontalScale(1.0f);
        content.addChildComponent(label);
    };
    configureDesignerTitle(designer1Placeholder, "FILTER 1 MODE DESIGNER");
    configureDesignerTitle(designer2Placeholder, "FILTER 2 MODE DESIGNER");

    responseDisplay.colourForFilter = [this](int filterIndex)
    {
        return filterAccent(filterIndex);
    };
    content.addAndMakeVisible(responseDisplay);

    const auto configureCombo = [this](SteppedComboBox& box,
                                        juce::Label& label,
                                        const juce::String& title,
                                        const juce::String& tooltip)
    {
        box.setJustificationType(juce::Justification::centred);
        box.setColour(juce::ComboBox::backgroundColourId,
                      juce::Colour(0xff19151f));
        box.setColour(juce::ComboBox::textColourId, knobValueColour);
        box.setColour(juce::ComboBox::outlineColourId, knobTrackColour);
        box.setColour(juce::ComboBox::arrowColourId, knobIndicatorColour);
        box.setScrollWheelEnabled(true);
        box.setName(title);
        box.getProperties().set(contextHelpTitleProperty, title);
        box.setTooltip(tooltip);
        content.addAndMakeVisible(box);
        label.setText(title.toUpperCase(), juce::dontSendNotification);
        label.setName(title);
        label.getProperties().set(contextHelpTitleProperty, title);
        label.setTooltip(tooltip);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, knobLabelColour);
        label.setFont(juce::Font(juce::FontOptions(
            10.5f, juce::Font::bold)));
        content.addAndMakeVisible(label);
    };

    juce::StringArray distortionTypeNames {
        "Soft Clip", "Saturation", "Tube", "Rectify", "Crackle",
        "Lightning", "Amp", "Bumpy", "Asym", "Razor", "Pixelated",
        "Comb", "Stressed", "Samurai", "Fresh", "Hard Clip", "Fuzz"
    };
    juce::StringArray architectureNames;
    for (const auto& descriptor : dusk::architecture::descriptors)
        architectureNames.add(descriptor.displayName);
    distortionTypeNames.sortNatural();
    routingBox.addItemList(
        { "Filter 1", "Filter 2", "1 > 2", "1 + 2" }, 1);
    filter1ArchitectureBox.addItemList(architectureNames, 1);
    filter2ArchitectureBox.addItemList(architectureNames, 1);
    filter1TypeBox.addItemList(
        { "LPF", "BPF", "HPF", "NOTCH", "PEAK" }, 1);
    filter2TypeBox.addItemList(
        { "LPF", "BPF", "HPF", "NOTCH", "PEAK" }, 1);
    filter1DistortionTypeBox.addItemList(distortionTypeNames, 1);
    filter2DistortionTypeBox.addItemList(distortionTypeNames, 1);
    filter1DistortionPlacementBox.addItemList(
        { "Pre", "Internal", "Post" }, 1);
    filter2DistortionPlacementBox.addItemList(
        { "Pre", "Internal", "Post" }, 1);
    configureCombo(routingBox, routingLabel, "Routing",
        "Choose Filter 1 only, Filter 2 only, serial Filter 1 into Filter 2, or parallel processing.");
    configureCombo(filter1ArchitectureBox, filter1ArchitectureLabel,
        "Architecture",
        "Choose Filter 1's genuine DSP structure. Factory and User Models configure its supported Designer controls rather than replace the architecture.");
    configureCombo(filter2ArchitectureBox, filter2ArchitectureLabel,
        "Architecture",
        "Choose Filter 2's DSP structure independently. Its Designer changes to the controls supported by the selected architecture.");
    configureCombo(filter1TypeBox, filter1TypeLabel, "Type",
        "Choose Filter 1's standard response: low-pass, band-pass, high-pass, notch or resonant peak.");
    configureCombo(filter2TypeBox, filter2TypeLabel, "Type",
        "Choose Filter 2's standard response independently from Filter 1.");
    configureCombo(filter1DistortionTypeBox, filter1DistortionTypeLabel,
        "Dist Type", "Choose Filter 1's independent distortion model.");
    configureCombo(filter2DistortionTypeBox, filter2DistortionTypeLabel,
        "Dist Type", "Choose Filter 2's independent distortion model.");
    configureCombo(filter1DistortionPlacementBox,
        filter1DistortionPlacementLabel, "Placement",
        "Place Filter 1 distortion before its input, inside its feedback path, or after its output.");
    configureCombo(filter2DistortionPlacementBox,
        filter2DistortionPlacementLabel, "Placement",
        "Place Filter 2 distortion before its input, inside its feedback path, or after its output.");
    configureCombo(filter1ModelBox, filter1ModelLabel, "Model",
        "Recall a Factory or User configuration of Filter 1's Architecture and Designer controls. Standard FILTER controls and Filter 2 remain unchanged.");
    configureCombo(filter2ModelBox, filter2ModelLabel, "Model",
        "Recall a Factory or User configuration of Filter 2's Architecture and Designer controls. Standard FILTER controls and Filter 1 remain unchanged.");
    const auto configureModelMenuButton = [this](
        juce::ToggleButton& button, int filterIndex)
    {
        const auto filterName = juce::String("Filter ")
            + juce::String(filterIndex);
        button.setName(filterName + " Model Menu");
        button.setButtonText("...");
        button.setClickingTogglesState(false);
        button.getProperties().set("standardTextButton", true);
        button.getProperties().set(
            "activeColour", knobIndicatorColour.toString());
        button.setTooltip(
            "Save the current " + filterName
            + " Architecture and Designer settings as a User Model, refresh the list, or open the Models folder");
        button.onClick = [this, filterIndex]
        {
            showFilterModelMenu(filterIndex);
        };
        content.addAndMakeVisible(button);
    };
    configureModelMenuButton(filter1ModelMenuButton, 1);
    configureModelMenuButton(filter2ModelMenuButton, 2);
    filter1ModelBox.setTextWhenNothingSelected("CUSTOM");
    filter2ModelBox.setTextWhenNothingSelected("CUSTOM");
    filter1ModelBox.onChange = [this]
    {
        handleFilterModelSelection(1);
    };
    filter2ModelBox.onChange = [this]
    {
        handleFilterModelSelection(2);
    };

    routingAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.parameters, "routing", routingBox);
    filter1ArchitectureAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.parameters, "filter1Architecture",
            filter1ArchitectureBox);
    filter2ArchitectureAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.parameters, "filter2Architecture",
            filter2ArchitectureBox);
    filter1TypeAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.parameters, "filter1Type", filter1TypeBox);
    filter2TypeAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.parameters, "filter2Type", filter2TypeBox);
    filter1DistortionTypeAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.parameters, "filter1DistortionType",
            filter1DistortionTypeBox);
    filter2DistortionTypeAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.parameters, "filter2DistortionType",
            filter2DistortionTypeBox);
    filter1DistortionPlacementAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.parameters, "filter1DistortionPlacement",
            filter1DistortionPlacementBox);
    filter2DistortionPlacementAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.parameters, "filter2DistortionPlacement",
            filter2DistortionPlacementBox);
    filter1ArchitectureBox.onChange = [this]
    {
        if (! applyingFilterModel)
            markFilterModelCustom(1);
        updateArchitectureContext();
        updatePresetBrowserVisibility();
    };
    filter2ArchitectureBox.onChange = [this]
    {
        if (! applyingFilterModel)
            markFilterModelCustom(2);
        updateArchitectureContext();
        updatePresetBrowserVisibility();
    };
    updateArchitectureContext();
    refreshFilterModelBoxes();

    const auto configureFilterPower = [this](
        juce::ToggleButton& button, const juce::String& filterName)
    {
        button.setClickingTogglesState(true);
        button.getProperties().set("statusLedOnly", true);
        button.getProperties().set(
            "activeColour", linkLedColour.toString());
        button.setName(filterName + " Power");
        button.setTooltip(
            "Switch " + filterName
            + " processing on or off. Lit amber means the filter is on.");
        content.addAndMakeVisible(button);
    };
    configureFilterPower(filter1PowerButton, "Filter 1");
    configureFilterPower(filter2PowerButton, "Filter 2");
    filter1PowerAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.parameters, "filter1Enabled", filter1PowerButton);
    filter2PowerAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.parameters, "filter2Enabled", filter2PowerButton);
    const auto configureDistortionHelp = [this](
        SteppedComboBox& box, const juce::String& filterName)
    {
        auto* const targetBox = &box;
        box.onChange = [this, targetBox, filterName]
        {
            const int type = DuskAudioProcessor::distortionAlgorithmForChoice(
                targetBox->getSelectedItemIndex());
            information.setText(
                contextHelpText(
                    filterName + " Dist Type: " + targetBox->getText(),
                    juce::String(
                        distortionSummaries[static_cast<size_t>(type)])
                        + ". "
                        + distortionDetails[static_cast<size_t>(type)]),
                juce::dontSendNotification);
        };
    };
    configureDistortionHelp(filter1DistortionTypeBox, "FILTER 1");
    configureDistortionHelp(filter2DistortionTypeBox, "FILTER 2");

    const auto configureFilterTypeHelp = [this](
        SteppedComboBox& box, const juce::String& filterName)
    {
        auto* const targetBox = &box;
        box.onChange = [this, targetBox, filterName]
        {
            constexpr std::array<const char*, 5> details
            {
                "Low-pass retains frequencies below Cutoff.",
                "Band-pass retains a resonant band around Cutoff using the selected architecture's own available outputs and pole taps.",
                "High-pass retains frequencies above Cutoff.",
                "Notch rejects a resonant band around Cutoff.",
                "Peak produces the selected architecture's resonant peak response; it is not a separate parametric-EQ bell with an independent gain control."
            };
            const int type = juce::jlimit(
                0, static_cast<int>(details.size()) - 1,
                targetBox->getSelectedItemIndex());
            information.setText(
                contextHelpText(
                    filterName + " Type: " + targetBox->getText(),
                    details[static_cast<size_t>(type)]),
                juce::dontSendNotification);
        };
    };
    configureFilterTypeHelp(filter1TypeBox, "FILTER 1");
    configureFilterTypeHelp(filter2TypeBox, "FILTER 2");

    const auto trackLockedMix = [this](Dial& dial)
    {
        const auto existing = dial.slider.onValueChange;
        dial.slider.onValueChange = [this, &dial, existing]
        {
            if (existing != nullptr)
                existing();
            if (mixLockButton.getToggleState() && ! restoringLockedMix)
                lockedMixValue = dial.slider.getValue();
        };
    };
    trackLockedMix(*mixDial);
    trackLockedMix(*browserMixDial);
    mixLockButton.setClickingTogglesState(true);
    mixLockButton.setName("Mix Lock");
    mixLockButton.getProperties().set("mixLockIcon", true);
    mixLockButton.getProperties().set(
        "activeColour", knobIndicatorColour.toString());
    mixLockButton.setTooltip(
        "Lock the current Mix level so loading presets cannot change it");
    mixLockButton.onClick = [this]
    {
        if (mixLockButton.getToggleState() && mixDial != nullptr)
            lockedMixValue = mixDial->slider.getValue();
        information.setText(
            mixLockButton.getToggleState()
                ? "MIX LOCK: ON | Presets retain the current Mix level."
                : "MIX LOCK: OFF | Presets can load their saved Mix level.",
            juce::dontSendNotification);
    };
    content.addAndMakeVisible(mixLockButton);

    bypassButton.setName("Power");
    bypassButton.setTooltip("Switch Dusk processing on or off");
    bypassButton.getProperties().set("ledOnly", true);
    bypassButton.getProperties().set(
        "activeColour", powerLedColour.toString());
    content.addAndMakeVisible(bypassButton);
    if (auto* parameter = processor.parameters.getParameter("bypass"))
        bypassAttachment = std::make_unique<juce::ParameterAttachment>(
            *parameter,
            [this](float value)
            {
                bypassButton.setToggleState(
                    value < 0.5f, juce::dontSendNotification);
            });
    bypassButton.onClick = [this]
    {
        // The host parameter is Bypass, while the UI presents Power.
        if (auto* parameter = processor.parameters.getParameter("bypass"))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(
                bypassButton.getToggleState() ? 0.0f : 1.0f);
            parameter->endChangeGesture();
        }
    };
    if (bypassAttachment != nullptr)
        bypassAttachment->sendInitialUpdate();

    presetBrowserButton.setName("Preset Browser");
    presetBrowserButton.setTooltip("Open or close the preset browser");
    presetBrowserButton.getProperties().set("listIcon", true);
    presetBrowserButton.getProperties().set(
        "activeColour", presetAccentColour.toString());
    presetBrowserButton.setClickingTogglesState(true);
    presetBrowserButton.onClick = [this]
    {
        setPresetBrowserOpen(presetBrowserButton.getToggleState());
    };
    content.addAndMakeVisible(presetBrowserButton);

    const auto configurePresetStepButton = [this](
        juce::ToggleButton& button, int direction,
        const juce::String& description)
    {
        button.setName(description);
        button.setTooltip(description);
        button.getProperties().set("presetArrowDirection", direction);
        button.setClickingTogglesState(false);
        content.addAndMakeVisible(button);
    };
    configurePresetStepButton(previousPresetButton, -1, "Previous preset");
    configurePresetStepButton(nextPresetButton, 1, "Next preset");
    previousPresetButton.onClick = [this] { stepPreset(-1); };
    nextPresetButton.onClick = [this] { stepPreset(1); };

    currentPresetLabel.setJustificationType(juce::Justification::centred);
    currentPresetLabel.setColour(juce::Label::backgroundColourId,
                                 juce::Colour(0xff14181d));
    currentPresetLabel.setColour(juce::Label::outlineColourId,
                                 knobTrackColour);
    currentPresetLabel.setColour(juce::Label::textColourId,
                                 knobValueColour);
    currentPresetLabel.setFont(juce::Font(
        juce::FontOptions(10.0f, juce::Font::bold)));
    currentPresetLabel.setMinimumHorizontalScale(0.68f);
    currentPresetLabel.setInterceptsMouseClicks(true, false);
    currentPresetLabel.setName("Current Preset");
    currentPresetLabel.onStep = [this](int direction) { stepPreset(direction); };
    currentPresetLabel.onShowMenu = [this] { showPresetQuickMenu(); };
    content.addAndMakeVisible(currentPresetLabel);

    const auto configureBrowserHeading = [this](
        juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, knobLabelColour);
        label.setFont(juce::Font(juce::FontOptions(
            9.5f, juce::Font::bold)));
        content.addChildComponent(label);
    };
    configureBrowserHeading(presetFolderLabel, "FOLDERS");
    configureBrowserHeading(presetListLabel, "PRESETS");
    presetFolderTree.setRootItemVisible(false);
    presetFolderTree.setDefaultOpenness(false);
    presetFolderTree.setIndentSize(14);
    presetFolderTree.setColour(juce::TreeView::backgroundColourId,
                               juce::Colour(0xff11151a));
    presetFolderTree.setColour(juce::TreeView::linesColourId,
                               knobTrackColour);
    presetFolderTree.setColour(
        juce::TreeView::selectedItemBackgroundColourId,
        juce::Colours::transparentBlack);
    content.addChildComponent(presetFolderTree);
    presetList.setRowHeight(22);
    presetList.setColour(juce::ListBox::backgroundColourId,
                         juce::Colour(0xff11151a));
    presetList.setColour(juce::ListBox::outlineColourId, knobTrackColour);
    presetList.setOutlineThickness(1);
    presetList.onHorizontalStep = [this](int direction)
    {
        return handlePresetCursorKey(direction);
    };
    content.addChildComponent(presetList);

    const auto configureBrowserActionButton = [this](
        juce::ToggleButton& button, const juce::String& name,
        const juce::Identifier& icon)
    {
        button.setName(name);
        button.setTooltip(name);
        button.getProperties().set(icon, true);
        button.getProperties().set(
            "activeColour", presetAccentColour.toString());
        button.setClickingTogglesState(false);
        content.addChildComponent(button);
    };
    configureBrowserActionButton(
        presetSaveButton, "Save preset", juce::Identifier("saveIcon"));
    configureBrowserActionButton(
        presetSaveAsButton, "Save preset as", juce::Identifier("saveAsIcon"));
    configureBrowserActionButton(
        presetOpenFolderButton, "Open current preset folder",
        juce::Identifier("folderIcon"));
    presetSaveButton.onClick = [this] { saveCurrentPreset(); };
    presetSaveAsButton.onClick = [this] { showPresetSaveAs(); };
    presetOpenFolderButton.onClick = [this]
    {
        const auto root = DuskAudioProcessor::getPresetsDirectory();
        auto folder = selectedPresetFolder;
        if (! folder.isDirectory()
            || (folder != root && ! folder.isAChildOf(root)))
            folder = root;
        if (! folder.startAsProcess())
            information.setText(
                "PRESET FOLDER | The current folder could not be opened.",
                juce::dontSendNotification);
    };

    randomButton.setClickingTogglesState(false);
    randomButton.setName("Randomise");
    randomButton.getProperties().set("randomIcon", true);
    randomButton.getProperties().set(
        "activeColour", knobIndicatorColour.toString());
    randomButton.setTooltip(
        "Randomise both filters' standard controls and Clean SVF, Ladder and OTA Designer values plus a safe OUT level while leaving Architecture, routing, filter power, placement, Mix, main Power and the sequencer unchanged");
    randomButton.onClick = [this] { randomiseEffectParameters(); };
    content.addAndMakeVisible(randomButton);

    information.setText(
        "DUSK 0.18.0 | Pre, Internal and Post now provide three genuine distortion positions.",
        juce::dontSendNotification);
    information.setColour(juce::Label::textColourId,
                          juce::Colours::lightgrey);
    information.setJustificationType(juce::Justification::centredLeft);
    information.setFont(juce::FontOptions(11.5f));
    information.setMinimumHorizontalScale(1.0f);
    content.addAndMakeVisible(information);

    sequencerPanel = std::make_unique<SequencerPanel>(
        processor, lookAndFeel,
        [this] { return panelColour(1); },
        [this](const juce::String& helpText)
        {
            information.setText(helpText, juce::dontSendNotification);
        });
    content.addAndMakeVisible(*sequencerPanel);
    sequencerPanel->setAnimate(storedSettings.sequencerAnimate);
    sequencerPanel->onAnimateChanged = [this]
    {
        updateAnimatedParameterDisplays();
        saveInterfaceSettings();
    };
    sequencerOpen = storedSettings.sequencerOpen;
    sequencerPanel->setVisible(sequencerOpen);

    sequencerButton.setClickingTogglesState(true);
    sequencerButton.setName("Sequencer");
    sequencerButton.setToggleState(sequencerOpen,
                                   juce::dontSendNotification);
    sequencerButton.getProperties().set("standardTextButton", true);
    sequencerButton.getProperties().set(
        "activeColour", controlAccentColour.toString());
    sequencerButton.setTooltip("Show or hide the sequencer");
    sequencerButton.onClick = [this]
    {
        setSequencerOpen(sequencerButton.getToggleState());
    };
    content.addAndMakeVisible(sequencerButton);

    updateSequencerTargetHighlights();
    updatePresetNavigationDisplay();
    updatePresetBrowserVisibility();
    setResizable(true, true);
    setResizeLimits(695, sequencerOpen ? 414 : 207,
                    1852, sequencerOpen ? 1104 : 552);
    if (auto* editorConstrainer = getConstrainer())
        editorConstrainer->setFixedAspectRatio(
            static_cast<double>(designWidth)
                / static_cast<double>(activeDesignHeight()));
    const float scale = static_cast<float>(storedSettings.scalePercent) * 0.01f;
    setSize(juce::roundToInt(static_cast<float>(designWidth) * scale),
            juce::roundToInt(static_cast<float>(activeDesignHeight()) * scale));
    observedEditorWidth = getWidth();
    observedThemeValue = themeDial->slider.getValue();
    saveInterfaceSettings();
    startTimerHz(30);
}

DuskAudioProcessorEditor::~DuskAudioProcessorEditor()
{
    stopTimer();
    saveInterfaceSettings();
    if (presetFolderNameWindow != nullptr)
    {
        presetFolderNameWindow->exitModalState(0);
        presetFolderNameWindow->setLookAndFeel(nullptr);
        presetFolderNameWindow.reset();
    }
    content.removeMouseListener(this);
    presetFolderTree.setRootItem(nullptr);
    presetFolderRoot.reset();
    setLookAndFeel(nullptr);
}

bool DuskAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::leftKey)
        return handlePresetCursorKey(-1);
    if (key.getKeyCode() == juce::KeyPress::rightKey)
        return handlePresetCursorKey(1);
    return juce::AudioProcessorEditor::keyPressed(key);
}

void DuskAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    if (! presetBrowserOpen || ! event.mods.isPopupMenu())
        return;
    for (auto* component = event.eventComponent; component != nullptr;
         component = component->getParentComponent())
    {
        if (component != &presetList)
            continue;
        const auto relative = event.getEventRelativeTo(&presetList);
        const int row = presetList.getRowContainingPosition(
            juce::roundToInt(relative.position.x),
            juce::roundToInt(relative.position.y));
        if (row < 0)
            showPresetContextMenu({}, event.getScreenPosition());
        return;
    }
}

void DuskAudioProcessorEditor::mouseEnter(const juce::MouseEvent& event)
{
    const auto isWithin = [](juce::Component* component,
                             const juce::Component& owner)
    {
        for (auto* current = component; current != nullptr;
             current = current->getParentComponent())
            if (current == &owner)
                return true;
        return false;
    };
    for (const auto& dial : dials)
        if (isWithin(event.eventComponent, dial->slider)
            || isWithin(event.eventComponent, dial->label))
        {
            information.setText(
                contextHelpText(dial->label.getText(), dial->help),
                juce::dontSendNotification);
            return;
        }
    if (sequencerPanel != nullptr
        && isWithin(event.eventComponent, *sequencerPanel))
        return;
    juce::String title;
    juce::String tooltipText;
    for (auto* component = event.eventComponent;
         component != nullptr && component != &content;
         component = component->getParentComponent())
    {
        if (title.isEmpty())
            title = component->getProperties()[
                contextHelpTitleProperty].toString().trim();
        if (tooltipText.isEmpty())
            if (auto* tooltip = dynamic_cast<juce::TooltipClient*>(component))
                tooltipText = tooltip->getTooltip().trim();
    }
    if (title.isEmpty())
        for (auto* component = event.eventComponent;
             component != nullptr && component != &content;
             component = component->getParentComponent())
        {
            title = component->getName().trim();
            if (title.isNotEmpty())
                break;
        }
    if (title.isEmpty())
        for (auto* component = event.eventComponent;
             component != nullptr && component != &content;
             component = component->getParentComponent())
            if (auto* label = dynamic_cast<juce::Label*>(component))
            {
                title = label->getText().trim();
                if (title.isNotEmpty())
                    break;
            }
    if (tooltipText.isNotEmpty())
        information.setText(
            contextHelpText(title, tooltipText),
            juce::dontSendNotification);
}

DuskAudioProcessorEditor::Dial* DuskAudioProcessorEditor::addDial(
    const char* id, const char* caption, const char* description,
    bool attachToParameter)
{
    auto dial = std::make_unique<Dial>();
    dial->help = description;
    dial->parameterId = attachToParameter ? juce::String(id)
                                          : juce::String();
    dial->slider.setSliderStyle(
        juce::Slider::RotaryHorizontalVerticalDrag);
    dial->slider.setTextBoxStyle(
        juce::Slider::TextBoxBelow, false, 72, 18);
    dial->slider.setMouseDragSensitivity(80);
    dial->slider.setColour(
        juce::Slider::rotarySliderFillColourId, knobIndicatorColour);
    dial->slider.setColour(
        juce::Slider::textBoxTextColourId, knobValueColour);
    dial->slider.setColour(juce::Slider::textBoxBackgroundColourId,
                           juce::Colours::transparentBlack);
    dial->slider.setColour(juce::Slider::textBoxOutlineColourId,
                           juce::Colours::transparentBlack);
    dial->label.setText(caption, juce::dontSendNotification);
    auto* pointer = dial.get();
    dial->label.parameterIdProvider = [pointer]
    {
        return pointer->parameterId;
    };
    dial->label.setTooltip(
        "Drag " + juce::String(caption) + " onto a sequencer target slot");
    dial->label.setJustificationType(juce::Justification::centredTop);
    dial->label.setInterceptsMouseClicks(true, false);
    dial->label.setColour(juce::Label::textColourId, knobLabelColour);
    dial->label.setFont(juce::Font(juce::FontOptions(
        10.5f, juce::Font::bold)));
    content.addAndMakeVisible(dial->slider);
    content.addAndMakeVisible(dial->label);
    if (attachToParameter)
        dial->attachment = std::make_unique<SliderAttachment>(
            processor.parameters, id, dial->slider);
    if (attachToParameter)
        pointer->slider.onSavedStateReset = [this, pointer]
        {
            return restoreDialFromSavedState(*pointer);
        };
    pointer->slider.onValueChange = [this, pointer]
    {
        information.setText(
            contextHelpText(pointer->label.getText(), pointer->help),
            juce::dontSendNotification);
        int filterIndex = 0;
        if (! applyingFilterModel
            && isFilterModelParameter(pointer->parameterId, filterIndex))
            markFilterModelCustom(filterIndex);
    };
    dials.push_back(std::move(dial));
    return pointer;
}

bool DuskAudioProcessorEditor::restoreDialFromSavedState(Dial& dial)
{
    if (dial.parameterId.isEmpty())
        return false;
    const auto saved = processor.getSavedParameterValue(dial.parameterId);
    if (! saved.has_value())
        return false;
    dial.slider.setValue(*saved, juce::sendNotificationSync);
    information.setText(
        "RESTORED: " + dial.label.getText() + " = "
            + dial.slider.getTextFromValue(dial.slider.getValue())
            + "\nReturned to the value stored in the current saved state.",
        juce::dontSendNotification);
    return true;
}

juce::Colour DuskAudioProcessorEditor::filterAccent(int filterIndex) const
{
    return filterColourForId(filterIndex, 0);
}

juce::Colour DuskAudioProcessorEditor::panelColour(int filterIndex) const
{
    const float amount = themeDial != nullptr
        ? juce::jlimit(0.0f, 1.0f,
            static_cast<float>(themeDial->slider.getValue() * 0.01))
        : 1.0f;
    return juce::Colour(0xff24292f).interpolatedWith(
        filterAccent(filterIndex), amount);
}

void DuskAudioProcessorEditor::updateContextSensitiveHelp()
{
    repaint();
}

int DuskAudioProcessorEditor::architectureChoiceForFilter(
    int filterIndex) const
{
    const auto* value = processor.parameters.getRawParameterValue(
        filterIndex == 2 ? "filter2Architecture" : "filter1Architecture");
    return dusk::architecture::normaliseChoice(value != nullptr
        ? juce::roundToInt(value->load()) : 0);
}

void DuskAudioProcessorEditor::updateArchitectureContext()
{
    const std::array<int, 2> choices {
        architectureChoiceForFilter(1), architectureChoiceForFilter(2)
    };
    const std::array<juce::Label*, 2> designerTitles {
        &designer1Placeholder, &designer2Placeholder
    };
    for (int index = 0; index < 2; ++index)
    {
        const auto& descriptor = dusk::architecture::descriptorForChoice(
            choices[static_cast<size_t>(index)]);
        designerTitles[static_cast<size_t>(index)]->setText(
            juce::String("FILTER ") + juce::String(index + 1) + " "
                + juce::String(descriptor.displayName).toUpperCase(),
            juce::dontSendNotification);
    }
    displayedArchitectureChoices = choices;
}

bool DuskAudioProcessorEditor::isFilterModelParameter(
    const juce::String& parameterId, int& filterIndex) const
{
    filterIndex = parameterId.startsWith("filter1") ? 1
        : (parameterId.startsWith("filter2") ? 2 : 0);
    if (filterIndex == 0)
        return false;
    const auto suffix = parameterId.substring(7);
    const int architecture = architectureChoiceForFilter(filterIndex);
    if (architecture == static_cast<int>(dusk::architecture::Id::ladder))
    {
        for (const auto* candidate : ladderModelParameterSuffixes)
            if (suffix == candidate)
                return true;
    }
    else if (architecture == static_cast<int>(dusk::architecture::Id::ota))
    {
        for (const auto* candidate : otaModelParameterSuffixes)
            if (suffix == candidate)
                return true;
    }
    else
    {
        for (const auto* candidate : cleanModelParameterSuffixes)
            if (suffix == candidate)
                return true;
    }
    return false;
}

void DuskAudioProcessorEditor::markFilterModelCustom(int filterIndex)
{
    if (! filterModelBoxesReady || applyingFilterModel)
        return;
    const auto index = static_cast<size_t>(juce::jlimit(1, 2, filterIndex) - 1);
    selectedFactoryFilterModels[index].clear();
    selectedUserFilterModelFiles[index] = {};
    auto& box = filterIndex == 2 ? filter2ModelBox : filter1ModelBox;
    box.setSelectedId(0, juce::dontSendNotification);
    box.setTextWhenNothingSelected("CUSTOM");
}

void DuskAudioProcessorEditor::refreshFilterModelBoxes()
{
    userFilterModelFiles.clear();
    const auto directory = DuskAudioProcessor::getModelsDirectory();
    directory.createDirectory();
    directory.findChildFiles(userFilterModelFiles, juce::File::findFiles,
                             false, "*" + juce::String(modelFileExtension));
    std::sort(userFilterModelFiles.begin(), userFilterModelFiles.end(),
        [](const juce::File& left, const juce::File& right)
        {
            return left.getFileNameWithoutExtension().compareNatural(
                       right.getFileNameWithoutExtension(), true) < 0;
        });

    filterModelBoxesReady = false;
    for (int filterIndex = 1; filterIndex <= 2; ++filterIndex)
    {
        auto& box = filterIndex == 2 ? filter2ModelBox : filter1ModelBox;
        box.clear(juce::dontSendNotification);
        box.addSectionHeading("FACTORY / CLEAN SVF");
        for (size_t index = 0; index < cleanFactoryModels.size(); ++index)
            box.addItem(cleanFactoryModels[index].name,
                        cleanFactoryModelIdBase
                            + static_cast<int>(index) + 1);
        box.addSectionHeading("FACTORY / LADDER");
        for (size_t index = 0; index < ladderFactoryModels.size(); ++index)
            box.addItem(ladderFactoryModels[index].name,
                        ladderFactoryModelIdBase
                            + static_cast<int>(index) + 1);
        box.addSectionHeading("FACTORY / OTA");
        for (size_t index = 0; index < otaFactoryModels.size(); ++index)
            box.addItem(otaFactoryModels[index].name,
                        otaFactoryModelIdBase
                            + static_cast<int>(index) + 1);
        if (! userFilterModelFiles.isEmpty())
        {
            box.addSectionHeading("USER");
            for (int index = 0; index < userFilterModelFiles.size(); ++index)
                box.addItem(
                    userFilterModelFiles[index].getFileNameWithoutExtension(),
                    userFilterModelIdBase + index + 1);
        }
        box.setTextWhenNothingSelected("CUSTOM");

        const auto stateIndex = static_cast<size_t>(filterIndex - 1);
        int selectedId = 0;
        const auto selectedUser = selectedUserFilterModelFiles[stateIndex];
        if (selectedUser != juce::File())
            for (int index = 0; index < userFilterModelFiles.size(); ++index)
                if (userFilterModelFiles[index] == selectedUser)
                {
                    selectedId = userFilterModelIdBase + index + 1;
                    break;
                }
        if (selectedId == 0
            && selectedFactoryFilterModels[stateIndex].isNotEmpty())
        {
            for (size_t index = 0; index < cleanFactoryModels.size(); ++index)
                if (selectedFactoryFilterModels[stateIndex]
                        == cleanFactoryModels[index].name)
                    selectedId = cleanFactoryModelIdBase
                        + static_cast<int>(index) + 1;
            for (size_t index = 0; index < ladderFactoryModels.size(); ++index)
                if (selectedFactoryFilterModels[stateIndex]
                        == ladderFactoryModels[index].name)
                    selectedId = ladderFactoryModelIdBase
                        + static_cast<int>(index) + 1;
            for (size_t index = 0; index < otaFactoryModels.size(); ++index)
                if (selectedFactoryFilterModels[stateIndex]
                        == otaFactoryModels[index].name)
                    selectedId = otaFactoryModelIdBase
                        + static_cast<int>(index) + 1;
        }
        box.setSelectedId(selectedId, juce::dontSendNotification);
    }
    filterModelBoxesReady = true;
}

void DuskAudioProcessorEditor::applyFactoryFilterModel(
    int filterIndex, int selectedId)
{
    const auto parameterId = [filterIndex](const char* suffix)
    {
        return juce::String("filter") + juce::String(filterIndex) + suffix;
    };
    const auto stateIndex = static_cast<size_t>(filterIndex - 1);
    juce::String modelName;
    const juce::ScopedValueSetter<bool> guard(applyingFilterModel, true);

    const int cleanIndex = selectedId - cleanFactoryModelIdBase - 1;
    const int ladderIndex = selectedId - ladderFactoryModelIdBase - 1;
    const int otaIndex = selectedId - otaFactoryModelIdBase - 1;
    if (juce::isPositiveAndBelow(
            cleanIndex, static_cast<int>(cleanFactoryModels.size())))
    {
        const auto& model = cleanFactoryModels[static_cast<size_t>(cleanIndex)];
        modelName = model.name;
        setParameterActualValue(processor.parameters.getParameter(
            parameterId("Architecture")),
            static_cast<float>(dusk::architecture::Id::cleanSvf));
        for (size_t index = 0; index < cleanModelParameterSuffixes.size();
             ++index)
            setParameterActualValue(processor.parameters.getParameter(
                parameterId(cleanModelParameterSuffixes[index])),
                model.values[index]);
    }
    else if (juce::isPositiveAndBelow(
                 ladderIndex,
                 static_cast<int>(ladderFactoryModels.size())))
    {
        const auto& model = ladderFactoryModels[
            static_cast<size_t>(ladderIndex)];
        modelName = model.name;
        setParameterActualValue(processor.parameters.getParameter(
            parameterId("Architecture")),
            static_cast<float>(dusk::architecture::Id::ladder));
        for (size_t index = 0; index < ladderModelParameterSuffixes.size();
             ++index)
            setParameterActualValue(processor.parameters.getParameter(
                parameterId(ladderModelParameterSuffixes[index])),
                model.values[index]);
    }
    else if (juce::isPositiveAndBelow(
                 otaIndex, static_cast<int>(otaFactoryModels.size())))
    {
        const auto& model = otaFactoryModels[static_cast<size_t>(otaIndex)];
        modelName = model.name;
        setParameterActualValue(processor.parameters.getParameter(
            parameterId("Architecture")),
            static_cast<float>(dusk::architecture::Id::ota));
        for (size_t index = 0; index < otaModelParameterSuffixes.size();
             ++index)
            setParameterActualValue(processor.parameters.getParameter(
                parameterId(otaModelParameterSuffixes[index])),
                model.values[index]);
    }
    if (modelName.isEmpty())
        return;

    selectedFactoryFilterModels[stateIndex] = modelName;
    selectedUserFilterModelFiles[stateIndex] = {};
    updateArchitectureContext();
    updatePresetBrowserVisibility();
    information.setText(contextHelpText(
        "Filter " + juce::String(filterIndex) + " Model: " + modelName,
        "Factory Architecture and Designer values were recalled and remain fully editable. Standard FILTER controls were not changed."),
        juce::dontSendNotification);
}

juce::Result DuskAudioProcessorEditor::loadUserFilterModel(
    int filterIndex, const juce::File& modelFile)
{
    if (! modelFile.existsAsFile())
        return juce::Result::fail("The User Model file no longer exists.");
    const auto xml = juce::XmlDocument::parse(modelFile);
    if (xml == nullptr || ! xml->hasTagName("DuskFilterModel")
        || xml->getIntAttribute("version", 0) != 1)
        return juce::Result::fail("This is not a current Dusk User Model.");
    const int architecture = xml->getIntAttribute("architecture", -1);
    if (architecture < 0
        || architecture >= static_cast<int>(
            dusk::architecture::descriptors.size()))
        return juce::Result::fail("The User Model Architecture is invalid.");

    std::vector<std::pair<juce::String, float>> settings;
    const auto appendRequiredSettings = [&settings, &xml](
        const auto& expected)
    {
        for (const auto* requiredSuffix : expected)
        {
            const juce::XmlElement* match = nullptr;
            for (auto* parameterXml
                 : xml->getChildWithTagNameIterator("Parameter"))
                if (parameterXml->getStringAttribute("id").trim()
                    == requiredSuffix)
                {
                    match = parameterXml;
                    break;
                }
            if (match == nullptr || ! match->hasAttribute("value"))
                return false;
            const double value = match->getDoubleAttribute("value");
            if (! std::isfinite(value))
                return false;
            settings.emplace_back(requiredSuffix,
                                  static_cast<float>(value));
        }
        return true;
    };
    bool complete = false;
    if (architecture == static_cast<int>(dusk::architecture::Id::ladder))
        complete = appendRequiredSettings(ladderModelParameterSuffixes);
    else if (architecture == static_cast<int>(dusk::architecture::Id::ota))
        complete = appendRequiredSettings(otaModelParameterSuffixes);
    else
        complete = appendRequiredSettings(cleanModelParameterSuffixes);
    if (! complete)
        return juce::Result::fail(
            "The User Model is missing required Designer values or contains an invalid value.");

    const auto parameterId = [filterIndex](const juce::String& suffix)
    {
        return juce::String("filter") + juce::String(filterIndex) + suffix;
    };
    const juce::ScopedValueSetter<bool> guard(applyingFilterModel, true);
    setParameterActualValue(processor.parameters.getParameter(
        parameterId("Architecture")), static_cast<float>(architecture));
    for (const auto& [suffix, value] : settings)
        setParameterActualValue(
            processor.parameters.getParameter(parameterId(suffix)), value);
    updateArchitectureContext();
    updatePresetBrowserVisibility();
    return juce::Result::ok();
}

juce::Result DuskAudioProcessorEditor::saveFilterModelToFile(
    int filterIndex, const juce::File& modelFile)
{
    const auto root = DuskAudioProcessor::getModelsDirectory();
    if (modelFile.getParentDirectory() != root)
        return juce::Result::fail(
            "User Models must be saved directly inside the Dusk Models folder.");
    if (! root.isDirectory() && root.createDirectory().failed())
        return juce::Result::fail("The Models folder could not be created.");

    const int architecture = architectureChoiceForFilter(filterIndex);
    auto xml = std::make_unique<juce::XmlElement>("DuskFilterModel");
    xml->setAttribute("version", 1);
    xml->setAttribute("architecture", architecture);
    const auto parameterId = [filterIndex](const char* suffix)
    {
        return juce::String("filter") + juce::String(filterIndex) + suffix;
    };
    const auto addSetting = [this, &xml, &parameterId](const char* suffix)
    {
        auto* parameter = processor.parameters.getParameter(
            parameterId(suffix));
        if (parameter == nullptr)
            return;
        auto* element = xml->createNewChildElement("Parameter");
        element->setAttribute("id", suffix);
        element->setAttribute("value",
            static_cast<double>(parameter->convertFrom0to1(
                parameter->getValue())));
    };
    if (architecture == static_cast<int>(dusk::architecture::Id::ladder))
        for (const auto* suffix : ladderModelParameterSuffixes)
            addSetting(suffix);
    else if (architecture == static_cast<int>(dusk::architecture::Id::ota))
        for (const auto* suffix : otaModelParameterSuffixes)
            addSetting(suffix);
    else
        for (const auto* suffix : cleanModelParameterSuffixes)
            addSetting(suffix);

    const juce::TemporaryFile temporaryFile(modelFile);
    if (! temporaryFile.getFile().replaceWithText(xml->toString())
        || ! temporaryFile.overwriteTargetFileWithTemporary())
        return juce::Result::fail("The User Model could not be written safely.");
    return juce::Result::ok();
}

void DuskAudioProcessorEditor::showSaveFilterModelAs(int filterIndex)
{
    const auto root = DuskAudioProcessor::getModelsDirectory();
    root.createDirectory();
    filterModelFileChooser = std::make_unique<juce::FileChooser>(
        "Save Filter " + juce::String(filterIndex) + " User Model",
        root.getChildFile("Custom Model" + juce::String(modelFileExtension)),
        "*" + juce::String(modelFileExtension));
    const juce::Component::SafePointer<DuskAudioProcessorEditor> safeThis(this);
    filterModelFileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [safeThis, filterIndex](const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;
            auto file = chooser.getResult();
            if (file != juce::File())
            {
                if (! file.hasFileExtension(modelFileExtension))
                    file = file.withFileExtension(modelFileExtension);
                const auto result = safeThis->saveFilterModelToFile(
                    filterIndex, file);
                if (result.wasOk())
                {
                    const auto index = static_cast<size_t>(filterIndex - 1);
                    const auto otherIndex = static_cast<size_t>(2 - filterIndex);
                    if (safeThis->selectedUserFilterModelFiles[otherIndex]
                        == file)
                    {
                        safeThis->selectedUserFilterModelFiles[otherIndex] = {};
                        safeThis->selectedFactoryFilterModels[otherIndex]
                            .clear();
                    }
                    safeThis->selectedUserFilterModelFiles[index] = file;
                    safeThis->selectedFactoryFilterModels[index].clear();
                    safeThis->refreshFilterModelBoxes();
                    safeThis->information.setText(contextHelpText(
                        "Filter " + juce::String(filterIndex)
                            + " User Model Saved",
                        file.getFileNameWithoutExtension()
                            + " now recalls this Architecture and its Designer values for either filter."),
                        juce::dontSendNotification);
                }
                else
                    safeThis->information.setText(contextHelpText(
                        "User Model Save Failed", result.getErrorMessage()),
                        juce::dontSendNotification);
            }
            safeThis->filterModelFileChooser.reset();
        });
}

void DuskAudioProcessorEditor::showFilterModelMenu(int filterIndex)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Save Current as User Model...");
    menu.addItem(2, "Refresh User Models");
    menu.addItem(3, "Open Models Folder");
    auto* target = filterIndex == 2 ? &filter2ModelMenuButton
                                    : &filter1ModelMenuButton;
    const juce::Component::SafePointer<DuskAudioProcessorEditor> safeThis(this);
    menu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetComponent(target)
            .withMinimumWidth(210)
            .withStandardItemHeight(22),
        [safeThis, filterIndex](int result)
        {
            if (safeThis == nullptr || result == 0)
                return;
            if (result == 1)
                safeThis->showSaveFilterModelAs(filterIndex);
            else if (result == 2)
            {
                safeThis->refreshFilterModelBoxes();
                safeThis->information.setText(contextHelpText(
                    "User Models", "The shared User Model list was refreshed."),
                    juce::dontSendNotification);
            }
            else if (result == 3)
            {
                const auto directory = DuskAudioProcessor::getModelsDirectory();
                directory.createDirectory();
                if (! directory.startAsProcess())
                    safeThis->information.setText(contextHelpText(
                        "Models Folder", "The Models folder could not be opened."),
                        juce::dontSendNotification);
            }
        });
}

void DuskAudioProcessorEditor::handleFilterModelSelection(int filterIndex)
{
    if (! filterModelBoxesReady || applyingFilterModel)
        return;
    auto& box = filterIndex == 2 ? filter2ModelBox : filter1ModelBox;
    const int selectedId = box.getSelectedId();
    if ((selectedId > cleanFactoryModelIdBase
         && selectedId <= cleanFactoryModelIdBase
            + static_cast<int>(cleanFactoryModels.size()))
        || (selectedId > ladderFactoryModelIdBase
            && selectedId <= ladderFactoryModelIdBase
                + static_cast<int>(ladderFactoryModels.size()))
        || (selectedId > otaFactoryModelIdBase
            && selectedId <= otaFactoryModelIdBase
                + static_cast<int>(otaFactoryModels.size())))
    {
        applyFactoryFilterModel(filterIndex, selectedId);
        return;
    }

    const int userIndex = selectedId - userFilterModelIdBase - 1;
    if (! juce::isPositiveAndBelow(userIndex, userFilterModelFiles.size()))
        return;
    const auto file = userFilterModelFiles[userIndex];
    const auto result = loadUserFilterModel(filterIndex, file);
    if (result.failed())
    {
        markFilterModelCustom(filterIndex);
        information.setText(contextHelpText(
            "User Model Load Failed", result.getErrorMessage()),
            juce::dontSendNotification);
        return;
    }
    const auto stateIndex = static_cast<size_t>(filterIndex - 1);
    selectedUserFilterModelFiles[stateIndex] = file;
    selectedFactoryFilterModels[stateIndex].clear();
    information.setText(contextHelpText(
        "Filter " + juce::String(filterIndex) + " User Model: "
            + file.getFileNameWithoutExtension(),
        "Architecture and Designer values were recalled and remain editable; standard FILTER controls were not changed."),
        juce::dontSendNotification);
}

void DuskAudioProcessorEditor::setMainView(MainView view)
{
    mainView = view;
    filterViewButton.setToggleState(mainView == MainView::filter,
                                    juce::dontSendNotification);
    designerViewButton.setToggleState(mainView == MainView::designer,
                                      juce::dontSendNotification);
    updateArchitectureContext();
    updatePresetBrowserVisibility();
    if (mainView == MainView::designer)
        information.setText(
            "DESIGNER | Each side follows its filter's selected DSP architecture.\nClean SVF, Ladder and OTA expose only the controls that genuinely belong to their own engines.",
            juce::dontSendNotification);
    else
        information.setText(
            "FILTER VIEW | Select each filter's Architecture and standard LPF, BPF, HPF, NOTCH or PEAK response.\nThe graph shows F1, F2 and the routing-aware combined small-signal response.",
            juce::dontSendNotification);
    saveInterfaceSettings();
}

void DuskAudioProcessorEditor::setPresetBrowserOpen(bool shouldOpen)
{
    presetBrowserOpen = shouldOpen;
    presetBrowserButton.setToggleState(shouldOpen,
                                       juce::dontSendNotification);
    if (shouldOpen)
    {
        const auto root = DuskAudioProcessor::getPresetsDirectory();
        root.createDirectory();
        if (! selectedPresetFolder.isDirectory()
            || (selectedPresetFolder != root
                && ! selectedPresetFolder.isAChildOf(root)))
            selectedPresetFolder = root;
        rebuildPresetFolderTree();
        information.setText(
            "PRESET BROWSER | Choose a folder, then click a preset to load it.\nSAVE overwrites the current preset; SAVE AS creates a new preset or copy.",
            juce::dontSendNotification);
    }
    updatePresetBrowserVisibility();
}

void DuskAudioProcessorEditor::updatePresetBrowserVisibility()
{
    const bool showFilterControls = ! presetBrowserOpen
                                 && mainView == MainView::filter;
    const bool showDesignerControls = ! presetBrowserOpen
                                   && mainView == MainView::designer;
    const bool filter1UsesCleanSvf = architectureChoiceForFilter(1)
        == static_cast<int>(dusk::architecture::Id::cleanSvf);
    const bool filter2UsesCleanSvf = architectureChoiceForFilter(2)
        == static_cast<int>(dusk::architecture::Id::cleanSvf);
    const bool filter1UsesLadder = architectureChoiceForFilter(1)
        == static_cast<int>(dusk::architecture::Id::ladder);
    const bool filter2UsesLadder = architectureChoiceForFilter(2)
        == static_cast<int>(dusk::architecture::Id::ladder);
    const bool filter1UsesOta = architectureChoiceForFilter(1)
        == static_cast<int>(dusk::architecture::Id::ota);
    const bool filter2UsesOta = architectureChoiceForFilter(2)
        == static_cast<int>(dusk::architecture::Id::ota);
    for (const auto& dial : dials)
    {
        bool visible = false;
        if (dial.get() == browserMixDial)
            visible = presetBrowserOpen;
        else if (! presetBrowserOpen)
            visible = dial.get() == themeDial
                || dial.get() == mixDial
                || dial.get() == outputDial
                || (showFilterControls
                    && (dial.get() == filter1DriveDial
                        || dial.get() == filter2DriveDial
                        || dial.get() == filter1CutoffDial
                        || dial.get() == filter2CutoffDial
                        || dial.get() == filter1ResonanceDial
                        || dial.get() == filter2ResonanceDial
                        || dial.get() == filter1SlopeDial
                        || dial.get() == filter2SlopeDial))
                || (showDesignerControls
                    && ((filter1UsesCleanSvf
                            && (dial.get() == filter1ResonanceShapeDial
                                || dial.get() == filter1ResonanceLimitDial
                                || dial.get() == filter1BassLossDial
                                || dial.get() == filter1CoreSaturationDial
                                || dial.get() == filter1SaturationAsymmetryDial
                                || dial.get() == filter1PoleSpreadDial
                                || dial.get() == filter1FeedbackDial
                                || dial.get() == filter1FeedbackLimitDial))
                        || (filter2UsesCleanSvf
                            && (dial.get() == filter2ResonanceShapeDial
                                || dial.get() == filter2ResonanceLimitDial
                                || dial.get() == filter2BassLossDial
                                || dial.get() == filter2CoreSaturationDial
                                || dial.get() == filter2SaturationAsymmetryDial
                                || dial.get() == filter2PoleSpreadDial
                                || dial.get() == filter2FeedbackDial
                                || dial.get() == filter2FeedbackLimitDial))
                        || (filter1UsesLadder
                            && (dial.get()
                                    == filter1LadderStageSaturationDial
                                || dial.get()
                                    == filter1LadderFeedbackSaturationDial
                                || dial.get()
                                    == filter1LadderResonanceCompensationDial))
                        || (filter2UsesLadder
                            && (dial.get()
                                    == filter2LadderStageSaturationDial
                                || dial.get()
                                    == filter2LadderFeedbackSaturationDial
                                || dial.get()
                                    == filter2LadderResonanceCompensationDial))
                        || (filter1UsesOta
                            && (dial.get() == filter1OtaCellDriveDial
                                || dial.get()
                                    == filter1OtaFeedbackDampingDial
                                || dial.get()
                                    == filter1OtaResonanceCompensationDial))
                        || (filter2UsesOta
                            && (dial.get() == filter2OtaCellDriveDial
                                || dial.get()
                                    == filter2OtaFeedbackDampingDial
                                || dial.get()
                                    == filter2OtaResonanceCompensationDial))));
        dial->slider.setVisible(visible);
        dial->label.setVisible(visible);
    }

    for (auto* component : std::array<juce::Component*, 16>
         { &filter1ArchitectureBox, &filter2ArchitectureBox,
           &filter1ArchitectureLabel, &filter2ArchitectureLabel,
           &filter1TypeBox, &filter2TypeBox,
           &filter1TypeLabel, &filter2TypeLabel,
           &filter1DistortionTypeBox, &filter2DistortionTypeBox,
           &filter1DistortionPlacementBox,
           &filter2DistortionPlacementBox,
           &filter1DistortionTypeLabel, &filter2DistortionTypeLabel,
           &filter1DistortionPlacementLabel,
           &filter2DistortionPlacementLabel })
        component->setVisible(showFilterControls);
    filter1TitleLabel.setVisible(showFilterControls);
    filter2TitleLabel.setVisible(showFilterControls);
    responseDisplay.setVisible(! presetBrowserOpen);
    filter1PowerButton.setVisible(showFilterControls);
    filter2PowerButton.setVisible(showFilterControls);
    designer1Placeholder.setVisible(showDesignerControls);
    designer2Placeholder.setVisible(showDesignerControls);
    for (auto* component : std::array<juce::Component*, 6>
         { &filter1ModelBox, &filter2ModelBox,
           &filter1ModelLabel, &filter2ModelLabel,
           &filter1ModelMenuButton, &filter2ModelMenuButton })
        component->setVisible(showDesignerControls);
    routingBox.setVisible(true);
    routingLabel.setVisible(true);
    information.setVisible(! presetBrowserOpen);
    randomButton.setVisible(! presetBrowserOpen);
    mixLockButton.setVisible(! presetBrowserOpen);
    presetFolderLabel.setVisible(presetBrowserOpen);
    presetListLabel.setVisible(presetBrowserOpen);
    presetFolderTree.setVisible(presetBrowserOpen);
    presetList.setVisible(presetBrowserOpen);
    presetSaveButton.setVisible(presetBrowserOpen);
    presetSaveAsButton.setVisible(presetBrowserOpen);
    presetOpenFolderButton.setVisible(presetBrowserOpen);
    resized();
    repaint();
}

void DuskAudioProcessorEditor::setSequencerOpen(bool shouldOpen)
{
    if (sequencerOpen == shouldOpen)
        return;

    const float scale = juce::jlimit(
        0.75f, 2.0f,
        static_cast<float>(getWidth()) / static_cast<float>(designWidth));
    sequencerOpen = shouldOpen;
    sequencerButton.setToggleState(shouldOpen, juce::dontSendNotification);
    if (sequencerPanel != nullptr)
        sequencerPanel->setVisible(shouldOpen);
    if (auto* editorConstrainer = getConstrainer())
        editorConstrainer->setFixedAspectRatio(
            static_cast<double>(designWidth)
                / static_cast<double>(activeDesignHeight()));
    setResizeLimits(695, shouldOpen ? 414 : 207,
                    1852, shouldOpen ? 1104 : 552);
    setSize(juce::roundToInt(static_cast<float>(designWidth) * scale),
            juce::roundToInt(static_cast<float>(activeDesignHeight()) * scale));
    saveInterfaceSettings();
    resized();
    repaint();
}

void DuskAudioProcessorEditor::updateSequencerTargetHighlights()
{
    if (sequencerPanel == nullptr)
        return;

    const auto accent = sequencerPanel->getAccent();
    for (const auto& dial : dials)
    {
        const auto parameterId = dial->label.parameterIdProvider != nullptr
            ? dial->label.parameterIdProvider() : juce::String();
        dial->label.setTargetAppearance(
            sequencerPanel->isTargetAssigned(parameterId), accent);
    }
    sequencerPanel->updateTargetHighlights();
}

void DuskAudioProcessorEditor::updateAnimatedParameterDisplays()
{
    const bool animate = sequencerPanel != nullptr
                      && sequencerPanel->getAnimate();
    for (const auto& dial : dials)
    {
        const auto parameterId = dial->label.parameterIdProvider != nullptr
            ? dial->label.parameterIdProvider() : juce::String();
        const auto value = animate && parameterId.isNotEmpty()
            ? processor.getSequencerAnimatedNormalised(parameterId)
            : std::nullopt;
        auto& sliderProperties = dial->slider.getProperties();
        if (value.has_value())
        {
            const bool changed = ! sliderProperties.contains("animatedNormalised")
                || std::abs(static_cast<float>(
                       sliderProperties["animatedNormalised"]) - *value)
                       > 0.000001f;
            if (changed)
            {
                sliderProperties.set("animatedNormalised", *value);
                dial->slider.repaint();
            }
        }
        else if (sliderProperties.contains("animatedNormalised"))
        {
            sliderProperties.remove("animatedNormalised");
            dial->slider.repaint();
        }
    }
}

void DuskAudioProcessorEditor::randomiseEffectParameters()
{
    juce::Random random;
    const auto randomCutoff = [&random]
    {
        return 20.0f * std::pow(1000.0f, random.nextFloat());
    };
    setParameterActualValue(
        processor.parameters.getParameter("filter1Cutoff"), randomCutoff());
    setParameterActualValue(
        processor.parameters.getParameter("filter1Type"),
        static_cast<float>(random.nextInt(5)));
    setParameterActualValue(
        processor.parameters.getParameter("filter1Resonance"),
        random.nextFloat() * 65.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter1ResonanceShape"),
        -75.0f + random.nextFloat() * 150.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter1ResonanceLimit"),
        50.0f + random.nextFloat() * 50.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter1BassLoss"),
        random.nextFloat() * 70.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter1CoreSaturation"),
        random.nextFloat() * 70.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter1SaturationAsymmetry"),
        -75.0f + random.nextFloat() * 150.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter1PoleSpread"),
        random.nextFloat() * 75.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter1Feedback"),
        random.nextFloat() * 65.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter1FeedbackLimit"),
        35.0f + random.nextFloat() * 65.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter1LadderStageSaturation"),
        random.nextFloat() * 80.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter1LadderFeedbackSaturation"),
        random.nextFloat() * 80.0f);
    setParameterActualValue(
        processor.parameters.getParameter(
            "filter1LadderResonanceCompensation"),
        random.nextFloat() * 100.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter1OtaCellDrive"),
        random.nextFloat() * 85.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter1OtaFeedbackDamping"),
        random.nextFloat() * 100.0f);
    setParameterActualValue(
        processor.parameters.getParameter(
            "filter1OtaResonanceCompensation"),
        random.nextFloat() * 100.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter1Slope"),
        6.0f + random.nextFloat() * 42.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter1DistortionType"),
        static_cast<float>(random.nextInt(17)));
    setParameterActualValue(
        processor.parameters.getParameter("filter1Drive"),
        10.0f + random.nextFloat() * 70.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter2DistortionType"),
        static_cast<float>(random.nextInt(17)));
    setParameterActualValue(
        processor.parameters.getParameter("filter2Type"),
        static_cast<float>(random.nextInt(5)));
    setParameterActualValue(
        processor.parameters.getParameter("filter2Drive"),
        10.0f + random.nextFloat() * 70.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter2Cutoff"), randomCutoff());
    setParameterActualValue(
        processor.parameters.getParameter("filter2Resonance"),
        random.nextFloat() * 65.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter2ResonanceShape"),
        -75.0f + random.nextFloat() * 150.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter2ResonanceLimit"),
        50.0f + random.nextFloat() * 50.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter2BassLoss"),
        random.nextFloat() * 70.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter2CoreSaturation"),
        random.nextFloat() * 70.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter2SaturationAsymmetry"),
        -75.0f + random.nextFloat() * 150.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter2PoleSpread"),
        random.nextFloat() * 75.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter2Feedback"),
        random.nextFloat() * 65.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter2FeedbackLimit"),
        35.0f + random.nextFloat() * 65.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter2LadderStageSaturation"),
        random.nextFloat() * 80.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter2LadderFeedbackSaturation"),
        random.nextFloat() * 80.0f);
    setParameterActualValue(
        processor.parameters.getParameter(
            "filter2LadderResonanceCompensation"),
        random.nextFloat() * 100.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter2OtaCellDrive"),
        random.nextFloat() * 85.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter2OtaFeedbackDamping"),
        random.nextFloat() * 100.0f);
    setParameterActualValue(
        processor.parameters.getParameter(
            "filter2OtaResonanceCompensation"),
        random.nextFloat() * 100.0f);
    setParameterActualValue(
        processor.parameters.getParameter("filter2Slope"),
        6.0f + random.nextFloat() * 42.0f);
    setParameterActualValue(
        processor.parameters.getParameter("output"),
        -6.0f + random.nextFloat() * 6.0f);
    information.setText(
        "RANDOMISE | Both standard controls and all Clean SVF, Ladder and OTA Designer parameter sets plus a safe OUT level were randomised.\nArchitecture, MIX, filter power, main Power, routing, placement and the complete sequencer were left unchanged.",
        juce::dontSendNotification);
}

void DuskAudioProcessorEditor::loadFavouritePresets()
{
    favouritePresetPaths.clear();
    const auto file = DuskAudioProcessor::getSettingsDirectory()
        .getChildFile("Favourites.ini");
    if (! file.existsAsFile())
        return;

    juce::StringArray lines;
    lines.addLines(file.loadFileAsString());
    bool inFavourites = false;
    for (auto line : lines)
    {
        line = line.trim();
        if (line.startsWithChar('['))
        {
            inFavourites = line.equalsIgnoreCase("[Favourites]");
            continue;
        }
        if (! inFavourites || ! line.startsWithIgnoreCase("Preset="))
            continue;

        auto relativePath = line.substring(7).trim()
            .replaceCharacter('\\', '/');
        if (relativePath.isNotEmpty()
            && ! favouritePresetPaths.contains(relativePath, true))
            favouritePresetPaths.add(relativePath);
    }
}

void DuskAudioProcessorEditor::saveFavouritePresets() const
{
    const auto settingsDirectory =
        DuskAudioProcessor::getSettingsDirectory();
    settingsDirectory.createDirectory();
    juce::String contents("[Favourites]\r\n");
    for (const auto& relativePath : favouritePresetPaths)
        contents += "Preset=" + relativePath + "\r\n";
    settingsDirectory.getChildFile("Favourites.ini")
        .replaceWithText(contents, false, false, "\r\n");
}

bool DuskAudioProcessorEditor::isPresetFavourite(
    const juce::File& file) const
{
    const auto root = DuskAudioProcessor::getPresetsDirectory();
    if (! file.existsAsFile()
        || (file.getParentDirectory() != root
            && ! file.isAChildOf(root)))
        return false;

    const auto relativePath = file.getRelativePathFrom(root)
        .replaceCharacter('\\', '/');
    return favouritePresetPaths.contains(relativePath, true);
}

juce::Array<juce::File>
DuskAudioProcessorEditor::favouritePresetFiles() const
{
    const auto root = DuskAudioProcessor::getPresetsDirectory();
    juce::Array<juce::File> files;
    for (const auto& relativePath : favouritePresetPaths)
    {
        const auto file = root.getChildFile(relativePath);
        if (file.existsAsFile() && file.hasFileExtension(presetFileExtension)
            && (file.getParentDirectory() == root || file.isAChildOf(root)))
            files.addIfNotAlreadyThere(file);
    }
    std::sort(files.begin(), files.end(),
              [](const juce::File& first, const juce::File& second)
              {
                  const int nameOrder = first.getFileNameWithoutExtension()
                      .compareNatural(second.getFileNameWithoutExtension(),
                                      true);
                  return nameOrder != 0
                      ? nameOrder < 0
                      : first.getFullPathName().compareNatural(
                            second.getFullPathName(), true) < 0;
              });
    return files;
}

juce::Array<juce::File>
DuskAudioProcessorEditor::navigationPresetFiles() const
{
    if (favouritesViewSelected)
        return favouritePresetFiles();

    const auto root = DuskAudioProcessor::getPresetsDirectory();
    auto folder = currentPresetFile.existsAsFile()
        ? currentPresetFile.getParentDirectory()
        : selectedPresetFolder;
    if (! folder.isDirectory()
        || (folder != root && ! folder.isAChildOf(root)))
        folder = root;
    return presetFilesInFolder(folder);
}

void DuskAudioProcessorEditor::togglePresetFavourite(
    const juce::File& file)
{
    const auto root = DuskAudioProcessor::getPresetsDirectory();
    if (! file.existsAsFile()
        || (file.getParentDirectory() != root && ! file.isAChildOf(root)))
        return;

    const auto relativePath = file.getRelativePathFrom(root)
        .replaceCharacter('\\', '/');
    const int existing = favouritePresetPaths.indexOf(relativePath, true);
    if (existing >= 0)
        favouritePresetPaths.remove(existing);
    else
        favouritePresetPaths.add(relativePath);
    saveFavouritePresets();
    refreshPresetList();
}

void DuskAudioProcessorEditor::copyPreset(const juce::File& file)
{
    if (! file.existsAsFile() || ! file.hasFileExtension(presetFileExtension))
        return;
    copiedPresetFile = file;
    information.setText(
        "PRESET COPIED: " + file.getFileNameWithoutExtension()
            + "\nRight-click a real preset folder or preset list and choose Paste preset here.",
        juce::dontSendNotification);
}

void DuskAudioProcessorEditor::pastePresetIntoFolder(
    const juce::File& folder)
{
    const auto root = DuskAudioProcessor::getPresetsDirectory();
    if (! copiedPresetFile.existsAsFile() || ! folder.isDirectory()
        || (folder != root && ! folder.isAChildOf(root)))
        return;

    const auto baseName = copiedPresetFile.getFileNameWithoutExtension();
    auto destination = folder.getChildFile(
        baseName + presetFileExtension);
    if (destination.exists())
    {
        destination = folder.getChildFile(
            baseName + " Copy" + presetFileExtension);
        int copyNumber = 2;
        while (destination.exists())
            destination = folder.getChildFile(
                baseName + " Copy " + juce::String(copyNumber++)
                    + presetFileExtension);
    }

    if (! copiedPresetFile.copyFileTo(destination))
    {
        information.setText(
            "PRESET PASTE FAILED | The preset file could not be copied.",
            juce::dontSendNotification);
        return;
    }

    selectedPresetFolder = folder;
    favouritesViewSelected = false;
    rebuildPresetFolderTree();
    information.setText(
        "PRESET PASTED: " + destination.getFileNameWithoutExtension()
            + "\nThe copy is independent of the original and can be edited or saved normally.",
        juce::dontSendNotification);
}

void DuskAudioProcessorEditor::showPresetContextMenu(
    const juce::File& file, juce::Point<int> position)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel(&lookAndFeel);
    if (file.existsAsFile())
    {
        menu.addItem(1, "Copy preset");
        menu.addSeparator();
    }
    menu.addItem(2, "Paste preset here",
                 copiedPresetFile.existsAsFile()
                    && ! favouritesViewSelected
                    && selectedPresetFolder.isDirectory());
    lookAndFeel.setPopupBorderColour(filterAccent(1));
    const auto safeThis = juce::Component::SafePointer<
        DuskAudioProcessorEditor>(this);
    const auto destinationFolder = selectedPresetFolder;
    menu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetScreenArea({ position.x, position.y, 1, 1 })
            .withMinimumWidth(150),
        [safeThis, file, destinationFolder](int result)
        {
            if (auto* editor = safeThis.getComponent())
            {
                if (result == 1)
                    editor->copyPreset(file);
                else if (result == 2)
                    editor->pastePresetIntoFolder(destinationFolder);
            }
        });
}

void DuskAudioProcessorEditor::showCreatePresetFolderDialog(
    const juce::File& parentFolder)
{
    const auto root = DuskAudioProcessor::getPresetsDirectory();
    if (! parentFolder.isDirectory()
        || (parentFolder != root && ! parentFolder.isAChildOf(root)))
        return;

    presetFolderNameWindow = std::make_unique<juce::AlertWindow>(
        "New Preset Folder",
        "Enter a name for the new folder inside "
            + (parentFolder == root ? juce::String("Presets")
                                    : parentFolder.getFileName()) + ".",
        juce::AlertWindow::NoIcon);
    presetFolderNameWindow->setLookAndFeel(&lookAndFeel);
    presetFolderNameWindow->addTextEditor(
        "folderName", {}, "Folder name:");
    presetFolderNameWindow->addButton(
        "Create", 1,
        juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
    presetFolderNameWindow->addButton(
        "Cancel", 0,
        juce::KeyPress(juce::KeyPress::escapeKey, 0, 0));

    const auto safeThis = juce::Component::SafePointer<
        DuskAudioProcessorEditor>(this);
    presetFolderNameWindow->enterModalState(
        true,
        juce::ModalCallbackFunction::create(
            [safeThis, parentFolder](int result)
            {
                auto* editor = safeThis.getComponent();
                if (editor == nullptr || editor->presetFolderNameWindow == nullptr)
                    return;

                const auto name = editor->presetFolderNameWindow
                    ->getTextEditorContents("folderName").trim();
                editor->presetFolderNameWindow->setLookAndFeel(nullptr);
                editor->presetFolderNameWindow.reset();
                if (result != 1)
                    return;

                if (name.isEmpty() || name == "." || name == ".."
                    || name.containsAnyOf("\\/:*?\"<>|")
                    || name.endsWithChar('.') || name.endsWithChar(' '))
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Invalid Folder Name",
                        "Please use a normal folder name without \\ / : * ? \" < > | or a trailing dot.",
                        "OK", editor);
                    return;
                }

                const auto folder = parentFolder.getChildFile(name);
                if (folder.exists())
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Folder Already Exists",
                        "A file or folder with that name already exists.",
                        "OK", editor);
                    return;
                }

                const auto createResult = folder.createDirectory();
                if (createResult.failed())
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Folder Creation Failed",
                        createResult.getErrorMessage(), "OK", editor);
                    return;
                }

                editor->selectedPresetFolder = folder;
                editor->favouritesViewSelected = false;
                editor->rebuildPresetFolderTree();
            }),
        false);
}

void DuskAudioProcessorEditor::confirmDeletePresetFolder(
    const juce::File& folder)
{
    const auto root = DuskAudioProcessor::getPresetsDirectory();
    if (! folder.isDirectory() || folder == root
        || ! folder.isAChildOf(root))
        return;

    const bool hasContents = folder.getNumberOfChildFiles(
        juce::File::findFilesAndDirectories) > 0;
    const auto message = hasContents
        ? "Delete the folder '" + folder.getFileName()
            + "' and everything inside it?\n\nThis moves the folder, presets and subfolders to the system Recycle Bin or Trash."
        : "Delete the empty folder '" + folder.getFileName()
            + "'?\n\nThis moves it to the system Recycle Bin or Trash.";
    const auto safeThis = juce::Component::SafePointer<
        DuskAudioProcessorEditor>(this);
    juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::WarningIcon,
        "Delete Preset Folder", message,
        "Delete", "Cancel", this,
        juce::ModalCallbackFunction::create(
            [safeThis, folder](int result)
            {
                auto* editor = safeThis.getComponent();
                if (editor == nullptr || result != 1)
                    return;

                const bool containedCurrentPreset =
                    editor->currentPresetFile.existsAsFile()
                    && editor->currentPresetFile.isAChildOf(folder);
                const bool containedCopiedPreset =
                    editor->copiedPresetFile.existsAsFile()
                    && editor->copiedPresetFile.isAChildOf(folder);
                if (! folder.moveToTrash())
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Folder Deletion Failed",
                        "The preset folder could not be moved to the system Recycle Bin or Trash.",
                        "OK", editor);
                    return;
                }

                const auto rootDirectory =
                    DuskAudioProcessor::getPresetsDirectory();
                bool favouritesChanged = false;
                for (int index = editor->favouritePresetPaths.size();
                     --index >= 0;)
                {
                    const auto favouriteFile = rootDirectory.getChildFile(
                        editor->favouritePresetPaths[index]);
                    if (favouriteFile.isAChildOf(folder))
                    {
                        editor->favouritePresetPaths.remove(index);
                        favouritesChanged = true;
                    }
                }
                if (favouritesChanged)
                    editor->saveFavouritePresets();
                if (containedCopiedPreset)
                    editor->copiedPresetFile = juce::File();
                if (containedCurrentPreset)
                {
                    editor->currentPresetFile = juce::File();
                    editor->processor.parameters.state.removeProperty(
                        "currentPresetPath", nullptr);
                }

                editor->selectedPresetFolder = rootDirectory;
                editor->favouritesViewSelected = false;
                editor->rebuildPresetFolderTree();
                editor->updatePresetNavigationDisplay();
            }));
}

void DuskAudioProcessorEditor::showPresetFolderContextMenu(
    const juce::File& folder, bool favourites, juce::Point<int> position)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel(&lookAndFeel);
    menu.addItem(1, "New folder...");
    menu.addItem(2, "Delete folder...",
                 ! favourites
                    && folder != DuskAudioProcessor::getPresetsDirectory()
                    && folder.isDirectory());
    menu.addSeparator();
    menu.addItem(3, "Paste preset here",
                 ! favourites && copiedPresetFile.existsAsFile()
                    && folder.isDirectory());
    lookAndFeel.setPopupBorderColour(filterAccent(1));
    const auto safeThis = juce::Component::SafePointer<
        DuskAudioProcessorEditor>(this);
    menu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetScreenArea({ position.x, position.y, 1, 1 })
            .withMinimumWidth(150),
        [safeThis, folder, favourites](int result)
        {
            if (auto* editor = safeThis.getComponent())
            {
                if (result == 1)
                    editor->showCreatePresetFolderDialog(
                        favourites
                            ? DuskAudioProcessor::getPresetsDirectory()
                            : folder);
                else if (result == 2)
                    editor->confirmDeletePresetFolder(folder);
                else if (result == 3)
                    editor->pastePresetIntoFolder(folder);
            }
        });
}

void DuskAudioProcessorEditor::rebuildPresetFolderTree()
{
    const auto root = DuskAudioProcessor::getPresetsDirectory();
    root.createDirectory();
    presetFolderTree.setRootItem(nullptr);
    presetFolderRoot.reset();
    presetFolderRoot = std::make_unique<FolderTreeItem>(
        root,
        [this](const juce::File& folder, bool favourites)
        {
            if (favourites)
                selectFavouritePresets();
            else
                selectPresetFolder(folder);
        },
        [this](const juce::File& folder, bool favourites,
               juce::Point<int> position)
        {
            showPresetFolderContextMenu(folder, favourites, position);
        });
    presetFolderTree.setRootItem(presetFolderRoot.get());
    presetFolderRoot->setOpen(true);

    auto target = selectedPresetFolder;
    if (! target.isDirectory()
        || (target != root && ! target.isAChildOf(root)))
        target = root;

    if (auto* item = presetFolderRoot->findItemForDirectory(
            target, favouritesViewSelected))
    {
        for (auto* parent = item->getParentItem(); parent != nullptr;
             parent = parent->getParentItem())
            parent->setOpen(true);
        item->setSelected(true, true, juce::sendNotification);
        presetFolderTree.scrollToKeepItemVisible(item);
    }
    else
    {
        selectedPresetFolder = root;
        presetFolderRoot->setSelected(true, true, juce::sendNotification);
    }
}

void DuskAudioProcessorEditor::selectPresetFolder(
    const juce::File& folder)
{
    const auto root = DuskAudioProcessor::getPresetsDirectory();
    if (! folder.isDirectory()
        || (folder != root && ! folder.isAChildOf(root)))
        return;

    favouritesViewSelected = false;
    selectedPresetFolder = folder;
    refreshPresetList();
}

void DuskAudioProcessorEditor::selectFavouritePresets()
{
    favouritesViewSelected = true;
    refreshPresetList();
}

void DuskAudioProcessorEditor::refreshPresetList()
{
    visiblePresetFiles = favouritesViewSelected
        ? favouritePresetFiles()
        : presetFilesInFolder(selectedPresetFolder);

    presetList.updateContent();
    presetList.deselectAllRows();
    if ((favouritesViewSelected && isPresetFavourite(currentPresetFile))
        || (! favouritesViewSelected
            && currentPresetFile.getParentDirectory() == selectedPresetFolder))
        for (int index = 0; index < visiblePresetFiles.size(); ++index)
            if (visiblePresetFiles[index] == currentPresetFile)
            {
                selectedPresetBrowserColumn = index % 2;
                presetList.selectRow(index / 2);
                break;
            }
    presetList.repaint();
    updatePresetNavigationDisplay();
}

void DuskAudioProcessorEditor::updatePresetNavigationDisplay()
{
    const auto files = navigationPresetFiles();
    const bool hasPresets = ! files.isEmpty();
    previousPresetButton.setEnabled(hasPresets);
    nextPresetButton.setEnabled(hasPresets);

    if (currentPresetFile != juce::File())
    {
        currentPresetLabel.setText(
            currentPresetFile.getFileNameWithoutExtension(),
            juce::dontSendNotification);
        currentPresetLabel.setTooltip(currentPresetFile.getFullPathName());
    }
    else
    {
        currentPresetLabel.setText("NO PRESET", juce::dontSendNotification);
        currentPresetLabel.setTooltip({});
    }
}

bool DuskAudioProcessorEditor::handlePresetCursorKey(int direction)
{
    auto& keyDown = direction < 0
        ? previousPresetKeyDown : nextPresetKeyDown;
    if (! keyDown)
        stepPreset(direction);
    keyDown = true;
    return true;
}

void DuskAudioProcessorEditor::stepPreset(int direction)
{
    const auto files = navigationPresetFiles();
    if (files.isEmpty())
    {
        information.setText(
            "PRESET NAVIGATION | No presets are available in the selected folder.",
            juce::dontSendNotification);
        updatePresetNavigationDisplay();
        return;
    }

    int currentIndex = files.indexOf(currentPresetFile);
    if (currentIndex < 0)
        currentIndex = direction >= 0 ? -1 : 0;
    const int nextIndex = (currentIndex + (direction >= 0 ? 1 : -1)
                           + files.size()) % files.size();
    loadPresetFile(files[nextIndex]);
}

void DuskAudioProcessorEditor::showPresetQuickMenu()
{
    const auto files = navigationPresetFiles();
    if (files.isEmpty())
    {
        information.setText(
            "PRESET MENU | No presets are available in the current folder.",
            juce::dontSendNotification);
        return;
    }

    const auto safeThis = juce::Component::SafePointer<
        DuskAudioProcessorEditor>(this);
    auto quickList = std::make_unique<PresetPopupList>(
        files, currentPresetFile,
        [safeThis](const juce::File& file)
        {
            if (auto* editor = safeThis.getComponent())
                editor->loadPresetFile(file);
        });

    juce::PopupMenu menu;
    menu.setLookAndFeel(&lookAndFeel);
    menu.addCustomItem(1, std::move(quickList), nullptr, "Preset list");
    lookAndFeel.setPopupBorderColour(filterAccent(1));
    menu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetComponent(&currentPresetLabel)
            .withTargetScreenArea(currentPresetLabel.getScreenBounds())
            .withPreferredPopupDirection(
                juce::PopupMenu::Options::PopupDirection::downwards)
            .withMinimumWidth(220)
            .withMaximumNumColumns(1));
}

int DuskAudioProcessorEditor::getNumRows()
{
    return (visiblePresetFiles.size() + 1) / 2;
}

void DuskAudioProcessorEditor::paintListBoxItem(
    int row, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    const int firstColumnWidth = width / 2;
    for (int column = 0; column < 2; ++column)
    {
        const int index = row * 2 + column;
        if (! juce::isPositiveAndBelow(index, visiblePresetFiles.size()))
            continue;

        const int cellX = column == 0 ? 0 : firstColumnWidth;
        const int cellWidth = column == 0
            ? firstColumnWidth : width - firstColumnWidth;
        const auto& file = visiblePresetFiles.getReference(index);
        const bool isSelected = rowIsSelected
                             && column == selectedPresetBrowserColumn;
        if (isSelected)
        {
            g.setColour(presetAccentColour.withAlpha(0.30f));
            g.fillRoundedRectangle(static_cast<float>(cellX + 2), 1.0f,
                                   static_cast<float>(cellWidth - 4),
                                   static_cast<float>(height - 2), 3.0f);
        }

        const bool isCurrent = file == currentPresetFile;
        const bool isFavourite = isPresetFavourite(file);
        g.setColour(isFavourite ? knobIndicatorColour
                                : knobLabelColour.darker(0.25f));
        g.fillPath(makeStarPath({ static_cast<float>(cellX + 7),
                                 static_cast<float>(height) * 0.5f - 5.0f,
                                 10.0f, 10.0f }));
        g.setColour(isCurrent ? knobIndicatorColour : knobValueColour);
        g.setFont(juce::Font(juce::FontOptions(
            11.0f, isCurrent ? juce::Font::bold : juce::Font::plain)));
        g.drawFittedText(file.getFileNameWithoutExtension(), cellX + 24, 0,
                         juce::jmax(0, cellWidth - 48), height,
                         juce::Justification::centredLeft, 1);
        if (isCurrent)
        {
            g.setColour(knobIndicatorColour);
            g.fillEllipse(static_cast<float>(cellX + cellWidth - 18),
                          static_cast<float>(height) * 0.5f - 3.0f,
                          6.0f, 6.0f);
        }
    }
}

void DuskAudioProcessorEditor::listBoxItemClicked(
    int row, const juce::MouseEvent& event)
{
    const int eventWidth = event.eventComponent != nullptr
        ? event.eventComponent->getWidth() : presetList.getWidth();
    const int clickedColumn = event.position.x
        >= static_cast<float>(eventWidth) * 0.5f ? 1 : 0;
    const int clickedIndex = row * 2 + clickedColumn;
    if (! juce::isPositiveAndBelow(clickedIndex, visiblePresetFiles.size()))
    {
        if (event.mods.isPopupMenu())
            showPresetContextMenu({}, event.getScreenPosition());
        return;
    }
    selectedPresetBrowserColumn = clickedColumn;
    presetList.repaintRow(row);
    const int index = row * 2 + selectedPresetBrowserColumn;
    if (! juce::isPositiveAndBelow(index, visiblePresetFiles.size()))
        return;

    const int firstColumnWidth = eventWidth / 2;
    const int cellX = selectedPresetBrowserColumn == 0
        ? 0 : firstColumnWidth;
    const float positionInCell = event.position.x
        - static_cast<float>(cellX);
    const auto file = visiblePresetFiles[index];
    if (event.mods.isPopupMenu())
    {
        showPresetContextMenu(file, event.getScreenPosition());
        return;
    }
    if (positionInCell >= 3.0f && positionInCell <= 22.0f)
    {
        togglePresetFavourite(file);
        return;
    }
    loadPresetFile(file);
}

void DuskAudioProcessorEditor::returnKeyPressed(int row)
{
    loadPresetAtBrowserCell(row, selectedPresetBrowserColumn);
}

void DuskAudioProcessorEditor::selectedRowsChanged(int row)
{
    if (row < 0)
        return;

    if (! juce::isPositiveAndBelow(
            row * 2 + selectedPresetBrowserColumn,
            visiblePresetFiles.size()))
        selectedPresetBrowserColumn = 0;

    presetList.repaintRow(row);
}

void DuskAudioProcessorEditor::loadPresetAtBrowserCell(
    int row, int column)
{
    const int index = row * 2 + juce::jlimit(0, 1, column);
    if (! juce::isPositiveAndBelow(index, visiblePresetFiles.size()))
        return;

    loadPresetFile(visiblePresetFiles[index]);
}

void DuskAudioProcessorEditor::loadPresetFile(const juce::File& file)
{
    const bool preserveMix = mixLockButton.getToggleState();
    const double mixToPreserve = lockedMixValue;
    const juce::ScopedValueSetter<bool> mixRestoreGuard(
        restoringLockedMix, preserveMix);
    const auto result = processor.loadPresetFromFile(file);
    if (result.failed())
    {
        information.setText("PRESET LOAD FAILED | " + result.getErrorMessage(),
                            juce::dontSendNotification);
        return;
    }

    if (preserveMix)
    {
        setParameterActualValue(
            processor.parameters.getParameter("mix"),
            static_cast<float>(mixToPreserve));
        lockedMixValue = mixToPreserve;
    }

    currentPresetFile = file;
    selectedPresetFolder = file.getParentDirectory();
    if (presetBrowserOpen)
        refreshPresetList();
    else
        updatePresetNavigationDisplay();
    information.setText(
        "PRESET LOADED: " + file.getFileNameWithoutExtension()
            + "\nClick another preset to load it, or use the Save As... button to save a copy.",
        juce::dontSendNotification);
}

void DuskAudioProcessorEditor::saveCurrentPreset()
{
    if (currentPresetFile == juce::File())
    {
        showPresetSaveAs();
        return;
    }

    const auto result = processor.savePresetToFile(currentPresetFile);
    if (result.failed())
    {
        information.setText("PRESET SAVE FAILED | " + result.getErrorMessage(),
                            juce::dontSendNotification);
        return;
    }

    refreshPresetList();
    updatePresetNavigationDisplay();
    information.setText(
        "PRESET SAVED: " + currentPresetFile.getFileNameWithoutExtension()
            + "\nThe current preset file has been overwritten with the latest settings.",
        juce::dontSendNotification);
}

void DuskAudioProcessorEditor::showPresetSaveAs()
{
    const auto root = DuskAudioProcessor::getPresetsDirectory();
    root.createDirectory();
    auto destinationFolder = selectedPresetFolder;
    if (! destinationFolder.isDirectory()
        || (destinationFolder != root
            && ! destinationFolder.isAChildOf(root)))
        destinationFolder = root;

    const auto suggestedName = currentPresetFile == juce::File()
        ? juce::String("New Preset")
        : currentPresetFile.getFileNameWithoutExtension();
    const auto suggestedFile = destinationFolder.getChildFile(
        suggestedName + presetFileExtension);
    presetFileChooser = std::make_unique<juce::FileChooser>(
        "Save Dusk Preset As", suggestedFile,
        "*" + juce::String(presetFileExtension), true, false, this);
    const auto safeThis = juce::Component::SafePointer<
        DuskAudioProcessorEditor>(this);
    presetFileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [safeThis](const juce::FileChooser& chooser)
        {
            auto* editor = safeThis.getComponent();
            if (editor == nullptr)
                return;

            auto file = chooser.getResult();
            if (file == juce::File())
                return;
            if (! file.hasFileExtension(presetFileExtension))
                file = file.withFileExtension(presetFileExtension);

            const auto rootDirectory =
                DuskAudioProcessor::getPresetsDirectory();
            const auto parentDirectory = file.getParentDirectory();
            if (parentDirectory != rootDirectory
                && ! parentDirectory.isAChildOf(rootDirectory))
            {
                editor->information.setText(
                    "PRESET SAVE FAILED | Presets must be saved inside the Dusk Presets folder.",
                    juce::dontSendNotification);
                return;
            }

            const auto result = editor->processor.savePresetToFile(file);
            if (result.failed())
            {
                editor->information.setText(
                    "PRESET SAVE FAILED | " + result.getErrorMessage(),
                    juce::dontSendNotification);
                return;
            }

            editor->currentPresetFile = file;
            editor->selectedPresetFolder = parentDirectory;
            editor->rebuildPresetFolderTree();
            editor->updatePresetNavigationDisplay();
            editor->information.setText(
                "PRESET SAVED AS: " + file.getFileNameWithoutExtension()
                    + "\nThe Save button now overwrites this file; use Save As... to save another copy.",
                juce::dontSendNotification);
        });
}

void DuskAudioProcessorEditor::paint(juce::Graphics& g)
{
    const int designHeight = activeDesignHeight();
    g.fillAll(juce::Colour(0xff1b2026));
    const float scale = juce::jmin(
        static_cast<float>(getWidth()) / static_cast<float>(designWidth),
        static_cast<float>(getHeight()) / static_cast<float>(designHeight));
    const float offsetX = 0.5f * (static_cast<float>(getWidth())
        - static_cast<float>(designWidth) * scale);
    const float offsetY = 0.5f * (static_cast<float>(getHeight())
        - static_cast<float>(designHeight) * scale);
    g.saveState();
    g.addTransform(
        juce::AffineTransform::scale(scale).translated(offsetX, offsetY));

    const auto filter1Colour = filterAccent(1);
    const auto filter2Colour = filterAccent(2);
    const auto combinedColour = filter1Colour.interpolatedWith(
        filter2Colour, 0.5f);
    const float fullColourAmount = themeDial != nullptr
        ? juce::jlimit(0.0f, 1.0f,
            static_cast<float>(themeDial->slider.getValue() * 0.01))
        : 1.0f;
    const auto surroundColour = juce::Colour(0xff24292f).interpolatedWith(
        combinedColour, fullColourAmount);
    g.setColour(surroundColour);
    g.fillRoundedRectangle(8.0f, 52.0f,
                           static_cast<float>(designWidth - 16), 216.0f,
                           12.0f);

    const auto drawControlPanel = [&g](float x, float y,
                                       float width, float height)
    {
        g.setColour(juce::Colour(0xff171b20));
        g.fillRoundedRectangle(x, y, width, height, 8.0f);
        g.setColour(knobTrackColour);
        g.drawRoundedRectangle(x, y, width, height, 8.0f, 1.0f);
    };

    if (presetBrowserOpen)
    {
        drawControlPanel(18.0f, 62.0f, 890.0f, 196.0f);
        g.setColour(knobTrackColour);
        g.drawLine(288.0f, 70.0f, 288.0f, 250.0f, 1.0f);
    }
    else if (mainView == MainView::designer)
    {
        drawControlPanel(18.0f, 62.0f, 303.0f, 170.0f);
        drawControlPanel(325.0f, 62.0f, 206.0f, 170.0f);
        drawControlPanel(535.0f, 62.0f, 303.0f, 170.0f);
        drawControlPanel(842.0f, 62.0f, 66.0f, 170.0f);
    }
    else
    {
        drawControlPanel(18.0f, 62.0f, 303.0f, 170.0f);
        drawControlPanel(325.0f, 62.0f, 206.0f, 170.0f);
        drawControlPanel(535.0f, 62.0f, 303.0f, 170.0f);
        drawControlPanel(842.0f, 62.0f, 66.0f, 170.0f);
    }

    const juce::Rectangle<float> typeStrip(
        18.0f, 53.0f, static_cast<float>(designWidth - 36), 7.0f);
    g.setGradientFill(juce::ColourGradient(
        filter1Colour, typeStrip.getX(), typeStrip.getCentreY(),
        filter2Colour, typeStrip.getRight(), typeStrip.getCentreY(), false));
    g.fillRoundedRectangle(typeStrip, 3.5f);

    if (logoDrawable != nullptr)
        logoDrawable->drawWithin(
            g, juce::Rectangle<float>(22.0f, 5.0f, 280.0f, 43.0f),
            juce::RectanglePlacement::xLeft
                | juce::RectanglePlacement::yMid
                | juce::RectanglePlacement::onlyReduceInSize,
            1.0f);
    g.restoreState();
}

void DuskAudioProcessorEditor::resized()
{
    const int designHeight = activeDesignHeight();
    content.setTransform(juce::AffineTransform());
    content.setBounds(0, 0, designWidth, designHeight);
    const float scale = juce::jmin(
        static_cast<float>(getWidth()) / static_cast<float>(designWidth),
        static_cast<float>(getHeight()) / static_cast<float>(designHeight));
    const float offsetX = 0.5f * (static_cast<float>(getWidth())
        - static_cast<float>(designWidth) * scale);
    const float offsetY = 0.5f * (static_cast<float>(getHeight())
        - static_cast<float>(designHeight) * scale);
    content.setTransform(
        juce::AffineTransform::scale(scale).translated(offsetX, offsetY));

    constexpr int knobWidth = 60;
    constexpr int labelWidth = 70;
    const auto placeDial = [knobWidth, labelWidth](
        Dial* dial, int knobX, int labelY)
    {
        if (dial == nullptr)
            return;
        dial->label.setBounds(
            knobX - (labelWidth - knobWidth) / 2,
            labelY, labelWidth, 13);
        dial->slider.setBounds(knobX, labelY + 13, knobWidth, 57);
    };
    // Four equal Designer cells fit two complete rows around the permanent
    // graph. Their slightly smaller font is uniform rather than condensed,
    // leaving natural room for both the caption and draggable "::" marker.
    constexpr int designerKnobWidth = 52;
    constexpr int designerKnobHeight = 49;
    constexpr int designerLabelWidth = 75;
    const auto placeDesignerDial = [designerKnobWidth,
                                    designerKnobHeight,
                                    designerLabelWidth](
        Dial* dial, int knobX, int labelY)
    {
        if (dial == nullptr)
            return;
        dial->label.setBounds(
            knobX - (designerLabelWidth - designerKnobWidth) / 2,
            labelY, designerLabelWidth, 13);
        dial->slider.setBounds(
            knobX, labelY + 13, designerKnobWidth, designerKnobHeight);
    };

    placeDial(filter1CutoffDial, 160, 81);
    placeDial(filter1ResonanceDial, 225, 81);
    placeDial(filter1DriveDial, 160, 154);
    placeDial(filter1SlopeDial, 225, 154);
    placeDial(filter2CutoffDial, 677, 81);
    placeDial(filter2ResonanceDial, 742, 81);
    placeDial(filter2DriveDial, 677, 154);
    placeDial(filter2SlopeDial, 742, 154);
    placeDesignerDial(filter1ResonanceShapeDial, 30, 89);
    placeDesignerDial(filter1ResonanceLimitDial, 105, 89);
    placeDesignerDial(filter1BassLossDial, 180, 89);
    placeDesignerDial(filter1PoleSpreadDial, 255, 89);
    placeDesignerDial(filter1CoreSaturationDial, 30, 159);
    placeDesignerDial(filter1SaturationAsymmetryDial, 105, 159);
    placeDesignerDial(filter1FeedbackDial, 180, 159);
    placeDesignerDial(filter1FeedbackLimitDial, 255, 159);
    placeDesignerDial(filter1LadderStageSaturationDial, 30, 89);
    placeDesignerDial(filter1LadderFeedbackSaturationDial, 105, 89);
    placeDesignerDial(filter1LadderResonanceCompensationDial, 180, 89);
    placeDesignerDial(filter1OtaCellDriveDial, 30, 89);
    placeDesignerDial(filter1OtaFeedbackDampingDial, 105, 89);
    placeDesignerDial(filter1OtaResonanceCompensationDial, 180, 89);
    placeDesignerDial(filter2ResonanceShapeDial, 547, 89);
    placeDesignerDial(filter2ResonanceLimitDial, 622, 89);
    placeDesignerDial(filter2BassLossDial, 697, 89);
    placeDesignerDial(filter2PoleSpreadDial, 772, 89);
    placeDesignerDial(filter2CoreSaturationDial, 547, 159);
    placeDesignerDial(filter2SaturationAsymmetryDial, 622, 159);
    placeDesignerDial(filter2FeedbackDial, 697, 159);
    placeDesignerDial(filter2FeedbackLimitDial, 772, 159);
    placeDesignerDial(filter2LadderStageSaturationDial, 547, 89);
    placeDesignerDial(filter2LadderFeedbackSaturationDial, 622, 89);
    placeDesignerDial(filter2LadderResonanceCompensationDial, 697, 89);
    placeDesignerDial(filter2OtaCellDriveDial, 547, 89);
    placeDesignerDial(filter2OtaFeedbackDampingDial, 622, 89);
    placeDesignerDial(filter2OtaResonanceCompensationDial, 697, 89);
    placeDial(mixDial, 845, 67);
    placeDial(outputDial, 845, 154);

    versionLabel.setBounds(designWidth - 65, 2, 66, 12);
    filterViewButton.setBounds(320, 9, 112, 14);
    designerViewButton.setBounds(320, 25, 112, 14);
    routingLabel.setBounds(516, 2, 110, 12);
    routingBox.setBounds(516, 16, 110, 26);
    currentPresetLabel.setBounds(632, 13, 94, 26);
    previousPresetButton.setBounds(726, 13, 21, 26);
    nextPresetButton.setBounds(747, 13, 21, 26);
    presetBrowserButton.setBounds(774, 13, 26, 26);
    if (themeDial != nullptr)
    {
        themeDial->label.setBounds(designWidth - 118, 2, 58, 12);
        themeDial->slider.setBounds(designWidth - 104, 16, 30, 30);
    }
    if (browserMixDial != nullptr)
    {
        browserMixDial->label.setBounds(designWidth - 118, 2, 58, 12);
        browserMixDial->slider.setBounds(designWidth - 104, 16, 30, 30);
    }
    mixLockButton.setBounds(891, 66, 14, 14);
    bypassButton.setBounds(designWidth - 47, 16, 30, 30);

    filter1TitleLabel.setBounds(26, 66, 190, 13);
    filter2TitleLabel.setBounds(543, 66, 190, 13);
    filter1PowerButton.setBounds(299, 65, 16, 16);
    filter2PowerButton.setBounds(816, 65, 16, 16);
    filter1ArchitectureLabel.setBounds(47, 81, 90, 12);
    filter1ArchitectureBox.setBounds(47, 93, 90, 20);
    filter1TypeLabel.setBounds(47, 120, 90, 12);
    filter1TypeBox.setBounds(47, 132, 90, 20);
    filter1DistortionTypeLabel.setBounds(47, 159, 90, 12);
    filter1DistortionTypeBox.setBounds(47, 171, 90, 20);
    filter1DistortionPlacementLabel.setBounds(47, 198, 90, 12);
    filter1DistortionPlacementBox.setBounds(47, 210, 90, 20);
    filter2ArchitectureLabel.setBounds(564, 81, 90, 12);
    filter2ArchitectureBox.setBounds(564, 93, 90, 20);
    filter2TypeLabel.setBounds(564, 120, 90, 12);
    filter2TypeBox.setBounds(564, 132, 90, 20);
    filter2DistortionTypeLabel.setBounds(564, 159, 90, 12);
    filter2DistortionTypeBox.setBounds(564, 171, 90, 20);
    filter2DistortionPlacementLabel.setBounds(564, 198, 90, 12);
    filter2DistortionPlacementBox.setBounds(564, 210, 90, 20);
    responseDisplay.setBounds(333, 70, 190, 154);
    designer1Placeholder.setBounds(26, 66, 124, 13);
    filter1ModelLabel.setBounds(150, 66, 38, 13);
    filter1ModelBox.setBounds(188, 61, 96, 20);
    filter1ModelMenuButton.setBounds(290, 61, 22, 20);
    designer2Placeholder.setBounds(543, 66, 124, 13);
    filter2ModelLabel.setBounds(667, 66, 38, 13);
    filter2ModelBox.setBounds(705, 61, 96, 20);
    filter2ModelMenuButton.setBounds(806, 61, 22, 20);

    presetFolderLabel.setBounds(28, 66, 252, 14);
    presetListLabel.setBounds(298, 66, 560, 14);
    presetFolderTree.setBounds(26, 82, 256, 168);
    presetList.setBounds(296, 82, 564, 168);
    presetSaveButton.setBounds(870, 85, 26, 26);
    presetSaveAsButton.setBounds(870, 130, 26, 26);
    presetOpenFolderButton.setBounds(870, 175, 26, 26);

    information.setBounds(25, 234, 803, 32);
    randomButton.setBounds(836, 236, 26, 26);
    sequencerButton.setBounds(presetBrowserOpen ? 865 : 872,
                              presetBrowserOpen ? 220 : 236,
                              36, 26);
    if (sequencerPanel != nullptr)
        sequencerPanel->setBounds(
            8, collapsedDesignHeight, designWidth - 16,
            expandedDesignHeight - collapsedDesignHeight - 8);

    if (isShowing() && ! presetBrowserOpen
        && getWidth() != observedEditorWidth)
    {
        const int scalePercent = juce::jlimit(
            75, 200, juce::roundToInt(
                100.0f * static_cast<float>(getWidth())
                    / static_cast<float>(designWidth)));
        information.setText(
            "GUI SIZE: " + juce::String(scalePercent) + "%\n"
                + juce::String(getWidth()) + " x "
                + juce::String(getHeight()) + " px",
            juce::dontSendNotification);
    }
}

void DuskAudioProcessorEditor::timerCallback()
{
    const bool leftKeyDown = isShowing()
        && juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::leftKey);
    const bool rightKeyDown = isShowing()
        && juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::rightKey);
    if (leftKeyDown && ! previousPresetKeyDown)
        stepPreset(-1);
    if (rightKeyDown && ! nextPresetKeyDown)
        stepPreset(1);
    previousPresetKeyDown = leftKeyDown;
    nextPresetKeyDown = rightKeyDown;

    updateSequencerTargetHighlights();
    if (sequencerOpen && sequencerPanel != nullptr)
        sequencerPanel->timerTick();
    updateAnimatedParameterDisplays();
    const std::array<int, 2> architectureChoices {
        architectureChoiceForFilter(1), architectureChoiceForFilter(2)
    };
    if (architectureChoices != displayedArchitectureChoices)
    {
        updateArchitectureContext();
        updatePresetBrowserVisibility();
    }
    if (responseDisplay.isVisible())
        responseDisplay.repaint();

    const auto processorPresetFile = processor.getCurrentPresetFile();
    if (processorPresetFile != currentPresetFile)
    {
        const auto root = DuskAudioProcessor::getPresetsDirectory();
        const auto parent = processorPresetFile.getParentDirectory();
        if (processorPresetFile != juce::File()
            && (parent == root || parent.isAChildOf(root)))
        {
            currentPresetFile = processorPresetFile;
            selectedPresetFolder = parent;
        }
        else
            currentPresetFile = juce::File();
        if (presetBrowserOpen)
            rebuildPresetFolderTree();
        else
            updatePresetNavigationDisplay();
    }

    const int currentWidth = getWidth();
    const double currentTheme = themeDial != nullptr
        ? themeDial->slider.getValue() : 100.0;
    if (currentWidth != observedEditorWidth
        || std::abs(currentTheme - observedThemeValue) > 0.0001)
    {
        observedEditorWidth = currentWidth;
        observedThemeValue = currentTheme;
        stableSettingsTicks = 0;
        settingsDirty = true;
    }
    else if (settingsDirty && ++stableSettingsTicks >= 5)
    {
        saveInterfaceSettings();
        stableSettingsTicks = 0;
        settingsDirty = false;
    }
}

void DuskAudioProcessorEditor::saveInterfaceSettings() const
{
    const auto directory = DuskAudioProcessor::getSettingsDirectory();
    if (! directory.isDirectory() && directory.createDirectory().failed())
        return;
    const int scalePercent = juce::jlimit(
        75, 200, juce::roundToInt(100.0f * static_cast<float>(getWidth())
                                  / static_cast<float>(designWidth)));
    const double themePercent = themeDial != nullptr
        ? juce::jlimit(0.0, 100.0, themeDial->slider.getValue()) : 100.0;
    juce::String text;
    const auto mainViewName = mainView == MainView::designer
        ? juce::String("Designer") : juce::String("Filter");
    text << "; Dusk interface settings\r\n"
         << "[GUI]\r\n"
         << "ScalePercent=" << scalePercent << "\r\n"
         << "ThemePercent=" << juce::String(themePercent, 1) << "\r\n"
         << "MainView=" << mainViewName
         << "\r\n"
         << "SequencerOpen=" << (sequencerOpen ? 1 : 0) << "\r\n"
         << "SequencerAnimate="
         << (sequencerPanel != nullptr && sequencerPanel->getAnimate()
                 ? 1 : 0)
         << "\r\n";
    directory.getChildFile("Settings.ini").replaceWithText(text);
}



