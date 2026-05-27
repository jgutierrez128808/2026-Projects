#include "Voice.h"

using namespace sh101;
using namespace daisysp;

void Voice::Init(float sr)
{
    sr_ = sr;

    saw_.Init(sr);
    saw_.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);

    pulse_.Init(sr);
    pulse_.SetWaveform(Oscillator::WAVE_POLYBLEP_SQUARE);

    sub_.Init(sr);
    sub_.SetWaveform(Oscillator::WAVE_POLYBLEP_SQUARE);

    noise_.Init();
    env_.Init(sr);
    lfo_.Init(sr);
}

float Voice::Process()
{
    const SynthParams& p = *p_;

    // --- LFO ---
    lfo_.SetShape(p.lfoShape);
    lfo_.SetRate(p.lfoRate);
    lfo_val_ = lfo_.Process();

    // --- Envelope (shared by VCF cutoff CV and VCA) ---
    env_.SetTime(ADSR_SEG_ATTACK, p.attack);
    env_.SetTime(ADSR_SEG_DECAY, p.decay);
    env_.SetSustainLevel(p.sustain);
    env_.SetTime(ADSR_SEG_RELEASE, p.release);
    env_val_ = env_.Process(gate_);

    // --- Pitch ---
    float semis = note_ + bend_ + p.tune + 12.0f * static_cast<float>(p.octave);
    semis += (p.vcoLfoDepth * 7.0f) * lfo_val_; // LFO -> pitch, up to ~7 semitones
    float freq = mtof(semis);

    // --- Pulse width modulation ---
    float pw = p.pulseWidth;
    if(p.pwmSource == PwmSource::Lfo)
        pw += p.pwmDepth * lfo_val_ * 0.45f;
    else if(p.pwmSource == PwmSource::Env)
        pw += p.pwmDepth * env_val_ * 0.45f;
    pw = fclamp(pw, 0.05f, 0.95f);

    // --- Sub oscillator frequency (-1 or -2 octaves) ---
    float subFreq = freq * 0.5f;
    if(p.subShape == SubShape::Oct2Square || p.subShape == SubShape::Oct2Pulse)
        subFreq = freq * 0.25f;

    // --- Oscillators ---
    saw_.SetFreq(freq);
    pulse_.SetFreq(freq);
    pulse_.SetPw(pw);
    sub_.SetFreq(subFreq);
    sub_.SetPw(p.subShape == SubShape::Oct2Pulse ? 0.25f : 0.5f);

    float s = 0.0f;
    s += saw_.Process() * p.sawLevel;
    s += pulse_.Process() * p.pulseLevel;
    s += sub_.Process() * p.subLevel;
    s += noise_.Process() * p.noiseLevel;

    return s * 0.5f; // headroom into the analog VCF
}
