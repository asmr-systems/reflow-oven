#ifndef REFLOW_STATE_H
#define REFLOW_STATE_H

class State {
public:
    bool          running = false;
    unsigned long start_ms = 0;
    struct {
        double    temp = 0; // C
        double    time = 0; // s
    } data;

    State() {}
};

#endif
