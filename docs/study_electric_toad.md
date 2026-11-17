# Study: "Electric Toad" (HauntologerFx, v1.0, 4 Nov 2026) vs BASSFORGE

Source: the hand-drawn schematic screenshot you supplied, from the YouTube video
`KsnpqNmQIao`. **I could not watch the video** (the page could not be fetched from here). Everything
below is read from the schematic alone. Values I could not read clearly are marked "(?)".
Frequencies are calculated from the drawn component values.

## 1. What the circuit is

A two-path parallel bass pedal: a **frequency-shaped overdrive** blended with an **optically
compressed clean** path, summed, then sent through a **4-band graphic EQ**. Single supply with a
half-supply reference Vb (100k/100k divider, 47 µF).

```
input ─ 220n/1M ─ buffer ─┬─ DRIVE: non-inverting op-amp, 1N4148 pair in feedback
                          │         gain = 1 + (47k + Drive 1M A) / Zshunt(f)
                          │         ─ 100n ─ (4n7) ─ Drive Level 100kA ─ buffer ─ +6 dB treble shelf ─┐
                          └─ CLEAN: buffer ─ 10n ─ op-amp, feedback 43k + (NSL-32 || Compression 1M B)   ├─ sum ─ graphic EQ ─ out
                                    side-chain: output ─ inverting amp (≈ ×4.7) ─ diode bridge ─ NSL-32 LED
                                    ─ Clean Level 100kA ─ buffer ────────────────────────────────────┘
```

### 1.1 Drive path: frequency-dependent gain before clipping
The gain-setting leg to ground is **two RC branches in parallel**:
* **47 nF + 5.6 kΩ**: turns on above **605 Hz**
* **1 µF + pot (?) + 2.2 kΩ**: turns on above **≤ 72 Hz**; the unlabelled pot moves this corner and the depth (probably a "bass drive / fat" control)

Below the corners the gain falls towards 1, so **the lowest notes are clipped much less than the
mids**. That is his way of keeping the fundamental clean without a crossover. Maximum gain at
mid/high frequencies is about 1 + 1.047 MΩ / (5.6k ∥ 2.2k) ≈ 660 (**+56 dB**). The antiparallel
1N4148 pair gives soft, symmetric (odd-harmonic) clipping at about ±0.6 V.

After the drive level, a non-inverting stage with 10k feedback and a 100 nF + 10k leg to ground gives
**+6 dB above about 160 Hz**, a presence/treble shelf on the dirty path only.

**About the circled "VCC?":** the line that rises from the drive op-amp's output region and runs to the
top of the 47k / Drive pot / diode network has to be **the op-amp output closing the feedback loop,
not VCC.** If that node were tied to +9 V, there would be a DC path through 1 MΩ into the inverting
input and the stage would sit against the rail. This is an inference from the circuit, not
something the drawing states.

The 4.7 nF after the 100 nF coupling cap has no series resistor in the drawing. As drawn, it mostly
loads the op-amp output and does very little filtering. It may be a drawing simplification of a
de-fizz low-pass. **Worth checking against his video.**

### 1.2 Clean path: an optical feedback compressor
* The gain stage has 43k to Vb and **43k + (NSL-32 photo-resistor ∥ 1 MΩ "Compression" pot)** as feedback.
  * Photo-cell dark (MΩ): gain ≈ 1 + (43k + 1M∥R_dark)/43k, up to about **×24 (+28 dB)** with the pot at max.
  * Photo-cell lit (a few kΩ): gain ≈ **×2 (+6 dB)**.
  * So the pot sets the maximum gain-reduction range (about 0–20 dB).
* **Side-chain:** taken from the compressor's own output through 330 Ω, amplified by an inverting stage
  (10k / 47k, about ×4.7), full-wave rectified by a 4-diode bridge, and driving the NSL-32's LED.
  That is a **feedback topology with an optical cell**, the same family of behaviour as the
  LA-2A: slowish attack and a programme-dependent release, because the cell remembers.

### 1.3 Graphic EQ (gyrator bands around an inverting op-amp, 22k/22k, 100 pF)
Gyrator centre frequency f = 1 / (2π √(R₁ R₂ C₁ C₂)), with R₁ = 200k to Vb:

| Band | Parts | Centre |
|---|---|---|
| 1 | 470 nF, 10 nF, 3.3k | **≈ 90 Hz** |
| 2 | 100 nF, 8.2 nF, 1k | **≈ 390 Hz** |
| 3 | 68 nF, 470 pF, 1k | **≈ 2.0 kHz** |
| 4 | cap (?) + 2k to Vb | treble band (value not legible) |

Each band's slider sits between EQ+ and EQ−, the classic boost/cut graphic EQ. It acts on the
**summed** signal, so it shapes clean and dirty together.

## 2. Same idea, different engineering

| Aspect | Electric Toad (analog pedal) | BASSFORGE |
|---|---|---|
| Keeping lows clean | RC legs reduce drive gain at low frequencies (gentle slopes, so some fundamental still clips) | LR4 crossover: the low band never enters the distortion, plus a separate DI and automatic low-end ownership |
| Clean/dirty phase | No alignment. Two op-amp paths with different filtering, summed. Fine in analog, at the cost of some phase smear near 70–600 Hz | DI phase-matched to the crossover. Measured flat to 0.000 dB when summed |
| Clean compression | **Optical feedback** (NSL-32 vactrol) | FET-style (v0.1) → **v0.2 adds an OPTO option** (see 3.1) |
| Distortion | One soft-clipping op-amp stage | Three voices, oversampled; Fuzz is a circuit-derived three-stage model |
| Post-distortion tone | +6 dB treble shelf on the dirty path | 7-band Drive EQ, per-voice tone, cabinet + mics |
| Master tone | 4-band graphic EQ on the sum | Mix EQ (programme-EQ style) on the sum |
| Cab / room | none (pedal) | procedural cab, 2 mics, room |

**Takeaway:** he reached the same core conclusion as your brief. The bass fundamental must stay clean
and steady while the mids and highs carry the dirt, and a compressed clean path is the glue. Our
version does each piece more precisely (a real crossover, phase-matched DI). His design shows two
things worth borrowing.

## 3. What we took from it

### 3.1 Implemented in v0.2: OPTO style for the DI compressor
A **FET | OPTO** switch on the DI compressor. OPTO uses our optical-cell model: feedback detection,
about 10 ms light rise, and a two-stage memory release, which is the same family as his NSL-32 loop. The
Compression knob drives it in both styles. Default stays FET, so existing sessions sound the same.
The switch crossfades (no click) and the GR meter follows the active style.

### 3.2 Not implemented (recorded as options)
* **"Pedal mode" crossover slopes:** a gentle 6 dB/oct "shelving split" option instead of LR4, for a
  looser, more analog blend like his RC legs. It is possible, but the DI phase match only works
  with the LR4 all-pass structure. I would only add this if you ask for it.
* **Graphic-EQ view for Mix EQ** (90 Hz / 390 Hz / 2 kHz / treble faders): an alternative face for the
  master tone. The Mix EQ already covers this role, so it is skipped to keep the UI uncluttered.
* **Post-drive presence shelf** (his +6 dB above 160 Hz): already covered by Character and the
  7-band Drive EQ.
