# Changelog

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
