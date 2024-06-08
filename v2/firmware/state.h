#ifndef REFLOW_STATE_H
#define REFLOW_STATE_H

#include <SPI.h>
#include "MAX6675.h"

#include "AT24C32E.h"
#include "driver.h"


// enum class LearningPhase {
//     MaxRamp,
//     Maintainence,
//     Cooldown,
// };

enum class TuningPhase {
    All,
    SteadyState,
    Velocity,
    Inertia,
};

enum class ControlStatus {
    Idle,
    Running,
    Tuning,
};

enum class ControlMode {
    SetPoint,
    Rate,
    DutyCycle,
};

class State {
public:
    static constexpr uint16_t MaxTemp          = 260;
    static constexpr uint16_t SteadyStateTemp  = 120;

    ControlStatus status       = ControlStatus::Idle;
    ControlMode   mode         = ControlMode::SetPoint;
    TuningPhase   tuning_phase = TuningPhase::All;

    bool heating_enabled = false;

    struct {
        float            temp = 0; // C
        unsigned long    time = 0; // ms
    } data;

    struct {
        float       point        = 25; // C TODO REMOVE
        float       rate         = 0;  // C/s TODO REMOVE
        float       duty_cycle   = 0;  // [0, 1.0] TODO REMOVE
        float       value        = 0;  // temp point, rate, or duty cycle
        TuningPhase tuning_phase = TuningPhase::All;
    } requested;

    State(int therm_cs_pin, Driver* driver)
        : therm_cs_pin(therm_cs_pin), thermocouple(MAX6675(therm_cs_pin, &SPI)) {}

    void begin() {
        pinMode(therm_cs_pin, OUTPUT);
        SPI.begin();
        thermocouple.begin();
        // eeprom.begin();

        reset();
    }

    void reset() {
        status       = ControlStatus::Idle;
        mode         = ControlMode::SetPoint;
        tuning_phase = TuningPhase::All;

        heating_enabled = false;
    }

    void disable() {
        this->heating_enabled = false;
        this->status = ControlStatus::Idle;
    }

    void enable() {
        this->heating_enabled = true;
    }

    void go_idle() {
        this->status = ControlStatus::Idle;
    }

    void start() {
        // ignore if we are tuning or disabled
        if (this->status == ControlStatus::Tuning || !this->heating_enabled)
            return;

        this->status = ControlStatus::Running;
    }

    void request_temp(float temp) {
        this->requested.value = temp;
        this->mode = ControlMode::SetPoint;
    }

    void request_temp_rate(float temp_rate) {
        this->requested.value = temp_rate;
        this->mode = ControlMode::Rate;
    }

    void request_duty_cycle(float duty_cycle) {
        if (duty_cycle > 1.0)
            duty_cycle = 1.0;
        this->requested.value = duty_cycle;
        this->mode = ControlMode::DutyCycle;
    }

    void request_tuning_phase(TuningPhase phase) {
        requested.tuning_phase = phase;
        this->status = ControlStatus::Tuning;
    }

    void set_data(uint16_t addr, String data) {
        // addr += 3; // offset for internal variables
        // each data slot is 60 bytes long with the size as the first byte
        // uint8_t c[60];
        // uint8_t size = 1;//(uint8_t)data.length();
        // c[0] = 'Z';
        char c = 'Z';
        // data.toCharArray((char *)c, size);
        // eeprom.write(addr, c, size);
        eeprom.write(0, (uint8_t)c);
    }

    String get_data(uint16_t addr) {
        // addr += 3; // offset for internal variables
        uint8_t size = 1;
        uint8_t c[60];
        // eeprom.read(addr, 1, &size);
        // eeprom.read(addr, size, c);
        c[0] = (char)eeprom.read(addr);


        // Serial.println(c[0]);

        String data = String((char *)c).substring(0, size);

        return data;
    }

    void record_temp() {
        // MAX6675 needs 250ms between reads.
        static unsigned long last_read_ms = 0;
        if (millis() - last_read_ms < 250) return;
        last_read_ms = millis();

        int status = thermocouple.read();
        this->data.temp = thermocouple.getTemperature();
        this->data.time = millis();
    }

private:
    int therm_cs_pin;
    MAX6675 thermocouple;
    AT24C32E eeprom;
};

#endif
