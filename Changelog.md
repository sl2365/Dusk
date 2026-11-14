# Dusk Changelog

## 0.21.5 — Phaser Feedback Squeal Correction — 14 November 2026

- Identified the sustained squeal as a feedback-loop fault rather than excessive Stage Sat output. The restored per-stage saturation makeup could multiply the small-signal loop gain above unity as Stages increased.
- Kept the strong 0.21.4 Stage Sat curve and direct Phaser output unchanged.
- Added a smooth loop-gain guard that begins only when the calculated cumulative stage gain would push the feedback return beyond the stable margin. It leaves values below the knee unchanged and asymptotically limits the effective loop gain to 0.90.
- Retained the existing final soft limiter as a second protection layer, but no longer relies on amplitude limiting to stop an already-sustained oscillation.
- Verified that the previously persistent reference state now decays to silence after excitation, including the 5.8–6-stage region, while the strong output-side Stage Sat and Stages interaction remains.
- Kept all parameter IDs, ranges, defaults, Models and sequencer target numbers unchanged; state schema remains version 20.

## 0.21.4 — Phaser Strength Restoration — 14 November 2026

- Restored Phaser Stage Sat to a strong cumulative per-stage curve, reducing the original maximum drive only modestly from nine-times to eight-times rather than making it gain-neutral.
- Restored maximum Phaser feedback from the over-restrained 0.82 to 0.86, close to the original 0.88, with no stage-dependent attenuation.
- Retained the separate smooth feedback safety limiter from 0.21.3. It acts on excessive loop level rather than weakening Stages or Stage Sat throughout their normal ranges.
- Measured Stage Sat and the four-to-eight-stage interaction within approximately 1–3 dB of the original Phaser behaviour in the reference test, instead of the 8–16 dB reduction in 0.21.3.
- Retained the corrected Model heading font calculation from 0.21.3 and kept all parameter IDs, ranges, defaults, Models and sequencer target numbers unchanged.

## 0.21.3 — Model Heading and Phaser Control Correction — 14 November 2026

- Matched Model section-heading glyph height to the exact ordinary-entry calculation, including the two-pixel text-area reduction used by JUCE's entry renderer. Headings retain bold weight and colour but are no longer visually larger.
- Restored most of the Phaser's original feedback strength instead of reducing it progressively with Stages, so moving Stages again produces an obvious change in notch density and character.
- Replaced the stage-dependent feedback reduction with a smooth, gain-preserving soft limiter inside the feedback return. Normal levels retain their character while excessive resonance can no longer run away around the six-stage transition.
- Strengthened Stage Sat's drive and wet curve substantially while retaining unity small-signal gain. It is clearly audible again without reinstating the previous gain multiplication through every stage.
- Kept all parameter IDs, ranges, defaults, Models and sequencer target numbers unchanged; state schema remains version 20.

## 0.21.2 — Model Heading Size and Phaser Restraint — 14 November 2026

- Corrected the actual Model section-heading glyph size. Headings now use the identical height cap used by ordinary popup entries, while retaining the requested bold weight and existing colour.
- Retained the ordinary one-row heading height with no added spacing above or below the title text.
- Reworked Phaser Stage Sat to preserve unity small-signal gain. Saturation no longer multiplies gain through every active all-pass stage or becomes disproportionately loud above six stages.
- Reduced the Stage Sat drive curve from an extreme nine-times maximum to a controlled four-times maximum while preserving progressive nonlinear colour.
- Reduced maximum Phaser feedback and added smooth stage-count compensation. Feedback strength now falls gradually as the cascade approaches twelve stages, preventing the loud resonance jump around 5.8–6 stages.
- Updated the exact Phaser response display for the compensated feedback law and retained continuous stage interpolation, 50 ms parameter smoothing and independent state memory.
- Kept all parameter IDs, displayed ranges, defaults, Models and sequencer target numbers unchanged; state schema remains version 20.

## 0.21.1 — Comb Presence and Upper-Range Correction — 14 November 2026

- Increased the audible delayed-branch level in Feedback and Feedforward Modes without raising the recirculating loop above its existing stable 0.98 limit.
- Replaced the short-delay linear interpolator with a magnitude-preserving first-order all-pass fractional delay, restoring high-frequency Comb strength and keeping the calculated response matched to the audio path.
- Reshaped Damping so ordinary positions retain substantially more high-frequency energy; 35% now reaches roughly 10 kHz instead of rolling the delayed branch down near 4 kHz.
- Focused Comb Frequency on 20 Hz–8 kHz. The complete control travel now produces useful audible teeth instead of placing the final portion above the range where a repeated Comb pattern can exist below Nyquist.
- Updated the exact Feedback and Feedforward response calculations for the stronger audible branch while retaining the All-pass Mode's unity-magnitude linear structure.
- Kept the established 85 ms multiplicative Frequency smoothing, high-feedback sub-bass protection, Designer controls, parameter IDs, Models and sequencer target numbers unchanged.
- Kept state schema version 20 because no parameter IDs or stored-state structure changed during this pre-release correction.

## 0.21.0 — Phaser Architecture and Compact Model Headings — 14 November 2026

