# Drone Synth

A 4-voice drone synthesizer for the Teensy 4.x. Each voice has its own pitch knob and on/off toggle, every pitch is quantized to a scale so any knob combination sounds consonant, and one Space knob sweeps from dry through reverb into a feedback delay. A toggle switches everything into an arpeggiator.

Work in progress — currently prototyped on a Teensy 4.1 with USB audio out.

---

## Sound

Each voice is two detuned band-limited saws plus a sine one octave down, with a touch of pink noise, through a state-variable lowpass and a slow envelope (2 s swell in, 3 s fade out).

- **Scale quantizing:** pitch knobs snap to C minor pentatonic over 3 octaves from C2, with a short glide between notes.
- **Motion:** each voice's filter cutoff and detune drift on their own slow LFOs (roughly 15–35 s per cycle), so the drone keeps evolving without a knob.
- **Space:** 0–60% fades in Freeverb; 60–100% brings in a 450 ms delay with rising feedback, and the echoes also feed the reverb.
- **Arpeggiator:** cycles through the voices that are switched on, playing short plucks every 225 ms (half the delay time, so echoes land in time), jumping up an octave every other cycle. Voice toggles add or remove notes while it runs. Switching it off swells everything back into a drone.

---

## Hardware

| Control | Pin | Notes |
|---------|-----|-------|
| Voice 1 pitch | 14 (A0) | 10k linear pot |
| Voice 2 pitch | 15 (A1) | 10k linear pot |
| Voice 3 pitch | 16 (A2) | 10k linear pot |
| Voice 4 pitch | 17 (A3) | 10k linear pot |
| Space | 22 (A8) | 10k linear pot |
| Voice 1–4 on/off | 0, 1, 2, 3 | Toggle switch → GND |
| Arp on/off | 4 | Toggle switch → GND |
| Tempo (optional) | 24 (A10) | Set `TEMPO_PIN = 24` to enable |

- **Pots:** outer legs to **3.3V** and **GND**, wiper to the pin. Don't use 5V — Teensy 4.x analog inputs aren't 5V tolerant. Use linear (B-marked) pots, not audio taper.
- **Switches:** one side to the pin, the other to GND. The sketch uses `INPUT_PULLUP`, so ON reads LOW. No resistors needed. Voices follow switch position, including at power-up.

### Audio out

Currently **USB audio**: the Teensy shows up as an audio input device on the computer (monitor it in a DAW with input monitoring on).

For the Rev D audio shield instead, swap `AudioOutputUSB` for `AudioOutputI2S` + `AudioControlSGTL5000` (see the comment in the sketch) and wire shield → Teensy: 3.3V, GND, 7, 18, 19, 20, 21, 23. Keep those jumpers short, especially MCLK on 23. Pins 0–4, 14–17, 22 and 24 are all free with the shield.

### Power (planned)

LiPo → TP4056 (B+/B−) → OUT+ → power switch → MT3608 boost set to **5.0 V** → Teensy VIN. Set the MT3608 trimmer with a multimeter before connecting. Cut the Teensy's VIN–VUSB trace (or add a Schottky diode on VIN) so USB doesn't back-feed the boost. Add 100–470 µF + 0.1 µF across the boost output to keep switching noise out of the audio.

---

## Building

1. Arduino IDE 2.x with the Teensy board package (`https://www.pjrc.com/teensy/package_teensy_index.json`). The Audio and Bounce libraries come with it.
2. **Tools → Board:** Teensy 4.1 (or 4.0)
3. **Tools → USB Type:** Audio, or Serial + MIDI + Audio if you want Serial debugging. Without an Audio USB type the sketch won't compile (`AudioOutputUSB`).

---

## Tweaking

All in the constants at the top of `drone_synth.ino`:

| Setting | What it does |
|---------|--------------|
| `SCALE`, `ROOT_MIDI` | Scale intervals and root note (36 = C2; +12 = one octave up) |
| `OCTAVES` | Knob range. 4 extends higher; 5+ starts to feel crowded |
| `GLIDE` | Slide speed between notes (smaller = slower) |
| `NOISE_LEVEL` | Breathy noise per voice (0 = off, ~0.08 = clearly audible) |
| `FILTER_BASE`, `FILTER_DEPTH` | Filter drift center and range. Raise `FILTER_BASE` if high notes sound dull |
| `DETUNE_CENTER`, `DETUNE_DEPTH` | Saw detune and its drift (beating speed) |
| `DELAY_MS` | Echo time |
| `ARP_STEP_MS`, `ARP_MIN_MS`, `ARP_MAX_MS` | Arp step time, without and with a tempo knob |
| `ARP_GATE` | How long each arp note is held, as a fraction of a step |
| `ARP_OCTAVE_JUMP` | Octave-up on every other arp cycle |

Gain staging is set so 4 voices + full Space don't clip. If you raise levels, watch the input meter in your DAW.

---

## Ideas / next

- 3-position scale switch (major pentatonic / minor pentatonic / whole-tone)
- Key select: DIP switch, or I²C OLED + rotary encoder sharing pins 18/19
- Tempo knob on pin 24
- Optional soft clipper at the end of the chain
- Speaker out via MAX98357A (shares the shield's I²S lines)
