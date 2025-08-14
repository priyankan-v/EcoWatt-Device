#ifndef SAMPLE_H
#define SAMPLE_H

#include <chrono>

struct Sample {
    double t;
    double voltage;
    double current;
    double power;
};

Sample acquire_sample();

#endif // SAMPLE_H
