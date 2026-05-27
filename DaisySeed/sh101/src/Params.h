#pragma once
#include <cstdint>

namespace sh101 {

enum class LfoShape : uint8_t { Triangle, Square, RampUp, RampDown, Random };
enum class PwmSource : uint8_t { Manual, Lfo, Env };
enum class SubShape : uint8_t { Oct1Square, Oct2Square, Oct2Pulse };
enum class VcaMode : uint8_t { Env, Gate };

// All live parameters. Written by the control loop (panel + MIDI),
// read by the audio callback. Plain floats: 32-bit aligned loads/stores
// are atomic on the Cortex-M7, so no locking is needed for this mono synth.
struct SynthParams
{
    // VCO
    float tune   = 0.0f; // master tune, semitones
    int   octave = 0;    // range switch, octaves relative to 8'

    // Source mixer levels, 0..1
    float sawLevel   = 0.8f;
    float pulseLevel = 0.0f;
    float subLevel   = 0.0f;
    float noiseLevel = 0.0f;

    // Pulse width / PWM
    float     pulseWidth = 0.5f; // 0.05..0.95
    PwmSource pwmSource  = PwmSource::Manual;
    float     pwmDepth   = 0.0f; // 0..1

    // Sub oscillator
    SubShape subShape = SubShape::Oct1Square;

    // VCO pitch modulation from LFO, 0..1 (scaled to semitones in Voice)
    float vcoLfoDepth = 0.0f;

    // LFO
    LfoShape lfoShape = LfoShape::Triangle;
    float    lfoRate  = 1.0f; // Hz

    // Envelope (shared by analog VCF cutoff CV and the digital VCA)
    float attack  = 0.01f; // s
    float decay   = 0.20f; // s
    float sustain = 0.80f; // 0..1
    float release = 0.20f; // s

    // VCF control values (sent out as CV to the analog filter), 0..1
    float cutoff      = 1.0f;
    float resonance   = 0.0f;
    float envAmount   = 0.0f; // env -> cutoff
    float vcfLfoDepth = 0.0f; // LFO -> cutoff
    float keyTrack    = 0.0f; // keyboard follow

    // VCA
    VcaMode vcaMode = VcaMode::Env;
};

} // namespace sh101
