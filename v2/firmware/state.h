#ifndef REFLOW_STATE_H
#define REFLOW_STATE_H

#include <SPI.h>
#include "MAX6675.h"


class State {
public:
    bool          running = false;
    unsigned long start_ms = 0;
    struct {
        double    temp = 0; // C
        double    time = 0; // s
    } data;

    State(int therm_cs_pin)
        : therm_cs_pin(therm_cs_pin) {}

    void begin() {
        pinMode(therm_cs_pin, OUTPUT);
        SPI.begin();
        thermocouple.begin(therm_cs_pin);
    }

    void record_temp() {
        thermocouple.read();
        this->data.temp = thermocouple.getTemperature();
        if (this->running) {
            this->data.time = (start_ms - millis())/1000.0;
        } else {
            this->data.time = 0;
        }
    }

private:
    int therm_cs_pin;
    MAX6675 thermocouple;
};

#endif
