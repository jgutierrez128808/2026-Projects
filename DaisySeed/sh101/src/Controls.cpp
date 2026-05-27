#include "Controls.h"
#include <cmath>

using namespace daisy;

namespace {

// One pot, 0..1, from board ADC channel `mux` (= which 4051) and `ch` (0..7).
inline float Knob(DaisySeed& hw, int mux, int ch)
{
    return hw.adc.GetMuxFloat(mux, ch);
}

// Quantize a 0..1 pot reading to one of `n` discrete switch positions.
inline int Switch(float v, int n)
{
    int i = static_cast<int>(v * n);
    return i < 0 ? 0 : (i >= n ? n - 1 : i);
}

// Map 0..1 to an exponential range (good for time/rate controls).
inline float Expo(float v, float lo, float hi)
{
    return lo * powf(hi / lo, v);
}

} // namespace

void sh101::ProcessControls(DaisySeed& hw, SynthParams& p)
{
    // --- Mux 0: VCO + source mixer ---
    p.tune        = (Knob(hw, 0, 0) - 0.5f) * 24.0f; // +/- 12 semitones
    p.pulseWidth  = 0.05f + 0.90f * Knob(hw, 0, 1);
    p.pwmDepth    = Knob(hw, 0, 2);
    p.sawLevel    = Knob(hw, 0, 3);
    p.pulseLevel  = Knob(hw, 0, 4);
    p.subLevel    = Knob(hw, 0, 5);
    p.noiseLevel  = Knob(hw, 0, 6);
    p.vcoLfoDepth = Knob(hw, 0, 7);

    // --- Mux 1: VCF + envelope ---
    p.cutoff      = Knob(hw, 1, 0);
    p.resonance   = Knob(hw, 1, 1);
    p.envAmount   = Knob(hw, 1, 2);
    p.vcfLfoDepth = Knob(hw, 1, 3);
    p.keyTrack    = Knob(hw, 1, 4);
    p.attack      = Expo(Knob(hw, 1, 5), 0.001f, 5.0f);
    p.decay       = Expo(Knob(hw, 1, 6), 0.001f, 5.0f);
    p.sustain     = Knob(hw, 1, 7);

    // --- Mux 2: LFO + release + switches ---
    p.lfoRate  = Expo(Knob(hw, 2, 0), 0.05f, 30.0f);
    p.release  = Expo(Knob(hw, 2, 1), 0.001f, 5.0f);
    p.octave   = Switch(Knob(hw, 2, 2), 4) - 1; // 16'/8'/4'/2' -> -1..+2
    p.subShape = static_cast<SubShape>(Switch(Knob(hw, 2, 3), 3));
    p.pwmSource = static_cast<PwmSource>(Switch(Knob(hw, 2, 4), 3));
    p.lfoShape = static_cast<LfoShape>(Switch(Knob(hw, 2, 5), 5));
    p.vcaMode  = static_cast<VcaMode>(Switch(Knob(hw, 2, 6), 2));
}
