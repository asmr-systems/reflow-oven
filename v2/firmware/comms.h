#ifndef REFLOW_COMMS_H
#define REFLOW_COMMS_H

// #include <ArduinoJson.h>
#include "state.h"

// Binary protocol
// all transmissions begin with a scream byte 0xAA which is not a valid
// ascii character importantly.
// # Commands (from daemon)
//

class Comms {
public:
    int baud;
    State* state;
    int temp_update_ms;

    const uint8_t StartByte = 0xAA;

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

    void handle_incoming_messages() {
        while (Serial.available()) {
            char rx;
            Serial.readBytes(&rx, 1);

            if ((uint8_t)rx == StartByte) {
                this->transmission.in_progress = true;
                this->transmission.bytes_received = 0;
                return;
            }

            if (!this->transmission.in_progress) return;

            if (this->transmission.in_progress && this->transmission.bytes_received == 0) {
                this->transmission.command = (Command)rx;
                this->transmission.bytes_received++;
            }


            switch (this->transmission.command) {
            case Command::Status:
                send_status();
                this->transmission.in_progress = false;
                break;
            case Command::Info:
                send_info();
                this->transmission.in_progress = false;
                break;
            case Command::Disable:
                this->state->disable();
                send_status();
                this->transmission.in_progress = false;
                break;
            case Command::Enable:
                this->state->enable();
                send_status();
                this->transmission.in_progress = false;
                break;
            case Command::Idle:
                this->state->go_idle();
                send_status();
                this->transmission.in_progress = false;
                break;
            case Command::SetTemp:
                uint8_t n_bytes = 2;
                if (this->transmission.bytes_received == 1) {
                    this->transmission.bytes_remaining = n_bytes;
                }
                if (this->transmission.bytes_remaining > 0) {
                    this->transmission.buffer[n_bytes - this->transmission.bytes_remaining] = (uint8_t)rx;
                } else {
                    this->state->request_temp(bytes_2_float(this->transmission.buffer));
                    send_scalar_temp_target();
                    this->transmission.in_progress = false;
                }
                break;
            case Command::SetTempSlope:
                this->state->request_temp_rate();
                send_slope_temp_target();
                this->transmission.in_progress = false;
                break;
            case Command::SetDutyCycle:
                this->state->request_duty_cycle();
                send_duty_cycle();
                this->transmission.in_progress = false;
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
                this->transmission.in_progress = false;
                break;
            }

            this->transmission.bytes_received++;
        }
    }

    void send_status() {
        // 0xAA StartByte
        Serial.write(StartByte);
        // 0x00 <DATA>
        Serial.write((uint8_t)Command::Status);
        // DATA[0] - 1:enabled, 0:disabled
        uint8_t ctrl_enabled = this->state->heating_enabled ? 0x01 : 0x00;
        // DATA[1:2] - 0:idle, 1:running, 2:tuning
        uint8_t ctrl_stat = 0x00;
        switch (this->state->status) {
        case ControlStatus::Idle:
            ctrl_stat = 0x00;
            break;
        case ControlStatus::Running:
            ctrl_stat = 0x01;
            break;
        case ControlStatus::Tuning:
            ctrl_stat = 0x10;
            break;
        }
        // DATA[3:4] - 0:n/a, 1:tuning steady-state, 2:tuning velocity, 3:tuning inertia
        uint8_t tuning_phase = 0x00;
        if (this->state->status == ControlStatus::Tuning) {
            switch (this->state->tuning_phase) {
            case TuningPhase::SteadyState:
                tuning_phase = 0x01;
                break;
            case TuningPhase::Velocity:
                tuning_phase = 0x10;
                break;
            case TuningPhase::Inertia:
                tuning_phase = 0x11;
                break;
            }
        }
        // DATA[5:6] - 0:n/a, 1:set_point, 2:rate, 3:duty_cycle
        uint8_t ctrl_mode = 0x00;
        if (this->state->status == ControlStatus::Running) {
            switch (this->state->mode) {
            case ControlMode::SetPoint:
                ctrl_mode = 0x01;
                break;
            case ControlMode::Rate:
                ctrl_mode = 0x10;
                break;
            case ControlMode::DutyCycle:
                ctrl_mode = 0x11;
                break;
            }
        }
        uint8_t data = (ctrl_mode << 5) | (tuning_phase << 3) | (ctrl_stat << 1) | ctrl_enabled;
        Serial.write(data);
    }

