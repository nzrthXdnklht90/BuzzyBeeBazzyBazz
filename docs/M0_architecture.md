# BASSFORGE — Milestone 0: engineering architecture, decisions and risks

Status: **Milestone 0 complete, and a working first build that covers Milestones 1–7 at prototype level** (16 Nov 2026).
Everything in this document is either *measured* (with the test that measures it), *decided* (with the reason), or *open* (with what closes it).

Release label: **experimental**. The plug-in builds, loads through the VST3 hosting path, passes 20 headless DSP tests and a host smoke test, and has a complete UI. It has **not** been listened to with real basses, calibrated against hardware, or tested in Cubase yet (see section 9).

---

## 1. Product decisions

| Topic | Decision | Reason |
|---|---|---|
| Framework | JUCE 8.0.9 (pinned) + a framework-free DSP core in `Source/Dsp` | The core is testable headless and portable. The same structure as CrestForge, so tools/CI are shared. |
| Formats | VST3 (Win x64, macOS universal), AU (macOS), Standalone for development | Brief §17, plus AU for LUNA on the M2 Mac. |
| Bus layouts | mono→stereo, stereo→stereo, mono→mono | A bass track is usually mono; the output is stereo because the room and mic spread are stereo. |
| Internal channel model | **The bass is summed to mono at the input.** Drive, DI and the cabinet's shared speaker model run mono. Stereo starts at the two-mic spread and the room. | The fundamental is mono by construction (brief §4: "mono bass fundamentals"). This also halves the CPU of the expensive stages. |
| Precision | Double precision inside; float32 and float64 host buffers | Stable recursive filters at 31 Hz and 192 kHz. |
| Names | Voices **Vintage / Drive / Modern**. Cabinets **4x10 Bright / 8x10 Classic / 2x15 Deep / 4x10 Modern**. No manufacturer names anywhere in the plug-in. | Brief §23 + your standing labelling rule. Reference brands appear only in these docs. |

## 2. Signal flow (as implemented)

```
in (mono/stereo → mono) → Input
 ├─► DRIVE ─ LR4 3-way crossover (Low Split / High Split)
 │     low  ─► Low Comp (4:1) ─► Low Level ─► Low-End ownership gain ─────────────────────┐
 │     mid  ─► oversample ─► voice (Vintage/Drive/Modern) ─► downsample ─► Mid Level ──┐   │
 │     high ─► oversample ─► voice ───────────────────────────► downsample ─► High Level ┤   │
 │                                    upper = mid + high ─► Output trim ─► CABINET (2 mics, stereo)
 │                                                                         + low band (mono, skips the cabinet)
 │                                    ─► 7-band EQ ─► DRIVE fader ─┬─► mute/solo ─► bus
 │                                                                 └─► ROOM send (post-fader, pre-mute)
 │                                                                       ─► 4th-order HP ─► room ─► ROOM fader ─► bus
 └─► DI ─ delay (= Drive latency) ─► crossover phase match ─► FET comp ─► 3-band EQ ─► DI fader ─► bus
bus (stereo) ─► Mix EQ ─► optical leveller ─► MASTER (attenuation only) ─► true-peak protection ─► out
```

### Decisions inside this flow (where the brief left room, or where I deviated)

