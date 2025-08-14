// poll_simulator.cpp
#include "../include/poll_simulator.h"
#include <cstdlib>

sim_dict poll_simulator() {
    sim_dict sim_dict;

    sim_dict.voltage = rand();
    sim_dict.current = rand();

    return sim_dict;
}