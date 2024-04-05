#ifndef REFLOW_DRIVER_H
#define REFLOW_DRIVER_H


class Driver {
public:
    int driver_pin;
    double cycle_period_ms;

    Driver(int driver_pin, double cycle_period_ms = 200)
        : driver_pin(driver_pin), cycle_period_ms(cycle_period_ms) {}

    void begin() {
        pinMode(driver_pin, OUTPUT);
        digitalWrite(driver_pin, LOW);
    }

    void off() {
        digitalWrite(driver_pin, LOW);
    }

    void set(double duty_cycle) {
        static unsigned long  cycle_start_ms = 0;

        unsigned long cycle_elapsed = (unsigned long)millis() - cycle_start_ms;
        unsigned long time_on = (unsigned long)ceil(duty_cycle * cycle_period_ms);
        if (cycle_elapsed < time_on) {
            digitalWrite(driver_pin, HIGH);
        } else {
            digitalWrite(driver_pin, LOW);
        }

        if (cycle_elapsed >= (unsigned long)cycle_period_ms) {
            cycle_start_ms = millis();
        }

    }
};

#endif
