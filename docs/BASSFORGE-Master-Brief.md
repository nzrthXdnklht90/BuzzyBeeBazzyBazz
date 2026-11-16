# BASSFORGE
## Master Product Design & Engineering Development Brief

**Project:** BASSFORGE — Hybrid Multiband Bass Processing & Mixing Plugin  
**Document version:** 1.0 — November 2026  
**Project status:** Product concept approved; engineering architecture and implementation pending  
**Primary platform:** VST3 for Windows and macOS  
**Working framework:** C++ / JUCE  
**Working product name:** BASSFORGE (subject to trademark clearance)

---

# 1. Project Vision

BASSFORGE is an ambitious bass guitar processing plugin designed to combine professional bass amplification, multiband distortion, parallel DI processing, virtual cabinet recording, room acoustics and studio-grade mixing into a single, intuitive instrument.

The goal is not to create another generic bass distortion plugin.

We want to develop a comprehensive digital bass production system that gives musicians the sonic flexibility of a sophisticated studio bass rig without requiring complicated DAW routing, multiple plugin instances or extensive knowledge of professional audio engineering.

The plugin should provide the immediacy of a conventional bass preamp, the tonal flexibility of Neural DSP Parallax X, and the intuitive mixing workflow of a professional recording console.

### Guiding philosophy

**The plugin should be simple enough for a beginner to achieve an excellent bass tone within minutes, but sophisticated enough for an experienced engineer to use on professional recordings.**

Three principles should guide development:

1. **Exceptional sound quality.** The modelling, dynamics and signal integration must take precedence over unnecessary features or visual complexity.
2. **Simple operation, sophisticated processing.** Users should not need to understand the technical processes occurring beneath the interface.
3. **Creative freedom.** The engineer is encouraged to challenge our proposed implementations, introduce alternative algorithms and pursue improvements that offer demonstrably better results.

The specifications in this document describe our desired product and user experience. They are not intended to restrict the developer to inferior technical methods simply because those methods were initially proposed.

---

# 2. Product Identity & Intended Users

BASSFORGE is intended for:

- Bass guitarists who want excellent recorded or live tones without complicated equipment.
- Producers working with rock, metal, punk, hardcore, pop, funk, alternative and other bass-driven genres.
- Mixing engineers who traditionally use multiple bass tracks for clean DI, distortion and cabinet ambience.
- Musicians using four-, five- or six-string bass guitars, including down-tuned instruments.
- Home recording musicians who may have limited equipment or acoustically untreated rooms.

The plugin must be equally capable of producing warm classic bass sounds, mildly saturated tones and aggressive modern distortion.

**Fuzz is deliberately excluded from the initial distortion design.** The desired sound palette focuses on convincing amplifier breakup, overdrive, saturation, grit and high-gain distortion.

### Primary sonic objectives

- Strong, stable bass fundamentals.
- Good articulation and pitch definition.
- Convincing analogue-inspired saturation.
- Aggressive harmonic content without uncontrolled fizz.
- Transparent or characterful dynamics when appropriate.
- Seamless integration of clean and processed signals.
- Musically natural cabinet and microphone behaviour.
- Reliable results across different bass guitars and playing styles.

---

# 3. Approved Product Architecture

The plugin will have **two main sound-design pages and one persistent, collapsible mixer on the right**.

## Page 1 — TONE

Contains two vertically stacked processing sections:

**Upper section: DRIVE**

A multiband distortion and amplifier/preamp modelling engine with three selectable tonal characters.

**Lower section: DI**

An independent clean bass channel equipped with 1176-inspired FET compression and three-band EQ.

## Page 2 — CAB & SPACE

Contains three main processing sections:

1. Virtual cabinet and dual microphone modelling.
2. Seven-band parametric EQ for the processed Drive signal.
3. Parallel studio room/reverb simulation.

## Persistent Right-Side Mixer

The mixer remains available from either page, with the ability to expand or collapse.

It contains:

- DRIVE channel fader
- DI channel fader
- ROOM return fader
- MIX EQ processing
- LIMITER processing
- MASTER output fader

The mixer is the central point where the independent signals combine into one finished bass sound.

### Locked UX requirements

- Two main pages only.
- Mixer remains visible or quickly accessible from both pages.
- Mixer can be minimised into a narrow metering strip.
- Drive appears above DI on Page 1.
- Individual audio-processing blocks have on/off switches.
- The mixer itself and its faders do not need bypass switches.
- The original Clean/Wet Mix knob is removed and replaced by independent channel faders.
- The interface must not become a crowded virtual rack.

---

# 4. Full Audio Signal Architecture

The intended routing is:

**Instrument input → Input conditioning → Two parallel processing paths**

### Path A — Drive

Input → Multiband Drive Engine → Virtual Cabinet → Seven-Band Parametric EQ → Drive Fader

The low-frequency component of the multiband engine should normally bypass speaker/cabinet coloration, while the relevant distorted components pass through the cabinet.

This maintains the clean and stable low-frequency foundation associated with Parallax-style multiband processing.

### Path B — DI

Input → FET Compressor → Three-Band EQ → DI Fader

This path must be independently processed and must not pass through amplifier distortion, cabinet simulation or the room processor.

### Path C — Room Return

Signal derived from the processed Drive path → Low-Frequency Filtering → Virtual Room/Reverb Engine → Room Fader

The room return is a **100% wet effect return** rather than another full-range bass signal.

The default room send should follow the Drive channel's fader level. This prevents an unnaturally loud room signal when the primary Drive channel is reduced.

### Master Signal Path

Drive + DI + Room → Summing Bus → Mix EQ → Optical Levelling → Peak Protection → Master Fader → Final Output

The Master fader should operate as an attenuation control, with 0 dB as its maximum gain, unless a different architecture can guarantee safe output peaks.

The final architecture must support mono bass fundamentals while allowing controlled stereo cabinet ambience and room processing.

### Important routing principle

The room return should remain a separate mixer channel. Adjusting its fader must change the amount of room sound without changing the Drive channel's direct signal.

Likewise, changing the Drive or DI faders should not modify the internal distortion or compression settings.

---

