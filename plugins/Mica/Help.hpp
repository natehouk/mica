// Mica — hover tips and the in-plugin help pages. License: MIT.
#pragma once
#include "MicaEngine.hpp"

namespace mica {

static const char* const kManualURL = "https://github.com/natehouk/mica/blob/main/docs/MANUAL.md";

inline const char* paramTip(uint32_t p) {
    switch (p) {
    case kProcMode: return "ZERO LATENCY: minimum-phase filters with no delay, like an analog EQ; best while tracking and for most mixing. LINEAR PHASE: no phase shift at all (useful on parallel or mid/side material and for mastering), at the cost of latency, which the host compensates.";
    case kLinQuality: return "Length of the linear-phase filter. Higher quality is more accurate at low frequencies but adds more latency: Low about 64 ms, Medium about 128 ms, High about 256 ms at 48 kHz.";
    case kChannelMode: return "STEREO works on left and right. MID/SIDE works on the centre (mid) and stereo (side) parts instead; each band can then be set to mid only or side only.";
    case kOutGain: return "Output level after the EQ. Use it to match the loudness of the bypassed signal when comparing.";
    case kAnalyzer: return "Real-time spectrum analyser behind the curve. PRE shows the incoming signal, POST shows the result after the EQ, OFF hides it.";
    case kRange: return "Vertical scale of the display: ±6 dB for subtle mastering moves, ±12 dB for mixing, ±30 dB for sound design and steep cuts.";
    case kMidiOn: return "Turns MIDI control on or off. When off, learned controllers are ignored.";
    default: break;
    }
    switch (((int)p - (int)kBandFirst) % kBandFields) {
    case kBOn: return "Switches this band in or out without deleting it. Switching fades smoothly, so it is safe while playing. Alt-click a dot in the display does the same.";
    case kBShape: return "BELL boosts or cuts around the frequency. LOW/HIGH SHELF raises or lowers everything below/above it. LOW CUT and HIGH CUT remove everything below/above it (high-pass and low-pass filters).";
    case kBFreq: return "Centre (bell) or corner (shelf, cut) frequency, 10 Hz to 30 kHz. Drag the band's dot left and right to set it.";
    case kBGain: return "Boost or cut in dB (bells and shelves). Drag the dot up and down. Cut filters have no gain; their dot sets the resonance instead.";
    case kBQ: return "Bandwidth. High Q is narrow and surgical, low Q is broad and musical. For shelves it shapes the knee; for cuts it adds a resonant peak. Mouse wheel over a dot changes it.";
    case kBSlope: return "Steepness of a cut filter: 6, 12, 24 or 48 dB per octave.";
    case kBPlace: return "Which channel the band works on: both (STEREO), only left / mid, or only right / side. Mid and side apply in Mid/Side channel mode.";
    default: return "";
    }
}

enum TipTarget { kTipUndo, kTipRedo, kTipLearn, kTipMidi, kTipHelp, kTipA, kTipB, kTipCopy, kTipPrev, kTipPresets, kTipNext,
                 kTipDot, kTipDisplay, kTipDelete, kNumTipTargets };
inline const char* targetTitle(int t) {
    static const char* const n[kNumTipTargets] = { "Undo", "Redo", "MIDI Learn", "MIDI", "Help", "Setting A", "Setting B", "Copy A/B",
                                                   "Previous preset", "Presets", "Next preset", "EQ band", "EQ display", "Delete band" };
    return n[t];
}
inline const char* targetTip(int t) {
    switch (t) {
    case kTipUndo: return "Steps back through your edits, including new and deleted bands, preset loads and A/B switches. Cmd/Ctrl-Z.";
    case kTipRedo: return "Re-applies an edit you undid. Cmd/Ctrl-Shift-Z or Cmd/Ctrl-Y.";
    case kTipLearn: return "Click, then click any control and move a knob or fader on your MIDI controller to assign it. Band knobs learn the band that is selected. Right-click a control to unassign.";
    case kTipMidi: return paramTip(kMidiOn);
    case kTipHelp: return "Opens the help pages: working with bands, shapes, processing modes, mid/side, the analyser, A/B, MIDI Learn and shortcuts.";
    case kTipA: case kTipB: return "Mica keeps two complete settings, A and B. Click the other letter to switch and compare by ear.";
    case kTipCopy: return "Copies the current setting to the other slot.";
    case kTipPrev: return "Load the previous preset. The mouse wheel over the preset name also steps through presets.";
    case kTipPresets: return "Open the preset browser. A * after the name means you have changed it.";
    case kTipNext: return "Load the next preset.";
    case kTipDot: return "Drag to move: left/right = frequency, up/down = gain (resonance for cut filters). Mouse wheel = Q. Shift-click adds to the selection, Alt-click switches the band on/off, right-click (or Delete) removes it.";
    case kTipDisplay: return "Double-click, or click on the curve, to add a band. Drag across empty space to select several bands; dragging any selected dot then moves them all.";
    case kTipDelete: return "Removes the selected band(s). Undo brings them back.";
    default: return "";
    }
}

struct HelpPage { const char* title; const char* body; };
static const HelpPage kHelpPages[] = {
    { "Overview",
      "Mica is a 24-band equalizer. Each band boosts, cuts or filters part of the frequency range, and the white curve shows the result of all of them together. "
      "A real-time spectrum analyser behind the curve shows what the audio is doing.\n\n"
      "To start, double-click the display (or click on the curve) to add a band and drag it into place. The band's controls appear in the panel below.\n\n"
      "Hover over anything for a moment to see what it does. Turn the tips off at the bottom of this window." },
    { "Working with bands",
      "ADD: double-click empty space, or click on the curve itself. Up to 24 bands.\n\n"
      "MOVE: drag a dot. Left/right sets the frequency, up/down the gain (for cut filters, the resonance). The mouse wheel over a dot changes Q.\n\n"
      "SELECT SEVERAL: Shift-click dots, or drag a box across empty space. Dragging any selected dot moves them all, and the band knobs change them all together.\n\n"
      "SWITCH OFF / DELETE: Alt-click a dot to switch it off, right-click it (or press Delete) to remove it." },
    { "Shapes",
      "BELL: a boost or cut around the frequency; Q sets the width.\n\n"
      "LOW SHELF / HIGH SHELF: raise or lower everything below / above the frequency.\n\n"
      "LOW CUT / HIGH CUT: remove everything below / above the frequency (high-pass / low-pass) at 6, 12, 24 or 48 dB per octave. "
      "Raising Q adds a resonant bump at the corner.\n\n"
      "CHANNEL: each band can work on both channels, only left (or mid), or only right (or side)." },
    { "Processing modes",
      "ZERO LATENCY uses minimum-phase filters, like an analog EQ: no delay, slight phase shifts around each band. Use it while recording and for most mixing.\n\n"
      "LINEAR PHASE builds one filter with no phase shift from the whole curve. Use it where phase shifts could smear transients or change the balance between "
      "mid and side or between parallel tracks, and for mastering. It adds latency (Low about 64 ms, Medium 128 ms, High 256 ms at 48 kHz), which your DAW compensates "
      "automatically. Higher quality is more precise in the low end." },
    { "Mid/side & analyser",
      "MID/SIDE: switch the channel mode to MID/SIDE to EQ the centre (mid) and the stereo information (side) separately; each band's channel setting then reads Mid or Side. "
      "Typical uses: cutting low end from the sides to tighten bass, or brightening only the sides to widen a mix.\n\n"
      "ANALYSER: PRE shows the incoming signal, POST the result after the EQ. It has a slight tilt so that a balanced mix looks roughly flat.\n\n"
      "RANGE: ±6 dB for subtle moves, ±12 dB for mixing, ±30 dB for big moves and steep cuts." },
    { "A/B & undo",
      "Mica holds two complete settings, A and B. Click B to switch to it (the first time, it starts as a copy of A); click A to go back. COPY copies the current setting into the other slot.\n\n"
      "Every edit is an undo step: moving, adding or deleting bands, changing controls, loading a preset, switching A/B. "
      "Use UNDO/REDO in the header, or Cmd/Ctrl-Z and Cmd/Ctrl-Shift-Z." },
    { "MIDI Learn & shortcuts",
      "MIDI LEARN: click LEARN, click a control, move a knob or fader on your controller. Band knobs learn the selected band. Right-click to unassign; LEARN again or Esc to finish.\n\n"
      "Double-click a knob to type a value (1.5k, -3, 0.7). Cmd/Ctrl-click a knob resets it. Shift-drag is fine adjustment; grab a knob's rim to turn it in a circle.\n"
      "Delete / Backspace removes the selected bands. Cmd/Ctrl-Z undo, Cmd/Ctrl-Shift-Z redo. Esc closes this window." },
};
static constexpr int kNumHelpPages = sizeof(kHelpPages) / sizeof(kHelpPages[0]);

} // namespace mica
