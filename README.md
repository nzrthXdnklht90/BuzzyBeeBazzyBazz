# BASSFORGE

Hybrid multiband bass processing and mixing plug-in: a three-band Drive engine (Drive / Fuzz / Modern),
a clean DI channel with a FET- or opto-style compressor, a procedural cabinet with two virtual microphones, a
7-band Drive EQ, a studio room return, and a persistent mixer with Mix EQ, an optical leveller and
true-peak protection.

**Status: v0.2.0, experimental.** It builds, passes automated DSP and host tests, and has a complete
UI. It still needs listening tests with real basses and a check in Cubase and LUNA. See
[`docs/M0_architecture.md`](docs/M0_architecture.md) for what is measured, what is decided and what is
still open.

## Get the plug-in (no compiling needed)

Every push builds Windows and macOS versions on GitHub:

1. Open the repository on GitHub → **Actions** tab → the newest green **build-and-validate** run.
2. Scroll to **Artifacts** and download `BASSFORGE-macos-universal` or `BASSFORGE-windows-x64`.
3. Unzip, then copy:
   * **macOS VST3:** `BASSFORGE.vst3` → `/Library/Audio/Plug-Ins/VST3/` (or `~/Library/Audio/Plug-Ins/VST3/`)
   * **macOS AU (for LUNA):** `BASSFORGE.component` → `~/Library/Audio/Plug-Ins/Components/`
   * **Windows VST3:** `BASSFORGE.vst3` → `C:\Program Files\Common Files\VST3\`
4. **macOS only, first time:** the build is signed ad hoc, not notarised. If macOS refuses to load it, run
   this in Terminal: `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/BASSFORGE.vst3`
   (use the path where you copied it; do the same for the `.component`).
5. In Cubase: **Studio → VST Plug-in Manager → Rescan**. In LUNA: restart LUNA (it scans AUs on launch).

The artifact also contains test logs (`dsp_tests.log`, `host_smoke.log`, `pluginval.log`, `cpu_bench.log`)
and UI screenshots.

## Build it yourself

You need CMake 3.22+ and a C++17 compiler (Xcode on macOS, Visual Studio 2022 on Windows). JUCE 8.0.9 is
downloaded automatically.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
./build/bassforge_tests          # headless DSP tests (Windows: build\Release\bassforge_tests.exe)
./build/bf_bench                 # CPU benchmark
```

The plug-in ends up in `build/BassForge_artefacts/Release/`.

Offline render through the engine (for A/B listening):
`./build/bf_render in.wav out.wav model=modern drive=0.6 cab=3`

## Layout

| Path | What |
|---|---|
| `Source/Dsp/` | Framework-free DSP engine (crossover, voices, dynamics, cabinet, EQs, room, true-peak guard) |
| `Source/Params.*` | Parameter registry: IDs, ranges, defaults and the plain-language hover help |
| `Source/Ui/` | Editor: theme, controls (typed entry, 3-second help), pages, mixer |
| `Tests/` | Headless DSP and engine tests |
| `tools/` | Render, benchmark, VST3 host smoke test, UI screenshot tool, analysis scripts |
| `docs/` | Brief, architecture and decisions, dependency manifest |

## Using it (quick start)

1. Pick an amp voice on the **TONE** page and set **Drive**.
2. On **CAB & SPACE**, pick a cabinet. Hover over it or click it to place the microphones.
3. Use the mixer on the right to balance **DRIVE** and **DI**, then add some **ROOM**.

Rest the mouse on any control for 3 seconds to get a plain-language explanation. Double-click any value to
type an exact number (out-of-range values clamp). Double-click a knob to reset it. Hold Shift for fine
adjustment.

## Licence note

JUCE 8 is AGPLv3 unless you hold a JUCE licence. Private use is fine. Distributing binaries needs a JUCE
licence or an AGPL source release. See `docs/dependency_manifest.md`.
