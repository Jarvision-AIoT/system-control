/*
  ---------------------------------
  IMPORTANT: Configuration Reminder
  ---------------------------------
  
  Before running this code, make sure to check the "secrets.h" file
  for important configuration details such as Wi-Fi credentials and 
  MQTT Broker settings.

  The "secrets.h" file should include:
  - Your Wi-Fi SSID and Password
  - MQTT Broker (mosquitto) IP, Topic, Port Address

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
#include "secrets.h"                // Contains WIFI_SSID and WIFI_PASSWORD
#include <ArduinoMqttClient.h>
#include "PinDefinitionsAndMore.h"  // IR pin definitions
#include <IRremote.hpp>
#include <WiFiS3.h>                 // For Arduino UNO R4 WiFi

#define DECODE_NEC
#define DECODE_DISTANCE_WIDTH

char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASSWORD;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = BROKER_IP;
const char topic[]  = TOPIC_ID;
int        port     = PORT_ADDR;

decode_results results;

void setup()
{
  Serial.begin(19200);
  delay(2000);

  //Serial.println("Setting static IP...");
  //WiFi.config(local_ip, dns, gateway, subnet);
  
  Serial.print("Connecting to WiFi SSID: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(ssid, pass);
  delay(3000);
  //Serial.println(WiFi.localIP());
  //Serial.println(WiFi.subnetMask());
  //Serial.println(WiFi.gatewayIP());
  //Serial.println(WiFi.dnsServerIP());
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print("-");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("==============================");

  #if defined(ARDUINO_UNOWIFIR4)
    digitalWrite(LED_BUILTIN, HIGH);
  #endif

  Serial.println("===== MQTT Connection Start =====");
  Serial.print("Connecting to broker: ");
  Serial.print(broker);
  Serial.print(":");
  Serial.println(port);

  while (!mqttClient.connect(broker, port))
  {
    Serial.print("[ERROR] MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    delay(1000);
  }

  Serial.println("Connected to MQTT broker!");
  mqttClient.subscribe(topic);
  Serial.print("Subscribed to topic: ");
  Serial.println(topic);
  Serial.println("==============================");

  Serial.println("===== IR Initialization =====");
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  Serial.print(F("Ready to receive IR at pin "));
  Serial.println(IR_RECEIVE_PIN);
  printActiveIRProtocols(&Serial);

  IrSender.begin();
  Serial.print(F("Ready to send IR at pin "));
  Serial.println(IR_SEND_PIN);
  Serial.println("==============================");
}

uint16_t sAddress = 0x0123;
uint8_t sCommand = 0xFF;
uint8_t sRepeats = 1;

void send_ir_data()
{
  Serial.print(F("Sending IR -> Addr: 0x"));
  Serial.print(sAddress, HEX);
  Serial.print(" Cmd: 0x");
  Serial.print(sCommand, HEX);
  Serial.print(" Repeats: ");
  Serial.println(sRepeats);
  Serial.flush();

  if (sRepeats > 3)
    sRepeats = 3;

  IrSender.sendNEC(sAddress, sCommand, sRepeats);
}

void receive_ir_data()
{
  if (IrReceiver.decode())
  {
    Serial.print(F("Received IR - Protocol: "));
    Serial.print(getProtocolString(IrReceiver.decodedIRData.protocol));
    Serial.print(" Raw: ");
  #if (__INT_WIDTH__ < 32)
    Serial.print(IrReceiver.decodedIRData.decodedRawData, HEX);
  #else
    PrintULL::print(&Serial, IrReceiver.decodedIRData.decodedRawData, HEX);
  #endif
    Serial.print(" Addr: ");
    Serial.print(IrReceiver.decodedIRData.address, HEX);
    Serial.print(" Cmd: ");
    Serial.println(IrReceiver.decodedIRData.command, HEX);
    IrReceiver.resume();
  }
}

int flagTV = 0;
int flagS = 0;

void IRsignal(String cmd)
{
  /*
  Open - 무드등 on
  Close - 무드등 off
  OK - TV on
  Peace - TV off
  One - 선풍기 속도 다운
  Two - 선풍기 속도 업
  Three - 
  Rock - 선풍기 on
  Phone - 선풍기 off
  Heart - 다중기기 동시제어
  */
  if (cmd == "{\"type\": \"Open\"}")                         //무드등 키기
  {
    sAddress = 0x00; sCommand = 0x03; sRepeats = 3;
  }
  else if (cmd == "{\"type\": \"Close\"}")                   //무드등 끄기
  { 
    sAddress = 0x00; sCommand = 0x02; sRepeats = 3;
  }
  else if (cmd == "{\"type\": \"OK\"}")                      //TV 켜기
  {
    if (flagTV == 0)
    {
      sAddress = 0x08; sCommand = 0xD7; sRepeats = 3;
      flagTV = 1;
    }
    else
    {
      sAddress = 0x0123; sCommand = 0xFF; sRepeats = 1;
      flagTV = 0;
    }
  }
  else if (cmd == "{\"type\": \"Peace\"}")                   //TV 끄기
  {
    if (flagTV == 1)
    {
      sAddress = 0x08; sCommand = 0xD7; sRepeats = 3;
      flagTV = 0;
    }
    else
    {
      sAddress = 0x0123; sCommand = 0xFF; sRepeats = 1;
      flagTV = 1;
    }
  }
  else if (cmd == "{\"type\": \"One\"}")                     //선풍기 속도 다운
  {
    sAddress = 0x00; sCommand = 0x18; sRepeats = 3;
  }
  else if (cmd == "{\"type\": \"Two\"}")                     //선풍기 속도 업
  {
    sAddress = 0x00; sCommand = 0x15; sRepeats = 3;
  }
  else if (cmd == "{\"type\": \"Three\"}")
  {
  }
  else if (cmd == "{\"type\": \"Rock\"}")                    //선풍기 On
  {
    if (flagS == 0)
    {
      sAddress = 0x00; sCommand = 0x45; sRepeats = 3;
      flagS = 1;
    }
    else
    {
      sAddress = 0x0123; sCommand = 0xFF; sRepeats = 1;
      flagS = 0;
    }
  }
  else if (cmd == "{\"type\": \"Phone\"}")                   //선풍기 Off
  {
    if (flagS == 1)
    {
      sAddress = 0x00; sCommand = 0x45; sRepeats = 3;
      flagS = 0;
    }
    else
    {
      sAddress = 0x0123; sCommand = 0xFF; sRepeats = 1;
      flagS = 1;
    }
  }

  return;
}

