# Mica

**A 24-band stereo equalizer plugin for macOS, Windows and Linux (VST3 · AU · CLAP · VST2).**

Part of **[Epsilon Audio](https://github.com/natehouk/epsilon-plugins)**, a family of free, open-source plugins: Monolith, Caldera, Obsidian, Strata, Basalt, Gneiss, Mica, Flint, Granite, Magma, Schist and Slate.

![Mica](screenshot.png)

Mica is a clean-room design with the feature set of FabFilter Pro-Q, FabFilter's seventh plugin (November 2009). It also includes the mouse-wheel Q adjustment and click-free band switching that arrived in Pro-Q 1.01. It was written from scratch, working only from FabFilter's public press releases. No FabFilter code, binaries, presets, artwork or trademarks were used.

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

**macOS:** the plugins are ad-hoc signed, not notarized. After copying, run:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/*/Mica*
```

## Features

**Bands**
- **Up to 24 bands.** Each one is a **bell**, **low/high shelf** or **low/high cut**.
- Cut filters have slopes of **6, 12, 24 or 48 dB/oct**, and their Q sets a resonant peak.
- **Ranges:** frequency 10 Hz–30 kHz, gain ±30 dB, Q 0.025–40.
- **Per-band channel:** each band works on both channels, the left (or mid) only, or the right (or side) only.
- **Click-free:** switching bands on and off, changing their shape and automating them are all smoothed.

**Processing**
- **Zero latency**: minimum-phase trapezoidal state-variable filters that match the analog prototype curves.
- **Linear phase**: an FIR filter designed from the analog curve and applied with FFT convolution. There are three quality settings (3072, 6144 or 12288 samples of latency, reported to the host).
- **Stereo** or **mid/side** operation.
- **Unlimited headroom**: floating-point processing throughout, plus output gain.

**Display and editing**
- **Large interactive display.** It shows the total curve and each band's own curve. When some bands affect only one channel, it draws a separate total curve for left/mid and right/side.
- **Making and moving bands:**
  - Click on the curve, or double-click, to create a band.
  - Drag a dot to set frequency and gain, or the resonance for cut filters.
  - Use the mouse wheel over a dot to change Q.
  - Alt-click a dot to switch the band on or off.
  - Right-click a dot or press Delete to remove the band.
- **Editing several bands at once:** Shift-click or drag a box to select bands. Dragging moves all the selected bands, and the band knobs change them all relatively.
- **Spectrum analyser** (pre- or post-EQ), drawn behind the curve.
- **Display range** of ±6, ±12 or ±30 dB.

**Workflow**
- **A/B comparison** with copy between the two settings.
- **Undo/redo** for every edit.
- **MIDI Learn.** Band knobs learn whichever band is selected.
- **Big knobs** with vertical and circular dragging. Double-click to type a value.
- **31 factory presets** in five categories, plus your own user presets.
- **Hover tips** on every control and dot, plus a **HELP window**.
- **[Full manual](docs/MANUAL.md)**, also in every release zip as `Mica-Manual.html`.

### Compared with FabFilter Pro-Q (2009, 1.01)

| Pro-Q | Mica |
|---|---|
| Up to 24 bands | ✓ |
| Bell, low/high shelf, low/high cut | ✓ |
| Cut slopes 6, 12, 24, 48 dB/oct | ✓ |
| Per-band stereo, left or right | ✓ (left/mid, right/side) |
| Zero-latency mode, linear-phase mode with adjustable latency | ✓ (three quality settings) |
| Analog-style modelling, unlimited internal headroom | Analog prototypes; linear phase follows them exactly. Floating-point headroom |
| Mid/side mode | ✓ |
| Real-time analyser, pre / post EQ | ✓ |
| Display ranges of 6, 12 and 30 dB | ✓ |
| Only the curves in use are shown | ✓ |
| Drag the curve to create bands; select and edit several in parallel | ✓ plus box selection |
| Double-click to type values | ✓ |
| Interactive MIDI Learn, undo/redo, A/B | ✓ |
| Smoothed parameters, sample-accurate automation | ✓ |
| Help file with interactive hints | ✓ hover tips, in-plugin help pages and full manual |
| Q with the mouse wheel; click-free band switching (1.01) | ✓ |
| Pro Tools (RTAS/AAX) | Not supported (needs Avid's SDK and signing) |

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
g++ -O2 -std=c++11 features.cpp -o features && ./features   # every shape/slope vs its analog curve, a six-band curve, linear phase (latency,
                                                            # symmetry, accuracy), placement, mid/side, click-free changes, output, analyser
g++ -O2 -std=c++11 fuzz.cpp -o fuzz && ./fuzz               # random parameters, both modes, all qualities, 44.1-192 kHz: always finite
g++ -O2 -std=c++11 presets.cpp -o presets && ./presets      # plays pink noise through every preset
g++ -O3 -std=c++11 bench.cpp -o bench && ./bench            # CPU with all 24 bands, zero latency and linear phase (high)
```

With all 24 bands in use, Mica takes about 2% of one CPU core in zero-latency mode. In linear-phase High, while its curve is being redesigned every block, it takes about 11%.

Release checks:
- Steinberg's VST3 validator passes 47/47.
- The interface is exercised with scripted input and live audio: creating, dragging and box-selecting bands, the analyser, A/B, tips and help.

## Source layout

| File | Contents |
|---|---|
| `plugins/Mica/MicaEngine.hpp` | All DSP: band filters, linear-phase FIR design and convolution, M/S, analyser feed. Header-only |
| `plugins/Mica/Fft.hpp` | Small radix-2 FFT (linear phase and the analyser) |
| `plugins/Mica/ParamDefs.hpp` | Parameter table, labels, value mapping |
| `plugins/Mica/Presets.hpp` | Factory presets |
| `plugins/Mica/UserPresets.hpp` | User preset files |
| `plugins/Mica/Help.hpp` | Hover tip text and in-plugin help pages |
| `plugins/Mica/MicaPlugin.hpp/.cpp` | Plugin wrapper: parameters, latency reporting, programs, MIDI Learn |
| `plugins/Mica/MicaUI.cpp` | Custom vector interface (NanoVG) |
| `docs/MANUAL.md` | User manual (`Mica-Manual.html` is generated from it with pandoc) |

New parameters are only ever appended, so saved sessions and automation keep working across versions.

## License

[MIT](LICENSE). Built with [DPF](https://github.com/DISTRHO/DPF) and its bundled NanoVG, which are under the ISC license. `plugins/Mica/ResizeHandle.hpp` comes from DPF's examples and keeps its ISC notice.

"FabFilter" and "FabFilter Pro-Q" are trademarks of FabFilter Software Instruments. Mica is not affiliated with or endorsed by FabFilter.
