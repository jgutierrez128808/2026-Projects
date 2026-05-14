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
 * STR-DH130 device addresses:
 *   0x30 (48)  — confirmed via forum/IRScrutinizer (RM-AAU188)
 *   0xB0 (176) — found via non-OEM remote; verify on hardware
 *
 * NOTE — BD/DVD: only a Sony20 (20-bit) code was found (d=16 subd=40 obc=22).
 * To use it, call IrSender.sendSony(0x28, 0x16, SONY_REPEATS, 20) and confirm
 * the address/command byte mapping against the raw pronto hex before enabling.
 */

#define IR_SEND_PIN 3
#include <IRremote.hpp>

// Device addresses
static const uint8_t SONY_ADDR     = 0x30;  // device 48  — confirmed
static const uint8_t SONY_ADDR_ALT = 0xB0;  // device 176 — needs hardware verification

// Commands — device 48
static const uint8_t CMD_POWER  = 0x15;  // P-Toggle  (fn 21)
static const uint8_t CMD_VOL_UP = 0x12;  // Vol+      (fn 18)
static const uint8_t CMD_VOL_DN = 0x13;  // Vol-      (fn 19)
static const uint8_t CMD_MUTE   = 0x14;  // Mute      (fn 20)

// Button pins (active LOW via INPUT_PULLUP)
static const uint8_t BTN_POWER  = 4;
static const uint8_t BTN_VOL_UP = 5;
static const uint8_t BTN_VOL_DN = 6;
static const uint8_t BTN_INPUT  = 7;

static const unsigned long DEBOUNCE_MS = 50;
static const unsigned long REPEAT_MS   = 180;  // vol repeat interval while held
static const uint8_t       SONY_REPEATS = 2;   // IRremote sends 1 + this = 3 total

// ---------------------------------------------------------------------------
// Repeating buttons (power / vol)
// ---------------------------------------------------------------------------

struct Button {
    uint8_t       pin;
    uint8_t       cmd;
    const char   *label;
    bool          lastState;
    unsigned long lastChangeMs;
    unsigned long lastSendMs;
};

Button buttons[] = {
    { BTN_POWER,  CMD_POWER,  "POWER",       true, 0, 0 },
    { BTN_VOL_UP, CMD_VOL_UP, "VOLUME UP",   true, 0, 0 },
    { BTN_VOL_DN, CMD_VOL_DN, "VOLUME DOWN", true, 0, 0 },
};

static const uint8_t NUM_BUTTONS = sizeof(buttons) / sizeof(buttons[0]);

// ---------------------------------------------------------------------------
// Input source state machine
// ---------------------------------------------------------------------------

struct InputSource {
    uint8_t     addr;
    uint8_t     cmd;
    const char *label;
};

// Each press of BTN_INPUT advances to the next entry and sends its discrete code.
// State drifts if the original remote is used — just cycle through to re-sync.
// Add BD/DVD here once its address/command are confirmed (see Sony20 note above).
static const InputSource INPUTS[] = {
    { SONY_ADDR,     0x1A, "FM"        },  // d=48,  fn=26  — confirmed
    { SONY_ADDR,     0x21, "AM"        },  // d=48,  fn=33  — confirmed
    { SONY_ADDR,     0x23, "VIDEO"     },  // d=48,  fn=35  — confirmed
    { SONY_ADDR,     0x25, "MD/TAPE"   },  // d=48,  fn=37  — confirmed
    { SONY_ADDR,     0x2E, "SA-CD/CD"  },  // d=48,  fn=46  — confirmed
    { SONY_ADDR,     0x7D, "PORTABLE"  },  // d=48,  fn=125 — confirmed
    { SONY_ADDR_ALT, 0x03, "SAT/CBL"   },  // d=176, fn=3   — VERIFY on hardware
};

static const uint8_t NUM_INPUTS = sizeof(INPUTS) / sizeof(INPUTS[0]);

// Start at last index so the first button press wraps to 0 (FM).
static uint8_t currentInput = NUM_INPUTS - 1;

// Debounce state for the input button — single fire per press, no auto-repeat.
static bool          inputLastState    = HIGH;
static unsigned long inputLastChangeMs = 0;
static bool          inputSentOnPress  = false;

// ---------------------------------------------------------------------------

void sendSonyCmd(uint8_t addr, uint8_t cmd, const char *label) {
    Serial.print("Sending: ");
    Serial.print(label);
    Serial.print("  (addr=0x");
    Serial.print(addr, HEX);
    Serial.print("  cmd=0x");
    Serial.print(cmd, HEX);
    Serial.println(")");
    IrSender.sendSony(addr, cmd, SONY_REPEATS, 15);
}

void setup() {
    Serial.begin(9600);
    IrSender.begin();

    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        pinMode(buttons[i].pin, INPUT_PULLUP);
        buttons[i].lastState = HIGH;
    }
    pinMode(BTN_INPUT, INPUT_PULLUP);

    Serial.println("Sony STR-DH130 remote ready.");
    Serial.print("Input sources available: ");
    Serial.println(NUM_INPUTS);
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
                sendSonyCmd(SONY_ADDR, b.cmd, b.label);
                b.lastSendMs = now;
            }
        }
    }

    // --- Input cycle button (single fire per press) ---
    bool inputReading = digitalRead(BTN_INPUT);

    if (inputReading != inputLastState) {
        inputLastChangeMs = now;
        inputLastState = inputReading;
        inputSentOnPress = false;  // reset on any edge so next press fires fresh
    }

    if ((now - inputLastChangeMs) >= DEBOUNCE_MS) {
        if (inputLastState == LOW && !inputSentOnPress) {
            currentInput = (currentInput + 1) % NUM_INPUTS;
            const InputSource &src = INPUTS[currentInput];
            sendSonyCmd(src.addr, src.cmd, src.label);
            inputSentOnPress = true;
        }
    }
}
