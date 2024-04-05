#ifndef REFLOW_PID_H
#define REFLOW_PID_H


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

    void learn() {
        // TODO learn
    }
};

#endif