- Added Phaser as a sixth genuine per-filter DSP Architecture, built from twelve continuously running first-order all-pass stages rather than a relabelled preset.
- Added Frequency, bipolar Feedback and Depth controls on FILTER. Phaser hides the unrelated standard Type, Cutoff, Resonance and Slope controls while retaining shared Drive and distortion controls.
- Added six context-sensitive Phaser Designer controls per filter: continuously blended 2–12 Stages, Spread, Skew, feedback Damping, per-stage Saturation and Stereo offset.
- Added the exact linear Phaser response to the permanent display, including the continuously interpolated stage taps and bounded damped feedback loop. Nonlinear Stage Saturation, Stereo and selectable distortion remain outside the mono small-signal curve.
- Inserted Internal distortion into the Phaser feedback return with its own state-memory bank, separate from every existing Architecture and the external Pre/Post path.
- Added Clean Phaser, Classic Phaser, Deep Cascade and Wide Phaser Factory Models plus Phaser User Model save/load whitelisting.
- Added 18 F1/F2 Phaser sequencer targets for all three continuous FILTER controls and all six Designer controls, appended after the existing target numbers.
- Removed the PopupMenu framework's extra 50% section-heading height. Factory and User Model headings now use exactly the same font size and row height as ordinary entries while retaining their colour and bold style.
- Added bounded Phaser randomisation, 50 ms coefficient smoothing and direct architecture crossfading without resetting all-pass state.
- Advanced the pre-release state schema to version 20 for the 18 new automatable Phaser parameters.

## 0.20.0 — Formant Architecture — 14 November 2026

- Added Formant as a fifth genuine per-filter DSP Architecture beside Clean SVF, Ladder, OTA and Comb.
- Added continuous Vowel morphing through A, E, I, O and U plus two-octave Shift and broad/gentle-to-sharp/prominent Focus controls on FILTER. Formant hides the unrelated standard Type, Cutoff, Resonance and Slope controls.
- Built the audio path from three independently stateful topology-preserving band-pass filters. Vowel centre frequencies interpolate logarithmically and every coefficient-driving control uses 50 ms smoothing.
- Added six context-sensitive Formant Designer controls per filter: F1 Level, F2 Level, F3 Level, Width, Spread and Tilt.
- Added the exact linear three-band Formant response to the permanent display, including Vowel, Shift, Focus and every Designer setting.
- Made Internal distortion process the three Formant bands separately before their level, Tilt and summing stages, with independent state memory for each band.
- Added Natural Formant, Bright Formant, Deep Formant and Nasal Formant Factory Models plus Formant User Model save/load whitelisting.
- Added 18 F1/F2 Formant sequencer targets for all continuous FILTER and Designer controls, appended after the existing target numbers.
- Advanced the pre-release state schema to version 19 for the 18 new automatable Formant parameters.

## 0.19.1 — Comb Frequency Smoothing and Interface Refinement — 14 November 2026

- Changed Comb Frequency from 20 ms linear-Hz smoothing to 85 ms multiplicative smoothing, giving perceptually even movement and substantially smoother delay-time changes in the low-frequency range.
- Added a gentle feedback-dependent low-cut inside every live Comb delayed branch. It begins only above 70% Feedback, progressively targets frequencies below 320 Hz and blends up to 45% of a 45 Hz high-pass, restraining sub-bass buildup without removing the established Comb character.
- Included the new linear low-frequency protection in the calculated Comb response so the permanent graph remains accurate.
- Shortened the Comb FILTER captions from `FREQUENCY` and `FEEDBACK` to `FREQ` and `FDBK` without changing parameter names, IDs, ranges or automation.
- Reduced Factory/User Model dropdown section headings to the same visual text size as the menu items while retaining bold emphasis.
- Changed the FILTER tab selector LED to aqua and the DESIGNER selector LED to the Sequencer's established Bipolar red.
- Kept state schema version 18 because no parameter, range, default, model format or sequencer-target mapping changed.

## 0.19.0 — Comb Filter Architecture — 14 November 2026

- Added `Comb` as a fourth genuine per-filter DSP Architecture beside Clean SVF, Ladder and OTA, using independent fractional-delay state for both filters and channels.
- Made the FILTER view context-sensitive: Comb replaces Type, Cutoff, Resonance and Slope with Mode, Frequency, Feedback and Damping while retaining the shared Drive and distortion controls.
- Added independently running Feedback, Feedforward and All-pass Comb Modes. Mode changes use a direct 50 ms crossfade between live engines rather than coefficient interpolation or state resets.
- Added six Comb Designer controls per filter: Fine Tune, Polarity, Stereo Spread, Dispersion, Loop Saturation and Feedback Limit.
- Added the exact linear Comb response to the permanent graph, including fractional delay, Damping, Dispersion, Fine Tune, Polarity and Mode. Stereo Spread and nonlinear Loop Saturation, Feedback Limit and selectable distortion remain outside the mono small-signal curve.
- Inserted `Internal` distortion into each Comb Mode's delayed branch and allocated separate distortion-state memory for all three live Modes.
- Added Clean Comb, Metallic Comb, Hollow Comb and Wide Stereo Comb Factory Models plus Comb-specific User Model save/load whitelisting.
- Added 18 F1/F2 Comb sequencer targets for the three continuous FILTER controls and six Designer controls, appended after all existing target numbers. Mode remains a deliberate dropdown choice rather than a continuously modulated target.
- Included all Comb controls in bounded randomisation and retained natural, uncondensed control labels throughout the context-sensitive layout.
- Advanced the pre-release state schema to version 18 for the 20 new automatable Comb parameters.

