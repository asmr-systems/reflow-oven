#ifndef REFLOW_COMMS_H
#define REFLOW_COMMS_H

#include <ArduinoJson.h>
#include "state.h"

class Comms {
public:
    int baud;
    State* state;

    Comms(State* state, int baud = 9600)
        : state(state), baud(baud) {}

    void begin() {
        Serial.begin(baud);
    }

    void send_status() {
        Serial.print("{\"action\":\"get\",\"type\":\"status\",\"data\":{\"connected\":true,\"running\":");
        if (state->running) {
            Serial.print("true");
        } else {
            Serial.print("false");
        }
        Serial.println("}}");
    }

    void send_temperature() {
        Serial.print("{\"action\":\"get\",");
        Serial.print("\"type\":\"temp\",\"data\":{\"temp\":");
        Serial.print(state->data.temp);
        Serial.print(",\"time\":");
        Serial.print(state->data.time);
        Serial.println("}}");
    }

    void handle_incoming_messages() {
        while (Serial.available()) {
            String incoming = Serial.readString();

            if (incoming == "status") {
                send_status();
                return;
            }

            if (incoming == "start") {
                state->running = true;
                state->start_ms = millis();
                send_status();
                return;
            }

            if (incoming == "stop") {
                state->running = false;
                send_status();
                return;
            }


            // this is when we load profiles
            deserializeJson(json, incoming);
            String msg_type = json["type"];  // TODO check if it is profile
        }
    }

private:
    JsonDocument json;
};

#endif
