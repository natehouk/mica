// Mica — hover tips and the in-plugin help pages. License: MIT.
#pragma once
#include "MicaEngine.hpp"
#include "common/UiKit.hpp"

namespace mica {

static const char* const kManualURL = "https://github.com/natehouk/mica/blob/main/docs/MANUAL.md";

inline const char* shapeTip(int s) {
    switch (s) {
    case kBell: return "BELL: a boost or cut around the frequency; Q sets the width.";
    case kLowShelf: return "LOW SHELF: raises or lowers everything below the frequency. Slope sets how quickly it changes (6 to 96 dB/oct); Q shapes the knee.";
    case kHighShelf: return "HIGH SHELF: raises or lowers everything above the frequency. Slope and Q as for the low shelf.";
    case kLowCut: return "LOW CUT: removes everything below the frequency (a high-pass filter), 6 to 96 dB/oct or Brickwall. Raising Q adds a resonant bump at the corner.";
    case kHighCut: return "HIGH CUT: removes everything above the frequency (a low-pass filter), 6 to 96 dB/oct or Brickwall.";
    case kNotch: return "NOTCH: removes a narrow band completely, for hum, whistles and ringing. Q sets the width.";
    case kBandPass: return "BAND PASS: keeps only the region around the frequency and removes the rest; Q sets the width, Slope the steepness of both sides. Good for telephone and radio sounds.";
    case kTiltShelf: return "TILT SHELF: tilts the balance around the frequency: half the gain is taken away below it and added above it (or the other way round), so the level at the frequency stays put.";
    case kFlatTilt: return "FLAT TILT: tilts the whole spectrum by an even number of dB per octave, pivoting at the frequency. Gain is the change across the 10 octaves from 20 Hz to 20 kHz, so +6 dB is +0.6 dB per octave.";
    case kAllPass: return "ALL PASS: changes the phase around the frequency without changing the level. Use it to align the phase of two tracks (kick and bass, two microphones); Slope sets how much phase it turns.";
    default: return "";
    }
}

inline const char* paramTip(uint32_t p) {
    switch (p) {
    case kProcMode: return "ZERO LATENCY: minimum-phase filters with no delay, like an analog EQ. NATURAL PHASE: the same filters, corrected so that their level and phase match an analog EQ right up to the top of the range, with 4 ms of latency. LINEAR PHASE: no phase shift at all, with more latency. Your DAW compensates the latency.";
    case kLinQuality: return "Length of the linear-phase filter. Higher quality is more accurate at low frequencies but adds more latency: Low about 64 ms, Medium about 128 ms, High about 256 ms at 48 kHz.";
    case kChannelMode: return "STEREO works on left and right. MID/SIDE works on the centre (mid) and stereo (side) parts instead; each band can then be set to mid only or side only.";
    case kOutGain: return "Output level after the EQ. Use it to match the loudness of the bypassed signal when comparing.";
    case kAnalyzer: return "Real-time spectrum analyser behind the curve. PRE shows the incoming signal, POST the result after the EQ, BOTH shows them together. More settings in the ANALYZER menu above the display.";
    case kRange: return "Vertical scale of the display: ±3 or ±6 dB for subtle mastering moves, ±12 dB for mixing, ±30 dB for big moves and steep cuts.";
    case kMidiOn: return "Turns MIDI control on or off. When off, learned controllers are ignored.";
    case kCharacter: return "CLEAN is a perfectly clean digital EQ. GENTLE and WARM add analog-style saturation to the changes the EQ makes (WARM more, with some even harmonics): a band you boost gains a little density. Flat bands stay clean.";
    case kAutoGain: return "Keeps the overall loudness about the same as you change the curve, so a boost does not just sound better because it is louder. Works from the curve, not the audio, so it doesn't pump.";
    case kGainScale: return "Scales the gain (and dynamic range) of every band at once: 50% halves all moves, 200% doubles them, negative values turn boosts into cuts. A quick way to set how much of an EQ Match or a preset you want.";
    case kPhaseInvert: return "Inverts the polarity of the output. Use it to check how two tracks combine, or to fix a microphone that is out of polarity.";
    case kAnaExternal: return "Also shows the spectrum of the sidechain input (another track routed to Mica's sidechain), so you can see where two sounds compete.";
    case kAnaRange: return "How many dB the analyser shows from top to bottom.";
    case kAnaSpeed: return "How fast the analyser follows the music. Slow shows the long-term tonal balance, fast shows transients.";
    case kAnaRes: return "Frequency resolution of the analyser. Higher resolution separates low notes better but reacts more slowly.";
    case kAnaTilt: return "Tilts the analyser so that a balanced mix looks roughly flat. 4.5 dB/oct is close to how we hear; 0 shows the raw spectrum.";
    case kAnaFreeze: return "Holds the analyser display so you can study it. Spectrum Grab still works on the frozen spectrum.";
    case kCollision: return "With the external spectrum shown, marks in red where the main signal and the sidechain are both loud at the same frequencies: the places to carve out room.";
    case kPiano: return "Shows a piano keyboard along the frequency axis and adds note names to the band readouts, for tuning bands to the key of a song.";
    default: break;
    }
    switch (field2Of(p)) {
    case kB2Slope: return "How steep the band is, from 6 to 96 dB per octave.";
    case kB2Dyn: return "Makes the band dynamic: it only moves when its part of the spectrum gets louder than the threshold, by up to the dynamic range. Use it to tame harshness only when it happens, or to bring out detail only when it's quiet.";
    case kB2Range: return "How far a dynamic band can move from its gain: negative values cut when it's loud (like a compressor on that band), positive values boost when it's loud. Also drag the small diamond above or below the band's dot.";
    case kB2Thresh: return "The level where the dynamic band starts to move. Above it, the band moves by half a dB per dB, up to the range.";
    case kB2Attack: return "How fast a dynamic band moves when its part of the spectrum gets loud.";
    case kB2Release: return "How fast a dynamic band returns when the level drops again.";
    case kB2Source: return "INTERNAL: the band listens to its own input. EXTERNAL: it listens to the sidechain input instead, so another track can trigger it (duck a vocal's presence range only while the guitar plays there).";
    case kB2ScFree: return "Normally a dynamic band listens to the same frequencies it changes. FREE lets it listen to another band of frequencies, set with the sidechain frequency and Q.";
    case kB2ScFreq: return "Centre of the frequencies a FREE dynamic band listens to.";
    case kB2ScQ: return "Width of the frequencies a FREE dynamic band listens to.";
    case kB2Spectral: return "SPECTRAL dynamics: instead of moving the whole band, Mica moves each frequency inside it on its own, so only the resonances and peaks that actually poke out are pulled down (or lifted). Adds 21 ms of latency.";
    default: break;
    }
    switch (fieldOf(p)) {
    case kBOn: return "Switches this band in or out without deleting it. Switching fades smoothly, so it is safe while playing. Alt-click a dot in the display does the same.";
    case kBShape: return "The band's shape: bell, low/high shelf, low/high cut, notch, band pass, tilt shelf, flat tilt or all pass. Hover a shape for details.";
    case kBFreq: return "Centre (bell) or corner (shelf, cut) frequency, 10 Hz to 30 kHz. Drag the band's dot left and right to set it.";
    case kBGain: return "Boost or cut in dB. Drag the dot up and down. Cuts, notches, band passes and all passes have no gain; their dot sets the resonance instead.";
    case kBQ: return "Bandwidth. High Q is narrow and surgical, low Q is broad and musical. For shelves it shapes the knee; for cuts it adds a resonant peak. Mouse wheel over a dot changes it.";
    case kBSlope: return "How steep the band is: 6 to 96 dB per octave, or Brickwall for cuts (an extremely steep cut).";
    case kBPlace: return "Which channel the band works on: both (STEREO), only left / mid, or only right / side. Mid and side apply in Mid/Side channel mode.";
    default: return "";
    }
}

enum TipTarget { kTipUndo, kTipRedo, kTipLearn, kTipMidi, kTipHelp, kTipA, kTipB, kTipCopy, kTipPrev, kTipPresets, kTipNext,
                 kTipDot, kTipDisplay, kTipDelete, kTipSolo, kTipSketch, kTipMatch, kTipAnaMenu, kTipRangeHandle, kTipGrab, kTipMeter,
                 kTipActiveFreq, kTipActiveGain, kTipActiveQ, kNumTipTargets };
inline const char* targetTitle(int t) {
    static const char* const n[kNumTipTargets] = { "Undo", "Redo", "MIDI Learn", "MIDI", "Help", "Setting A", "Setting B", "Copy A/B",
                                                   "Previous preset", "Presets", "Next preset", "EQ band", "EQ display", "Delete band", "Solo",
                                                   "EQ Sketch", "EQ Match", "Analyzer settings", "Dynamic range", "Spectrum Grab", "Output meter",
                                                   "Active band frequency", "Active band gain", "Active band Q" };
    return n[t];
}
inline const char* targetTip(int t) {
    switch (t) {
    case kTipUndo: return "Steps back through your edits, including new and deleted bands, preset loads, sketches, matches and A/B switches. Cmd/Ctrl-Z.";
    case kTipRedo: return "Re-applies an edit you undid. Cmd/Ctrl-Shift-Z or Cmd/Ctrl-Y.";
    case kTipLearn: return "Click, then click any control and move a knob or fader on your MIDI controller to assign it. Band knobs learn the band that is selected. The ACTIVE BAND chips learn whichever band you select later. Right-click a control to unassign.";
    case kTipMidi: return paramTip(kMidiOn);
    case kTipHelp: return "Opens the help pages: bands and shapes, dynamic EQ, processing modes, EQ Sketch and Match, the analyser, A/B, MIDI Learn and shortcuts.";
    case kTipA: case kTipB: return "Mica keeps two complete settings, A and B. Click the other letter to switch and compare by ear.";
    case kTipCopy: return "Copies the current setting to the other slot.";
    case kTipPrev: return "Load the previous preset. The mouse wheel over the preset name also steps through presets.";
    case kTipPresets: return "Open the preset browser. A * after the name means you have changed it.";
    case kTipNext: return "Load the next preset.";
    case kTipDot: return "Drag to move: left/right = frequency, up/down = gain (resonance for cuts). Mouse wheel = Q. Shift-click adds to the selection, Alt-click switches the band on/off, right-click (or Delete) removes it.";
    case kTipDisplay: return "Double-click, or click on the curve, to add a band. Drag across empty space to select several bands. With the analyser on, peaks in the spectrum light up: click one to grab it as a new band.";
    case kTipDelete: return "Removes the selected band(s). Undo brings them back.";
    case kTipSolo: return "Hold to hear only what this band affects: the region around a bell, what a cut removes, what a shelf changes. Release to hear everything again.";
    case kTipSketch: return "Draw the curve you want with the mouse. When you let go, Mica places bands to match it. Each stroke refines the curve; click SKETCH again to finish.";
    case kTipMatch: return "Makes your track's tonal balance match a reference. Route the reference to Mica's sidechain, click LISTEN, play both for a while, then APPLY: Mica builds the bands for you. Use GAIN SCALE to set how much of the match you want.";
    case kTipAnaMenu: return "External spectrum, range, speed, resolution, tilt, freeze, collision detection and the piano display.";
    case kTipRangeHandle: return "Drag up or down to set how far this dynamic band can move. The shaded area shows the range; the moving line shows what it's doing now.";
    case kTipGrab: return "A peak in the spectrum. Click to create a band right on it, then drag up or down to cut or boost.";
    case kTipMeter: return "Output level, left and right. The number is the highest peak; click to reset it.";
    case kTipActiveFreq: return "In MIDI Learn: assign a controller to the frequency of whichever band is selected, so one knob can work on every band.";
    case kTipActiveGain: return "In MIDI Learn: assign a controller to the gain of whichever band is selected.";
    case kTipActiveQ: return "In MIDI Learn: assign a controller to the Q of whichever band is selected.";
    default: return "";
    }
}

static const epsilon::HelpPage kHelpPages[] = {
    { "Overview",
      "Mica is a 24-band equalizer. Each band boosts, cuts or filters part of the frequency range, and the white curve shows the result of all of them together. "
      "A real-time spectrum analyser behind the curve shows what the audio is doing.\n\n"
      "To start, double-click the display (or click on the curve) to add a band and drag it into place. The band's controls appear in the panel below.\n\n"
      "Hover over anything for a moment to see what it does. Turn the tips off at the bottom of this window." },
    { "Working with bands",
      "ADD: double-click empty space, or click on the curve itself. Up to 24 bands. With the analyser on, click a highlighted peak to grab it as a band.\n\n"
      "MOVE: drag a dot. Left/right sets the frequency, up/down the gain (for cuts, the resonance). The mouse wheel over a dot changes Q.\n\n"
      "SELECT SEVERAL: Shift-click dots, or drag a box across empty space. Dragging any selected dot moves them all.\n\n"
      "SOLO: hold the band's SOLO button to hear only what it affects.\n\n"
      "SWITCH OFF / DELETE: Alt-click a dot to switch it off, right-click it (or press Delete) to remove it." },
    { "Shapes & slopes",
      "BELL, LOW and HIGH SHELF, LOW and HIGH CUT, NOTCH, BAND PASS, TILT SHELF (tilts around the frequency), FLAT TILT (tilts the whole spectrum) and ALL PASS (phase only).\n\n"
      "SLOPE: shelves, cuts, band passes, tilt shelves and all passes go from 6 to 96 dB per octave; cuts also have BRICKWALL, an extremely steep cut.\n\n"
      "CHANNEL: each band can work on both channels, only left (or mid), or only right (or side)." },
    { "Dynamic EQ",
      "Switch DYNAMIC on and a band moves only when its part of the spectrum gets loud. THRESHOLD sets where it starts, RANGE how far it can go "
      "(negative cuts when loud, positive boosts), ATTACK and RELEASE how fast. The shaded area in the display shows the range; drag the diamond to change it.\n\n"
      "SIDECHAIN EXTERNAL lets another track drive the band. FREE lets it listen to different frequencies than the ones it changes.\n\n"
      "SPECTRAL: the band treats every frequency inside it on its own, so only the resonances that poke out are pulled down. It adds 21 ms of latency." },
    { "Processing & character",
      "ZERO LATENCY: like an analog EQ, no delay. NATURAL PHASE: the same, but level and phase match an analog EQ right up to 20 kHz, where digital filters otherwise squash; 4 ms of latency. "
      "LINEAR PHASE: no phase shift at all, for mastering and parallel or mid/side work; 64 to 256 ms of latency. Your DAW compensates latency.\n\n"
      "CHARACTER: GENTLE and WARM add analog-style saturation to what the EQ changes.\n\n"
      "AUTO GAIN keeps the loudness steady; GAIN SCALE scales every band's gain at once; INVERT flips polarity." },
    { "Sketch & Match",
      "EQ SKETCH: click SKETCH and draw the curve you want across the display. When you let go, Mica places bands to match it. Draw again to refine; click SKETCH to finish.\n\n"
      "EQ MATCH: route a reference track to Mica's sidechain. Click MATCH, then LISTEN, and play both your track and the reference for 10 seconds or more. "
      "APPLY builds the bands that make your track sound like the reference. Then use GAIN SCALE to choose how much of the match you want (50% is often plenty)." },
    { "Analyser",
      "PRE shows the incoming signal, POST the result after the EQ, BOTH shows them together. The ANALYZER menu above the display adds the EXTERNAL spectrum (the sidechain), "
      "range, speed, resolution and tilt, FREEZE, COLLISION detection (red where your track and the sidechain fight for the same frequencies) and the PIANO display.\n\n"
      "SPECTRUM GRAB: peaks in the spectrum light up as you move over them; click one to create a band right on it." },
    { "A/B, MIDI & shortcuts",
      "A/B: two complete settings; COPY copies the current one to the other. Every edit is an undo step (Cmd/Ctrl-Z, Cmd/Ctrl-Shift-Z).\n\n"
      "MIDI LEARN: click LEARN, click a control, move a controller. The ACTIVE BAND chips assign a controller to whichever band you select. Right-click unassigns.\n\n"
      "Double-click a knob to type a value (1.5k, -3, 0.7). Cmd/Ctrl-click resets. Shift-drag is fine adjustment. Delete removes the selected bands. Esc closes this window." },
};
static constexpr int kNumHelpPages = sizeof(kHelpPages) / sizeof(kHelpPages[0]);

} // namespace mica
