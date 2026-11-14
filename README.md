# Dusk

<div align="center">

[![Release](https://img.shields.io/github/v/release/sl2365/Dusk?style=for-the-badge-square&logo=github&logoColor=white&color=purple)](https://github.com/sl2365/Dusk/releases/latest/download/Dusk.rar)
[![Release Date](https://img.shields.io/github/release-date/sl2365/Dusk?style=for-the-badge-square&logo=github&logoColor=white&color=yellow)](https://github.com/sl2365/Dusk/releases)

[![Latest Asset Downloads](https://img.shields.io/github/downloads/sl2365/Dusk/latest/Dusk.rar?style=for-the-badge-square&logo=github&logoColor=white&label=downloads-latest&displayAssetName=false&color=blue)](https://github.com/sl2365/Dusk/releases/latest)
[![Total Downloads](https://img.shields.io/github/downloads/sl2365/Dusk/total?style=for-the-badge-square&logo=github&logoColor=white&label=downloads-total&color=blue)](https://github.com/sl2365/Dusk/releases)

</div>

Dusk is a dual creative-filter VST3 by Dead Channel Audio. Each filter combines standard response controls with a selectable DSP architecture, editable architecture-specific Designer settings, its own distortion stage and sequencer modulation.

![Dusk](Resources/Dusk.jpg)

## Requirements

- Windows x64
- A 64-bit VST3 host
- A writable VST3 folder if Dusk's portable settings, presets, sequences and Models are to remain beside the plugin

## Installation

1. Extract `Dusk.vst3` from the release archive.
2. Copy it into a folder scanned by the host for VST3 plugins.
3. Rescan plugins in the host and load Dusk as an audio effect.

Dusk creates a portable `Dusk` data folder beside the installed plugin. If the chosen VST3 folder is protected by Windows, give the current account write access or use another scanned folder that is writable.

## Quick start

1. Select `Filter 1`, `Filter 2`, `1 > 2` or `1 + 2` from Routing.
2. Use each amber filter LED to switch that filter on or off. Lit means on.
3. Choose an Architecture for each active filter. Clean SVF, Ladder and OTA use the standard Type control; Comb, Formant and Phaser provide their own main controls.
4. For standard architectures, set Cutoff, Resonance and the continuous 6–48 dB/octave Slope. Comb uses Frequency, Feedback and Damping; Formant uses Vowel, Shift and Focus; Phaser uses Frequency, Feedback and Depth.
5. Add Drive, choose a distortion Type, then choose its Pre, Internal or Post Placement.
6. Open DESIGNER to change the controls belonging to the selected Architecture or recall a Model.
7. Use MIX and OUT to set the final wet balance and level.

Hover any control or its label to see its matching description in the fixed help strip.

## Signal routing

| Routing | Signal path |
|---|---|
| Filter 1 | Filter 1 only |
| Filter 2 | Filter 2 only |
| 1 > 2 | Filter 1 feeds Filter 2 in series |
| 1 + 2 | Both filters process the input in parallel and are gain-compensated before mixing |

The response display follows the selected architectures and their applicable FILTER and DESIGNER controls. It shows the exact small-signal filter response; nonlinear saturation, high-level limiting and selectable distortion cannot be represented by one fixed level-independent curve. Comb Stereo Spread and Phaser Stereo are excluded because the compact display represents the centre-tuned mono response.

## FILTER view

Each side controls one independent filter engine.

| Control | Function |
|---|---|
| Architecture | Selects the genuine DSP structure: Clean SVF, Ladder, OTA, Comb, Formant or Phaser |
| Type | For Clean SVF, Ladder and OTA, selects LPF, BPF, HPF, NOTCH or PEAK |
| Cutoff | For Clean SVF, Ladder and OTA, sets the principal frequency from 20 Hz to 20 kHz |
| Resonance | For Clean SVF, Ladder and OTA, emphasises the response around Cutoff |
| Slope | For Clean SVF, Ladder and OTA, continuously changes the response from 6 to 48 dB/octave |
| Comb Mode | For Comb, selects Feedback, Feedforward or All-pass |
| Frequency | For Comb, sets the fundamental delay frequency from 20 Hz to 8 kHz |
| Feedback | For Comb, sets the amount and strength of the delayed branch |
| Damping | For Comb, darkens the delayed branch and shortens bright ringing |
| Vowel | For Formant, morphs continuously through A, E, I, O and U |
| Shift | For Formant, moves all three vocal bands together by plus or minus 24 semitones |
| Focus | For Formant, changes the three vocal bands from broad and gentle to sharp and prominent |
| Frequency | For Phaser, moves the complete phase-cancellation pattern from 20 Hz to 20 kHz |
| Feedback | For Phaser, sets positive or negative bounded feedback around the all-pass cascade |
| Depth | For Phaser, changes the internal dry/all-pass blend from transparent to deep notches |
| Drive | Sets the amount sent to the selected distortion model |
| Dist Type | Selects one of the 17 distortion models |
| Placement | Places that distortion before, inside or after the filter |
| Amber LED | Powers the individual filter on or off; lit is on |

PEAK is the selected architecture's resonant peak response. It is not a parametric-EQ bell with a separate gain control.

## Architectures

Architecture chooses how the filter is actually calculated. Changing Architecture also changes the controls shown on the DESIGNER view.

### Clean SVF

A topology-preserving state-variable core with continuously blended one-pole and cascaded-stage responses. It provides the widest general-purpose Designer set and an additional bounded feedback return.

### Ladder

A nonlinear cascaded-pole filter with one to eight continuously blended poles. The 24 dB position uses four poles. Resonance is handled by a delay-free feedback solution rather than by relabelling the Clean SVF.

### OTA

A nonlinear cascade of one to eight transconductance-style cells with a damped bounded feedback return. The 24 dB position uses four cells.

### Comb

A fractional-delay filter with three independently running structures. Feedback recirculates the delayed branch for resonant ringing, Feedforward creates a non-recurring notch/peak pattern, and All-pass changes phase while retaining a nominally flat linear magnitude response. Changing Mode crossfades the live engines instead of interpolating incompatible coefficients or clearing their state.

Frequency movement uses perceptual smoothing so long, low-frequency delay times do not jump abruptly while the control is moved or automated. The 20 Hz–8 kHz range keeps meaningful repeated teeth available throughout the knob instead of spending its upper section above the useful audible Comb range. A magnitude-preserving fractional-delay stage retains much more of the delayed branch at high frequencies than linear interpolation.

Feedback and audible delayed-branch level use separate curves: the recirculating loop remains safely below unity while the output branch is strong enough to make low and moderate Feedback positions clearly audible. Damping uses a gentler shaped range, retaining substantially more brightness at ordinary settings while still becoming dark at its upper extreme. At very high Feedback and low Frequency, a gentle internal low-cut progressively restrains sub-bass accumulation while leaving the main pitched and metallic Comb character intact. This protection is included in the response display and requires no extra control.

The Comb Architecture and the `Comb` distortion model are separate. The Architecture is a complete filter engine with Frequency, Feedback, Damping and dedicated Designer controls; the distortion model is a short resonant colour available in the Dist Type list.

### Formant

A three-band vocal resonator built from independent topology-preserving band-pass filters. Vowel morphs logarithmically through A, E, I, O and U rather than switching between fixed snapshots, so the intermediate positions remain usable sounds. Shift moves the complete vocal spectrum without changing its internal proportions, while Focus changes the prominence and sharpness of all three bands. Designer Width changes their bandwidth separately without changing peak emphasis.

Formant has no standard LPF/BPF/HPF Type because its three simultaneous vocal bands are the architecture. Its complete linear response, including every Designer level, Width, Spread and Tilt setting, is shown in the permanent response display.

### Phaser

A cascade of twelve continuously running first-order all-pass stages. The Stages control blends between the outputs of 2, 4, 6, 8, 10 and 12 stages instead of clearing state or abruptly switching DSP. Frequency moves the complete stage pattern, positive or negative Feedback reshapes the notches, and Depth controls the internal cancellation blend. Stage Sat's cumulative gain remains in the audible output, while an automatic loop-gain guard removes only that excess gain from the feedback return when it would cross the stable margin. This prevents a sustained squeal without flattening Stages or Stage Sat.

Phaser has no fixed internal LFO. Move or automate Frequency directly, or assign it to Dusk's sequencer for rhythmic and continuously repeating sweeps. The permanent response display includes Frequency, Feedback, Depth, Stages, Spread, Skew and feedback Damping. Stage Saturation, Stereo and selectable distortion are intentionally excluded from the mono small-signal curve.

## DESIGNER view

Designer controls edit the character of the selected Architecture. Clean SVF, Ladder and OTA keep the standard Type, Cutoff, Resonance and Slope controls on FILTER. Comb provides Mode, Frequency, Feedback and Damping; Formant provides Vowel, Shift and Focus; Phaser provides Frequency, Feedback and Depth. Drive remains available for every Architecture.

### Clean SVF Designer

| Control | Function |
|---|---|
| Res Shape | Changes how quickly the Resonance control builds from restrained to aggressive |
| Res Limit | Limits the maximum effective resonance |
| Bass Loss | Adds resonance-linked low-frequency thinning |
| Core Sat | Saturates the completed filter core; 0% is transparent |
| Sat Asym | Biases Core Sat toward one polarity for even-harmonic character; it requires Core Sat |
| Pole Spread | Separates applicable cascaded stages; it is inactive at 12 dB/octave and below |
| Feedback | Returns the completed filter output through a bounded one-sample loop |
| FB Limit | Sets how tightly the Designer Feedback return is constrained; it requires Feedback |

### Ladder Designer

| Control | Function |
|---|---|
| Stage Sat | Changes the nonlinearity inside every active Ladder stage |
| FB Sat | Saturates the Ladder resonance return inside its delay-free solve |
| Res Comp | Restores level as Resonance rises |

### OTA Designer

| Control | Function |
|---|---|
| Cell Drive | Changes the nonlinearity inside every active OTA cell |
| FB Damp | Low-pass damps and restrains the OTA resonance return |
| Res Comp | Restores level as Resonance rises |

### Comb Designer

| Control | Function |
|---|---|
| Fine Tune | Offsets the Comb frequency by plus or minus 100 cents |
| Polarity | Moves continuously between negative and positive delayed polarity; 0% removes the delayed branch |
| Stereo | Detunes the left and right delay frequencies in opposite directions by up to 100 cents |
| Dispersion | Adds all-pass phase dispersion inside the delayed branch |
| Loop Sat | Saturates and compresses the delayed branch at higher levels |
| FB Limit | Sets the high-level ceiling used to restrain ringing |

### Formant Designer

| Control | Function |
|---|---|
| F1 Level | Sets the lowest vocal resonance from silent to twice its natural level |
| F2 Level | Sets the middle resonance that carries much of the vowel identity |
| F3 Level | Sets the highest resonance controlling presence and articulation |
| Width | Scales all three vowel bandwidths from 50% to 200% |
| Spread | Compresses or expands the outer formants around the middle band by up to half an octave |
| Tilt | Rebalances the formants continuously from low and throaty to high and bright |

### Phaser Designer

| Control | Function |
|---|---|
| Stages | Continuously blends from 2 to 12 active all-pass stages; more stages create more notches |
| Spread | Expands the paired stage frequencies from a tight cluster to a broad six-octave pattern |
| Skew | Redistributes the stage frequencies around the centre without moving Frequency |
| FB Damp | Low-pass damps the Phaser feedback return |
| Stage Sat | Strongly saturates every all-pass stage, producing cumulative colour and level interaction as Stages increases; the protected feedback return prevents runaway resonance |
| Stereo | Moves the left and right stage patterns in opposite directions by up to three quarters of an octave |

## Filter Models

A Model is an editable configuration of one Architecture and its supported Designer controls. It is not another filter Type and it does not change Type, Cutoff, Resonance, Slope, Drive, routing, sequencer settings or the other filter.

Factory Models provide starting points for Clean SVF, Ladder, OTA, Comb, Formant and Phaser. After recalling one, move any included Designer control to customise it; the Model display then changes to `CUSTOM` without undoing the recalled values.

Use the `...` button beside either Model selector to:

- save that filter's current Architecture and Designer settings as a User Model;
- refresh the shared User Model list; or
- open the portable Models folder.

A User Model saved from Filter 1 can be recalled by Filter 2 and vice versa. User Models use `.duskmodel`; full-plugin presets use `.duskpreset`.

## Distortion

Each filter has an independent distortion model, Drive amount and Placement. Available models are:

`Amp`, `Asym`, `Bumpy`, `Comb`, `Crackle`, `Fresh`, `Fuzz`, `Hard Clip`, `Lightning`, `Pixelated`, `Razor`, `Rectify`, `Samurai`, `Saturation`, `Soft Clip`, `Stressed` and `Tube`.

The list is naturally sorted; each visible name remains mapped to its own DSP algorithm.

| Placement | Function |
|---|---|
| Pre | Distorts the signal before it enters the filter core |
| Internal | Distorts an architecture-specific feedback return inside the filter |
| Post | Distorts the completed filter output |

Internal behaves according to the selected Architecture:

- Clean SVF shapes the bounded Designer Feedback return, so it requires the Clean SVF `Feedback` control to be above 0%.
- Ladder shapes the stored resonance feedback component before the protected delay-free solve, so it requires `Resonance`.
- OTA shapes the damped bounded resonance return before it is subtracted from the input, so it requires `Resonance`.
- Comb shapes each Mode's delayed branch before its loop saturation, limiting and output/write calculation. Its three Modes keep separate internal distortion memory.
- Formant shapes each of its three vocal bands independently before their level, Tilt and final summing stages. Every band keeps separate internal distortion memory.
- Phaser shapes the selected all-pass output inside its bounded feedback return. The direct phase-cancellation path remains separate, and the Phaser has its own distortion-state memory.

The external Pre/Post path and each Architecture's Internal path use separate state memory. Stateful models such as Comb therefore cannot carry external-placement history into an Architecture's feedback path, or internal history from one Architecture into another.

## Sequencer

Press `SEQ` to show or hide the 32-step parameter sequencer.

### Creating a pattern

- Click or drag vertically across the step grid to set values.
- Use Waveform to replace the grid with a generated shape.
- Set Start and End to restrict the active range; enable the link button to move that range without changing its length.
- Use Loop, Bounce, Reverse, Played or Random direction.
- Enable Host Sync for tempo-locked rates, or disable it for the free/played behaviour.
- Attack and Release smooth rising and falling changes.
- Depth expands or contracts the complete pattern; Offset moves it vertically.
- Cycle the polarity LED through Unipolar +, Bipolar and Unipolar - to choose how modulation moves its targets.

### Assigning targets

Drag a parameter label marked `::` onto one of the 16 target slots. The target's small knob sets its individual modulation Depth, and its LED enables or disables that assignment. `ANIMATE` displays the resulting control movement without replacing stored parameter values.

### Gate steps

Each step can be Off, Short, Long or Rest. SHORT and LONG set the open duration. Gate can act on Input, Effect, Output or be switched Off independently of parameter modulation.

User sequences are stored as `.duskseq` files in the portable Sequences folder.

## Presets and global controls

- Open the preset browser with the list button.
- Use Save to overwrite the current preset or Save As to create a new `.duskpreset`.
- Use the arrow buttons, keyboard Left/Right keys or mouse wheel over the preset name to step through presets.
- Right-click the current preset name for the quick menu.
- MIX LOCK keeps the current MIX value when another preset is loaded.
- The global Power LED bypasses or enables Dusk processing.
- THEME changes the interface colour treatment.
- Drag the resize handle to scale the interface from 75% to 200%.
- The randomise button changes supported filter and Designer controls within bounded ranges while leaving Architecture, routing, filter power, Placement, MIX, global Power and sequencer state unchanged.

## Portable files

Dusk keeps its user data beside `Dusk.vst3`:

```text
Dusk/
├── Settings/
│   └── Settings.ini
├── Presets/
│   └── *.duskpreset
├── Sequences/
│   └── *.duskseq
└── Models/
    └── *.duskmodel
```

Back up this folder with the plugin to preserve all user content and interface settings.

## Building from source

Place the project at `_Projects\Dusk` beside `_Projects\_Tools`, then double-click `- Build.bat`.

The supplied build script expects:

- CMake at `_Projects\_Tools\cmake\_4.4.2\bin\cmake.exe`;
- JUCE 9.0.4 at `_Projects\_Tools\JUCE\_9.0.4`; and
- Visual Studio Community 2026 with the x64 C++ toolchain.

The script closes `PolyHostInterface.exe` before compiling, so save any work open in PHI first. It configures and builds the Release VST3, validates the output as Windows x64 and writes `dist\Dusk.vst3`. It does not install the plugin automatically.

If the build fails, read or supply the generated `Results.log`; it contains the complete configuration and compiler output.

## Project identity

| Field | Value |
|---|---|
| Manufacturer | `sl23` |
| Manufacturer code | `sl23` |
| Plugin code | `dsk1` |
| Bundle ID | `com.sl23.dusk` |
| VST3 category | `Fx Filter` |

Release history and implementation changes are documented in [Changelog.md](Changelog.md).
