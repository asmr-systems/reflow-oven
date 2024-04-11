#ifndef REFLOW_COMMS_H
#define REFLOW_COMMS_H

#include <ArduinoJson.h>
#include "state.h"

class Comms {
public:
    int baud;
    State* state;
    int temp_update_ms;

    enum class Command {
        Status          = 0x00,
        Info            = 0x01,
        Disable         = 0x10,
        Enable          = 0x11,
        Idle            = 0x12,
        SetTemp         = 0x20,
        SetTempSlope    = 0x21,
        SetDutyCycle    = 0x22,
        TuneAll         = 0x30,
        TuneSteadyState = 0x31,
        TuneVelocity    = 0x32,
        TuneInertia     = 0x33,
        SetData         = 0x40,
    };

    Comms(State* state, int temp_update_ms = 100, int baud = 9600)
        : state(state), temp_update_ms(temp_update_ms), baud(baud) {}

    void begin() {
        Serial.begin(baud);
    }

    void send_status_old() {
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
                Serial.print("\"max-ramp\"");
            } else if (state->learning_phase == LearningPhase::Maintainence) {
                Serial.print("\"maintainence\"");
            } else if (state->learning_phase == LearningPhase::Cooldown) {
                Serial.print("\"cooldown\"");
            }
        } else if (state->running) {
            // TODO maybe put reflow phase
            Serial.print("\"running\"");
        } else {
            Serial.print("\"none\"");
        }
        Serial.println("}}");
    }

    void send_temperature() {
        static unsigned long ping_start = 0;
        if (millis() - ping_start < temp_update_ms) return;
        ping_start = millis();

        Serial.print("{\"action\":\"get\",");
        Serial.print("\"type\":\"temp\",\"data\":{\"temp\":");
        Serial.print(state->data.temp);
        Serial.print(",\"time\":");
        Serial.print(state->data.time);
        Serial.println("}}");
    }

    void send_status() {
        Serial.write((uint8_t)Command::Status);
        Serial.write((uint8_t)this->state->heat_enabled ? 0x01 : 0x00);
        Serial.write((uint8_t)this->state->control == ControlState::Idle ? 0x00 : 0x01);
        Serial.write((uint8_t)this->state->control == ControlState::Tuning ? 0x00 : 0x01);
        Serial.write((uint8_t)this->state->control == ControlState::Running ? 0x00 : 0x01);
    }

    void handle_incoming_messages() {
        while (Serial.available()) {
            Command command;
            Serial.readBytes(&(int)command, 1);
            switch (command) {
            case Command::Status:
                send_status();
                break;
            case Command::Info:
                break;
            case Command::Disable:
                break;
            case Command::Enable:
                break;
            case Command::Idle:
                break;
            case Command::SetTemp:
                break;
            case Command::SetTempSlop:
                break;
            case Command::SetDutyCycle:
                break;
            case Command::TuneAll:
                break;
            case Command::TuneSteadyState:
                break;
            case Command::TuneVelocity:
                break;
            case Command::TuneInertia:
                break;
            case Command::SetData:
                break;
            default:
                break;
            }
        }
    }

    void handle_incoming_messages_old() {
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
                state->driver->enable();
                state->start_ms = millis();
                send_status();
                return;
            }

            if (incoming == "stop") {
                state->running = false;
                state->learning = false;
                state->testing = false;
                state->driver->disable();
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
                state->driver->enable();
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
