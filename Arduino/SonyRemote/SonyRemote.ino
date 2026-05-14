/*
 * Sony STR-DH130 IR Remote — Arduino Nano
 *
 * Hardware:
 *   IR LED (with ~100Ω series resistor) on pin 3 (IRremote default TX)
 *   Power  button: pin 4 → GND  (INPUT_PULLUP)
 *   Vol+   button: pin 5 → GND  (INPUT_PULLUP)
 *   Vol-   button: pin 6 → GND  (INPUT_PULLUP)
 *   Input  button: pin 7 → GND  (INPUT_PULLUP)  ← cycles input sources
 *
 * Library: IRremote >= 3.x  (install via Library Manager)
 *
 * Sony SIRC-15 protocol: 7-bit command + 8-bit address, 40 kHz carrier
 * STR-DH130 device address: 0x30 (48) — confirmed via forum/IRScrutinizer (RM-AAU188)
 */

#define IR_SEND_PIN 3
#include <IRremote.hpp>

static const uint8_t SONY_ADDR = 0x30;

// Commands — device 48
static const uint8_t CMD_POWER  = 0x15;  // P-Toggle (fn 21)
static const uint8_t CMD_VOL_UP = 0x12;  // Vol+     (fn 18)
static const uint8_t CMD_VOL_DN = 0x13;  // Vol-     (fn 19)
static const uint8_t CMD_MUTE   = 0x14;  // Mute     (fn 20)

// Button pins (active LOW via INPUT_PULLUP)
static const uint8_t BTN_POWER  = 4;
static const uint8_t BTN_VOL_UP = 5;
static const uint8_t BTN_VOL_DN = 6;
static const uint8_t BTN_INPUT  = 7;

static const unsigned long DEBOUNCE_MS  = 50;
static const unsigned long REPEAT_MS    = 180;  // vol repeat interval while held
static const uint8_t       SONY_REPEATS = 2;    // IRremote sends 1 + this = 3 total

// ---------------------------------------------------------------------------
// Repeating buttons (power / vol)
// ---------------------------------------------------------------------------

struct Button {
    uint8_t       pin;
    uint8_t       cmd;
    bool          lastState;
    unsigned long lastChangeMs;
    unsigned long lastSendMs;
};

Button buttons[] = {
    { BTN_POWER,  CMD_POWER,  true, 0, 0 },
    { BTN_VOL_UP, CMD_VOL_UP, true, 0, 0 },
    { BTN_VOL_DN, CMD_VOL_DN, true, 0, 0 },
};

static const uint8_t NUM_BUTTONS = sizeof(buttons) / sizeof(buttons[0]);

// ---------------------------------------------------------------------------
// Input source state machine
// ---------------------------------------------------------------------------

struct InputSource {
    uint8_t cmd;
};

// Each press of BTN_INPUT advances to the next entry and sends its discrete code.
// State drifts if the original remote is used — just cycle through to re-sync.
static const InputSource INPUTS[] = {
    { 0x22 },  // VIDEO    fn=34
    { 0x23 },  // MD/TAPE  fn=35
    { 0x25 },  // SA-CD/CD fn=37
    { 0x7D },  // BD/DVD   fn=125
};

static const uint8_t NUM_INPUTS = sizeof(INPUTS) / sizeof(INPUTS[0]);

static uint8_t       currentInput     = NUM_INPUTS - 1;
static bool          inputLastState   = HIGH;
static unsigned long inputLastChangeMs = 0;
static bool          inputSentOnPress  = false;

// ---------------------------------------------------------------------------

void setup() {
    IrSender.begin();
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        pinMode(buttons[i].pin, INPUT_PULLUP);
        buttons[i].lastState = HIGH;
    }
    pinMode(BTN_INPUT, INPUT_PULLUP);
}

void loop() {
    unsigned long now = millis();

    // --- Repeating buttons (power, vol+, vol-) ---
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        Button &b = buttons[i];
        bool reading = digitalRead(b.pin);

        if (reading != b.lastState) {
            b.lastChangeMs = now;
            b.lastState = reading;
        }

        if ((now - b.lastChangeMs) < DEBOUNCE_MS) continue;

        if (b.lastState == LOW) {
            if ((now - b.lastSendMs) >= REPEAT_MS) {
                IrSender.sendSony(SONY_ADDR, b.cmd, SONY_REPEATS, 15);
                b.lastSendMs = now;
            }
        }
    }

    // --- Input cycle button (single fire per press) ---
    bool inputReading = digitalRead(BTN_INPUT);

    if (inputReading != inputLastState) {
        inputLastChangeMs = now;
        inputLastState = inputReading;
        inputSentOnPress = false;
    }

    if ((now - inputLastChangeMs) >= DEBOUNCE_MS) {
        if (inputLastState == LOW && !inputSentOnPress) {
            currentInput = (currentInput + 1) % NUM_INPUTS;
            IrSender.sendSony(SONY_ADDR, INPUTS[currentInput].cmd, SONY_REPEATS, 15);
            inputSentOnPress = true;
        }
    }
}