## 0.18.0 — Internal Distortion Placement — 13 November 2026

- Enabled the existing per-filter `Internal` distortion Placement for all three implemented filter Architectures and removed the former unavailable label and disabled menu item.
- Inserted the selected distortion into the Clean SVF's bounded Designer Feedback return, the Ladder's stored resonance-feedback component before its protected delay-free solve, and the OTA's damped bounded resonance return.
- Kept the Ladder's arbitrary selectable distortion outside its numerical iteration, so stateful and discontinuous models cannot invalidate the derivative used by the nonlinear feedback solver.
- Added independent distortion memory banks for the external Pre/Post path and for each Architecture's Internal path, preventing stateful distortion history from leaking between unrelated signal positions or Architectures.
- Retained the existing Drive, Dist Type and Placement parameter IDs, ranges, defaults and automation mappings; no new parameter or preset-state migration is required.
- Replaced the development-status README with a user manual covering installation, signal routing, every FILTER and DESIGNER control, Architectures, Models, distortion Placement, sequencer operation, portable files and source building.
- Preserved the repository badges and added `Resources/Dusk.jpg` so the manual's interface image resolves when the source is uploaded.
- Kept state schema version 17 because this stage changes DSP routing, interface availability and documentation without adding or changing stored parameters.

## 0.17.1 — Designer Character Revoicing — 13 November 2026

- Revoiced the existing Designer DSP ranges after extreme settings proved too subtle in listening tests; no parameter IDs, displayed ranges, defaults, model format, sequencer targets or state schema changed.
- Expanded Clean SVF Resonance Shape, doubled maximum Bass Loss to 24 dB, doubled maximum Pole Spread to ±3 octaves and widened Feedback Limit from 0.25–4.0 to 0.1–8.0.
- Lowered the maximum Core Saturation threshold fivefold and replaced the former bias-normalised Sat Asym curve with bounded independent positive/negative thresholds for much clearer even-harmonic character without destabilising filter memory.
- Increased Clean SVF maximum Feedback gain from 0.95 to 0.995 while retaining the bounded return and exact response calculation.
- Expanded Ladder Stage Saturation and Feedback Saturation drive from 8x to 32x at maximum, with a shaped upper-range response that keeps the lower half controllable.
- Expanded OTA Cell Drive from 10x to 40x at maximum and greatly widened Feedback Damping's bandwidth and resonance-restraint range.
- Increased Ladder and OTA Resonance Compensation from 2.5x/2.25x to 4x maximum input gain at full Resonance.
- Kept all 0% nonlinear settings exactly transparent and retained 50 ms smoothing, continuous live states and exact low-level response-graph tracking.
- Passed 21,600 Clean SVF graph-versus-audio response checks, 5,625 Core/Sat Asym static stress cases, 180 rapid nonlinear automation cases, 2,025 bounded-feedback stress cases, and 500,000-sample Ladder and OTA automation/stability suites.
- Retained state schema version 17 because this stage changes only DSP voicing and documentation.

## 0.17.0 — OTA Architecture — 13 November 2026

- Added `OTA` as a third genuine per-filter DSP architecture beside Clean SVF and Ladder, with its own eight-cell state, feedback return and standard LPF, BPF, HPF, NOTCH and PEAK outputs.
- Made the existing continuous 6–48 dB/octave Slope select and interpolate one through eight OTA cells without changing the basic FILTER-tab workflow.
- Added context-sensitive Cell Drive, Feedback Damping and Resonance Compensation controls to each OTA Designer panel, using 50 ms smoothing and sequencer-sized knob layout.
- Implemented unity-small-signal nonlinear transconductance-cell drive, a bounded below-unity one-sample resonance return, continuously variable feedback-path damping with phase-lag compensation, and resonance-linked input compensation.
- Added exact low-level OTA response calculation to the permanently visible response graph, including continuous cell depth, feedback delay, damping and compensation.
- Kept Clean SVF, Ladder and OTA engines live and changed Architecture switching to a direct 50 ms crossfade between the actual previous and selected engines; unrelated coefficients are never interpolated.
- Added Clean OTA, Punchy OTA, Hot OTA and Dark OTA Factory Models plus OTA-specific User Model save/load whitelisting.
- Added F1/F2 OTA Cell Drive, Feedback Damping and Resonance Compensation sequencer targets, appended after every existing target so prior target numbers remain unchanged.
- Passed 4,050 analytic-versus-impulse OTA response checks, 500,000 rapid nonlinear automation samples, rail-lock and silence-recovery checks, plus an explicit high-level Cell Drive difference test. Re-ran the complete Clean SVF, saturation, bounded-feedback and Ladder validators without regression.
- Advanced the pre-release state schema to version 17 for the six new automatable OTA Designer parameters.

## 0.16.1 — Designer Spacing and Factory Model Contrast — 13 November 2026