# 5. DRIVE ENGINE — Design Requirements

## 5.1 Multiband Foundation

The Drive engine takes inspiration from the three-band architecture of Neural DSP Parallax X.

The main concept is to separate bass frequencies into three independently processed regions:

| Band | Processing objective |
|---|---|
| LOW | Compression, fundamental weight, transient stability |
| MID | Primary distortion character, growl and harmonic definition |
| HIGH | Articulation, presence, pick attack and higher-frequency distortion |

The bands are recombined after processing with appropriate phase and latency compensation.

### Reference specifications

The supplied Parallax X v1.0.0 manual documents:

- Adjustable low-pass filter, 70–400 Hz.
- Low compressor with 4:1 ratio.
- Fixed low-band attack of 3 ms.
- Fixed low-band release of 600 ms.
- Mid distortion stage with independently adjustable drive.
- High distortion stage with adjustable high-pass and drive.
- Independent level controls for each band.

These are useful engineering references, **not mandatory values for our final plugin**.

The engineer should determine whether alternative compressor behaviour, crossover frequencies or distortion topologies improve the final result.

## 5.2 Crossover Design

A proposed starting point is two cascaded fourth-order Linkwitz–Riley crossovers.

Suggested initial ranges:

| Parameter | Proposed range | Starting point |
|---|---|---|
| Low/Mid crossover | 70–400 Hz | 140 Hz |
| Mid/High crossover | 300–2,000 Hz | 850 Hz |
| Filter slope | 24 dB/octave | LR4 |

These are development proposals, not locked final specifications.

The engineer must verify flat-magnitude reconstruction when all nonlinear and dynamics stages are bypassed.

A cascaded three-way crossover requires more consideration than two independent frequency splits. The additional phase response introduced by the second crossover should be compensated in the low branch.

The implementation must also account for:

- Oversampling latency.
- NAM inference and resampling latency.
- Cabinet and microphone processing delay.
- Dynamics processing.
- Parallel DI summing.
- User-adjustable crossover positions.

**The ultimate objective is not just a flat multiband sum, but a clean and stable combination of the processed Drive and separate DI channels.**

Exact phase identity after nonlinear processing cannot be guaranteed. The engineer should aim for correct baseline reconstruction, deterministic latency and a musically reliable low-frequency blend.

## 5.3 Low-Frequency Ownership

A specific design challenge is that both the Drive path's low band and the independent DI path can contain the same bass fundamentals.

This can create excessive low-frequency energy or inconsistent dynamics.

Please investigate a low-frequency management system with the following possible modes:

- **DI Priority:** DI supplies most of the fundamental, with the Drive low band reduced.
- **Hybrid:** Both low-frequency paths contribute.
- **Full Parallel:** Both paths remain fully active.

The first mode should be evaluated as the default for aggressive modern bass sounds.

The engineer may implement this with frequency-selective blending, gain compensation, phase-aware routing or another superior technique.

The feature should remain automatic or hidden from beginners unless advanced adjustment is necessary.

---

# 6. Three Selectable Drive Characters

The product will provide three clearly named amplifier/preamp characters.

| UI name | Sonic reference | Intended character |
|---|---|---|
| **VINTAGE** | Ampeg SVT-inspired | Warm, muscular, punchy tube breakup |
| **DRIVE** | Tech 21 SansAmp Bass Driver DI V2-inspired | Versatile tube-like overdrive and saturation |
| **MODERN** | Darkglass Microtubes B7K-inspired | Tight, aggressive, articulate distortion |

These are tonal inspirations, not claims of officially licensed or component-exact emulations.

## 6.1 VINTAGE

Reference: Ampeg SVT-style valve amplifier.

Desired characteristics:

- Full, harmonically rich low mids.
- Gradual transition from clean to tube breakup.
- Strong dynamic response to picking intensity.
- Natural compression as gain increases.
- Warm upper-frequency behaviour.
- Suitable for classic rock, punk, alternative and heavy bass.

Research candidates:

- Cascaded nonlinear gain stages.
- Asymmetric tube-like transfer characteristics.
- Interstage frequency shaping.
- Dynamic bias or sag.
- Optional power-stage inspired nonlinear behaviour.
- Tone-stack interactions.

The engineer should avoid a simplistic static soft clipper if it fails to reproduce the expected dynamic character.

## 6.2 DRIVE

Reference: Tech 21 SansAmp Bass Driver DI V2-style preamp.

Desired characteristics:

- Warm but articulate drive.
- Broad range between subtle saturation and aggressive overdrive.
- Strong tonal flexibility.
- Musical high-frequency presence.
- Good response across fingerstyle and pick playing.

The reference hardware has an integrated speaker-emulation character, along with frequency-selectable bass and mid controls.

**Important:** If we use captures of the original hardware, its built-in speaker-like coloration may already be included.

The engineer should carefully prevent undesirable double-cabinet filtering when combining this model with our separate virtual cabinet.

A custom amp/preamp-only DSP approximation may be preferable where capture-based modelling cannot cleanly separate these behaviours.

## 6.3 MODERN

Reference: Darkglass Microtubes B7K-style distortion.

Desired characteristics:

- Tight and controlled low end.
- Strong upper-mid harmonic detail.
- High articulation during fast playing.
- Aggressive but controlled distortion.
- Effective response with down-tuned bass guitars.
- No unnecessary fuzz-like instability.

The original B7K architecture uses frequency-dependent control before distortion, including its Attack and Grunt behaviours.

Our model should investigate similar pre-distortion frequency shaping rather than relying on post-distortion EQ alone.

## 6.4 Drive Controls

The primary interface should expose:

- **Model:** Vintage / Drive / Modern
- **Drive:** Primary saturation intensity
- **Character:** Model-dependent tonal character
- **Output:** Model output trim

An expanded panel can expose:

- Low/Mid crossover.
- Mid/High crossover.
- Mid Drive.
- High Drive.
- Low Compression.
- Independent band levels.
- Optional model-specific voicing parameters.

Do not expose all advanced controls on the default page.

The engineer should develop musically useful macro mappings rather than making one knob simply control several unrelated parameters linearly.

