#ifndef REFLOW_DRIVER_H
#define REFLOW_DRIVER_H


class Driver {
public:
    int driver_pin;
    double cycle_period_ms;
    bool enabled = false;

    Driver(int driver_pin, double cycle_period_ms = 200)
        : driver_pin(driver_pin), cycle_period_ms(cycle_period_ms) {}

    void begin() {
        pinMode(driver_pin, OUTPUT);
        digitalWrite(driver_pin, LOW);
    }

    void update(double duty_cycle) {
        static unsigned long  cycle_start_ms = 0;
        static bool on                       = false;

        // disallow any updating, keep off if disabled.
        if (!this->enabled) {
            digitalWrite(driver_pin, LOW);
            return;
        }

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

    void enable() {
        this->enabled = true;
    }

    void disable() {
        this->enabled = false;
        digitalWrite(driver_pin, LOW);
    }
};

#endif
