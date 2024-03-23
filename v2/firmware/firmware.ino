#include "state.h"
#include "pid.h"
#include "comms.h"
#include "driver.h"

const int THERMISTOR_CS_PIN = 0;
const int DRIVER_PIN        = 1;

State  state;
Comms  comms;
PID    pid;
Driver driver;


void setup() {
    state  = State(THERMISTOR_CS_PIN);
    comms  = Comms(*state);
    pid    = PID();
    driver = Driver(DRIVER_PIN);

    state.begin();
    comms.begin();
}

void loop() {
    state.record_temp();

    comms.handle_incoming_messages();

    // return early if we aint running this jawn
    if (!state.running) return;

    double duty_cycle = pid.update();

    driver.update(duty_cycle);

    comms.send_temperature();
}