- Moved every Clean SVF and Ladder Designer knob down by 8 px, creating clear separation beneath the new Model header while retaining the existing horizontal alignment and sequencer-sized knobs.
- Revoiced all Factory Models except the neutral Clean SVF reference with substantially wider Designer-value differences.
- Made Warm SVF and Wide Resonance use much stronger Core Saturation, Feedback, resonance shaping and related character settings so they remain more readily distinguishable without changing standard FILTER controls.
- Made Clean Ladder fully linear and uncompensated, retained Classic Ladder as the moderate reference, pushed Acid Ladder to the strongest supported internal nonlinear settings, and separated Warm Vintage between those extremes.
- Kept Type, Cutoff, Resonance, Slope, Drive, distortion settings, routing, sequencer state and the other filter untouched when recalling a Model.
- Changed no DSP equations, parameters, parameter IDs, state schema or sequencer targets; the state schema remains version 16.

## 0.16.0 — Factory / User Filter Models — 13 November 2026

- Added independent Model selectors to the Filter 1 and Filter 2 Designer headers without displacing the permanently visible response display or the existing two-row Designer layouts.
- Added four editable Clean SVF Factory Models and four editable Ladder Factory Models, grouped by their genuine DSP Architecture rather than presented as opaque filter algorithms.
- Made Model recall change only the chosen filter's Architecture and the Designer parameters supported by that Architecture. Type, Cutoff, Resonance, Slope, Drive, distortion settings, routing, sequencer state and the other filter are deliberately untouched.
- Added a `CUSTOM` state which appears whenever the recalled Architecture or any included Designer control is edited, while preserving all recalled values.
- Added per-filter `...` menus to save the current Architecture-specific design as a User Model, refresh the shared User list or open the Models folder.
- Added naturally sorted, filter-neutral `.duskmodel` files under the portable `Dusk/Models` folder. A User Model saved from either filter can be recalled into either filter.
- Used atomic User Model writes, explicit format/version validation and Architecture-specific parameter whitelists. Unknown values are ignored and standard FILTER controls cannot be injected through a Model file.
- Kept Factory Models read-only and kept User Models separate from full-plugin `.duskpreset` files.
- Added no parameters and changed no audio-processing functions, parameter IDs, sequencer targets or state schema; the state schema remains version 16.

## 0.15.1 — Context Help Identity and Layout Fix — 13 November 2026

- Fixed dropdown hover help incorrectly displaying `DUSK` when JUCE reported a nameless internal ComboBox child beneath the pointer.
- Assigned one explicit contextual-help title and description to every main label/dropdown pair, so hovering `ROUTING`, Architecture, Type, Dist Type or Placement now produces identical help over both parts of the control.
- Removed the generic `DUSK` hover fallback entirely. An unnamed child now resolves its owning control; if no valid title exists, only that hovered component's own description is shown.
- Changed hover help from a title-only first line plus description to `CONTROL | description`, allowing the sentence to use both available lines.
- Expanded the fixed help strip from 670 x 28 px to the available 803 x 32 px area beside the bottom buttons.
- Disabled horizontal help-text compression so glyph proportions remain natural and consistent.
- Applied the same two-line formatting rule to knob/value changes and all Sequencer hover help, including target depth, scrolling and randomisation controls.
- Made no DSP, parameter, preset-schema or sequencer-target changes; the state schema remains version 16.

## 0.15.0 — Ladder Feedback Saturation — 13 November 2026

- Added independent 0–100% `FB SAT` controls to both context-sensitive Ladder Designer panels.
- Placed the new nonlinearity inside each Ladder resonance return rather than after the filter, keeping it distinct from Stage Saturation and the selectable Pre/Post distortion bank.
- Used a bounded four-iteration solve for the nonlinear delay-free feedback equation, preserving stable instantaneous feedback as the control is automated.
- Preserved the exact v0.14 Ladder equation at the 0% default and kept Feedback Saturation naturally inactive while Resonance is 0%.
- Kept a unity small-signal slope so the existing calculated response remains exact at normal graph level while the control changes loud resonance and self-oscillation character.
- Added 50 ms smoothing, safe randomisation, double-click reset and F1/F2 FB Sat sequencer targets without changing any existing target number.
- Expanded the Ladder Designer's first row to three naturally spaced sequencer-sized controls: Stage Sat, FB Sat and Res Comp.
- Passed 2,700 analytic-versus-impulse response checks across both Feedback Saturation endpoints, plus 500,000 rapid nonlinear automation samples, rail-lock detection, silence recovery and an explicit audible-difference check.
- Advanced the pre-release state schema to version 16 for the two new automatable parameters.

## 0.14.0 — Nonlinear Ladder Architecture — 13 November 2026

