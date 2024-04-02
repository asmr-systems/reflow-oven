#ifndef REFLOW_STATE_H
#define REFLOW_STATE_H

#include <SPI.h>
#include "MAX6675.h"

#include "driver.h"


enum class LearningPhase {
    MaxRamp,
    Maintainence,
    Cooldown,
};

class State {
public:
    static constexpr uint16_t MaxTemp          = 120; // 260
    static constexpr uint16_t MaintainenceTemp = 120;

    Driver* driver;

    bool          running  = false;
    bool          learning = false;
    bool          testing  = false;
    unsigned long start_ms = 0;
    struct {
        double    temp = 0; // C
        double    time = 0; // s
    } data;
    LearningPhase learning_phase = LearningPhase::MaxRamp;

    State(int therm_cs_pin, Driver* driver)
        : therm_cs_pin(therm_cs_pin), driver(driver), thermocouple(MAX6675(therm_cs_pin, &SPI)) {}

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
        if (this->running || (this->learning && this->learning_phase != LearningPhase::Cooldown)) {
            this->data.time = (double)(millis() - start_ms)/1000.0;
        } else {
            this->data.time = 0;
        }
    }

    void begin_learning() {
        this->running = false;
        this->learning = true;
        this->testing = false;
        this->driver->enable();
        this->start_ms = millis();
        this->learning_phase = LearningPhase::MaxRamp;
    }
private:
    int therm_cs_pin;
    MAX6675 thermocouple;
};

#endif