---

# 7. NAM Integration and Nonlinear Modelling Strategy

We want the plugin architecture to support embedded **Neural Amp Modeler (NAM)** inference.

However, NAM should be treated as an engineering tool, not a mandatory substitute for all custom DSP.

### Recommended hybrid strategy

1. Develop a reliable multiband DSP foundation.
2. Integrate NeuralAmpModelerCore as a modular nonlinear-model engine.
3. Experiment with appropriate reference captures of the three amplifier/preamp characters.
4. Compare captured-model results against original circuit-inspired DSP.
5. Use the approach—or combination of approaches—that best meets sound quality, performance and controllability targets.

The engineer is encouraged to explore:

- Grey-box modelling.
- White-box circuit modelling.
- Wave Digital Filters.
- Nonlinear state-space methods.
- Antiderivative antialiasing.
- Neural-network profiling.
- Hybrid neural and conventional DSP.

### Critical capture limitations

A NAM model typically represents the device at its captured settings.

A single capture does not automatically reproduce all control interactions of the source equipment.

We must not imply that a Drive knob offers accurate continuous hardware behaviour merely because it changes gain entering a static NAM capture.

Potential solutions include:

- Parameterised custom nonlinear DSP.
- Multiple carefully matched captures.
- Explicitly trained parameter-conditioned models.
- Hybrid capture and circuit-inspired processing.

The engineer should evaluate transitions between different model states to prevent phase, dynamics or loudness discontinuities.

Full-range reference captures should normally receive appropriate full-range input. Feeding an arbitrary preamp capture only the mid or high band is not guaranteed to preserve its original behaviour.

NAM inference should therefore have a clearly defined position relative to our multiband system.

### Model ownership

Any NAM model shipped with the plugin must have appropriate distribution rights.

The project must not depend on unlicensed third-party captures.

A runtime loader may be developed for embedded models. User-importable NAM support is optional for the first public version, subject to technical and product decisions.

---

# 8. CLEAN DI ENGINE

The DI channel exists to supply clarity, fundamental weight and dynamic consistency independently of distortion.

It consists of:

**Direct Input → FET Compressor → Three-Band EQ → DI Channel Output**

## 8.1 FET Compressor

The compressor should be inspired by the classic 1176 design, but simplified specifically for bass guitar.

We do not require an exact replica of the 1176 interface.

### Primary user controls

- Compression
- Output

### Proposed internal starting behaviour

- Fixed ratio approximately 4:1.
- Medium-fast FET-style attack.
- Musically responsive release.
- Linked threshold/input drive and makeup compensation.
- Clear gain-reduction metering.

The real 1176 family has extremely fast attack and release capabilities. The final values must be chosen by listening tests rather than arbitrary imitation of a typical hardware knob position.

The compressor must retain bass transients while preventing individual notes from becoming excessively uneven.

Avoid aggressive compression artefacts on sustained low notes.

The engineer may implement sidechain filtering, adaptive release or nonlinear amplifier coloration where these provide a benefit.

## 8.2 Three-Band DI Equalizer

The clean DI should have:

- Bass
- Mid
- Treble

Suggested implementation:

| Band | Initial design |
|---|---|
| Bass | Adjustable low shelf |
| Mid | Broad parametric or semi-parametric bell |
| Treble | Adjustable high shelf |

The default interface shows only three EQ knobs.

Detailed frequency and Q adjustments can be hidden within Advanced controls.

The clean EQ should remain neutral at its default settings.

## 8.3 DI Monitoring

The DI path must support:

- Independent bypass.
- Mute and solo from the mixer.
- Level metering.
- Automatic latency alignment.
- Clean summing with the Drive channel.

A DI bypass should be clearly distinguished from muting the DI channel. Bypassing a compressor or EQ preserves the signal, whereas muting a mixer channel removes its contribution.

---

# 9. CABINET MODELLING ENGINE

This is one of BASSFORGE's most important features.

We do not want a conventional user-facing IR browser with hundreds of cabinet files.

Instead, the goal is a purpose-built **virtual cabinet, speaker and microphone modelling system**.

The proposed modelling approach is procedural or hybrid grey-box modelling rather than simple fixed recorded-IR selection.

## 9.1 Fixed Cabinet Selection

The plugin will include exactly four primary cabinet choices:

1. **Hartke 4×10**
2. **Ampeg 8×10**
3. **Mesa/Boogie 2×15**
4. **Darkglass 4×10**

Specific reference models and revisions must be confirmed during measurement and calibration.

The initial development may use approximated models, but these should be labelled as provisional until their responses are properly evaluated.

Do not assume that a generic four-by-ten response accurately reproduces a specific cabinet brand.

## 9.2 Proposed Cabinet DSP Architecture

A cabinet model may comprise:

**Speaker Driver → Enclosure Response → Directional Radiation → Microphone Response → Acoustic Delay**

Potential components:

- Driver resonance.
- Cabinet low-frequency response.
- Sealed or ported enclosure behaviour.
- Speaker breakup and upper-frequency rolloff.
- Cabinet resonance and diffraction effects.
- Position-dependent frequency response.
- Microphone frequency coloration.
- Proximity effect.
- Distance-dependent amplitude and delay.
- Phase response.

The engineer is encouraged to begin with a manageable filter-based implementation, then progressively improve it using actual reference measurements.

A full physical loudspeaker simulation is not compulsory if a significantly more efficient model produces better perceptual results.

Internally generated FIR filters, parametric filters or synthetic impulse responses are acceptable computational techniques. The restriction concerns reliance on an uncurated library of recorded commercial cabinet IRs, not the mathematical use of convolution.

## 9.3 Accuracy Requirements

Reference measurements should ideally be obtained for each cabinet.

Measurements may include:

- On-axis frequency response.
- Off-axis response.
- Close and distant microphone positions.
- Phase and impulse response.
- Speaker impedance/resonance data where available.
- Relevant enclosure specifications.

Calibration should focus on perceptually important characteristics rather than arbitrarily fitting an EQ curve.

**A convincing generic cabinet model is preferable to an inaccurate model falsely marketed as an exact recreation.**

