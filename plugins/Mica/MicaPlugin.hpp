// Mica — DPF plugin wrapper: parameters, programs, sidechain, MIDI learn (including the active band), solo,
// latency, analyser access for the UI. License: MIT.
// Declared in a header so the UI can reach the analyser ring through DPF's direct access.
#pragma once
#include "DistrhoPlugin.hpp"
#include "ParamDefs.hpp"
#include "Presets.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

START_NAMESPACE_DISTRHO

using namespace mica;

class MicaPlugin : public Plugin {
public:
    MicaPlugin() : Plugin(kParamCount, kNumPresets, 2) {
        for (int& m : ccMap) m = -1;
        engine.setSampleRate((float)getSampleRate());
    }

protected:
    const char* getLabel() const override { return "Mica"; }
    const char* getDescription() const override {
        return "24-band stereo equalizer: ten band shapes with slopes to 96 dB/oct and brickwall cuts, dynamic EQ and spectral "
               "dynamics with external sidechain, zero-latency, natural-phase or linear-phase processing, character modes, "
               "stereo or mid/side with per-band channel selection, and a real-time spectrum analyser.";
    }
    const char* getMaker() const override { return "Epsilon Audio"; }
    const char* getHomePage() const override { return "https://epsilonrecords.com"; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(2, 1, 0); }

    void initAudioPort(bool input, uint32_t index, AudioPort& port) override {
        if (input && index >= 2) {                 // inputs 3-4: external sidechain (dynamic EQ trigger, analyser, EQ Match)
            port.hints = kAudioPortIsSidechain;
            port.name = index == 2 ? "Sidechain Left" : "Sidechain Right";
            port.symbol = index == 2 ? "sc_left" : "sc_right";
            port.groupId = kPortGroupNone;
            return;
        }
        port.groupId = kPortGroupStereo;
        Plugin::initAudioPort(input, index, port);
    }

    void initParameter(uint32_t i, Parameter& prm) override {
        const ParamDef& d = paramDef(i);
        prm.hints = (d.hints & kHintOutput) ? kParameterIsOutput
                  : (((d.hints & kHintNoAuto) ? 0 : kParameterIsAutomatable)
                     | ((d.hints & kHintInteger) ? kParameterIsInteger : 0)
                     | ((d.hints & kHintBoolean) ? kParameterIsBoolean : 0)
                     | ((d.hints & kHintLog) ? kParameterIsLogarithmic : 0));
        prm.name = d.name; prm.symbol = d.sym; prm.unit = d.unit;
        prm.ranges.min = d.min; prm.ranges.max = d.max;
        Engine def; prm.ranges.def = std::min(d.max, std::max(d.min, def.p[i]));
        int n = 0;
        if (const char* const* labels = enumLabels(i, n)) {
            ParameterEnumerationValue* ev = new ParameterEnumerationValue[n];
            for (int k = 0; k < n; ++k) { ev[k].value = (float)k; ev[k].label = labels[k]; }
            prm.enumValues.count = (uint8_t)n; prm.enumValues.restrictedMode = true; prm.enumValues.values = ev;
        }
    }

    // state: "midimap" is saved with the session; "cmd" carries one-shot UI requests
    void initState(uint32_t index, State& state) override {
        if (index == 0) { state.key = "midimap"; state.defaultValue = ""; state.label = "MIDI Learn map"; state.hints = kStateIsHostReadable; }
        else            { state.key = "cmd";     state.defaultValue = ""; state.label = "UI command";     state.hints = kStateIsOnlyForDSP; }
    }
    String getState(const char* key) const override {
        if (std::strcmp(key, "midimap") == 0) return String(serializeMap().c_str());
        return String();
    }
    void setState(const char* key, const char* value) override {
        if (std::strcmp(key, "midimap") == 0) { parseMap(value); return; }
        if (std::strcmp(key, "cmd") == 0 && value && *value) {
            if (std::strncmp(value, "learn:", 6) == 0)      learnTarget = std::atoi(value + 6);
            else if (std::strcmp(value, "cancel") == 0)     learnTarget = -1;
            else if (std::strncmp(value, "unmap:", 6) == 0) { const int p = std::atoi(value + 6); for (int& m : ccMap) if (m == p) m = -1; }
            else if (std::strcmp(value, "clear") == 0)      { for (int& m : ccMap) m = -1; }
            else if (std::strncmp(value, "solo:", 5) == 0)  engine.soloBand = std::atoi(value + 5);
            else if (std::strncmp(value, "active:", 7) == 0) activeBand = std::atoi(value + 7);
        }
    }

