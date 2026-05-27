#pragma once
#include "Params.h"

namespace sh101 {

// Single global modulation LFO. Output is bipolar (-1..1).
class Lfo
{
  public:
    void  Init(float sample_rate);
    void  SetRate(float hz);
    void  SetShape(LfoShape shape) { shape_ = shape; }
    float Process();
    float Value() const { return value_; }

  private:
    float    sr_    = 48000.0f;
    float    phase_ = 0.0f;
    float    inc_   = 0.0f;
    float    value_ = 0.0f;
    float    rand_  = 0.0f; // held sample for Random shape
    LfoShape shape_ = LfoShape::Triangle;
};

} // namespace sh101
