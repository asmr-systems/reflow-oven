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


void setup() {
    state.begin();
    comms.begin();
    driver.begin();
}

void loop() {
    state.record_temp();

    comms.handle_incoming_messages();

    comms.send_temperature(); // DEBUG

    // return early if we aint running this jawn
    if (state.running) {
        double set_point = 100;

        double duty_cycle = 1; //pid.update(set_point, state.data.temp);

        driver.update(duty_cycle);
    }

    if (state.learning) {
        // learn dC/dt
        if (state.learning_phase == LearningPhase::MaxRamp) {
            driver.update(1); // drive at full blast

            if (state.data.temp >= State::MaxTemp) {
                state.learning_phase = LearningPhase::Cooldown;
                driver.disable();
                comms.send_status();
            }
        }

        if (state.learning_phase == LearningPhase::Cooldown) {
            if (state.data.temp <= State::MaintainenceTemp) {
                state.learning_phase = LearningPhase::Maintainence;
                driver.enable();
                comms.send_status();
            }
        }

        if (state.learning_phase == LearningPhase::Maintainence) {
            // learn maintain power
        }
    }

    if (state.testing) {

    }

}
