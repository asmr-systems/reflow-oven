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

    static const uint8_t StartByte = 0xAA;

    // TODO remove this, not needed anymore.
    // enum class RxResult {
    //     Ok,
    //     Timeout,
    //     StartByteEncountered,
    //     DataUnavailable,
    // };

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

    // TODO remove this commented out code. it is not a great design afterall
    // void handle_command(Command cmd) {
    //         switch (cmd) {
    //         case Command::Status:
    //             send_status();
    //             break;
    //         case Command::Info:
    //             send_info();
    //             break;
    //         case Command::Disable:
    //             this->state->disable();
    //             send_status();
    //             break;
    //         case Command::Enable:
    //             this->state->enable();
    //             send_status();
    //             break;
    //         case Command::Idle:
    //             this->state->go_idle();
    //             send_status();
    //             break;
    //         case Command::SetTemp:
    //             if (read_bytes_until(transmission.buffer, 4) == RxResult::Ok) {
    //                 this->state->request_temp(bytes_2_float(this->transmission.buffer));
    //                 send_scalar_temp_target();
    //             }
    //             break;
    //         case Command::SetTempSlope:
    //             if (read_bytes_until(transmission.buffer, 4) == RxResult::Ok) {
    //                 this->state->request_temp_rate(bytes_2_float(this->transmission.buffer));
    //                 send_slope_temp_target();
    //             }
    //             break;
    //         case Command::SetDutyCycle:
    //             if (read_bytes_until(transmission.buffer, 1) == RxResult::Ok) {
    //                 this->state->request_duty_cycle((uint8_t)transmission.buffer[0]);
    //                 send_duty_cycle();
    //             }
    //             break;
    //         case Command::TuneAll:
    //             break;
    //         case Command::TuneSteadyState:
    //             break;
    //         case Command::TuneVelocity:
    //             break;
    //         case Command::TuneInertia:
    //             break;
    //         case Command::SetData:
    //             break;
    //         default:
    //             break;
    //         }
    // }

    // void handle_incoming_messages_old() {
    //     if (Serial.available()) {
    //         RxResult result = RxResult::StartByteEncountered;

    //         // keep looping until we don't receive the start byte
    //         // this ensures that if we get multiple start bytes in a
    //         // row, we will ignore all but the final one received.
    //         while (result == RxResult::StartByteEncountered) {
    //             Serial.write(0xA0);
    //             // if the first byte is not the start byte, ignore.
    //             result = read_bytes_until(transmission.buffer, 1);
    //             if (result != RxResult::StartByteEncountered)
    //                     return;

    //             // if we are here, a start byte has occured.
    //             Serial.write(0x0A);

    //             // read next byte.
    //             result = read_bytes_until(transmission.buffer, 1);

    //             // quit if a timeout occurred.
    //             if (result == RxResult::Timeout)
    //                 return;

    //             Serial.write(0x0B);

    //             // exit loop if result is ok.
    //         }

    //         Serial.write(0x0C);

    //         // a command has been received.
    //         handle_command((Command)transmission.buffer[0]);
    //     }
    // }

    void handle_incoming_messages() {
        while (Serial.available()) {
            char rx;
            Serial.readBytes(&rx, 1);

            // if the start byte is encountered, begin transmission cycle
            if ((uint8_t)rx == StartByte) {
                this->transmission.in_progress = true;
                this->transmission.bytes_received = 0;
                return;
            }

            // if a transmission is not in progress, ignore current byte.
            if (!this->transmission.in_progress) return;

            // if a transmission is in progress and no bytes have been received,
            // interpret the next byte as the command.
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
                if (!this->collecting_cmd_bytes((uint8_t)rx, 4)) {
                    this->state->request_temp(bytes_2_float(this->transmission.buffer));
                    send_scalar_temp_target();
                    this->transmission.in_progress = false;
                }
                break;
            case Command::SetTempSlope:
                if (!this->collecting_cmd_bytes((uint8_t)rx, 4)) {
                    this->state->request_temp_rate(bytes_2_float(this->transmission.buffer));
                    send_slope_temp_target();
                    this->transmission.in_progress = false;
                }
                break;
            case Command::SetDutyCycle:
                this->state->request_duty_cycle((uint8_t)rx);
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
        Serial.write(0xAA);
        float n = 666.6;
        byte *b = (byte *)&n;
        Serial.write(b, 4);
    }

    void send_scalar_temp_target() {
        // 0xAA StartByte
        Serial.write(StartByte);
        // 0x01 <FLOAT[4]>
        Serial.write((uint8_t)Command::SetTemp);
        uint8_t *b = (uint8_t *)&this->state->requested.point;
        Serial.write(b, 4);
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


    // AA 20 66 A6 26 44
    bool collecting_cmd_bytes(uint8_t rx, uint8_t n_bytes, uint8_t skip_bytes = 2) {
        if (this->transmission.bytes_received == skip_bytes) {
            this->transmission.bytes_remaining = n_bytes;
        }
        else if (this->transmission.bytes_received < skip_bytes) {
            // we are still collecting bytes, but still need to skip
            // some bytes before collecting input data.
            return true;
        }

        if (this->transmission.bytes_remaining > 0) {
            this->transmission.buffer[n_bytes - this->transmission.bytes_remaining] = rx;
            this->transmission.bytes_remaining--;
            return true;
        }
        return false;
    }

    float bytes_2_float(uint8_t buf[4]) {
        // convert buffered bytes into float32 (4 bytes)
        float f;
        memcpy(&f, buf, 4);
        return f;
    }

    // TODO give up on this, its a worse design
    // // reads bytes until buffer is full, the start byte is encountered,
    // // a timeout occurs, or no serial is available.
    // RxResult read_bytes_until(uint8_t *buffer, uint16_t length, uint8_t start_byte = StartByte, uint16_t timeout_ms = 100) {
    //     unsigned long start_time = millis();
    //     uint16_t bytes_read = 0;

    //     Serial.write(0xFF);
    //     while (bytes_read < length) {
    //         // check for timeout
    //         if (millis() - start_time >= timeout_ms)
    //             return RxResult::Timeout;

    //         // read 1 byte
    //         char rx;
    //         Serial.readBytes(&rx, 1);
    //         Serial.write((uint8_t)rx);

    //         // check for start byte
    //         if ((uint8_t)rx == StartByte)
    //             return RxResult::StartByteEncountered;

    //         buffer[bytes_read++] = (uint8_t)rx;
    //     }

    //     Serial.write(0xEE);
    //     return RxResult::Ok;
    // }
};

#endif
