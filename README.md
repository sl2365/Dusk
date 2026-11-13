# Dusk

<div align="center">

[![Release](https://img.shields.io/github/v/release/sl2365/Dusk?style=for-the-badge-square&logo=github&logoColor=white&color=purple)](https://github.com/sl2365/Dusk/releases/latest/download/Dusk.rar)
[![Release Date](https://img.shields.io/github/release-date/sl2365/Dusk?style=for-the-badge-square&logo=github&logoColor=white&color=yellow)](https://github.com/sl2365/Dusk/releases)

[![Latest Asset Downloads](https://img.shields.io/github/downloads/sl2365/Dusk/latest/Dusk.rar?style=for-the-badge-square&logo=github&logoColor=white&label=downloads-latest&displayAssetName=false&color=blue)](https://github.com/sl2365/Dusk/releases/latest)
[![Total Downloads](https://img.shields.io/github/downloads/sl2365/Dusk/total?style=for-the-badge-square&logo=github&logoColor=white&label=downloads-total&color=blue)](https://github.com/sl2365/Dusk/releases)

</div>

Dusk is a Dead Channel Audio creative-filter VST3 by sl23. Version 0.17.1 substantially widens the audible range of the existing Clean SVF, Ladder and OTA Designer controls without changing their parameter IDs, ranges or defaults.

It is intentionally not a delay and does not load or convert any legacy preset format.

![Dusk](Resources/Dusk.jpg)

## Current audio behaviour

- Stereo and mono audio are processed by two independent architecture-dispatched filter engines. `Clean SVF` retains the established topology-preserving-transform State-Variable Filter, `Ladder` uses its own nonlinear cascaded-pole state and delay-free resonance loop, and `OTA` uses an independent nonlinear transconductance-cell cascade with a damped feedback return.
- There are no echo buffers, delay lines, timing controls, repeat paths, freeze capture, multi-tap system or delay modes.
- Each filter has independent 20 Hz–20 kHz Cutoff, Resonance, continuously variable 6.0–48.0 dB/octave Slope and smoothly crossfaded Power controls. Its ordinary round amber LED is lit when that filter is processing and unlit when it is off.
- Each FILTER panel has an independent Type selector containing LPF, BPF, HPF, NOTCH and PEAK. Type changes crossfade over 20 ms through the selected architecture's own continuously available outputs instead of resetting filter state.
- Each FILTER panel also has its own Architecture selector containing `Clean SVF`, `Ladder` and `OTA`. All engines remain live and Architecture changes use a direct 50 ms output crossfade between the previous and selected engines rather than coefficient interpolation or an abrupt state replacement.
- In Clean SVF, LPF and HPF use true one-pole 6 dB endpoints and mirrored continuously introduced stages up to 48 dB/octave. BPF uses the genuine two-pole band output below 12 dB, then progressively adds normalised band stages above 12 dB. Notch and Peak likewise use their genuine SVF outputs and progressively add same-cutoff stages above 12 dB.
- Slope is a basic FILTER-tab control beside Cutoff, Resonance and Drive. Clean SVF retains its established continuous one-pole / SVF / cascaded-stage response. Ladder and OTA each continuously select and interpolate one through eight of their own cascade taps, covering 6–48 dB/octave with 24 dB as the four-cell/four-pole position.
- The Clean SVF Designer has independent Resonance Shape, Resonance Limit, Bass Loss, Core Saturation, Saturation Asymmetry, Pole Spread, Feedback and Feedback Limit controls. Their parameter ranges and defaults remain unchanged, while the underlying sound-design curves now use far more of the available range.
- Resonance Shape now spans a wider exponential response, Bass Loss can remove up to 24 dB at maximum effective resonance, and Pole Spread can separate applicable cascaded stages by up to ±3 octaves. Pole Spread remains naturally inactive at 12 dB/octave or below.
- Core Saturation reaches a much lower maximum threshold, making the upper half strongly audible. Saturation Asymmetry now uses bounded independent positive and negative thresholds, producing much clearer even-harmonic character while preserving zero at zero and DC-blocking only the asymmetric difference.
- Clean SVF Feedback reaches 0.995 maximum gain. Feedback Limit now spans a 0.1–8.0 return threshold, providing a much larger contrast between tight compressed oscillation and a wide, louder return while remaining inactive at 0% Feedback.
- Feedback and Feedback Limit use 50 ms smoothing for safe manual, host-automation and sequencer movement.
- The Ladder Designer exposes independent Stage Saturation, Feedback Saturation and Resonance Compensation. Both saturation controls now use strongly expanded upper-range drive while retaining a unity small-signal slope and exact 0% transparency.
- Ladder Resonance Compensation now reaches four-times input gain at maximum Resonance and Compensation, making its level-restoration range immediately apparent without changing the feedback amount.
- Ladder resonance uses a delay-free feedback solution with a nonlinear stage cascade and a bounded four-iteration solve when Feedback Saturation is active. Its maximum loop gain follows the selected pole depth so high slopes do not begin uncontrolled oscillation prematurely. All three Ladder Designer controls use 50 ms smoothing.
- The OTA Designer exposes Cell Drive, Feedback Damping and Resonance Compensation. Cell Drive now reaches aggressive per-cell compression in its upper range, Feedback Damping reaches a much darker and more restrained return, and Resonance Compensation can reach four-times input gain at maximum Resonance.
- OTA resonance uses a bounded one-sample return kept below unity loop gain, with additional gain compensation as Feedback Damping introduces phase lag. The damped return, all eight cell states and all three OTA Designer controls run continuously with 50 ms parameter smoothing so switching and automation do not recall stale state.
- Each Designer header now has its own Model selector. A Model recalls only that filter's Architecture and the Designer parameters genuinely owned by the selected Architecture; it never changes Type, Cutoff, Resonance, Slope, Drive, distortion settings, routing, sequencer state or the other filter.
- Factory Models provide deliberately contrasted editable starting points: Clean SVF, Smooth SVF, Warm SVF, Wide Resonance, Clean Ladder, Classic Ladder, Acid Ladder, Warm Vintage, Clean OTA, Punchy OTA, Hot OTA and Dark OTA. Editing Architecture or any included Designer control changes the Model display to `CUSTOM` without undoing the recalled values.
- The adjacent `...` menu saves the current Architecture and applicable Designer controls as a shared User Model, refreshes the User list or opens the portable Models folder. User Models saved from either filter can be recalled by either filter.
- Each filter owns an independent copy of the retained 17-model distortion bank, Drive amount and Pre / Internal / Post placement parameter.
- Distortion Type choices are naturally sorted alphabetically while a stable mapping retains the correct DSP model and contextual description for every name.
- `MIX`, `OUT` and smoothed host bypass / Power remain functional.
- Routing provides Filter 1 only, Filter 2 only, serial Filter 1 into Filter 2, and gain-compensated parallel processing.
- Pre and Post distortion use genuinely different signal paths around each filter core. `Internal (N/A)` remains visibly unavailable until a later stage explicitly inserts the selected distortion model into an architecture's bounded nonlinear feedback return. It will then read `Internal` and become selectable only for architectures that safely support it; the Clean SVF Designer Feedback path and Ladder resonance loop are already active independently.
- The response display derives Filter 1, Filter 2 and combined curves from the exact low-level transfer calculations of the selected Clean SVF, Ladder or OTA engine. Serial routing multiplies the complex responses; parallel routing combines them with phase intact.
- The graph follows all applicable linear and low-level controls, including Ladder Resonance Compensation plus OTA Feedback Damping and Resonance Compensation. Because OTA Cell Drive, Ladder Stage Saturation, Feedback Saturation, Core Saturation, Feedback Limit and selectable distortion are nonlinear, the graph deliberately represents the exact small-signal response rather than claiming to show one fixed high-level response.

Each Comb distortion instance retains its own tiny 96-sample resonator state. These belong exclusively to that distortion algorithm; they are not echo buffers or a dormant delay engine.

## Retained interface systems

- Dark scalable 75–200% GUI and panel framework.
- Two vertical FILTER / DESIGNER LED radio choices occupy the clear header space to the right of the logo without overlapping it and persist in `Settings.ini`.
- FILTER provides independent Architecture selectors for Filter 1 and Filter 2 alongside their standard controls. `Clean SVF`, `Ladder` and `OTA` are separate DSP implementations, not relabelled factory Models.
- Equal Filter 1 and Filter 2 control areas with a centred 206 px response-display foundation.
- Standard response Type remains correctly on the FILTER tab. The new per-filter Model selectors live in the Designer headers and recall editable Architecture / Designer configurations rather than replacing the real DSP structures with opaque model names.
- The calculated Filter Response remains in the same centre panel on FILTER and DESIGNER, eliminating view changes merely to inspect response edits.
- Two simultaneous context-sensitive Mode Designer areas identify themselves as the selected architecture's Designer. Clean SVF exposes its established eight controls; Ladder exposes Stage Saturation, Feedback Saturation and Resonance Compensation; OTA exposes Cell Drive, Feedback Damping and Resonance Compensation. Essential Type, Cutoff, Resonance, Drive and Slope remain together on FILTER.
- Designer knobs use the same 52 x 49 px footprint as the sequencer's Attack and Release knobs. FILTER-tab knob sizing is unchanged.
- Designer caption bounds provide natural room for each complete draggable `::` target marker without condensing or horizontally scaling the label font.
- Binary `Resources/logo.svg` loading and visible version label.
- Theme amount, fixed two-colour bar and separate Filter 1 / Filter 2 type-colour hooks.
- Preset browser with folders, favourites, copy/paste, Save, Save As, quick menu, previous/next navigation and keyboard navigation.
- Portable `Settings/Settings.ini` persistence for scale, theme, FILTER / DESIGNER selection and sequencer view state.
- Fixed two-line contextual help.
- Context help always identifies the control actually under the pointer. A label and its associated knob or dropdown share the same visible control title and description; nameless internal child components never fall back to `DUSK` or another unrelated title.
- Hover help uses `CONTROL | description` so useful text begins on the first line and can naturally wrap across both lines. The strip uses the available width beside the bottom buttons and keeps natural, uncondensed font proportions.
- Main randomiser plus the sequencer target randomiser.
- Mix Lock and Power.

## Sequencer and Gate

The complete 32-step hide/show sequencer remains available:

- Host Sync or Played MIDI triggering.
- Loop, Bounce, Reverse, Played and Random directions.
- Unipolar +, Bipolar and Unipolar - operation.
- Start / End range, linked range movement, Attack, Release, Depth and Offset.
- Generated waveforms, saved user sequences and animated destination displays.
- Sixteen drag-and-drop target slots with enable state and individual 0–100% Depth.
- Per-step Off / Short / Long / Rest Gate states, independent Short and Long lengths, Gate nudging and Gate randomisation.
- Gate routing modes: Input, Effect, Output and Off.

The target list contains only controls that genuinely exist: MIX, OUT, Filter 1/2 Cutoff, Resonance, Resonance Shape, Resonance Limit, Bass Loss, Core Saturation, Saturation Asymmetry, Pole Spread, Feedback, Feedback Limit, Ladder Stage Saturation, Ladder Feedback Saturation, Ladder Resonance Compensation, OTA Cell Drive, OTA Feedback Damping, OTA Resonance Compensation, Slope and Drive, plus the sequencer's own eligible controls.

User sequence files use the new `.duskseq` format and `DuskSequence` root tag.

## New identity and storage

- Product: `Dusk`
- Version: `0.17.1`
- Manufacturer / code: `sl23`
- Plugin code: `dsk1`
- Bundle ID: `com.sl23.dusk`
- VST3 category: `Fx Filter`
- State root: `DuskParameters`
- State schema: `17`
- Preset extension: `.duskpreset`
- User Model extension / root: `.duskmodel` / `DuskFilterModel`
- User data beside the installed plugin: `Dusk/Settings`, `Dusk/Presets`, `Dusk/Sequences`, `Dusk/Models`

No old parameter IDs, state migration, preset conversion or compatibility code is included.

## Per-filter Factory / User Models

- Filter 1 and Filter 2 each have an independent Designer-header Model selector with Factory Clean SVF, Factory Ladder, Factory OTA and naturally sorted User sections.
- Architecture remains a real DSP selection on FILTER. A Model is only a configuration of that Architecture and its supported Designer controls.
- Clean SVF Models contain Resonance Shape, Resonance Limit, Bass Loss, Core Saturation, Saturation Asymmetry, Pole Spread, Feedback and Feedback Limit. Ladder Models contain Stage Saturation, Feedback Saturation and Resonance Compensation. OTA Models contain Cell Drive, Feedback Damping and Resonance Compensation.
- Factory Models are read-only starting points and every recalled value remains immediately editable. User Models use filter-neutral parameter names, so a Model created from Filter 1 can be recalled into Filter 2 and vice versa.
- User Models are deliberately separate from global `.duskpreset` files. Global presets continue to store the complete plugin state; `.duskmodel` files store only one filter's Architecture-specific design recipe.
- Loading a Model leaves LPF, BPF, HPF, Notch or Peak Type untouched. The same design can therefore be used across any compatible standard response without silently changing the musician's basic filter setup.
- Comb and future architectures will receive their own Model whitelist only after their genuine DSP and context-sensitive Designer controls exist; they will not be approximated by relabelling an existing response.

## Build

Place the project at `_Projects\Dusk` beside `_Projects\_Tools`, then double-click:

`- Build.bat`

The script expects:

- CMake at `_Projects\_Tools\cmake\_4.4.2\bin\cmake.exe`
- JUCE 9.0.4 at `_Projects\_Tools\JUCE\_9.0.4`
- Visual Studio Community 2026 x64

The portable VST3 is written to `dist\Dusk.vst3`. Nothing is installed automatically. If a build fails, send the generated `Results.log`.

## Next development stage

Evaluate the widened Designer ranges, Factory / User Model contrast, architecture switching and portable file handling in the Windows build, then continue with the next genuine architecture or architecture-specific Internal distortion stage without changing the Model / Architecture boundary.