- Added `Ladder` as the first additional genuine per-filter DSP architecture beside the unchanged Clean SVF.
- Implemented an independent nonlinear cascaded-pole engine with persistent per-stage state, unity-small-signal stage saturation and an internally solved resonance feedback path.
- Made the existing 6–48 dB/octave Slope control continuously select one through eight Ladder poles, with the classic four-pole position at 24 dB/octave.
- Derived LPF, BPF, HPF, NOTCH and PEAK from the Ladder's own continuously running tap combinations; Type remains a standard FILTER-tab choice rather than an architecture or Model.
- Added 50 ms dual-engine Architecture crossfading. Clean SVF and Ladder remain live so Architecture automation never coefficient-interpolates unrelated algorithms or recalls stale state.
- Added context-sensitive per-filter `STAGE SAT` and `RES COMP` Designer controls, with separate automatable parameters, double-click defaults, randomisation and four new sequencer targets.
- Extended the response display with the exact Ladder low-level transfer including pole interpolation, resonance feedback and Resonance Compensation. Stage Saturation remains intentionally outside the small-signal graph because it is nonlinear.
- Kept the Clean SVF response and audio-processing function bodies byte-for-byte identical to v0.13.1; its default architecture path returns the same calculated output.
- Passed 1,350 Ladder analytic-versus-impulse response checks with a worst relative error below 0.0000004, plus 500,000 rapid nonlinear automation samples, rail-lock detection and silence recovery.
- Kept selectable `Internal (N/A)` distortion disabled; architecture-specific insertion and protection remain a separate stage.
- Advanced the pre-release state schema to version 15 for the four new Ladder Designer parameters and the expanded Architecture choice list.

## 0.13.1 — Architecture Selector Layout Correction — 12 November 2026

- Removed the redundant standalone ARCHITECTURE page and its third header LED, restoring the simpler FILTER / DESIGNER workflow.
- Moved each independent Architecture selector directly onto its corresponding FILTER panel, above Type, Dist Type and Placement.
- Matched the Architecture selectors to the established 90 x 20 px Sequencer-style dropdown size and retained natural, uncondensed text rendering.
- Kept the architecture registry, automatable per-filter parameters, DSP dispatch and response dispatch introduced in 0.13.0 unchanged.
- Kept DESIGNER context-sensitive: each side continues to follow its filter's selected genuine DSP architecture and will expose only that engine's supported design controls.
- Existing `MainView=Architecture` interface settings now naturally reopen on FILTER; audio state and the version-14 parameter schema are unchanged.
- Clarified that `Internal (N/A)` is reserved for inserting the selected distortion inside a supported architecture's bounded nonlinear feedback path. It will become `Internal` only where that routing is implemented and stability-tested.

## 0.13.0 — Context-Sensitive Architecture Foundation — 12 November 2026

- Added a third vertical `ARCHITECTURE` LED choice beside FILTER and DESIGNER in the clear header area to the right of the logo, preserving the vertical layout without covering the artwork.
- Added independent, automatable Filter 1 and Filter 2 Architecture parameters. `Clean SVF` is the first registered genuine DSP structure and remains the default for both filters.
- Introduced a central architecture descriptor registry with stable IDs, display names, Designer names and concise capability summaries for future DSP engines.
- Added an ARCHITECTURE view with independent per-filter selectors and architecture summaries while keeping the calculated Filter Response permanently visible in the centre.
- Routed both audio processing and response calculation through architecture dispatch functions. Clean SVF currently resolves to the unchanged v0.12 processing and graph implementations, so this structural stage does not alter its sound.
- Made each Designer heading follow its filter's selected architecture and made Designer control visibility architecture-specific. The current eight controls per side are now explicitly Clean SVF controls rather than speculative universal controls.
- Persisted FILTER / ARCHITECTURE / DESIGNER view selection in `Settings.ini` and retained natural, uncondensed label rendering.
- Kept Architecture separate from the future Factory / User Model layer: an architecture selects real DSP, while a Model will recall an editable configuration of that DSP and its Designer.
- Advanced the pre-release Dusk state schema to version 14 for the two new per-filter Architecture parameters.

## 0.12.0 — Bounded Feedback Designer — 12 November 2026

- Added independent 0–100% Feedback controls to both Mode Designer panels, returning each filter's previous completed output through a genuine one-sample internal loop.
- Mapped Feedback to a maximum loop gain of 0.95 so it can intensify resonance and become self-sustaining at suitable settings without an unbounded direct return.
- Added independent 0–100% Feedback Limit controls with a soft threshold spanning 0.25–4.0; lower values constrain high-level oscillation sooner while retaining unity low-level slope.
- Smoothed Feedback and Feedback Limit over 50 ms to protect manual, host-automation and sequencer movement through sensitive loop settings.
- Kept both controls exactly inactive at 0% Feedback and continuously updated the return state so enabling the loop never recalls stale audio.
- Extended the calculated response display with the exact low-level closed-loop transfer, including its one-sample delay, while honestly leaving the nonlinear Feedback Limit out of the high-level graph.
- Filled each Designer panel as two rows of four sequencer-sized knobs. Used a uniform natural 9 px Designer-label font and concise `POLE SPRD` caption so labels and complete draggable `::` markers fit without horizontal scaling.
- Added F1/F2 Feedback and F1/F2 FB Limit sequencer targets and included both control pairs in bounded randomisation.
- Kept selectable Internal distortion placement reserved for a later stage that explicitly inserts those distortion models into this new bounded return.
- Advanced the pre-release Dusk state schema to version 13 for the four new automatable parameters.

## 0.11.0 — Always-Visible Response and Pole Spread — 12 November 2026

