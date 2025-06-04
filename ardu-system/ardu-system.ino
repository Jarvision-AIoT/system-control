#include <Arduino.h>
#include "secrets.h"                // WIFI_SSID, WIFI_PASSWORD
#include <ArduinoMqttClient.h>
#include "PinDefinitionsAndMore.h"  // IR_RECEIVE_PIN, IR_SEND_PIN
#include <IRremote.hpp>
#include <WiFiS3.h>                 // Arduino UNO R4 WiFi
#include <Servo.h>

#define DECODE_NEC
#define DECODE_DISTANCE_WIDTH

char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASSWORD;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = BROKER_IP;
const char topic[]  = TOPIC_ID;
int        port     = PORT_ADDR;

Servo servo;
int value = 0; // 각도 조절 변수

// ==============================================
//  기기 상태 및 IR 신호 관리 클래스
//  • 토글용 ON/OFF보내기(sendIr) 및
//    원하는 addr/cmd/repeats 바로 보내기(sendCustom)
// ==============================================
class DeviceController {
private:
  uint16_t onAddr;
  uint8_t  onCmd;
  uint8_t  onRepeats;

  uint16_t offAddr;
  uint8_t  offCmd;
  uint8_t  offRepeats;

  bool state; // false = OFF(추정), true = ON(추정)

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
  void toggleState() {
    state = !state;
  }

  // ON/OFF 상태에 따라 토글 신호 전송
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

  // 커스텀 addr/cmd/repeats 값으로 IR 전송
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
// • tv:        ON=0x08/0xD7, OFF=0x08/0xD7 (토글 신호)
// • fan:       ON=0x00/0x45, OFF=0x00/0x45 (토글 신호, 실제 상태 알 수 없음)
// ==============================================
DeviceController moodLight(0x00, 0x03, 3,
                           0x00, 0x02, 3);

DeviceController tv(0x08, 0xD7, 3,
                    0x08, 0xD7, 3);

// “fan”은 ON=0x00/0x45, OFF=0x00/0x45(토글) 신호만 보냄. 실제 켜짐/꺼짐은 알 수 없음
DeviceController fan(0x00, 0x45, 3,
                     0x00, 0x45, 3);

void setup() {
  servo.attach(7);
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
    // 무드등 ON
    value = 0;
    servo.write(value);
    delay(300);
    moodLight.setState(true);
    moodLight.sendIr();
    Serial.println(moodLight.getState());
  }
  else if (cmd == "{\"type\": \"Close\"}") {
    // 무드등 OFF
    value = 0;
    servo.write(value);
    delay(300);
    moodLight.setState(false);
    moodLight.sendIr();
    Serial.println(moodLight.getState());
  }

  // ------------ TV (토글) ------------
  else if (cmd == "{\"type\": \"Peace\"}") {
    // TV 토글 신호
    value = 90;
    servo.write(value);
    delay(300);
    tv.toggleState();
    tv.sendIr();
    Serial.println(tv.getState());
  }

  // ------------ 선풍기 (토글) ------------
  else if (cmd == "{\"type\": \"Rock\"}") {
    // 선풍기 팬 토글 신호
    value = 180;
    servo.write(value);
    delay(300);
    fan.toggleState();
    fan.sendIr();
    Serial.println(fan.getState());
  }

  // ------------ 선풍기 속도 ↓ (One) ------------
  else if (cmd == "{\"type\": \"One\"}") {
    // 팬이 켜져 있는 상태에서만 ↓ 신호 전송
    if (fan.getState()) {
      value = 180;
      servo.write(value);
      delay(300);
      fan.sendCustom(0x00, 0x18, 3); // Addr=0x00, Cmd=0x18, Repeats=3
    }
  }
  // ------------ 선풍기 속도 ↑ (Two) ------------
  else if (cmd == "{\"type\": \"Two\"}") {
    // 팬이 켜져 있는 상태에서만 ↑ 신호 전송
    if (fan.getState()) {
      value = 180;
      servo.write(value);
      delay(300);
      fan.sendCustom(0x00, 0x15, 3); // Addr=0x00, Cmd=0x15, Repeats=3
    }
  }

  // ------------ 다중 기기 동시 제어 (Heart) ------------
  else if (cmd == "{\"type\": \"Heart\"}") {
    // 무드등 토글
    value = 0;
    servo.write(value);
    delay(300);
    moodLight.toggleState();
    moodLight.sendIr();
    delay(300);

    // TV 토글
    value = 90;
    servo.write(value);
    delay(300);
    tv.toggleState();
    tv.sendIr();
    delay(300);

    // 팬 토글
    value = 180;
    servo.write(value);
    delay(300);
    fan.toggleState();
    fan.sendIr();
    delay(300);
    Serial.println(moodLight.getState());
    Serial.println(tv.getState());
    Serial.println(fan.getState());
  }
}

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
    Serial.print("Parsed command: ");
    Serial.println(command);

    processCommand(command);
  }
}
