# Mica 1.0 — User Manual

Mica is a 24-band stereo equalizer. Each band is a bell, a low or high shelf, or a low or high cut filter (6 to 48 dB per octave). Mica runs in zero-latency or linear-phase mode, in stereo or mid/side, and every band can work on both channels or just one. A real-time spectrum analyser sits behind the curve. Mica runs as VST3, AU, CLAP and VST2 on macOS, Windows and Linux.

**Contents**

1. [Installing](#1-installing)
2. [A quick tour](#2-a-quick-tour)
3. [Working with bands](#3-working-with-bands)
4. [Band shapes](#4-band-shapes)
5. [The band panel](#5-the-band-panel)
6. [Processing modes: zero latency and linear phase](#6-processing-modes-zero-latency-and-linear-phase)
7. [Stereo and mid/side](#7-stereo-and-midside)
8. [The analyser and display range](#8-the-analyser-and-display-range)
9. [A/B, undo and redo](#9-ab-undo-and-redo)
10. [MIDI Learn](#10-midi-learn)
11. [Presets](#11-presets)
12. [Help and hover tips](#12-help-and-hover-tips)
13. [Recipes](#13-recipes)
14. [Keyboard and mouse reference](#14-keyboard-and-mouse-reference)
15. [Parameter reference](#15-parameter-reference)
16. [Troubleshooting](#16-troubleshooting)

---

## 1. Installing

Download the zip for your system from the project's **Releases** page and copy the plugin files:

| System | Copy | To |
|---|---|---|
| macOS | `Mica.vst3`, `Mica.component` (AU), `Mica.clap` | `~/Library/Audio/Plug-Ins/VST3`, `.../Components`, `.../CLAP` |
| Windows | the `Mica.vst3` folder, `Mica.clap` | `C:\Program Files\Common Files\VST3\`, `...\CLAP\` |
| Linux | `Mica.vst3`, `Mica.clap` | `~/.vst3/`, `~/.clap/` |

**macOS:** the builds are signed ad hoc but not notarized. If macOS blocks them, open Terminal and run:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/*/Mica*
```

Rescan plugins in your DAW. Mica appears under **Epsilon Audio** as an EQ.

---

## 2. A quick tour

![Mica](../screenshot.png)

- **Header**: the preset browser, **A / B / COPY**, **UNDO / REDO**, **LEARN**, **MIDI** and **HELP**. When linear phase is on, the right edge shows the latency.
- **Display**: the frequency range from 10 Hz to 30 kHz.
  - The **white curve** is the total EQ.
  - Each band has a numbered, coloured **dot** and a faint curve of its own.
  - The **analyser** is drawn in blue-grey behind everything.
- **Band panel** (bottom left): the controls of the selected band.
- **Global panel** (bottom right):
  - processing mode and quality
  - channel mode
  - analyser
  - display range
  - output gain

Hover over anything for a moment and a tip explains it.

---

## 3. Working with bands

| To | Do this |
|---|---|
| **Add** a band | Double-click empty space in the display, or click directly on the curve. The new band is a bell at that frequency and gain, and you can keep dragging it. |
| **Move** a band | Drag its dot: left/right sets the frequency, up/down sets the gain. For cut filters, up/down sets the resonance (Q). Hold **Shift** for fine moves. |
| Change the **Q** | Turn the mouse wheel over the dot. |
| **Select** several bands | **Shift-click** (or Cmd/Ctrl-click) dots, or drag a box across empty space. |
| Edit several at once | Drag any selected dot to move them all together. With several selected, the band knobs change all of them: frequency and Q by the same ratio, gain by the same amount. The buttons set all of them to the same choice. |
| Switch a band **off** | **Alt-click** its dot, or use the switch next to "BAND n". A band that is off keeps its settings and shows as a hollow dot. |
| **Delete** bands | Right-click a dot, press **Delete/Backspace**, or click **×** in the band panel. |

Mica has room for 24 bands. Switching a band on or off, changing its shape, and moving it are all smoothed, so none of these click while audio plays.

---

## 4. Band shapes

| Shape | What it does | Controls |
|---|---|---|
| **Bell** | Boosts or cuts a region around the frequency | Frequency, Gain (±30 dB), Q (width) |
| **Low Shelf** | Raises or lowers everything below the frequency | Frequency, Gain, Q (steepness of the transition) |
| **High Shelf** | Raises or lowers everything above the frequency | Frequency, Gain, Q |
| **Low Cut** | Removes everything below the frequency (high-pass) | Frequency, Slope (6/12/24/48 dB/oct), Q (resonance at the corner) |
| **High Cut** | Removes everything above the frequency (low-pass) | Frequency, Slope, Q |

**Q** ranges from 0.025 (extremely wide) to 40 (extremely narrow). A Q of about 0.7 is a neutral starting point for shelves and cuts. For bells, 0.5–1.5 sounds broad and musical, and 4 or more is surgical.

---

## 5. The band panel

The band panel shows the selected band, or the last one you clicked if several are selected:

- **BAND n** switch: on/off.
- **×**: delete.
- **Shape**: BELL, LO SHELF, HI SHELF, LO CUT, HI CUT.
- **Slope**: 6, 12, 24 or 48 dB/oct. Only used by cut filters.
- **Channel**:
  - **STEREO**: both channels.
  - **LEFT** / **RIGHT**: one channel only (in stereo mode).
  - **MID** / **SIDE**: one part only (in mid/side mode).
- **Frequency**, **Gain** and **Q** knobs. Drag the middle up and down, or grab the rim and turn. Double-click a knob to type a value (`2.5k`, `-3`, `0.7`). Cmd/Ctrl-click resets it.

---

## 6. Processing modes: zero latency and linear phase

**Zero latency** (the default) uses minimum-phase filters, like an analog EQ. There is no delay, but each band shifts the phase slightly around its frequency. This is the right choice for recording, for live use and for most mixing.

**Linear phase** builds a single filter from the whole curve that has no phase shift at all. Use it when phase shifts would cause problems:

- on one track of a group that is EQ'd differently from the others (for example drum mics or parallel chains)
- in mid/side work
- in mastering, where the transients should stay exactly as they were

It adds latency, and your DAW compensates for it automatically:

| Quality | Latency (at any sample rate) | At 48 kHz | Low-frequency precision |
|---|---|---|---|
| Low | 3072 samples | 64 ms | Good above ~100 Hz |
| Medium | 6144 samples | 128 ms | Good above ~50 Hz |
| High | 12288 samples | 256 ms | Good above ~25 Hz |

Linear-phase mode follows the analog curve exactly, including near the top of the frequency range. Zero-latency filters narrow slightly as they approach half the sample rate, as all minimum-phase digital EQs do.

---

## 7. Stereo and mid/side

**STEREO**: the two channels are left and right. Bands set to LEFT or RIGHT affect only that side.

**MID / SIDE**: Mica splits the signal into **mid** (what both speakers share: vocals, kick, bass) and **side** (the difference between them: reverb, room sound, wide synths). It EQs them separately and recombines them afterwards. Bands set to MID or SIDE affect only that part. Common uses:

- **Tighter low end**: a low cut at 100–150 Hz on SIDE only (see the *Mono Low End* preset).
- **Wider mix**: a gentle high shelf on SIDE only.
- **Vocal presence without touching the stereo bed**: a bell on MID only.

When some bands work on only one channel, the display draws two total curves, one for each channel.

---

## 8. The analyser and display range

**Analyser**:
- **PRE** shows the signal coming into Mica.
- **POST** shows the signal after the EQ.
- **OFF** hides it.

It uses a 4096-point FFT that updates about 30 times a second, has a fast attack and a slow fall, and runs on its own scale (0 to −90 dBFS). A slight upward tilt (3 dB per octave around 1 kHz) makes a well-balanced mix look roughly flat.

**Display range**: ±6 dB for subtle mastering moves, ±12 dB for mixing, ±30 dB for big moves and deep cuts. The range changes only how the display looks, not the sound.

---

## 9. A/B, undo and redo

**A/B**: Mica holds two complete settings. Click **B** to switch to it; the first time, B starts as a copy of A. Click **A** to go back. **COPY** copies the current slot into the other one.

**Undo / redo**: every edit made in Mica's window counts as one step. That includes adding, moving and deleting bands, changing controls, loading presets and switching A/B. Use the header buttons, or **Cmd/Ctrl-Z** and **Cmd/Ctrl-Shift-Z** (or **Cmd/Ctrl-Y**). Changes from host automation or MIDI are not undo steps.

---

## 10. MIDI Learn

1. Route a MIDI track or controller to Mica in your DAW.
2. Click **LEARN**.
3. Click a control. Band knobs learn the band that is currently selected.
4. Move a knob or fader on your controller. The control shows its **CC** number.
5. Click **LEARN** again, or press Esc.

To remove assignments, right-click a control in Learn mode, or use **CLEAR ALL ASSIGNMENTS**. Assignments are saved with your project. Every band parameter can also be automated directly from your DAW: each band's frequency, gain, Q, shape, slope, channel and on/off are all host parameters.

---

## 11. Presets

| Category | Inside |
|---|---|
| **Vocals** | Clarity, warm, air, de-harsh, podcast voice, backing vocals |
| **Drums** | Kick punch, snare crack, overheads, drum bus, hi-hats, toms |
| **Bass & Instruments** | Bass weight and definition, acoustic and electric guitar, piano, synth pad |
| **Mix & Master** | Smile curve, gentle and bright master (linear phase), wider sides, mono low end, loudness curve |
| **Corrective** | Rumble filter, 50 Hz and 60 Hz hum removers, mud, boxiness, telephone |

- Loading a preset keeps your analyser and display-range settings.
- A **\*** after the preset name means you have changed it.
- **SAVE CURRENT AS...** stores your own preset.
- To delete a user preset, right-click it, then click it again.

| System | User preset folder |
|---|---|
| macOS | `~/Library/Audio/Presets/Epsilon Audio/Mica/` |
| Windows | `%APPDATA%\Epsilon Audio\Mica\Presets\` |
| Linux | `~/.config/epsilon-audio/mica/presets/` |

---

## 12. Help and hover tips

- **Hover tips**: rest the pointer on any control, dot, the display or a header button. A tip explains it, with its current value and any MIDI CC assigned to it.
- **HELP** opens short help pages. The **HOVER TIPS** switch turns tips on or off, and **FULL MANUAL (WEB)** opens this manual.

---

## 13. Recipes

**Clean up a vocal.**
1. Add a low cut at 80–100 Hz (24 dB/oct).
2. Cut 2–3 dB around 250–350 Hz (Q about 1.2) to remove boxiness.
3. Boost 2–3 dB around 3 kHz for presence.
4. Add a high shelf of +2–3 dB at 10–12 kHz for air.

**Find and remove a resonance.**
1. Add a narrow bell (Q 6–10) with +10 dB of gain.
2. Sweep it slowly until the harsh tone jumps out.
3. Pull the gain down to −4 to −8 dB.
4. Repeat on POST with the analyser to confirm.

**Kick and bass.** On the kick, boost about 60 Hz and cut about 350 Hz. On the bass, low-cut at about 35 Hz, and add a small dip where the kick's boost sits, so the two don't fight.

**Remove hum.** Load *Hum Remover 50 Hz* (Europe and most of the world) or *60 Hz* (Americas): four very narrow notches on the hum frequency and its first three harmonics.

**Master gently.** Switch to linear phase (Medium). Low cut at 25 Hz, a tiny wide dip around 300 Hz, and +1 dB of high shelf at 12 kHz. Keep every move under 2 dB and compare with A/B.

**Tighten a stereo mix's low end.** Switch to MID/SIDE, add a 24 dB/oct low cut at 120 Hz, and set its channel to SIDE.

---

## 14. Keyboard and mouse reference

| Action | Result |
|---|---|
| Double-click display / click on curve | Add a band |
| Drag a dot | Frequency (x), gain or cut resonance (y); Shift for fine |
| Mouse wheel over a dot | Q |
| Shift- / Cmd/Ctrl-click a dot | Add to / remove from selection |
| Drag across empty space | Box-select dots |
| Alt-click a dot | Switch band on/off |
| Right-click a dot, Delete, Backspace | Delete band(s) |
| Drag a knob's middle | Adjust (Shift = fine) |
| Grab a knob's rim | Turn in a circle |
| Double-click a knob | Type a value |
| Cmd/Ctrl-click a knob | Reset |
| Cmd/Ctrl-Z | Undo |
| Cmd/Ctrl-Shift-Z, Cmd/Ctrl-Y | Redo |
| Mouse wheel on preset name | Previous / next preset |
| Esc | Leave Learn mode, close Help, cancel typing |

---

## 15. Parameter reference

| Parameter | Range | Default |
|---|---|---|
| Processing Mode | Zero Latency, Linear Phase | Zero Latency |
| Linear Phase Quality | Low, Medium, High | Medium |
| Channel Mode | Stereo, Mid/Side | Stereo |
| Output Gain | −24 – +24 dB | 0 dB |
| Analyzer | Off, Pre EQ, Post EQ | Post EQ |
| Display Range | 6, 12, 30 dB | 12 dB |
| MIDI | Off, On | On |
| Band n Used | Off, On | Off |
| Band n Enabled | Off, On | On |
| Band n Shape | Bell, Low Shelf, High Shelf, Low Cut, High Cut | Bell |
| Band n Frequency | 10 Hz – 30 kHz | 1 kHz (new bands: where you click) |
| Band n Gain | −30 – +30 dB | 0 dB |
| Band n Q | 0.025 – 40 | 1 |
| Band n Slope | 6, 12, 24, 48 dB/oct | 24 dB/oct |
| Band n Channel | Stereo, Left/Mid, Right/Side | Stereo |

---

## 16. Troubleshooting

**The analyser doesn't move.** Make sure it is set to PRE or POST and that audio is playing through Mica.

**Linear phase sounds late, or my DAW shows a delay.** Linear phase needs latency. Hosts with plugin delay compensation (all major DAWs) correct it automatically. If you are recording through Mica, switch to zero latency.

**A low cut set very low doesn't seem as steep in linear phase.** Raise the linear-phase quality (High follows the curve down to about 25 Hz), or use zero-latency mode for that track.

**Cmd/Ctrl-Z undoes something in my DAW.** The host keeps the shortcut. Use the UNDO / REDO buttons.

**The plugin doesn't appear.** Check the install folders and rescan. On macOS, run the `xattr` command from section 1. Logic needs the `.component` file.

---

Mica is free software under the MIT license. "FabFilter" and "Pro-Q" are trademarks of FabFilter Software Instruments; Mica is an independent design and is not affiliated with or endorsed by FabFilter.
