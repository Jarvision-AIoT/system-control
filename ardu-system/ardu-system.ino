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
#include "secrets.h"                // WIFI_SSID, WIFI_PASSWORD
#include <ArduinoMqttClient.h>
#include "PinDefinitionsAndMore.h"  // IR_RECEIVE_PIN, IR_SEND_PIN
#include <IRremote.hpp>
#include <WiFiS3.h>                 // Arduino UNO R4 WiFi

#define DECODE_NEC
#define DECODE_DISTANCE_WIDTH

char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASSWORD;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = BROKER_IP;
const char topic[]  = TOPIC_ID;
int        port     = PORT_ADDR;

// ==============================================
//  기기 상태 및 IR 신호 관리 클래스
//  • ON/​OFF 신호 외에 커스텀 명령도 보낼 수 있도록 sendCustom() 추가
// ==============================================
class DeviceController {
private:
  uint16_t onAddr;
  uint8_t  onCmd;
  uint8_t  onRepeats;

  uint16_t offAddr;
  uint8_t  offCmd;
  uint8_t  offRepeats;

  bool state; // false = OFF, true = ON

public:
  DeviceController(uint16_t on_address, uint8_t on_command, uint8_t on_repeat,
                   uint16_t off_address, uint8_t off_command, uint8_t off_repeat)
    : onAddr(on_address),  onCmd(on_command),  onRepeats(on_repeat),
      offAddr(off_address), offCmd(off_command), offRepeats(off_repeat),
      state(false) {}

  bool getState() const {
    return state;
  }

  void setState(bool newState) {
    state = newState;
  }

  // ON 또는 OFF 상태에 따라 기본 토글 신호 전송
  void sendIr() const {
    uint16_t addrToSend  = (state ? onAddr  : offAddr);
    uint8_t  cmdToSend   = (state ? onCmd   : offCmd);
    uint8_t  repToSend   = (state ? onRepeats : offRepeats);

    Serial.print(F("Sending IR -> Addr: 0x"));
    Serial.print(addrToSend, HEX);
    Serial.print(" Cmd: 0x");
    Serial.print(cmdToSend, HEX);
    Serial.print(" Repeats: ");
    Serial.println(repToSend);

    IrSender.sendNEC(addrToSend, cmdToSend, repToSend);
  }

  // 원하는 addr/cmd/repeats 값으로 바로 IR 전송 (로그 출력 포함)
  void sendCustom(uint16_t addr, uint8_t cmd, uint8_t repeats) const {
    Serial.print(F("Sending IR -> Addr: 0x"));
    Serial.print(addr, HEX);
    Serial.print(" Cmd: 0x");
    Serial.print(cmd, HEX);
    Serial.print(" Repeats: ");
    Serial.println(repeats);

    IrSender.sendNEC(addr, cmd, repeats);
  }
};

// ==============================================
//  무드등, TV, 선풍기 컨트롤러 객체 생성
// ==============================================
// • moodLight: ON=0x00/0x03, OFF=0x00/0x02 (Repeats=3)
// • tv:        ON=0x08/0xD7, OFF=0x08/0xD7 (전원 토글용 동일 신호, 상태 분기 처리)
// • fan:       ON=0x00/0x45, OFF=0x00/0x45 (ON/OFF 분기 처리)
// ==============================================
DeviceController moodLight(0x00, 0x03, 3,
                           0x00, 0x02, 3);

DeviceController tv(0x08, 0xD7, 3,
                    0x08, 0xD7, 3);

DeviceController fan(0x00, 0x45, 3,
                     0x00, 0x45, 3);

// ==============================================
//  선풍기 속도 상태 변수
//    • fanSpeed = 1 또는 2
//    • 팬이 켜질 때 기본값은 1
// ==============================================
int fanSpeed = 1;

