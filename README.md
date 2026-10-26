# Mica

**A 24-band stereo equalizer plugin for macOS, Windows and Linux (VST3 · AU · CLAP · VST2).**

Part of **[Epsilon Audio](https://github.com/natehouk/epsilon-plugins)**, a family of free, open-source plugins: Monolith, Caldera, Obsidian, Strata, Basalt, Gneiss, Mica, Flint, Granite, Magma, Schist, Slate, Talc, Geode and Malachite.

![Mica](screenshot.png)

Mica is a clean-room design with the feature set of FabFilter Pro-Q 4 (December 2024). Mica 1.x matched the original Pro-Q (2009); version 2.0 adds dynamic EQ, spectral dynamics, natural phase, character modes, EQ Sketch, EQ Match and Spectrum Grab. It was written from scratch, working only from FabFilter's public product pages and press releases. No FabFilter code, binaries, presets, artwork or trademarks were used.

It is part of a family of plugins:
- [Monolith](https://github.com/natehouk/monolith) (synth)
- [Caldera](https://github.com/natehouk/caldera) (filter)
- [Obsidian](https://github.com/natehouk/obsidian) (polysynth)
- [Strata](https://github.com/natehouk/strata) (tape delay)
- [Basalt](https://github.com/natehouk/basalt) (live dual filter)
- [Gneiss](https://github.com/natehouk/gneiss) (compressor)
- **Mica**, named after a thin, transparent mineral

## Download

Get the latest build from [**Releases**](../../releases):

| Platform | File | Contains |
|---|---|---|
| macOS 10.13+ (Apple Silicon + Intel) | `Mica-macos-universal.zip` | VST3, AU, CLAP, manual |
| Windows x64 | `Mica-windows-x64.zip` | VST3, CLAP, VST2, manual |
| Linux x64 | `Mica-linux-x64.zip` | VST3, CLAP, VST2, manual |

### Install

| Platform | Copy | To |
|---|---|---|
| macOS | `Mica.vst3` / `Mica.component` / `Mica.clap` | `~/Library/Audio/Plug-Ins/VST3` / `Components` / `CLAP` |
| Windows | `Mica.vst3` (folder) / `Mica.clap` | `C:\Program Files\Common Files\VST3\` / `CLAP\` |
| Linux | `Mica.vst3` / `Mica.clap` | `~/.vst3/` / `~/.clap/` |

**macOS:** if a build is not notarized and macOS blocks it, run:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/*/Mica*
```

## Features

**Bands**
- **Up to 24 bands**, in ten shapes:
  - bell
  - low and high shelf
  - low and high cut
  - notch
  - band pass
  - tilt shelf
  - flat tilt (a constant dB-per-octave tilt of the whole spectrum)
  - all pass (phase alignment)
- **Slopes from 6 to 96 dB/oct** in steps of 6. Cuts also have a **brickwall** slope (192 dB/oct).
- **Ranges:** frequency 10 Hz–30 kHz, gain ±30 dB, Q 0.025–40.
- **Per-band channel:** both channels, left/mid only, or right/side only.
- **Solo:** hold SOLO to hear what a band affects: the region around a bell, what a cut removes.
- **Click-free:** switching, reshaping and automating bands are all smoothed.

**Dynamics**
- **Dynamic EQ on every bell, shelf and tilt.** Each band has its own range (±30 dB, cutting or boosting), threshold, attack and release.
- **Sidechain filter:** a band can listen to its own frequencies or to a free band.
- **External trigger:** a band can follow Mica's stereo **sidechain input**.
- **Spectral dynamics:** inside a band, each frequency is treated on its own, so only the resonances that poke out move.

**Processing**
- **Zero latency**: trapezoidal state-variable filters built from the analog prototypes.
- **Natural phase**: the same filters plus a short correction filter. Level and phase then match an analog EQ right up to 20 kHz, with about 4 ms of latency.
- **Linear phase**: an FIR filter designed from the analog curve, at three qualities (64, 128 or 256 ms at 48 kHz).
- **Character**: CLEAN, GENTLE or WARM. The two warmer modes add analog-style saturation only to what the EQ changes.
- **Auto gain**, **gain scale** (−100 to 200%), **phase invert** and output gain.
- **Stereo** or **mid/side**.
- Floating-point headroom throughout.

**Display and editing**
- **Large interactive display.** It shows each band's curve, a dynamic band's range and its live movement, and separate totals when bands affect only one channel.
- **Editing bands:**
  - Create bands by clicking the curve, double-clicking, or **Spectrum Grab** (click a peak in the analyser).
  - Drag a dot for frequency and gain, use the wheel for Q, and drag the diamond for dynamic range.
  - Shift-click or box-select several bands and edit them together.
  - **Copy and paste** bands between instances.
- **EQ Sketch:** draw a curve, and Mica places the bands.
- **EQ Match:** match your track's tonal balance to a reference on the sidechain. Then scale the result with gain scale.
- **Analyser:**
  - pre, post or both, plus the **external** (sidechain) spectrum
  - **collision detection**
  - range, speed, resolution (2048–16384 points), tilt and **freeze**
- **Piano display:** note names, and band frequencies snap to notes.
- **Display ranges:** ±3, ±6, ±12 and ±30 dB.
- **Output meter** with peak hold.

**Workflow**
- **A/B comparison** with copy between the two settings.
- **Undo/redo** for every edit, including sketches and matches.
- **MIDI Learn:**
  - Band controls learn the selected band.
  - **ACTIVE BAND** frequency, gain and Q follow whichever band is selected.
- **Big knobs** with vertical and circular dragging. Double-click to type a value.
- **39 factory presets** in five categories, plus your own user presets.
- The preset browser **searches** names and categories as you type, and a star keeps a preset in **Favourites**.
- **Hover tips** on every control, shape, dot and peak, plus a **HELP window**.
- **[Full manual](docs/MANUAL.md)**, also in every release zip as `Mica-Manual.html`.

### Compared with FabFilter Pro-Q 4 (2024)

| Pro-Q 4 | Mica |
|---|---|
| Up to 24 bands; bell, notch, shelves, cuts, band pass, tilt shelf, flat tilt, all pass | ✓ all ten shapes |
| Continuous slopes up to 96 dB/oct; brickwall cuts | Slopes from 6 to 96 dB/oct in steps of 6; brickwall cuts |
| Zero latency, natural phase, linear phase (adjustable) | ✓ (three linear-phase qualities) |
| Gentle and Warm character modes | ✓ |
| Dynamic EQ: attack, release, sidechain filter, external trigger | ✓ on bells, shelves and tilts |
| Spectral dynamics | ✓ (STFT, about 21 ms latency while in use) |
| EQ Sketch | ✓ |
| EQ Match against an external signal | ✓ (sidechain input) |
| EQ Match against another instance | Not supported (needs instances to talk to each other) |
| Spectrum Grab | ✓ |
| Analyser: pre, post, external; range, speed, resolution, tilt, freeze; collision detection | ✓ |
| Display ranges 3, 6, 12, 30 dB | ✓ |
| Piano roll display | ✓ (note names, snapping) |
| Auto gain, gain scale, phase invert | ✓ |
| Intelligent solo | ✓ |
| Per-band mid/side or left/right | ✓ |
| MIDI Learn, including the active band | ✓ |
| Copy and paste bands between instances | ✓ |
| Output meter with peak readout | ✓ |
| Undo/redo, A/B, double-click entry, help hints | ✓ |
| Resizable interface | ✓ (resizes freely) |
| Instance List | Not supported (needs instances to talk to each other) |
| Surround and Dolby Atmos up to 9.1.6 | Not supported (stereo only) |
| AAX, AudioSuite, Pro Tools control surfaces | Not supported (needs Avid's SDK and signing) |

## Build from source

The plugin is built on [DPF](https://github.com/DISTRHO/DPF):

```
git clone --recursive https://github.com/DISTRHO/DPF.git dpf
make -C plugins/Mica                    # Linux / macOS (macOS also builds the AU)
make -C plugins/Mica WINDOWS=true CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++
```

GitHub Actions builds all three platforms on every push. The macOS build is universal and ad-hoc signed. A `v*` tag, or a release published on GitHub, attaches the zips to that release.

## Tests

```
cd tests
g++ -O2 -std=c++17 -I../plugins/Mica features.cpp -o features && ./features     # 1.x behaviour: shapes, slopes, linear phase, placement, M/S, click-free changes
g++ -O2 -std=c++17 -I../plugins/Mica features2.cpp -o features2 && ./features2  # 2.0: new shapes and slopes, natural phase, dynamic EQ (internal, external,
                                                                                 # all modes), spectral dynamics, character, auto gain, gain scale, invert, solo
g++ -O2 -std=c++17 -I../plugins/Mica compat.cpp -o compat && ./compat           # random 1.x settings through the frozen 1.0.1 engine and 2.0: outputs agree
g++ -O2 -std=c++17 -I../plugins/Mica fuzz.cpp -o fuzz && ./fuzz                 # random parameters, all modes, 44.1-192 kHz: always finite
g++ -O2 -std=c++17 -I../plugins/Mica presets.cpp -o presets && ./presets        # plays pink noise through every preset
g++ -O3 -std=c++17 -I../plugins/Mica bench.cpp -o bench && ./bench              # CPU, every mode
```

CI runs all of them except the benchmark on every push. With all 24 bands in use, Mica takes about 4% of one CPU core in zero-latency mode. While the curve is being automated every block, natural phase takes about 16% and linear-phase High about 14%. The shared EQ engine has its own tests in [epsilon-common](https://github.com/natehouk/epsilon-common).

Release checks:
- Steinberg's VST3 validator.
- The interface is exercised with scripted input and live audio, including a sidechain:
  - creating, dragging and selecting bands
  - dynamics, solo
  - Sketch, Match, Spectrum Grab
  - the analyser menu, A/B, tips and help

## Source layout

| File | Contents |
|---|---|
| `plugins/Mica/MicaEngine.hpp` | All DSP. Header-only. Covers: band filters, natural-phase correction, linear-phase FIR, dynamic EQ, spectral dynamics, character, auto gain, M/S, analyser feeds |
| `plugins/Mica/common/` | Vendored copy of [epsilon-common](https://github.com/natehouk/epsilon-common): EQ band engine, FFT, convolver, curve fitting, undo/A-B, user presets, UI kit. Don't edit; run its `tools/sync.sh` |
| `plugins/Mica/ParamDefs.hpp` | Parameter table, labels, value mapping |
| `plugins/Mica/Presets.hpp` | Factory presets |
| `plugins/Mica/Help.hpp` | Hover tip text and in-plugin help pages |
| `plugins/Mica/MicaPlugin.hpp/.cpp` | Plugin wrapper: parameters, sidechain, latency reporting, programs, MIDI Learn, solo |
| `plugins/Mica/MicaUI.cpp` | Custom vector interface (NanoVG) |
| `tests/legacy/` | Frozen copy of the 1.0.1 engine, used only by the compatibility test |
| `docs/MANUAL.md` | User manual (`Mica-Manual.html` is generated from it with pandoc) |

New parameters are only ever appended, so saved sessions and automation keep working across versions.

## Changelog

- **2.1:** the preset browser searches names and categories as you type, and keeps Favourites.
- **2.0:** Brings Mica up to Pro-Q 4. New in this version:
  - Notch, band pass, tilt shelf, flat tilt and all-pass shapes.
  - Slopes up to 96 dB/oct and brickwall cuts; shelves now have a slope too.
  - Dynamic EQ with a sidechain filter and an external sidechain input; spectral dynamics.
  - Natural phase mode; Gentle and Warm character.
  - Auto gain, gain scale and phase invert.
  - Intelligent solo.
  - EQ Sketch, EQ Match and Spectrum Grab.
  - Analyser: external spectrum, collision detection, range, speed, resolution, tilt and freeze.
  - Piano display; 3 dB display range; output meter.
  - MIDI Learn of the active band; copy and paste of bands.
  - Eight new presets.

  Sessions, automation and presets from 1.x load and sound the same. A test checks this against the frozen 1.0.1 engine.
- **1.0.1:** Right-click actions respond to the right mouse button (unassigning MIDI Learn, deleting user presets, deleting bands).
- **1.0:** First release.

## License

[MIT](LICENSE). Built with [DPF](https://github.com/DISTRHO/DPF) and its bundled NanoVG, which are under the ISC license. `plugins/Mica/ResizeHandle.hpp` comes from DPF's examples and keeps its ISC notice.

"FabFilter" and "FabFilter Pro-Q" are trademarks of FabFilter Software Instruments. Mica is not affiliated with or endorsed by FabFilter.