- Kept the calculated Filter Response display permanently visible in the same centre panel on both FILTER and DESIGNER views.
- Rebuilt the Designer layout as two three-column rows per filter around the response display while leaving every FILTER-tab knob and control at its established size and position.
- Reduced only Designer knobs to the sequencer Attack / Release footprint of 52 x 49 px; retained natural label proportions and complete draggable `::` target markers.
- Added independent 0–100% Pole Spread controls for Filter 1 and Filter 2, offsetting cascaded pole stages by up to ±1.5 octaves above 12 dB/octave.
- Made 0% mathematically identical to the established aligned-pole response and kept Pole Spread naturally inactive at 12 dB/octave or below.
- Extended the calculated response display with the same Pole Spread geometry used by the audio path across LPF, BPF, HPF, Notch and Peak.
- Added F1 Pole Spread and F2 Pole Spread sequencer targets and included both in safe randomisation up to 75%.
- Advanced the pre-release Dusk state schema to version 12 for the two new automatable parameters.

## 0.10.1 — Memory-Safe Saturation Fix — 12 November 2026

- Fixed the severe clicks, audio dropouts and positive/negative full-scale rail locks exposed by moving Sat Asym and Core Sat in the v0.10.0 user recording.
- Stopped Core Saturation and Saturation Asymmetry from rewriting the one-pole, SVF, continuous-slope and Bass Loss integrator memories.
- Moved nonlinear shaping to the completed instantaneous filter-core signal before the Bass Loss shelf, preserving the intended per-filter character without destabilising stored filter state.
- DC-blocked only the asymmetric difference component at 5 Hz, leaving centred symmetric saturation and the established low-level response intact.
- Increased Core Sat and Sat Asym parameter smoothing from 20 ms to 50 ms for safer manual, host-automation and sequencer movement.
- Kept every parameter ID, range, default, sequencer target and state schema unchanged at version 11.

## 0.10.0 — Saturation Asymmetry Designer — 11 November 2026

- Added independent bipolar Saturation Asymmetry controls to both Mode Designer panels.
- Biased each filter's internal Core Saturation toward either signal polarity, creating continuously variable even-harmonic character without adding another external distortion stage.
- Kept the centred 0% position perfectly symmetrical and made Saturation Asymmetry exactly inactive whenever Core Saturation is 0%.
- Removed DC offset at the nonlinear centre and normalised its local slope, preserving zero output at zero input and the established unity small-signal response.
- Smoothed Saturation Asymmetry over 20 ms, added F1 Sat Asym and F2 Sat Asym sequencer targets, and included both in safe bipolar randomisation.
- Added the fifth Designer knob to each filter's existing aligned row without moving the FILTER-tab essentials or response display.
- Advanced the pre-release Dusk state schema to version 11 for the two new automatable parameters.

## 0.9.0 — Core Saturation Designer — 11 November 2026

- Added independent 0–100% Core Saturation controls to the two Mode Designer panels.
- Implemented Core Saturation as soft nonlinear limiting inside the one-pole and SVF integrator memories, rather than as another pre/post waveshaper.
- Kept 0% mathematically transparent and used a unity small-signal slope so low-level response remains consistent with the calculated graph.
- Applied the internal saturation consistently across LPF, BPF, HPF, Notch and Peak, including active continuous-slope stages.
- Smoothed Core Saturation over 20 ms, added F1 Core Sat and F2 Core Sat sequencer targets, and included both in safe randomisation up to 70%.
- Widened the Designer caption bounds without changing or horizontally scaling the font, leaving enough room for the complete draggable `::` markers on Resonance Shape, Bass Loss, Resonance Limit and Core Sat.
- Advanced the pre-release Dusk state schema to version 10 for the two new automatable parameters.

## 0.8.0 — Standard Filter Types and Resonance Limit — 11 November 2026

- Added independent FILTER-tab Type selectors for Filter 1 and Filter 2 with genuine LPF, BPF, HPF, NOTCH and PEAK state-variable responses.
- Kept standard response Type separate from Designer architecture and character: Comb remains a future topology, while Ladder remains a future architecture / Designer preset family.
- Smoothed Type changes over 20 ms by morphing between simultaneously available SVF outputs without resetting filter state.
- Mirrored the continuous 6–48 dB slope system for HPF, added progressively cascaded normalised band stages for BPF, and added same-cutoff stage progression for Notch and Peak.
- Extended the exact response display to calculate every new response type, including routing-aware serial and parallel combinations.
- Added independent 0–100% Resonance Limit controls to both Mode Designer panels, with 100% preserving the established full resonance range.
- Applied Resonance Shape, Resonance Limit and Bass Loss consistently across all five standard response types.
- Added F1 Resonance Limit and F2 Resonance Limit as sequencer targets and included Type and Resonance Limit in safe randomisation.
- Advanced the pre-release Dusk state schema to version 9 for four new automatable parameters.

## 0.7.0 — Resonance-Linked Bass Loss — 11 November 2026

