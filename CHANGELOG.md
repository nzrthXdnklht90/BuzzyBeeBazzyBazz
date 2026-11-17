# Changelog

## 0.2.0 — 2026-11-17 (experimental)

Changes requested after the first listening session.

* **New FUZZ voice replaces VINTAGE.** The voice order is now **Drive / Fuzz / Modern**. Fuzz is a reduced-order
  model of the Swollen Pickle MkII-topology circuit taken from the Big Dill project (same netlist and
  component values, NAM-calibrated diodes). It is fitted against Big Dill's validated full-circuit
  engine; see `docs/fuzz_voice_report.md`.
  * Like a real fuzz pedal, it runs as **one** fuzz across the mid and high bands while the low band stays
    clean underneath (Low End ownership still applies).
  * Drive = Sustain, Character = Tone (Filter). New **Mids / Grit / Clip** controls (Scoop / Crunch /
    Clip) appear under the main knobs when Fuzz is selected.
  * In Fuzz mode, Mid Drive is the fuzz input trim, High Drive sets how much of the high band feeds the
    fuzz, and High Level becomes a post-fuzz brightness shelf above the High Split.
  * Three new fuzz factory presets: *Fuzz Wall*, *Doom Fuzz*, *Fuzz Cut-Through*. *Classic Vintage* and
    *Roomy Vintage* became *Classic Drive* and *Roomy Drive*.
* **DI compressor: FET | OPTO switch** (idea from the Electric Toad study, `docs/study_electric_toad.md`).
  OPTO is an optical feedback compressor, level-matched to FET (within 1 dB) at every Compression setting.
* **Loudness compensation recalibrated on decaying plucks.** All voices and Drive settings are now within
  1.4 dB of each other on a bass phrase (was 2.7 dB).
* **Session compatibility:** sessions from 0.1 that used Vintage or Drive open on Drive; Modern stays
  Modern.
* Tests: 22 (new: FUZZ-01 pot behaviour, ENG-DISTYLE), all passing.

## 0.1.0 — 2026-11-16 (experimental)

First complete build: Milestone 0 (architecture) plus prototype-level coverage of Milestones 1–7.

* **Engine:** 3-way LR4 Drive crossover with all-pass compensation; DI phase-matched to the crossover
  (Drive + DI sum flat to 0.000 dB); oversampled Vintage / Drive / Modern grey-box voices with automatic
  loudness compensation; 4:1 low-band compressor; automatic low-frequency ownership (DI Priority / Hybrid /
  Full Parallel).
* **DI:** FET-style compressor with programme-dependent release; 3-band EQ.
* **Cabinet:** procedural 4-cabinet model (provisional voicings), two mics (dynamic / condenser / ribbon),
  continuous position and distance, Auto Align or Natural timing, polarity, balance, stereo spread.
* **Space:** 7-band Drive EQ (graph with draggable points); room return (Tight / Live / Large), early
  reflections + 8-line FDN, high-passed send.
* **Master:** Mix EQ (programme-EQ-inspired boost/cut interaction), optical leveller (two-stage
  programme-dependent release), true-peak protection, attenuation-only master.
* **UI:** TONE and CAB & SPACE pages; persistent, collapsible mixer with meters, mute and solo (solo is
  never saved); hover/click microphone placement grid; typed value entry on every control; 3-second
  plain-language help; resizable (70–200 %).
* **Presets:** 10 factory starting points, plus user presets (`.bfpreset` XML).
* **Validation:** 20 headless DSP/engine tests, VST3 host smoke test, CI builds for Windows and macOS with
  pluginval, auval and UI screenshots.