The engineer must document which aspects are physically modelled, empirically fitted or approximated.

---

# 10. DUAL MICROPHONE SYSTEM

The selected cabinet should support two independently configurable virtual microphone channels.

The default implementation uses **two microphones on one selected cabinet**, not necessarily two different cabinet models at once.

## 10.1 Microphone Types

Each microphone channel must support:

- Dynamic
- Condenser
- Ribbon

The engineer may implement multiple microphone voicings within each type if useful.

The first release should focus on one excellent, distinctive model per microphone category before expanding the library.

## 10.2 Microphone Positioning

We want a simplified grid system inspired by modern digital modelers.

A proposed matrix is:

**Four radial positions:**

- Cap
- Cap Edge
- Cone
- Cone Edge

**Eight distances:**

- 0 in
- 0.5 in
- 1 in
- 2 in
- 3 in
- 4 in
- 5 in
- 6 in

This creates a 32-position selection grid.

The engineer may implement continuous interpolation or continuous acoustic modelling beneath these positions if a smooth model offers better behaviour.

## 10.3 Grid Interface

The cabinet should appear as a clean, attractive visual element on Page 2.

When the user hovers over the cabinet illustration, a placement grid appears as an overlay/popover.

The overlay contains:

- Mic 1 / Mic 2 selection.
- Currently selected microphone type.
- Placement grid.
- Selected position indicator.
- Distance information.
- Optional axis selection.
- Optional phase/polarity control.

Clicking the cabinet should pin the panel open so that users can make precise adjustments without relying solely on mouse hover.

The system must also work through keyboard focus and clicking, not just hover.

The grid should feel like interacting with a virtual recording cabinet, not configuring an engineering spreadsheet.

## 10.4 Two-Microphone Summing

The dual microphone section should include:

- Individual microphone enable/bypass.
- Relative microphone levels.
- Mic 1 / Mic 2 balance.
- Polarity inversion.
- Automatic time alignment, where appropriate.
- Optional natural phase mode.

Natural Phase should preserve relevant acoustic path-length differences.

Auto Align should compensate for significant timing differences when both microphones are combined.

The engineer must account for the fact that phase relationships can influence low-mid character as well as high-frequency comb filtering.

---

# 11. SEVEN-BAND PARAMETRIC EQ

The second page includes a seven-band parametric equalizer dedicated to the Drive channel.

It should sit after cabinet processing.

The EQ must not affect the separate clean DI.

## Requirements

Seven independently controllable bands with:

- Frequency
- Gain
- Q/bandwidth
- Individual band enable
- Draggable graphical control points
- Live or optionally enabled spectrum analysis

Proposed initial flexibility:

- Frequencies across the useful bass/audio spectrum.
- Gain ranges approximately ±18 dB.
- Broad and narrow Q values.
- Optional shelving modes on the outer bands.

Actual limits should be chosen according to filter stability and musical relevance.

The frequency response should be displayed on a clean, minimalist graph.

Clicking an EQ point should reveal its controls. The interface should not display 21 knobs at once.

### Purpose

This EQ is intended for:

- Eliminating harsh cabinet resonances.
- Reducing unnecessary low-frequency buildup.
- Enhancing pick and finger articulation.
- Managing the 200–500 Hz low-mid region.
- Balancing distortion against the clean DI.
- Controlling aggressive upper-midrange frequencies.

The EQ must be genuinely neutral when bypassed.

---

# 12. ROOM / REVERB ENGINE

We want a studio ambience system, not a conventional huge atmospheric reverb.

The reverb should simulate the sense of hearing a bass cabinet recorded in a physical space.

It should function as an independent stereo effect return.

## 12.1 Three Initial Room Types

- **Tight Studio:** Short, controlled reflections, minimal decay.
- **Live Room:** Moderate early reflections and natural room presence.
- **Large Studio:** Larger perceived dimensions and longer ambient decay.

These room types are creative targets; the engineer should develop the final acoustic parameters.

## 12.2 Proposed Implementation

Investigate an algorithmic room model incorporating:

- Early reflections.
- Diffuse late reflections.
- Room-size control.
- Decay shaping.
- Frequency-dependent damping.
- Stereo decorrelation.
- Controlled pre-delay.
- Low-frequency filtering.

A feedback delay network, suitable early-reflection generator or another high-quality room algorithm may be used.

The engineer is free to use a carefully licensed convolution-based approach if it substantially improves the acoustic realism, provided the implementation remains computationally efficient and legally distributable.

## 12.3 Primary Controls

Keep the UI simple:

- Room Type
- Size
- Character

The ROOM mixer fader controls overall ambience level.

Do not add a second redundant Reverb Mix knob to the main reverb block.

## 12.4 Default Room Routing

The room send should originate from the processed Drive path after cabinet and seven-band EQ.

The default send follows Drive channel level.

The room return must be fully wet and should have a low-frequency high-pass filter to avoid excessive bass buildup.

A starting high-pass region around 150–200 Hz is worth testing, but the engineer should optimise this value for different room models.

A pre-fader send mode may be offered as an advanced feature, but is not essential for the first release.

---

# 13. PERSISTENT MIXER

The right-side mixer is a defining feature of the product.

It must remain visible or instantly accessible on both main pages.

## 13.1 Required Channels

### DRIVE

Controls the level of the completed Drive/Cab/EQ path.

### DI

Controls the level of the independently processed clean bass.

### ROOM

Controls the return level of the studio ambience.

### MASTER

Controls the final combined output after master-bus processing.

The Master should be visually separated from the three source channels.

## 13.2 Channel Controls

Each source channel should include:

- Vertical fader
- Level readout in dB
- Peak meter
- Mute
- Solo

The mixer does not need individual on/off buttons.

The MASTER section should include an output meter and a clear clipping indicator.

Optional pan controls should be considered cautiously. The low-frequency foundation should remain centred by default.

## 13.3 Collapsible Interface

Expanded mode:

- Approximately 380–440 px wide as a starting layout target.
- Displays all three source faders and the Master.
- Shows Mix EQ and Limiter access.
- Allows comfortable operation during recording and mixing.

