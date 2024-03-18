#include <ArduinoJson.h>

JsonDocument json; // parsed json
String incoming;   // raw incoming message

void setup() {
  Serial.begin(9600);
}

void handle_message() {
  incoming = Serial.readString();// read the incoming data as string
  deserializeJson(json, incoming);
  String command = json["command"];

  if (command == "status") {
    Serial.println("{\"command\":\"status\",\"data\":\"connected\"}");
  }
}

void loop() {
  while(Serial.available()) {
    handle_message();
  }
}
