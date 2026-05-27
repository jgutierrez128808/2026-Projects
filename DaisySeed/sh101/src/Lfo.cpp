#include "Lfo.h"
#include <cmath>
#include <cstdlib>

using namespace sh101;

void Lfo::Init(float sample_rate)
{
    sr_    = sample_rate;
    phase_ = 0.0f;
    SetRate(1.0f);
}

void Lfo::SetRate(float hz)
{
    inc_ = hz / sr_;
}

static inline float frand()
{
    return (rand() / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
}

float Lfo::Process()
{
    phase_ += inc_;
    bool wrapped = phase_ >= 1.0f;
    if(wrapped)
        phase_ -= 1.0f;

    switch(shape_)
    {
        case LfoShape::Triangle: value_ = 4.0f * fabsf(phase_ - 0.5f) - 1.0f; break;
        case LfoShape::Square:   value_ = phase_ < 0.5f ? 1.0f : -1.0f; break;
        case LfoShape::RampUp:   value_ = 2.0f * phase_ - 1.0f; break;
        case LfoShape::RampDown: value_ = 1.0f - 2.0f * phase_; break;
        case LfoShape::Random:
            if(wrapped)
                rand_ = frand();
            value_ = rand_;
            break;
    }
    return value_;
}