Collapsed mode:

- Narrow right-side strip.
- Retains the main output meter.
- Displays a clear expand button.
- Does not obscure the main page.

The expanded panel must resize or reflow intelligently rather than simply covering essential controls.

All faders must support automation, precise numeric input and reset-to-default behaviour.

## 13.4 Mixer Operation

The faders must perform real level control, not serve as a decorative alternative to a blend knob.

The user must be able to:

- Blend DI and distortion freely.
- Hear only the DI.
- Hear only the Drive.
- Hear Drive plus Room without DI.
- Add Room to the full mix.
- Mute and solo channels.
- Adjust Master output independently.

Solo behaviour should be clearly defined and should not permanently change preset settings or bypass states.

---

# 14. MIX EQ — MASTER TONE SHAPING

The Mix EQ replaces the previously discussed EQP-1A-branded block.

The internal behaviour should be inspired by the musical curves of a Pultec EQP-1A-type programme equalizer.

It processes the **combined Drive + DI + Room bus**, not the individual channels.

## Desired Features

- Broad low-frequency boost.
- Broad low-frequency attenuation.
- Musical upper-frequency boost.
- Upper-frequency attenuation.
- Frequency selection.
- Nonlinear interaction between simultaneous low boost and attenuation, where useful.

The distinctive low-frequency boost-and-attenuate interaction should be preserved rather than reduced to an ordinary bass shelf.

### UI approach

The default interface should prioritise a few musical macro controls.

An expanded Mix EQ view may expose:

- Low Boost
- Low Atten
- Low Frequency
- High Boost
- High Atten
- High Frequency
- Optional bandwidth control

The engineer should evaluate whether the original-inspired structure or a simplified macro interface produces the best user experience.

Both should be driven by the same underlying DSP.

### Role

Mix EQ is for final tonal balancing, not corrective cabinet EQ.

The seven-band EQ on Page 2 remains the dedicated surgical processing stage.

---

# 15. LIMITER — MASTER DYNAMICS

The Limiter block replaces the previously discussed LA-2A-branded block.

It should be based primarily on the musical behaviour of an LA-2A-style optical levelling amplifier.

However, we also require dependable protection against unwanted digital clipping.

These are two different engineering tasks.

## 15.1 Optical Levelling

Desired characteristics:

- Smooth gain reduction.
- Gentle dynamic control.
- Program-dependent release.
- Natural handling of sustained bass notes.
- Optional subtle harmonic coloration.

The classic LA-2A is known for a relatively slow attack and two-stage, program-dependent release.

The engineer should model these dynamic characteristics rather than implementing an ordinary compressor with fixed attack and release and simply labelling it optical.

### Primary controls

- Amount
- Output / Gain
- Gain-reduction meter

More detailed options may be hidden from beginners.

## 15.2 Peak Protection

An optical compressor is not inherently a reliable brickwall limiter.

Therefore, a true-peak protection stage should be investigated after optical levelling.

Possible features:

- User-selectable protection.
- Factory ceiling around −1 dBTP.
- Oversampled or sufficiently accurate inter-sample peak detection.
- Transparent gain reduction.
- Clearly communicated latency when lookahead is enabled.

The engineer should evaluate the most sensible placement of this protection relative to the Master attenuator.

The interface should not claim true-peak protection unless measurements verify it.

---

# 16. USER INTERFACE & VISUAL DESIGN

The previously approved minimalist BASSFORGE renders should serve as visual references.

The design direction is inspired by the restraint and clarity of Klevgrand and modern Arturia interfaces.

We do not want to reproduce a large photographic hardware rack.

## 16.1 Visual Language

Recommended palette:

| Element | Style |
|---|---|
| Background | Warm off-white / cream |
| Primary typography | Dark graphite |
| Drive accent | Soft orange |
| DI accent | Muted blue |
| Room accent | Muted green |
| Neutral controls | Soft grey |
| Graphs and meters | Restrained accent colours |

Use:

- Clean typography.
- Generous whitespace.
- Rounded modules.
- Subtle shadows.
- Flat or softly dimensional controls.
- Limited visual texture.
- Consistent knob geometry.
- Clear grouping and spacing.

Avoid:

- Excessive skeuomorphism.
- Decorative screws and bolts.
- Oversized hardware photographs.
- Too many simultaneous graphs.
- Constant animation.
- Heavy neon or aggressive gaming-style aesthetics.
- Too many visible parameters.

## 16.2 Interaction Philosophy

Default mode should expose a small number of useful controls.

Advanced controls should appear through:

- Expandable modules.
- Focused block inspectors.
- Hover/click overlays.
- Contextual parameter panels.

Changing pages must never interrupt audio.

The mixer should not be recreated separately on each page. It should be one persistent UI component sharing one set of DSP parameters.

## 16.3 Block Bypass

Every audio-processing block should have a clear power button, including:

- Drive
- DI processor path
- Cabinet
- Seven-Band EQ
- Room
- Mix EQ
- Limiter

The mixer itself does not need a power button.

Define bypass behaviour carefully. In particular, disabling a complete source path must not inadvertently introduce an additional unprocessed bass feed into the final sum.

All bypass and model transitions must be click-free.

## 16.4 Resizing and Accessibility

The interface should support:

- Smooth resizing.
- High-DPI monitors.
- Mouse and keyboard operation.
- Clear parameter values and units.
- Fine adjustment modifiers.
- Double-click reset.
- Keyboard focus indicators.
- Accessible contrast.
- Predictable automation behaviour.

The engineer should maintain design consistency across both main pages and every block inspector.

---

# 17. PERFORMANCE & REAL-TIME REQUIREMENTS

The primary targets are modern macOS and Windows DAWs, including Cubase Pro 14.

## Supported Formats

Initial:

- VST3, Windows 64-bit.
- VST3, macOS Intel and Apple Silicon where feasible.

Subsequent:

- Audio Unit.
- Standalone application, if desirable.

## Sample Rates

Test at minimum:

- 44.1 kHz
- 48 kHz
- 96 kHz

Other common rates may be added after validation.

## Real-Time Audio Requirements

The engine must:

