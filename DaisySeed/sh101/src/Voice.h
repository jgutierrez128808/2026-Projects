#pragma once
#include "daisysp.h"
#include "Params.h"
#include "Lfo.h"

namespace sh101 {

// Monophonic SH-101 voice. Produces the pre-filter source mix that is sent
// out to the analog VCF. The shared ADSR and LFO are advanced here; their
// values are exposed so the main loop can build the cutoff CV and drive the
// digital VCA on the filtered signal returning from the analog filter.
class Voice
{
  public:
    void Init(float sample_rate);
    void SetParams(const SynthParams* p) { p_ = p; }

    void NoteOn(float note) { note_ = note; gate_ = true; }
    void NoteOff() { gate_ = false; }
    void SetBend(float semitones) { bend_ = semitones; }

    // One sample of the source mix. Call once per audio frame.
    float Process();

    // Valid after Process():
    float Env() const { return env_val_; }
    float LfoValue() const { return lfo_val_; }
    float Note() const { return note_; }
    bool  Gate() const { return gate_; }

  private:
    const SynthParams* p_  = nullptr;
    float              sr_ = 48000.0f;

    daisysp::Oscillator saw_;
    daisysp::Oscillator pulse_;
    daisysp::Oscillator sub_;
    daisysp::WhiteNoise noise_;
    daisysp::Adsr       env_;
    Lfo                 lfo_;

    float note_    = 60.0f;
    float bend_    = 0.0f;
    bool  gate_    = false;
    float env_val_ = 0.0f;
    float lfo_val_ = 0.0f;
};

} // namespace sh101
