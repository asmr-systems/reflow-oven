#ifndef REFLOW_PID_H
#define REFLOW_PID_H

#include "state.h"

struct TuningResult {
    bool    done       = false;
    uint8_t duty_cycle = 0;

    TuningResult(bool d, uint8_t d_c) : done(d), duty_cycle(d_c) {}
};

class PID {
public:
    double Kp = 0;
    double Ki = 0;
    double Kd = 0;

    int dt = 100; // ms

    PID() {}
    PID(double Kp, double Ki, double Kd)
        : Kp(Kp), Ki(Ki), Kd(Kd) {}

    // outputs new duty cycle for powering heating elements
    double setpoint(double set_point, double current_temp) {
        // dt is in milliseconds.
        static double output       = 100;
        static int    start_time   = 0;
        static double proportional = 0;
        static double integral     = 0;

        // enforce periodic calculation.
        if (millis() - start_time < dt) return output;
        start_time = millis();

        double error, derivative, prior_error, dout;

        // perform standard pid calculation:
        // e = target - actual
        // y(t) = (Kp*e) + (Ki*int(e(t), dt)) + (Kd*de/dt)
        // note: we are not using "proportional on measurement" or
        // "derivative on measurement", just vanilla "proportional/derivative
        // on error".
        prior_error        = proportional;
        error              = set_point - current_temp;
        proportional       = error;
        integral           = integral + error;
        derivative         = error - prior_error;

        // compute change in output
        dout = Kp*proportional + Ki*integral + Kd*derivative;

        output += dout;

        return constrain(output, 0, 100);
    }

    double setrate(double set_rate, double current_temp) {
        // TODO return somethign real
        return 0;
    }

    TuningResult tune(TuningPhase phase, double temp, unsigned long time) {
        if (phase == TuningPhase::SteadyState)
            return this->tune_steadystate(temp, time);
        if (phase == TuningPhase::Velocity)
            return this->tune_velocity(temp, time);
        if (phase == TuningPhase::Inertia)
            return this->tune_inertia(temp, time);
    }
private:
    TuningResult tune_steadystate(double temp, unsigned long time) {
        bool done = false;
        uint8_t duty_cycle = 0;

        // TODO fill me in

        return {done, duty_cycle};
    }

    TuningResult tune_velocity(double temp, unsigned long time) {
        bool done = false;
        uint8_t duty_cycle = 0;

        // TODO fill me in

        return {done, duty_cycle};
    }

    TuningResult tune_inertia(double temp, unsigned long time) {
        bool done = false;
        uint8_t duty_cycle = 0;

        // TODO fill me in

        return {done, duty_cycle};
    }
};

#endif
