#include "state.h"
#include "pid.h"
#include "comms.h"
#include "driver.h"
#include "nvm.h"

const int THERMISTOR_CS_PIN = 3;
const int DRIVER_PIN        = 4;

Driver driver = Driver(DRIVER_PIN);
State  state = State(THERMISTOR_CS_PIN, &driver);
Comms  comms = Comms(&state);
PID    pid;

// ### Architecture
// #### Controller
// * controller states:
//  * **disabled** - elements cannot be turned on. they are off.
//  * **tuning** - controller is learning PID parameters.
//  * **idle** - controller is not controlling temperature.
//  * **running** - controller is actively controlling temperature.
// * reports temperature in every state.
// * responds to commands in every state.
// ##### API
// ###### Commands (from daemon)
// * 0x00 - get status
// * 0x01 - get info (includes k-v values)
// * 0x10 - disable
// * 0x11 - enable
// * 0x12 - become idle (stops tuning as well)
// * 0x20 <MSB> <B> <B> <LSB> - set scalar target temperature (big endian, float32)
// * 0x21 <MSB> <B> <B> <LSB> - set target temperature slope (big endian, float32)
// * 0x22 <DUT> - drive heating elements at duty cycle
// * 0x30 - tune all
// * 0x31 - tune base (maintain) power parameter
// * 0x32 - tune +velocity (ramp) power parameter
// * 0x33 - tune inertia (insulation) power parameter
// * 0x40 <ADDR> <N_BYTES> <DATA MSB> ... <DATA LSB> - set data (?)
// ###### Responses (from controller)
// * 0x00 <DATA> - status
//  * DATA[0]   - 1:enabled, 0:disabled
//  * DATA[1:2] - 0:idle, 1:running, 2:tuning
//  * DATA[3:4] - 0:n/a, 1:tuning base, 2:tuning velocity, 3: tuning inertia
//  * DATA[5:6] - 0:n/a, 1:set_point, 2:rate, 3:duty_cycle
//  * DATA[7]   - don't care
// * 0x01 <N_BYTES> <MSB> ... <LSB> - info
// * 0x20 <MSB> <LSB> - scalar target set
// * 0x21 <MSB> <LSB> - temp slope set
// * 0x22 <DUT> - duty cycle set

void setup() {
    state.begin();
    comms.begin();
    driver.begin();

    pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {


    // state.record_temp();

    comms.handle_incoming_messages();

    // comms.send_temperature();
    return; // DEBUG

    if (!state.heating_enabled) {
        driver.off();
        return;
    }

    // heating is enabled from this point onward.

    if (state.status == ControlStatus::Idle) {
        driver.off();
    }

    if (state.status == ControlStatus::Tuning) {
        TuningResult result = pid.tune(state.tuning_phase, state.data.temp, state.data.time);
        driver.set(result.duty_cycle);
        bool advance_phase = result.done && state.requested.tuning_phase == TuningPhase::All;

        if (advance_phase) {
            if (state.tuning_phase == TuningPhase::SteadyState)
                state.tuning_phase = TuningPhase::Velocity;
            if (state.tuning_phase == TuningPhase::Velocity)
                state.tuning_phase = TuningPhase::Inertia;
            if (state.tuning_phase == TuningPhase::Inertia)
                state.status = ControlStatus::Idle;
        }
    }

    if (state.status == ControlStatus::Running) {
        double duty_cycle;

        if (state.mode == ControlMode::SetPoint) {
            duty_cycle = pid.setpoint(state.requested.point, state.data.temp);
        }
        if (state.mode == ControlMode::Rate) {
            duty_cycle = pid.setrate(state.requested.rate, state.data.temp);
        }
        if (state.mode == ControlMode::DutyCycle) {
            duty_cycle = state.requested.duty_cycle;
        }

        driver.set(duty_cycle);
    }
}
