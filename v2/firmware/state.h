#ifndef REFLOW_STATE_H
#define REFLOW_STATE_H

#include <SPI.h>
#include "MAX6675.h"

#include "driver.h"


// enum class LearningPhase {
//     MaxRamp,
//     Maintainence,
//     Cooldown,
// };

enum class TuningStage {
    All,
    SteadyState,
    Velocity,
    Inertia,
}

enum class ControlState {
    Idle,
    Tuning,
    Running,
};

enum class ControlMode {
    SetPoint,
    Rate,
    DutyCycle,
}

class State {
public:
    static constexpr uint16_t MaxTemp          = 260;
    static constexpr uint16_t SteadyStateTemp  = 120;

    ControlState control      = ControlState::Idle;
    ControlMode  mode         = ControlMode::SetPoint;
    TuningPhase  tuning_phase = TuningPhase::All;

    struct {
        double           temp = 0; // C
        unsigned long    time = 0; // ms
    } data;

    struct {
        double      point        = 25; // C
        double      rate         = 0;  // C/s
        uint8_t     duty_cycle   = 0;  // [0, 100]
        TuningPhase tuning_phase = TuningPhase::All;
    } requested;

    State(int therm_cs_pin, Driver* driver)
        : therm_cs_pin(therm_cs_pin), thermocouple(MAX6675(therm_cs_pin, &SPI)) {}

    void begin() {
        pinMode(therm_cs_pin, OUTPUT);
        SPI.begin();
        thermocouple.begin();
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

    void begin_learning() {
        this->running = false;
        this->learning = true;
        this->testing = false;
        this->start_ms = millis();
        this->learning_phase = LearningPhase::MaxRamp;
    }
private:
    int therm_cs_pin;
    MAX6675 thermocouple;
};

#endif
