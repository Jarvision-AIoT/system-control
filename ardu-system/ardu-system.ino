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
#include "secrets.h"
#include <Firebase.h>
#include <ArduinoJson.h>

/* Use the following instance for Test Mode (No Authentication) */
// Firebase fb(REFERENCE_URL);

/* Use the following instance for Locked Mode (With Authentication) */
Firebase fb(REFERENCE_URL, AUTH_TOKEN);

unsigned long previousMillis = 0;
const long interval = 2000;

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

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("-");
    delay(500);
  }

  Serial.println();
  Serial.println("WiFi Connected");
  Serial.println();

  #if defined(ARDUINO_UNOWIFIR4)
    digitalWrite(LED_BUILTIN, HIGH);
  #endif

  fb.setString("Example/IR_COMMAND", "Hello World!");

  /* ----- */ 

  /*
    Set String, Int, Float, or Bool in Firebase
    
    Parameters:
      - path: The path in Firebase where the data will be stored.
      - data: The value to set, which can be of type String, Int, Float, or Bool.

    Returns:
      - HTTP response code as an integer.
        - 200 indicates success.
        - Other codes indicate failure.
  */

  /*
  fb.setString("Example/myString", "Hello World!");
  fb.setInt("Example/myInt", 123);
  fb.setFloat("Example/myFloat", 45.67);
  fb.setBool("Example/myBool", true);
  */

  /*
    Push String, Int, Float, or Bool in Firebase
    
    Parameters:
      - path: The path in Firebase where the data will be stored.
      - data: The value to push, which can be of type String, Int, Float, or Bool.

    Returns:
      - HTTP response code as an integer.
        - 200 indicates success.
        - Other codes indicate failure.
  */
  
  /*fb.pushString("Push", "Foo-Bar");
  fb.pushInt("Push", 890);
  fb.pushFloat("Push", 12.34);
  fb.pushBool("Push", false);
  */
  
  /*
    Get String, Int, Float, or Bool from Firebase
    
    Parameters:
      - path: The path in Firebase from which the data will be retrieved.

    Returns:
      - The value retrieved from Firebase as a String, Int, Float, or Bool.
      - If the HTTP response code is not 200, returns NULL (for String) or 0 (for Int, Float, Bool).
  */

  /*
  String retrievedString = fb.getString("Example/myString");
  Serial.print("Retrieved String:\t");
  Serial.println(retrievedString);

  int retrievedInt = fb.getInt("Example/myInt");
  Serial.print("Retrieved Int:\t\t");
  Serial.println(retrievedInt);

  float retrievedFloat = fb.getFloat("Example/myFloat");
  Serial.print("Retrieved Float:\t");
  Serial.println(retrievedFloat);

  bool retrievedBool = fb.getBool("Example/myBool");
  Serial.print("Retrieved Bool:\t\t");
  Serial.println(retrievedBool);
  */

  /*
    Remove Data from Firebase
    
    Parameters:
      - path: The path in Firebase from which the data will be removed.

    Returns:
      - HTTP response code as an integer.
        - 200 indicates success.
        - Other codes indicate failure.
  */

  //fb.remove("Example");
}

void loop()
{
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;

    // Firebase에서 데이터 가져오기
    String command = fb.getString("Example/IR_COMMAND");
    if (command.length() > 0)
    {
      Serial.print(currentMillis);
      Serial.print(") ");
      Serial.print("Received IR Command: ");
      Serial.println(command);

      // 이후 이 부분에서 command 값을 IR 신호로 파싱 및 전송하도록 처리
    }
    else
    {
      Serial.println("No command received or error occurred.");
    }
  }
}
