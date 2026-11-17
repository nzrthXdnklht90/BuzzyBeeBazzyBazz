# FUZZ voice ("Fast Pickle") — design and validation report (v0.2, 17 Nov 2026)

You asked for the Fuzz voice to be "based on our past plugin but able to adopt to BASSFORGE's approach".

## 1. What was reused from Big Dill
* The **circuit**: `spice/mkII_circuit.inc`, the Swollen Pickle MkII topology with 470k/100k voicing.
  It has the same stages, component values and pot laws (Sustain 100kB, Clip 500kB, Crunch 470kA, Filter 100kB,
  Scoop 50kB, Voice 500kB fully clockwise as on the factory unit).
* The **calibrated devices**: Si pair Is 2.308e-8 / N 1.878 and LED pair Is 9.951e-12 / N 3.73, from
  Big Dill's fit to NAM captures of a real MkIIS.
* The **validated full-circuit engine** (Big Dill `RtEngine`, which matches SPICE). It is used as the
  **reference** the new voice is measured and fitted against. Its code is not linked into BASSFORGE.

## 2. Why not just drop Big Dill's engine in
Big Dill's full 11-port Newton solve cost 26–45 % of this cloud core per mono instance. Inside
BASSFORGE it would run next to the crossover, two other band voices, the cabinet, room and mix bus.
That breaks the CPU priority in your project rules.

## 3. How it is adapted to BASSFORGE
* **Reduced-order circuit model** (`Source/Dsp/FuzzVoice.*`). Each transistor stage is one node
  equation at the base, with exact trapezoidal models of the input R-C, Rf‖Cf feedback, base resistors
  and the **diode branches with their series caps and pot resistances**. The caps keep the bias shift
  and the "breathing" that make the pedal feel alive. The transistor is a gain stage with a smooth,
  asymmetric swing limit. One warm-started Newton solve per stage per sample, using pre-solved diode
  tables.
* The **Filter / Scoop / Voice tone network is solved exactly** (linear nodal), so Tone and Mids are
  the real passive network.
* **Multiband routing:** a fuzz pedal must see the whole guitar/bass band to behave like itself, so in Fuzz
  mode the mid and high bands are summed into **one** fuzz instance. Meanwhile the clean, compressed low band stays
  underneath and keeps the DI phase alignment. This is the classic "bass fuzz with clean low blend", done
  with a phase-coherent crossover.
* Controls: Drive → Sustain, Character → Tone, plus **Mids / Grit / Clip** (Scoop / Crunch / Clip).
  Loudness is replaced by BASSFORGE's automatic loudness compensation and Output trim.

## 4. Validation against Big Dill's engine
Reference set: 12 control settings (Sustain, Tone, Mids, Grit and Clip at their extremes and in
combinations) × 3 input levels (50 mV, 300 mV, 1 V) × 2 signals (220 Hz sine, plucked bass), plus a
small-signal noise sweep per setting. That is 84 cases at 192 kHz (`tools/fuzz_fit/refgen.cpp`, `cases.txt`).

Eight constants were fitted (Nelder–Mead): loop gain, two swing limits, the three Q4 constants, the tone
source impedance and the base input resistance. Every other number is the circuit's own value.

| Metric | Circuit values only (before fit) | After fit |
|---|---|---|
| Small-signal tone (Filter/Scoop network), shape error | ~1.2 dB | **0.30–0.61 dB** (one setting 1.5 dB) |
| Output level, mean / worst | 1.47 / 4.29 dB | **0.95 / 3.27 dB** |
| Harmonic-spectrum shape (1/3-oct, level removed), mean / worst | 3.91 / 13.98 dB | **2.31 / 6.42 dB** |

Full per-case table: `tools/fuzz_fit/fit_result_2026-11-17.txt`. The worst cases are a 50 mV sine at full
Grit (shape 6.4 dB) and full-level bass at Grit 0.5 (shape 4–5 dB). These are the regions where the simplified
transistor model differs most from the full Ebers–Moll devices.

**Fidelity label:** "Reduced-order model of Big Dill's circuit model, fitted to Big Dill's SPICE-validated engine.
The level and tone network match closely; the fine harmonic detail is approximate. Not hardware 1:1."

## 5. Behaviour checks in BASSFORGE (`FUZZ-01`)
Sustain 0.1 → 0.9: THD −17 → −8 dB. Tone: +18.7 dB at 3 kHz. Mids: +17 dB at 700 Hz. Clip Si → LED:
+5.5 dB (more open, louder). 5 V input at extreme settings: bounded (6 V peak). The engine loudness check puts Fuzz
within 0.6 dB of the Drive voice across the Drive knob.

## 6. CPU (this cloud core, 48 kHz, Standard 4×, everything on)
Drive 5.8 % · Modern 7.0 % · **Fuzz 10.2 %**. Fuzz adds about 4–5 % over the other voices (Big Dill's full engine
would add about 25–45 %). Eco quality halves the fuzz cost. Next step if needed: SIMD across the three stages.

## 7. Not done
* No listening comparison against a real Dirty Donny / MkII (you don't own one). The reference is
  Big Dill's model, which was calibrated to third-party MkIIS captures.
* The 47k "Bright" voicing and the red-LED option from Big Dill are not exposed in BASSFORGE (to keep the
  UI simple). They can be added as advanced options if you want them.
