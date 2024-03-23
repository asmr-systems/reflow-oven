#ifndef REFLOW_DRIVER_H
#define REFLOW_DRIVER_H


class Driver {
public:
    int driver_pin;
    double cycle_period_ms;

    Driver(int driver_pin, double cycle_period_ms = 100)
        : driver_pin(driver_pin), cycle_period_ms(cycle_period_ms) {}

    void begin() {
        pinMode(driver_pin, OUTPUT);
        digitalWrite(driver_pin, LOW);
    }

    void update(double duty_cycle) {
        static int  cycle_start_ms = 0;
        static bool on             = false;
        if (millis() - cycle_start_ms < cycle_period_ms) return;
        cycle_start_ms = millis();

        if ((millis() - cycle_start_ms) < (duty_cycle * cycle_period_ms)) {
            digitalWrite(driver_pin, HIGH);
        } else {
            digitalWrite(driver_pin, LOW);
        }
    }
};

#endif
