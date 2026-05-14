/*
 * IR code capture — flash this to identify the real codes from your Sony remote.
 * Wire: IR receiver module (e.g. TSOP38238 / VS1838B) signal pin → D2, VCC → 5V, GND → GND.
 * Open Serial Monitor at 9600 baud, point remote at receiver, press each button.
 */

#define IR_RECEIVE_PIN 2
#include <IRremote.hpp>

void setup() {
    Serial.begin(9600);
    IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
    Serial.println("IR capture ready — point remote at receiver and press buttons.");
}

void loop() {
    if (!IrReceiver.decode()) return;

    Serial.print("Protocol : ");
    Serial.println(IrReceiver.getProtocolString());
    Serial.print("Address  : 0x");
    Serial.println(IrReceiver.decodedIRData.address, HEX);
    Serial.print("Command  : 0x");
    Serial.println(IrReceiver.decodedIRData.command, HEX);
    Serial.print("Raw data : 0x");
    Serial.println(IrReceiver.decodedIRData.decodedRawData, HEX);
    Serial.print("Bits     : ");
    Serial.println(IrReceiver.decodedIRData.numberOfBits);
    Serial.println("---");

    IrReceiver.resume();
}
