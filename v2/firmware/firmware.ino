#include "state.h"
#include "pid.h"
#include "comms.h"

State state;
Comms comms;
PID   pid;


unsigned long start_millis_monitoring;

void setup() {
    state = State();
    comms = Comms(*state);
    pid   = PID();

    comms.begin();
    start_millis_monitoring = millis();
}

void loop() {
    comms.handle_incoming_messages();

    if (state.running) {

        if (millis() - start_millis_monitoring > 100) {
            start_millis_monitoring = millis();

            state.data.temp = 160.6;
            state.data.time = (start_millis_monitoring - state.start_ms)/1000.0;

            comms.send_temperature();
        }
    }
}
