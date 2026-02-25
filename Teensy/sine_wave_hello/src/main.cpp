// Sine Wave Hello World — Teensy 4.1 + Audio Shield (SGTL5000)
// Generates a 440 Hz sine wave (concert A) out of the audio shield.
//
// Requirements:
//   - Teensy 4.1
//   - PJRC Teensy Audio Shield (Rev D)
//   - PlatformIO with Teensy platform installed

#include <Arduino.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// Audio objects
AudioSynthWaveformSine   sine;
AudioOutputI2S           audioOut;
AudioControlSGTL5000     sgtl5000;

// Route sine -> left and right output
AudioConnection patchL(sine, 0, audioOut, 0);
AudioConnection patchR(sine, 0, audioOut, 1);

void setup() {
  AudioMemory(8);

  // Initialize audio shield
  sgtl5000.enable();
  sgtl5000.volume(0.5);

  // Set frequency and amplitude
  sine.frequency(440.0);  // 440 Hz = concert A
  sine.amplitude(0.8);
}

void loop() {
  // Nothing needed here — audio runs in the background
}
