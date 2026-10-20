// Drone synth — 4 voices, scale quantizing, motion, Space knob, noise, arpeggiator
// Tools > Board: Teensy 4.1 (or 4.0)
// Tools > USB Type: "Audio" (or "Serial + MIDI + Audio")
//
// Wiring (pots: outer legs to 3.3V and GND, wiper to pin; toggles: pin to GND):
//   Voice 1: pot -> 14, toggle -> 0
//   Voice 2: pot -> 15, toggle -> 1
//   Voice 3: pot -> 16, toggle -> 2
//   Voice 4: pot -> 17, toggle -> 3
//   Space:   pot -> 22   (dry -> reverb -> reverb + delay)
//   Arp:     toggle -> 4 (arpeggiator on/off)
//   Tempo:   (optional, later) set TEMPO_PIN below to an analog pin

#include <Audio.h>
#include <Bounce.h>
#include <math.h>

const int NUM_VOICES = 4;
const int POT_PINS[NUM_VOICES]    = {14, 15, 16, 17};
const int BUTTON_PINS[NUM_VOICES] = {0, 1, 2, 3};
const int SPACE_PIN = 22;
const int ARP_PIN   = 4;
const int TEMPO_PIN = -1;   // -1 = no knob. Set to an analog pin (e.g. 24 on the 4.1) when you add one

// ---------- Scale ----------
const int   SCALE[]   = {0, 3, 5, 7, 10};   // minor pentatonic (semitones from root)
const int   SCALE_LEN = sizeof(SCALE) / sizeof(SCALE[0]);
const int   ROOT_MIDI = 36;                 // C2 (~65 Hz). 33 = A1, 38 = D2, etc.
const int   OCTAVES   = 3;                  // knob range
const int   NUM_NOTES = SCALE_LEN * OCTAVES + 1;
const float GLIDE     = 0.015;              // smaller = slower slide between notes

// ---------- Noise ----------
const float NOISE_LEVEL = 0.04;   // per voice. 0 = off, ~0.08 = clearly audible

// ---------- Motion (slow drift, no knob) ----------
const float MOTION_RATE_A[NUM_VOICES] = {0.047, 0.061, 0.039, 0.073};  // Hz, filter drift
const float MOTION_RATE_B[NUM_VOICES] = {0.029, 0.043, 0.053, 0.034};  // Hz, detune drift
const float FILTER_BASE   = 900.0;   // Hz, center of filter drift
const float FILTER_DEPTH  = 0.9;     // octaves up/down
const float DETUNE_CENTER = 1.004;   // osc2 ratio
const float DETUNE_DEPTH  = 0.003;   // drifts between ~1.001 and ~1.007

// ---------- Space ----------
const float DELAY_MS = 450;

// ---------- Arpeggiator ----------
const float ARP_STEP_MS     = 225;    // step time with no tempo knob (half the delay = echoes in time)
const float ARP_MIN_MS      = 80;     // tempo knob fully CW
const float ARP_MAX_MS      = 600;    // tempo knob fully CCW
const float ARP_GATE        = 0.5;    // fraction of a step the note is held
const bool  ARP_OCTAVE_JUMP = true;   // every other cycle plays an octave up

// ---------- Audio objects (per voice) ----------
AudioSynthWaveform        osc1[NUM_VOICES];
AudioSynthWaveform        osc2[NUM_VOICES];
AudioSynthWaveform        sub[NUM_VOICES];
AudioMixer4               voiceMix[NUM_VOICES];
AudioFilterStateVariable  filter[NUM_VOICES];
AudioEffectEnvelope       env[NUM_VOICES];

// ---------- Shared objects ----------
AudioSynthNoisePink       noise;     // shared, but each voice's own filter shapes it
AudioMixer4               sumMix;
AudioMixer4               fbMix;
AudioEffectDelay          delayFx;
AudioMixer4               preVerb;
AudioEffectFreeverb       verb;
AudioMixer4               outMix;
AudioOutputUSB            usbOut;
// For the audio shield later, swap usbOut for:
//   AudioOutputI2S i2sOut;  AudioControlSGTL5000 codec;
// and in setup(): codec.enable(); codec.volume(0.4);

AudioConnection* cords[NUM_VOICES * 7];

AudioConnection s1(sumMix, 0, outMix, 0);     // dry
AudioConnection s2(sumMix, 0, preVerb, 0);
AudioConnection s3(sumMix, 0, fbMix, 0);      // delay send
AudioConnection s4(fbMix, 0, delayFx, 0);
AudioConnection s5(delayFx, 0, fbMix, 1);     // feedback
AudioConnection s6(delayFx, 0, preVerb, 1);   // echoes get reverb too
AudioConnection s7(delayFx, 0, outMix, 2);    // delay wet
AudioConnection s8(preVerb, 0, verb, 0);
AudioConnection s9(verb, 0, outMix, 1);       // reverb wet
AudioConnection s10(outMix, 0, usbOut, 0);    // left
AudioConnection s11(outMix, 0, usbOut, 1);    // right

