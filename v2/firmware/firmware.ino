#include "state.h"
#include "pid.h"
#include "comms.h"
#include "driver.h"
#include "nvm.h"

const int THERMISTOR_CS_PIN = 3;
const int DRIVER_PIN        = 4;

State  state = State(THERMISTOR_CS_PIN);
Comms  comms = Comms(&state);
PID    pid;
Driver driver = Driver(DRIVER_PIN);


void setup() {
    state.begin();
    comms.begin();
}

void loop() {
    state.record_temp();

    comms.handle_incoming_messages();

    comms.send_temperature(); // DEBUG

    // return early if we aint running this jawn
    if (!state.running) return;

    double set_point = 100;

    double duty_cycle = pid.update(set_point, state.data.temp);

    driver.update(duty_cycle);

    comms.send_temperature();
}
