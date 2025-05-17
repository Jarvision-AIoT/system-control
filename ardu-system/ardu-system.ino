/*
  ---------------------------------
  IMPORTANT: Configuration Reminder
  ---------------------------------
  
  Before running this code, make sure to check the "secrets.h" file
  for important configuration details such as Wi-Fi credentials and 
  Firebase settings.

  The "secrets.h" file should include:
  - Your Wi-Fi SSID and Password
  - Your MQTT Broker URL or IP address
  - Your selected MQTT Broker Topic
  - Port for your Broker (Usually 1883)

  Ensure that "secrets.h" is properly configured and includes the correct
  information for your project. Failure to do so may result in connection
  errors or incorrect behavior of your application.

  Note: The "secrets.h" file should be located in the same directory as
  this sketch.
*/

#include <Arduino.h>
#include <WiFiS3.h>                 // For Arduino UNO R4 WiFi
#include <ArduinoMqttClient.h>
#include "PinDefinitionsAndMore.h"  // IR pin definitions
#include <IRremote.hpp>
#include "secrets.h"                // Contains WIFI_SSID and WIFI_PASSWORD

#define DECODE_NEC
#define DECODE_DISTANCE_WIDTH

char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASSWORD;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = BROKER_IP;
const char topic[]  = TOPIC_ID;
int        port     = 1883;

unsigned long previousMillis = 0;
const long interval = 500;
decode_results results;

/*
  Note: if your wifi connection has some issue, try using fixed IP, gateway, subnet, and dns using code below.
  This method WORK but not recommended.
  IPAddress local_ip(?, ?, ?, ?);     // IP address that is not likely to be used
  IPAddress gateway(?, ?, ?, ?);      // gateway checked from PC
  IPAddress subnet(?, ?, ?, ?);       // subnet checked from PC
  IPAddress dns(?, ?, ?, ?);          // We used Google Public DNS when testing
*/

void setup()
{
  Serial.begin(9600);
  delay(2000);

  Serial.println("Setting static IP...");
  //WiFi.config(local_ip, dns, gateway, subnet);
  
  Serial.print("Connecting to WiFi SSID: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(ssid, pass);
  delay(3000);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print("-");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  #if defined(ARDUINO_UNOWIFIR4)
    digitalWrite(LED_BUILTIN, HIGH);
  #endif

  Serial.println("\n===== MQTT Connection Start =====");
  Serial.print("Connecting to broker: ");
  Serial.print(broker);
  Serial.print(":");
  Serial.println(port);

  if (!mqttClient.connect(broker, port))
  {
    Serial.print("[ERROR] MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    while (1); // stop here
  }

  Serial.println("MQTT broker connected!");
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
uint8_t sCommand = 0x45;
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

int IRsignal(String cmd)
{
  if (cmd == "{\"type\":\"fist\"}") { sAddress = 0x00; sCommand = 0x02; sRepeats = 3; }
  else if (cmd == "{\"type\":\"open\"}") { sAddress = 0x00; sCommand = 0x03; sRepeats = 3; }
  else if (cmd == "{\"type\":\"ok_sign\"}") { sAddress = 0x0123; sCommand = 0x45; sRepeats = 1; }
  else if (cmd == "{\"type\":\"point\"}") { sAddress = 0x0123; sCommand = 0x45; sRepeats = 2; }
  else if (cmd == "{\"type\":\"peace\"}") { sAddress = 0x0123; sCommand = 0x45; sRepeats = 3; }
  else if (cmd == "{\"type\":\"standby\"}") { sAddress = 0x0123; sCommand = 0x45; sRepeats = 4; }
  else if (cmd == "{\"type\":\"thumbs_up\"}") { sAddress = 0x0123; sCommand = 0x45; sRepeats = 5; }
  else if (cmd == "{\"type\":\"rock\"}") { sAddress = 0x0123; sCommand = 0x45; sRepeats = 6; }
  else if (cmd == "{\"type\":\"love_u\"}") { sAddress = 0x0123; sCommand = 0x45; sRepeats = 7; }
  return 0;
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

    Serial.print("Parsed command: ");
    Serial.println(command);

    if (prevCmd != command)
    {
      IRsignal(command);
      send_ir_data();
      IrReceiver.restartAfterSend();
    }

    prevCmd = command;
  }
}
