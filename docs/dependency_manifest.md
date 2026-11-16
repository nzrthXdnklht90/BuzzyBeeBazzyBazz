# Dependency manifest

| Dependency | Version / commit | Licence | Used for | Shipped in plug-in? | Modifications |
|---|---|---|---|---|---|
| JUCE | tag 8.0.9, commit `f72bad64d29715216226685810c5196bd0d79d77` | **AGPLv3 or commercial (dual)** | VST3/AU/Standalone wrapper, editor, parameters/state, VST3 hosting in the smoke-test tool | Yes | None (fetched by CMake) |
| VST3 SDK | the copy bundled inside JUCE 8.0.9 | Steinberg VST3 License or GPLv3 (top level); BSD-style for `base/` and `public.sdk/` | VST3 interfaces (through JUCE) | Yes | None |
| pluginval | v1.0.4 release binary, downloaded in CI | GPL-3.0 | External validation tool, run as a separate process | No | n/a |

No other third-party code is linked. In particular, **no code from Crypta (AGPL), BYOD (GPL), Soundshed
(AGPL), ConeTracer, CabImpulse or NeuralAmpModelerCore is used.** The brief listed these as research
references (§22). Only published, textbook-level techniques were used, written as original code:

* Linkwitz–Riley crossovers built from trapezoidal SVFs. The SVF equations follow A. Simper (Cytomic),
  "Solving the continuous SVF equations using trapezoidal integration", 2013.
* Kaiser-windowed halfband/decimator design (Kaiser 1974), shared from the CrestForge project (same owner).
* True-peak guard and look-ahead envelope, shared from the CrestForge project (same owner). Its
  meter-style estimator reproduces the libebur128 4× interpolator design (MIT), which is credited in
  CrestForge's ThirdPartyNotices.
* Feedback delay network room: classic Jot / Stautner–Puckette structure with a Hadamard matrix.
* polyBLEP oscillator (used only to generate the calibration signal).

## Licence decision required before distribution

JUCE 8 is AGPLv3 unless you hold a commercial licence. Personal use and private builds are fine.
**Distributing binaries** (selling them, or giving them to others) requires either:
(a) a JUCE commercial licence (check current tiers and revenue caps), or
(b) releasing BASSFORGE's source under AGPLv3.

## Trademarks

Reference products (Neural DSP Parallax, Ampeg SVT, Tech 21 SansAmp, Darkglass B7K, Hartke, Mesa/Boogie,
Universal Audio 1176 / Teletronix LA-2A, Pultec) are named only in documentation, as sonic references. The
plug-in UI uses original names. The product name "BASSFORGE" is still provisional until a trademark search
is done (brief §23).
