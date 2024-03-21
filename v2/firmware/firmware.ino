#include <ArduinoJson.h>

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

void handle_message() {
  incoming = Serial.readString();// read the incoming data as string

  if (incoming == "status") {
    Serial.println("{\"command\":\"status\",\"data\":\"connected\"}");
  }
  else if (incoming == "start") {
    Serial.println("{\"command\":\"start\",\"data\":\"ok\"}");
    running = true;
    start_millis = millis();
  }
  else if (incoming == "stop") {
    Serial.println("{\"command\":\"stop\",\"data\":\"ok\"}");
    running = false;
  }
  else {
    // this is when we load profiles
    deserializeJson(json, incoming);
    String command = json["command"];
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
      Serial.print("{\"command\":\"monitor\",\"temp\":");
      Serial.print(current_temp);
      Serial.print(",\"time\":");
      Serial.print(elapsed_seconds);
      Serial.println("}");
    }
  }
}
