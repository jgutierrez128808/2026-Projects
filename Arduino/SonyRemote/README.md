# SonyRemote

Arduino Nano sketch that emulates a physical remote for the **Sony STR-DH130** stereo receiver using an IR LED. Replaces the OEM remote with four tactile buttons: power, volume up, volume down, and input source cycling.

---

## Hardware

| Component | Details |
|-----------|---------|
| Microcontroller | Arduino Nano |
| IR LED | Any 940 nm LED with a ~100 Ω series resistor on pin 3 |
| Power button | Pin 4 → GND |
| Vol+ button | Pin 5 → GND |
| Vol− button | Pin 6 → GND |
| Input button | Pin 7 → GND |

All buttons use the Nano's internal pull-up resistors (INPUT_PULLUP) — no external resistors needed. Each button connects between its pin and GND; the sketch reads it as active LOW.

---

## Dependencies

**IRremote >= 3.x** — install via the Arduino Library Manager.

---

## How It Works

### Protocol

The STR-DH130 uses the **Sony SIRC-15** protocol: a 15-bit frame consisting of a 7-bit command and an 8-bit device address, transmitted at 40 kHz. The confirmed device address for this receiver is `0x30` (decimal 48). Per the Sony spec, each command is sent three times (one initial + two repeats).

### Button Behavior

**Power** — sends a power toggle command on press. Repeats if held (at 180 ms intervals), matching the behavior of the other buttons, though a single tap is all that's needed.

**Vol+ / Vol−** — send their respective commands immediately on press, then repeat at 180 ms intervals for as long as the button is held. A 50 ms debounce filters contact noise on the initial edge.

**Input** — cycles through the four input sources in order, sending a discrete select command on each press. No auto-repeat. If the receiver's input is changed via another remote, just press through the cycle to re-sync.

### Input Cycle Order

```
VIDEO → MD/TAPE → SA-CD/CD → BD/DVD → VIDEO → ...
```

| Input | Command (hex) | SIRC Function |
|-------|--------------|---------------|
| VIDEO | `0x22` | fn 34 |
| MD/TAPE | `0x23` | fn 35 |
| SA-CD/CD | `0x25` | fn 37 |
| BD/DVD | `0x7D` | fn 125 |

### Other Confirmed Codes (device `0x30`)

These are implemented but not wired to buttons — available if you want to extend the sketch:

| Function | Command (hex) | SIRC Function |
|----------|--------------|---------------|
| Power toggle | `0x15` | fn 21 |
| Power on (discrete) | `0x2F` | fn 47 |
| Power off (discrete) | `0x4B` | fn 75 |
| Vol+ | `0x12` | fn 18 |
| Vol− | `0x13` | fn 19 |
| Mute | `0x14` | fn 20 |
| FM | `0x1A` | fn 26 |
| AM | `0x21` | fn 33 |
| Portable | `0x7D` | fn 125 |
| Display | `0x60` | fn 96 |
| Sleep | `0x69` | fn 105 |

> **BD/DVD note:** A Sony20 (20-bit) code also exists for BD/DVD (`d=16 subd=40 obc=22`). The SIRC-15 code above (`0x7D`) was confirmed working on hardware and is what the sketch uses.

---

## Extending

To add or remove input sources, edit the `INPUTS[]` array in `SonyRemote.ino`. Each entry is just a one-byte command — the cycle length adjusts automatically via `sizeof`.

```cpp
static const InputSource INPUTS[] = {
    { 0x22 },  // VIDEO
    { 0x23 },  // MD/TAPE
    { 0x25 },  // SA-CD/CD
    { 0x7D },  // BD/DVD
};
```