- Added independent 0–100% Bass Loss controls to the Filter 1 and Filter 2 Mode Designer panels.
- Modelled classic resonance-linked low-end thinning with a smooth one-pole shelf two octaves below each filter's Cutoff.
- Scaled Bass Loss from 0 dB to a maximum -12 dB using the shaped effective resonance, so Resonance Shape genuinely influences the model behaviour.
- Made Bass Loss naturally fade out between 12 and 6 dB/octave, reaching exactly no loss at the true non-resonant one-pole endpoint.
- Kept 0% mathematically transparent and continuously ran the shelf state so automation and sequencer movement remain click-free.
- Extended the calculated response display with the exact same shelf transfer used by the audio path.
- Added F1 Bass Loss and F2 Bass Loss as sequencer targets and included both in safe randomisation up to 70%.
- Kept Bass Loss as an ordinary editable per-filter parameter so future Factory / User Designer presets can populate it without locking the filter to a fixed model.
- Advanced the pre-release Dusk state schema to version 8 for the two new automatable Designer parameters.

## 0.6.0 — Resonance Shape Designer — 11 November 2026

- Added the first dedicated character control to both Mode Designer panels: independent bipolar Resonance Shape knobs for Filter 1 and Filter 2.
- Made negative Resonance Shape values produce a more restrained, gradual resonance build and positive values produce an earlier, more aggressive peak.
- Kept the centred 0% position mathematically identical to the established Clean SVF response and added a centre detent plus double-click reset.
- Smoothed both new controls over 20 ms, reflected them in the exact response calculation and exposed them as independent sequencer targets.
- Added Resonance Shape to safe randomisation while limiting its random range to -75% through +75%.
- Aligned each filter's Drive and Slope bottom row directly beneath its Cutoff and Resonance top row.
- Matched both Dist Type and Placement selectors to the sequencer selector size of 90 x 20 px.
- Naturally sorted the Dist Type names alphabetically while preserving the correct DSP algorithm and contextual help behind every choice.
- Advanced the pre-release Dusk state schema to version 7 for the two new automatable parameters and reordered choice list.

## 0.5.1 — Basic 6–48 dB Slope — 11 November 2026

- Moved both Slope knobs from the advanced Designer view onto the FILTER tab beside Cutoff, Resonance and Drive.
- Expanded each independent Slope range from 12–24 to a continuous 6–48 dB/octave while retaining 12 dB/octave as the default.
- Added a true one-pole 6 dB endpoint that smoothly blends into the existing resonant two-pole core at 12 dB.
- Extended the upper range with three continuously introduced non-resonant TPT stages, reaching 24, 36 and 48 dB/octave without fixed slope switching.
- Kept every filter state running continuously and retained 20 ms parameter smoothing for click-free Slope movement and sequencer animation.
- Updated the response calculation to match the complete one-pole, resonant core and multi-stage cascade exactly.
- Extended the graph floor from -60 to -96 dB so the new 36 and 48 dB/octave responses remain visibly distinct.
- Kept F1 Slope and F2 Slope available as sequencer targets and expanded safe randomisation to the full 6–48 dB range.
- Advanced the pre-release Dusk state schema to version 6 because the parameter range and normalised automation mapping changed.

## 0.5.0 — Continuous Designer Slope — 11 November 2026

- Added the first real Mode Designer control independently to Filter 1 and Filter 2: continuously variable Slope from 12.0 to 24.0 dB/octave.
- Implemented Slope by continuously lowering a second non-resonant TPT stage from the top of the usable band to the main cutoff rather than switching between fixed slope buttons.
- Kept 12.0 dB/octave as the default so the established v0.4.0 Clean SVF sound remains unchanged until Slope is raised.
- Smoothed Slope changes in the audio path and kept the additional stage running continuously to prevent state-reset clicks while automating it.
- Extended the calculated response display with the exact same two-stage blend used by the audio path.
- Added F1 Slope and F2 Slope to the sequencer target system, including per-target Depth and animated knob/graph display.
- Included both Slope controls in safe randomisation and added the two live knobs to the FILTER 1 / FILTER 2 Mode Designer areas.
- Advanced the pre-release Dusk state schema to version 5 because the two new automatable parameters change the preset state.

## 0.4.0 — Calculated Response Display — 5 November 2026

- Replaced the temporary graph message with live Filter 1, Filter 2 and combined frequency-response curves calculated from the implemented Clean SVF state update.
- Made serial response use complex multiplication and parallel response use phase-correct complex summation.
- Made the graph follow Cutoff, Resonance, per-filter Power, routing and sequencer-animated Cutoff / Resonance values.
- Added a neutral OUT curve alongside the two filter-coloured curves and expanded the graph to a useful +30 dB to -60 dB range.
- Kept the graph honest for nonlinear processing: it shows the Clean SVF small-signal response and does not pretend selectable distortion has one fixed response.
- Restored both filter controls to ordinary small round LEDs while keeping their corrected amber lit-is-on Power behaviour and title-row positions.
- Kept state schema 4 because this stage adds no parameters or preset-format changes.

## 0.3.1 — Filter Power Controls — 5 November 2026

- Replaced the confusing per-filter Bypass switches with true Power parameters: lit means processing is on, and unlit means it is off.
- Changed both per-filter Power symbols to the existing amber LED colour.
- Removed the BYPASS text and moved each compact power symbol to the top-right of its filter panel, aligned with the FILTER 1 / FILTER 2 title.
- Removed the redundant one-choice Model dropdowns and their parameter IDs. The Clean SVF remains the internal architecture foundation for the Designer.
- Kept the confirmed working Cutoff, Resonance, distortion, routing and MIX audio behaviour unchanged.
- Advanced the pre-release Dusk state schema to version 4 without compatibility code.