String prevCmd = "temp";

void loop()
{
  receive_ir_data();

  int messageSize = mqttClient.parseMessage();
  if (messageSize)
  {
    String command = "";
    Serial.print(millis());
    Serial.print("ms) Received MQTT topic '");
    Serial.print(mqttClient.messageTopic());
    Serial.print("' (");
    Serial.print(messageSize);
    Serial.println(" bytes):");

    while (mqttClient.available())
    {
      command += (char)mqttClient.read();
    }

    //test code for time taken
    //if (command == "test complete")
    //  return;
    //mqttClient.beginMessage(topic);
    //mqttClient.print("test complete");
    //mqttClient.endMessage();
    
    Serial.print("Parsed command: ");
    Serial.println(command);

    if (prevCmd != command)
    {
      if (command == "{\"type\": \"Heart\"}")
      {
        command = "{\"type\": \"OK\"}";
        IRsignal(command);
        send_ir_data();
        IrReceiver.restartAfterSend();
        command = "{\"type\": \"Open\"}";
        IRsignal(command);
        send_ir_data();
        IrReceiver.restartAfterSend();
        command = "{\"type\": \"Rock\"}";
        IRsignal(command);
        send_ir_data();
        IrReceiver.restartAfterSend();
      }
      else
      {
        IRsignal(command);
        send_ir_data();
        IrReceiver.restartAfterSend();
      }
    }

    prevCmd = command;
  }
}
