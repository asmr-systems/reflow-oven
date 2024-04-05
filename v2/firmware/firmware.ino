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
// * 0x20 <MSB> <LSB> - set scalar target temperature (big endian)
// * 0x21 <MSB> <LSB> - set target temperature slope (big endian)
// * 0x22 <DUT> - drive heating elements at duty cycle
// * 0x30 - tune all
// * 0x31 - tune base (maintain) power parameter
// * 0x32 - tune +velocity (ramp) power parameter
// * 0x33 - tune inertia (insulation) power parameter
// * 0x40 <ADDR> <N_BYTES> <DATA MSB> ... <DATA LSB> - set data (?)
// ###### Responses (from controller)
// * 0x00 <DATA> - status
//  * DATA[0] - 1:enabled, 0:disabled
//  * DATA[1] - 1:running, 0:idle
//  * DATA[2] - 1:tuning, 0:not-tuning
//  * DATA[3:4] - 0:tuning base, 1:tuning velocity, 2: tuning inertia
//  * DATA[5:7] - don't care
// * 0x01 <N_BYTES> <MSB> ... <LSB> - info
// * 0x20 <MSB> <LSB> - scalar target set
// * 0x21 <MSB> <LSB> - temp slope set
// * 0x22 <DUT> - duty cycle set

void setup() {
    state.begin();
    comms.begin();
    driver.begin();
}

void loop() {
    state.record_temp();

    comms.handle_incoming_messages();

    comms.send_temperature();


    if (!state.heating_enabled) {
        driver.off();
        return;
    }

    // heating is enabled from this point onward.

    if (state.control == ControlState::Idle) {
        driver.off();
    }

    if (state.control == ControlState::Tuning) {
        // TODO do tuning stuff
    }

    if (state.control == ControlState::Running) {
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
