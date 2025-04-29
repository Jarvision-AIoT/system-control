/*
  ---------------------------------
  IMPORTANT: Configuration Reminder
  ---------------------------------
  
  Before running this code, make sure to check the "secrets.h" file
  for important configuration details such as Wi-Fi credentials and 
  Firebase settings.

  The "secrets.h" file should include:
  - Your Wi-Fi SSID and Password
  - Your Firebase Realtime Database URL
  - (OPTIONAL) Firebase Authentication Token

  Ensure that "secrets.h" is properly configured and includes the correct
  information for your project. Failure to do so may result in connection
  errors or incorrect behavior of your application.

  Note: The "secrets.h" file should be located in the same directory as
  this sketch.
*/

/*
  ---------------------------------
      INFO: ArduinoJson library   
  ---------------------------------

  Download ArduinoJson library from the Library Manager:
  https://www.arduino.cc/reference/en/libraries/arduinojson/

  For guidance on serialization and deserialization, visit:
  https://arduinojson.org/v7/assistant/
*/

#include <Arduino.h>
#include "secrets.h"                // Define WIFI and Firebase Realtime Database
#include <Firebase.h>               // Firebase library
#include "PinDefinitionsAndMore.h"  // Define macros for input and output pin etc.
#include <IRremote.hpp>             // IR signal library

#define DECODE_NEC          // Includes Apple and Onkyo
#define DECODE_DISTANCE_WIDTH // In case NEC is not received correctly. Universal decoder for pulse distance width protocols

/* Use the following instance for Test Mode (No Authentication) */
// Firebase fb(REFERENCE_URL);

/* Use the following instance for Locked Mode (With Authentication) */
Firebase fb(REFERENCE_URL, AUTH_TOKEN);

unsigned long previousMillis = 0;
const long interval = 500; //default: 2000 recommended: 500~1000
decode_results results; // 수신 데이터 저장 구조체

void setup()
{
  Serial.begin(9600);
  #if !defined(ARDUINO_UNOWIFIR4)
    WiFi.mode(WIFI_STA);
  #else
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
  #endif
  WiFi.disconnect();
  delay(1000);

  /* Connect to WiFi */
  Serial.println();
  Serial.println();
  Serial.print("Connecting to: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("-");
    delay(500);
  }

  Serial.println();
  Serial.println("WiFi Connected");
  Serial.println();

  #if defined(ARDUINO_UNOWIFIR4)
    digitalWrite(LED_BUILTIN, HIGH);
  #endif

  Serial.println("Start IR");
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // 수신기 작동 시작
  Serial.print(F("Ready to receive IR signals of protocols: "));
  printActiveIRProtocols(&Serial);
  Serial.println(F("at pin " STR(IR_RECEIVE_PIN)));
  IrSender.begin(); // Start with IR_SEND_PIN -which is defined in PinDefinitionsAndMore.h- as send pin and enable feedback LED at default feedback LED pin
  Serial.println(F("Send IR signals at pin " STR(IR_SEND_PIN)));
}

uint16_t sAddress = 0x0123;
uint8_t sCommand = 0x45;
uint8_t sRepeats = 1;

/*
 * Send NEC IR protocol
 */
void send_ir_data()
{
    Serial.print(F("Sending: 0x"));
    Serial.print(sAddress, HEX);
    Serial.print(sCommand, HEX);
    Serial.println(sRepeats, HEX);
    Serial.flush(); // To avoid disturbing the software PWM generation by serial output interrupts

    // clip repeats at 3
    if (sRepeats > 3)
    {
        sRepeats = 3;
    }
    // Results for the first loop to: Protocol=NEC Address=0x0123 Command=0x45(32 bits)
    IrSender.sendNEC(sAddress, sCommand, sRepeats);
}

void receive_ir_data()
{
    if (IrReceiver.decode())
    {
        Serial.print(F("Decoded protocol: "));
        Serial.print(getProtocolString(IrReceiver.decodedIRData.protocol));
        Serial.print(F(", decoded raw data: "));
    #if (__INT_WIDTH__ < 32)
        Serial.print(IrReceiver.decodedIRData.decodedRawData, HEX);
    #else
        PrintULL::print(&Serial, IrReceiver.decodedIRData.decodedRawData, HEX);
    #endif
        Serial.print(F(", decoded address: "));
        Serial.print(IrReceiver.decodedIRData.address, HEX);
        Serial.print(F(", decoded command: "));
        Serial.println(IrReceiver.decodedIRData.command, HEX);
        IrReceiver.resume();
    }
}

int IRsignal(String cmd)
{
  if (cmd == "{\"type\":\"fist\"}") //off
  {
    sAddress = 0x00;
    sCommand = 0x02;
    sRepeats = 3;
  }
  else if (cmd == "{\"type\":\"open\"}") //on
  {
    sAddress = 0x00;
    sCommand = 0x03;
    sRepeats = 3;
  }
  else if (cmd == "{\"type\":\"ok_sign\"}")
  {
    sAddress = 0x0123;
    sCommand = 0x45;
    sRepeats = 1;
  }
  else if (cmd == "{\"type\":\"point\"}")
  {
    sAddress = 0x0123;
    sCommand = 0x45;
    sRepeats = 2;
  }
  else if (cmd == "{\"type\":\"peace\"}")
  {
    sAddress = 0x0123;
    sCommand = 0x45;
    sRepeats = 3;
  }
  else if (cmd == "{\"type\":\"standby\"}")
  {
    sAddress = 0x0123;
    sCommand = 0x45;
    sRepeats = 4;
  }
  else if (cmd == "{\"type\":\"thumbs_up\"}")
  {
    sAddress = 0x0123;
    sCommand = 0x45;
    sRepeats = 5;
  }
  else if (cmd == "{\"type\":\"rock\"}")
  {
    sAddress = 0x0123;
    sCommand = 0x45;
    sRepeats = 6;
  }
  else if (cmd == "{\"type\":\"love_u\"}")
  {
    sAddress = 0x0123;
    sCommand = 0x45;
    sRepeats = 7;
  }
}

String prevCmd = "temp";

void loop()
{
  unsigned long currentMillis = millis();
  receive_ir_data();

  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;

    // Firebase에서 데이터 가져오기
    //String command = fb.getString("Example/IR_COMMAND");
    String command = fb.getString("arduino/gesture/right");
    if (command.length() > 0)
    {
      Serial.print(currentMillis);
      Serial.print(") ");
      Serial.print("Received IR Command: ");
      Serial.println(command);
      if (prevCmd != command) // 임시용 / 변화가 생기면 단일 출력
      {
        IRsignal(command);
        send_ir_data();
        IrReceiver.restartAfterSend(); // Is a NOP if sending does not require a timer.
      }

      prevCmd = command;
    }
    else
    {
      Serial.println("No command received or error occurred.");
    }
  }
}
