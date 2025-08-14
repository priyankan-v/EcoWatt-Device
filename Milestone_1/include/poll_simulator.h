// Function declarations for poll_simulator
#ifndef POLL_SIMULATOR_H
#define POLL_SIMULATOR_H

struct sim_dict {
    float voltage;
    float current;
};

sim_dict poll_simulator();

#endif // POLL_SIMULATOR_H