- Avoid memory allocation in the audio callback.
- Avoid file loading and blocking operations on the audio thread.
- Handle variable host buffer sizes.
- Provide click-free parameter smoothing.
- Maintain deterministic processing latency.
- Properly report latency to the DAW.
- Report reverb tail behaviour.
- Handle host playback start/stop and offline rendering.
- Support reliable preset recall.
- Avoid CPU spikes during cabinet or amp model changes.

Oversampling should be used where beneficial, especially around nonlinear distortion.

However, **higher oversampling factors must not be treated as automatically superior**. Compare alias rejection, phase behaviour, latency and CPU usage.

A low-latency monitoring mode and higher-quality offline mode may be useful, provided switching modes does not create unpredictable host behaviour.

---

# 18. PHASE, LATENCY & SIGNAL-INTEGRITY REQUIREMENTS

This is among the highest-priority technical areas.

Our architecture contains multiple related signal paths, meaning timing and phase mistakes can significantly reduce bass quality.

## Key Risks

- Multiband crossover phase interaction.
- Unequal oversampling latency.
- Nonlinear processing affecting phase and harmonic structure.
- NAM model latency and sample-rate conversion.
- Virtual cabinet group delay.
- Dual-microphone timing differences.
- Different compressor envelope responses.
- Clean DI and processed Drive interference.
- Stereo room processing affecting mono compatibility.

## Required Approach

1. Establish correct baseline multiband reconstruction.
2. Compensate deterministic processing latency across branches.
3. Align the DI path to the appropriate reference.
4. Calibrate virtual microphone acoustic delay.
5. Provide optional polarity and phase-alignment controls.
6. Verify low-frequency summing using real bass material.
7. Test mono compatibility and stereo output behaviour.

A perfectly flat bypassed filter sum is necessary but not sufficient to guarantee perfect alignment once nonlinear processing and cabinet modelling are enabled.

The engineer should prioritise a stable, musical result rather than promising impossible universal phase cancellation.

### Important test signals

- Sine sweeps.
- Impulses.
- Stepped amplitude signals.
- Multitone tests.
- Fingerstyle bass.
- Picked bass.
- Slap bass.
- Distorted bass.
- Low-B and Drop-B bass recordings.

The low B fundamental near 31 Hz is especially relevant for our testing.

---

# 19. GAIN STAGING & METERING

The plugin should be calibrated around a documented nominal DI input level.

We must prevent hidden excessive gain from accumulating across the processing chain.

Recommended design practices:

- Separate pre-distortion drive from post-distortion output.
- Use meaningful internal reference levels.
- Preserve adequate headroom between modules.
- Compensate perceived loudness where appropriate.
- Prevent unnecessary DC offset after nonlinear processing.
- Use protective gain management without masking poor modelling.
- Keep the default preset at a safe output level.

Changing amplifier characters should not create dramatic jumps in output volume unless intentionally requested.

Changing cabinet models should be reasonably level-matched.

### Metering

Include:

- Input meter.
- Drive output meter.
- DI output meter.
- Room return meter.
- Master stereo output meter.
- DI compressor gain reduction.
- Optical leveller gain reduction.
- Clip indicators.

Meters should provide useful feedback without dominating the minimalist interface.

---

# 20. PRESET SYSTEM

The plugin should ship with a small, carefully curated set of factory presets.

Suggested categories:

| Preset concept | Target |
|---|---|
| Classic Vintage | Warm, controlled vintage tube bass |
| Clean Studio | Strong DI definition with subtle saturation |
| Warm Drive | Rounded SansAmp-style grit |
| Punk Pick | Aggressive pick attack and midrange |
| Modern Tight | Tight, articulated distortion |
| Heavy Grind | More intense modern bass drive |
| Studio Blend | Balanced Drive and clean DI |
| Roomy Vintage | Amplified bass with restrained studio ambience |
| Deep Low B | Controlled extended-range fundamentals |

These are working descriptions, not final marketing names.

Every preset should be:

- Gain-staged.
- Tested with more than one bass.
- Musically useful.
- Clearly different.
- Reasonably level-matched.
- Stable when recalled.

The preset manager should support user presets, host automation and reliable project-state restoration.

---

# 21. Recommended Software Architecture

A modular C++ implementation using JUCE is the preferred starting point.

Suggested internal systems:

**Core**

- Plugin processor and host integration.
- Parameter management.
- Preset management.
- Signal graph and bus routing.
- Latency management.
- Audio metering.

**Drive DSP**

- Multiband crossover.
- Low compressor.
- Distortion engines.
- NAM inference wrapper.
- Oversampling and antialiasing.
- Gain compensation.

**DI DSP**

- FET compressor.
- Three-band EQ.
- DI alignment.

**Cabinet DSP**

- Driver/enclosure models.
- Microphone models.
- Position and distance calculations.
- Dual-mic summing.
- Calibration profiles.

**Space DSP**

- Room/early reflections.
- Late reverb.
- Return filtering.

**Mix Bus DSP**

- Three-channel summing.
- Programme EQ.
- Optical levelling.
- Peak protection.
- Master output.

**UI**

- Persistent mixer.
- Tone page.
- Cab & Space page.
- Overlay/inspector system.
- Control widgets.
- Metering and visualisations.

The engineer may choose a different internal organisation if it is more maintainable or efficient.

---

# 22. Research Repositories & Engineering References

The following references have been identified as potentially useful.

They should be studied, not blindly copied.

## A. JUCE

https://github.com/juce-framework/JUCE

Primary C++ framework for audio plugin development.

Relevant DSP components include Linkwitz–Riley filtering, dynamics processing, EQ filters, oversampling, convolution and audio plugin hosting integration.

Documentation:

https://docs.juce.com/

## B. Basilica Audio — Crypta

https://github.com/basilica-audio/Crypta

An open-source, Parallax-style three-band bass processor.

Particularly relevant for:

- Three-way crossover design.
- Low-band compression.
- Mid/high distortion routing.
- Crossover phase correction.
- Oversampling latency compensation.
- Real-time DSP tests.

Licence: AGPLv3.

Useful as a research reference; copying code creates licence obligations.