    void initProgramName(uint32_t i, String& name) override { name = presets()[i].name; }
    void loadProgram(uint32_t i) override {
        for (uint32_t k = 0; k < kParamCount; ++k) if (isSetting(k)) engine.p[k] = presets()[i].v[k];
    }

    float getParameterValue(uint32_t i) const override { return i == kOutLearn ? learnEcho : engine.p[i]; }
    void setParameterValue(uint32_t i, float v) override {
        if (!(paramDef(i).hints & kHintOutput)) engine.p[i] = v;
    }

    void activate() override { engine.setSampleRate((float)getSampleRate()); setLatency(engine.latency()); }
    void sampleRateChanged(double sr) override { engine.setSampleRate((float)sr); }

    void run(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* ev, uint32_t evCount) override {
        const bool midiOn = engine.p[kMidiOn] > 0.5f;
        uint32_t pos = 0;
        // the processing mode decides the latency; report changes before processing this block
        const uint32_t want = engine.wantedLatency();
        if (want != reportedLatency) { reportedLatency = want; setLatency(want); }
        for (uint32_t e = 0; e <= evCount; ++e) {
            const uint32_t until = e < evCount ? std::min(ev[e].frame, frames) : frames;
            if (until > pos) {
                engine.process(inputs[0] + pos, inputs[1] + pos, inputs[2] + pos, inputs[3] + pos, outputs[0] + pos, outputs[1] + pos, until - pos);
                pos = until;
            }
            if (e == evCount) break;
            if (!midiOn || ev[e].size > 3) continue;
            const uint8_t* d = ev[e].data;
            const uint8_t st = d[0] & 0xF0;
            if (st == 0xB0) {
                const int cc = d[1] & 0x7F, val = d[2] & 0x7F;
                if (learnTarget >= 0 && validTarget(learnTarget)) {               // MIDI learn: bind this controller
                    for (int& m : ccMap) if (m == learnTarget) m = -1;
                    ccMap[cc] = learnTarget;
                    learnSerial = (learnSerial + 1) % 8;
                    const int echoTarget = learnTarget >= kActiveTarget ? (int)kParamCount + (learnTarget - kActiveTarget) : learnTarget;   // active-band targets echo as kParamCount + 0..2
                    learnEcho = (float)(learnSerial * 1000000 + echoTarget * 128 + cc + 1);
                    learnTarget = -1;
                }
                int prm = ccMap[cc];
                if (prm >= kActiveTarget) {                                         // the band selected in the editor
                    prm = (activeBand >= 0 && activeBand < kNumBands && engine.p[bp(activeBand, kBUsed)] > 0.5f) ? (int)bp(activeBand, activeField(prm - kActiveTarget)) : -1;
                }
                if (prm >= 0) {
                    const float v = fromNorm((uint32_t)prm, val / 127.f);
                    engine.p[prm] = v;
                    requestParameterValueChange((uint32_t)prm, v);
                }
            }
        }
    }

public:
    // MIDI Learn targets that follow the band selected in the editor: kActiveTarget + 0 (frequency), 1 (gain), 2 (Q)
    static constexpr int kActiveTarget = 100000;
    static int activeField(int i) { return i == 0 ? kBFreq : (i == 1 ? kBGain : kBQ); }
    static bool validTarget(int t) { return (t >= 0 && t < (int)kParamCount && isSetting((uint32_t)t)) || (t >= kActiveTarget && t < kActiveTarget + 3); }
    Engine engine;                      // public for the UI's direct access to the analyser ring

private:
    uint32_t reportedLatency = 0;
    std::string serializeMap() const {
        std::string s; char b[24];
        for (int cc = 0; cc < 128; ++cc) if (ccMap[cc] >= 0) { std::snprintf(b, sizeof b, "%d:%d;", cc, ccMap[cc]); s += b; }
        return s;
    }
    void parseMap(const char* v) {
        for (int& m : ccMap) m = -1;
        if (!v) return;
        const char* q = v;
        while (*q) {
            int cc = -1, prm = -1;
            if (std::sscanf(q, "%d:%d", &cc, &prm) == 2 && cc >= 0 && cc < 128 && validTarget(prm)) ccMap[cc] = prm;
            const char* semi = std::strchr(q, ';');
            if (!semi) break;
            q = semi + 1;
        }
    }

    int ccMap[128];
    int learnTarget = -1, learnSerial = 0, activeBand = -1;
    float learnEcho = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MicaPlugin)
};


END_NAMESPACE_DISTRHO
