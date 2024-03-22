#include <ArduinoJson.h>
#include "clock.h"

JsonDocument json; // parsed json
String incoming;   // raw incoming message

unsigned long start_millis_monitoring;
unsigned long start_millis;
double elapsed_seconds = 0;
double current_temp = 160.6;

void setup() {
  Serial.begin(9600);
  start_millis_monitoring = millis();
}

bool running = false;

void send_status() {
    Serial.print("{\"action\":\"get\",\"type\":\"status\",\"data\":{\"connected\":true,\"running\":");
    if (running) {
        Serial.print("true");
    } else {
        Serial.print("false");
    }
    Serial.println("}}");
}
void handle_message() {
  incoming = Serial.readString();// read the incoming data as string

  if (incoming == "status") {
      send_status();
  }
  else if (incoming == "start") {
    running = true;
    start_millis = millis();
    send_status();
  }
  else if (incoming == "stop") {
    running = false;
    send_status();
  }
  else {
    // this is when we load profiles
    deserializeJson(json, incoming);
    String msg_type = json["type"];  // TODO check if it is profile
  }
}

void loop() {
  while(Serial.available()) {
    handle_message();
  }

  if (running) {

    if (millis() - start_millis_monitoring > 100) {
      start_millis_monitoring = millis();
      elapsed_seconds = (start_millis_monitoring - start_millis)/1000.0;
      Serial.print("{\"action\":\"get\",");
      Serial.print("\"type\":\"temp\",\"data\":{\"temp\":");
      Serial.print(current_temp);
      Serial.print(",\"time\":");
      Serial.print(elapsed_seconds);
      Serial.println("}}");
    }
  }
}
