/*
 * Sony STR-DH130 IR Remote — Arduino Nano
 *
 * Hardware:
 *   IR LED (with ~100Ω series resistor) on pin 3 (IRremote default TX)
 *   Power  button: pin 4 → GND  (INPUT_PULLUP)
 *   Vol+   button: pin 5 → GND  (INPUT_PULLUP)
 *   Vol-   button: pin 6 → GND  (INPUT_PULLUP)
 *
 * Library: IRremote >= 3.x  (install via Library Manager)
 *
 * Sony SIRC-15 protocol: 7-bit command + 8-bit address, 40 kHz carrier
 * STR-DH130 device address: 48 (0x30) — confirmed via forum/IRScrutinizer
 * Source: remotecentral.com forum, RM-AAU188 reverse-engineered codes
 */

#define IR_SEND_PIN 3
#include <IRremote.hpp>

// Sony SIRC-15 device address for STR-DH130 (confirmed: device 48)
static const uint8_t SONY_ADDR = 0x30;

// Commands (device 48, confirmed against STR-DH130)
static const uint8_t CMD_POWER  = 0x18;  // P-Toggle (fn 24)
static const uint8_t CMD_VOL_UP = 0x13;  // Vol+ hold (fn 19)
static const uint8_t CMD_VOL_DN = 0x14;  // Vol- hold (fn 20)
static const uint8_t CMD_MUTE   = 0x15;  // Mute toggle (fn 21)

// Button pins (active LOW via INPUT_PULLUP)
static const uint8_t BTN_POWER  = 4;
static const uint8_t BTN_VOL_UP = 5;
static const uint8_t BTN_VOL_DN = 6;

static const unsigned long DEBOUNCE_MS  = 50;
static const unsigned long REPEAT_MS    = 180;  // repeat interval while held
// Sony spec recommends sending the command 3 times
static const uint8_t SONY_REPEATS = 2;          // IRremote sends 1 + this many repeats

struct Button {
    uint8_t       pin;
    uint8_t       cmd;
    const char   *label;
    bool          lastState;
    unsigned long lastChangeMs;
    unsigned long lastSendMs;
};

Button buttons[] = {
    { BTN_POWER,  CMD_POWER,  "POWER",      true, 0, 0 },
    { BTN_VOL_UP, CMD_VOL_UP, "VOLUME UP",  true, 0, 0 },
    { BTN_VOL_DN, CMD_VOL_DN, "VOLUME DOWN",true, 0, 0 },
};

static const uint8_t NUM_BUTTONS = sizeof(buttons) / sizeof(buttons[0]);

void sendSonyCmd(const Button &b) {
    Serial.print("Button pressed: ");
    Serial.print(b.label);
    Serial.print("  (cmd=0x");
    Serial.print(b.cmd, HEX);
    Serial.println(")");
    IrSender.sendSony(SONY_ADDR, b.cmd, SONY_REPEATS, 15);
}

void setup() {
    Serial.begin(9600);
    Serial.println("Sony STR-DH130 remote ready.");
    IrSender.begin();
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        pinMode(buttons[i].pin, INPUT_PULLUP);
        buttons[i].lastState = HIGH;
    }
}

void loop() {
    unsigned long now = millis();

    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        Button &b = buttons[i];
        bool reading = digitalRead(b.pin);

        // Debounce: ignore transitions shorter than DEBOUNCE_MS
        if (reading != b.lastState) {
            b.lastChangeMs = now;
            b.lastState = reading;
        }

        if ((now - b.lastChangeMs) < DEBOUNCE_MS) continue;

        bool pressed = (b.lastState == LOW);

        if (pressed) {
            if ((now - b.lastSendMs) >= REPEAT_MS) {
                sendSonyCmd(b);
                b.lastSendMs = now;
            }
        }
    }
}
