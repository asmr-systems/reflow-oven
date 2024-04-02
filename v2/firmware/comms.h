#ifndef REFLOW_COMMS_H
#define REFLOW_COMMS_H

#include <ArduinoJson.h>
#include "state.h"

class Comms {
public:
    int baud;
    State* state;
    int temp_update_ms;

    Comms(State* state, int temp_update_ms = 100, int baud = 9600)
        : state(state), temp_update_ms(temp_update_ms), baud(baud) {}

    void begin() {
        Serial.begin(baud);
    }

    void send_status() {
        Serial.print("{\"action\":\"get\",\"type\":\"status\",\"data\":{\"connected\":true,\"running\":");
        if (state->running) {
            Serial.print("true,");
        } else {
            Serial.print("false,");
        }
        Serial.print("\"learning\":");
        if (state->learning) {
            Serial.print("true,");
        } else {
            Serial.print("false,");
        }
        Serial.print("\"testing\":");
        if (state->testing) {
            Serial.print("true,");
        } else {
            Serial.print("false,");
        }
        Serial.print("\"phase\":");
        if (state->learning) {
            if (state->learning_phase == LearningPhase::MaxRamp) {
                Serial.print("max-ramp");
            } else if (state->learning_phase == LearningPhase::Maintainence) {
                Serial.print("maintainence");
            } else if (state->learning_phase == LearningPhase::Cooldown) {
                Serial.print("cooldown");
            }
        } else if (state->running) {
            // TODO maybe put reflow phase
            Serial.print("running");
        } else {
            Serial.print("none");
        }
        Serial.println("}}");
    }

    void send_temperature() {
        static int ping_start = 0;
        if (millis() - ping_start < temp_update_ms) return;
        ping_start = millis();

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
                state->learning = false;
                state->testing = false;
                state->start_ms = millis();
                send_status();
                return;
            }

            if (incoming == "stop") {
                state->running = false;
                state->learning = false;
                state->testing = false;
                send_status();
                return;
            }

            if (incoming == "learn") {
                state->begin_learning();
                send_status();
                return;
            }

            if (incoming == "test") {
                state->running = false;
                state->learning = false;
                state->testing = true;
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
