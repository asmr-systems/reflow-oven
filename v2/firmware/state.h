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
        : therm_cs_pin(therm_cs_pin), thermocouple(MAX6675(therm_cs_pin, &SPI)) {}

    void begin() {
        pinMode(therm_cs_pin, OUTPUT);
        SPI.begin();
        thermocouple.begin();
    }

    void record_temp() {
        // MAX6675 needs 250ms between reads.
        static int last_read_ms = 0;
        if (millis() - last_read_ms < 250) return;
        last_read_ms = millis();

        int status = thermocouple.read();
        this->data.temp = thermocouple.getTemperature();
        if (this->running) {
            this->data.time = (double)(millis() - start_ms)/1000.0;
        } else {
            this->data.time = 0;
        }
    }

private:
    int therm_cs_pin;
    MAX6675 thermocouple;
};

#endif
