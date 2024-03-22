#ifndef REFLOW_CLOCK_H
#define REFLOW_CLOCK_H

class Clock {
public:
    int total_duration;
    // count down clock
    int time_elapsed = 0;   // ms
    int time_remaining = 0; // ms

    Clock(int total_duration) : total_duration(total_duration) {
        reset();
    }

    void start() {
        reset();
        start_time = millis();
    }

    void reset() {
        time_elapsed = 0;
        time_remaining = total_duration;
    }

    bool tick() {
        if (time_remaining == 0) {
            return false;
        }

        if (millis() - start_time > 1000) {
            start_time = millis();
            time_elapsed++;
            time_remaining--;

            return true;
        }

        return false;
    }

private:
    unsigned long start_time = 0;
};

#endif
