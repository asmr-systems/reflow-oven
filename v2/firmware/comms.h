#ifndef REFLOW_COMMS_H
#define REFLOW_COMMS_H

#include "state.h"


class Comms {
public:
    int baud;
    State* state;
    int temp_update_ms;

    static const uint8_t StartByte = 0x02; // ASCII STX
    static const uint8_t Delimiter = 0x20; // ASCII SPACE

    enum class Command {
        Status          = 'A',
        Info            = 'B',
        Disable         = 'C',
        Enable          = 'D',
        Idle            = 'E',
        SetTemp         = 'F',
        SetTempSlope    = 'G',
        SetDutyCycle    = 'H',
        TuneAll         = 'I',
        TuneSteadyState = 'J',
        TuneVelocity    = 'K',
        TuneInertia     = 'L',
        SetData         = 'M',
        GetData         = 'N',
        Reset           = 'Z',
    };

    Comms(State* state, int temp_update_ms = 100, int baud = 9600)
        : state(state), temp_update_ms(temp_update_ms), baud(baud) {}

    void begin() {
        Serial.begin(baud);
    }

    void handle_incoming_messages() {
        while (Serial.available()) {
            String msg = Serial.readString();
            msg.trim();

            // if first character isn't start transmission, ignore message.
            if (msg[0] != StartByte)
                return;

            uint8_t data[60];

            switch ((Command)msg[1]) {
            case Command::Reset:
                this->state->reset();
                send_status();
                break;
            case Command::Status:
                send_status();
                break;
            case Command::Info:
                send_info();
                break;
            case Command::Disable:
                this->state->disable();
                send_status();
                break;
            case Command::Enable:
                this->state->enable();
                send_status();
                break;
            case Command::Idle:
                this->state->go_idle();
                send_status();
                break;
            case Command::SetTemp:
                this->state->request_temp(get_word(msg, 1).toFloat());
                send_scalar_temp_target();
                break;
            case Command::SetTempSlope:
                this->state->request_temp_rate(get_word(msg, 1).toFloat());
                send_slope_temp_target();
                break;
            case Command::SetDutyCycle:
                this->state->request_duty_cycle(get_word(msg, 1).toInt());
                send_duty_cycle();
                break;
            case Command::TuneAll:
                this->state->request_tuning_phase(TuningPhase::All);
                send_status();
                break;
            case Command::TuneSteadyState:
                this->state->request_tuning_phase(TuningPhase::SteadyState);
                send_status();
                break;
            case Command::TuneVelocity:
                this->state->request_tuning_phase(TuningPhase::Velocity);
                send_status();
                break;
            case Command::TuneInertia:
                this->state->request_tuning_phase(TuningPhase::Inertia);
                send_status();
                break;
            case Command::SetData:
                this->state->set_data(
                    get_word(msg, 1).toInt(),  // data addr
                    get_word(msg, 2, -1)
                );
                // send_info();
                // break;
            case Command::GetData:
                this->state->get_data(
                    get_word(msg, 1).toInt(),  // data addr
                    data
                );
                Serial.write(StartByte);
                Serial.write((uint8_t)Command::GetData);
                Serial.write(Delimiter);
                Serial.print(get_word(msg, 1));
                Serial.println(*data);
                break;
            default:
                break;

            }
        }
    }

    void send_status() {
        // StartByte
        Serial.write(StartByte);

        // Status <DATA>
        Serial.write((uint8_t)Command::Status);
        Serial.write(Delimiter);

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

        char buf[4];
        sprintf(buf, "%02X", data);
        Serial.println(buf);
    }

    void send_info() {
        // StartByte
        Serial.write(StartByte);

        // Info <DATA>
        Serial.write((uint8_t)Command::Info);
        Serial.write(Delimiter);

        // TODO send more stuff
        Serial.println();
    }

    void send_scalar_temp_target() {
        // StartByte
        Serial.write(StartByte);
        // SetTemp STR_FLOAT
        Serial.write((uint8_t)Command::SetTemp);
        Serial.write(Delimiter);

        Serial.println(this->state->requested.point);
    }

    void send_slope_temp_target() {
        // StartByte
        Serial.write(StartByte);
        // SetTempSlope STR_FLOAT
        Serial.write((uint8_t)Command::SetTempSlope);
        Serial.write(Delimiter);

        Serial.println(this->state->requested.rate);
    }

    void send_duty_cycle() {
        // StartByte
        Serial.write(StartByte);
        // SetDutyCycle
        Serial.write((uint8_t)Command::SetDutyCycle);
        Serial.write(Delimiter);

        Serial.println(this->state->requested.duty_cycle);
    }

private:
    String get_word(String msg, int word_start, int n = 1) {
        uint8_t head_idx = 0, tail_idx = 0, word = 0;
        bool head_anchored = false;
        for (uint16_t i = 0; i < msg.length(); i++) {
            if (msg[i] == Delimiter || i == msg.length()-1) {
                word++;

                if (!head_anchored) {
                    if (word_start == word) {
                        // the first index of the substring will be
                        // the next index (after the delimiter)
                        head_idx = i+1;
                        head_anchored = true;

                        // if the ending of the substring is the remainder
                        // of the message, return now.
                        if (n < 0) {
                            return msg.substring(head_idx, msg.length());
                        }
                    }
                }
                else {
                    if (word_start + n == word) {
                        return msg.substring(head_idx, i == msg.length()-1 ? i+1 : i);
                    }
                }
            }
        }

        if (head_idx == 0 && tail_idx == 0) {
            return "";
        }

        return msg.substring(head_idx, msg.length());
    }
};

#endif