    void send_info() {

    }

    void send_scalar_temp_target() {

    }

    void send_slope_temp_target() {

    }

    void send_duty_cycle() {

    }

    // void send_status_old() {
    //     Serial.print("{\"action\":\"get\",\"type\":\"status\",\"data\":{\"connected\":true,\"running\":");
    //     if (state->running) {
    //         Serial.print("true,");
    //     } else {
    //         Serial.print("false,");
    //     }
    //     Serial.print("\"learning\":");
    //     if (state->learning) {
    //         Serial.print("true,");
    //     } else {
    //         Serial.print("false,");
    //     }
    //     Serial.print("\"testing\":");
    //     if (state->testing) {
    //         Serial.print("true,");
    //     } else {
    //         Serial.print("false,");
    //     }
    //     Serial.print("\"phase\":");
    //     if (state->learning) {
    //         if (state->learning_phase == LearningPhase::MaxRamp) {
    //             Serial.print("\"max-ramp\"");
    //         } else if (state->learning_phase == LearningPhase::Maintainence) {
    //             Serial.print("\"maintainence\"");
    //         } else if (state->learning_phase == LearningPhase::Cooldown) {
    //             Serial.print("\"cooldown\"");
    //         }
    //     } else if (state->running) {
    //         // TODO maybe put reflow phase
    //         Serial.print("\"running\"");
    //     } else {
    //         Serial.print("\"none\"");
    //     }
    //     Serial.println("}}");
    // }

    // void send_temperature() {
    //     static unsigned long ping_start = 0;
    //     if (millis() - ping_start < temp_update_ms) return;
    //     ping_start = millis();

    //     Serial.print("{\"action\":\"get\",");
    //     Serial.print("\"type\":\"temp\",\"data\":{\"temp\":");
    //     Serial.print(state->data.temp);
    //     Serial.print(",\"time\":");
    //     Serial.print(state->data.time);
    //     Serial.println("}}");
    // }


    // void handle_incoming_messages_old() {
    //     while (Serial.available()) {
    //         String incoming = Serial.readString();


    //         if (incoming == "status") {
    //             send_status();
    //             return;
    //         }

    //         if (incoming == "start") {
    //             state->running = true;
    //             state->learning = false;
    //             state->testing = false;
    //             state->driver->enable();
    //             state->start_ms = millis();
    //             send_status();
    //             return;
    //         }

    //         if (incoming == "stop") {
    //             state->running = false;
    //             state->learning = false;
    //             state->testing = false;
    //             state->driver->disable();
    //             send_status();
    //             return;
    //         }

    //         if (incoming == "learn") {
    //             state->begin_learning();
    //             send_status();
    //             return;
    //         }

    //         if (incoming == "test") {
    //             state->running = false;
    //             state->learning = false;
    //             state->testing = true;
    //             state->driver->enable();
    //             send_status();
    //             return;
    //         }

    //         // this is when we load profiles
    //         deserializeJson(json, incoming);
    //         String msg_type = json["type"];  // TODO check if it is profile
    //     }
    // }

private:
    struct {
        bool    in_progress     = false;
        uint8_t bytes_received  = 0;
        uint8_t bytes_remaining = 0;
        uint8_t buffer[50]      = {0};
        Command command         = Command::Status;
    } transmission;

    void bytes_2_float(uint8_t &buf) {
        // TODO convert buffered bytes into float32 (4 bytes)
    }

};

#endif