## 0.3.0 — First Clean SVF Architecture — 5 November 2026

- Moved the vertical FILTER / DESIGNER LED selectors into the clear header space to the right of the Dusk logo so they no longer overlap the artwork.
- Added independent topology-preserving-transform State-Variable Filter low-pass cores for Filter 1 and Filter 2.
- Added independent 20 Hz–20 kHz Cutoff, Resonance and smoothly crossfaded Bypass controls for both filters.
- Made Pre and Post distortion placement genuinely distinct around each filter core while continuing to keep unsupported Internal placement unavailable.
- Added F1/F2 Cutoff and Resonance to the existing sequencer target system, including per-target Depth and animated destination display.
- Extended safe randomisation to both Cutoff and Resonance pairs.
- Updated the response display status to identify the active Clean SVF DSP without drawing unverified curves; calculated curves remain the next stage.
- Advanced the pre-release Dusk state schema to version 3 without legacy conversion.

## 0.2.0 — Dual-Filter Structure — 5 November 2026

- Added vertical FILTER and DESIGNER LED radio choices beside the Dusk logo and persisted the selected view in `Settings.ini`.
- Added equal Filter 1 and Filter 2 control areas with a centred 206 px response-display foundation; no decorative response curves are shown before real filter DSP exists.
- Added simultaneous Filter 1 and Filter 2 Mode Designer areas. Their controls remain intentionally empty until each architecture supplies meaningful parameters.
- Replaced the single global distortion ownership with independent Filter 1 and Filter 2 Distortion Type, Drive and Placement parameters and separate stateful distortion processing.
- Added Filter 1 only, Filter 2 only, serial Filter 1 into Filter 2, and unity-preserving parallel routing.
- Marked Internal distortion unavailable until a filter architecture provides a genuine nonlinear feedback path; Pre and Post remain structurally separate but sound identical while the filter cores are transparent.
- Replaced the sequencer's single Dist destination with independent F1 Drive and F2 Drive targets while retaining all target Depth behaviour.
- Advanced the clean Dusk state schema to version 2; no legacy conversion was added during this pre-release design phase.
- Kept MIX, OUT, Power, Mix Lock, presets, settings, randomisation, contextual help and the complete hide/show sequencer and Gate systems.

## 0.1.2 — Clean Compiler Output — 5 November 2026

- Renamed the local editor constrainer pointer so it no longer hides JUCE's inherited `AudioProcessorEditor::constrainer` member.
- Removed the resulting MSVC C4458 compiler warning without changing interface layout or behaviour.

## 0.1.1 — Original Interface Geometry Restored — 4 November 2026

- Restored the exact original upper-interface surround, colour strip and all nine control-panel rectangles.
- Restored the original header, preset navigation, Theme, Power, information-row and action-button positions.
- Restored the retained DIST, MIX and OUT knobs to their original 60×57 sizing and exact Afterglow positions.
- Left every removed delay-control position clean and empty for the future Dusk filter design instead of replacing the layout with newly designed filter panels.
- Restored the preset browser's original panel geometry and browser MIX control.
- Removed the additional outer border that had been added around the sequencer; the original sequencer's own styling remains unchanged.
- Kept the Dusk logo and all 0.1.0 processor, identity, preset and sequencer separation work unchanged.

## 0.1.0 — Independent Filter Template — 4 November 2026

### New project identity

- Created Dusk as a separate VST3 project with product code `dsk1`, bundle ID `com.sl23.dusk` and `Fx Filter` category.
- Started a new `DuskParameters` schema at version 1.
- Added independent `.duskpreset` and `.duskseq` formats.
- Moved portable settings, presets and user sequences to the separate `Dusk` data tree.

### Clean processor foundation

- Replaced the former effect engine with a transparent mono/stereo signal path.
- Removed all echo buffers, timing, repeat routing, freeze capture, multi-tap processing, delay modes, conversion and migration code.
- Retained the complete 17-model distortion bank, with 0% DIST exactly transparent.
- Retained smoothed MIX, OUT and bypass processing.
- Reserved distinct Filter 1 Type, Filter 2 Type and Serial / Parallel routing parameters without inventing filter DSP before the design is settled.
- Reserved pre-filter / post-filter distortion placement for the later filter stage.

### Interface and workflow retained

- Retained the scalable panel framework, logo resource loading, version label, Theme, fixed colour bar and contextual help.
- Reserved independent Filter 1 and Filter 2 parameters and separate type-colour hooks without replacing the original panel geometry.
- Retained preset folders, favourites, copy/paste, saving, Save As, quick selection and previous/next navigation.
- Retained portable interface settings, Power, MIX, OUT and Mix Lock.
- Updated randomisation to affect only the currently meaningful distortion controls and a safe OUT trim.

### Sequencer retained

- Retained the complete 32-step hide/show sequencer, waveform tools, saved user sequences, Host Sync, Played trigger, direction modes, polarity modes, Start / End, Attack / Release, Depth / Offset and animation.
- Retained all 16 target slots, enable state and individual target Depth.
- Rebuilt the target list around the new Dusk parameters only.
- Retained the complete per-step Gate editor and changed its routes to Input, Effect, Output and Off so no delay-feedback concept remains.