// ---------- State ----------
Bounce*  buttons[NUM_VOICES];
Bounce   arpButton(ARP_PIN, 10);
bool     droneOn[NUM_VOICES];
int      noteIdx[NUM_VOICES];
float    freqNow[NUM_VOICES];
float    phaseA[NUM_VOICES];
float    phaseB[NUM_VOICES];
float    octMul[NUM_VOICES];
bool     gateOpen[NUM_VOICES];
uint32_t gateOffAt[NUM_VOICES];
float    spaceNow = 0;
uint32_t lastMicros;

bool     arpOn = false;
int      arpPos = -1;
bool     arpUp = false;
uint32_t lastStep = 0;

const float TAU_F = 6.2831853f;

// Scale step index -> frequency
float noteToFreq(int idx) {
  int octave = idx / SCALE_LEN;
  int degree = idx % SCALE_LEN;
  int midi   = ROOT_MIDI + 12 * octave + SCALE[degree];
  return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

// Pot -> scale step, with hysteresis so it doesn't flicker between notes
int readNote(int v) {
  float pos = analogRead(POT_PINS[v]) / 1023.0f * (NUM_NOTES - 1);
  if (fabsf(pos - noteIdx[v]) > 0.6f) {
    noteIdx[v] = constrain((int)roundf(pos), 0, NUM_NOTES - 1);
  }
  return noteIdx[v];
}

// Space: 0 -> 0.6 fades in reverb, 0.6 -> 1.0 adds delay with rising feedback
void setSpace(float s) {
  float verbAmt  = min(s / 0.6f, 1.0f);
  float delayAmt = (s > 0.6f) ? (s - 0.6f) / 0.4f : 0.0f;

  outMix.gain(0, 0.6f - 0.2f * s);            // dry
  outMix.gain(1, 0.5f * verbAmt);             // reverb
  outMix.gain(2, 0.3f * delayAmt);            // delay
  fbMix.gain(0, delayAmt);                    // send into delay
  fbMix.gain(1, 0.25f + 0.35f * delayAmt);    // feedback (max 0.6)
}

// ---------- Envelope shapes ----------
void setDroneEnv(int v) {
  env[v].attack(2000);
  env[v].decay(0);
  env[v].sustain(1.0);
  env[v].release(3000);
}

void setPluckEnv(int v) {
  env[v].attack(5);
  env[v].decay(0);
  env[v].sustain(1.0);
  env[v].release(400);
}

// ---------- Arp mode switching ----------
void enterArp() {
  arpOn = true;
  for (int v = 0; v < NUM_VOICES; v++) {
    env[v].noteOff();          // drones fade out under the arp
    setPluckEnv(v);
    gateOpen[v] = false;
    octMul[v] = 1.0f;
  }
  arpPos = -1;
  arpUp = false;
  lastStep = 0;                // first step fires immediately
}

void exitArp() {
  arpOn = false;
  for (int v = 0; v < NUM_VOICES; v++) {
    setDroneEnv(v);
    gateOpen[v] = false;
    octMul[v] = 1.0f;
    if (droneOn[v]) env[v].noteOn();   // swell back into drone
    else            env[v].noteOff();
  }
}

// Next active voice after 'from', or -1 if none are on
int nextActive(int from) {
  for (int i = 1; i <= NUM_VOICES; i++) {
    int idx = ((from < 0 ? -1 : from) + i + NUM_VOICES) % NUM_VOICES;
    if (droneOn[idx]) return idx;
  }
  return -1;
}

float arpStepMs() {
  if (TEMPO_PIN < 0) return ARP_STEP_MS;
  float t = analogRead(TEMPO_PIN) / 1023.0f;
  return ARP_MAX_MS * powf(ARP_MIN_MS / ARP_MAX_MS, t);   // CW = faster
}

void runArp() {
  uint32_t now = millis();

  // Close gates whose time is up
  for (int v = 0; v < NUM_VOICES; v++) {
    if (gateOpen[v] && (int32_t)(now - gateOffAt[v]) >= 0) {
      env[v].noteOff();
      gateOpen[v] = false;
    }
  }

  float stepMs = arpStepMs();
  if (lastStep != 0 && now - lastStep < stepMs) return;
  lastStep = now;

  int n = nextActive(arpPos);
  if (n < 0) return;                       // no voices on: arp waits silently

  if (arpPos >= 0 && n <= arpPos && ARP_OCTAVE_JUMP) {
    arpUp = !arpUp;                        // wrapped around: new cycle
  }
  arpPos = n;

  octMul[n] = arpUp ? 2.0f : 1.0f;
  env[n].noteOn();
  gateOpen[n] = true;
  gateOffAt[n] = now + (uint32_t)(stepMs * ARP_GATE);
}

void setup() {
  AudioMemory(250);   // delay line needs extra memory
  analogReadAveraging(16);

  noise.amplitude(1.0);

  for (int v = 0; v < NUM_VOICES; v++) {
    int c = v * 7;
    cords[c + 0] = new AudioConnection(osc1[v], 0, voiceMix[v], 0);
    cords[c + 1] = new AudioConnection(osc2[v], 0, voiceMix[v], 1);
    cords[c + 2] = new AudioConnection(sub[v],  0, voiceMix[v], 2);
    cords[c + 3] = new AudioConnection(noise,   0, voiceMix[v], 3);
    cords[c + 4] = new AudioConnection(voiceMix[v], 0, filter[v], 0);
    cords[c + 5] = new AudioConnection(filter[v], 0, env[v], 0);
    cords[c + 6] = new AudioConnection(env[v], 0, sumMix, v);

    pinMode(BUTTON_PINS[v], INPUT_PULLUP);
    buttons[v] = new Bounce(BUTTON_PINS[v], 10);
    droneOn[v]  = false;
    gateOpen[v] = false;
    octMul[v]   = 1.0f;

    noteIdx[v] = -10;                 // forces first read to set the note
    freqNow[v] = noteToFreq(readNote(v));
    phaseA[v]  = v * 1.7f;
    phaseB[v]  = v * 2.3f;

    osc1[v].begin(0.30, freqNow[v],                 WAVEFORM_BANDLIMIT_SAWTOOTH);
    osc2[v].begin(0.30, freqNow[v] * DETUNE_CENTER, WAVEFORM_BANDLIMIT_SAWTOOTH);
    sub[v].begin (0.40, freqNow[v] * 0.5,           WAVEFORM_SINE);

    voiceMix[v].gain(0, 0.5);
    voiceMix[v].gain(1, 0.5);
    voiceMix[v].gain(2, 0.6);
    voiceMix[v].gain(3, NOISE_LEVEL);

    filter[v].frequency(FILTER_BASE);
    filter[v].resonance(0.9);

    setDroneEnv(v);

    sumMix.gain(v, 0.2);              // headroom for 4 stacked voices + effects
  }

  pinMode(ARP_PIN, INPUT_PULLUP);

  delayFx.delay(0, DELAY_MS);

  preVerb.gain(0, 0.7);
  preVerb.gain(1, 0.4);

  verb.roomsize(0.85);
  verb.damping(0.4);

  spaceNow = analogRead(SPACE_PIN) / 1023.0f;
  setSpace(spaceNow);

  // Start in whatever state the switches are in
  for (int v = 0; v < NUM_VOICES; v++) {
    droneOn[v] = (digitalRead(BUTTON_PINS[v]) == LOW);
    if (droneOn[v]) env[v].noteOn();
  }
  if (digitalRead(ARP_PIN) == LOW) enterArp();

  lastMicros = micros();
}

void loop() {
  uint32_t now = micros();
  float dt = (now - lastMicros) * 1e-6f;
  lastMicros = now;

  // Arp follows switch position
  arpButton.update();
  if (arpButton.fallingEdge()) enterArp();
  if (arpButton.risingEdge())  exitArp();

  for (int v = 0; v < NUM_VOICES; v++) {
    // Voice toggle: in drone mode it swells the voice in/out,
    // in arp mode it adds/removes the voice from the pattern
    buttons[v]->update();
    if (buttons[v]->fallingEdge() || buttons[v]->risingEdge()) {
      droneOn[v] = (buttons[v]->read() == LOW);   // switch ON = LOW
      if (!arpOn) {
        if (droneOn[v]) env[v].noteOn();
        else            env[v].noteOff();
      } else if (!droneOn[v]) {
        env[v].noteOff();
        gateOpen[v] = false;
      }
    }

    // Quantized pitch with glide
    float target = noteToFreq(readNote(v));
    freqNow[v] += (target - freqNow[v]) * GLIDE;

    // Motion: slow drifting filter + detune
    phaseA[v] += TAU_F * MOTION_RATE_A[v] * dt;
    phaseB[v] += TAU_F * MOTION_RATE_B[v] * dt;
    if (phaseA[v] > TAU_F) phaseA[v] -= TAU_F;
    if (phaseB[v] > TAU_F) phaseB[v] -= TAU_F;

    float cutoff = FILTER_BASE * powf(2.0f, FILTER_DEPTH * sinf(phaseA[v]));
    float detune = DETUNE_CENTER + DETUNE_DEPTH * sinf(phaseB[v]);
    float f = freqNow[v] * octMul[v];

    AudioNoInterrupts();
    osc1[v].frequency(f);
    osc2[v].frequency(f * detune);
    sub[v].frequency(f * 0.5);
    filter[v].frequency(cutoff);
    AudioInterrupts();
  }

  if (arpOn) runArp();

  // Space knob, smoothed
  float s = analogRead(SPACE_PIN) / 1023.0f;
  spaceNow += (s - spaceNow) * 0.05f;
  AudioNoInterrupts();
  setSpace(spaceNow);
  AudioInterrupts();

  delay(2);
}