## C. NeuralAmpModelerCore

https://github.com/sdatkinson/NeuralAmpModelerCore

Official C++ inference library for NAM models.

Licence: MIT.

Relevant for embedding neural amp/preamp models and benchmarking model inference.

## D. chowdsp_wdf

https://github.com/Chowdhury-DSP/chowdsp_wdf

A C++ library for Wave Digital Filter modelling.

Licence: BSD-3-Clause.

Useful for investigating circuit-based nonlinear gain stages.

## E. ChowDSP BYOD

https://github.com/Chowdhury-DSP/BYOD

An open-source modular distortion processor with useful DSP research and implementation examples.

Licence: GPLv3.

Review distortion concepts and relevant design techniques; do not incorporate GPL code into a closed-source project without appropriate compliance.

## F. Soundshed Guitar

https://github.com/webprofusion/soundshed-guitar

Includes a procedural/filter-based Simple Cabinet engine with microphone modelling and optional dual microphones.

Particularly relevant for our no-recorded-IR cabinet architecture.

Licence: AGPL-3.0.

## G. ConeTracer

https://github.com/vessel-dsp/ConeTracer

Research into continuous microphone-position-dependent cabinet response generation.

Licence: MIT for project code, with separate rights applying to training and third-party assets.

The existing model is trained primarily on guitar cabinet measurements and should not be treated as an already calibrated solution for our four bass cabinets.

## H. CabImpulse

https://github.com/DCisHurt/CabImpulse

A JUCE-based cabinet and microphone simulator using measured impulse responses.

Relevant primarily for UI and cabinet/microphone selection ideas.

Licence: MIT for repository code; verify rights for any audio assets.

### Reference hardware and documentation

- Tech 21 SansAmp Bass Driver DI V2: https://www.tech21nyc.com/products/sansamp-2/bassdriver-di/
- Ampeg SVT-CL: https://ampeg.com/pdf/SVTCL_OM.pdf
- Darkglass B7K: https://www.darkglass.com/pages/microtubes-b7k-manual
- Universal Audio 1176 documentation: https://help.uaudio.com/hc/en-us/articles/4419447352980-UA-1176-Classic-Limiter-Collection-Manual
- Universal Audio LA-2A history: https://help.uaudio.com/hc/en-us/articles/215779663-A-History-of-the-Teletronix-LA-2A-Leveling-Amplifier

Additional supplied references:

- Neural DSP Parallax X v1.0.0 user manual.
- Approved minimalist BASSFORGE UI concepts.
- Ocean Way Studios interface screenshot.
- Fender Tone Master Pro microphone-grid interaction concept.

Reference materials are intended to guide development, not to authorise reproduction of proprietary code, artwork, factory presets, captured models or protected audio assets.

---

# 23. Licensing, IP & Distribution Considerations

Before integrating third-party code, assets or trained models, the engineer must verify their exact licence and permitted distribution uses.

Key requirements:

- Do not copy proprietary Neural DSP algorithms or reverse-engineer licence-protection systems.
- Do not distribute third-party NAM captures without appropriate permission.
- Do not bundle commercial IRs without redistribution rights.
- Do not treat the licence of modelling software as permission to redistribute its training data or outputs automatically.
- Do not reuse branded hardware interface artwork without permission.
- Review JUCE licensing before deciding on a proprietary versus open-source release.
- Verify the legality and marketing implications of manufacturer names.

The user-facing voice names **Vintage, Drive and Modern** help distinguish our own product from the reference hardware.

Likewise, exact cabinet manufacturer names should be evaluated for appropriate descriptive use and any required clearances.

The project name BASSFORGE is provisional until checked.

---

# 24. Proposed Development Milestones

## Milestone 0 — Engineering Research & Feasibility

Deliver:

- Reviewed technical architecture.
- Framework and licence decision.
- CPU and latency budget.
- Prototype modelling strategy.
- Reference measurement plan.
- Principal technical risks.
- Proposed project structure.

The engineer should challenge assumptions and recommend changes before production implementation.

## Milestone 1 — Audio Foundation

Deliver:

- Working VST3 shell.
- Input/output controls.
- Parallel Drive and DI routing.
- Three-way crossover.
- Phase alignment.
- Basic source mixer.
- Functional bypasses.
- DSP test harness.

Success: clean, phase-consistent signal routing without audible artefacts.

## Milestone 2 — Drive Modelling

Deliver:

- Low-band compression.
- Mid/high distortion system.
- Vintage, Drive and Modern voicing prototypes.
- NAM inference integration.
- Appropriate antialiasing.
- Gain staging and level matching.

Success: three audibly distinct, musically convincing gain characters.

## Milestone 3 — DI Processing

Deliver:

- Bass-optimised FET compressor.
- Three-band EQ.
- Gain-reduction meter.
- Reliable DI/Drive summing.

Success: clean DI remains strong, controllable and independent.

## Milestone 4 — Cabinet & Microphones

Deliver:

- First calibrated cabinet prototype.
- Two microphone channels.
- Position grid and overlay interaction.
- Dynamic/condenser/ribbon models.
- Mic balance and phase controls.
- Additional cabinet models.

Success: changing microphone type and position produces predictable, musically credible results.

## Milestone 5 — Room & Parametric EQ

Deliver:

- Seven-band EQ.
- Three room models.
- Parallel reverb return.
- Room fader.
- Send/return gain management.

Success: realistic ambience without muddying bass fundamentals.

## Milestone 6 — Master Bus

Deliver:

- Mix EQ.
- Optical levelling.
- Peak protection.
- Master fader.
- Stereo metering.

Success: smooth, musical final processing with dependable output control.

## Milestone 7 — Complete GUI

Deliver:

- Approved minimalist visual direction.
- Two navigable pages.
- Expandable/collapsible mixer.
- Cabinet hover grid.
- Block bypass controls.
- Preset interface.
- Smooth resize behaviour.

Success: complete usability without sacrificing audio performance.

## Milestone 8 — Calibration, QA & Release Candidate

Deliver:

