// Run pink-ish noise through every factory preset; report level change and check output is finite.
#include "../plugins/Mica/ParamDefs.hpp"
#include "../plugins/Mica/Presets.hpp"
#include <cstdio>
#include <vector>
using namespace mica;
int main() {
    const int SR = 48000, N = SR * 2; int bad = 0;
    std::vector<float> x(N); uint32_t r = 5; float b0 = 0, b1 = 0, b2 = 0;
    for (int i = 0; i < N; ++i) { r = r * 1664525u + 1013904223u; const float w = (int32_t)r * (1.f / 2147483648.f);
        b0 = 0.99765f * b0 + w * 0.0990460f; b1 = 0.96300f * b1 + w * 0.2965164f; b2 = 0.57000f * b2 + w * 1.0526913f; x[i] = 0.2f * (b0 + b1 + b2 + w * 0.1848f); }
    double si = 0; for (float v : x) si += v * v; const float inR = (float)std::sqrt(si / N);
    for (uint32_t p = 0; p < kNumPresets; ++p) {
        Engine e; e.setSampleRate(SR); for (uint32_t k = 0; k < kParamCount; ++k) e.p[k] = presets()[p].v[k]; e.reset();
        std::vector<float> L(N), R(N); for (int i = 0; i < N; i += 512) e.process(x.data() + i, x.data() + i, L.data() + i, R.data() + i, std::min(512, N - i));
        double s = 0; bool nf = false; for (int i = N / 2; i < N; ++i) { s += L[i] * L[i]; nf |= !std::isfinite(L[i]); }
        const float rel = 20 * std::log10(std::sqrt(s / (N / 2)) / inR + 1e-9f);
        int used = 0; for (int b = 0; b < kNumBands; ++b) used += presets()[p].v[bp(b, kBUsed)] > 0.5f;
        const bool ok = !nf && rel > -15 && rel < 8; bad += !ok;
        printf("%-18s %-19s %d bands  %s  level %+5.1f dB %s\n", presets()[p].name, presets()[p].category, used, e.p[kProcMode] > 1.5f ? "natural" : (e.p[kProcMode] > 0.5f ? "linear " : (e.latency() ? "zero+sp" : "zero   ")), rel, ok ? "" : "<-- CHECK");
    }
    printf("%u presets, %d flagged\n", kNumPresets, bad); return bad;
}