void setup() {
  Serial.begin(19200);
  delay(2000);

  Serial.print("Connecting to WiFi SSID: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(ssid, pass);
  delay(3000);

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

  while (!mqttClient.connect(broker, port)) {
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

// 리모컨에서 수신된 IR 신호 로그 출력
void receive_ir_data() {
  if (IrReceiver.decode()) {
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

// ==============================================
//  MQTT에서 받은 문자열 명령 처리
// ==============================================
void processCommand(const String &cmd) {
  // ------------ 무드등 ------------
  if (cmd == "{\"type\": \"Open\"}") {
    // 무드등이 꺼져 있을 때만 ON
    // if (!moodLight.getState()) {
      moodLight.setState(true);
      moodLight.sendIr();
    // }
  }
  else if (cmd == "{\"type\": \"Close\"}") {
    // 무드등이 켜져 있을 때만 OFF
    // if (moodLight.getState()) {
      moodLight.setState(false);
      moodLight.sendIr();
    // }
  }

  // ------------ TV ------------
  else if (cmd == "{\"type\": \"OK\"}") {
    // TV가 꺼져 있을 때만 ON
    if (!tv.getState()) {
      tv.setState(true);
      tv.sendIr();
    }
  }
  else if (cmd == "{\"type\": \"Peace\"}") {
    // TV가 켜져 있을 때만 OFF
    if (tv.getState()) {
      tv.setState(false);
      tv.sendIr();
    }
  }

  // ------------ 선풍기 ---------------
  else if (cmd == "{\"type\": \"Rock\"}") {
    // 팬이 꺼져 있을 때만 ON (켜지면 속도=1)
    if (!fan.getState()) {
      fan.setState(true);
      fan.sendIr();
      fanSpeed = 1;
    }
  }

  else if (cmd == "{\"type\": \"Phone\"}") {
    // 팬이 켜져 있을 때만 OFF
    if (fan.getState()) {
      fan.setState(false);
      fan.sendIr();
      // 꺼질 때 속도 상태 초기화는 필요 없으므로 별도 처리 없음
    }
  }

  // ------------ 선풍기 속도 ↓ (One) ------------
  else if (cmd == "{\"type\": \"One\"}") {
    // 팬이 켜져 있고, 현재 속도가 2일 때만 ↓ 신호 전송
    if (fan.getState() && fanSpeed == 2) {
      // DeviceController::sendCustom() 사용
      fan.sendCustom(0x00, 0x18, 3);  // Addr=0x00, Cmd=0x18, Repeats=3
      fanSpeed = 1;
    }
  }
  // ------------ 선풍기 속도 ↑ (Two) ------------
  else if (cmd == "{\"type\": \"Two\"}") {
    // 팬이 켜져 있고, 현재 속도가 1일 때만 ↑ 신호 전송
    if (fan.getState() && fanSpeed == 1) {
      fan.sendCustom(0x00, 0x15, 3);  // Addr=0x00, Cmd=0x15, Repeats=3
      fanSpeed = 2;
    }
  }

  // ------------ 다중 기기 동시 제어 예시 ------------
  else if (cmd == "{\"type\": \"Heart\"}") {
    // TV ON (켜져 있지 않을 때만)
    if (!tv.getState()) {
      tv.setState(true);
      tv.sendIr();
      delay(100);
    }
    // 무드등 ON (켜져 있지 않을 때만)
    // if (!moodLight.getState()) {
      moodLight.setState(true);
      moodLight.sendIr();
      delay(100);
    // }
    // 선풍기 ON (켜져 있지 않을 때만, 속도=1)
    if (!fan.getState()) {
      fan.setState(true);
      fan.sendIr();
      fanSpeed = 1;
    }
  }
}

String prevCmd = "";

void loop() {
  receive_ir_data();

  int messageSize = mqttClient.parseMessage();
  if (messageSize) {
    String command = "";
    Serial.print(millis());
    Serial.print("ms) Received MQTT topic '");
    Serial.print(mqttClient.messageTopic());
    Serial.print("' (");
    Serial.print(messageSize);
    Serial.println(" bytes):");

    while (mqttClient.available()) {
      command += (char)mqttClient.read();
    }

    //test code for time taken
    if (command == "test complete")
     return;
    mqttClient.beginMessage(topic);
    mqttClient.print("test complete");
    mqttClient.endMessage();

    Serial.print("Parsed command: ");
    Serial.println(command);

    if (prevCmd != command) {
      processCommand(command);
      prevCmd = command;
    }
  }
}