1. **The DI is phase-matched to the crossover, not just delay-matched.** A Linkwitz–Riley crossover is not a pure delay: its bands share the phase of an all-pass filter. If the DI were only delayed, the DI fundamental and the Drive low band would partly cancel around the split frequencies. The DI therefore runs through the same two all-pass sections (2 cheap filters). *Measured:* clean Drive + DI sum to exactly +6.02 dB from 25 Hz to 16 kHz, with a maximum deviation of 0.000 dB (`ENG-COH`). This is the core of the brief's §5.2 / §18 requirement for "a clean and stable combination of the processed Drive and separate DI channels".
2. **Three-way crossover = two cascaded LR4 splits + an all-pass in the low branch** (the brief's starting point, kept). *Measured:* reconstruction equals AP(low)·AP(high) to 1e-16, so the magnitude is flat to 0.00000 dB at 140/850, 70/300 and 400/2000 Hz (`XO-01`). Crossovers are built from trapezoidal SVFs, so they can be automated while audio is playing (`ENG-CLICK`, crossover jump: 1.13× the steady-state step).
3. **Oversampling only around the nonlinear bands** (mid and high), using linear-phase halfband cascades (2×/4×/8× = Eco/Standard/High). The low band and the DI get the same delay as whole samples (the round trip is padded to a whole number of samples), so nothing is fractionally misaligned.
4. **Low-frequency ownership (brief §5.3) is automatic.** *DI Priority* (default) cuts the Drive low band by 12 dB; *Hybrid* by 6 dB; *Full Parallel* by 0 dB. The cut scales with how present the DI really is: muting the DI, or pulling its fader below about −36 dB, gives the low band back smoothly. Solo deliberately does **not** change it, so soloing the Drive lets you hear its real contribution to the mix. *Measured:* −11.9 dB in DI Priority, and +0.0 dB of the Full Parallel level when the DI is muted (`ENG-ROUTE`).
5. **Room send is post-fader and pre-mute.** The default follows the Drive fader (brief §4/§12.4). Pre-mute means *Solo Room* still lets you hear the room. Muting the Drive leaves the room running unless the Room is also muted.
6. **Drive power switch = amp bypass, not channel mute.** With the Drive block off, the clean (latency-aligned, crossover-phase) signal still goes through the cabinet, like a bass played straight into a speaker. To remove the channel, use Mute. This avoids the brief's §16.3 trap of an extra unprocessed feed: switching Drive off replaces the processed signal, it never adds a second one.
7. **Peak protection always runs, even when switched off.** This keeps the latency fixed, and the plug-in outputs the stage's aligned input when protection is off. The protection sits **after** the Master fader, so whatever the faders do, the output is held at the ceiling.
8. **Quality changes the latency** (2.1 / 2.6 / 3.0 ms at 48 kHz). All three oversamplers are prepared up front, so switching quality never allocates memory. The output fades out and back in around a switch (12 ms), and the host is told the new latency.

## 3. Latency (measured: `ENG-LAT`, host smoke `HOST-04`)

| Sample rate | Eco (2×) | Standard (4×, default) | High (8×) |
|---|---|---|---|
| 44.1 kHz | 99 smp (2.24 ms) | 123 smp (2.79 ms) | 141 smp (3.20 ms) |
| 48 kHz | 101 smp (2.10 ms) | 125 smp (2.60 ms) | 143 smp (2.98 ms) |
| 96 kHz | 125 smp (1.30 ms) | 149 smp (1.55 ms) | 167 smp (1.74 ms) |

The DI impulse lands **exactly** on the reported latency at every rate, quality and host block size from 32 to 512.

## 4. Modelling strategy (brief §7) — and why the first build does not use NAM

**Decision: the three voices are original grey-box DSP. NAM is designed in as a future slot, but it is not used in v0.1.**

* No licensed captures of the three reference devices exist for this project yet (brief §7, "Model ownership").
* A single NAM capture is fixed at its captured settings. Our Drive and Character controls must behave continuously (brief §7, "Critical capture limitations").
* NAM wants full-range input. Our voices sit *inside* the multiband split (brief §7, last paragraph). A capture fed only the mid band would no longer behave like the device that was captured.
* Grey-box voices cost about 1–2 % CPU. A WaveNet NAM model costs much more, run twice (once per band).

**How NAM would fit later (proposal):** as an *optional full-range "Capture" voice* that sits before the split (full-range input, as the brief requires), followed by the clean multiband re-split. That needs captures you own: see question 1 in section 10.

### The voices (all TUNABLE starting points, waiting on listening tests)

| Voice | Structure | Character knob |
|---|---|---|
| **Vintage** (SVT-inspired) | 2 asymmetric valve-like stages. The bias shifts with level, and a coupling capacitor removes the shifted DC, which gives the dynamic "blocking" asymmetry. Miller low-pass between the stages, a tone stack, a soft push-pull power stage, and supply sag (25/180 ms). | Dark and fat ↔ bright growl (tone-stack tilt) |
| **Drive** (SansAmp-inspired) | Presence pre-emphasis into a soft, slightly asymmetric preamp; a second soft stage engages above 55 % drive; de-emphasis. **No speaker emulation inside**, because the real cabinet stage follows (brief §6.2). | Presence (pre- and post-emphasis) |
| **Modern** (B7K-inspired) | Frequency-dependent drive *before* the clipper: a low "weight" shelf and a high "bite" shelf (brief §6.3). Then a hard-knee op-amp-style clipper and a 4th-order anti-fizz low-pass. | Weight ↔ bite (pre-clip emphasis balance) |

*Measured:* even/odd harmonic balance differs by voice: H2/H3 = −20.5 dB (Vintage), −9.6 dB (Drive), −28.4 dB (Modern) (`VOICE-02`). All voices stay finite and bounded at +20 dBFS input (`VOICE-01`).

**Automatic loudness compensation (brief §19).** At first load, each voice × 11 drive steps × 3 character steps is measured on a bass-like note, split by the default crossover. The inverse gain is stored. This runs once per process, at a fixed internal rate, and every instance shares it. *Measured on a bass phrase through the whole engine:* every voice and drive setting is within +2.7 dB of the others (`ENG-LEVEL`; the target was ±3 dB). Remaining trend: higher drive is a little louder (+2 dB at 0.8). That can be tightened after listening tests, if you would like it tighter.

## 5. Other blocks

| Block | Implementation | Measured |
|---|---|---|
| Low comp | 4:1, 6 dB soft knee, 3 ms attack, two-speed release (120 ms → 600 ms when the compression is sustained), partial auto make-up | 12 dB in → 3.00 dB out = 4.0:1 (`DYN-01`) |
| DI FET comp | Compression = input drive into a fixed 4:1 threshold, 0.8 ms attack, programme-dependent release (60 → 400 ms), 30 Hz side-chain HP, subtle even harmonics that rise with gain reduction, auto make-up | Recovery 375 ms after a 50 ms burst vs 769 ms after 2 s (`DYN-01`) |
| Cabinet | Procedural (no recorded IRs): shared speaker/enclosure filters (ported/sealed corner, box bump, body, dip, two break-up regions, roll-off) + per-mic radial position, proximity, capsule voicing, distance level/HF and acoustic delay (fractional, Lagrange). Two renders crossfade on discrete changes. | Four cabinets level-matched within 0.15 dB on band noise; distinct responses (> 3 dB spectral difference between every pair) (`CAB-01`). Cap vs cone edge at 3 kHz: +10.8 dB. Ribbon proximity: 7.6 dB more 90 Hz tilt at 0 in than at 6 in (`MIC-01`). |
| Mic timing | Auto Align (no delay difference) / Natural (keeps the path-length difference, nearer mic = reference) | — |
| 7-band EQ | SVF bells; band 1 = Bell/Low Shelf/Low Cut, band 7 = Bell/High Shelf/High Cut; enable fades; graph uses the exact digital response | Flat at 0 dB to 0.00e+00 dB (`EQ-01`) |
| Room | Early reflections + 8-line FDN (Hadamard, RT60-derived absorption, damping, slow modulation), 4th-order high-pass on the send, per-room settings | −30 dB after 0.15 / 0.33 / 0.75 s (Tight / Live / Large); 50 Hz kept 22–38 dB below 900 Hz; L/R correlation 0.05–0.32 (`ROOM-01`) |
| Mix EQ | Programme-EQ-inspired: a resonant low boost, plus a broad low cut whose corner is fitted so boost+cut together gives the "fundamental up, mud down" shape | 60 Hz boost 5 + cut 5 → +2.4 dB at 40 Hz, −1.4 dB at 300 Hz (`EQ-01`; fit in `tools/analysis/fit_program_eq.py`) |
| Leveller | Feedback optical model: light rise about 10 ms, fast cell component 60 ms, slow memory component that charges over about 1 s and releases in 0.5–5 s depending on how charged it is | Recovery 88 ms after 0.2 s vs 1308 ms after 4 s (`DYN-01`) |
| Peak protection | True-peak guard reused from CrestForge (8-phase reconstruction estimate + meter-style 4× estimate, look-ahead envelope). New: a provably safe quiet fast path that skips the estimate when nothing in the window can reach the ceiling. | Hot, heavily driven input: sample peak −1.03 dBFS, independent 8× interpolated peak −1.03 dBFS at a −1.00 ceiling (`ENG-TP`) |

## 6. Real-time safety (brief §17)

* No allocation, locks or file access in `process()`. Buffers, all three oversamplers, the voice tables and the delay lines are prepared in `prepare()`.
* Parameters are smoothed: gains are ramped per sample; filter settings are updated per 32-sample chunk in their natural domain (log-Hz / dB). Every on/off is a 12 ms crossfade. Voice, cabinet and capsule changes are crossfades between two instances. *Measured:* the worst switching step is 1.26× the steady-state sample step, with no clicks (`ENG-CLICK`, 12 switch types).
* NaN/Inf input is sanitised, and a non-finite internal state resets the engine (counted in meters; it never happened in a 20 s all-parameter random automation run, `ENG-STRESS`). Silence decays to −334 dBFS: no DC, no denormals (`ENG-SAFE`).
* Host bypass outputs the input delayed by the reported latency (host smoke `HOST-08`).

## 7. CPU (measured, `tools/bf_bench`, all blocks on, both mics)

Linux cloud vCPU (slow server core, the only machine available to me). The M2 should be noticeably faster; CI prints the Windows/macOS numbers in `cpu_bench.log` on every push.

| | Eco | Standard | High |
|---|---|---|---|
| 48 kHz | 5.5 % | 8.4 % | 9.7 % |
| 96 kHz | — | 15–16 % | — |

Main cost: the Vintage voice at the oversampled rate (about 50 %). Next optimisation steps, in order: SIMD (process the mid and high bands together as one 2-lane vector), float precision inside the voices, and the Eco mode default when tracking. **Not done yet.**

## 8. Validation summary

| Suite | Result |
|---|---|
| `bassforge_tests` (20 tests: filters, crossover, oversampling, voices, dynamics, cabinet/mics, room, EQs, whole-engine latency/coherence/safety/level/clicks/routing/quality/stress) | **20/20 pass** |
| `bf_host_smoke` (loads the built `.vst3` through the VST3 hosting path: layouts, 101 parameters, latency at 15 rate/block combinations, hot input, automation storm, state round-trip, bypass, editor ×30) | **pass (0 failures)** on Linux |
| pluginval strictness 7, auval | CI (Windows/macOS), every push |
| UI screenshots | `bf_ui_capture`, CI artefact `ui_screenshots/` |

## 9. What is NOT done (honest list)

1. **No listening tests with real basses yet.** Every voicing constant is a starting point marked TUNABLE. This is the most important next step.
2. **Cabinets are provisional.** They are hand-voiced from the general character of each cabinet family, not fitted to measurements (brief §9.3). Marketing must not call them recreations.
3. **Not yet loaded in Cubase Pro 14 or LUNA.** Only my headless host and pluginval/auval in CI have loaded it.
4. Factory presets are first drafts by ear on synthetic material (brief §20 asks for tests with more than one bass).
5. No spectrum analyser in the EQ graph yet (brief §11 lists it as optional).
6. NAM slot is not implemented (section 4).
7. No SIMD optimisation yet (section 7).
8. Pre-fader room send mode not offered (brief §12.4 lists it as optional).

## 10. Questions for you (these decide the next milestone)

1. **Hardware for captures and calibration.** Do you own, or can you borrow, a SansAmp Bass Driver, a B7K, or an SVT, or any of the four cabinets? With them I can (a) calibrate the voices against real measurements and (b) consider a full-range NAM "Capture" voice made from captures you own.
2. **Reference DI recordings.** Could you share 3–4 short dry DI files (fingers, pick, slap, a 5-string low B)? Synthetic test material gets the engineering right; only real playing gets the voicing right.
3. **Distribution plan.** Personal use, or a commercial release? JUCE is AGPLv3 unless you hold a JUCE licence. That matters before any public binary (see `dependency_manifest.md`).
4. **Default Quality.** The default is Standard (4×). Would you rather have Eco by default for low-latency tracking, and switch to High for mixdowns?

## 11. Proposed next steps (re-ordered from brief §24 to reduce risk)

1. **Listening + voicing pass** with your DI files (voices, low-comp timing, Low End mode defaults, presets).
2. **Cubase / LUNA verification** on the M2 and the Zephyrus (load, automation, recall, offline bounce).
3. **Cabinet calibration** if measurements or hardware are available; otherwise refine by ear against reference recordings.
4. **SIMD/CPU pass**, then the spectrum analyser and smaller UI refinements.
