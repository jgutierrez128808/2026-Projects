#pragma once
#include "daisy_seed.h"
#include "Params.h"

namespace sh101 {

// Reads the panel pots/switches (scanned through the analog multiplexers)
// and writes mapped, ranged values into params. Call once per control loop.
void ProcessControls(daisy::DaisySeed& hw, SynthParams& params);

} // namespace sh101