- Audio measurement reports.
- Reference listening comparisons.
- Performance benchmarks.
- Preset library.
- Regression tests.
- Windows/macOS builds.
- Known-issue list.
- Release documentation.

Success: reliable, polished performance in real recording sessions.

The engineer may reorder milestones if a different sequence reduces technical risk or leads to a better result.

---

# 25. Testing & Acceptance Criteria

Testing must combine objective audio measurements and subjective listening.

## DSP Integrity

Verify:

- No unintended frequency holes in bypassed crossover reconstruction.
- Correct deterministic latency compensation.
- Stable frequency response when crossover controls move.
- No NaN/Inf output values.
- No denormal-related performance issues.
- No unintended DC offset accumulation.
- Click-free automation.
- Stable extreme parameter settings.
- Predictable bypass behaviour.

## Distortion Quality

Verify:

- Distinct harmonic character between the three amp voices.
- Good note definition under high gain.
- Controlled aliasing.
- Responsive playing dynamics.
- No unnecessary high-frequency harshness.
- Stable sub-bass when distortion increases.

## Cabinet Quality

Verify:

- Distinct cabinet responses.
- Convincing microphone differences.
- Plausible movement between grid positions.
- Smooth switching.
- Predictable phase behaviour.
- Acceptable agreement with reference measurements, where available.

## Mix Quality

Verify:

- Drive/DI blending does not unexpectedly remove bass fundamentals.
- Room return does not introduce excessive low-frequency buildup.
- Output remains controlled when multiple channels combine.
- Master EQ and Limiter work correctly across different gain conditions.
- Mono compatibility is maintained.

## Host Compatibility

Test:

- Cubase Pro 14 on Windows and macOS.
- Plugin loading and unloading.
- Automation.
- Preset recall.
- Offline bounce.
- Project reopening.
- Sample-rate changes.
- Varying audio-buffer sizes.
- Multiple plugin instances.
- GUI open/close operations during playback.

## User Experience

A beginner should be able to:

1. Select a Drive character.
2. Adjust distortion.
3. Select a cabinet.
4. Balance Drive and DI.
5. Add some Room.
6. Achieve a usable sound without opening Advanced controls.

A professional should be able to access the detailed controls needed to perform sophisticated bass tone shaping, microphone positioning and parallel mixing.

---

# 26. Engineering Deliverables

The final handover should include:

- Complete project source code.
- All required project assets.
- Build instructions.
- Dependency and licence inventory.
- DSP architecture documentation.
- Parameter list with ranges/defaults.
- Preset file specifications.
- Measurement and calibration methodology.
- Unit and integration tests.
- Host compatibility results.
- CPU and latency benchmarks.
- Known limitations.
- Release builds and installers, where applicable.
- User manual.

We also want the engineer to document significant design decisions, particularly when the chosen approach differs from this brief.

The documentation should explain the engineering reasoning and measurable benefit, not merely state that an alternative was selected.

---

# 27. Engineering Freedom & Innovation Directive

This section is especially important.

**We are not commissioning a literal clone of Neural DSP Parallax X, a SansAmp, an SVT, a B7K or any other existing product.**

We are commissioning an original bass processing instrument informed by the strengths of these devices.

The reference products define sonic aspirations and useful design principles. They do not define the maximum quality that our plugin may achieve.

The engineer is explicitly encouraged to:

- Investigate newer DSP algorithms.
- Experiment with different nonlinear modelling approaches.
- Propose more accurate amplifier modelling.
- Improve the crossover architecture.
- Develop better phase-alignment techniques.
- Explore more realistic cabinet and microphone simulation.
- Optimise the compression characteristics for bass guitar.
- Introduce intelligently designed automatic gain compensation.
- Suggest improvements to our signal routing.
- Simplify controls that do not add genuine musical value.
- Reject technically weak solutions.
- Challenge assumptions that reduce sound quality.
- Develop innovative features that complement the product rather than unnecessarily complicate it.

For example, if a hybrid speaker model clearly outperforms a conventional filter approximation, we want to explore it.

If an original nonlinear DSP engine provides more convincing continuous gain control than a static NAM capture, we should prioritise the better sound.

If a different crossover topology gives improved phase behaviour and lower latency, the engineer should propose it with suitable test evidence.

If a more effective compressor or room algorithm achieves the intended musical behaviour, the engineer is encouraged to recommend it.

### What should remain fixed

The product's core experience:

- Two main sound-design pages.
- A persistent, collapsible mixer.
- Separate Drive and DI channels.
- Three named Drive voices.
- Four cabinet choices.
- Dual virtual microphones.
- Drive-only parametric EQ.
- Independent Room return.
- Mix EQ and Limiter on the final bus.
- A minimalist, accessible interface.

### What can be improved

- DSP algorithms.
- Detailed signal routing.
- Phase compensation.
- Modelling techniques.
- Compression timing.
- Cabinet acoustic implementation.
- Internal gain structures.
- Parameter mapping.
- Software architecture.
- Performance optimisation.
- Advanced controls and workflow.

Any major change to the product experience should be discussed before implementation.

**Technical excellence is more important than preserving an early design assumption.**

---

# 28. Final Product Standard

The completed BASSFORGE plugin should feel like a premium instrument, not a collection of unrelated effects.

The Drive section should feel responsive and musical.

The DI should feel solid and reassuring.

The cabinet should feel like placing microphones on a physical speaker.

The room should add a believable sense of space.

The mixer should make combining the signals intuitive.

The master processing should add polish without destroying the original bass dynamics.

Ideally, users should be able to produce a professional, release-ready bass tone entirely within one instance of BASSFORGE.

Our benchmark is not simply whether the plugin contains the same features as the reference products.

Our benchmark is whether the whole system produces a better, faster and more satisfying bass-recording experience.

**The final objective:**

Create a bass plugin that combines classic musical character, modern engineering precision and the intuitive workflow of a studio console—an instrument that beginners enjoy using and professional engineers can trust.

We want the engineer to take ownership of the technical quality, experiment confidently, challenge our assumptions and pursue the best possible final result.

**Build something exceptional, not merely something functional.**

---

**END OF MASTER ENGINEERING BRIEF — BASSFORGE v1.0**